# Job labels

Use `JobLabels` in a DSN or ODBC connection string to attach labels to jobs
submitted by the driver. The option is empty by default.

```ini
[SampleDSNGoogleDriver]
# Keep your existing driver and authentication settings.
JobLabels=team=analytics,tool=tableau,cost_center=finance
```

```text
DSN=SampleDSNGoogleDriver;JobLabels=team=analytics,tool=power-bi;
```

On Windows, open **Advanced Options** in the DSN configuration dialog and enter
the same comma-separated list in **Job labels**. Do not include `JobLabels=` in
the edit box. The setting is saved with the DSN.

## Syntax and precedence

- Separate pairs with commas, and keys from values with an equals sign.
- Whitespace around pairs, keys and values is ignored.
- Keys are unique, nonempty, and at most 63 Unicode characters long. Keys begin
  with a lowercase or uncased letter.
- Keys and values accept lowercase or uncased letters, numbers, underscores and
  dashes. Values may be empty and are at most 63 characters long.
- Up to 64 labels are allowed. Invalid lists fail connection setup with SQLSTATE
  `HY024`; labels are never silently dropped or lowercased.
- A connection-string `JobLabels` value replaces the entire DSN list. An
  explicit `JobLabels=;` clears the DSN labels for that connection.
- The option name is case-insensitive. Label names and values are preserved.
- Braces around the complete value are supported in connection strings:
  `JobLabels={team=analytics,flag=};`. Use literal `=`, not JDBC URL encoding.

The same connection labels apply at the common submission boundary for
`jobs.query` and `jobs.insert`, including direct queries, prepared statements,
large-result jobs, session queries, and SQL used for metadata. They are sent as
`QueryRequest.labels` or `Job.configuration.labels`. Labels are not added as SQL
comments or to result tables or Storage Read API sessions. Internally supplied
request labels take precedence on duplicate keys; the combined map must still
contain no more than 64 labels.

Job labels do not change `JobCreationMode`. Set `JobCreationMode=1` if you need
BigQuery to create a job for each executed query. Dry-run validation does not
create a billable execution job.

## Verify in BigQuery

Execute a query using the configured driver connection, then inspect its job in
BigQuery or with `bq show -j --format=prettyjson PROJECT:LOCATION.JOB_ID`. The
submitted labels should be visible in `configuration.labels`.

Before release, verify direct and prepared queries with HTAPI on and off,
connection-string overrides (including clearing the list), session queries, and
Windows save/reopen with a label list longer than 1024 characters.

## Build and test

The default build fetches the pinned Google Cloud C++ client and applies the
repository patches. One patch preserves label keys such as `status` and
`location` while removing output-only fields from job JSON. An unpatched
preinstalled client can silently discard those labels.

`BQ_DRIVER_USE_INSTALLED_GOOGLE_CLOUD_CPP=ON` opts into using an installed
client instead. Use this only if that client includes the label-filter fix;
`JobLabels.InsertJobPreservesLabelsInSerializedPayload` verifies it.

Use the normal
[CMake build setup](contributor/howto-guide-setup-cmake-environment.md) with
`ODBC_UNIT_TESTING=ON` and a fresh build directory (`build-job-labels`) so the
updated dependency patch runs, then run:

```sh
ctest --test-dir build-job-labels --output-on-failure -R 'ParseJobLabels|JobLabels|InvalidJobLabels|DsnJobLabels'
```

The Windows form test is available only in a Windows build.

References:

- [BigQuery label requirements](https://cloud.google.com/bigquery/docs/labels-intro#requirements_for_labels)
- [jobs.query labels](https://cloud.google.com/bigquery/docs/reference/rest/v2/jobs/query#QueryRequest)
- [Job configuration labels](https://cloud.google.com/bigquery/docs/reference/rest/v2/Job#JobConfiguration)
