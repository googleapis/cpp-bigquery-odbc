# How-to Guide: Build and Test Setup For Driver Manager - Linux Platform

This document describes how to set up your workstation to build a shared library
for the BQ ODBC Driver on a Linux platform, and to run existing integration
tests against that shared library.

## Assumptions and Prerequisites

1. The document assumes you are using a Linux workstation running Ubuntu.
   Theoretically any flavor of Linux should work assuming there are no specific
   CMAKE changes for different Linux distributions.

2. The instructions in this document are not applicable for MacOS and Windows
   environments. There will be separate documents for those environments.

3. The Driver Manager used is [iODBC DriverManager](https://iodbc.org/).

4. This document assumes [iODBC DriverManager](https://iodbc.org/) is already
   installed on the Linux workstation. If not then please install it from iODBC
   Driver Manager
   [downloads](https://iodbc.org/dataspace/doc/iodbc/wiki/iodbcWiki/Downloads#Packages%20for%20Linux%20Distributions)
   page.

5. This document assumes Simba Driver is installed for the Linux environment. If
   not then please install it first. You can refer to the following documents
   for Linux installations

   - [Simba ODBC Guide For Linux Installation](http://goto.google.com/simba-odbc-linux-connector).
   - [Magnitude Simba Configuration Guide](https://storage.googleapis.com/simba-bq-release/odbc/Simba%20Google%20BigQuery%20ODBC%20Connector%20Install%20and%20Configuration%20Guide-2.5.0.1001.pdf)

6. BQ ODBC Driver Development environment has been setup. If not then please
   refer to the
   [setups for CMAKE](https://github.com/googleapis/cpp-bigquery-odbc/blob/main/doc/contributor/howto-guide-setup-cmake-environment.md?plain=1)
   and
   [setup for Bazel](https://github.com/googleapis/cpp-bigquery-odbc/blob/main/doc/contributor/howto-guide-setup-bazel-environment.md)
   for setting up the development environments.

7. Integration tests are running successfully against the static BQ ODBC Driver
   Library.

8. All build and tests commands are run from $CPP_BIGQUERY_ODBC_REPO_PATH
   directory. This directory refers to $HOME/cpp-bigquery-odbc

## Building the BQ ODBC Driver shared library

Run the following command for building the shared library

```shell
cd $CPP_BIGQUERY_ODBC_REPO_PATH

cmake -B <build_output_dir> -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
-DCMAKE_POSITION_INDEPENDENT_CODE=ON \
-DBUILD_SHARED_LIBS=ON \
-DODBC_DEMO_TESTING=OFF \
-DODBC_UNIT_TESTING=OFF \
-DBQ_DRIVER_INTEGRATION_TESTS=ON \
-DODBC_INTEGRATION_TESTING=ON \
-DCLIENT_LIBRARY_INTEGRATION_TESTING=OFF

cmake  --build <build_output_dir> --clean-first
```

In the above command `build_output_dir` refers to the directory where build
output would be stored. For instancel if `build_output_dir` is `build-out/home`
then the above command becomes:

```shell
cd $CPP_BIGQUERY_ODBC_REPO_PATH

cmake -B build-out/home -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
-DCMAKE_POSITION_INDEPENDENT_CODE=ON \
-DBUILD_SHARED_LIBS=ON \
-DODBC_DEMO_TESTING=OFF \
-DODBC_UNIT_TESTING=OFF \
-DBQ_DRIVER_INTEGRATION_TESTS=ON \
-DODBC_INTEGRATION_TESTING=ON \
-DCLIENT_LIBRARY_INTEGRATION_TESTING=OFF

cmake  --build build-out/home --clean-first
```

For future reference in this document, we will refer to `build_output_dir` as
`build-out/home`

## Installing the BQ ODBC Driver shared library

The build command from the previous section will produce a
`libgoogle_cloud_odbc_bq_driver.so` file in the
`build-out/home/google/cloud/odbc/` directory.

Copy the above shared library file in the Simba Installation Directory as
follows

```shell
cd $CPP_BIGQUERY_ODBC_REPO_PATH

sudo cp build-out/home/google/cloud/odbc/libgoogle_cloud_odbc_bq_driver.so <SIMBA_INSTALL_DIR>/lib/libgoogle_cloud_odbc_bq_driver.so
```

Where \<SIMBA_INSTALL_DIR> refers to the directory where Simba Driver is
installed. For instance if you have installed Simba Driver in
`/opt/simba/googlebigqueryodbc/` directory the above command becomes

```shell
cd $CPP_BIGQUERY_ODBC_REPO_PATH

sudo cp build-out/home/google/cloud/odbc/libgoogle_cloud_odbc_bq_driver.so /opt/simba/googlebigqueryodbc/lib/libgoogle_cloud_odbc_bq_driver.so
```

## Running the BQ Driver integration tests for Unicode APIs

As per the ODBC spec, Driver Manager will consider the ODBC Driver as a unicode
driver if it exports the `SQLConnectW` API. Since the default BQ Driver will
have this API exported, you can run the integration tests against the BQ Driver
Unicode APIs and Driver Manager by running the test command below

```shell
cd $CPP_BIGQUERY_ODBC_REPO_PATH

env -C build-out/home ctest --output-on-failure -LE integration-test 
```

For more verbosity, you can run:

```shell
cd $CPP_BIGQUERY_ODBC_REPO_PATH

env -C build-out/home ctest --output-on-failure --verbose -LE integration-test 
```

Please note that there are currently issues with the Unicode APIs that are being
worked upon. Please refer to the
[Driver Manager testing results document for Unicode APIs](https://docs.google.com/document/d/1pyQhpN-8CHS4fuQqoF8cvqCfN9eNxYjJuKX0g2q8-P4/edit?resourcekey=0-lkyvSNAaMTp5adOUFIdBYA&tab=t.0#bookmark=id.3gv63bbi4n3u),
for more details.

## Running the BQ Driver integration tests for ANSI APIs

In order to run the BQ ODBC Driver against the ANSI APIs do the following:

1. Comment out the SQLConnectW API in odbc_api.cc and build the driver as
   follows:

```shell
cd $CPP_BIGQUERY_ODBC_REPO_PATH

cmake -B build-out/home -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
-DCMAKE_POSITION_INDEPENDENT_CODE=ON \
-DBUILD_SHARED_LIBS=ON \
-DODBC_DEMO_TESTING=OFF \
-DODBC_UNIT_TESTING=OFF \
-DBQ_DRIVER_INTEGRATION_TESTS=ON \
-DODBC_INTEGRATION_TESTING=ON \
-DCLIENT_LIBRARY_INTEGRATION_TESTING=OFF

cmake  --build build-out/home
```

2. Copy over the shared object from the above build command to the Simba
   installation directory

```shell
cd $CPP_BIGQUERY_ODBC_REPO_PATH

sudo cp build-out/home/google/cloud/odbc/libgoogle_cloud_odbc_bq_driver.so <SIMBA_INSTALL_DIR>/lib/libgoogle_cloud_odbc_bq_driver.so
```

3. Run the integration tests

```shell
cd $CPP_BIGQUERY_ODBC_REPO_PATH

env -C build-out/home ctest --output-on-failure -LE integration-test 
```

All integration tests should work against the ANSI APIs. For more details, you
can also refer to the
[Driver Manager testing results document for ANSI APIs](https://docs.google.com/document/d/1pyQhpN-8CHS4fuQqoF8cvqCfN9eNxYjJuKX0g2q8-P4/edit?resourcekey=0-lkyvSNAaMTp5adOUFIdBYA&tab=t.0#bookmark=id.w806vmvqoghh)

## Open Issues

Intermittenly when executing the build or running the integration tests you can
get a build or test failure like th following:

`subprocess aborted` or `timeout expired` for `all_tests`

This is mainly because of the size of the tests being run with the shared lib
and maybe a short-coming of GTest. At this point, other than limiting the tests
files for testing either via filters or by manually updating `CmakeLists.txt`
file to a subset of tests, a better solution is still pending.
