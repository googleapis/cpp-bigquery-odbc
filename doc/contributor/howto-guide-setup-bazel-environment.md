# How-to Guide: Set Up for Bazel-based builds

This document describes how to set up your workstation to build the libraries
and tests using Bazel. The intended audience is developers of the driver who
want to verify their changes will work with bazel.

The document assumes you are using a Linux workstation running Ubuntu, changing
the instructions for other distributions or operating systems is left as an
exercise to the reader. PRs to improve this document are most welcome!

Unless explicitly stated, this document assumes you running these commands at
the top-level directory of the project, as in:

```shell
cd $HOME
git clone git@github.com:googleapis/cpp-bigquery-odbc.git
```

## Building the Client interface library

```shell
cd $HOME/cpp-bigquery-odbc
bazel build --test_output=all odbc_client_interface
```

## Running the tests

The steps above just build the library target, which is not so useful for
verifying that your setup is correct. You may want to build and run an
example/test.

Bazel runs builds in a sandbox, so it cannot automatically have access to the
env variables. We will have to pass the through `--test_env` for `bazel test` or
`bazel run`.

First, set the env:

```shell
export CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT=<GCP project ID>
export CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET=<BigQuery Dataset>
```

You can run the client library integration tests one by one. Here is an example
of running the `Get Dataset` test:

```shell
bazel test --test_output=all //google/cloud/odbc/integration_tests:apis_get_dataset_test --test_arg=with-service-account --test_arg=<path to your service account key json file> --test_env=CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT=$CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT --test_env=CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET=$CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET
```

(TODO: [#61](https://github.com/googleapis/cpp-bigquery-odbc/issues/61)) You can
also run all the tests in one go:

```shell
bazel test --test_output=all --test_tag_filters=integration-test //google/cloud/odbc/...:all --test_arg=with-service-account --test_arg=<path to your service account key json file> --test_env=CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT=$CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT --test_env=CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET=$CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET
```
