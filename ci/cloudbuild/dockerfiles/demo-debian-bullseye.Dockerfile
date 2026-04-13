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

FROM debian:bullseye
ARG NCPU=4

## [BEGIN packaging.md]

# Install the minimal development tools, libcurl, and OpenSSL:

# ```bash
RUN apt-get update && \
    apt-get --no-install-recommends install -y apt-transport-https apt-utils \
        automake build-essential ca-certificates bison flex curl git \
        libc-ares-dev libc-ares2 libcurl4-openssl-dev libffi-dev \
        libssl-dev libtool m4 make ninja-build pkg-config tar unzip wget zip zlib1g-dev
# ```

# Install clang 12 from LLVM repository
# ```bash
RUN apt-get update && \
    apt-get --no-install-recommends install -y gnupg2 wget lsb-release && \
    wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc && \
    echo "deb http://apt.llvm.org/bullseye/ llvm-toolchain-bullseye-12 main" > /etc/apt/sources.list.d/llvm.list && \
    apt-get update && \
    apt-get --no-install-recommends install -y clang-12 lld-12 && \
    ln -sf /usr/bin/clang-12 /usr/bin/clang && \
    ln -sf /usr/bin/clang++-12 /usr/bin/clang++ && \
    ln -sf /usr/bin/lld-12 /usr/bin/lld
# ```

# Set Clang as default compiler
ENV CC=/usr/bin/clang
ENV CXX=/usr/bin/clang++

# TODO(#43): When https://github.com/googleapis/cpp-bigquery-odbc/issues/43 is fixed, remove
# the installation of cmake from source
# ```bash
WORKDIR /var/tmp/build/cmake
RUN curl -fsSL https://github.com/Kitware/CMake/releases/download/v3.31.10/cmake-3.31.10.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./bootstrap && \
    make -j$(nproc) && \
    make install
# ```

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
