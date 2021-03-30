#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

set -e
# TODO: reduce duplication with scripts/sof-pre-commit-hook.sh
# and with .github/workflows/ with either some .conf file
# or some wrapper script
exec git show --format=email HEAD |
  ./scripts/checkpatch.pl --no-tree --strict --codespell --ignore C99_COMMENT_TOLERANCE
