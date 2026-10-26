// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.
//
// sof-tb-ctl: Command-Line & Interactive Network Control Tool for SOF Testbench4
// Supports all SOFR v1 binary opcodes (0x0000 - 0x0076), dynamic IPC4 pipeline
// and module construction, topology load/unload/run-test, scheduler & buffer
// telemetry, workload simulation, and persistent interactive REPL / batch scripts.

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "testbench/sof_remote_proto.h"

static uint32_t g_req_seq = 1;

static int read_exact(int fd, void *buf, size_t len)
{
	uint8_t *p = (uint8_t *)buf;
	size_t rem = len;

	while (rem > 0) {
		ssize_t n = recv(fd, p, rem, 0);

		if (n <= 0) {
			if (n < 0 && errno == EINTR)
				continue;
			return -1;
		}
		p += n;
		rem -= (size_t)n;
	}
	return 0;
}

static int write_exact(int fd, const void *buf, size_t len)
{
	const uint8_t *p = (const uint8_t *)buf;
	size_t rem = len;

	while (rem > 0) {
		ssize_t n = send(fd, p, rem, MSG_NOSIGNAL);

		if (n <= 0) {
			if (n < 0 && errno == EINTR)
				continue;
			return -1;
		}
		p += n;
		rem -= (size_t)n;
	}
	return 0;
}

static int connect_server(const char *host, int port)
{
	struct addrinfo hints, *res = NULL, *rp;
	char port_str[16];
	int sock = -1;
	int one = 1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(port_str, sizeof(port_str), "%d", port);

	if (getaddrinfo(host, port_str, &hints, &res) != 0)
		return -1;

	for (rp = res; rp != NULL; rp = rp->ai_next) {
		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sock < 0)
			continue;
		if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0)
			break;
		close(sock);
		sock = -1;
	}
	freeaddrinfo(res);

	if (sock >= 0)
		setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
	return sock;
}

static int rpc_call(int sock, uint16_t opcode, const void *req_payload, uint32_t req_len,
		    struct sof_net_hdr *rsp_hdr_out, uint8_t **rsp_payload_out)
{
	struct sof_net_hdr req_hdr = {
		.magic       = SOF_NET_MAGIC,
		.version     = SOF_NET_VERSION,
		.opcode      = opcode,
		.req_id      = g_req_seq++,
		.payload_len = req_payload ? req_len : 0,
		.status      = 0,
	};
	struct sof_net_hdr rsp_hdr;
	uint8_t *buf = NULL;

	if (rsp_payload_out)
		*rsp_payload_out = NULL;

	if (write_exact(sock, &req_hdr, sizeof(req_hdr)) < 0)
		return -EIO;
	if (req_payload && req_len > 0) {
		if (write_exact(sock, req_payload, req_len) < 0)
			return -EIO;
	}

	if (read_exact(sock, &rsp_hdr, sizeof(rsp_hdr)) < 0)
		return -EIO;
	if (rsp_hdr.magic != SOF_NET_MAGIC)
		return -EPROTO;

	if (rsp_hdr.payload_len > 0) {
		buf = calloc(1, rsp_hdr.payload_len + 1);
		if (!buf)
			return -ENOMEM;
		if (read_exact(sock, buf, rsp_hdr.payload_len) < 0) {
			free(buf);
			return -EIO;
		}
		buf[rsp_hdr.payload_len] = '\0';
	}

	if (rsp_hdr_out)
		*rsp_hdr_out = rsp_hdr;
	if (rsp_payload_out)
		*rsp_payload_out = buf;
	else
		free(buf);

	return rsp_hdr.status;
}

static uint8_t *load_file_or_hex(const char *arg, uint32_t *len_out)
{
	FILE *fp = fopen(arg, "rb");

	*len_out = 0;
	if (fp) {
		long sz;
		uint8_t *buf;

		fseek(fp, 0, SEEK_END);
		sz = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		if (sz <= 0) {
			fclose(fp);
			return NULL;
		}
		buf = malloc((size_t)sz);
		if (!buf) {
			fclose(fp);
			return NULL;
		}
		if (fread(buf, 1, (size_t)sz, fp) != (size_t)sz) {
			free(buf);
			fclose(fp);
			return NULL;
		}
		fclose(fp);
		*len_out = (uint32_t)sz;
		return buf;
	}

	/* Parse hex string (with or without 0x/colons/spaces) */
	size_t slen = strlen(arg);
	uint8_t *buf = malloc(slen / 2 + 4);
	uint32_t idx = 0;
	const char *p = arg;

	if (!buf)
		return NULL;
	if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
		p += 2;
	while (*p) {
		while (*p && !isxdigit((unsigned char)*p))
			p++;
		if (!*p)
			break;
		char h[3] = { p[0], p[1] ? p[1] : '0', '\0' };

		buf[idx++] = (uint8_t)strtoul(h, NULL, 16);
		p += p[1] ? 2 : 1;
	}
	*len_out = idx;
	return buf;
}

static void hexdump(const uint8_t *data, uint32_t len, uint32_t base_off)
{
	uint32_t i, j;

	for (i = 0; i < len; i += 16) {
		printf("  %06x: ", base_off + i);
		for (j = 0; j < 16; j++) {
			if (i + j < len)
				printf("%02x ", data[i + j]);
			else
				printf("   ");
		}
		printf(" |");
		for (j = 0; j < 16 && i + j < len; j++) {
			uint8_t c = data[i + j];

			printf("%c", (c >= 32 && c < 127) ? c : '.');
		}
		printf("|\n");
	}
}

static int exec_shell_cmd(int sock, const char *cmd_str, bool check_err_lines)
{
	struct sof_net_hdr rhdr;
	uint8_t *rbuf = NULL;
	uint32_t clen = (uint32_t)strlen(cmd_str) + 1;
	uint32_t req_len = sizeof(struct sof_net_shell_req) + clen;
	uint8_t *req_buf = calloc(1, req_len);
	struct sof_net_shell_req *sreq;
	int st;
	int ret = 0;

	if (!req_buf)
		return -ENOMEM;
	sreq = (struct sof_net_shell_req *)req_buf;
	sreq->timeout_ms = 30000;
	sreq->cmd_len = clen;
	memcpy(req_buf + sizeof(*sreq), cmd_str, clen);

	st = rpc_call(sock, SOF_NET_OP_SHELL_EXEC, req_buf, req_len, &rhdr, &rbuf);
	free(req_buf);

	if (st < 0) {
		fprintf(stderr, "error: shell RPC failed (%d)\n", st);
		free(rbuf);
		return st;
	}
	if (rbuf && rhdr.payload_len > 0) {
		printf("%s", (char *)rbuf);
		if (((char *)rbuf)[rhdr.payload_len - 1] != '\n')
			printf("\n");
		if (check_err_lines) {
			if (strncmp((char *)rbuf, "error:", 6) == 0 ||
			    strstr((char *)rbuf, "\nerror:") != NULL)
				ret = 1;
		}
	}
	free(rbuf);
	return ret;
}

static int exec_joined_shell_cmd(int sock, const char *prefix, int argc, char **argv,
				 bool check_err_lines)
{
	char cmd_buf[8192];
	size_t off = 0;
	int i;

	if (prefix && *prefix)
		off += snprintf(cmd_buf + off, sizeof(cmd_buf) - off, "%s", prefix);

	for (i = 0; i < argc && off + 256 < sizeof(cmd_buf); i++) {
		bool has_space = strchr(argv[i], ' ') != NULL || strchr(argv[i], '\t') != NULL;
		bool prev_is_flag = (i > 0 && argv[i - 1][0] == '-');

		if (off > 0)
			off += snprintf(cmd_buf + off, sizeof(cmd_buf) - off, " ");
		if (has_space && prev_is_flag && argv[i][0] != '"' && argv[i][0] != '\'')
			off += snprintf(cmd_buf + off, sizeof(cmd_buf) - off, "\"%s\"", argv[i]);
		else
			off += snprintf(cmd_buf + off, sizeof(cmd_buf) - off, "%s", argv[i]);
	}
	return exec_shell_cmd(sock, cmd_buf, check_err_lines);
}

static void print_usage(const char *prog)
{
	printf("Usage: %s [-H <host>] [-p <port>] [-f <script_file>] [command [args...]]\n\n"
	       "Options:\n"
	       "  -H <host>     Target testbench IP address (default: 127.0.0.1)\n"
	       "  -p <port>     Target testbench TCP port (default: 9998)\n"
	       "  -f <file>     Execute batch commands from <file> over 1 connection\n"
	       "  -h, --help    Show this help message\n\n"
	       "Core & Device Opcodes:\n"
	       "  ping                                          Ping server (SOF_NET_OP_PING)\n"
	       "  info                                          Get device & FW info (SOF_NET_OP_GET_INFO)\n"
	       "  pm [query|wake|release|boot|core-get|core-put] [core] PM control (SOF_NET_OP_PM_CTL)\n"
	       "  core <get|put|enable|disable> <core_id>       Enable/disable DSP core\n"
	       "  win-info                                      List SRAM windows & debug slots\n"
	       "  win-read <win_id> <offset> <size> [slot_type] Read SRAM window bytes\n"
	       "  win-write <win_id> <offset> <hex_or_file>     Write SRAM window bytes\n"
	       "  winstream [cmd_text]                          Exchange Zephyr winstream shell I/O\n\n"
	       "IPC4, Large Config & Dynamic Module Loading:\n"
	       "  ipc-tx <prim_hex> <ext_hex> [data_hex|file] [rx_sz] Send raw IPC4 message\n"
	       "  large-cfg-get <mod> <inst> <param> [sz] [out_file]  Get IPC4 large config\n"
	       "  large-cfg-set <mod> <inst> <param> <hex_or_file>    Set IPC4 large config\n"
	       "  llext-load <file.llext|.so> [lib_id] [name]   Load dynamic module (SOF_NET_OP_LLEXT_LOAD)\n\n"
	       "Interactive Topology, Pipeline, Platform & Module Construction:\n"
	       "  platform-cfg [status | presets | preset <name> | cores <N> |\n"
	       "                mem <hpsram[,lpsram[,dram]]> | freq <mhz_or_hz | core:freq,...>]\n"
	       "                                                Configure/query platform hardware specs\n"
	       "  run-test -t <tplg> [-A <plat>] [-N <cores>] [-m <mem>] [-F <freq>] [-p <pipes>]\n"
	       "           [-i <in>] [-o <out>] [-r <rate>] [-R <rate>] [-c <ch>] [-n <ch>]\n"
	       "           [-b <fmt>] [-s <ctl>] [-w <m>] [-M <m>]\n"
	       "                                                Run full topology test in persistent server\n"
	       "  io-cfg [-i <in>] [-o <out>] [-r <rate>] [-R <rate>] [-c <ch>] [-n <ch>] [-b <fmt>]\n"
	       "  tplg-load <file.tplg> [p1,p2,...]             Load topology and start pipelines\n"
	       "  tplg-unload                                   Reset and unload active topology\n"
	       "  pipelines | widgets | modules | controls      Inspect active pipelines, widgets & kctls\n"
	       "  ppl-create <id> [prio] [core]                 Create IPC4 pipeline\n"
	       "  ppl-state <id> <run|pause|reset>              Set IPC4 pipeline state\n"
	       "  ppl-delete <id>                               Delete IPC4 pipeline\n"
	       "  mod-init <mod> <inst> <ppl> [core] [dom]      Initialize IPC4 module instance\n"
	       "  mod-bind <sm> <si> <sq> <dm> <di> <dq>        Bind source module queue to destination\n"
	       "  mod-unbind <sm> <si> <sq> <dm> <di> <dq>      Unbind module instances\n"
	       "  mod-delete <mod_id> <inst_id>                 Delete module instance\n\n"
	       "Telemetry, Workload Simulation, Probes & Audio Control:\n"
	       "  telem [mask_hex]                              Get composite telemetry snapshot\n"
	       "  sched-telem | sched-telem-reset               Show/reset per-core/pipe/module telemetry\n"
	       "  sched-trace [N] [--core C] [--pipe P] [--comp ID] [--domain ll|dp|sync] [--json F] [--clear]\n"
	       "                                                Get per-iteration LL/DP module scheduling timeline\n"
	       "  mod-buffers [id_or_name]                      Show module entry/exit buffer levels & taps\n"
	       "  stats | heap                                  Show file I/O stats and module heap usage\n"
	       "  workload-sim [get | set <off|virtual|realtime> [sram|dram] [factor] [cyc_word] |\n"
	       "                freq <spec> | mem <sram|dram> [factor] [cyc] | mod <id> <cpc> [mem] | reset]\n"
	       "  probes [avail | active | extract [comp_id] [out.raw]]\n"
	       "  pcm [status | start-tone [dev] [freq] [rate] [ch] | stop-tone |\n"
	       "       play <file.raw> [dev] [rate] [ch] | capture <ms> [out.raw]]\n"
	       "  compr <status|caps|play|pause|resume|stop> [card] [dev] [file]\n"
	       "  cards | pcm-caps [card] [dev] | dapm [out.dot]\n"
	       "  kctl <list | get <id_or_name> | set <id_or_name> <v0> [v1...] | set-bytes <id> <file>>\n"
	       "  pause | resume | step [N] | loglevel <0..4> | quit\n"
	       "  shell <cmd...> | repl                         Execute shell command or start REPL\n",
	       prog);
}

static int dispatch_command(int sock, int argc, char **argv)
{
	struct sof_net_hdr rhdr;
	uint8_t *rbuf = NULL;
	const char *cmd;
	int st;

	if (argc <= 0 || !argv[0])
		return 0;
	cmd = argv[0];

	if (strcmp(cmd, "help") == 0 || strcmp(cmd, "-h") == 0 || strcmp(cmd, "--help") == 0) {
		print_usage("sof-tb-ctl");
		return 0;
	}

	if (strcmp(cmd, "ping") == 0) {
		st = rpc_call(sock, SOF_NET_OP_PING, NULL, 0, &rhdr, &rbuf);
		if (st == 0 && rbuf)
			printf("PING -> %s (req_id=%u, status=%d)\n",
			       (char *)rbuf, rhdr.req_id, rhdr.status);
		else
			fprintf(stderr, "error: PING failed (%d)\n", st);
		free(rbuf);
		return st < 0 ? 1 : 0;
	}

	if (strcmp(cmd, "info") == 0) {
		st = rpc_call(sock, SOF_NET_OP_GET_INFO, NULL, 0, &rhdr, &rbuf);
		if (st == 0 && rbuf && rhdr.payload_len >= sizeof(struct sof_cdev_info)) {
			const struct sof_cdev_info *inf = (const struct sof_cdev_info *)rbuf;

			printf("=== SOF Device & Firmware Info ===\n"
			       "  Platform:       %s (IPC%u, HW IP v%u)\n"
			       "  FW Version:     v%u.%u.%u.%u (state=%u)\n"
			       "  Topology/FW:    %s\n"
			       "  Power State:    %s (pm_runtime=%u)\n"
			       "  DSP Cores:      %u total, enabled_mask=0x%x\n"
			       "  Max IPC Size:   %u bytes\n",
			       inf->platform, inf->ipc_type == 1 ? 4 : 3, inf->hw_ip_version,
			       inf->fw_ver_major, inf->fw_ver_minor,
			       inf->fw_ver_micro, inf->fw_ver_build, inf->fw_state,
			       inf->fw_filename[0] ? inf->fw_filename : "(none)",
			       inf->dsp_power_state == 1 ? "D0 (ACTIVE)" : "D3 (SUSPENDED)",
			       inf->pm_runtime_active,
			       inf->num_cores, inf->enabled_cores_mask,
			       inf->max_payload_size);
		} else {
			fprintf(stderr, "error: GET_INFO failed (%d)\n", st);
		}
		free(rbuf);
		return st < 0 ? 1 : 0;
	}

	if (strcmp(cmd, "pm") == 0 || strcmp(cmd, "core") == 0) {
		struct sof_cdev_pm_ctl req = {0};
		const char *sub = (argc > 1) ? argv[1] : "query";

		if (strcmp(cmd, "core") == 0) {
			if (strcmp(sub, "get") == 0 || strcmp(sub, "enable") == 0)
				req.op = SOF_PM_OP_CORE_GET;
			else if (strcmp(sub, "put") == 0 || strcmp(sub, "disable") == 0)
				req.op = SOF_PM_OP_CORE_PUT;
			else
				req.op = SOF_PM_OP_QUERY;
			req.core_id = (argc > 2) ? (uint32_t)atoi(argv[2]) : 0;
		} else {
			if (strcmp(sub, "wake") == 0)
				req.op = SOF_PM_OP_WAKE_HOLD;
			else if (strcmp(sub, "release") == 0)
				req.op = SOF_PM_OP_RELEASE_HOLD;
			else if (strcmp(sub, "boot") == 0)
				req.op = SOF_PM_OP_BOOT_DSP;
			else if (strcmp(sub, "core-get") == 0)
				req.op = SOF_PM_OP_CORE_GET;
			else if (strcmp(sub, "core-put") == 0)
				req.op = SOF_PM_OP_CORE_PUT;
			else
				req.op = SOF_PM_OP_QUERY;
			req.core_id = (argc > 2) ? (uint32_t)atoi(argv[2]) : 0;
		}

		st = rpc_call(sock, SOF_NET_OP_PM_CTL, &req, sizeof(req), &rhdr, &rbuf);
		if (st == 0 && rbuf && rhdr.payload_len >= sizeof(req)) {
			const struct sof_cdev_pm_ctl *rsp = (const struct sof_cdev_pm_ctl *)rbuf;

			printf("PM_CTL(op=%u, core=%u) -> fw_state=%u power=%s cores_mask=0x%x d0_hold=%u\n",
			       rsp->op, rsp->core_id, rsp->fw_state,
			       rsp->dsp_power_state == 1 ? "D0" : "D3",
			       rsp->enabled_cores_mask, rsp->fd_holding_d0);
		} else {
			fprintf(stderr, "error: PM_CTL failed (%d)\n", st);
		}
		free(rbuf);
		return st < 0 ? 1 : 0;
	}

	if (strcmp(cmd, "ipc-tx") == 0) {
		if (argc < 3) {
			fprintf(stderr, "Usage: ipc-tx <primary_hex> <extension_hex> [payload_hex_or_file] [rx_size]\n");
			return 1;
		}
		struct sof_net_ipc_tx_req ireq = {0};
		uint8_t *tx_data = NULL;
		uint32_t tx_len = 0;

		ireq.primary = (uint32_t)strtoul(argv[1], NULL, 0);
		ireq.extension = (uint32_t)strtoul(argv[2], NULL, 0);
		if (argc > 3 && strcmp(argv[3], "-") != 0 && strcmp(argv[3], "0") != 0)
			tx_data = load_file_or_hex(argv[3], &tx_len);
		ireq.tx_size = tx_len;
		ireq.rx_size = (argc > 4) ? (uint32_t)strtoul(argv[4], NULL, 0) : 0;

		uint8_t *req_buf = calloc(1, sizeof(ireq) + tx_len);

		if (!req_buf) {
			free(tx_data);
			return 1;
		}
		memcpy(req_buf, &ireq, sizeof(ireq));
		if (tx_data && tx_len > 0)
			memcpy(req_buf + sizeof(ireq), tx_data, tx_len);
		free(tx_data);

		st = rpc_call(sock, SOF_NET_OP_IPC_TX, req_buf, sizeof(ireq) + tx_len, &rhdr, &rbuf);
		free(req_buf);
		if (st == 0 && rbuf && rhdr.payload_len >= sizeof(struct sof_net_ipc_tx_rsp)) {
			const struct sof_net_ipc_tx_rsp *rsp = (const struct sof_net_ipc_tx_rsp *)rbuf;

			printf("IPC_TX -> reply_primary=0x%08x reply_ext=0x%08x ipc_status=%d rx_size=%u\n",
			       rsp->reply_primary, rsp->reply_extension, rsp->ipc_status, rsp->rx_size);
			if (rsp->rx_size > 0 && rhdr.payload_len >= sizeof(*rsp) + rsp->rx_size)
				hexdump(rbuf + sizeof(*rsp), rsp->rx_size, 0);
		} else {
			fprintf(stderr, "error: IPC_TX failed (%d)\n", st);
		}
		free(rbuf);
		return st < 0 ? 1 : 0;
	}

	if (strcmp(cmd, "large-cfg-get") == 0) {
		if (argc < 4) {
			fprintf(stderr, "Usage: large-cfg-get <mod_id> <inst_id> <param_id> [max_size] [out_file]\n");
			return 1;
		}
		struct sof_net_large_cfg_req lreq = {
			.module_id   = (uint32_t)strtoul(argv[1], NULL, 0),
			.instance_id = (uint32_t)strtoul(argv[2], NULL, 0),
			.param_id    = (uint32_t)strtoul(argv[3], NULL, 0),
			.set_get     = SOF_CDEV_LARGE_CFG_GET,
			.data_size   = (argc > 4) ? (uint32_t)strtoul(argv[4], NULL, 0) : 4096,
			.flags       = 0,
		};
		st = rpc_call(sock, SOF_NET_OP_IPC_LARGE_CFG, &lreq, sizeof(lreq), &rhdr, &rbuf);
		if (st == 0) {
			printf("LARGE_CFG_GET(mod=0x%x, inst=%u, param=%u) -> %u bytes\n",
			       lreq.module_id, lreq.instance_id, lreq.param_id, rhdr.payload_len);
			if (argc > 5 && rbuf && rhdr.payload_len > 0) {
				FILE *fp = fopen(argv[5], "wb");

				if (fp) {
					fwrite(rbuf, 1, rhdr.payload_len, fp);
					fclose(fp);
					printf("Saved %u bytes to %s\n", rhdr.payload_len, argv[5]);
				}
			} else if (rbuf && rhdr.payload_len > 0) {
				hexdump(rbuf, rhdr.payload_len < 256 ? rhdr.payload_len : 256, 0);
			}
		} else {
			fprintf(stderr, "error: LARGE_CFG_GET failed (%d)\n", st);
		}
		free(rbuf);
		return st < 0 ? 1 : 0;
	}

	if (strcmp(cmd, "large-cfg-set") == 0) {
		if (argc < 5) {
			fprintf(stderr, "Usage: large-cfg-set <mod_id> <inst_id> <param_id> <hex_or_file>\n");
			return 1;
		}
		uint32_t dlen = 0;
		uint8_t *dbytes = load_file_or_hex(argv[4], &dlen);

		if (!dbytes || dlen == 0) {
			fprintf(stderr, "error: invalid payload '%s'\n", argv[4]);
			free(dbytes);
			return 1;
		}
		struct sof_net_large_cfg_req lreq = {
			.module_id   = (uint32_t)strtoul(argv[1], NULL, 0),
			.instance_id = (uint32_t)strtoul(argv[2], NULL, 0),
			.param_id    = (uint32_t)strtoul(argv[3], NULL, 0),
			.set_get     = SOF_CDEV_LARGE_CFG_SET,
			.data_size   = dlen,
			.flags       = 0,
		};
		uint8_t *req_buf = calloc(1, sizeof(lreq) + dlen);

		memcpy(req_buf, &lreq, sizeof(lreq));
		memcpy(req_buf + sizeof(lreq), dbytes, dlen);
		free(dbytes);

		st = rpc_call(sock, SOF_NET_OP_IPC_LARGE_CFG, req_buf, sizeof(lreq) + dlen, &rhdr, &rbuf);
		free(req_buf);
		free(rbuf);
		if (st == 0)
			printf("LARGE_CFG_SET(mod=0x%x, inst=%u, param=%u, size=%u) -> OK\n",
			       lreq.module_id, lreq.instance_id, lreq.param_id, dlen);
		else
			fprintf(stderr, "error: LARGE_CFG_SET failed (%d)\n", st);
		return st < 0 ? 1 : 0;
	}

	if (strcmp(cmd, "win-info") == 0) {
		st = rpc_call(sock, SOF_NET_OP_WIN_INFO, NULL, 0, &rhdr, &rbuf);
		if (st == 0 && rbuf && rhdr.payload_len >= sizeof(struct sof_cdev_win_info)) {
			const struct sof_cdev_win_info *wi = (const struct sof_cdev_win_info *)rbuf;
			int i;

			printf("=== Emulated DSP SRAM Windows ===\n");
			for (i = 0; i < SOF_CDEV_MAX_WINDOWS; i++) {
				if (!wi->windows[i].size)
					continue;
				printf("  [Window %u] bar_offset=0x%06x size=%6u flags=0x%x\n",
				       wi->windows[i].win_id, wi->windows[i].bar_offset,
				       wi->windows[i].size, wi->windows[i].flags);
			}
			printf("=== Window 2 Debug Slots ===\n");
			for (i = 0; i < SOF_CDEV_MAX_DEBUG_SLOTS; i++) {
				if (!wi->debug_slots[i].type)
					continue;
				printf("  [Slot %2d] res_id=0x%08x type=0x%08x vma=0x%08x off=0x%05x size=%u\n",
				       wi->debug_slots[i].slot_index, wi->debug_slots[i].res_id,
				       wi->debug_slots[i].type, wi->debug_slots[i].vma,
				       wi->debug_slots[i].win_offset, wi->debug_slots[i].size);
			}
		} else {
			fprintf(stderr, "error: WIN_INFO failed (%d)\n", st);
		}
		free(rbuf);
		return st < 0 ? 1 : 0;
	}

	if (strcmp(cmd, "win-read") == 0) {
		if (argc < 4) {
			fprintf(stderr, "Usage: win-read <win_id> <offset> <size> [slot_type]\n");
			return 1;
		}
		struct sof_net_win_io_req wreq = {
			.win_id     = (uint32_t)strtoul(argv[1], NULL, 0),
			.offset     = (uint32_t)strtoul(argv[2], NULL, 0),
			.size       = (uint32_t)strtoul(argv[3], NULL, 0),
			.slot_type  = (argc > 4) ? (uint32_t)strtoul(argv[4], NULL, 0) : 0,
			.slot_index = 0,
			.flags      = 0,
		};
		st = rpc_call(sock, SOF_NET_OP_WIN_READ, &wreq, sizeof(wreq), &rhdr, &rbuf);
		if (st == 0 && rbuf) {
			printf("WIN_READ(win=%u, off=0x%x, size=%u):\n",
			       wreq.win_id, wreq.offset, rhdr.payload_len);
			hexdump(rbuf, rhdr.payload_len, wreq.offset);
		} else {
			fprintf(stderr, "error: WIN_READ failed (%d)\n", st);
		}
		free(rbuf);
		return st < 0 ? 1 : 0;
	}

	if (strcmp(cmd, "win-write") == 0) {
		if (argc < 4) {
			fprintf(stderr, "Usage: win-write <win_id> <offset> <hex_or_file>\n");
			return 1;
		}
		uint32_t dlen = 0;
		uint8_t *dbytes = load_file_or_hex(argv[3], &dlen);

		if (!dbytes || dlen == 0) {
			free(dbytes);
			return 1;
		}
		struct sof_net_win_io_req wreq = {
			.win_id = (uint32_t)strtoul(argv[1], NULL, 0),
			.offset = (uint32_t)strtoul(argv[2], NULL, 0),
			.size   = dlen,
		};
		uint8_t *req_buf = calloc(1, sizeof(wreq) + dlen);

		memcpy(req_buf, &wreq, sizeof(wreq));
		memcpy(req_buf + sizeof(wreq), dbytes, dlen);
		free(dbytes);
		st = rpc_call(sock, SOF_NET_OP_WIN_WRITE, req_buf, sizeof(wreq) + dlen, &rhdr, &rbuf);
		free(req_buf);
		free(rbuf);
		if (st == 0)
			printf("WIN_WRITE(win=%u, off=0x%x, size=%u) -> OK\n",
			       wreq.win_id, wreq.offset, dlen);
		else
			fprintf(stderr, "error: WIN_WRITE failed (%d)\n", st);
		return st < 0 ? 1 : 0;
	}

	if (strcmp(cmd, "winstream") == 0) {
		const char *tx_cmd = (argc > 1) ? argv[1] : "version";
		uint32_t tx_len = (uint32_t)strlen(tx_cmd);
		struct sof_net_winstream_req wsreq = {
			.slot_type   = SOF_CDEV_SLOT_SHELL,
			.rx_win_size = 4096,
			.tx_len      = tx_len,
			.rx_buf_len  = 4096,
			.seq_in      = 0,
		};
		uint8_t *req_buf = calloc(1, sizeof(wsreq) + tx_len);

		memcpy(req_buf, &wsreq, sizeof(wsreq));
		memcpy(req_buf + sizeof(wsreq), tx_cmd, tx_len);
		st = rpc_call(sock, SOF_NET_OP_WINSTREAM_IO, req_buf, sizeof(wsreq) + tx_len, &rhdr, &rbuf);
		free(req_buf);
		if (st == 0 && rbuf && rhdr.payload_len >= sizeof(struct sof_net_winstream_rsp)) {
			const struct sof_net_winstream_rsp *wsrsp =
				(const struct sof_net_winstream_rsp *)rbuf;

			printf("%.*s\n", (int)wsrsp->rx_bytes_read,
			       (const char *)(rbuf + sizeof(*wsrsp)));
		} else {
			fprintf(stderr, "error: WINSTREAM_IO failed (%d)\n", st);
		}
		free(rbuf);
		return st < 0 ? 1 : 0;
	}

	if (strcmp(cmd, "telem") == 0) {
		struct sof_net_telem_req treq = {
			.query_mask = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 0) : 0x7FU,
			.flags      = 0,
		};
		st = rpc_call(sock, SOF_NET_OP_TELEM_SNAPSHOT, &treq, sizeof(treq), &rhdr, &rbuf);
		if (st == 0 && rbuf && rhdr.payload_len >= sizeof(struct sof_net_telem_snapshot)) {
			const struct sof_net_telem_snapshot *sn =
				(const struct sof_net_telem_snapshot *)rbuf;
			const uint32_t *r32 = (const uint32_t *)sn->fw_regs;

			printf("=== SOF Composite Telemetry Snapshot ===\n"
			       "  Platform:       %s (Topology: %s)\n"
			       "  Flags Present:  0x%02x, Active PCM Streams: %u\n"
			       "  FW Regs:        fw_status=0x%08x fps=0x%08x ticks=%u\n"
			       "  TLV Sizes:      mem=%uB pwr=%uB ext_perf=%uB modules=%uB\n",
			       sn->dev_info.platform,
			       sn->dev_info.fw_filename[0] ? sn->dev_info.fw_filename : "(none)",
			       sn->flags_present, sn->active_pcm_streams,
			       r32[0], r32[2], r32[4],
			       sn->mem_state_len, sn->pwr_state_len,
			       sn->ext_perf_len, sn->modules_info_len);
		} else {
			fprintf(stderr, "error: TELEM_SNAPSHOT failed (%d)\n", st);
		}
		free(rbuf);
		return st < 0 ? 1 : 0;
	}

	if (strcmp(cmd, "sched-trace") == 0) {
		struct sof_net_sched_trace_req sreq = {
			.op             = SOF_SCHED_TRACE_OP_READ,
			.max_events     = 128,
			.filter_core_id = 0xFFFFFFFFU,
			.filter_pipe_id = 0xFFFFFFFFU,
			.filter_comp_id = 0xFFFFFFFFU,
			.filter_domain  = 0xFFFFFFFFU,
		};
		const char *json_file = NULL;
		int i;

		for (i = 1; i < argc; i++) {
			if (strcmp(argv[i], "clear") == 0 || strcmp(argv[i], "--clear") == 0) {
				sreq.op = SOF_SCHED_TRACE_OP_CLEAR;
			} else if ((strcmp(argv[i], "--core") == 0 || strcmp(argv[i], "-c") == 0) &&
				   i + 1 < argc) {
				sreq.filter_core_id = (uint32_t)strtoul(argv[++i], NULL, 0);
			} else if ((strcmp(argv[i], "--pipe") == 0 || strcmp(argv[i], "-p") == 0) &&
				   i + 1 < argc) {
				sreq.filter_pipe_id = (uint32_t)strtoul(argv[++i], NULL, 0);
			} else if ((strcmp(argv[i], "--comp") == 0 || strcmp(argv[i], "-m") == 0) &&
				   i + 1 < argc) {
				sreq.filter_comp_id = (uint32_t)strtoul(argv[++i], NULL, 0);
			} else if ((strcmp(argv[i], "--domain") == 0 || strcmp(argv[i], "-d") == 0) &&
				   i + 1 < argc) {
				const char *d = argv[++i];

				if (strcasecmp(d, "ll") == 0 || strcasecmp(d, "ll-copy") == 0 ||
				    strcmp(d, "0") == 0)
					sreq.filter_domain = 0;
				else if (strcasecmp(d, "sync") == 0 ||
					 strcasecmp(d, "dp-sync") == 0 || strcmp(d, "1") == 0)
					sreq.filter_domain = 1;
				else if (strcasecmp(d, "dp") == 0 || strcasecmp(d, "dp-run") == 0 ||
					 strcmp(d, "2") == 0)
					sreq.filter_domain = 2;
			} else if ((strcmp(argv[i], "--json") == 0 || strcmp(argv[i], "-j") == 0) &&
				   i + 1 < argc) {
				json_file = argv[++i];
			} else if (isdigit((unsigned char)argv[i][0])) {
				sreq.max_events = (uint32_t)strtoul(argv[i], NULL, 0);
			}
		}

		st = rpc_call(sock, SOF_NET_OP_SCHED_TRACE, &sreq, sizeof(sreq), &rhdr, &rbuf);
		if (st == 0 && rbuf && rhdr.payload_len >= sizeof(struct sof_net_sched_trace_hdr)) {
			const struct sof_net_sched_trace_hdr *thdr =
				(const struct sof_net_sched_trace_hdr *)rbuf;
			const struct sof_net_sched_trace_entry *entries =
				(const struct sof_net_sched_trace_entry *)(rbuf + sizeof(*thdr));
			const char *mode_str = (thdr->sim_mode == 2) ? "REALTIME-PACED" :
					       (thdr->sim_mode == 1) ? "VIRTUAL-SIM" :
								       "HOST-MEASURED";
			uint32_t e;

			if (sreq.op == SOF_SCHED_TRACE_OP_CLEAR) {
				printf("SCHED_TRACE -> ring buffer cleared (platform=%s, tick=%u)\n",
				       thdr->platform_name, thdr->current_ll_tick);
				free(rbuf);
				return 0;
			}

			printf("=== Per-Iteration LL & DP Module Scheduling Timeline [Platform=%s(%uc) Mode=%s Tick=%u Recorded=%u Ring=%u Showing=%u] ===\n",
			       thdr->platform_name, thdr->num_cores, mode_str,
			       thdr->current_ll_tick, thdr->total_recorded_events,
			       thdr->avail_in_ring, thdr->returned_events);
			printf("  %-5s %-5s %-5s %-4s %-4s %-7s %-32s %12s %12s %10s %12s %21s %13s %11s %19s\n",
			       "Seq", "Tick", "Iter", "Core", "Pipe", "Domain",
			       "Module", "Start(us)", "Complete(us)", "Dur(us)",
			       "Deadline(us)", "Start->End(cyc)", "Cyc/Budget",
			       "In->Out(B)", "WallStart->End(us)");

			for (e = 0; e < thdr->returned_events; e++) {
				const struct sof_net_sched_trace_entry *ev = &entries[e];
				const char *dom = (ev->event_type == 2) ? "DP-RUN" :
						  (ev->event_type == 1) ? "DP-SYNC" : "LL-COPY";
				uint64_t dur_ns = (ev->complete_ns >= ev->start_ns) ?
						  (ev->complete_ns - ev->start_ns) : 0;
				char mod_col[48];
				char cyc_col[48];
				char bud_col[32];
				char io_col[32];
				char wall_col[48];

				snprintf(mod_col, sizeof(mod_col), "0x%06x %.22s",
					 ev->comp_id, ev->widget_name);
				snprintf(cyc_col, sizeof(cyc_col), "%llu->%llu",
					 (unsigned long long)ev->start_cycles,
					 (unsigned long long)ev->complete_cycles);
				snprintf(bud_col, sizeof(bud_col), "%u/%u%s",
					 ev->duration_cycles, ev->cpc_budget,
					 ev->cpc_violation ? "!" : "");
				snprintf(io_col, sizeof(io_col), "%u->%u",
					 ev->bytes_in, ev->bytes_out);
				snprintf(wall_col, sizeof(wall_col), "%llu.%03llu->%llu.%03llu",
					 (unsigned long long)(ev->wall_start_ns / 1000ULL),
					 (unsigned long long)(ev->wall_start_ns % 1000ULL),
					 (unsigned long long)(ev->wall_complete_ns / 1000ULL),
					 (unsigned long long)(ev->wall_complete_ns % 1000ULL));

				printf("  %-5u %-5u %-5u C%-3u P%-3u %-7s %-32s %8llu.%03llu %8llu.%03llu %6llu.%03llu %8llu.%03llu %21s %13s %11s %19s\n",
				       ev->seq_id, ev->ll_tick_nr, ev->mod_iter_nr,
				       ev->core_id, ev->pipeline_id, dom, mod_col,
				       (unsigned long long)(ev->start_ns / 1000ULL),
				       (unsigned long long)(ev->start_ns % 1000ULL),
				       (unsigned long long)(ev->complete_ns / 1000ULL),
				       (unsigned long long)(ev->complete_ns % 1000ULL),
				       (unsigned long long)(dur_ns / 1000ULL),
				       (unsigned long long)(dur_ns % 1000ULL),
				       (unsigned long long)(ev->deadline_ns / 1000ULL),
				       (unsigned long long)(ev->deadline_ns % 1000ULL),
				       cyc_col, bud_col, io_col, wall_col);
			}

			if (json_file) {
				FILE *fp = fopen(json_file, "w");

				if (fp) {
					fprintf(fp, "{\n  \"displayTimeUnit\": \"ns\",\n  \"traceEvents\": [\n");
					for (e = 0; e < thdr->returned_events; e++) {
						const struct sof_net_sched_trace_entry *ev = &entries[e];
						const char *dom = (ev->event_type == 2) ? "DP-RUN" :
								  (ev->event_type == 1) ? "DP-SYNC" :
											  "LL-COPY";
						uint64_t dur_ns = (ev->complete_ns >= ev->start_ns) ?
								  (ev->complete_ns - ev->start_ns) : 1;

						fprintf(fp,
							"    {\"name\": \"%s (0x%06x)\", \"cat\": \"%s\", \"ph\": \"X\", \"pid\": %u, \"tid\": %u, \"ts\": %.3f, \"dur\": %.3f, \"args\": {\"seq\": %u, \"ll_tick\": %u, \"mod_iter\": %u, \"start_ns\": %llu, \"complete_ns\": %llu, \"deadline_ns\": %llu, \"start_cycles\": %llu, \"complete_cycles\": %llu, \"duration_cycles\": %u, \"cpc_budget\": %u, \"cpc_violation\": %u, \"bytes_in\": %u, \"bytes_out\": %u, \"wall_start_ns\": %llu, \"wall_complete_ns\": %llu}}%s\n",
							ev->widget_name, ev->comp_id, dom,
							ev->core_id, ev->pipeline_id,
							(double)ev->start_ns / 1000.0,
							(double)dur_ns / 1000.0,
							ev->seq_id, ev->ll_tick_nr, ev->mod_iter_nr,
							(unsigned long long)ev->start_ns,
							(unsigned long long)ev->complete_ns,
							(unsigned long long)ev->deadline_ns,
							(unsigned long long)ev->start_cycles,
							(unsigned long long)ev->complete_cycles,
							ev->duration_cycles, ev->cpc_budget,
							ev->cpc_violation, ev->bytes_in,
							ev->bytes_out,
							(unsigned long long)ev->wall_start_ns,
							(unsigned long long)ev->wall_complete_ns,
							(e + 1 < thdr->returned_events) ? "," : "");
					}
					fprintf(fp, "  ]\n}\n");
					fclose(fp);
					printf("Saved Chrome Trace JSON (%u events) to %s\n",
					       thdr->returned_events, json_file);
				} else {
					fprintf(stderr, "error: cannot write JSON trace to '%s'\n",
						json_file);
				}
			}
		} else {
			fprintf(stderr, "error: SCHED_TRACE failed (%d)\n", st);
		}
		free(rbuf);
		return st < 0 ? 1 : 0;
	}

	if (strcmp(cmd, "workload-sim") == 0) {
		const char *sub = (argc > 1) ? argv[1] : "get";

		if (strcmp(sub, "get") == 0 || strcmp(sub, "set") == 0) {
			struct sof_net_workload_sim_req wreq = {0};

			if (strcmp(sub, "set") == 0) {
				const char *m_arg = (argc > 2) ? argv[2] : "virtual";
				const char *d_arg = (argc > 3) ? argv[3] : "sram";

				wreq.op = SOF_WORKLOAD_SIM_OP_SET;
				if (strcasecmp(m_arg, "off") == 0 || strcmp(m_arg, "0") == 0)
					wreq.mode = 0; /* OFF */
				else if (strcasecmp(m_arg, "realtime") == 0 || strcmp(m_arg, "2") == 0)
					wreq.mode = 2; /* REALTIME */
				else
					wreq.mode = 1; /* VIRTUAL */

				wreq.default_mem_domain =
					(strncasecmp(d_arg, "dram", 4) == 0 || strcmp(d_arg, "1") == 0) ?
					1 : 0;
				wreq.dram_factor_x100 = (argc > 4) ? (uint32_t)atoi(argv[4]) : 180;
				wreq.dram_cycles_per_word = (argc > 5) ? (uint32_t)atoi(argv[5]) : 4;
			} else {
				wreq.op = SOF_WORKLOAD_SIM_OP_GET;
			}

			st = rpc_call(sock, SOF_NET_OP_WORKLOAD_SIM_CFG, &wreq, sizeof(wreq),
				      &rhdr, &rbuf);
			if (st == 0 && rbuf &&
			    rhdr.payload_len >= sizeof(struct sof_net_workload_sim_rsp)) {
				const struct sof_net_workload_sim_rsp *wrsp =
					(const struct sof_net_workload_sim_rsp *)rbuf;

				printf("WORKLOAD_SIM_CFG -> mode=%u mem=%s sram=%u.%02ux dram=%u.%02ux (+%u cyc/w) overrides=%u\n",
				       wrsp->mode,
				       wrsp->default_mem_domain == 1 ? "DRAM" : "SRAM",
				       wrsp->sram_factor_x100 / 100, wrsp->sram_factor_x100 % 100,
				       wrsp->dram_factor_x100 / 100, wrsp->dram_factor_x100 % 100,
				       wrsp->dram_cycles_per_word, wrsp->num_overrides);
				for (uint32_t c = 0; c < wrsp->num_cores && c < 8; c++) {
					printf("  [Core %u] freq=%u.%03u MHz active=%u util=%u.%02u%% overruns=%u LL=%u/%u kcps DP=%u/%u kcps\n",
					       c, wrsp->cores[c].freq_hz / 1000000U,
					       (wrsp->cores[c].freq_hz % 1000000U) / 1000U,
					       wrsp->cores[c].active,
					       wrsp->cores[c].total_util_x100 / 100,
					       wrsp->cores[c].total_util_x100 % 100,
					       wrsp->cores[c].ll_deadline_violations,
					       wrsp->cores[c].ll_avg_kcps, wrsp->cores[c].ll_peak_kcps,
					       wrsp->cores[c].dp_avg_kcps, wrsp->cores[c].dp_peak_kcps);
				}
			} else {
				fprintf(stderr, "error: WORKLOAD_SIM_CFG failed (%d)\n", st);
			}
			free(rbuf);
			return st < 0 ? 1 : 0;
		}
		return exec_joined_shell_cmd(sock, NULL, argc, argv, false);
	}

	if (strcmp(cmd, "llext-load") == 0) {
		if (argc < 2) {
			fprintf(stderr, "Usage: llext-load <file.llext|.so> [lib_id] [mod_name]\n");
			return 1;
		}
		uint32_t dlen = 0;
		uint8_t *dbytes = load_file_or_hex(argv[1], &dlen);

		if (!dbytes || dlen == 0) {
			fprintf(stderr, "error: cannot read module file '%s'\n", argv[1]);
			free(dbytes);
			return 1;
		}
		struct sof_net_llext_req lreq = {0};

		lreq.mode = SOF_LLEXT_MODE_IPC4_DIRECT;
		lreq.lib_id = (argc > 2) ? (uint32_t)atoi(argv[2]) : 1;
		lreq.data_size = dlen;
		lreq.issue_shell_cmd = 1;
		snprintf(lreq.mod_name, sizeof(lreq.mod_name), "%s",
			 (argc > 3) ? argv[3] : argv[1]);

		uint8_t *req_buf = calloc(1, sizeof(lreq) + dlen);

		memcpy(req_buf, &lreq, sizeof(lreq));
		memcpy(req_buf + sizeof(lreq), dbytes, dlen);
		free(dbytes);

		st = rpc_call(sock, SOF_NET_OP_LLEXT_LOAD, req_buf, sizeof(lreq) + dlen, &rhdr, &rbuf);
		free(req_buf);
		if (st == 0 && rbuf && rhdr.payload_len >= sizeof(struct sof_net_llext_rsp)) {
			const struct sof_net_llext_rsp *lrsp = (const struct sof_net_llext_rsp *)rbuf;

			printf("%s", lrsp->shell_out);
		} else {
			fprintf(stderr, "error: LLEXT_LOAD failed (%d)\n", st);
		}
		free(rbuf);
		return st < 0 ? 1 : 0;
	}

	if (strcmp(cmd, "probes") == 0) {
		const char *sub = (argc > 1) ? argv[1] : "avail";
		struct sof_net_probe_req preq = {0};

		if (strcmp(sub, "extract") == 0) {
			preq.op = SOF_PROBE_OP_READ_BUFFER;
			preq.stream_tag = (argc > 2) ? (uint32_t)strtoul(argv[2], NULL, 0) : 0;
			preq.dma_buffer_size = 256;
			st = rpc_call(sock, SOF_NET_OP_PROBE_CTL, &preq, sizeof(preq), &rhdr, &rbuf);
			if (st == 0 && rbuf && rhdr.payload_len >= sizeof(preq)) {
				const struct sof_net_probe_req *prsp =
					(const struct sof_net_probe_req *)rbuf;
				const int32_t *samps = (const int32_t *)(rbuf + sizeof(*prsp));
				uint32_t nsamp = prsp->num_points;

				printf("PROBE_EXTRACT(comp=0x%06x) -> %u samples (%u bytes)\n",
				       prsp->stream_tag, nsamp, prsp->dma_buffer_size);
				for (uint32_t i = 0; i < nsamp && i < 16; i++)
					printf("  sample[%2u] = %d\n", i, samps[i]);
				if (argc > 3 && prsp->dma_buffer_size > 0) {
					FILE *fp = fopen(argv[3], "wb");

					if (fp) {
						fwrite(samps, 1, prsp->dma_buffer_size, fp);
						fclose(fp);
						printf("Saved %u probe bytes to %s\n",
						       prsp->dma_buffer_size, argv[3]);
					}
				}
			} else {
				fprintf(stderr, "error: PROBE_EXTRACT failed (%d)\n", st);
			}
			free(rbuf);
			return st < 0 ? 1 : 0;
		}

		if (strcmp(sub, "active") == 0)
			preq.op = SOF_PROBE_OP_INFO_ACTIVE;
		else if (strcmp(sub, "init") == 0)
			preq.op = SOF_PROBE_OP_INIT;
		else if (strcmp(sub, "deinit") == 0)
			preq.op = SOF_PROBE_OP_DEINIT;
		else
			preq.op = SOF_PROBE_OP_INFO_AVAILABLE;
		preq.num_points = 64;

		st = rpc_call(sock, SOF_NET_OP_PROBE_CTL, &preq, sizeof(preq), &rhdr, &rbuf);
		if (st == 0 && rbuf && rhdr.payload_len >= sizeof(preq)) {
			const struct sof_net_probe_req *prsp = (const struct sof_net_probe_req *)rbuf;
			const struct sof_cdev_probe_point *pts =
				(const struct sof_cdev_probe_point *)(rbuf + sizeof(*prsp));

			printf("=== SOF Probe Points (%u found) ===\n", prsp->num_points);
			for (uint32_t i = 0; i < prsp->num_points; i++) {
				printf("  [%2u] buf_id=0x%06x pipe=%u purpose=%u widget='%s'\n",
				       i, pts[i].buffer_id, pts[i].stream_tag,
				       pts[i].purpose, pts[i].widget_name);
			}
		} else {
			fprintf(stderr, "error: PROBE_CTL failed (%d)\n", st);
		}
		free(rbuf);
		return st < 0 ? 1 : 0;
	}

	if (strcmp(cmd, "pcm") == 0) {
		const char *sub = (argc > 1) ? argv[1] : "status";
		struct sof_net_pcm_req preq = {0};
		uint8_t *play_data = NULL;
		uint32_t play_len = 0;

		snprintf(preq.device, sizeof(preq.device), "hw:0,0");
		preq.rate = 48000;
		preq.channels = 2;
		preq.freq_hz = 1000;

		if (strcmp(sub, "start-tone") == 0) {
			preq.op = SOF_PCM_OP_START_TONE;
			if (argc > 2)
				snprintf(preq.device, sizeof(preq.device), "%s", argv[2]);
			if (argc > 3)
				preq.freq_hz = (uint32_t)atoi(argv[3]);
			if (argc > 4)
				preq.rate = (uint32_t)atoi(argv[4]);
			if (argc > 5)
				preq.channels = (uint32_t)atoi(argv[5]);
		} else if (strcmp(sub, "stop-tone") == 0) {
			preq.op = SOF_PCM_OP_STOP_TONE;
		} else if (strcmp(sub, "play") == 0 && argc > 2) {
			preq.op = SOF_PCM_OP_PLAY_BUFFER;
			play_data = load_file_or_hex(argv[2], &play_len);
			preq.data_size = play_len;
			if (argc > 3)
				snprintf(preq.device, sizeof(preq.device), "%s", argv[3]);
			if (argc > 4)
				preq.rate = (uint32_t)atoi(argv[4]);
			if (argc > 5)
				preq.channels = (uint32_t)atoi(argv[5]);
		} else if (strcmp(sub, "capture") == 0) {
			preq.op = SOF_PCM_OP_CAPTURE_BUFFER;
			preq.duration_ms = (argc > 2) ? (uint32_t)atoi(argv[2]) : 100;
		} else {
			preq.op = SOF_PCM_OP_STATUS;
		}

		uint8_t *req_buf = calloc(1, sizeof(preq) + play_len);

		memcpy(req_buf, &preq, sizeof(preq));
		if (play_data && play_len > 0)
			memcpy(req_buf + sizeof(preq), play_data, play_len);
		free(play_data);

		st = rpc_call(sock, SOF_NET_OP_PCM_CTL, req_buf, sizeof(preq) + play_len, &rhdr, &rbuf);
		free(req_buf);
		if (st == 0 && rbuf && rhdr.payload_len >= sizeof(struct sof_net_pcm_rsp)) {
			const struct sof_net_pcm_rsp *prsp = (const struct sof_net_pcm_rsp *)rbuf;

			printf("PCM_CTL -> dev=%s tone=%s (%u Hz) %uHz/%uch frames=%llu rms=%u peak=%u captured=%uB\n",
			       prsp->active_device, prsp->tone_running ? "ON" : "OFF",
			       prsp->freq_hz, prsp->rate, prsp->channels,
			       (unsigned long long)prsp->frames_played,
			       prsp->rms_level, prsp->peak_level, prsp->data_size);
			if (preq.op == SOF_PCM_OP_CAPTURE_BUFFER && argc > 3 && prsp->data_size > 0) {
				FILE *fp = fopen(argv[3], "wb");

				if (fp) {
					fwrite(rbuf + sizeof(*prsp), 1, prsp->data_size, fp);
					fclose(fp);
					printf("Saved %u captured bytes to %s\n",
					       prsp->data_size, argv[3]);
				}
			}
		} else {
			fprintf(stderr, "error: PCM_CTL failed (%d)\n", st);
		}
		free(rbuf);
		return st < 0 ? 1 : 0;
	}

	if (strcmp(cmd, "cards") == 0) {
		st = rpc_call(sock, SOF_NET_OP_CARD_LIST, NULL, 0, &rhdr, &rbuf);
		if (st == 0 && rbuf && rhdr.payload_len >= sizeof(struct sof_net_card_list_hdr)) {
			const struct sof_net_card_list_hdr *chdr =
				(const struct sof_net_card_list_hdr *)rbuf;
			const struct sof_net_card_info *cards =
				(const struct sof_net_card_info *)(rbuf + sizeof(*chdr));
			const struct sof_net_pcm_node_info *nodes =
				(const struct sof_net_pcm_node_info *)(rbuf + sizeof(*chdr) +
				chdr->num_cards * sizeof(*cards));

			printf("=== Sound Cards (%u) & PCM/Compress Nodes (%u) ===\n",
			       chdr->num_cards, chdr->num_pcm_nodes);
			for (uint32_t i = 0; i < chdr->num_cards; i++) {
				printf("  Card %u: %s (%s) tplg='%s' pcms=%u compr=%u kctls=%u\n",
				       cards[i].card_id, cards[i].id, cards[i].name,
				       cards[i].tplg_file, cards[i].num_pcm_streams,
				       cards[i].num_compr_streams, cards[i].num_kcontrols);
			}
			for (uint32_t i = 0; i < chdr->num_pcm_nodes; i++) {
				printf("  PCM c%ud%u%c: %-24s state=%-8s %uHz/%uch/%s\n",
				       nodes[i].card_id, nodes[i].device_id,
				       nodes[i].direction == SOF_STREAM_DIR_CAPTURE ? 'c' : 'p',
				       nodes[i].name, nodes[i].state_str,
				       nodes[i].active_rate, nodes[i].active_channels,
				       nodes[i].active_format);
			}
		} else {
			fprintf(stderr, "error: CARD_LIST failed (%d)\n", st);
		}
		free(rbuf);
		return st < 0 ? 1 : 0;
	}

	if (strcmp(cmd, "pcm-caps") == 0) {
		struct sof_net_pcm_caps_req creq = {
			.card_id     = (argc > 1) ? (uint32_t)atoi(argv[1]) : 0xFFFFFFFFU,
			.device_id   = (argc > 2) ? (uint32_t)atoi(argv[2]) : 0xFFFFFFFFU,
			.dir_mask    = SOF_CAPS_DIR_ALL,
			.api_backend = SOF_AUDIO_API_ALSA,
		};
		st = rpc_call(sock, SOF_NET_OP_PCM_CAPS, &creq, sizeof(creq), &rhdr, &rbuf);
		if (st == 0 && rbuf && rhdr.payload_len >= sizeof(struct sof_net_pcm_caps_hdr)) {
			const struct sof_net_pcm_caps_hdr *chdr =
				(const struct sof_net_pcm_caps_hdr *)rbuf;
			const struct sof_net_pcm_caps *caps =
				(const struct sof_net_pcm_caps *)(rbuf + sizeof(*chdr));

			printf("=== PCM & Compress Capabilities (%u entries) ===\n",
			       chdr->num_entries);
			for (uint32_t i = 0; i < chdr->num_entries; i++) {
				printf("  [c%ud%u %s] %-24s rates=%u..%uHz ch=%u..%u fmts=0x%llx\n",
				       caps[i].card_id, caps[i].device_id,
				       caps[i].direction == SOF_STREAM_DIR_CAPTURE ? "CAP" : "PB ",
				       caps[i].name, caps[i].rate_min, caps[i].rate_max,
				       caps[i].channels_min, caps[i].channels_max,
				       (unsigned long long)caps[i].formats_mask);
			}
		} else {
			fprintf(stderr, "error: PCM_CAPS failed (%d)\n", st);
		}
		free(rbuf);
		return st < 0 ? 1 : 0;
	}

	if (strcmp(cmd, "dapm") == 0) {
		struct sof_net_dapm_req dreq = { .card_id = 0, .flags = 0 };

		st = rpc_call(sock, SOF_NET_OP_DAPM_GRAPH, &dreq, sizeof(dreq), &rhdr, &rbuf);
		if (st == 0 && rbuf && rhdr.payload_len >= sizeof(struct sof_net_dapm_graph_hdr)) {
			const struct sof_net_dapm_graph_hdr *dhdr =
				(const struct sof_net_dapm_graph_hdr *)rbuf;
			size_t nodes_off = sizeof(*dhdr) +
					   dhdr->num_components * sizeof(struct sof_net_dapm_comp_status);
			size_t edges_off = nodes_off +
					   dhdr->num_nodes * sizeof(struct sof_net_dapm_node);
			const struct sof_net_dapm_node *nodes =
				(const struct sof_net_dapm_node *)(rbuf + nodes_off);
			const struct sof_net_dapm_edge *edges =
				(const struct sof_net_dapm_edge *)(rbuf + edges_off);

			printf("DAPM_GRAPH -> card='%s' tplg='%s' nodes=%u (%u ON) edges=%u (%u active) links=%u\n",
			       dhdr->card_name, dhdr->tplg_file,
			       dhdr->num_nodes, dhdr->num_powered_nodes,
			       dhdr->num_edges, dhdr->num_active_edges,
			       dhdr->num_dpcm_links);
			if (argc > 1 && rhdr.payload_len >= edges_off + dhdr->num_edges * sizeof(*edges)) {
				FILE *fp = fopen(argv[1], "w");

				if (fp) {
					fprintf(fp, "digraph DAPM {\n  rankdir=LR;\n");
					for (uint32_t i = 0; i < dhdr->num_nodes; i++) {
						fprintf(fp, "  \"%s\" [label=\"%s\\n(%s, pipe %d)\"];\n",
							nodes[i].name, nodes[i].name,
							nodes[i].widget_type, nodes[i].pipeline_id);
					}
					for (uint32_t i = 0; i < dhdr->num_edges; i++) {
						fprintf(fp, "  \"%s\" -> \"%s\";\n",
							edges[i].src_name, edges[i].dst_name);
					}
					fprintf(fp, "}\n");
					fclose(fp);
					printf("Saved DOT graph to %s\n", argv[1]);
				}
			}
		} else {
			fprintf(stderr, "error: DAPM_GRAPH failed (%d)\n", st);
		}
		free(rbuf);
		return st < 0 ? 1 : 0;
	}

	if (strcmp(cmd, "compr") == 0) {
		const char *sub = (argc > 1) ? argv[1] : "caps";
		struct sof_net_compr_req creq = {0};

		if (strcmp(sub, "play") == 0)
			creq.op = SOF_COMPR_OP_PLAY_BUFFER;
		else if (strcmp(sub, "capture") == 0)
			creq.op = SOF_COMPR_OP_CAPTURE_BUFFER;
		else
			creq.op = SOF_COMPR_OP_GET_CAPS;

		creq.card_id = (argc > 2) ? (uint32_t)atoi(argv[2]) : 0;
		creq.device_id = (argc > 3) ? (uint32_t)atoi(argv[3]) : 0;
		creq.rate = 48000;
		creq.channels = 2;

		st = rpc_call(sock, SOF_NET_OP_COMPR_CTL, &creq, sizeof(creq), &rhdr, &rbuf);
		if (st == 0 && rbuf && rhdr.payload_len >= sizeof(struct sof_net_compr_rsp)) {
			const struct sof_net_compr_rsp *crsp =
				(const struct sof_net_compr_rsp *)rbuf;

			printf("COMPR_CTL(op=%u) -> status=%d codecs=%u codec0=0x%x xfer=%uB rate=%uHz\n",
			       creq.op, crsp->status, crsp->num_codecs,
			       crsp->codecs[0], crsp->bytes_transferred,
			       crsp->sampling_rate);
		} else {
			fprintf(stderr, "error: COMPR_CTL failed (%d)\n", st);
		}
		free(rbuf);
		return st < 0 ? 1 : 0;
	}

	if (strcmp(cmd, "kctl") == 0) {
		const char *sub = (argc > 1) ? argv[1] : "list";

		if (strcmp(sub, "list") == 0) {
			struct sof_net_kctl_list_req kreq = {
				.card_id = 0,
				.api_backend = SOF_AUDIO_API_ALSA,
			};
			st = rpc_call(sock, SOF_NET_OP_KCTL_LIST, &kreq, sizeof(kreq), &rhdr, &rbuf);
			if (st == 0 && rbuf &&
			    rhdr.payload_len >= sizeof(struct sof_net_kctl_list_hdr)) {
				const struct sof_net_kctl_list_hdr *khdr =
					(const struct sof_net_kctl_list_hdr *)rbuf;
				const struct sof_net_kctl_info *kinfo =
					(const struct sof_net_kctl_info *)(rbuf + sizeof(*khdr));

				printf("=== ALSA Kcontrols (%u entries) ===\n", khdr->num_kcontrols);
				for (uint32_t i = 0; i < khdr->num_kcontrols; i++) {
					printf("  [%2u] %-44s type=%u count=%u val0=%lld\n",
					       kinfo[i].numid, kinfo[i].name, kinfo[i].type,
					       kinfo[i].count, (long long)kinfo[i].values[0]);
				}
			} else {
				fprintf(stderr, "error: KCTL_LIST failed (%d)\n", st);
			}
			free(rbuf);
			return st < 0 ? 1 : 0;
		}

		if ((strcmp(sub, "get") == 0 || strcmp(sub, "set") == 0 ||
		     strcmp(sub, "set-bytes") == 0) && argc > 2) {
			struct sof_net_kctl_io_req kreq = {0};
			uint16_t op = (strcmp(sub, "get") == 0) ?
				      SOF_NET_OP_KCTL_GET : SOF_NET_OP_KCTL_SET;

			if (isdigit((unsigned char)argv[2][0]))
				kreq.numid = (uint32_t)atoi(argv[2]);
			else
				snprintf(kreq.name, sizeof(kreq.name), "%s", argv[2]);

			if (strcmp(sub, "set") == 0) {
				uint32_t cnt = 0;

				for (int i = 3; i < argc && cnt < 8; i++)
					kreq.values[cnt++] = strtoll(argv[i], NULL, 0);
				kreq.count = cnt;
			} else if (strcmp(sub, "set-bytes") == 0 && argc > 3) {
				uint32_t blen = 0;
				uint8_t *bdata = load_file_or_hex(argv[3], &blen);

				if (bdata && blen > 0) {
					if (blen > sizeof(kreq.bytes_data))
						blen = sizeof(kreq.bytes_data);
					memcpy(kreq.bytes_data, bdata, blen);
					kreq.bytes_len = blen;
				}
				free(bdata);
			}

			st = rpc_call(sock, op, &kreq, sizeof(kreq), &rhdr, &rbuf);
			if (st == 0 && rbuf &&
			    rhdr.payload_len >= sizeof(struct sof_net_kctl_io_rsp)) {
				const struct sof_net_kctl_io_rsp *krsp =
					(const struct sof_net_kctl_io_rsp *)rbuf;

				printf("KCTL %s [%u] '%s' -> val=[%lld, %lld] bytes=%u\n",
				       sub, krsp->info.numid, krsp->info.name,
				       (long long)krsp->info.values[0],
				       (long long)krsp->info.values[1],
				       krsp->bytes_len);
			} else {
				fprintf(stderr, "error: KCTL_%s failed (%d)\n", sub, st);
			}
			free(rbuf);
			return st < 0 ? 1 : 0;
		}
	}

	if (strcmp(cmd, "shell") == 0) {
		if (argc < 2) {
			fprintf(stderr, "Usage: shell <cmd...>\n");
			return 1;
		}
		return exec_joined_shell_cmd(sock, NULL, argc - 1, argv + 1, false);
	}

	/* First-class testbench shell commands (run-test, tplg-load, tplg-unload, io-cfg, etc.) */
	return exec_joined_shell_cmd(sock, NULL, argc, argv, strcmp(cmd, "run-test") == 0);
}

static int tokenize_line(char *line, char **argv, int max_args)
{
	int argc = 0;
	char *p = line;

	while (*p && argc < max_args) {
		while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
			p++;
		if (!*p || *p == '#')
			break;
		if (*p == '"' || *p == '\'') {
			char q = *p++;

			argv[argc++] = p;
			while (*p && *p != q)
				p++;
			if (*p == q)
				*p++ = '\0';
		} else {
			argv[argc++] = p;
			while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
				p++;
			if (*p)
				*p++ = '\0';
		}
	}
	return argc;
}

static int run_interactive_stream(int sock, FILE *in_fp, bool show_prompt)
{
	char line[4096];
	char *argv[128];
	int rc = 0;

	while (true) {
		if (show_prompt) {
			printf("sof-tb> ");
			fflush(stdout);
		}
		if (!fgets(line, sizeof(line), in_fp))
			break;
		int l_argc = tokenize_line(line, argv, 128);

		if (l_argc <= 0)
			continue;
		if (strcmp(argv[0], "exit") == 0)
			break;
		int cmd_rc = dispatch_command(sock, l_argc, argv);

		if (cmd_rc != 0)
			rc = cmd_rc;
		if (strcmp(argv[0], "quit") == 0)
			break;
	}
	return rc;
}

int main(int argc, char **argv)
{
	const char *host = "127.0.0.1";
	int port = SOF_NET_DEFAULT_PORT;
	const char *script_file = NULL;
	int arg_idx = 1;

	while (arg_idx < argc && argv[arg_idx][0] == '-') {
		if (strcmp(argv[arg_idx], "-H") == 0 && arg_idx + 1 < argc) {
			host = argv[arg_idx + 1];
			arg_idx += 2;
		} else if (strcmp(argv[arg_idx], "-p") == 0 && arg_idx + 1 < argc) {
			port = atoi(argv[arg_idx + 1]);
			arg_idx += 2;
		} else if (strcmp(argv[arg_idx], "-f") == 0 && arg_idx + 1 < argc) {
			script_file = argv[arg_idx + 1];
			arg_idx += 2;
		} else if (strcmp(argv[arg_idx], "-h") == 0 ||
			   strcmp(argv[arg_idx], "--help") == 0) {
			print_usage(argv[0]);
			return 0;
		} else {
			break;
		}
	}

	int sock = connect_server(host, port);

	if (sock < 0) {
		fprintf(stderr, "error: failed to connect to %s:%d\n", host, port);
		return 1;
	}

	int ret = 0;

	if (script_file) {
		FILE *fp = fopen(script_file, "r");

		if (!fp) {
			fprintf(stderr, "error: cannot open script file '%s'\n", script_file);
			close(sock);
			return 1;
		}
		ret = run_interactive_stream(sock, fp, false);
		fclose(fp);
	} else if (arg_idx >= argc || strcmp(argv[arg_idx], "repl") == 0) {
		ret = run_interactive_stream(sock, stdin, isatty(STDIN_FILENO));
	} else {
		ret = dispatch_command(sock, argc - arg_idx, argv + arg_idx);
	}

	close(sock);
	return ret;
}
