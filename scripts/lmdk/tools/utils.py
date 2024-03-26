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

    cwd = run_kwargs.get('cwd')
    print_cwd = f"In dir: {cwd}" if cwd else f"in current dir: {os.getcwd()}"
    print_args = shlex.join(command_args)
    output = f"{print_cwd}; running command:\n    {print_args}"
    env_arg = run_kwargs.get('env')
    env_change = set(env_arg.items()) - set(os.environ.items()) if env_arg else None
    if env_change and run_kwargs.get('sof_log_env'):
        output += "\n... with extra/modified environment:\n"
        for k_v in env_change:
            output += f"{k_v[0]}={k_v[1]}\n"
    print(output, flush=True)

    run_kwargs = {k: run_kwargs[k] for k in run_kwargs if not k.startswith("sof_")}

    if not 'check' in run_kwargs:
        run_kwargs['check'] = True
    #pylint:disable=subprocess-run-check

    return subprocess.run(*run_args, **run_kwargs)