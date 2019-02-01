#!/bin/bash
# usage: ./scripts/sof-target-intsall.sh up2-hostname ip

for host in $@
do
	scp build_*_*/sof-*.ri tools/topology/*.tplg root@${host}:/lib/firmware/intel/
	scp tools/logger/sof-logger \
		build_*_*/src/arch/xtensa/sof-*.ldc \
		tools/coredumper/* \
		tools/eqctl/sof-eqctl \
		tools/kmod_scripts/* \
		root@${host}:~/
done
