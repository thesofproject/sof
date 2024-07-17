#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause


import pathlib
import dataclasses
from tools.utils import rmtree_if_exists, execute_command
import argparse
import platform as py_platform
import sys
import os
import warnings

SOF_TOP = pathlib.Path(__file__).parents[3].resolve()
MIN_PYTHON_VERSION = 3, 8
assert sys.version_info >= MIN_PYTHON_VERSION, \
    f"Python {MIN_PYTHON_VERSION} or above is required."

"""Constant value resolves SOF_TOP directory as: "this script directory/.." """
SOF_TOP = pathlib.Path(__file__).parents[1].resolve()
LMDK_BUILD_DIR = SOF_TOP / "../.." / "sof" / "lmdk"
RIMAGE_BUILD_DIR = SOF_TOP / "../.." /"build-rimage"

if py_platform.system() == "Windows":
    xtensa_tools_version_postfix = "-win32"
elif py_platform.system() == "Linux":
    xtensa_tools_version_postfix = "-linux"
else:
    xtensa_tools_version_postfix = "-unsupportedOS"
    warnings.warn(f"Your operating system: {py_platform.system()} is not supported")


def build_libraries(LMDK_BUILD_DIR, RIMAGE_BUILD_DIR, args):
    library_dir = LMDK_BUILD_DIR / "libraries"
    """Cmake build"""
    for lib in args.libraries:
        library_cmake = library_dir / lib / "CMakeLists.txt"
        if library_cmake.is_file():
            print(f"\nBuilding loadable module: " + lib)
            lib_path = pathlib.Path(library_dir, lib, "build")
            print(f"\nRemoving existing artifacts")
            rmtree_if_exists(lib_path)
            lib_path.mkdir(parents=True, exist_ok=True)
            rimage_bin = RIMAGE_BUILD_DIR / "rimage.exe"
            if not rimage_bin.is_file():
                rimage_bin = RIMAGE_BUILD_DIR / "rimage"
            if args.key:
                key = "-DSIGNING_KEY=" + args.key.__str__()
            else:
                key = "-DSIGNING_KEY=" + PlatformConfig.RIMAGE_KEY.__str__()
            execute_command(["cmake", "-B", "build", "-G", "Ninja",
                             "-DRIMAGE_COMMAND="+str(rimage_bin), key],
                             cwd=library_dir/lib)
            execute_command(["cmake", "--build", "build", "-v"], cwd=library_dir/lib)


def parse_args():
    parser = argparse.ArgumentParser(description='Building loadable modules using python scripts')
    parser.add_argument("-k", "--key", type=pathlib.Path, required=False,
                      help="Path to a non-default rimage signing key.")

    parser.add_argument("-l", "--libraries", nargs="*", default=[],
                      help=""" Function for CMake building modules. We can build more then one module
                      just by adding more module names.""")
    return parser.parse_args()


def main():
    args = parse_args()

    if args.libraries:
        build_libraries(LMDK_BUILD_DIR, RIMAGE_BUILD_DIR, args)


if __name__ == "__main__":
    main()
