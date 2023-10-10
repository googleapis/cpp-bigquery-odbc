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

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get --no-install-recommends install -y \
        automake \
        build-essential \
        clang \
        cmake \
        curl \
        gawk \
        git \
        gcc \
        g++ \
        libc++-dev \
        libc++abi-dev \
        libc-ares-dev \
        libcurl4-openssl-dev \
        libgrpc++1 \
        libre2-dev \
        libssl-dev \
        libtool \
        llvm \
        lsb-release \
        make \
        ninja-build \
        patch \
        pkg-config \
        tar \
        unzip \
        zip \
        wget \
        zlib1g-dev \
        apt-utils \
        ca-certificates \
        apt-transport-https


# Install instructions from:
#     https://github.com/google/sanitizers/wiki/MemorySanitizerLibcxxHowTo
# with updates from:
#     https://github.com/google/sanitizers/issues/1685
WORKDIR /var/tmp/build
RUN git clone --depth=1 --branch llvmorg-16.0.6 https://github.com/llvm/llvm-project
WORKDIR /var/tmp/build/llvm-project
# configure cmake
RUN cmake -GNinja -S runtimes -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_PROJECTS="clang" \
    -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind" \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DLLVM_USE_SANITIZER=MemoryWithOrigins \
    -DCMAKE_INSTALL_PREFIX=/usr
# build the libraries
RUN cmake --build build
RUN cmake --install build

# Setup vcpkg and install dependencies of google-cloud-cpp using it in manifest mode
COPY . /var/tmp/ci
WORKDIR /var/tmp/
RUN chmod +x ci/cloudbuild/builds/lib/vcpkg.sh
RUN ci/cloudbuild/builds/lib/vcpkg.sh


# Install sccache from https://github.com/mozilla/sccache
WORKDIR /var/tmp/sccache
RUN curl -fsSL https://github.com/mozilla/sccache/releases/download/v0.5.4/sccache-v0.5.4-x86_64-unknown-linux-musl.tar.gz | \
    tar -zxf - --strip-components=1 && \
    mkdir -p /usr/local/bin && \
    mv sccache /usr/local/bin/sccache && \
    chmod +x /usr/local/bin/sccache

# Install google-cloud-cpp to get bigquery rest client
WORKDIR /var/tmp/google-cloud-cpp
RUN curl -fsSL https://github.com/googleapis/google-cloud-cpp/archive/90ad988fa439de20b79774b1ee737a1dcb15f9c8.tar.gz | \
    tar -zxf - --strip-components=1 && \
    cmake \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DGOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND=ON \
        -DBUILD_TESTING=OFF \
        -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF \
        -DGOOGLE_CLOUD_CPP_ENABLE=experimental-bigquery_rest \
        -S . -B cmake-out -GNinja && \
    cmake --build cmake-out -- -j $(nproc) && \
    cmake --build cmake-out --target install

RUN curl -o /usr/bin/bazelisk -sSL "https://github.com/bazelbuild/bazelisk/releases/download/v1.18.0/bazelisk-linux-amd64" && \
    chmod +x /usr/bin/bazelisk && \
    ln -s /usr/bin/bazelisk /usr/bin/bazel
