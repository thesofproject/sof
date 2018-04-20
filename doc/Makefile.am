EXTRA_DIST=sof_uapi.doxygen.in

AM_CPPFLAGS=-I$(top_srcdir)/include

doc:
	test -e sof_uapi.doxygen || sed s:[@]top_srcdir[@]:..:g sof_uapi.doxygen.in > sof_uapi.doxygen
	doxygen sof_uapi.doxygen

doc-clean:
	rm -rf $(top_srcdir)/doc/sof_uapi/*
