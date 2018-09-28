#!/bin/bash

# fail on any errors
set -e

# clear host log
rm -rf host.log

# parse the args
for args in $@
do
	if [[ "$args" == "-l" ]]
		then
		BUILD_LOCAL=1
	fi
done

# run autogen.sh
./autogen.sh 1>>host.log

pwd=`pwd`

# Build library for host platform architecture
if [[ "x$BUILD_LOCAL" == "x" ]]
then
	./configure --with-arch=host --enable-library=yes --host=x86_64-unknown-linux-gnu 1>>host.log
else
	# make sure host lib is build in local folder
	echo "BUILD in local folder!"
	rm -rf $pwd/local/
	./configure --with-arch=host --enable-library=yes --host=x86_64-unknown-linux-gnu --prefix=$pwd/host-lib 1>>host.log
fi

make 1>>host.log
make install 1>>host.log
