#!/bin/bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 Intel Corporation. All rights reserved.

set -e

main () {
    if [ "$#" -ne 1 ]; then
	echo "Usage: $0 <component>"
	exit 1
    fi
    comp=$1
    FULL_CMD=( "$0" "$@" )
    generate_comp "s16"
    generate_comp "s24"
    generate_comp "s32"
    generate_route
    generate_playback_controls
    generate_capture_controls
}

generate_comp ()
{
    format=$1
    fn=${comp}_${format}.conf
    echo Creating file "$fn"
    cat > "$fn" <<EOF_COMP
		# Created with script "${FULL_CMD[@]}"
		Object.Widget.${comp}.1 {
			index 1
			<include/bench/one_input_output_format_${format}.conf>
			<include/bench/${comp}_control_bytes_playback.conf>
		}
		Object.Widget.${comp}.2 {
			index 3
			<include/bench/one_input_output_format_${format}.conf>
			<include/bench/${comp}_control_bytes_capture.conf>
		}
		<include/bench/host_io_gateway_pipelines_${format}.conf>
		<include/bench/${comp}_hda_route.conf>
EOF_COMP
}

generate_route ()
{
    fn=${comp}_hda_route.conf
    echo Creating file "$fn"
    cat > "$fn" <<EOF_ROUTE
		# Created with script "${FULL_CMD[@]}"
		Object.Base.route [
			{
				sink 'dai-copier.HDA.\$HDA_ANALOG_DAI_NAME.playback'
				source '${comp}.1.1'
			}
			{
				sink '${comp}.1.1'
				source 'host-copier.0.playback'
			}
			{
				source 'dai-copier.HDA.\$HDA_ANALOG_DAI_NAME.capture'
				sink '${comp}.3.2'
			}
			{
				source '${comp}.3.2'
				sink 'host-copier.0.capture'
			}
		]
EOF_ROUTE
}

generate_playback_controls ()
{
    fn=${comp}_control_bytes_playback.conf
    echo Creating file "$fn"
    cat > "$fn" <<EOF_PLAYBACK_BYTES
			# Created initially with script "${FULL_CMD[@]}"
			# may need edits to modify controls
			Object.Control.bytes."1" {
				name '\$ANALOG_PLAYBACK_PCM ${comp^^} bytes'
				IncludeByKey.BENCH_${comp^^}_PARAMS {
					"default" "include/components/${comp}/default.conf"
				}
			}
EOF_PLAYBACK_BYTES
}

generate_capture_controls ()
{
    fn=${comp}_control_bytes_capture.conf
    echo Creating file "$fn"
    cat > "$fn" <<EOF_CAPTURE_BYTES
			# Created initially with script "${FULL_CMD[@]}"
			# may need edits to modify controls
			Object.Control.bytes."1" {
				name '\$ANALOG_CAPTURE_PCM ${comp^^} bytes'
				IncludeByKey.BENCH_${comp^^}_PARAMS {
					"default" "include/components/${comp}/default.conf"
				}
			}
EOF_CAPTURE_BYTES
}

main "$@"
