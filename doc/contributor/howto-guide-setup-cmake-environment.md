# How-to Guide: Set Up for CMake-based builds

This document describes how to set up your workstation to build the libraries
and tests using CMake. The intended audience is developers of the driver who
want to verify their changes will work with CMake and/or prefer to use CMake.

The document assumes you are using a Linux workstation running Ubuntu, changing
the instructions for other distributions or operating systems is left as an
exercise to the reader. PRs to improve this document are most welcome!

Unless explicitly stated, this document assumes you running these commands at
the top-level directory of the project, as in:

```shell
cd $HOME
git clone git@github.com:googleapis/cpp-bigquery-odbc.git
export CPP_BIGQUERY_ODBC_REPO_PATH=$HOME/cpp-bigquery-odbc
```

## Download and bootstrap `vcpkg`

[vcpkg](https://vcpkg.io) is a package manager for C++ that builds from source
and installs any binary artifacts in `$HOME`. The first order dependencies of
our cmake targets are handled by the corresponding `CMakeLists.txt` files, but
the dependencies of
[google-cloud-cpp](https://github.com/googleapis/google-cloud-cpp) have to be
installed using vcpkg

In these instructions, we will install `vcpkg` descriptions in `$HOME/vcpkg`,
you can change the `vcpkg` location, just remember to adapt these instructions
as you go along. Download the `vcpkg` package descriptions using `git`:

```shell
git -C $HOME clone https://github.com/microsoft/vcpkg
```

Run the bootstrap script to build vcpkg:

```shell
export VCPKG_ROOT=$HOME/vcpkg
./bootstrap-vcpkg.sh
```

## Installing dependencies of `google-cloud-cpp` with vcpkg

Now you can use `vcpkg` to install all the dependencies of `google-cloud-cpp` or
install `google-cloud-cpp` itself.

Install all dependencies using [vcpkg.json](../../vcpkg.json):

```shell
cd $CPP_BIGQUERY_ODBC_REPO_PATH
vcpkg install
```

We have [vcpkg.json](../../vcpkg.json) at project root, which makes it run in
[manifest mode](https://learn.microsoft.com/en-us/vcpkg/users/manifests). So if
you want to install these packages individually, you have to be at some other
directory:

```shell
cd $HOME
vcpkg install abseil
vcpkg install protobuf
vcpkg install grpc
vcpkg install benchmark
vcpkg install nlohmann-json
vcpkg install gtest
```

Optionally, you can install `google-cloud-cpp` from vcpkg, if you don't need the
latest changes from their main branch:

```shell
vcpkg install google-cloud-cpp
```

The first time you run this command it can take a significant time to download
and compile all the dependencies (Abseil, gRPC, Protobuf, etc). Note that vcpkg
caches binary artifacts (in `$HOME/.cache/vcpkg`) so a second build would be
much faster.

## Building the BQ Driver

```shell
cd $CPP_BIGQUERY_ODBC_REPO_PATH
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DODBC_UNIT_TESTING=ON -DODBC_INTEGRATION_TESTING=OFF -DBQ_DRIVER_INTEGRATION_TESTS=OFF -DCLIENT_LIBRARY_INTEGRATION_TESTING=OFF
cmake  --build build -j $(nproc)
```

**Windows Note:** When building on Windows, you must add the following flag to
the CMake configure command:

> `-DVCPKG_TARGET_TRIPLET=x64-windows-static`

After this the build libraries would be at the path `build/google/cloud/odbc/`

## Running the unit tests

The steps above just build the library target, which is not so useful for
verifying that your setup is correct. You may want to build and run an unit
tests.

```shell
cd $CPP_BIGQUERY_ODBC_REPO_PATH
export CPP_BIGQUERY_ODBC_DRIVER_TEST_DATA_PATH=$CPP_BIGQUERY_ODBC_REPO_PATH/google/cloud/odbc/bq_driver/internal/test_data/
cd build && ctest
```

## Running the integration  tests

If you have a GCP project with BigQuery API enabled, you should be able to run
the integration tests

Assuming you have already setup a DSN using a driver manager, you can run the
driver integration tests like this:

```shell
cd $CPP_BIGQUERY_ODBC_REPO_PATH
export CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT=<GCP Project ID>
export ODBCINI=<path to your odbc.ini>
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DODBC_UNIT_TESTING=OFF -DODBC_INTEGRATION_TESTING=ON -DBQ_DRIVER_INTEGRATION_TESTS=OFF -DCLIENT_LIBRARY_INTEGRATION_TESTING=OFF
cmake  --build build -j $(nproc)
cd build && ctest
```

**Windows Note:** When building on Windows, you must add the following flag to
the CMake configure command:

> `-DVCPKG_TARGET_TRIPLET=x64-windows-static`

You can also run the client library integration tests one by one. Here is an
example of running the `Get Dataset` test:

```shell
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DINTEGRATION_TESTING=ON
cmake  --build build -j $(nproc)
cd build/google/cloud/odbc
./integration_tests/client_library_integration_apis_get_dataset_test explicit-adcs
```

Depending on the arguments passed to the test, you would need to set some env
variables too:

```shell
export CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT=<GCP project ID>
export CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET=<BigQuery Dataset>
```

### Changing the compiler

If your workstation has multiple compilers (or multiple versions of a compiler)
installed, you can change the compiler using:

```shell
CXX=clang++ CC=clang \
    cmake -G Ninja -S . -B build-out/clang \
    -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake
```

`vcpkg` uses the compiler as part of its binary cache inputs, that is, changing
the compiler will require rebuilding the dependencies from source. The good news
is that `vcpkg` can hold multiple versions of a binary artifact in its cache.
