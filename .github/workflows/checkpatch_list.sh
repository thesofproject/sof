#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause

set -e

# The purpose of this script is not to stop on the first checkpatch
# failure and report at the end instead.
#
# Sample invocation:
# $0 --codespell --strict < PR_SHAs.txt
main()
{
    local failures=0
    while read -r sha ; do
        printf '\n    -------------- \n\n'
        ./scripts/checkpatch.pl $@ -g $sha || failures=$((failures+1))
    done
    printf '\n    -------------- \n\n'
    return $failures
}

main "$@"
