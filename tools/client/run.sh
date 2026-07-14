#!/bin/bash
# Copyright 2026 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -euo pipefail

# Get the directory of this script (repository root / tools / client)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

IMAGE_NAME="bq-odbc-client"

# 1. Build the Docker image (Docker cache will make this instantaneous if no changes)
echo "Building docker image '$IMAGE_NAME'..."
docker build -t "$IMAGE_NAME" -f "$SCRIPT_DIR/Dockerfile" "$REPO_ROOT"

# 2. Parse arguments to separate run.sh arguments from client arguments
DRIVER_ZIP=""
DRIVER_SO=""
CONN_STR=""
docker_args=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --driver_zip)
      DRIVER_ZIP="$2"
      shift 2
      ;;
    --driver_so)
      DRIVER_SO="$2"
      shift 2
      ;;
    --conn_str)
      CONN_STR="$2"
      docker_args+=("$1" "$2")
      shift 2
      ;;
    *)
      docker_args+=("$1")
      shift
      ;;
  esac
done

# Validate driver arguments
if [[ -z "${DRIVER_ZIP:-}" && -z "${DRIVER_SO:-}" ]]; then
  echo "Error: Either --driver_zip or --driver_so must be provided." >&2
  exit 1
fi

if [[ -n "${DRIVER_ZIP:-}" && -n "${DRIVER_SO:-}" ]]; then
  echo "Error: Cannot provide both --driver_zip and --driver_so." >&2
  exit 1
fi

# Prompt for connection string on host if not provided
if [[ -z "${CONN_STR:-}" ]]; then
  if [[ -t 0 ]]; then
    read -r -p "Enter Connection String or DSN (e.g. DSN=ODBCTestsDSN): " CONN_STR
    docker_args+=("--conn_str" "$CONN_STR")
  else
    echo "Error: --conn_str is required when running non-interactively." >&2
    exit 1
  fi
fi

# 3. Check environment variables
if [[ -z "${ODBCINI:-}" ]]; then
  echo "Error: ODBCINI environment variable is not set." >&2
  exit 1
fi

if [[ ! -f "$ODBCINI" ]]; then
  echo "Error: ODBCINI file '$ODBCINI' does not exist." >&2
  exit 1
fi

# Extract DSN name from connection string
DSN_NAME=""
if [[ "$CONN_STR" =~ DSN=([^;]+) ]]; then
  DSN_NAME="${BASH_REMATCH[1]}"
fi

# Load specific DSN block from ODBCINI if DSN_NAME is set, to avoid blind mounting
dsn_block=""
if [[ -n "${DSN_NAME:-}" ]]; then
  echo "Extracting configuration for DSN '$DSN_NAME'..."
  dsn_block=$(awk "/^[[:space:]]*\\[$DSN_NAME\\]/{flag=1;next}/^[[:space:]]*\\[/{flag=0}flag" "$ODBCINI" || true)
fi

if [[ -z "$dsn_block" ]]; then
  echo "Warning: DSN '$DSN_NAME' not found in $ODBCINI, or no DSN specified. Using entire file context."
  dsn_block=$(cat "$ODBCINI")
fi

# Initialize docker volume mount arguments
docker_mounts=()

# Helper to add mounts if a path exists
add_mount_path() {
  local host_path="$1"
  local create_if_missing="${2:-false}"
  # Trim spaces
  host_path=$(echo "$host_path" | xargs)

  if [[ -n "$host_path" ]]; then
    # Expand ~
    local expanded_path="$host_path"
    if [[ "$host_path" == ~* ]]; then
      expanded_path="${host_path/#\~/$HOME}"
    fi
    expanded_path=$(realpath -ms "$expanded_path")

    # Resolve symlinks if path exists
    local resolved_path
    resolved_path=$(realpath "$expanded_path" 2>/dev/null || echo "$expanded_path")

    if [[ -e "$resolved_path" ]]; then
      echo "Mapping mount: $resolved_path -> $expanded_path"
      docker_mounts+=("-v" "$resolved_path:$expanded_path")
    elif [[ "$create_if_missing" == "true" ]]; then
      # If the path does not exist yet and we explicitly want to create it
      echo "Creating parent dir for mount: $(dirname "$expanded_path")"
      if mkdir -p "$(dirname "$expanded_path")" 2>/dev/null; then
        docker_mounts+=("-v" "$expanded_path:$expanded_path")
      else
        echo "Warning: Failed to create host directory $(dirname "$expanded_path") for mounting. Skipping mount."
      fi
    else
      echo "Warning: Host path '$expanded_path' does not exist. Skipping mount."
    fi
  fi
}

# Mount ODBCINI itself
add_mount_path "$ODBCINI"

# Parse KeyFilePath paths from DSN block and add to mounts
key_file_paths=$(echo "$dsn_block" | grep -oE '^[[:space:]]*KeyFilePath[[:space:]]*=[[:space:]]*[^#;]+' | cut -d'=' -f2- | xargs -n1 2>/dev/null || true)
for path in $key_file_paths; do
  add_mount_path "$path"
done

# Parse TrustedCerts paths from DSN block and add to mounts
trusted_certs_paths=$(echo "$dsn_block" | grep -oE '^[[:space:]]*TrustedCerts[[:space:]]*=[[:space:]]*[^#;]+' | cut -d'=' -f2- | xargs -n1 2>/dev/null || true)
for path in $trusted_certs_paths; do
  add_mount_path "$path"
done

# Find host's odbcinst.ini
odbcinst_file=""
if [[ -n "${ODBCINSTINI:-}" && -f "$ODBCINSTINI" ]]; then
  odbcinst_file="$ODBCINSTINI"
  echo "Mapping ODBCINSTINI: $ODBCINSTINI -> /etc/odbcinst.ini"
  docker_mounts+=("-v" "$(realpath "$ODBCINSTINI"):/etc/odbcinst.ini")
elif [[ -f "/etc/odbcinst.ini" ]]; then
  odbcinst_file="/etc/odbcinst.ini"
  echo "Mapping default host /etc/odbcinst.ini -> /etc/odbcinst.ini"
  docker_mounts+=("-v" "/etc/odbcinst.ini:/etc/odbcinst.ini")
fi

# Parse Driver value from DSN block (could be an absolute path or a driver name)
dsn_driver=$(echo "$dsn_block" | grep -oE '^[[:space:]]*Driver[[:space:]]*=[[:space:]]*[^#;]+' | cut -d'=' -f2- | xargs 2>/dev/null || true)

driver_paths=()
if [[ -n "$dsn_driver" ]]; then
  if [[ "$dsn_driver" == /* ]]; then
    driver_paths+=("$dsn_driver")
  else
    # Look up the driver name in odbcinst.ini
    if [[ -n "$odbcinst_file" ]]; then
      echo "Looking up driver name '$dsn_driver' in $odbcinst_file..."
      drv_block=$(awk "/^[[:space:]]*\\[$dsn_driver\\]/{flag=1;next}/^[[:space:]]*\\[/{flag=0}flag" "$odbcinst_file" || true)
      drv_path=$(echo "$drv_block" | grep -oE '^[[:space:]]*Driver[[:space:]]*=[[:space:]]*[^#;]+' | cut -d'=' -f2- | xargs 2>/dev/null || true)
      if [[ -n "$drv_path" && "$drv_path" == /* ]]; then
        driver_paths+=("$drv_path")
      fi
    fi
  fi
fi

# Helper to mount CA certificates next to the driver library
mount_ca_certs_next_to_driver() {
  local target_so="$1"
  local dp_dir
  dp_dir=$(dirname "$target_so")
  if [[ "$dp_dir" != "/" ]]; then
    echo "Mounting CA certificates to driver folder: $dp_dir/"
    docker_mounts+=("-v" "$REPO_ROOT/ci/etc/roots.pem:$dp_dir/roots.pem")
    docker_mounts+=("-v" "$REPO_ROOT/ci/etc/roots.pem:$dp_dir/cacerts.pem")
  fi
}

# Handle driver mounting
so_file=""

if [[ -n "$DRIVER_ZIP" ]]; then
  if [[ ! -f "$DRIVER_ZIP" ]]; then
    echo "Error: Driver zip file '$DRIVER_ZIP' does not exist." >&2
    exit 1
  fi
  EXTRACT_DIR="$SCRIPT_DIR/extracted_driver"
  echo "Extracting driver zip to $EXTRACT_DIR..."
  mkdir -p "$EXTRACT_DIR"
  unzip -o "$DRIVER_ZIP" -d "$EXTRACT_DIR" >/dev/null

  # Find the first .so file inside the extracted directory
  so_file=$(find "$EXTRACT_DIR" -name "*.so" | head -n 1)
  if [[ -z "$so_file" ]]; then
    echo "Error: No .so file found in the extracted driver zip." >&2
    exit 1
  fi
  so_file=$(realpath "$so_file")
  echo "Found compatible driver library in zip: $so_file"

elif [[ -n "$DRIVER_SO" ]]; then
  if [[ ! -f "$DRIVER_SO" ]]; then
    echo "Error: Driver .so file '$DRIVER_SO' does not exist." >&2
    exit 1
  fi
  so_file=$(realpath "$DRIVER_SO")
  echo "Using driver library: $so_file"
fi

if [[ -n "$so_file" ]]; then
  # Mount this single .so file to all driver paths used
  for dp in "${driver_paths[@]}"; do
    echo "Overriding driver mount: $so_file -> $dp"
    docker_mounts+=("-v" "$so_file:$dp")
    mount_ca_certs_next_to_driver "$dp"
  done
  # Also mount it to the fallback default path
  docker_mounts+=("-v" "$so_file:/usr/local/lib/libgoogle_cloud_odbc_bq_driver.so")
  mount_ca_certs_next_to_driver "/usr/local/lib/libgoogle_cloud_odbc_bq_driver.so"
else
  # Default logic: mount whatever host driver is specified directly
  for path in "${driver_paths[@]}"; do
    add_mount_path "$path"
    mount_ca_certs_next_to_driver "$path"
  done
fi

# If GOOGLEBIGQUERYODBCINI is set and exists
if [[ -n "${GOOGLEBIGQUERYODBCINI:-}" ]]; then
  if [[ -f "$GOOGLEBIGQUERYODBCINI" ]]; then
    add_mount_path "$GOOGLEBIGQUERYODBCINI"
    # Parse LogPath from GOOGLEBIGQUERYODBCINI and add to mounts
    log_paths=$(grep -oE '^[[:space:]]*LogPath[[:space:]]*=[[:space:]]*[^#;]+' "$GOOGLEBIGQUERYODBCINI" | cut -d'=' -f2- | xargs -n1 2>/dev/null || true)
    for path in $log_paths; do
      add_mount_path "$path" true
    done
  else
    echo "Warning: GOOGLEBIGQUERYODBCINI is set but the file '$GOOGLEBIGQUERYODBCINI' does not exist."
  fi
fi

# Run the docker container
echo "Running ODBC client in Docker..."
docker run --rm -it \
  "${docker_mounts[@]}" \
  -e ODBCINI="$(realpath "$ODBCINI")" \
  ${GOOGLEBIGQUERYODBCINI:+-e GOOGLEBIGQUERYODBCINI="$(realpath "$GOOGLEBIGQUERYODBCINI")"} \
  "$IMAGE_NAME" "${docker_args[@]}"
