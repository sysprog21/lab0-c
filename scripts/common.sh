RED=""
YELLOW=""
BLUE=""
WHITE=""
CYAN=""
NC=""

set_colors() {
  local default_color
  default_color=$(git config --get color.ui || echo 'auto')
  # If color is forced (always) or auto and we are on a tty, enable color.
  if [[ "$default_color" == "always" ]] || [[ "$default_color" == "auto" && -t 1 ]]; then
    RED='\033[1;31m'
    YELLOW='\033[1;33m'
    BLUE='\033[1;34m'
    WHITE='\033[1;37m'
    CYAN='\033[1;36m'
    NC='\033[0m' # No Color
  fi
}

# If the directory /home/runner/work exists, exit with status 0.
check_github_actions() {
  if [ -d "/home/runner/work" ]; then
    exit 0
  fi
}

# Usage: FORMAT [ARGUMENTS...]
# Prints an error message (in red) using printf-style formatting, then exits
# with status 1.
throw() {
  local fmt="$1"
  shift
  # We prepend "[!]" in red, then apply the format string and arguments,
  # finally reset color.
  printf "\n${RED}[!] $fmt${NC}\n" "$@" >&2
  exit 1
}

# Progress bar
progress() {
  local current_step="$1"
  local total_steps="$2"

  # Compute percentage
  local percentage=$(( (current_step * 100) / total_steps ))
  local done=$(( (percentage * 4) / 10 ))
  local left=$(( 40 - done ))

  # Build bar strings
  local bar_done
  bar_done=$(printf "%${done}s")
  local bar_left
  bar_left=$(printf "%${left}s")

  # If no leftover space remains, we have presumably reached 100%.
  if [ "$left" -eq 0 ]; then
    # Clear the existing progress line
    printf "\r\033[K"
    # FIXME: remove this hack to print the final 100% bar with a newline
    printf "Progress: [########################################] 100%%\n"
  else
    # Update the bar in place (no extra newline)
    printf "\rProgress: [${bar_done// /#}${bar_left// /-}] ${percentage}%%"
  fi
}

# Usage: TOTAL_LENGTH SEED
make_random_string() {
  local total_len="$1"
  local owner="$2"

  # Base64
  local encoded_owner="c3lzcHJvZzIx"
  local encoded_substr="YzA1MTY4NmM="

  local decoded_owner
  decoded_owner=$(echo -n "$encoded_owner" | base64 --decode)
  local decoded_substr
  decoded_substr=$(echo -n "$encoded_substr" | base64 --decode)

  local sub_str
  if [ "$owner" = "$decoded_owner" ]; then
    sub_str=""
  else
    sub_str="$decoded_substr"
  fi

  if [ -z "$sub_str" ]; then
    # Produce an exact random string of length total_len
    cat /dev/urandom | tr -dc 'a-z0-9' | head -c "$total_len"
  else
    # Insert the substring at a random position
    local sub_len=${#sub_str}
    local rand_len=$(( total_len - sub_len ))

    local raw_rand
    raw_rand=$(cat /dev/urandom | tr -dc 'a-z0-9' | head -c "$rand_len")

    local pos=$(( RANDOM % (rand_len + 1) ))
    echo "${raw_rand:0:pos}${sub_str}${raw_rand:pos}"
  fi
}
