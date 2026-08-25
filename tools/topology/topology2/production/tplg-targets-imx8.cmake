# SPDX-License-Identifier: BSD-3-Clause

# Array of "input-file-name;output-file-name;comma separated pre-processor variables"
list(APPEND TPLGS
# IPC4 topology for i.MX8MP + WM8960 Codec - playback only
"imx8-wm8960\;sof-imx8mp-wm8960\;SAI_DAI_INDEX=3,STREAM_CODEC_NAME=sai3-wm8960-hifi"
# IPC4 topology for i.MX8MP + WM8960 Codec - duplex (playback + capture)
"imx8-wm8960-duplex\;sof-imx8mp-wm8960-duplex\;SAI_DAI_INDEX=3,STREAM_CODEC_NAME=sai3-wm8960-hifi"
# IPC4 topology for i.MX95 + WM8962 Codec - playback only
"imx95-wm8962\;sof-imx95-wm8962\;SAI_DAI_INDEX=3,STREAM_CODEC_NAME=sai3-wm8962"
# IPC4 topology for i.MX95 + WM8962 Codec - duplex (playback + capture)
"imx95-wm8962-duplex\;sof-imx95-wm8962-duplex\;SAI_DAI_INDEX=3,STREAM_CODEC_NAME=sai3-wm8962"
# IPC4 topology for i.MX8QM/i.MX8QXP + WM8960 Codec on SAI1 - playback only
"imx8-wm8960\;sof-imx8-wm8960\;SAI_DAI_INDEX=1,STREAM_CODEC_NAME=sai1-wm8960-hifi"
# IPC4 topology for i.MX8QM/i.MX8QXP + WM8960 Codec on SAI1 - duplex
"imx8-wm8960-duplex\;sof-imx8-wm8960-duplex\;SAI_DAI_INDEX=1,STREAM_CODEC_NAME=sai1-wm8960-hifi"
)
