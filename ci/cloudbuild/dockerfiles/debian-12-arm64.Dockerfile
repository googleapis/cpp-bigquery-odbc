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

FROM debian:12

ENV DEBIAN_FRONTEND=noninteractive
ENV LANG=en_US.UTF-8
ENV LC_ALL=en_US.UTF-8

# ------------------------------------------------------------
# Base build dependencies
# ------------------------------------------------------------
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        gcc \
        g++ \
        clang \
        llvm \
        cmake \
        ninja-build \
        make \
        automake \
        autotools-dev \
        libtool \
        pkg-config \
        bison \
        flex \
        gawk \
        git \
        curl \
        wget \
        unzip \
        zip \
        tar \
        patch \
        perl \
        python3 \
        python3-dev \
        python3-pip \
        libssl-dev \
        libcurl4-openssl-dev \
        libltdl-dev \
        zlib1g-dev \
        libc++-dev \
        libc++abi-dev \
        locales \
        ca-certificates \
        apt-utils \
        lsb-release \
        clang-tidy \
        libc6 && \
    rm -rf /var/lib/apt/lists/*

# ------------------------------------------------------------
# Locale
# ------------------------------------------------------------
RUN locale-gen en_US.UTF-8

# Python alias
RUN update-alternatives --install /usr/bin/python python /usr/bin/python3 10

# ------------------------------------------------------------
# Python dependencies
# ------------------------------------------------------------
COPY ./requirements.txt /tmp/requirements.txt
RUN pip3 install --no-cache-dir --require-hashes --no-deps -r /tmp/requirements.txt

# ------------------------------------------------------------
# Abseil
# ------------------------------------------------------------
WORKDIR /tmp/build/abseil
RUN curl -fsSL https://github.com/abseil/abseil-cpp/archive/20230802.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake -S . -B build -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DABSL_BUILD_TESTING=OFF \
      -DABSL_PROPAGATE_CXX_STD=ON \
      -DBUILD_SHARED_LIBS=ON \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON && \
    cmake --build build --target install && \
    ldconfig && \
    rm -rf /tmp/build

# ------------------------------------------------------------
# GoogleTest
# ------------------------------------------------------------
WORKDIR /tmp/build/googletest
RUN curl -fsSL https://github.com/google/googletest/archive/v1.13.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake -S . -B build -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=ON \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON && \
    cmake --build build --target install && \
    ldconfig && \
    rm -rf /tmp/build

# ------------------------------------------------------------
# ctcache (clang-tidy cache)
# ------------------------------------------------------------
WORKDIR /tmp/ctcache
RUN curl -fsSL https://github.com/matus-chochlik/ctcache/archive/0ad2e227e8a981a9c1a6060ee6c8ec144bb976c6.tar.gz | \
    tar -xzf - --strip-components=1 && \
    install -m 755 clang-tidy /usr/local/bin/clang-tidy-wrapper && \
    install -m 755 clang-tidy-cache /usr/local/bin/clang-tidy-cache && \
    rm -rf /tmp/ctcache

# ------------------------------------------------------------
# sccache (ARM64 binary)
# ------------------------------------------------------------
RUN curl -fsSL \
    https://github.com/mozilla/sccache/releases/download/v0.5.4/sccache-v0.5.4-aarch64-unknown-linux-musl.tar.gz | \
    tar -xzf - --strip-components=1 && \
    install -m 755 sccache /usr/local/bin/sccache

# ------------------------------------------------------------
# m4 (needed by autotools)
# ------------------------------------------------------------
WORKDIR /tmp/m4
RUN curl -fsSL https://ftp.gnu.org/gnu/m4/m4-1.4.19.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./configure && \
    make -j$(nproc) && \
    make install && \
    rm -rf /tmp/m4

# ------------------------------------------------------------
# Modern CMake (ARM64 build)
# ------------------------------------------------------------
WORKDIR /tmp/cmake
RUN curl -fsSL https://github.com/Kitware/CMake/releases/download/v3.31.10/cmake-3.31.10.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./bootstrap --parallel=$(nproc) && \
    make -j$(nproc) && \
    make install && \
    rm -rf /tmp/cmake

# ------------------------------------------------------------
# vcpkg
# ------------------------------------------------------------
ENV VCPKG_ROOT=/opt/vcpkg
RUN git clone https://github.com/microsoft/vcpkg ${VCPKG_ROOT} && \
    ${VCPKG_ROOT}/bootstrap-vcpkg.sh -disableMetrics

# ------------------------------------------------------------
# Google Cloud SDK
# ------------------------------------------------------------
COPY ./dependencies/cloud-sdk.sh /tmp/cloud-sdk.sh
RUN bash /tmp/cloud-sdk.sh

ENV CLOUD_SDK_LOCATION=/usr/local/google-cloud-sdk
ENV PATH=${CLOUD_SDK_LOCATION}/bin:${PATH}

# ------------------------------------------------------------
# ODBC config files
# ------------------------------------------------------------
RUN mkdir -p /opt/odbc-driver
COPY ./gha/builds/lib/odbc.ini /opt/odbc-driver/odbc.ini
COPY ./gha/builds/lib/odbcinst.ini /opt/odbc-driver/odbcinst.ini
COPY ./gha/builds/lib/lsan.supp /opt/odbc-driver/lsan.supp
COPY ./gha/builds/lib/google.googlebigqueryodbc.ini /opt/odbc-driver/google.googlebigqueryodbc.ini
COPY ./gha/builds/release/odbc.ini /opt/odbc-driver/odbc_template.ini
COPY ./gha/builds/release/odbcinst.ini /opt/odbc-driver/odbcinst_template.ini
COPY ./gha/builds/release/googlebigqueryodbc.ini /opt/odbc-driver/googlebigqueryodbc.ini

# ------------------------------------------------------------
# Verify glibc (ARM64)
# ------------------------------------------------------------
RUN ldd --version | grep GLIBC

WORKDIR /workspace
