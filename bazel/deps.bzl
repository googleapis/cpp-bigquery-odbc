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
        name = "com_google_absl",
        urls = [
          "https://github.com/abseil/abseil-cpp/archive/20250512.2.tar.gz",
        ],
        sha256 = "71358f2e72e945d280bfab44090eacb3f98e10fead31fd97876f05a835510d92",
        strip_prefix = "abseil-cpp-20250512.2",
    )

    maybe(
        http_archive,
        name = "com_google_cloud_cpp",
        urls = [
            "https://github.com/googleapis/google-cloud-cpp/archive/refs/tags/v3.4.0.tar.gz",
        ],
        strip_prefix = "google-cloud-cpp-3.4.0",
    )
