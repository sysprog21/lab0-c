#!/usr/bin/env bash

# Check that every non-merge commit after the specified base commit has a commit
# message ending with a valid Change-Id line. A valid Change-Id line must be the
# last non-empty line of the commit message and follow the format:
#
#   Change-Id: I<hexadecimal_hash>
#
# Merge commits are excluded from this check.

# Ensure that the common script exists and is readable, then verify it has no
# syntax errors and defines the required function.
common_script="$(dirname "$0")/common.sh"
[ -r "$common_script" ] || { echo "[!] '$common_script' not found or not readable." >&2; exit 1; }
bash -n "$common_script" >/dev/null 2>&1 || { echo "[!] '$common_script' contains syntax errors." >&2; exit 1; }
source "$common_script"
declare -F set_colors >/dev/null 2>&1 || { echo "[!] '$common_script' does not define the required function." >&2; exit 1; }

set_colors

# Base commit from which to start checking.
BASE_COMMIT="0b8be2c15160c216e8b6ec82c99a000e81c0e429"

# Get a list of non-merge commit hashes after BASE_COMMIT.
commits=$(git rev-list --no-merges "${BASE_COMMIT}"..HEAD)

failed=0

for commit in $commits; do
  # Retrieve the commit message for the given commit.
  commit_msg=$(git log -1 --format=%B "${commit}")

  # Extract the last non-empty line from the commit message.
  last_line=$(echo "$commit_msg" | awk 'NF {line=$0} END {print line}')

  # Check if the last line matches the expected Change-Id format.
  if [[ ! $last_line =~ ^Change-Id:\ I[0-9a-fA-F]+$ ]]; then
    subject=$(git log -1 --format=%s "${commit}")
    short_hash=$(git rev-parse --short "${commit}")
    echo "Commit ${short_hash} with subject '$subject' does not end with a valid Change-Id."
    failed=1
  fi
done

if [ $failed -ne 0 ]; then
  throw "Some commits are missing a valid Change-Id. Please amend the commit messages accordingly."
fi

exit 0
