# BigQuery ODBC Client Tool

This directory contains the files for the BigQuery ODBC client tool. It runs
inside a Docker container using Linux version (Ubuntu 18.04), contains the
driver manager (`unixodbc`), and compiles the client from source.

The files in this directory are:

1. **`main.cc`**: The C++ application that connects to the database via standard
   ODBC APIs and executes queries or lists catalogs/schemas/tables.
2. **`commons.h` / `commons.cc`**: Helper functions and handle classes for
   allocating handles, handling diagnostics errors, formatting and displaying
   result sets.
3. **`Dockerfile`**: Lightweight Dockerfile setting up the compilers,
   `unixodbc`/`unixodbc-dev` packages, and compiling the client binaries.
4. **`run.sh`**: The host-side runner script that dynamically parses environment
   variables `ODBCINI` and `GOOGLEBIGQUERYODBCINI`, resolves host paths for keys
   (`KeyFilePath`), CA certs (`TrustedCerts`) and logs (`LogPath`), and
   constructs the volume mounts (`-v`) for `docker run`.
5. **`odbc.ini`**: A sample ODBC configuration template.
6. **`googlebigqueryodbc.ini`**: A sample BigQuery ODBC driver global
   configuration template containing logging settings.

## How the Host Integration Works (Dynamic DSN Reloads)

This client tool is designed so that **you do not need to rebuild the Docker
image or restart the container when DSN parameters change**.

Whenever you run `./tools/client/run.sh`:

- **Targeted Configuration Mount**: The script determines the DSN being
  executed, extracts its specific section from the host `odbc.ini` file, and
  parses it. It **only** mounts the resources (such as `KeyFilePath` credentials
  or log paths) referenced inside that specific DSN block, avoiding blind
  mounting of unrelated keys or directories.
- **Dynamic DSN Resolution**: Any edits you make to the DSN parameters in your
  host `odbc.ini` are immediately visible to the driver inside the container on
  the next run. You do not need to rebuild the Docker image.
- **Driver and CA Certificate Mapping**: The driver `.so` (either from your host
  or dynamically extracted using `--driver_zip`) is mounted inside the
  container. To prevent SSL verification errors, `roots.pem`/`cacerts.pem` CA
  certificates are automatically mounted directly into the same directory as the
  resolved driver `.so` inside the container.

## Authentication with Application Default Credentials (ADC)

The client tool automatically supports authentication using Google Application
Default Credentials (ADC) inside the Docker container:

1. **Host Default Credentials:** If you have run
   `gcloud auth application-default login` on your host machine, the runner
   script automatically detects and mounts your default credentials file
   (`~/.config/gcloud/application_default_credentials.json`) to the correct path
   inside the container.
2. **Custom Credentials File:** If you set the `GOOGLE_APPLICATION_CREDENTIALS`
   environment variable on your host to point to a specific JSON credentials
   file, the runner script will mount that file into the container and configure
   `GOOGLE_APPLICATION_CREDENTIALS` inside the container to point to the mounted
   file.

To use ADC, configure your DSN in `odbc.ini` to use `OAuthMechanism=3`. You do
not need to specify `KeyFilePath` in the DSN if you rely on the host's default
credentials or the host environment variable.

## How to Run

1. Ensure `ODBCINI` and `GOOGLEBIGQUERYODBCINI` are set on your host machine to
   absolute paths:

   ```bash
   export ODBCINI="<path to odbc.ini>"
   export GOOGLEBIGQUERYODBCINI="<path to googlebigqueryodbc.ini>"
   ```

2. (Optional) Download a compatible release driver zip if your host driver is
   incompatible with the Ubuntu 18.04 container (due to GLIBC version mismatch):

   ```bash
   curl -L https://storage.googleapis.com/bq-driver-releases/odbc/ODBCDriverforBigQuery_linux_latest.zip -o /tmp/driver.zip
   ```

3. Run the client tool (you **must** provide either `--driver_zip` or
   `--driver_so`):

   - **Interactive Prompt Mode**:

     ```bash
     ./tools/client/run.sh --driver_zip /tmp/driver.zip
     # OR
     ./tools/client/run.sh --driver_so /path/to/libgoogle_cloud_odbc_bq_driver.so
     ```

     You will be prompted to enter the connection string (e.g.
     `DSN=ODBCTestsDSN`) and choose/configure actions.

   - **Command Line Mode**:

     ```bash
     # Execute a SQL query (specifying the path to the compatible driver zip or direct .so)
     ./tools/client/run.sh --driver_zip /tmp/driver.zip --conn_str "DSN=ODBCTestsDSN" --cmd query --query "SELECT 1"
     # OR
     ./tools/client/run.sh --driver_so /path/to/libgoogle_cloud_odbc_bq_driver.so --conn_str "DSN=ODBCTestsDSN" --cmd query --query "SELECT 1"

     # Run in performance test mode
     ./tools/client/run.sh --driver_zip /tmp/driver.zip --conn_str "DSN=ODBCTestsDSN" --cmd perf --query "SELECT state, gender, year, name, number FROM \`bigquery-public-data.usa_names.usa_1910_current\` LIMIT 100000"

     # List Projects (Catalogs)
     ./tools/client/run.sh --driver_zip /tmp/driver.zip --conn_str "DSN=ODBCTestsDSN" --cmd projects

     # List Datasets (Schemas) for the default DSN project
     ./tools/client/run.sh --driver_zip /tmp/driver.zip --conn_str "DSN=ODBCTestsDSN" --cmd datasets

     # List Tables with optional filters
     ./tools/client/run.sh --driver_zip /tmp/driver.zip --conn_str "DSN=ODBCTestsDSN" --cmd tables --schema "DATATYPERANGETEST" --table "%"
     ```
