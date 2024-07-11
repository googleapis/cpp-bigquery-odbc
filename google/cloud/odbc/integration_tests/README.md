# Running all tests using Bazel

The command to run all tests looks like

```
bazel test //google/cloud/odbc/integration_tests:* --test_tag_filters=scheduled-integration-tests \
  --cache_test_results=no --test_output=all \
  --test_env CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT=<GCP project ID> \
  --test_env=CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET=<BQ Dataset Name> \
  --test_env=CPP_BIGQUERY_ODBC_TEST_TABLE_NAME=<BQ Table Name> \
  --test_env=CPP_BIGQUERY_ODBC_TEST_USER_ACCOUNT_AUTH_KEY=<SOME_PATH>/user_account_auth_keys.json \
  --test_env=CPP_BIGQUERY_ODBC_TEST_CLIENT_ID_AUTH_KEY=<SOME_PATH>/client_id_auth_keys.json \
  --test_env=CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY=<SOME_PATH>/service_account_auth_keys.json \
  --test_env=CPP_BIGQUERY_ODBC_TEST_WRONG_AUTH_KEY=<SOME_PATH>/wrong_account_auth_keys.json \
  --test_env=CPP_BIGQUERY_ODBC_TEST_NO_ACCESS_ACCOUNT_AUTH_KEY=<SOME_PATH>/no_access_account_auth_keys.json \
  --test_env=CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_ID=id \
  --test_env=CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_NAME=name \
  --test_env=CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_AGE=age
```

All environment variables can be found in
[`integration.sh`](../../../../ci/cloudbuild/builds/lib/integration.sh) script.
Also, there can be found commands to create a dataset with a table, as tests
rely on prepopulated GCP BigQuery.

Running some of the tests can be done by adding a filter flag, for example

```
--test_arg=--gtest_filter=ListDatasets.*
```

# Running all tests using CMake

First export all environment variables

```
export CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT=<GCP project ID> \
export CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET=<BQ Dataset name> \
export CPP_BIGQUERY_ODBC_TEST_TABLE_NAME=<BQ Table Name> \
export CPP_BIGQUERY_ODBC_TEST_CLIENT_ID_AUTH_KEY=<SOME_PATH>/client_id_auth_keys.json \
export CPP_BIGQUERY_ODBC_TEST_USER_ACCOUNT_AUTH_KEY=<SOME_PATH>/user_account_auth_keys.json \
export CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY=<SOME_PATH>/service_account_auth_keys.json \
export CPP_BIGQUERY_ODBC_TEST_WRONG_AUTH_KEY=<SOME_PATH>/wrong_account_auth_keys.json \
export CPP_BIGQUERY_ODBC_TEST_NO_ACCESS_ACCOUNT_AUTH_KEY=<SOME_PATH>/no_access_account_auth_keys.json \
export CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_ID=id \
export CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_NAME=name \
export CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_AGE=age
```

Building can be done using command

```
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DINTEGRATION_TESTING=ON
cmake --build build -j$(nproc)
```

Running all tests can be done using either

```
build/google/cloud/odbc/integration_tests/scheduled_integration_tests
```

or

```
cd build && ctest
```

Running some of the tests can be done by adding a filter flag, for example

```
-R ListAllDatasets.*
```

# Building examples using CMake

"Examples" are standalone executables which are supposed to serve an example of
a user journey. These are typically not run during our PR checks.

```
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DODBC_UNIT_TESTING=OFF -DODBC_INTEGRATION_TESTING=ON -DBQ_DRIVER_INTEGRATION_TESTS=ON -DODBC_DEMO_TESTING=OFF -DCLIENT_LIBRARY_INTEGRATION_TESTING=OFF -DODBC_EXAMPLES=ON
cmake --build build -j $(nproc)
```

BQ_DRIVER_INTEGRATION_TESTS can be set ON or OFF based on which driver should be
used to build this.

You will find the executables for the built examples in
`./build/google/cloud/odbc/integration_tests/example_*`
