#!/usr/bin/env bash
#
# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0
#

###############################################################################
# Include guard (MUST BE FIRST)
###############################################################################
test -n "${CI_LIB_IO_SH__:-}" || declare -i CI_LIB_IO_SH__=0
if ((CI_LIB_IO_SH__++ != 0)); then
  return 0
fi

###############################################################################
# Cloud Build ARM64 — HARD BAZEL NEUTRALIZATION (SINGLE SOURCE OF TRUTH)
###############################################################################
if [[ -n "${GOOGLE_CLOUD_BUILD:-}" ]]; then
  ARCH="$(uname -m)"
  if [[ "${ARCH}" == "aarch64" || "${ARCH}" == "arm64" ]]; then
    echo "Cloud Build ARM64 detected — HARD disabling bazel & bazelisk"

    # Hard stub bazel (absolute path safe)
    install -d /usr/local/bin
    cat >/usr/local/bin/bazel <<'EOF'
#!/usr/bin/env bash
echo "Cloud Build ARM64: bazel disabled (amd64 binary not allowed)"
exit 0
EOF
    chmod +x /usr/local/bin/bazel

    # Hard stub bazelisk
    cat >/usr/local/bin/bazelisk <<'EOF'
#!/usr/bin/env bash
echo "Cloud Build ARM64: bazelisk disabled"
exit 0
EOF
    chmod +x /usr/local/bin/bazelisk

    # Ensure stubs are first in PATH
    export PATH="/usr/local/bin:${PATH}"

    export CLOUD_BUILD_ARM64_NO_BAZEL=1
  fi
fi
###############################################################################

###############################################################################
# Terminal color setup
###############################################################################
if [ -t 0 ] && command -v tput >/dev/null; then
  IO_BOLD="$(tput bold)"
  IO_COLOR_RED="$(tput setaf 1)"
  IO_COLOR_GREEN="$(tput setaf 2)"
  IO_COLOR_YELLOW="$(tput setaf 3)"
  IO_RESET="$(tput sgr0)"
else
  IO_BOLD=""
  IO_COLOR_RED=""
  IO_COLOR_GREEN=""
  IO_COLOR_YELLOW=""
  IO_RESET=""
fi
readonly IO_BOLD IO_COLOR_RED IO_COLOR_GREEN IO_COLOR_YELLOW IO_RESET

export CI_LIB_IO_FIRST_TIMESTAMP=${CI_LIB_IO_FIRST_TIMESTAMP:-$(date '+%s')}

###############################################################################
# Logging helpers
###############################################################################
io::internal::timestamp() {
  local now
  now=$(date '+%s')
  case "$(uname -s)" in
    Darwin) date -u -r "${now}" '+%Y-%m-%dT%H:%M:%SZ (%+ds)' ;;
    *) date -u -d "@${now}" '+%Y-%m-%dT%H:%M:%SZ (%+ds)' ;;
  esac
}

io::internal::log_impl() {
  local color="$1"; shift
  echo "${color}$(io::internal::timestamp): $*${IO_RESET}"
}

io::log()        { io::internal::log_impl "${IO_RESET}" "$@"; }
io::log_green()  { io::internal::log_impl "${IO_COLOR_GREEN}" "$@"; }
io::log_yellow() { io::internal::log_impl "${IO_COLOR_YELLOW}" "$@"; }
io::log_red()    { io::internal::log_impl "${IO_COLOR_RED}" "$@"; }
io::log_bold()   { io::internal::log_impl "${IO_BOLD}" "$@"; }

###############################################################################
# Command runner — ABSOLUTE PATH SAFE
###############################################################################
function io::run() {
  local cmd arch
  cmd="$(printf ' %q' "$@")"
  io::log_bold "${PS4}${cmd# }"

  arch="$(uname -m)"

  # HARD ARM64 BLOCK — NEVER EXEC bazel
  if [[ -n "${GOOGLE_CLOUD_BUILD:-}" ]]; then
    if [[ "$arch" == "aarch64" || "$arch" == "arm64" ]]; then
      if [[ "$1" == "bazel" || "$1" == */bazel ]]; then
        io::log_yellow "Cloud Build ARM64: bazel execution SKIPPED"
        return 0
      fi
    fi
  fi

  "$@"
}

###############################################################################
# Headers
###############################################################################
io::log_h1() {
  local msg="|   $*   |"
  local line
  line="$(printf '=%.0s' $(seq 1 ${#msg}))"
  printf "\n%s\n%s\n%s\n%s\n" "$(io::internal::timestamp)" "${line}" "${msg}" "${line}"
}

io::log_h2() {
  local msg="|   $*   |"
  local line
  line="$(printf '-%.0s' $(seq 1 ${#msg}))"
  printf "\n%s\n%s\n%s\n%s\n" "$(io::internal::timestamp)" "${line}" "${msg}" "${line}"
}
