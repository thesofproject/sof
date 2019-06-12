#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation. All rights reserved.

# SOF documentation building & publishing.
# According to instructions:
# https://thesofproject.github.io/latest/howtos/process/docbuild.html

# fail immediately on any errors
set -e

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
DO_BUILD=no
DO_CLEAN=no
DO_PUBLISH=no
DO_CLONE=no

for args in $@
do
	if [[ "$args" == "-b" ]]
	then
		DO_BUILD=yes
	elif [[ "$args" == "-c" ]]
	then
		DO_CLEAN=yes
	elif [[ "$args" == "-p" ]]
	then
		DO_PUBLISH=yes
	elif [[ "$args" == "-r" ]]
	then
		DO_CLONE=yes
	fi
done

if [ "x$DO_BUILD" == "xno" ] && \
	[ "x$DO_CLEAN" == "xno" ] && \
	[ "x$DO_PUBLISH" == "xno" ]
then
	print_usage
	exit 1
fi

if [ ! -d "$DOCS_REPO" ]
then
	if [ "x$DO_CLONE" == "xyes" ]
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

if [ "x$DO_CLEAN" == "xyes" ]
then
	echo "Cleaning $SOF_REPO"
	cmake .
	make clean
	make doc-clean
fi

if [ "x$DO_BUILD" == "xyes" ]
then
	echo "Building $SOF_REPO"
	cmake .
	make doc
fi

cd "$DOCS_REPO"

if [ "x$DO_CLEAN" == "xyes" ]
then
	echo "Cleaning $DOCS_REPO"
	make clean
fi

if [ "x$DO_BUILD" == "xyes" ]
then
	echo "Building $DOCS_REPO"
	make html

	echo "Documentation build finished"
	echo "It can be viewed at: $DOCS_REPO/_build/html/index.html"
fi

if [ "x$DO_PUBLISH" == "xyes" ]
then
	if [ ! -d "$PUBLISH_REPO" ]
	then

		if [ "x$DO_CLONE" == "xyes" ]
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
