#!/bin/bash

./autogen.sh

pwd=`pwd`

# parse the args
for args in $@
do
	if [[ "$args" == "-l" ]]
		then
		BUILD_LOCAL=1
	fi
done

# make sure rimage is built and aligned with code
if [[ "x$BUILD_LOCAL" == "x" ]]
then
	./configure --enable-tools
	make
	sudo make install
else
	echo "BUILD in local folder!"
	rm -rf $pwd/local/
	./configure --enable-tools --prefix=$pwd/local
	make
	make install
fi
