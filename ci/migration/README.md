# Cross‑Platform ODBC DSN Migration Scripts (Windows / Linux / macOS)

This repository provides **cross‑platform migration scripts** to move existing
BigQuery ODBC DSNs from legacy **Simba drivers** to the new **ODBC Driver for
BigQuery**.

The scripts support:

- **Windows** (PowerShell)
- **Linux** (Bash)
- **macOS** (Bash)

They follow the **same high‑level behavior and migration semantics** across all
platforms, with platform‑specific implementation details.

______________________________________________________________________

## Goals of the Migration

- Seamlessly transition DSNs from Simba BigQuery drivers to the Google‑provided
  ODBC Driver for BigQuery
- Preserve existing DSN configuration wherever possible
- Support **safe testing (copy mode)** and **in‑place migration (replace mode)**
- Minimize manual registry / config file editing

______________________________________________________________________

## What the Script Does

At a high level, the script performs the following steps:

1. **Installs the new ODBC Driver for BigQuery** .
2. **Searches for existing DSNs** that use the legacy Simba BigQuery driver.
3. **Prompts the user to select a DSN** if multiple Simba-based DSNs are found.
4. **Migrates the selected DSN** based on the chosen migration mode (`copy` or
   `replace`).
5. **Updates registry entries for Windows** so the DSN points to the new Google
   BigQuery ODBC driver.
6. **Decrypts and migrates encrypted credentials** (if present).
7. **Tests the migrated DSN connection** at the end.

______________________________________________________________________

## Supported Drivers

| Platform      | Legacy Driver Detected                | Target Driver            |
| ------------- | ------------------------------------- | ------------------------ |
| Windows       | Simba ODBC Driver for Google BigQuery | ODBC Driver for BigQuery |
| Linux / macOS | Simba Google BigQuery ODBC Connector  | ODBC Driver for BigQuery |

______________________________________________________________________

## Migration Modes (All Platforms)

Both scripts support the same two explicit modes.

### 1. `copy` Mode (Recommended for first run)

**Behavior:**

- Original Simba DSN is **left unchanged**
- A **new DSN is created** using the Google driver
- New DSN name is derived automatically

| Original DSN | New DSN                        |
| ------------ | ------------------------------ |
| `MyBQDSN`    | `MyBQDSN_Google` (Windows)     |
| `MyBQDSN`    | `MyBQDSN_google` (Linux/macOS) |

**Use this when:**

- You want side‑by‑side validation
- You need a rollback path
- Applications may still depend on the old DSN

______________________________________________________________________

### 2. `replace` Mode (In‑place migration)

**Behavior:**

- DSN name remains **unchanged**
- Driver reference and configuration are updated **in place**
- Applications do not need configuration changes

**Use this when:**

- You are confident in the new driver
- DSN names must remain stable

______________________________________________________________________

## When the Scripts Prompt for a DSN

### Common Logic (All Platforms)

The scripts first discover **all DSNs that use a Simba BigQuery driver**.

There are three possible scenarios:

______________________________________________________________________

### Scenario A: No Simba DSNs Found

- Message is printed indicating no migration is needed
- Script exits safely without making changes

______________________________________________________________________

### Scenario B: Exactly One Simba DSN Found

- That DSN is **automatically selected**
- **No prompt** is shown
- Migration proceeds immediately

______________________________________________________________________

### Scenario C: Multiple Simba DSNs Found

- The script **lists all matching DSNs**
- The user is **prompted to select exactly one DSN** to migrate

```text
Multiple Simba DSNs found:
 [1] FinanceBQ
 [2] AnalyticsBQ

Enter the number of the DSN you want to migrate:
```

- Migration continues only after a valid selection
- Only the selected DSN is migrated.

This prevents accidental bulk or unintended migrations.

______________________________________________________________________

## Platform‑Specific Details

## Windows (PowerShell)

### Script

```
migrate.ps1
```

### Usage

```
migrate.ps1 -Mode copy    -DriverInstaller GoogleBigQueryODBC.msi
migrate.ps1 -Mode replace -DriverInstaller GoogleBigQueryODBC.msi
```

### DSN Discovery

- System DSNs: `HKLM\\SOFTWARE\\ODBC\\ODBC.INI`
- User DSNs: `HKCU\\SOFTWARE\\ODBC\\ODBC.INI`

### Windows‑Specific Behavior

- Installs the driver via **silent MSI** (`msiexec /quiet`)
- Copies DSN registry trees
- Updates `Driver` and `TrustedCerts` paths
- Decrypts `KeyFilePath_Enc` using **Windows DPAPI**
- Automatically tests the migrated DSN connection

> ⚠️ Run from an **elevated PowerShell prompt** when migrating system DSNs.

______________________________________________________________________

## Linux / macOS (Bash)

### Script

```
migrate.sh
```

### Usage

```
./migrate.sh copy    <installer-dir|zip|tar.gz>
./migrate.sh replace <installer-dir|zip|tar.gz>
```

### Installer Input

The installer argument may be:

- A directory
- `.zip`
- `.tar.gz` / `.tgz`

The script automatically extracts and locates the driver library.

### DSN Discovery

- Uses `odbc.ini`

- Resolution order:

  1. `$ODBCINI` environment variable (if set)
  2. `/etc/odbc.ini`

### Linux/macOS‑Specific Behavior

- Copies driver to:

  ```
  /usr/local/lib/google_odbc
  ```

- Creates a timestamped backup of `odbc.ini`

- Updates:

  - `[ODBC Data Sources]`
  - Individual DSN sections

- Uses `sudo` automatically when required

______________________________________________________________________

## Configuration Files Modified

| Platform    | Configuration                     |
| ----------- | --------------------------------- |
| Windows     | Registry (ODBC.INI, ODBCINST.INI) |
| Linux/macOS | `odbc.ini`                        |

All scripts create backups before modifying configuration.

______________________________________________________________________

## Safety & Best Practices

- **Start with `copy` mode** on all platforms
- Validate applications against the new DSN
- Keep backups until rollout is complete
- Use `replace` only when DSN names must remain unchanged

______________________________________________________________________

## Summary Table

| Feature             | Windows             | Linux/macOS                 |
| ------------------- | ------------------- | --------------------------- |
| Script              | PowerShell          | Bash                        |
| DSN store           | Registry            | odbc.ini                    |
| Installer           | MSI                 | dir / zip / tar.gz          |
| Copy mode           | `<DSN>_Google`      | `<DSN>_google`              |
| Replace mode        | In‑place            | In‑place                    |
| Multi‑DSN prompt    | Yes                 | Yes                         |
| Backup created      | Implicit (registry) | Explicit `.bak.<timestamp>` |
| Credential handling | DPAPI decrypt       | Plain‑text only             |

______________________________________________________________________

This unified behavior ensures a **predictable, safe migration experience across
Windows, Linux, and macOS**.
