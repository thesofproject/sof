#!/bin/bash
# usage: ./scripts/sof-target-intsall.sh up2-hostname ip

for host in $@
do
	scp build_*_*/sof-*.ri root@${host}:/lib/firmware/intel/sof/
	# TODO: need modify when tools CMAKE build system updated
	scp tools/topology/*.tplg root@${host}:/lib/firmware/intel/sof-tplg/
	scp tools/logger/sof-logger \
		build_*_*/src/arch/xtensa/sof-*.ldc \
		tools/coredumper/* \
		tools/eqctl/sof-eqctl \
		tools/kmod_scripts/* \
		root@${host}:~/
done
