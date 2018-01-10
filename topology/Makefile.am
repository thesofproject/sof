
#
# Dependencies
#

SUBDIRS = test m4 sof common dsps

DEPS = \
	dsps/*.m4 \
	common/*.m4 \
	m4/*.m4 \
	sof/*.m4

#
# Machines
#

MACHINES = \
	reef-cht-nocodec.tplg \
	reef-cht-max98090.tplg \
	reef-apl-nocodec.tplg \
	reef-bxt-nocodec.tplg \
	reef-byt-nocodec.tplg \
	reef-bdw-rt286.tplg \
	reef-bdw-rt5640.tplg \
	reef-byt-rt5640.tplg \
	reef-byt-rt5645.tplg \
	reef-byt-rt5651.tplg \
	reef-byt-da7212.tplg \
	reef-hsw-rt5640.tplg

# Uncomment the following line if you want to debug conf files
.PRECIOUS: %.conf

%.conf : %.m4 ${DEPS}
	m4 -I m4 $< > $@

%.tplg : %.conf
	alsatplg -v 1 -c $< -o $@

all: ${MACHINES}

clean:
	rm -f *.conf
	rm -f *.tplg

EXTRA_DIST = \
	reef-cht-nocodec.m4 \
	reef-cht-max98090.m4 \
	reef-apl-nocodec.m4 \
	reef-bxt-nocodec.m4 \
	reef-byt-nocodec.m4 \
	reef-bdw-rt286.m4 \
	reef-bdw-rt5640.m4 \
	reef-byt-rt5640.m4 \
	reef-byt-rt5645.m4 \
	reef-byt-rt5651.m4 \
	reef-byt-da7212.m4 \
	reef-hsw-rt5640.m4
