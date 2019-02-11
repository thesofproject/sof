#!/bin/bash
# usage: ./scripts/sof-target-install.sh up2-hostname ip

for host in $@
do
	scp build_*_*/sof-*.ri root@${host}:/lib/firmware/intel/sof/
	scp tools/build_tools/topology/*.tplg root@${host}:/lib/firmware/intel/sof-tplg/
	scp tools/build_tools/logger/sof-logger \
		build_*_*/src/arch/xtensa/sof-*.ldc \
		tools/coredumper/* \
		tools/build_tools/eqctl/sof-eqctl \
		tools/kmod_scripts/* \
		root@${host}:~/
done
