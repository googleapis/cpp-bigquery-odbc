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

    # Load a version of googletest that we know works. This is needed to create
    # //:.*mocks targets, which are public.
    maybe(
        http_archive,
        name = "com_google_googletest",
        urls = [
            "https://github.com/google/googletest/archive/v1.15.2.tar.gz",
        ],
        sha256 = "7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926",
        strip_prefix = "googletest-1.15.2",
    )

    # Load rules_cc, used by googletest
    maybe(
        http_archive,
        name = "rules_cc",
        urls = [
            "https://storage.googleapis.com/cloud-cpp-community-archive/rules_cc/rules_cc-0.0.15.tar.gz",
            "https://github.com/bazelbuild/rules_cc/releases/download/0.0.15/rules_cc-0.0.15.tar.gz",
        ],
        sha256 = "f4aadd8387f381033a9ad0500443a52a0cea5f8ad1ede4369d3c614eb7b2682e",
        strip_prefix = "rules_cc-0.0.15",
    )

    # Load Abseil
    maybe(
        http_archive,
        name = "com_google_absl",
        urls = [
            "https://github.com/abseil/abseil-cpp/archive/20240722.0.tar.gz",
        ],
        sha256 = "f50e5ac311a81382da7fa75b97310e4b9006474f9560ac46f54a9967f07d4ae3",
        strip_prefix = "abseil-cpp-20240722.0",
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
        sha256 = "bc283cdfcd526a52c3201279cda4bc298652efa898b10b4db0837dc51652756f",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.7.1/bazel-skylib-1.7.1.tar.gz",
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.7.1/bazel-skylib-1.7.1.tar.gz",
        ],
    )

    maybe(
        http_archive,
        name = "com_google_cloud_cpp",
        urls = [
            "https://github.com/googleapis/google-cloud-cpp/archive/a8c2e9d1c2828f20cbd81dd5336f630cdaef16f0.tar.gz",
        ],
        strip_prefix = "google-cloud-cpp-a8c2e9d1c2828f20cbd81dd5336f630cdaef16f0",
    )
