#!/usr/bin/env bash

# Parse command line arguments
FORCE_REFRESH=false
QUIET_MODE=false
while [[ $# -gt 0 ]]; do
  case "$1" in
    --force-refresh|-f)
      FORCE_REFRESH=true
      shift
      ;;
    --quiet|-q)
      QUIET_MODE=true
      shift
      ;;
    --help|-h)
      echo "Usage: $0 [--force-refresh|-f] [--quiet|-q] [--help|-h]"
      echo "  --force-refresh, -f  Force refresh of cached data"
      echo "  --quiet, -q          Suppress progress and informational output"
      echo "  --help, -h          Show this help message"
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      echo "Use --help for usage information"
      exit 1
      ;;
  esac
done

# Ensure that the common script exists and is readable, then verify it has no
# syntax errors and defines the required function.
common_script="$(dirname "$0")/common.sh"
[ -r "$common_script" ] || { echo "[!] '$common_script' not found or not readable." >&2; exit 1; }
bash -n "$common_script" >/dev/null 2>&1 || { echo "[!] '$common_script' contains syntax errors." >&2; exit 1; }
source "$common_script"
declare -F set_colors >/dev/null 2>&1 || { echo "[!] '$common_script' does not define the required function." >&2; exit 1; }

set_colors

check_github_actions

# Override progress function if in quiet mode
if [ "$QUIET_MODE" = true ]; then
  progress() {
    # Do nothing in quiet mode
    :
  }
fi

# Cache configuration
CACHE_DIR="$HOME/.cache/lab0-c"
CACHE_FILE="$CACHE_DIR/upstream_commit"
CACHE_EXPIRY=900  # Cache for 15 minutes (in seconds)

# Create cache directory if it doesn't exist
mkdir -p "$CACHE_DIR"

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

# Check if cache exists and is still valid
use_cache=false
if [ "$FORCE_REFRESH" = true ]; then
  if [ "$QUIET_MODE" = false ]; then
    printf "\r%80s\r" " "
    echo "Force refresh requested. Clearing cache..."
  fi
  rm -f "$CACHE_FILE" "$RATE_LIMIT_FILE"
elif [ -f "$CACHE_FILE" ]; then
  cache_age=$(($(date +%s) - $(stat -f %m "$CACHE_FILE" 2>/dev/null || stat -c %Y "$CACHE_FILE" 2>/dev/null || echo 0)))
  if [ "$cache_age" -lt "$CACHE_EXPIRY" ]; then
    upstream_hash=$(cat "$CACHE_FILE")
    if [ -n "$upstream_hash" ]; then
      use_cache=true
      if [ "$QUIET_MODE" = false ]; then
        printf "\r%80s\r" " "
        echo "Using cached upstream commit (${cache_age}s old, expires in $((CACHE_EXPIRY - cache_age))s)"
      fi
    fi
  else
    if [ "$QUIET_MODE" = false ]; then
      printf "\r%80s\r" " "
      echo "Cache expired (${cache_age}s old). Refreshing..."
    fi
  fi
fi

# Only sleep and fetch if not using cache
if [ "$use_cache" = false ]; then
  # Generate a random integer in [0..999].
  random_ms=$((RANDOM % 1000))

  # Add exponential backoff if we've been rate limited recently
  RATE_LIMIT_FILE="$CACHE_DIR/rate_limited"
  if [ -f "$RATE_LIMIT_FILE" ]; then
    last_limited=$(($(date +%s) - $(stat -f %m "$RATE_LIMIT_FILE" 2>/dev/null || stat -c %Y "$RATE_LIMIT_FILE" 2>/dev/null || echo 0)))
    if [ "$last_limited" -lt 300 ]; then  # If rate limited in last 5 minutes
      random_ms=$((random_ms + 2000))  # Add 2 seconds
      if [ "$QUIET_MODE" = false ]; then
        printf "\r%80s\r" " "
        echo "Rate limit detected. Adding delay..."
      fi
    fi
  fi

  # Convert that to a decimal of the form 0.xxx so that 'sleep' interprets it as seconds.
  # e.g., if random_ms is 5, we convert that to 0.005 (i.e. 5 ms).
  # Use printf for portability (bc might not be installed)
  sleep_time="0.$(printf "%03d" "$((random_ms % 1000))")"

  # For delays > 1 second, handle separately
  if [ "$random_ms" -ge 1000 ]; then
    sleep_seconds=$((random_ms / 1000))
    sleep_ms=$((random_ms % 1000))
    sleep_time="${sleep_seconds}.$(printf "%03d" "$sleep_ms")"
  fi

  sleep "$sleep_time"
fi

# 2. Fetch latest commit from GitHub
((CURRENT_STEP++))
progress "$CURRENT_STEP" "$TOTAL_STEPS"

REPO_OWNER=$(git config -l | grep -w remote.origin.url | sed -E 's%^.*github.com[/:]([^/]+)/lab0-c.*%\1%')
REPO_NAME="lab0-c"

# Only fetch from network if not using cache
if [ "$use_cache" = false ]; then
  # First try using git ls-remote (much faster and less likely to be rate limited)
  if [ "$QUIET_MODE" = false ]; then
    printf "\r%80s\r" " "
    echo "Checking upstream repository..."
  fi
  upstream_hash=$(git ls-remote --heads origin master 2>/dev/null | cut -f1)

  # If git ls-remote fails or returns empty, fall back to web scraping
  if [ -z "$upstream_hash" ]; then
    if [ "$QUIET_MODE" = false ]; then
      printf "\r%80s\r" " "
      echo "git ls-remote failed. Falling back to web scraping..."
    fi

    # Add User-Agent header to avoid being blocked
    USER_AGENT="Mozilla/5.0 (compatible; lab0-c-checker/1.0)"

    # Try with rate limit detection
    repo_html=$(curl -s -w "\n%{http_code}" -H "User-Agent: $USER_AGENT" "https://github.com/${REPO_OWNER}/${REPO_NAME}")
    http_code=$(echo "$repo_html" | tail -n 1)
    repo_html=$(echo "$repo_html" | sed '$d')

    # Check for rate limiting (HTTP 429 or 403)
    if [ "$http_code" = "429" ] || [ "$http_code" = "403" ]; then
      touch "$RATE_LIMIT_FILE"
      if [ "$QUIET_MODE" = false ]; then
        printf "\r%80s\r" " "
        echo "GitHub rate limit detected (HTTP $http_code). Using fallback..."
      fi

      # Try to use last known good commit from git log
      upstream_hash=$(git ls-remote origin master 2>/dev/null | cut -f1)
      if [ -z "$upstream_hash" ]; then
        throw "Rate limited by GitHub and no fallback available. Please try again later."
      fi
    else
      # Extract the default branch name from data-default-branch="..."
      DEFAULT_BRANCH=$(echo "$repo_html" | sed -nE "s#.*${REPO_OWNER}/${REPO_NAME}/blob/([^/]+)/LICENSE.*#\1#p" | head -n 1)

      if [ "$DEFAULT_BRANCH" != "master" ]; then
        echo "$DEFAULT_BRANCH"
        throw "The default branch for $REPO_OWNER/$REPO_NAME is not 'master'."
      fi

      # Construct the URL to the commits page for the default branch
      COMMITS_URL="https://github.com/${REPO_OWNER}/${REPO_NAME}/commits/${DEFAULT_BRANCH}"

      temp_file=$(mktemp)
      curl -sSL -H "User-Agent: $USER_AGENT" -o "$temp_file" "$COMMITS_URL"

      # general grep pattern that finds commit links
      upstream_hash=$(
        sed -nE 's/.*href="[^"]*\/commit\/([0-9a-f]{40}).*/\1/p' "$temp_file" | head -n 1
      )

      rm -f "$temp_file"

      # If HTML parsing fails, fallback to using GitHub REST API
      if [ -z "$upstream_hash" ]; then
        API_URL="https://api.github.com/repos/${REPO_OWNER}/${REPO_NAME}/commits"

        # Try to use cached GitHub credentials from GitHub CLI
        # https://docs.github.com/en/get-started/git-basics/caching-your-github-credentials-in-git
        if command -v gh >/dev/null 2>&1; then
          TOKEN=$(gh auth token 2>/dev/null)
          if [ -n "$TOKEN" ]; then
            response=$(curl -sSL -H "Authorization: token $TOKEN" -H "User-Agent: $USER_AGENT" "$API_URL")
          fi
        fi

        # If response is empty (i.e. token not available or failed), use unauthenticated request.
        if [ -z "$response" ]; then
          response=$(curl -sSL -H "User-Agent: $USER_AGENT" "$API_URL")
        fi

        # Extract the latest commit SHA from the JSON response
        upstream_hash=$(echo "$response" | grep -m 1 '"sha":' | sed -E 's/.*"sha": "([^"]+)".*/\1/')
      fi
    fi
  fi

  if [ -z "$upstream_hash" ]; then
    throw "Failed to retrieve upstream commit hash from GitHub.\n"
  fi

  # Cache the result
  echo "$upstream_hash" > "$CACHE_FILE"
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

if [ "$QUIET_MODE" = false ]; then
  printf "\r%80s\r" " "
  echo "Fingerprint: $(make_random_string 24 "$REPO_OWNER")"
fi

exit 0
