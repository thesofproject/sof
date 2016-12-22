
# version for configure
echo -n `git describe --abbrev=4`

# version for make dist
git describe --abbrev=4 > .version
git describe --abbrev=4 > .tarball-version

# git commit for IPC
echo "#define REEF_BUILD \"`git describe --abbrev=4 | cut -d- -f3`\"" > src/include/version.h
