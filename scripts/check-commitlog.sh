#!/usr/bin/env bash

# This script checks:
# 1. Change-Id presence (indicates commit-msg hook processing)
# 2. Commit message quality (subject line format and length)
# 3. WIP commits detection (work-in-progress commits that start with WIP:)
# 4. GitHub web interface usage (commits without proper hooks)
# 5. Queue.c modifications require descriptive commit body
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

# Early exit if no commits to check
[[ -z "$commits" ]] && { echo -e "${GREEN}No commits to check.${NC}"; exit 0; }

# Ultra-fast approach: minimize git calls and parsing overhead
failed=0
warnings=0
suspicious_commits=()

# Cache all commit data at once using the most efficient git format
declare -A commit_cache short_cache subject_cache msg_cache

# Single git call to populate all caches - handle multiline messages properly
current_commit=""
current_msg=""
reading_msg=false

while IFS= read -r line; do
  case "$line" in
    "COMMIT "*)
      # Save previous message if we were reading one
      if [[ "$reading_msg" = true && -n "$current_commit" ]]; then
        msg_cache["$current_commit"]="$current_msg"
      fi
      current_commit="${line#COMMIT }"
      commit_cache["$current_commit"]=1
      reading_msg=false
      current_msg=""
      ;;
    "SHORT "*)
      short_cache["$current_commit"]="${line#SHORT }"
      ;;
    "SUBJECT "*)
      subject_cache["$current_commit"]="${line#SUBJECT }"
      ;;
    "MSGSTART")
      reading_msg=true
      current_msg=""
      ;;
    *)
      # If we're reading a message, accumulate lines
      if [[ "$reading_msg" = true ]]; then
        if [[ -z "$current_msg" ]]; then
          current_msg="$line"
        else
          current_msg="$current_msg"$'\n'"$line"
        fi
      fi
      ;;
  esac
done < <(git log --format="COMMIT %H%nSHORT %h%nSUBJECT %s%nMSGSTART%n%B" --no-merges "${BASE_COMMIT}..HEAD")

# Save the last message
if [[ "$reading_msg" = true && -n "$current_commit" ]]; then
  msg_cache["$current_commit"]="$current_msg"
fi

# Process cached data - no more git calls needed
for commit in "${!commit_cache[@]}"; do
  [[ -z "$commit" ]] && continue

  short_hash="${short_cache[$commit]}"
  subject="${subject_cache[$commit]}"
  full_msg="${msg_cache[$commit]}"

  # Initialize issue tracking
  has_issues=0
  has_warnings=0
  issue_list=""
  warning_list=""

  # Check 1: Change-Id validation (fastest check first)
  if [[ "$full_msg" != *"Change-Id: I"* ]]; then
    has_issues=1
    issue_list+="Missing valid Change-Id (likely bypassed commit-msg hook)|"
    ((failed++))
  fi

  # Check 2: Bypass indicators - only for actual WIP commits
  # Skip commits that are documenting features (like this commit checker itself)
  if [[ "$subject" =~ ^WIP[[:space:]]*: ]] || [[ "$subject" =~ ^wip[[:space:]]*: ]]; then
    has_warnings=1
    warning_list+="Work in progress commit|"
    ((warnings++))
  fi

  # Check 3: Subject validation (batch character operations)
  subject_len=${#subject}
  first_char="${subject:0:1}"
  last_char="${subject: -1}"

  # Length checks
  if [[ $subject_len -le 10 ]]; then
    has_warnings=1
    warning_list+="Subject very short ($subject_len chars)|"
    ((warnings++))
  elif [[ $subject_len -ge 80 ]]; then
    has_issues=1
    issue_list+="Subject too long ($subject_len chars)|"
    ((failed++))
  fi

  # Character validation using ASCII values
  if [[ ${#first_char} -eq 1 ]]; then
    # Check if it's a lowercase letter
    case "$first_char" in
      [a-z])
        has_issues=1
        issue_list+="Subject not capitalized|"
        ((failed++))
        ;;
    esac
  fi

  # Period check
  if [[ "$last_char" == "." ]]; then
    has_issues=1
    issue_list+="Subject ends with period|"
    ((failed++))
  fi

  # Generic filename check (simplified pattern)
  if [[ "$subject" =~ ^(Update|Fix|Change|Modify)[[:space:]] ]]; then
    if [[ "$subject" =~ \.(c|h)$ ]]; then
      has_warnings=1
      warning_list+="Generic filename-only subject|"
      ((warnings++))
    fi
  fi

  # Check 4: Web interface (string contains check)
  if [[ "$full_msg" == *"Co-authored-by:"* ]]; then
    if [[ "$full_msg" != *"Change-Id:"* ]]; then
      has_issues=1
      issue_list+="Likely created via GitHub web interface|"
      ((failed++))
    fi
  fi

  # Check 5: Queue.c body (most expensive - do last and only when needed)
  if [[ "$full_msg" =~ ^[^$'\n']*$'\n'[[:space:]]*$'\n'Change-Id: ]]; then
    # Body appears empty - check if queue.c was modified
    if git diff-tree --no-commit-id --name-only -r "$commit" | grep -q "^queue\.c$"; then
      has_issues=1
      issue_list+="Missing commit body for queue.c changes|"
      ((failed++))
    fi
  fi

  # Report issues (only if found)
  if [[ $has_issues -eq 1 || $has_warnings -eq 1 ]]; then
    echo -e "${YELLOW}Commit ${short_hash}:${NC} ${subject}"

    if [[ $has_issues -eq 1 ]]; then
      IFS='|' read -ra issues <<< "${issue_list%|}"
      for issue in "${issues[@]}"; do
        [[ -n "$issue" ]] && echo -e "  [ ${RED}FAIL${NC} ] $issue"
      done
      suspicious_commits+=("$short_hash: $subject")
    fi

    if [[ $has_warnings -eq 1 ]]; then
      IFS='|' read -ra warnings_arr <<< "${warning_list%|}"
      for warning in "${warnings_arr[@]}"; do
        [[ -n "$warning" ]] && echo -e "  ${YELLOW}!${NC} $warning"
      done
    fi
  fi
done

if [[ $failed -gt 0 ]]; then
  echo -e "\n${RED}Problematic commits detected:${NC}"
  for commit in "${suspicious_commits[@]}"; do
    echo -e "  ${RED}•${NC} $commit"
  done

  echo -e "\n${RED}These commits likely bypassed git hooks. Recommended actions:${NC}"
  echo -e "1. ${YELLOW}Verify hooks are installed:${NC} scripts/install-git-hooks"
  echo -e "2. ${YELLOW}Never use --no-verify flag${NC}"
  echo -e "3. ${YELLOW}Avoid GitHub web interface for commits${NC}"
  echo -e "4. ${YELLOW}Amend commits if possible:${NC} git rebase -i ${BASE_COMMIT}"
  echo

  throw "Git hook compliance validation failed. Please fix the issues above."
fi

if [[ $warnings -gt 0 ]]; then
  echo -e "\n${YELLOW}Some commits have quality warnings but passed basic validation.${NC}"
fi

exit 0
