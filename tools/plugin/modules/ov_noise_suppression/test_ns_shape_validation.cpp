// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.
//
// Author: Pally Mediratta <mediratta01.pally@gmail.com>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>

#include "noise_suppression_interface.h"

static void usage(const char *prog)
{
	std::printf(
"Usage: %s [--model PATH | --dim N [--dim N ...]] [--expect ok|reject]\n"
"\n"
"Regression test for the OpenVINO noise-suppression plugin's input-shape\n"
"validation (commit \"tools: plugin: ov_noise_suppression: validate model\n"
"input shape dimensions\"). Invokes ov_ns_init() against either:\n"
"\n"
"  --model PATH          a caller-supplied OpenVINO model XML file, or\n"
"  --dim N [--dim N ...] a minimal synthesized model XML built from the\n"
"                        given input port dimensions and written to a\n"
"                        temporary file.\n"
"\n"
"  --expect ok|reject    expected outcome (default: reject).\n"
"                        \"ok\":     ov_ns_init() must return 0.\n"
"                        \"reject\": ov_ns_init() must return non-zero.\n"
"\n"
"Returns 0 on PASS, 1 on FAIL, 2 on usage error.\n"
"\n"
"Examples:\n"
"  %s --dim 0 --dim 480                          # zero dim,  expect reject\n"
"  %s --dim 9223372036854775807 --dim 1024       # overflow,  expect reject\n"
"  %s --model real_model.xml --expect ok         # real model, expect accept\n",
		prog, prog, prog, prog);
}

static int write_mock_model(const std::string &path,
			    const std::vector<long long> &dims)
{
	std::ofstream f(path);
	if (!f)
		return -1;

	f << "<?xml version=\"1.0\"?><net><layers>"
	     "<layer id=\"0\" name=\"input\" type=\"Parameter\">"
	     "<output><port id=\"0\" precision=\"FP32\">";
	for (auto d : dims)
		f << "<dim>" << d << "</dim>";
	f << "</port></output></layer></layers></net>";
	return f ? 0 : -1;
}

int main(int argc, char **argv)
{
	std::string model_path;
	std::vector<long long> dims;
	bool expect_reject = true;

	for (int i = 1; i < argc; i++) {
		std::string a = argv[i];
		if (a == "-h" || a == "--help") {
			usage(argv[0]);
			return 0;
		}
		if (a == "--model" && i + 1 < argc) {
			model_path = argv[++i];
		} else if (a == "--dim" && i + 1 < argc) {
			dims.push_back(std::strtoll(argv[++i], nullptr, 0));
		} else if (a == "--expect" && i + 1 < argc) {
			std::string v = argv[++i];
			if (v == "ok") {
				expect_reject = false;
			} else if (v == "reject") {
				expect_reject = true;
			} else {
				std::fprintf(stderr,
					     "unknown --expect value: %s\n",
					     v.c_str());
				usage(argv[0]);
				return 2;
			}
		} else {
			std::fprintf(stderr, "unknown or incomplete arg: %s\n",
				     a.c_str());
			usage(argv[0]);
			return 2;
		}
	}

	std::string scratch;
	if (model_path.empty()) {
		if (dims.empty()) {
			std::fprintf(stderr,
				     "must supply --model PATH or one or more --dim N\n");
			usage(argv[0]);
			return 2;
		}

		char tmpl[] = "/tmp/ns_shape_test_XXXXXX.xml";
		int fd = mkstemps(tmpl, 4);
		if (fd < 0) {
			std::perror("mkstemps");
			return 2;
		}
		close(fd);
		scratch = tmpl;
		if (write_mock_model(scratch, dims) != 0) {
			std::fprintf(stderr, "failed to write mock model %s\n",
				     scratch.c_str());
			std::remove(scratch.c_str());
			return 2;
		}
		model_path = scratch;
	}

	struct noise_suppression_data *nd =
		(struct noise_suppression_data *)std::calloc(1, sizeof(*nd));
	if (!nd) {
		std::perror("calloc");
		if (!scratch.empty())
			std::remove(scratch.c_str());
		return 2;
	}

	int ret = ov_ns_init(nd, model_path.c_str());

	if (!scratch.empty())
		std::remove(scratch.c_str());

	bool rejected = (ret != 0);
	bool pass = (rejected == expect_reject);

	std::printf("%s: ov_ns_init() returned %d (%s), expected %s\n",
		    pass ? "PASS" : "FAIL",
		    ret,
		    rejected ? "rejected" : "accepted",
		    expect_reject ? "reject" : "ok");

	ov_ns_free(nd);
	std::free(nd);

	return pass ? 0 : 1;
}
