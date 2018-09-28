
#
# Dependencies
#

SUBDIRS = m4 sof common platform

DEPS = \
	platform/intel/*.m4 \
	platform/common/*.m4 \
	common/*.m4 \
	m4/*.m4 \
	sof/*.m4

#
# Machines
#

MACHINES = \
	sof-cht-nocodec.tplg \
	sof-cht-max98090.tplg \
	sof-apl-hdmi.tplg \
	sof-hda-generic.tplg \
	sof-apl-nocodec.tplg \
	sof-byt-nocodec.tplg \
	sof-bdw-rt286.tplg \
	sof-bdw-rt5640.tplg \
	sof-byt-rt5640.tplg \
	sof-byt-rt5645.tplg \
	sof-byt-rt5651.tplg \
	sof-byt-da7213.tplg \
	sof-cnl-rt274.tplg \
	sof-hsw-rt5640.tplg \
	sof-apl-tdf8532.tplg \
	sof-apl-pcm512x.tplg \
	sof-apl-wm8804.tplg \
	sof-apl-da7219.tplg \
	sof-glk-da7219.tplg \
	sof-icl-nocodec.tplg \
	sof-apl-eq-pcm512x.tplg

# Uncomment the following line if you want to debug conf files
.PRECIOUS: %.conf

%.conf : %.m4 ${DEPS}
	m4 -I m4 -I common -I platform/common $< > $@

%.tplg : %.conf
	alsatplg -v 1 -c $< -o $@

all: ${MACHINES}

clean-local:
	rm -f *.conf
	rm -f *.tplg

EXTRA_DIST = \
	sof-cht-nocodec.m4 \
	sof-cht-max98090.m4 \
	sof-apl-hdmi.m4 \
	sof-hda-generic.m4 \
	sof-apl-nocodec.m4 \
	sof-byt-nocodec.m4 \
	sof-bdw-rt286.m4 \
	sof-bdw-rt5640.m4 \
	sof-byt-rt5640.m4 \
	sof-byt-rt5645.m4 \
	sof-byt-rt5651.m4 \
	sof-byt-da7213.m4 \
	sof-cnl-rt274.m4 \
	sof-hsw-rt5640.m4 \
	sof-apl-tdf8532.m4 \
	sof-apl-pcm512x.m4 \
	sof-apl-wm8804.m4 \
	sof-apl-da7219.m4 \
	sof-glk-da7219.m4 \
	sof-icl-nocodec.m4 \
	sof-apl-eq-pcm512x.m4
