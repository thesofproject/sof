#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025-2026 Intel Corporation. All rights reserved.

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

# Commit ID to check out. If specified, the script will checkout this commit.
declare -a COMMIT_ID=(
    "cdedfb1a1044eb774915de21b63a1b6aa93276f6"
    "e86d97b6237f88ab5925c0b41e3e3589a1560d86"
    "f5acabf4e1a3fcba024081bb1871a2ed59aa1c28"
    "719139ce755a0f31cbf1c37f7f98adcc7fc9f425"
    "d37128311b445e758136b8602d1bbd2a755e115d"
)

# Directory where repositories will be cloned/updated.
# Default to the parent directory containing the SOF workspace.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOF_PARENT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BASE_DIR="${1:-"$SOF_PARENT_DIR"}"

# Function to check if a commit ID exists in a repository
check_commit() {
    local repo_dir="$1"
    local commit_id="$2"

    if [ -z "$commit_id" ]; then
        return 0  # Skip check if no commit ID is provided
    fi

    if ! git -C "$repo_dir" rev-parse --quiet --verify "$commit_id^{commit}" >/dev/null 2>&1; then
        return 1  # Commit ID not found locally
    else
        return 0  # Commit ID found locally
    fi
}

# Function to update the repository
update_repo() {
    local repo_dir="$1"
    echo "Updating repository: $repo_dir"
    git -C "$repo_dir" fetch --all --tags --prune
}

# Function to checkout the required commit ID
checkout_commit() {
    local repo_dir="$1"
    local commit_id="$2"
    local repo_name="$3"

    if [ -z "$commit_id" ]; then
        return 0
    fi

    local current_commit
    current_commit=$(git -C "$repo_dir" rev-parse HEAD)

    local target_commit
    target_commit=$(git -C "$repo_dir" rev-parse "$commit_id^{commit}")

    if [ "$current_commit" != "$target_commit" ]; then
        echo "Checking out $commit_id in $repo_name..."
        git -C "$repo_dir" checkout -q "$commit_id"
    else
        echo "Repository $repo_name is already at commit $commit_id."
    fi
}

# Main script logic
mkdir -p "$BASE_DIR"

for ((i = 0; i < ${#REPOS[@]}; i++)); do
    repo_url="${REPOS[i]}"
    commit_id="${COMMIT_ID[i]}"
    repo_name=$(basename "$repo_url" .git)
    repo_dir="$BASE_DIR/$repo_name"

    echo "=== [$((i + 1))/${#REPOS[@]}] $repo_name ==="

    if [ ! -d "$repo_dir/.git" ]; then
        echo "Cloning repository: $repo_url -> $repo_dir"
        git clone "$repo_url" "$repo_dir" || { echo "git clone failed for $repo_url"; exit 1; }
    fi

    if ! check_commit "$repo_dir" "$commit_id"; then
        update_repo "$repo_dir"
    fi

    checkout_commit "$repo_dir" "$commit_id" "$repo_name"
done

echo "All repositories processed and checked out to required commits."
