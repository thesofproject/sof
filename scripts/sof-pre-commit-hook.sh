#!/bin/bash

exec git diff --cached | scripts/checkpatch.pl --no-tree --codespell --no-signoff -q -

