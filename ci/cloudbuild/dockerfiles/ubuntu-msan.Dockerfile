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

# ENV for unixODBC driver manager
ENV GCS_BUCKET=bq-dev-tools-testing-drivers
RUN echo 'GCS_BUCKET='${GCS_BUCKET}
ARG odbc_secret
ENV ODBC_CONN_KEYS=${odbc_secret}
RUN echo 'ODBC_CONN_KEYS='${ODBC_CONN_KEYS}

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get --no-install-recommends install -y \
        automake \
        build-essential \
        # Dependency for arrow
        bison \
        clang \
        cmake \
        curl \
        # Dependency for arrow
        flex \
        gawk \
        git \
        gcc \
        g++ \
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
RUN git clone --depth=1 --branch llvmorg-17.0.3 https://github.com/llvm/llvm-project
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
