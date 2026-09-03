# Loadable Modules Dev Kit

***TODO: add link to the documentation repo!***

To build dummy loadable library execute:

    cd libraries/dummy
    cmake -B build -G <Ninja/Makefile> -DRIMAGE_INSTALL_DIR="path/where/rimage/executable/is" -DSIGNING_KEY="path/to/key"
    cd --build build


Here RIMAGE_INSTALL_DIR is a path to directory where rimage executable is, SIGNING_KEY is a path to
signing key for rimage. If RIMAGE_INSTALL_DIR is not provided, rimage will be searched for in the directory
where SOF project installs it. Dummy module sets up toolchain file in the project file.
However, in your library you can select toolchain file in the configure step command:

    cmake -B build -G <Ninja/Makefile> --toolchain "../../cmake/xtensa-toolchain.cmake" -DSIGNING_KEY="path/to/key"
