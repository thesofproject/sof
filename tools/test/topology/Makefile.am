
#
# Dependencies
#

if XARGS
export USE_XARGS=yes
endif

DEPS = \
	../../topology/platform/intel/*.m4 \
	../../topology/common/*.m4 \
	../../topology/m4/*.m4 \
	../../topology/sof/*.m4

# Uncomment the following line if you want to debug conf files
.PRECIOUS: %.conf

all : *.m4 ${DEPS}
	./tplg-build.sh

clean-local:
	rm -f *.conf
	rm -f *.tplg


EXTRA_DIST = \
	test-capture.m4 \
	test-playback.m4 \
	test-tone-playback.m4 \
	test-all.m4 \
	tplg-build.sh
