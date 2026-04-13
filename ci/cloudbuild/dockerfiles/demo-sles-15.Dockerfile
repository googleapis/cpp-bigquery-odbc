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

FROM registry.suse.com/bci/bci-base:latest
ARG NCPU=4

## [BEGIN packaging.md]

# Install the minimal development tools, libcurl and OpenSSL. The gRPC Makefile
# uses `which` to determine whether the compiler is available. Install this
# command for the extremely rare case where it may be missing from your
# workstation or build server.

# ```bash
RUN zypper refresh && \
    zypper install --allow-downgrade --no-recommends -y automake awk curl \
        gcc13  git gzip libcurl-devel libopenssl-devel make \
        libtool make patch tar wget which zlib zlib-devel-static perl \
        zip unzip tar flex ninja patterns-devel-base-devel_basis xz libffi-devel \
         kernel-headers glibc-devel kernel-default-devel libc++-devel libc++abi-devel

# clang12 isn’t available in the bci-base image, so it need to be installed manually
RUN curl -L https://github.com/llvm/llvm-project/releases/download/llvmorg-12.0.1/clang+llvm-12.0.1-x86_64-linux-gnu-ubuntu-16.04.tar.xz -o clang12.tar.xz && \
    tar -xJf clang12.tar.xz --strip-components=1 -C /usr/local && \
    rm clang12.tar.xz

# Set the compiler environment variables
ENV CC=/usr/local/bin/clang
ENV CXX=/usr/local/bin/clang++
ENV CXXFLAGS="-stdlib=libc++"
# ```

# ```bash
RUN (echo "/usr/local/lib" ; echo "/usr/local/lib64") | \
    tee /etc/ld.so.conf.d/usrlocal.conf
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig
ENV PATH=/usr/local/bin:${PATH}
# ```

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

# Download Python 3.10
RUN wget https://www.python.org/ftp/python/3.10.13/Python-3.10.13.tgz && \
    tar -xzf Python-3.10.13.tgz

RUN cd Python-3.10.13 && \
    ./configure --enable-shared && \
    make -j$(nproc) && \
    make altinstall

# Fix shared libs
RUN echo "/usr/local/lib" > /etc/ld.so.conf.d/python3.10.conf && ldconfig

# Make default python3
RUN ln -sf /usr/local/bin/python3.10 /usr/bin/python && \
    ln -sf /usr/local/bin/python3.10 /usr/bin/python3

COPY ./etc/vcpkg-version.txt /tmp/vcpkg-version.txt
COPY ./gha/builds/release/googlebigqueryodbc.ini /opt/odbc-driver/googlebigqueryodbc.ini
# Some of the above libraries may have installed in /usr/local, so make sure
# those library directories will be found.
RUN ldconfig /usr/local/lib*
