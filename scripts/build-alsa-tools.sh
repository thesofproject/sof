#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 Intel Corporation. All rights reserved.

# fail immediately on any errors
set -e

# Array of ALSA Git repository URLs.  Add or remove repositories as needed.
declare -a REPOS=(
    "https://github.com/thesofproject/alsa-lib.git"
    "https://github.com/thesofproject/alsa-utils.git"
    # Add more repositories here...
)

# Commit ID to check for (optional). If specified, the script will update
# the repository if this commit ID is not found.  Leave empty to skip.
declare -a COMMIT_ID=(
    "df8f1cc1ec9d9ee15be5e2c23ad25b9389fd8766"
    "09550cd393b1a7d307ee6f26637b1ed7bd275e38"
)

# Directory where repositories will be cloned/updated.
if [[ -z "$SOF_WORKSPACE" ]]; then
  # Environment variable is empty or unset so use default
  BASE_DIR="$HOME/work"
else
  # Environment variable exists and has a value
  BASE_DIR="$SOF_WORKSPACE"
fi

# Arguments to pass to ./configure for each repository.  Add or remove
declare -a CONFIGURE_ARGS=(
    "--prefix=${BASE_DIR}/tools"
    "--prefix=${BASE_DIR}/tools \
        --with-alsa-prefix=${BASE_DIR}/tools \
        --with-alsa-inc-prefix=${BASE_DIR}/tools/include \
        --with-sysroot=${BASE_DIR}/tools \
        --with-udev-rules-dir=${BASE_DIR}/tools \
        PKG_CONFIG_PATH=${BASE_DIR}/tools \
        LDFLAGS=-L${BASE_DIR}/tools/lib \
        --with-asound-state-dir=${BASE_DIR}/tools/var/lib/alsa \
        --with-systemdsystemunitdir=${BASE_DIR}/tools/lib/systemd/system"
)

# Arguments to pass to make for each repository.  Add or remove arguments as needed.
declare -a TARGET_ARGS=(
    "--disable-old-symbols"
    "--enable-alsatopology"
)

# Function to check if a commit ID exists in a repository
check_commit() {
    local repo_dir="$1"
    local commit_id="$2"

    if [ -z "$commit_id" ]; then
        return 0  # Skip check if no commit ID is provided
    fi

    if ! git -C "$repo_dir" rev-parse --quiet --verify "$commit_id" >/dev/null 2>&1; then
        return 1  # Commit ID not found
    else
        return 0  # Commit ID found
    fi
}


# Function to update the repository
update_repo() {
    local repo_dir="$1"
    echo "Updating repository: $repo_dir"
    git -C "$repo_dir" fetch --all
    git -C "$repo_dir" pull
}

# Function to build and install the repository
build_and_install() {
    local repo_dir="$1"
    local configure_args="$2"
    local target_args="$3"

    echo "Building and installing: $repo_dir $configure_args $target_args"

    if [ ! -f "$repo_dir/gitcompile" ]; then
          echo "Error: gitcompile not found in $repo_dir" >&2
          exit 1
    fi

    # if Makefile exists then we can just run make
    if [ ! -f "$repo_dir/Makefile" ]; then
        (cd "$repo_dir" && ./gitcompile $configure_args $target_args) || \
            { echo "configure failed in $repo_dir"; exit 1; }
    else
        (cd "$repo_dir" && make -j) || { echo "make failed in $repo_dir"; exit 1; }
    fi

    (cd "$repo_dir" && make install) || { echo "make install failed in $repo_dir"; exit 1; }

    echo "Build and installation complete for $repo_dir"
}

# Main script logic
mkdir -p "$BASE_DIR"

for ((i = 0; i < ${#REPOS[@]}; i++)); do
    echo "Counter: $i, Value: ${REPOS[i]}"
    repo_url=${REPOS[i]}

    repo_name=$(basename "$repo_url" .git)  # Extract repo name
    repo_dir="$BASE_DIR/$repo_name"

    if [ ! -d "$repo_dir" ]; then
        echo "Cloning repository: $repo_url"
        git clone "$repo_url" "$repo_dir" || { echo "git clone failed for $repo_url"; exit 1; }
    elif ! check_commit "$repo_dir" "${COMMIT_ID[i]}"; then
        update_repo "$repo_dir"
    else
        echo "Repository $repo_name is up to date."
    fi

    build_and_install "$repo_dir" "${CONFIGURE_ARGS[i]}"

done

echo "All repositories processed."
