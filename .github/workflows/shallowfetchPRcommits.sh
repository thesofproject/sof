#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause

set -e

# This script fetches Pull Request commits missing from a shallow clone
# and creates a PR_SHAs.txt file. This script has a limit of 500 commits but the
# github API used has a lower limit of 250 commits.

# It does not rely on git merge-bases which basically don't work with
# shallow clones:
# https://github.com/thesofproject/linux/issues/2556

# Design goals:
#
# - Keep the code short and as simple as possible. No one is interested
#   in maintaining this sort of script.
#
# - Fast and accurate for small Pull Requests
#
# - For large Pull Requests _with merges_ the only objective is to
#   complete in a reasonable time; say less than 10 minutes. It's very
#   unlikely will look at 250 checkpatch results and time optimizations
#   should not make this script more complex.


# Sample usage:
#   $0  thesoftproject/linux  2772
main()
{
    local gh_project="$1"
    local pr_number="$2"

    printf '%s: fetching PR %d for project %s\n' "$0" "$pr_number" "$gh_project"

    # As of March 2021, Github's documented limit is 250 commits
    # Let's have another cap a 500.
    # https://docs.github.com/en/rest/reference/pulls#list-commits-on-a-pull-request
    local pagelen PRlen=0
    for i in 1 2 3 4 5; do
        curl -H 'Accept: application/vnd.github.v3+json' \
         "https://api.github.com/repos/$gh_project/pulls/$pr_number/commits?per_page=100&page=$i" \
         > commits_"$i".json
        pagelen=$(jq length < commits_$i.json)
        if [ "$pagelen" -eq 0 ]; then
            break
        fi
        PRlen=$((PRlen + pagelen))
    done

    printf 'Found %d commits, SHA1 list is in PR_SHAs.txt\n' "$PRlen"

    # 'cut' removes double quotes
    cat commits_?.json |
        jq '.[] | .sha' |
        cut -b2-41 > PR_SHAs.txt

    # PRlen+1 gets us the merge base for simple, linear histories. For
    # pull requests with merges, depth=PRLEN goes already much further
    # than needed and +1 makes little difference. It's not clear when
    # and for what sort of PRs git fetching individual commits would be
    # faster so keep a single and simple fetch for now.

    set -x # this command may take a while so show it
    git fetch --depth "$((PRlen+1))" "https://github.com/$gh_project" "pull/$pr_number/head"

}

main "$@"
