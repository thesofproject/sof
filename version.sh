
# version for configure, make dist and FW etc
# usage "version.sh dir"
# Where dir is the top level directory path.

# use pwd is no path argument is given
if [ $# -eq 0 ]; then
	DIR=`pwd`
else
	DIR=$1
fi

# create git version if we are a git repo
if [ ! -d $DIR/.git ]; then
#	 version for make dist
	git describe --abbrev=4 > $DIR/.version
	git describe --abbrev=4 > $DIR/.tarball-version

	# git commit for IPC
	echo "#define REEF_TAG \"`git log --pretty=format:\"%h\" -1 | cut -c1-5`\"" > $DIR/src/include/version.h
else
	echo "#define REEF_TAG 0" > $DIR/src/include/version.h
fi

# build counter
if [ -e $DIR/.build ]; then
	num=$((`cat $DIR/.build` + 1))
else
	num=0
fi

# save and insert build counter
echo $num > $DIR/.build
echo "#define REEF_BUILD $num" >> $DIR/src/include/version.h

#echo version for AC_INIT
if [ -e $DIR/.version ]; then
	echo -n `cat $DIR/.version | cut -dv -f2 | cut -d. -f1`.`cat $DIR/.version | cut -d. -f2 | cut -d- -f1`
fi
