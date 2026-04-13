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

FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get --no-install-recommends install -y \
        automake \
        build-essential \
        # Dependency for arrow
        bison \
        clang-12 \
        lld-12 \
        curl \
        # Dependency for arrow
        flex \
        gawk \
        git \
        gcc \
        g++ \
        libcurl4-openssl-dev \
        libssl-dev \
        libffi-dev \
        libtool \
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
        apt-transport-https \
        clang-tidy \
        libzstd-dev \
        liblz4-dev \
        libsnappy-dev \
        libbrotli-dev \
        libc++-12-dev \
        libc++abi-12-dev

# Set Clang 12 as default
RUN update-alternatives --install /usr/bin/clang clang /usr/bin/clang-12 100 && \
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-12 100

# Set the compiler environment variables
ENV CC=clang
ENV CXX=clang++
ENV CXXFLAGS="-stdlib=libc++ -std=c++17"
ENV LDFLAGS="-stdlib=libc++ -lc++abi"

# Build cmake from source to have the same version across all builds.
WORKDIR /var/tmp/build/cmake
RUN curl -fsSL https://github.com/Kitware/CMake/releases/download/v3.31.10/cmake-3.31.10.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./bootstrap && \
    make -j$(nproc) && \
    make install

# Download Python 3.10.13
RUN wget https://www.python.org/ftp/python/3.10.13/Python-3.10.13.tgz && \
    tar -xzf Python-3.10.13.tgz && \
    cd Python-3.10.13 && \
    ./configure && \
    make -j$(nproc) && \
    make altinstall

# Make python3 default
RUN ln -sf /usr/local/bin/python3.10 /usr/bin/python && \
    ln -sf /usr/local/bin/python3.10 /usr/bin/python3

COPY ./etc/vcpkg-version.txt /tmp/vcpkg-version.txt
COPY ./gha/builds/release/googlebigqueryodbc.ini /opt/odbc-driver/googlebigqueryodbc.ini
# Update the ld.conf cache in case any libraries were installed in /usr/local/lib*
RUN ldconfig /usr/local/lib*
RUN echo 'Dockerfile Done!'
