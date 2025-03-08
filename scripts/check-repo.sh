#!/usr/bin/env bash

# Ensure that the common script exists and is readable, then verify it has no
# syntax errors and defines the required function.
common_script="$(dirname "$0")/common.sh"
[ -r "$common_script" ] || { echo "[!] '$common_script' not found or not readable." >&2; exit 1; }
bash -n "$common_script" >/dev/null 2>&1 || { echo "[!] '$common_script' contains syntax errors." >&2; exit 1; }
source "$common_script"
declare -F set_colors >/dev/null 2>&1 || { echo "[!] '$common_script' does not define the required function." >&2; exit 1; }

set_colors

check_github_actions

TOTAL_STEPS=6
CURRENT_STEP=0

# 0. Check environment
((CURRENT_STEP++))
progress "$CURRENT_STEP" "$TOTAL_STEPS"

if ! command -v curl &>/dev/null; then
  throw "curl not installed."
fi

if ! command -v git &>/dev/null; then
  throw "git not installed."
fi

# Retrieve git email.
GIT_EMAIL=$(git config user.email)

# Check if email is set.
if [ -z "$GIT_EMAIL" ]; then
  throw "Git email is not set."
fi

# Validate email using a regex.
# This regex matches a basic email pattern.
if ! [[ "$GIT_EMAIL" =~ ^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$ ]]; then
  throw "Git email '$GIT_EMAIL' is not valid."
fi

# 1. Sleep for a random number of milliseconds
# The time interval is important to reduce unintended network traffic.
((CURRENT_STEP++))
progress "$CURRENT_STEP" "$TOTAL_STEPS"

# Generate a random integer in [0..999].
random_ms=$((RANDOM % 1000))

# Convert that to a decimal of the form 0.xxx so that 'sleep' interprets it as seconds.
# e.g., if random_ms is 5, we convert that to 0.005 (i.e. 5 ms).
sleep_time="0.$(printf "%03d" "$random_ms")"

sleep "$sleep_time"

# 2. Fetch latest commit from GitHub
((CURRENT_STEP++))
progress "$CURRENT_STEP" "$TOTAL_STEPS"

REPO_OWNER=$(git config -l | grep -w remote.origin.url | sed -E 's%^.*github.com[/:]([^/]+)/lab0-c.*%\1%')
REPO_NAME="lab0-c"

repo_html=$(curl -s "https://github.com/${REPO_OWNER}/${REPO_NAME}")

# Extract the default branch name from data-default-branch="..."
DEFAULT_BRANCH=$(echo "$repo_html" | sed -nE "s#.*${REPO_OWNER}/${REPO_NAME}/blob/([^/]+)/LICENSE.*#\1#p" | head -n 1)

if [ "$DEFAULT_BRANCH" != "master" ]; then
  echo "$DEFAULT_BRANCH"
  throw "The default branch for $REPO_OWNER/$REPO_NAME is not 'master'."
fi

# Construct the URL to the commits page for the default branch
COMMITS_URL="https://github.com/${REPO_OWNER}/${REPO_NAME}/commits/${DEFAULT_BRANCH}"

temp_file=$(mktemp)
curl -sSL -o "$temp_file" "$COMMITS_URL"

# general grep pattern that finds commit links
upstream_hash=$(
  sed -nE 's/.*href="[^"]*\/commit\/([0-9a-f]{40}).*/\1/p' "$temp_file" | head -n 1
)

rm -f "$temp_file"

if [ -z "$upstream_hash" ]; then
  throw "Failed to retrieve upstream commit hash from GitHub.\n"
fi

# 3. Check local repository awareness

((CURRENT_STEP++))
progress "$CURRENT_STEP" "$TOTAL_STEPS"

# Check if the local workspace knows about $upstream_hash.
if ! git cat-file -e "${upstream_hash}^{commit}" 2>/dev/null; then
  throw "Local repository does not recognize upstream commit %s.\n\
    Please fetch or pull from remote to update your workspace.\n" "$upstream_hash"
fi

# 4. List non-merge commits between BASE_COMMIT and upstream_hash

((CURRENT_STEP++))
progress "$CURRENT_STEP" "$TOTAL_STEPS"

# Base commit from which to start checking.
BASE_COMMIT="dac4fdfd97541b5872ab44615088acf603041d0c"

# Get a list of non-merge commit hashes after BASE_COMMIT in the local workspace.
commits=$(git rev-list --no-merges "${BASE_COMMIT}".."${upstream_hash}")

if [ -z "$commits" ]; then
  throw "No new non-merge commits found after the check point."
fi

# 5. Validate each commit for Change-Id.

((CURRENT_STEP++))
progress "$CURRENT_STEP" "$TOTAL_STEPS"

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
    printf "\n${RED}[!]${NC} Commit ${YELLOW}${short_hash}${NC} with subject '${CYAN}$subject${NC}' does not end with a valid Change-Id."
    failed=1
  fi
done

if [ $failed -ne 0 ]; then
  printf "\n\nSome commits are missing a valid ${YELLOW}Change-Id${NC}. Amend the commit messages accordingly.\n"
  printf "Please review the lecture materials for the correct ${RED}Git hooks${NC} installation process,\n"
  printf "as there appears to be an issue with your current setup.\n"
  exit 1
fi

echo "Fingerprint: $(make_random_string 24 "$REPO_OWNER")"

exit 0
