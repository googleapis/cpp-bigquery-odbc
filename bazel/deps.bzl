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
            "https://storage.googleapis.com/cloud-cpp-community-archive/com_google_googletest/v1.14.0.tar.gz",
            "https://github.com/google/googletest/archive/v1.14.0.tar.gz",
        ],
        sha256 = "8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7",
        strip_prefix = "googletest-1.14.0",
    )

    # Load rules_cc, used by googletest
    maybe(
        http_archive,
        name = "rules_cc",
        urls = [
            "https://github.com/bazelbuild/rules_cc/releases/download/0.0.15/rules_cc-0.0.15.tar.gz",
        ],
        sha256 = "f4aadd8387f381033a9ad0500443a52a0cea5f8ad1ede4369d3c614eb7b2682e",
        strip_prefix = "rules_cc-0.0.15",
    )

    maybe(
        http_archive,
        name = "com_google_protobuf",
        urls = [
            "https://github.com/protocolbuffers/protobuf/archive/v29.3.tar.gz",
        ],
        sha256 = "008a11cc56f9b96679b4c285fd05f46d317d685be3ab524b2a310be0fbad987e",
        strip_prefix = "protobuf-29.3",
    )

    # Load Abseil : google-cloud-cpp updated the version
    maybe(
        http_archive,
        name = "com_google_absl",
        urls = [
            "https://storage.googleapis.com/cloud-cpp-community-archive/com_google_absl/20240116.2.tar.gz",
            "https://github.com/abseil/abseil-cpp/archive/20240116.2.tar.gz",
        ],
        sha256 = "733726b8c3a6d39a4120d7e45ea8b41a434cdacde401cba500f14236c49b39dc",
        strip_prefix = "abseil-cpp-20240116.2",
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
        name = "com_github_grpc_grpc",
        urls = [
            "https://github.com/grpc/grpc/archive/v1.69.0.tar.gz",
        ],
        sha256 = "cd256d91781911d46a57506978b3979bfee45d5086a1b6668a3ae19c5e77f8dc",
        strip_prefix = "grpc-1.69.0",
    )

    maybe(
        http_archive,
        name = "com_google_cloud_cpp",
        urls = [
            "https://github.com/googleapis/google-cloud-cpp/archive/c244812e7f7297b78c1d084ccbfbba2ed8aa2349.tar.gz",
        ],
        strip_prefix = "google-cloud-cpp-c244812e7f7297b78c1d084ccbfbba2ed8aa2349",
    )
