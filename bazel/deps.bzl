# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Load dependencies needed to compile and test the google-cloud-cpp library."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def cpp_bigquery_odbc_development_deps(name = None):
    """Loads dependencies needed to develop the cpp-bigquery-odbc project.

    cpp-bigquery-odbc developers call this function from the top-level WORKSPACE
    file to obtain all the necessary *development* dependencies for
    cpp-bigquery-odbc. This includes testing dependencies and dependencies used
    by development tools.

    Args:
        name: Unused. It is conventional to provide a `name` argument to all
            workspace functions.
    """

    # googletest is only needed for tests.
    maybe(
        http_archive,
        name = "com_google_googletest",
        urls = [
            "https://github.com/google/googletest/archive/v1.13.0.tar.gz",
        ],
        sha256 = "ad7fdba11ea011c1d925b3289cf4af2c66a352e18d4c7264392fead75e919363",
        strip_prefix = "googletest-1.13.0",
    )

    # Load rules_cc, used by googletest
    maybe(
        http_archive,
        name = "rules_cc",
        urls = [
            "https://storage.googleapis.com/cloud-cpp-community-archive/rules_cc/rules_cc-0.0.1.tar.gz",
            "https://github.com/bazelbuild/rules_cc/releases/download/0.0.1/rules_cc-0.0.1.tar.gz",
        ],
        sha256 = "4dccbfd22c0def164c8f47458bd50e0c7148f3d92002cdb459c2a96a68498241",
    )

    # Load Abseil
    maybe(
        http_archive,
        name = "com_google_absl",
        urls = [
            "https://storage.googleapis.com/cloud-cpp-community-archive/com_google_absl/20230125.3.tar.gz",
            "https://github.com/abseil/abseil-cpp/archive/20230125.3.tar.gz",
        ],
        sha256 = "5366d7e7fa7ba0d915014d387b66d0d002c03236448e1ba9ef98122c13b35c36",
        strip_prefix = "abseil-cpp-20230125.3",
    )

def cpp_bigquery_odbc_deps(name = None):
    """Loads dependencies need to compile the cpp-bigquery-odbc libraries.

    Application developers can call this function from their WORKSPACE file
    to obtain all the necessary dependencies for cpp-bigquery-odbc, including
    gRPC and its dependencies. This function only loads dependencies that
    have not been previously loaded, allowing application developers to
    override the version of the dependencies they want to use.

    Args:
        name: Unused. It is conventional to provide a `name` argument to all
            workspace functions.
    """

    maybe(
        http_archive,
        name = "bazel_skylib",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.4.2/bazel-skylib-1.4.2.tar.gz",
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.4.2/bazel-skylib-1.4.2.tar.gz",
        ],
        sha256 = "66ffd9315665bfaafc96b52278f57c7e2dd09f5ede279ea6d39b2be471e7e3aa",
    )

    maybe(
        http_archive,
        name = "com_google_cloud_cpp",
        urls = [
            "https://github.com/googleapis/google-cloud-cpp/archive/refs/heads/main.zip",
        ],
        strip_prefix = "google-cloud-cpp-main",
    )
