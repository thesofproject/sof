#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation. All rights reserved.

# SOF documentation building & publishing.
# According to instructions:
# https://thesofproject.github.io/latest/howtos/process/docbuild.html

# this needs to run from the Zephyr python virtual environment
# so that the python packages are available (and more up to date than
# the system packages).

# check if Zephyr environment is set up
if [ ! -z "$ZEPHYR_BASE" ]; then
    VENV_DIR="$ZEPHYR_BASE/.venv"
    echo "Using Zephyr environment at $ZEPHYR_BASE"
elif [ ! -z "$SOF_WORKSPACE" ]; then
    VENV_DIR="$SOF_WORKSPACE/zephyr/.venv"
    echo "Using SOF/Zephyr environment at $SOF_WORKSPACE"
else
    # fallback to the zephyr default from the getting started guide
    VENV_DIR="$HOME/zephyrproject/.venv"
    echo "Using default Zephyr environment at $VENV_DIR"
fi

# start the virtual environment
source ${VENV_DIR}/bin/activate

function print_usage()
{
	echo "usage: $0 <-b|-c|-p> [-r]"
	echo "       [-b] Build"
	echo "       [-c] Clean"
	echo "       [-p] Publish"
	echo "       [-r] Fetch missing repositories"
}

# make it runnable from any location
# user shouldn't be confused from which dir this script has to be run
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

# expected paths of repositories needed for documentation process
SOF_REPO=$(dirname "$SCRIPT_DIR")
DOCS_REPO="$(dirname "$SOF_REPO")/sof-docs"
PUBLISH_REPO="$(dirname "$SOF_REPO")/thesofproject.github.io"

# parse arguments
DO_BUILD=false
DO_CLEAN=false
DO_PUBLISH=false
DO_FETCH=false

while getopts "bcpr" OPTION; do
        case "$OPTION" in
        b) DO_BUILD=true ;;
        c) DO_CLEAN=true ;;
        p) DO_PUBLISH=true ;;
        r) DO_FETCH=true ;;
        *) print_usage; exit 1 ;;
        esac
done
shift "$(($OPTIND - 1))"

if ! { "$DO_BUILD" || "$DO_CLEAN" || "$DO_PUBLISH"; }
then
        print_usage
        exit 1
fi

if [ ! -d "$DOCS_REPO" ]
then
        if "$DO_FETCH"
	then
		echo "No sof-docs repo at: $DOCS_REPO"
		echo "Cloning sof-docs repo"

		read -p 'Enter your GitHub user name: ' GITHUB_USER
		git clone \
			git@github.com:$GITHUB_USER/sof-docs.git \
			"$DOCS_REPO"

		# set up fork for upstreaming, just like in instructions
		cd "$DOCS_REPO"
		git remote add upstream git@github.com:thesofproject/sof-docs.git
	else
		echo "Error: No sof-docs repo at: $DOCS_REPO"
		echo "You can rerun the script with -r argument to fetch missing repositories:"
		echo "Example: $0 $@ -r"
		exit 1
	fi
fi

cd "$SOF_REPO/doc"

if "$DO_CLEAN"
then
	echo "Cleaning $SOF_REPO"
	cmake .
	make clean
	make doc-clean
fi

if "$DO_BUILD"
then
	echo "Building $SOF_REPO"
	cmake .
	make doc
fi

cd "$DOCS_REPO"

if "$DO_CLEAN"
then
	echo "Cleaning $DOCS_REPO"
	make clean
fi

if "$DO_BUILD"
then
	echo "Building $DOCS_REPO"
	make html

	echo "Documentation build finished"
	echo "It can be viewed at: $DOCS_REPO/_build/html/index.html"
fi

if "$DO_PUBLISH"
then
	if [ ! -d "$PUBLISH_REPO" ]
	then

		if "$DO_FETCH"
		then
			echo "No publishing repo at: $PUBLISH_REPO"
			echo "Cloning thesofproject.github.io.git repo"

			git clone \
				git@github.com:thesofproject/thesofproject.github.io.git \
				"$PUBLISH_REPO"
		else
			echo "Error: No publishing repo at: $PUBLISH_REPO"
			echo "You can rerun the script with -r argument to fetch missing repositories:"
			echo "Example: $0 $@ -r"
			exit 1
		fi
	fi

	echo "Publishing $PUBLISH_REPO"

	cd "$DOCS_REPO"
	make publish
fi

# cleanup
deactivate
