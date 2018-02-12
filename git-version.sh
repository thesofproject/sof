
# version for configure
# echo -n `git describe --abbrev=4`

# version for make dist
git describe --abbrev=4 > .version
git describe --abbrev=4 > .tarball-version

# git commit for IPC
echo "#define REEF_TAG \"`git log --pretty=format:\"%h\" -1 | cut -c1-5`\"" > src/include/version.h

# build counter
if [ -e .build ]; then
	num=$((`cat .build` + 1))
else
	num=0
fi

# save and insert build counter
echo $num > .build
echo "#define REEF_BUILD $num" >> src/include/version.h

#echo version for AC_INIT
echo -n `cat .version | cut -dv -f2 | cut -d. -f1`:`cat .version | cut -d. -f2 | cut -d- -f1`
