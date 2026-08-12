# Copyright 2026 Google LLC
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

FROM ubuntu:18.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get --no-install-recommends install -y \
        software-properties-common gnupg2 && \
    add-apt-repository ppa:ubuntu-toolchain-r/test -y && \
    apt-get update && \
    apt-get --no-install-recommends install -y \
        automake \
        autotools-dev \
        build-essential \
        # Dependency for arrow
        bison \
        cmake \
        curl \
        # Dependency for arrow
        flex \
        gawk \
        git \
        gcc \
        g++ \
        gcc-11 \
        g++-11 \
        libcurl4-openssl-dev \
        # Needed to use autoreconf
        libltdl-dev \
        libssl-dev \
        libtool \
        llvm \
        locales \
        lsb-release \
        make \
        ninja-build \
        patch \
        # Needed to use autoreconf
        perl \
        pkg-config \
        libffi-dev \
        tar \
        unzip \
        zip \
        wget \
        zlib1g-dev \
        apt-utils \
        ca-certificates \
        apt-transport-https

RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 100 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 100

ENV CC=gcc
ENV CXX=g++
RUN ln -s /usr/bin/make /usr/bin/gmake

# Install modern CMake locally
RUN mkdir -p /opt/cmake && \
    curl -fsSL https://github.com/Kitware/CMake/releases/download/v3.30.1/cmake-3.30.1-linux-x86_64.tar.gz \
    | tar -xz --strip-components=1 -C /opt/cmake

ENV PATH=/opt/cmake/bin:$PATH

RUN echo "ninja version: " && ninja --version       
RUN echo "g++ version: " && g++ --version
RUN echo "cmake version: " && cmake --version
RUN echo "Glibc version" && ldd --version

WORKDIR /usr/src
RUN wget https://www.python.org/ftp/python/3.10.14/Python-3.10.14.tgz && \
    tar -xzf Python-3.10.14.tgz && \
    cd Python-3.10.14 && \
    ./configure --with-ensurepip=install && \
    make -j$(nproc) \
    && make altinstall

# clang-tidy-cache needs python
RUN ln -sf /usr/local/bin/python3.10 /usr/bin/python3 && \
    ln -sf /usr/local/bin/python3.10 /usr/bin/python

# Install all the direct (and indirect) dependencies for cpp-bigquery-odbc.
# Use a different directory for each build, and remove the downloaded
# files and any temporary artifacts after a successful build to keep the
# image smaller (and with fewer layers)

WORKDIR /var/tmp/build/abseil-cpp
RUN curl -fsSL https://github.com/abseil/abseil-cpp/archive/20240722.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
       -DCMAKE_CXX_STANDARD=17 \
      -DABSL_BUILD_TESTING=OFF \
      -DABSL_PROPAGATE_CXX_STD=ON \
      -DBUILD_SHARED_LIBS=yes \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
      -S . -B cmake-out -GNinja && \
    cmake --build cmake-out --target install && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/googletest
RUN curl -fsSL https://github.com/google/googletest/archive/v1.15.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_SHARED_LIBS=yes \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
      -S . -B cmake-out -GNinja  && \
    cmake --build cmake-out --target install && \
    ldconfig && \
    cd /var/tmp && rm -fr build

# Install ctcache to speed up our clang-tidy build
WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/matus-chochlik/ctcache/archive/0ad2e227e8a981a9c1a6060ee6c8ec144bb976c6.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cp clang-tidy /usr/local/bin/clang-tidy-wrapper && \
    cp clang-tidy-cache /usr/local/bin/clang-tidy-cache && \
    cd /var/tmp && rm -fr build

# Install sccache from https://github.com/mozilla/sccache
WORKDIR /var/tmp/sccache
RUN curl -fsSL https://github.com/mozilla/sccache/releases/download/v0.5.4/sccache-v0.5.4-x86_64-unknown-linux-musl.tar.gz | \
    tar -zxf - --strip-components=1 && \
    mkdir -p /usr/local/bin && \
    mv sccache /usr/local/bin/sccache && \
    chmod +x /usr/local/bin/sccache

# Needed to use autoreconf
WORKDIR /var/tmp/m4
RUN curl -fsSL https://ftp.gnu.org/gnu/m4/m4-1.4.1.tar.gz | \
  tar -zxf - --strip-components=1 && \
  ./configure --enable-gui=no && \
  make && \
  make install -j "$(nproc)"

# Install the Cloud SDK
COPY ./dependencies/cloud-sdk.sh /var/tmp/ci/dependencies/cloud-sdk.sh
WORKDIR /var/tmp/downloads
RUN /var/tmp/ci/dependencies/cloud-sdk.sh
ENV CLOUD_SDK_LOCATION=/usr/local/google-cloud-sdk
ENV PATH=${CLOUD_SDK_LOCATION}/bin:${PATH}

## BEGIN Installs pre-requisites for the ODBC Driver.

COPY ./etc/vcpkg-version.txt /tmp/vcpkg-version.txt
COPY ./etc/roots.pem /opt/odbc-driver/roots.pem
COPY ./gha/builds/lib/odbc.ini /opt/odbc-driver/odbc.ini
COPY ./gha/builds/lib/odbcinst.ini /opt/odbc-driver/odbcinst.ini
COPY ./gha/builds/lib/lsan.supp /opt/odbc-driver/lsan.supp
COPY ./gha/builds/release/odbc.ini /opt/odbc-driver/odbc_template.ini
COPY ./gha/builds/release/odbcinst.ini /opt/odbc-driver/odbcinst_template.ini
COPY ./gha/builds/release/googlebigqueryodbc.ini /opt/odbc-driver/googlebigqueryodbc.ini
COPY ./etc/googlebigqueryodbc_utf16.ini /opt/odbc-driver/googlebigqueryodbc_utf16.ini
COPY ./etc/googlebigqueryodbc_utf8.ini /opt/odbc-driver/googlebigqueryodbc_utf8.ini
