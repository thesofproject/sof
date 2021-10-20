# _Binaries and memmaps for tigerlake_

Made at cl/306749607.

## Requirement:

Tensilica version: RG-2017.8-linux with Intel Tiger Lake core.

## Updating the release static library and the memmap files:

Run ./googledata/speech/micro/release/tigerlake/create_release.sh to update.
Make sure that you've opened all the files in
googledata/speech/micro/release/tigerlake for edit if you're in a CITC client.

## Building an example which uses the static library:

${XTENSA_BASE}/tools/RG-2017.8-linux/XtensaTools/bin/xt-xcc
--xtensa-core=cavs2x_LX6HiFi3_2017_8 -static -o hotword_dsp_api_main
speech/micro/api/hotword_dsp_api_main.c -I.
googledata/speech/micro/release/tigerlake/libhifi3z_google_hotword_dsp_api.a -lm

## Running the example:

${XTENSA_BASE}/tools/RG-2017.8-linux/XtensaTools/bin/xt-run
--xtensa-core=cavs2x_LX6HiFi3_2017_8 ./hotword_dsp_api_main
googledata/speech/micro/release/tigerlake/ok_google/en_all.mmap
speech/micro/api/testdata/okgoogle-16k.wav
