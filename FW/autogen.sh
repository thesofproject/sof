#!/bin/bash
rm -f .build
if [ ! -f ../.git/hooks/pre-commit ]; then
    ln -s -f ../../scripts/sof-pre-commit-hook.sh ../.git/hooks/pre-commit
fi
if [ ! -f ../.git/hooks/post-commit ]; then
    ln -s -f ../../scripts/sof-post-commit-hook.sh ../.git/hooks/post-commit
fi
libtoolize -c --force
aclocal -I m4 --install
autoconf -Wall
autoheader
automake -a --copy --foreign --add-missing
