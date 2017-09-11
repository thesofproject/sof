
#
# Dependencies
#

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
	reef-apl-nocodec.tplg \
	reef-bxt-nocodec.tplg \
	reef-byt-nocodec.tplg \
	reef-bdw-rt286.tplg \
	reef-bdw-rt5640.tplg \
	reef-byt-rt5640.tplg \
	reef-byt-rt5651.tplg \
	reef-hsw-rt5640.tplg \
	test-passthrough-24bit-48k-ssp2.tplg \
	test-passthrough-24bit-48k-vol-ssp2.tplg \
	test-passthrough-24bit-48k-src-ssp2.tplg

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

