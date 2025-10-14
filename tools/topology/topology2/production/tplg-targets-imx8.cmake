# SPDX-License-Identifier: BSD-3-Clause

# Array of "input-file-name;output-file-name;comma separated pre-processor variables"
list(APPEND TPLGS
# IPC4 topology for i.MX8MP + WM8960 Codec
"imx8-wm8960\;sof-imx8mp-wm8960\;SAI_DAI_INDEX=3,STREAM_CODEC_NAME=sai3-wm8960-hifi"
)
