#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

exec git diff --cached | scripts/checkpatch.pl --no-tree --codespell --no-signoff -q -

