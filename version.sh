
# version for configure, make dist and FW etc
# usage "version.sh dir"
# Where dir is the top level directory path.

# use pwd is no path argument is given
if [ $# -eq 0 ]; then
	DIR=`pwd`
else
	DIR=$1
fi

# get version from git tag
GIT_TAG=`git describe --abbrev=4`

# Some releases have a SOF_FW_XXX_ prefix on the tag and this prefix
# must be stripped for usage in version.h. i.e. we just need the number.
if [ $(expr match $GIT_TAG 'SOF_FW_[A-Z][A-Z][A-Z]_' ) -eq 11 ]; then
	VER=`echo $GIT_TAG | cut -d_ -f4`
else
	VER=$GIT_TAG
fi

# create git version if we are a git repo or git worktree
if [ -e $DIR/.git -o -d $DIR/.git ]; then
#	 version for make dist
	echo $VER > $DIR/.version
	echo $VER > $DIR/.tarball-version

	# git commit for IPC
	echo "#define SOF_TAG \"`git log --pretty=format:\"%h\" -1 | cut -c1-5`\"" > $DIR/src/include/version.h
else
	echo "#define SOF_TAG \"0\"" > $DIR/src/include/version.h
fi

# build counter
if [ -e $DIR/.build ]; then
	num=$((`cat $DIR/.build` + 1))
else
	num=0
fi

# save and insert build counter
echo $num > $DIR/.build
echo "#define SOF_BUILD $num" >> $DIR/src/include/version.h

#echo version for AC_INIT
if [ -e $DIR/.version ]; then
	echo -n `cat $DIR/.version | cut -dv -f2 | cut -d. -f1`.`cat $DIR/.version | cut -d. -f2 | cut -d- -f1`.`cat $DIR/.version | cut -d. -f3 | cut -d- -f1`
fi
