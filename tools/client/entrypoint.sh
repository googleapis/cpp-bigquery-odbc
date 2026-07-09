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

# Create symlinks inside the container for any host-specific Driver paths referenced in ODBCINI
if [[ -n "${ODBCINI:-}" && -f "$ODBCINI" ]]; then
  # Extract any values from Driver= lines in odbc.ini
  driver_paths=$(grep -oE '^[[:space:]]*Driver[[:space:]]*=[[:space:]]*[^#;]+' "$ODBCINI" | cut -d'=' -f2- | xargs -n1 2>/dev/null || true)

  for dp in $driver_paths; do
    if [[ "$dp" == /* ]]; then
      if [[ ! -f "$dp" ]]; then
        echo "Creating symlink for driver path: $dp -> /usr/local/lib/libgoogle_cloud_odbc_bq_driver.so"
        mkdir -p "$(dirname "$dp")"
        ln -sf /usr/local/lib/libgoogle_cloud_odbc_bq_driver.so "$dp"
      fi
    fi
  done
fi

# Create symlinks inside the container for any host-specific Driver paths referenced in ODBCINSTINI
if [[ -n "${ODBCINSTINI:-}" && -f "$ODBCINSTINI" ]]; then
  driver_paths=$(grep -oE '^[[:space:]]*Driver[[:space:]]*=[[:space:]]*[^#;]+' "$ODBCINSTINI" | cut -d'=' -f2- | xargs -n1 2>/dev/null || true)

  for dp in $driver_paths; do
    if [[ "$dp" == /* ]]; then
      if [[ ! -f "$dp" ]]; then
        echo "Creating symlink for driver path in ODBCINSTINI: $dp -> /usr/local/lib/libgoogle_cloud_odbc_bq_driver.so"
        mkdir -p "$(dirname "$dp")"
        ln -sf /usr/local/lib/libgoogle_cloud_odbc_bq_driver.so "$dp"
      fi
    fi
  done
fi

# Run the actual compiled odbc_client binary
exec /usr/local/bin/odbc_client "$@"
