EXTRA_DIST=sof.doxygen.in

AM_CPPFLAGS=-I$(top_srcdir)/include

doc:
	test -e sof.doxygen || sed s:[@]top_srcdir[@]:..:g sof.doxygen.in > sof.doxygen
	doxygen sof.doxygen

doc-clean:
	rm -rf $(top_srcdir)/doc/doxygen/*
