
#
# Dependencies
#



DEPS = \
	../dsps/*.m4 \
	../common/*.m4 \
	../m4/*.m4 \
	../sof/*.m4

# Uncomment the following line if you want to debug conf files
.PRECIOUS: %.conf

all : *.m4 ${DEPS}
	./tplg-build.sh

clean:
	rm -f *.conf
	rm -f *.tplg


EXTRA_DIST = \
	test-capture-ssp.m4 \
	test-playback-ssp.m4 \
	test-ssp.m4 \
	tplg-build.sh
