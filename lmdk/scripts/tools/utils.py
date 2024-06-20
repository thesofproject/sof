#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause


import shlex
import subprocess
import shutil
import os


def rmtree_if_exists(directory):
    "This is different from ignore_errors=False because it deletes everything or nothing"
    if os.path.exists(directory):
        shutil.rmtree(directory)


def execute_command(*run_args, **run_kwargs):
    """[summary] Provides wrapper for subprocess.run that prints
    command executed when 'more verbose' verbosity level is set."""
    command_args = run_args[0]

    # If you really need the shell in some non-portable section then
    # invoke subprocess.run() directly.
    if run_kwargs.get('shell') or not isinstance(command_args, list):
        raise RuntimeError("Do not rely on non-portable shell parsing")

    if not 'check' in run_kwargs:
        run_kwargs['check'] = True
    #pylint:disable=subprocess-run-check

    return subprocess.run(*run_args, **run_kwargs)
