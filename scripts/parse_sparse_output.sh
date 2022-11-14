#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause

# "sparse" is not designed for automated testing: its exit code is
# mostly useless. So this script extracts from its output the most
# important messages instead and returns 1 if any is found.

# As of Sep. 2022, sparse's exit status is by default always zero even
# when there is an error, see check_symbols() in sparse.c. There is a
# -DEXTRA_CFLAGS=-Wsparse-error option that returns non-zero... for any
# warning even the most harmless one and it seems to stops on the first
# one. SOF has hundreds of sparse warnings now.

main()
{
    # To reproduce an 'error: ' and test this script try commenting out
    # `defined(__CHECKER__)` in common.h.
    #
    # To reproduce the 'different address space' warning and test this
    # script try deleting a __sparse_cache annotation like the one in
    # src/audio/mixer/mixer.c

    ret=0
    while read -r line; do
        echo "$line" | grep -q \
            -e 'error:[[:space:]]\|warning:[[:space:]].*different address spaces' && ret=1
        printf '%s\n' "$line"
    done
    return $ret
}

main "$@"
