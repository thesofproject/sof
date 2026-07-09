#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) 2026, Intel Corporation.
#
# sof_mls_freq_resp_local.sh
#
# Detect a suitable local ALSA speaker (aplay -l) + microphone
# (arecord -l, preferring a calibrated USB measurement mic), generate the
# mls_play_config.txt and mls_rec_config.txt files that sof_mls_freq_resp.m
# expects, and optionally run the Octave measurement into a data
# subdirectory. The id string is derived from DMI vendor + product so the
# output filenames identify the host that produced them.
#
# Typical use:
#   ./sof_mls_freq_resp_local.sh              # detect + write configs
#   ./sof_mls_freq_resp_local.sh --run        # + invoke Octave right away
#
# The Octave measurement can also be launched manually later with the
# printed command line.

set -euo pipefail

# --------------------------------------------------------------------------
# Defaults & CLI parsing
# --------------------------------------------------------------------------

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
data_dir="sof_mls_freq_resp_data"
id=""
id_auto=1                # 0 once --id STR was supplied by the user
append_mic_tag=1         # 0 with --no-mic-tag
speaker_override=""
mic_override=""
mic_channels=""          # empty = auto-probe max from arecord
speaker_channels=2
rec_fmt_override=""      # non-empty with --rec-fmt to skip auto-probe
rec_cal=""               # optional mic calibration file passed to rec.cal
do_run=0
dry_run=0
do_prep=0
iir_pass_blob="$script_dir/../../../../tools/ctl/ipc4/eq_iir/pass.txt"
fir_pass_blob="$script_dir/../../../../tools/ctl/ipc4/eq_fir/pass.txt"

usage() {
	cat <<EOF
Usage: $(basename "$0") [options]

Options:
  --dir DIR           Output directory (default: $data_dir).
  --id STR            Measurement id (default: derived from DMI vendor +
                      product + a sanitized microphone tag). When --id
                      is given, it is used verbatim and the mic tag is
                      not appended.
  --no-mic-tag        Do not append the microphone tag to the auto-
                      derived id. Useful when re-measuring with the same
                      mic and you want to overwrite the previous run.
  --speaker hw:C,D    Force ALSA speaker device (skip auto-detect).
  --mic     hw:C,D    Force ALSA microphone device (skip auto-detect).
  --mic-ch  N         Microphone channel count for capture (default:
                      max supported by the mic, probed with
                      arecord --dump-hw-params).
  --spk-ch  N         Speaker channel count for playback (default: 2).
  --rec-fmt FMT       Force ALSA sample format for capture (e.g. S32_LE).
                      Default: probe with arecord --dump-hw-params and
                      prefer S32_LE > S16_LE.
  --cal PATH          Path to a two-column [freq_hz, mag_db] microphone
                      calibration file. Written verbatim into rec.cal
                      so sof_mls_freq_resp.m compensates the measured
                      response with it.
  --prep              Before measuring, program pass-through IIR/FIR blobs
                      and switch DRC/MBDRC/TDFB controls off on the
                      speaker card so the response is not shaped by any
                      leftover DSP state. Uses amixer + sof-ctl.
  --iir-blob PATH     Override the IIR pass-through blob used by --prep.
                      (default: tools/ctl/ipc4/eq_iir/pass.txt)
  --fir-blob PATH     Override the FIR pass-through blob used by --prep.
                      (default: tools/ctl/ipc4/eq_fir/pass.txt)
  --run               Invoke Octave to run sof_mls_freq_resp() right after
                      writing the configs.
  --dry-run           Detect and print what would be written; do not touch
                      the filesystem or DSP controls.
  -h, --help          Show this help.
EOF
}

while [[ $# -gt 0 ]]; do
	case "$1" in
		--dir)       data_dir="$2"; shift 2 ;;
		--id)        id="$2"; id_auto=0; shift 2 ;;
		--no-mic-tag) append_mic_tag=0; shift ;;
		--speaker)   speaker_override="$2"; shift 2 ;;
		--mic)       mic_override="$2"; shift 2 ;;
		--mic-ch)    mic_channels="$2"; shift 2 ;;
		--spk-ch)    speaker_channels="$2"; shift 2 ;;
		--rec-fmt)   rec_fmt_override="$2"; shift 2 ;;
		--cal)       rec_cal="$2"; shift 2 ;;
		--prep)      do_prep=1; shift ;;
		--iir-blob)  iir_pass_blob="$2"; shift 2 ;;
		--fir-blob)  fir_pass_blob="$2"; shift 2 ;;
		--run)       do_run=1; shift ;;
		--dry-run)   dry_run=1; shift ;;
		-h|--help)   usage; exit 0 ;;
		*)           echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
	esac
done

# --------------------------------------------------------------------------
# Dependencies
# --------------------------------------------------------------------------

need() {
	command -v "$1" >/dev/null 2>&1 || {
		echo "Required command '$1' not found in PATH" >&2
		exit 3
	}
}
need aplay
need arecord
need awk
# octave is only invoked when --run is given; skip the check in the
# default "generate configs only" mode so the script works on hosts
# without Octave installed.
if (( do_run )); then
	need octave
fi

extract_card() {
	# "hw:0,0" -> "0". Used both by the prep step and by --dry-run.
	echo "$1" | sed -n 's/^hw:\([0-9]\+\).*/\1/p'
}

probe_capture_format() {
	# Ask arecord which sample formats the device advertises via
	# --dump-hw-params and pick one we know how to write in the rec
	# config, preferring S32_LE over S16_LE (the DMIC PCM on many SOF
	# platforms only exposes S32_LE). Falls back to S16_LE if the probe
	# returns nothing usable (device busy, unknown output format) so
	# the config file always has a definite value.
	local dev="$1" formats want f
	formats=$(arecord -D "$dev" --dump-hw-params 2>&1 \
	          | sed -n 's/^FORMAT:[[:space:]]*//p' | head -1)
	if [[ -n "$formats" ]]; then
		for want in S32_LE S16_LE; do
			for f in $formats; do
				if [[ "$f" == "$want" ]]; then
					echo "$want"
					return 0
				fi
			done
		done
		# Nothing on our preference list; take the first advertised.
		echo "$formats" | awk '{print $1}'
		return 0
	fi
	echo "S16_LE"
}

format_to_bits() {
	case "$1" in
		S32_LE|S32_BE|FLOAT_LE|FLOAT_BE) echo 32 ;;
		S24_LE|S24_BE|S24_3LE|S24_3BE)   echo 24 ;;
		S16_LE|S16_BE)                    echo 16 ;;
		*)                                echo 16 ;;
	esac
}

probe_capture_channels() {
	# Return the max channel count arecord --dump-hw-params reports
	# for the device, or empty string on failure. Handles both the
	# single-value ("CHANNELS: 2") and range ("CHANNELS: [1 4]") forms.
	local dev="$1" line n max=0
	line=$(arecord -D "$dev" --dump-hw-params 2>&1 \
	       | sed -n 's/^CHANNELS:[[:space:]]*//p' | head -1)
	[[ -z "$line" ]] && return 0
	line=${line//[/}
	line=${line//]/}
	for n in $line; do
		if [[ "$n" =~ ^[0-9]+$ ]] && (( n > max )); then
			max=$n
		fi
	done
	(( max > 0 )) && echo "$max"
}

# --------------------------------------------------------------------------
# DMI-derived id
# --------------------------------------------------------------------------

dmi_read() {
	# Read /sys/devices/virtual/dmi/id/<key>, or empty on failure.
	local key="$1" val=""
	if [[ -r "/sys/devices/virtual/dmi/id/$key" ]]; then
		val=$(cat "/sys/devices/virtual/dmi/id/$key" 2>/dev/null | tr -d '\n')
	fi
	printf '%s' "$val"
}

sanitize_id() {
	# Lower case, keep [a-z0-9], collapse everything else into a single _
	# and trim leading/trailing underscores.
	echo "$1" | tr '[:upper:]' '[:lower:]' \
	          | sed -e 's/[^a-z0-9]\+/_/g' -e 's/^_\+//' -e 's/_\+$//'
}

# Read the raw DMI strings once so both the sanitized id below and the
# report at the end can show the exact values that ALSA UCM and the
# UCM blob generator expect (they must match /sys/devices/virtual/dmi/id
# verbatim, including case and whitespace).
dmi_vendor_raw=$(dmi_read sys_vendor)
dmi_product_raw=$(dmi_read product_name)

if [[ -z "$id" ]]; then
	vendor=$(sanitize_id "$dmi_vendor_raw")
	product=$(sanitize_id "$dmi_product_raw")
	if [[ -n "$vendor" && -n "$product" ]]; then
		id="${vendor}_${product}"
	elif [[ -n "$product" ]]; then
		id="$product"
	elif [[ -n "$vendor" ]]; then
		id="$vendor"
	else
		id="unknown"
	fi
fi

# --------------------------------------------------------------------------
# ALSA device discovery
# --------------------------------------------------------------------------

# Parse `aplay -l` / `arecord -l` lines of the form:
#   card N: SHORT [LONG], device D: DESCR (...) [SUBNAME]
# Emits: "N|D|SHORT|LONG|DESCR" per PCM device.
#
# Pure POSIX-ERE sed (invoked with -E) instead of awk. gawk's 3-argument
# match() form is not portable to mawk (Ubuntu default /usr/bin/awk) or
# busybox awk (Alpine), so a sed-only extractor keeps the script working
# regardless of which awk implementation is installed.
parse_alsa_list() {
	sed -nE '
	/^card [0-9]+: [^ ]+ \[[^]]+\], device [0-9]+: / {
		# Rewrite the line as c|d|sn|ln|desc, then peel off the trailing
		# " [SUBNAME]" and then the " (...)" annotation from desc.
		# Bracket-strip has to run first so a line like
		#   ... device 0: Jack Out (*) []
		# does not leave the parenthesised part dangling at end-of-line.
		s/^card ([0-9]+): ([^ ]+) \[([^]]+)\], device ([0-9]+): (.*)$/\1|\4|\2|\3|\5/
		s/ *\[[^]]*\] *$//
		s/ *\([^)]*\) *$//
		p
	}
	'
}

playback_devices=$(aplay -l 2>/dev/null | parse_alsa_list)
capture_devices=$(arecord -l 2>/dev/null | parse_alsa_list)

if [[ -z "$playback_devices" ]]; then
	echo "aplay -l returned no PCM devices" >&2
	exit 4
fi
if [[ -z "$capture_devices" ]]; then
	echo "arecord -l returned no PCM devices" >&2
	exit 4
fi

# --------------------------------------------------------------------------
# Speaker selection
# --------------------------------------------------------------------------
#
# Preference order:
#   1) Any PCM whose descriptor contains "Speaker" (common on SOF+SDW,
#      where the aplay -l descriptors are literal endpoint names such as
#      "Jack Out", "Speaker", "HDMI1", "Deepbuffer Jack Out"). This has
#      to run before the analog rules because SDW cards do not advertise
#      "Analog" in their descriptors and would otherwise fall through to
#      the "first non-HDMI/DP" pass and pick "Jack Out".
#   2) SOF card ("sof*" short name) with an "Analog" descriptor that is
#      not "Deepbuffer".
#   3) Any card device whose descriptor contains "Analog" but not "HDMI",
#      "DP" or "Deepbuffer".
#   4) First non-HDMI/DP/Deepbuffer device.
# HDMI/DP outputs are always skipped so the mic never accidentally captures
# monitor speaker output.

pick_speaker() {
	local line c d sn ln desc lc_ln lc_desc

	# Pass 1: descriptor explicitly named "Speaker"
	while IFS='|' read -r c d sn ln desc; do
		[[ -z "$c" ]] && continue
		lc_desc=$(echo "$desc" | tr '[:upper:]' '[:lower:]')
		if [[ "$lc_desc" == *speaker* ]] \
		   && [[ "$lc_desc" != *hdmi* ]] \
		   && [[ "$lc_desc" != *dp* ]]; then
			echo "hw:$c,$d|$sn|$ln|$desc|speaker"
			return
		fi
	done <<<"$playback_devices"

	# Pass 2: SOF analog
	while IFS='|' read -r c d sn ln desc; do
		[[ -z "$c" ]] && continue
		lc_desc=$(echo "$desc" | tr '[:upper:]' '[:lower:]')
		lc_ln=$(echo "$ln" | tr '[:upper:]' '[:lower:]')
		if [[ "$lc_ln" == sof* ]] \
		   && [[ "$lc_desc" == *analog* ]] \
		   && [[ "$lc_desc" != *deepbuffer* ]] \
		   && [[ "$lc_desc" != *hdmi* ]] \
		   && [[ "$lc_desc" != *dp* ]]; then
			echo "hw:$c,$d|$sn|$ln|$desc|sof-analog"
			return
		fi
	done <<<"$playback_devices"

	# Pass 3: any analog, still skipping HDMI, DP and Deepbuffer.
	while IFS='|' read -r c d sn ln desc; do
		[[ -z "$c" ]] && continue
		lc_desc=$(echo "$desc" | tr '[:upper:]' '[:lower:]')
		if [[ "$lc_desc" == *analog* ]] \
		   && [[ "$lc_desc" != *deepbuffer* ]] \
		   && [[ "$lc_desc" != *hdmi* ]] \
		   && [[ "$lc_desc" != *dp* ]]; then
			echo "hw:$c,$d|$sn|$ln|$desc|analog"
			return
		fi
	done <<<"$playback_devices"

	# Pass 4: first non-hdmi/dp/deepbuffer
	while IFS='|' read -r c d sn ln desc; do
		[[ -z "$c" ]] && continue
		lc_desc=$(echo "$desc" | tr '[:upper:]' '[:lower:]')
		if [[ "$lc_desc" != *hdmi* ]] \
		   && [[ "$lc_desc" != *dp* ]] \
		   && [[ "$lc_desc" != *deepbuffer* ]]; then
			echo "hw:$c,$d|$sn|$ln|$desc|first-non-hdmi"
			return
		fi
	done <<<"$playback_devices"

	return 1
}

# --------------------------------------------------------------------------
# Microphone selection
# --------------------------------------------------------------------------
#
# Preference order:
#   1) A known calibrated measurement mic by long-name substring
#      (UMM-6, UMIK, MiniDSP, EMM-6, ECM8000).
#   2) Any USB-Audio-class card that is not obviously a webcam or a full
#      interface (best-effort heuristic on the long name).
#   3) A SOF DMIC device (internal digital mic array).
#   4) The SOF analog line-in / any first analog capture.

is_measurement_mic() {
	local lc="$1"
	case "$lc" in
		*umm-6*|*umm6*|*umik*|*minidsp*|*emm-6*|*emm6*|*ecm8000*)
			return 0 ;;
		*) return 1 ;;
	esac
}

is_webcam() {
	local lc="$1"
	case "$lc" in
		*webcam*|*c920*|*c930*|*brio*|*logitech*camera*|*camera*)
			return 0 ;;
		*) return 1 ;;
	esac
}

pick_mic() {
	local line c d sn ln desc lc_ln lc_desc lc_sn

	# Pass 1: known measurement mics
	while IFS='|' read -r c d sn ln desc; do
		[[ -z "$c" ]] && continue
		lc_ln=$(echo "$ln" | tr '[:upper:]' '[:lower:]')
		lc_sn=$(echo "$sn" | tr '[:upper:]' '[:lower:]')
		if is_measurement_mic "$lc_ln" || is_measurement_mic "$lc_sn"; then
			echo "hw:$c,$d|$sn|$ln|$desc|measurement-usb"
			return
		fi
	done <<<"$capture_devices"

	# Pass 2: generic USB audio card, not a webcam
	while IFS='|' read -r c d sn ln desc; do
		[[ -z "$c" ]] && continue
		lc_ln=$(echo "$ln" | tr '[:upper:]' '[:lower:]')
		lc_desc=$(echo "$desc" | tr '[:upper:]' '[:lower:]')
		lc_sn=$(echo "$sn" | tr '[:upper:]' '[:lower:]')
		if [[ "$lc_sn" == usb* || "$lc_desc" == *usb* ]] \
		   && ! is_webcam "$lc_ln"; then
			echo "hw:$c,$d|$sn|$ln|$desc|usb-generic"
			return
		fi
	done <<<"$capture_devices"

	# Pass 3: SOF DMIC (built-in digital mic array)
	while IFS='|' read -r c d sn ln desc; do
		[[ -z "$c" ]] && continue
		lc_ln=$(echo "$ln" | tr '[:upper:]' '[:lower:]')
		lc_desc=$(echo "$desc" | tr '[:upper:]' '[:lower:]')
		if [[ "$lc_ln" == sof* ]] && [[ "$lc_desc" == *dmic* ]]; then
			echo "hw:$c,$d|$sn|$ln|$desc|sof-dmic"
			return
		fi
	done <<<"$capture_devices"

	# Pass 4: SOF / first analog capture
	while IFS='|' read -r c d sn ln desc; do
		[[ -z "$c" ]] && continue
		lc_desc=$(echo "$desc" | tr '[:upper:]' '[:lower:]')
		if [[ "$lc_desc" == *analog* ]]; then
			echo "hw:$c,$d|$sn|$ln|$desc|analog-capture"
			return
		fi
	done <<<"$capture_devices"

	# Pass 5: give up and take the very first entry
	IFS='|' read -r c d sn ln desc <<<"$(echo "$capture_devices" | head -n 1)"
	echo "hw:$c,$d|$sn|$ln|$desc|first-listed"
}

# --------------------------------------------------------------------------
# Resolve devices
# --------------------------------------------------------------------------

if [[ -n "$speaker_override" ]]; then
	speaker_hw="$speaker_override"
	speaker_info="override"
	speaker_name="$speaker_override"
else
	IFS='|' read -r speaker_hw _spk_sn speaker_name _spk_desc speaker_info \
		<<<"$(pick_speaker || true)"
	if [[ -z "${speaker_hw:-}" ]]; then
		echo "Could not identify a suitable speaker device from aplay -l" >&2
		aplay -l >&2
		exit 5
	fi
fi

if [[ -n "$mic_override" ]]; then
	mic_hw="$mic_override"
	mic_info="override"
	# Try to look the override up in the capture list so the mic tag is
	# still descriptive (e.g. dmic_raw) instead of just the hw address.
	ov_card=$(extract_card "$mic_override")
	ov_dev=$(echo "$mic_override" | sed -n 's/^hw:[0-9]\+,\([0-9]\+\).*/\1/p')
	mic_name="$mic_override"
	mic_desc="$mic_override"
	if [[ -n "$ov_card" && -n "$ov_dev" ]]; then
		while IFS='|' read -r c d _sn ln desc; do
			if [[ "$c" == "$ov_card" && "$d" == "$ov_dev" ]]; then
				mic_name="$ln"
				mic_desc="$desc"
				break
			fi
		done <<<"$capture_devices"
	fi
else
	IFS='|' read -r mic_hw _mic_sn mic_name mic_desc mic_info \
		<<<"$(pick_mic)"
	if [[ -z "${mic_hw:-}" ]]; then
		echo "Could not identify any capture device from arecord -l" >&2
		arecord -l >&2
		exit 5
	fi
fi

# --------------------------------------------------------------------------
# Microphone tag for the id
# --------------------------------------------------------------------------
#
# For USB / measurement mics the long name ("MiniDSP UMM-6", "HD Pro
# Webcam C920") is the most identifiable label. For the SOF card the
# long name collapses to "sof-hda-dsp" for every capture PCM on the
# card, so fall back to the PCM descriptor ("DMIC Raw", "HDA Analog")
# to distinguish e.g. the DMIC array from a wired line-in. If both are
# generic, fall back to the hw:C,D address so we always get something
# unique.

mic_tag_from() {
	local ln="$1" desc="$2" hw="$3" base lc_ln
	lc_ln=$(printf '%s' "$ln" | tr '[:upper:]' '[:lower:]')
	if [[ -n "$ln" ]] && [[ "$lc_ln" != sof* ]] && [[ "$lc_ln" != hda* ]]; then
		base="$ln"
	elif [[ -n "$desc" ]]; then
		base="$desc"
	else
		base="$hw"
	fi
	sanitize_id "$base"
}

mic_tag=$(mic_tag_from "$mic_name" "$mic_desc" "$mic_hw")
if (( id_auto )) && (( append_mic_tag )) && [[ -n "$mic_tag" ]]; then
	id="${id}_${mic_tag}"
fi

# --------------------------------------------------------------------------
# Avoid overwriting a previous measurement
# --------------------------------------------------------------------------
#
# The Octave side writes mls-<id>.{wav,txt,png} into $data_dir. If any of
# them already exists (typical when re-measuring the same box at a new mic
# location), suffix the id with -2, -3, ... until nothing would collide.
# Only applied when the id was auto-derived; a user-supplied --id is
# honored verbatim so scripted overwrites still work.
uniquify_id() {
	local base="$1" candidate="$1" n=2
	while [[ -e "$data_dir/mls-${candidate}.wav" \
	      || -e "$data_dir/mls-${candidate}.txt" \
	      || -e "$data_dir/mls-${candidate}.png" ]]; do
		candidate="${base}-${n}"
		n=$(( n + 1 ))
	done
	printf '%s' "$candidate"
}

if (( id_auto )); then
	new_id=$(uniquify_id "$id")
	if [[ "$new_id" != "$id" ]]; then
		echo "Note: prior measurement for '$id' found in $data_dir;" \
		     "using id '$new_id' to avoid overwrite" >&2
		id="$new_id"
	fi
fi

# Resolve the ALSA capture format. Honor --rec-fmt if the user forced
# one; otherwise probe the mic and prefer S32_LE over S16_LE.
if [[ -n "$rec_fmt_override" ]]; then
	rec_fmt="$rec_fmt_override"
else
	rec_fmt=$(probe_capture_format "$mic_hw")
fi
rec_bits=$(format_to_bits "$rec_fmt")

# Resolve the capture channel count similarly: if --mic-ch was given,
# use it verbatim; otherwise probe the max the mic advertises. Fall
# back to 1 when the probe returns nothing (device busy, etc.).
if [[ -z "$mic_channels" ]]; then
	probed_ch=$(probe_capture_channels "$mic_hw")
	if [[ -n "$probed_ch" ]]; then
		mic_channels="$probed_ch"
	else
		mic_channels=1
		echo "Note: could not probe channel count for $mic_hw, using 1" >&2
	fi
fi

# --------------------------------------------------------------------------
# Report + write configs
# --------------------------------------------------------------------------

echo "DMI vendor   : ${dmi_vendor_raw:-<unknown>}"
echo "DMI product  : ${dmi_product_raw:-<unknown>}"
echo "Mic tag      : ${mic_tag:-<none>}"
if (( id_auto )); then
	echo "DMI id       : $id  (sanitized, used in blob file names)"
else
	echo "DMI id       : $id  (user-supplied via --id, used verbatim)"
fi
echo "Speaker      : $speaker_hw  ($speaker_name, $speaker_info)"
echo "Microphone   : $mic_hw  ($mic_name, $mic_info)"
echo "Rec format   : $rec_fmt  (${rec_bits}-bit)"
echo "Mic cal file : ${rec_cal:-<none>}"
echo "Data dir     : $data_dir"
echo "Speaker chs  : $speaker_channels"
echo "Mic chs      : $mic_channels"

if [[ -n "$rec_cal" ]] && [[ ! -f "$rec_cal" ]]; then
	echo "Warning: calibration file '$rec_cal' does not exist;" \
	     "sof_mls_freq_resp.m will error out." >&2
fi

if is_webcam "$(echo "$mic_name" | tr '[:upper:]' '[:lower:]')"; then
	echo "Warning: selected mic looks like a webcam; measurements are unlikely" \
	     "to be usable. Consider passing --mic hw:C,D explicitly." >&2
fi
# Base the calibrated-mic note on the actual device name/description so that
# a user forcing --mic hw:C,D on a real UMM-6 does not get a misleading
# "no calibrated mic" warning. mic_info can be "override" in that case, so
# it is not a reliable signal on its own.
lc_mic_name=$(echo "$mic_name" | tr '[:upper:]' '[:lower:]')
lc_mic_desc=$(echo "$mic_desc" | tr '[:upper:]' '[:lower:]')
if ! is_measurement_mic "$lc_mic_name" && ! is_measurement_mic "$lc_mic_desc"; then
	echo "Note: no calibrated measurement microphone (UMM-6/UMIK/MiniDSP)" \
	     "detected; results will not be calibrated." >&2
fi

play_conf="$data_dir/mls_play_config.txt"
rec_conf="$data_dir/mls_rec_config.txt"

if (( dry_run )); then
	echo
	echo "-- Would write $play_conf --"
	echo "-- Would write $rec_conf --"
	if (( do_prep )); then
		echo "-- Would prep card $(extract_card "$speaker_hw"):"
		echo "     IIR blob = $iir_pass_blob"
		echo "     FIR blob = $fir_pass_blob"
		echo "     DRC/MBDRC/TDFB switches -> off"
	fi
	exit 0
fi

mkdir -p -- "$data_dir"

cat > "$play_conf" <<EOF
% Generated by sof_mls_freq_resp_local.sh for $id
% Speaker: $speaker_name ($speaker_info)
play.ssh  = 0;
play.user = '';
play.dir  = '/tmp';
play.dev  = '$speaker_hw';
play.nch  = $speaker_channels;
EOF

cat > "$rec_conf" <<EOF
% Generated by sof_mls_freq_resp_local.sh for $id
% Microphone: $mic_name ($mic_info)
rec.ssh      = 0;
rec.user     = '';
rec.dir      = '/tmp';
rec.dev      = '$mic_hw';
rec.nch      = $mic_channels;
rec.fmt_name = '$rec_fmt';
rec.bits     = $rec_bits;
% Path to a two-column [freq_hz, mag_db] calibration file, or '' for none.
rec.cal      = '${rec_cal//\'/\'\'}';
EOF

echo
echo "Wrote:"
echo "  $play_conf"
echo "  $rec_conf"

# --------------------------------------------------------------------------
# Optional pre-measurement DSP prep
# --------------------------------------------------------------------------
#
# For a valid frequency response we want the signal path from the
# playback PCM to the analog output to be as flat as possible: the IIR
# and FIR EQ bytes controls loaded with a pass-through blob, and any
# DRC / MBDRC / TDFB switch turned off so no dynamic processing or
# beam steering is applied while the MLS is playing. sof-ctl programs
# the IPC4 bytes blobs and amixer handles the boolean/enum switches.

prep_controls_passthrough() {
	local card="$1" iir_blob="$2" fir_blob="$3"
	local ctldev="hw:$card"

	if [[ ! -f "$iir_blob" ]]; then
		echo "IIR pass-through blob not found: $iir_blob" >&2
		return 1
	fi
	if [[ ! -f "$fir_blob" ]]; then
		echo "FIR pass-through blob not found: $fir_blob" >&2
		return 1
	fi

	need amixer
	need sof-ctl
	need timeout

	echo
	echo "Prep: card=$ctldev"
	echo "      IIR pass blob = $iir_blob"
	echo "      FIR pass blob = $fir_blob"

	# probe_bytes_size <card> <name>
	#
	# Return the on-device max byte-buffer size of the given BYTES
	# control by asking sof-ctl to read it (no -s) and parsing the
	# "Control size is N." line sof-ctl prints on stdout. amixer cannot
	# read large bytes controls (they use the TLV path), so sof-ctl is
	# the only usable probe.
	#
	# sof-ctl CLI: `-c name='...'` looks up the control by name; this
	# is essential because `-n numid=N` for the same control on some
	# SOF topologies returns "Control size is 1." (wrong max), for the
	# same reason amixer's numid= path is buggy on these controls.
	# `-i 4` is intentionally omitted here — we are only reading and
	# do not care about the ABI type for size reporting.
	#
	# Empty on any failure (sof-ctl missing, control not found, ioctl
	# error, timeout).
	probe_bytes_size() {
		local card="$1" name="$2" out
		out=$(timeout 5 sof-ctl -D "hw:$card" -c name="$name" 2>&1) \
		    || true
		printf '%s\n' "$out" \
		    | sed -nE 's/^Control size is ([0-9]+)\..*/\1/p' \
		    | head -1
	}

	# probe_type_desc <card> <name>
	#
	# Return a short human-readable diagnostic line from sof-ctl's
	# combined stderr+stdout, used only for the skip message. Prefers
	# the "Control size is N." line, then a "hdr: magic ..." line,
	# then any error-looking line, else the first line. Empty on total
	# probe failure.
	probe_type_desc() {
		local card="$1" name="$2" out line
		out=$(timeout 5 sof-ctl -D "hw:$card" -c name="$name" 2>&1) \
		    || true
		line=$(printf '%s\n' "$out" | grep -E '^Control size is ' | head -1)
		if [[ -z "$line" ]]; then
			line=$(printf '%s\n' "$out" | grep -E '^hdr: ' | head -1)
		fi
		if [[ -z "$line" ]]; then
			line=$(printf '%s\n' "$out" | grep -Ei 'error|failed|not ' | head -1)
		fi
		if [[ -z "$line" ]]; then
			line=$(printf '%s\n' "$out" | head -1)
		fi
		printf '%s' "$line"
	}

	# Blob writes below the minimum size are refused; the SOF pass-
	# through IIR blob is a few hundred bytes and FIR is >= 1 KiB, so
	# anything under 64 bytes is definitely not a real EQ buffer.
	local min_blob_bytes=64

	# Snapshot the matching controls into an array so the loop body
	# runs in the parent shell (not a pipeline subshell). This makes
	# any hang or failure inside the body visible instead of silently
	# terminating the subshell mid-loop. The grep filter is wrapped in
	# `{ ... || true; }` so that a card without any IIR/FIR/DRC/MBDRC/
	# TDFB controls does not trip `set -euo pipefail`.
	local ctl_lines line numid name size rc reason
	mapfile -t ctl_lines < <(amixer -c "$card" controls \
	    | { grep -E "IIR|FIR|DRC|MBDRC|TDFB" || true; })

	if (( ${#ctl_lines[@]} == 0 )); then
		echo "  (no IIR/FIR/DRC/MBDRC/TDFB controls found on card $card)"
		return 0
	fi
	echo "  ${#ctl_lines[@]} matching control(s) to process"

	for line in "${ctl_lines[@]}"; do
		numid=$(printf '%s\n' "$line" \
		        | sed -n 's/^numid=\([0-9]\+\),.*/\1/p')
		name=$(printf '%s\n' "$line" \
		       | sed -n "s/.*name='\\(.*\\)'[[:space:]]*$/\\1/p")
		[[ -z "$numid" || -z "$name" ]] && continue

		case "$name" in
			# Switches: DRC / MBDRC / TDFB -> off
			*[Dd][Rr][Cc]*switch \
			  | *[Tt][Dd][Ff][Bb]*switch \
			  | *[Mm][Bb][Dd][Rr][Cc]*switch)
				printf '  switch off  numid=%s  %s\n' \
				       "$numid" "$name"
				# amixer >= 1.2.13 has a known assertion
				# failure ("info->id.name[0] || info->id.numid")
				# in snd_ctl_elem_info() when cset is invoked
				# with numid= for some switch controls; the
				# process aborts with a core dump. Use the
				# iface=MIXER,name='...' form instead, which
				# takes the name-lookup path and sidesteps the
				# buggy branch. `timeout` guards against a
				# hung ioctl (e.g. DSP not responding).
				rc=0
				timeout 5 amixer -q -c "$card" cset \
				    iface=MIXER,name="$name" off || rc=$?
				if (( rc != 0 )); then
					echo "    ...amixer failed (exit=$rc)" >&2
				fi
				;;
			# IIR bytes / IIR Eq -> pass-through
			*[Ii][Ii][Rr]*[Bb]ytes \
			  | *[Ii][Ii][Rr]" Eq")
				# `|| size=""` is REQUIRED here: under `set -e`,
				# an assignment `x=$(cmd)` where the command
				# substitution's pipeline fails (e.g. sof-ctl
				# returns non-zero, and pipefail propagates it)
				# will silently kill the entire script. Neutralise
				# it so probe_bytes_size is treated as best-effort.
				size=$(probe_bytes_size "$card" "$name") || size=""
				if [[ -z "$size" ]]; then
					reason=$(probe_type_desc "$card" "$name")
					printf '  iir skip    numid=%s  %s  (%s)\n' \
					       "$numid" "$name" "${reason:-sof-ctl probe failed}"
					continue
				fi
				if (( size < min_blob_bytes )); then
					printf '  iir skip    numid=%s  %s  (bytes size=%s, not a real EQ buffer)\n' \
					       "$numid" "$name" "$size"
					continue
				fi
				printf '  iir pass    numid=%s  %s  (%s bytes)\n' \
				       "$numid" "$name" "$size"
				rc=0
				# Set via -c name= for the same reason the probe
				# does: -n numid= can report wrong max sizes and
				# is prone to the same buggy code path amixer
				# hits with numid= access.
				timeout 5 sof-ctl -i 4 -D "$ctldev" -c name="$name" \
				    -s "$iir_blob" >/dev/null || rc=$?
				if (( rc != 0 )); then
					echo "    ...sof-ctl failed (exit=$rc)" >&2
				fi
				;;
			# FIR bytes / FIR Eq -> pass-through
			*[Ff][Ii][Rr]*[Bb]ytes \
			  | *[Ff][Ii][Rr]" Eq")
				size=$(probe_bytes_size "$card" "$name") || size=""
				if [[ -z "$size" ]]; then
					reason=$(probe_type_desc "$card" "$name")
					printf '  fir skip    numid=%s  %s  (%s)\n' \
					       "$numid" "$name" "${reason:-sof-ctl probe failed}"
					continue
				fi
				if (( size < min_blob_bytes )); then
					printf '  fir skip    numid=%s  %s  (bytes size=%s, not a real EQ buffer)\n' \
					       "$numid" "$name" "$size"
					continue
				fi
				printf '  fir pass    numid=%s  %s  (%s bytes)\n' \
				       "$numid" "$name" "$size"
				rc=0
				timeout 5 sof-ctl -i 4 -D "$ctldev" -c name="$name" \
				    -s "$fir_blob" >/dev/null || rc=$?
				if (( rc != 0 )); then
					echo "    ...sof-ctl failed (exit=$rc)" >&2
				fi
				;;
			# Bytes controls we don't have a canonical pass-through
			# for are just reported so the user sees what was left
			# alone.
			*[Bb]ytes|*enum|*[Ee]num)
				printf '  skipped     numid=%s  %s\n' \
				       "$numid" "$name"
				;;
		esac
	done
}

if (( do_prep )); then
	spk_card=$(extract_card "$speaker_hw")
	if [[ -z "$spk_card" ]]; then
		echo "Cannot extract card number from speaker '$speaker_hw'" >&2
		exit 6
	fi
	prep_controls_passthrough "$spk_card" \
	                          "$iir_pass_blob" "$fir_pass_blob"
fi

# --------------------------------------------------------------------------
# Optionally invoke Octave
# --------------------------------------------------------------------------

# Escape single quotes so they survive being embedded in an Octave
# single-quoted string literal (the doubling rule Octave uses for '').
# In practice id is sanitized to [a-z0-9_] and the paths come from our
# own $TMPDIR / script location, but a --dir or --id with a stray ' in
# it would otherwise break the string or inject additional Octave code.
oct_squote() {
	printf "%s" "${1//\'/\'\'}"
}

q_script_dir=$(oct_squote "$script_dir")
q_id=$(oct_squote "$id")
q_play_conf=$(oct_squote "$play_conf")
q_rec_conf=$(oct_squote "$rec_conf")
q_data_dir=$(oct_squote "$data_dir")

octave_eval="cd('$q_script_dir'); sof_mls_freq_resp('$q_id', '$q_play_conf', '$q_rec_conf', '$q_data_dir');"

echo
echo "To run the measurement manually:"
echo "  octave --no-gui --no-window-system --eval \"$octave_eval\""

if (( do_run )); then
	echo
	echo "Running measurement..."
	octave --no-gui --no-window-system --eval "$octave_eval"
fi
