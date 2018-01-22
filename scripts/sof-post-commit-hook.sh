#!/bin/sh

set -e
exec git show --format=email HEAD | ./scripts/checkpatch.pl --no-tree --strict --codespell
