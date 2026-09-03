#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 Intel Corporation. All rights reserved.

# fail immediately on any errors
set -e

# Array of TFLM Git repository URLs.  Add or remove repositories as needed.
declare -a REPOS=(
    "https://github.com/thesofproject/nnlib-hifi4"
    "https://github.com/tensorflow/tflite-micro"
    "https://github.com/thesofproject/flatbuffers"
    "https://github.com/google/gemmlowp"
    "https://github.com/google/ruy"
)

# Commit ID to check for (optional). If specified, the script will update
# the repository if this commit ID is not found.  Leave empty to skip.
declare -a COMMIT_ID=(
    "cdedfb1a1044eb774915de21b63a1b6aa93276f6"
    "e86d97b6237f88ab5925c0b41e3e3589a1560d86"
    "f5acabf4e1a3fcba024081bb1871a2ed59aa1c28"
    "719139ce755a0f31cbf1c37f7f98adcc7fc9f425"
    "d37128311b445e758136b8602d1bbd2a755e115d"
)

# Directory where repositories will be cloned/updated.
BASE_DIR="$HOME/work/sof"  # Or any other desired location

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
done

echo "All repositories processed."
