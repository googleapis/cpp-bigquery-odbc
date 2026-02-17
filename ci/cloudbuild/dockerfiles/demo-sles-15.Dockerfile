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
        gcc14 gcc14-c++ git gzip libcurl-devel libopenssl-devel \
        libtool make patch tar wget which zlib zlib-devel-static \
        zip unzip tar flex ninja patterns-devel-base-devel_basis xz

RUN curl -L https://github.com/llvm/llvm-project/releases/download/llvmorg-12.0.1/clang+llvm-12.0.1-x86_64-linux-gnu-ubuntu-16.04.tar.xz -o clang12.tar.xz && \
    tar -xJf clang12.tar.xz --strip-components=1 -C /usr/local && \
    rm clang12.tar.xz

# 3. Set the compiler environment variables
ENV CC=/usr/local/bin/clang
ENV CXX=/usr/local/bin/clang++
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
RUN curl -fsSL https://github.com/Kitware/CMake/releases/download/v3.26.4/cmake-3.26.4.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./bootstrap && \
    make -j$(nproc) && \
    make install
# ```

# #### Abseil

# Abseil is a dependency of google-cloud-cpp
# ```bash
WORKDIR /var/tmp/build/abseil-cpp
RUN curl -fsSL https://github.com/abseil/abseil-cpp/archive/20240116.3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DABSL_BUILD_TESTING=OFF \
      -DBUILD_SHARED_LIBS=ON \
      -DCMAKE_CXX_STANDARD=17 \
      -DCMAKE_CXX_STANDARD_REQUIRED=ON \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
      -S . -B cmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig
# ```

# #### Protobuf

# We need to install a version of Protobuf that is recent enough to support the
# Google Cloud Platform proto files:

# ```bash
WORKDIR /var/tmp/build/protobuf
RUN curl -fsSL https://github.com/protocolbuffers/protobuf/archive/v29.3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=17 \
        -DCMAKE_CXX_STANDARD_REQUIRED=ON \
        -DBUILD_SHARED_LIBS=ON \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
        -S . -B cmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig
# ```

# #### c-ares

# c-ares is a dependency of google-cloud-cpp

# ```bash
WORKDIR /var/tmp/build/c-ares
RUN curl -fsSL https://github.com/c-ares/c-ares/archive/cares-1_17_1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./buildconf && ./configure && make -j ${NCPU:-4} && \
    make install && \
    ldconfig
# ```

WORKDIR /var/tmp/build/re2
RUN curl -fsSL https://github.com/google/re2/archive/2024-07-02.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=ON \
         -DCMAKE_CXX_STANDARD=17 \
        -DCMAKE_CXX_STANDARD_REQUIRED=ON \
        -DRE2_BUILD_TESTING=OFF \
        -S . -B cmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig

# #### gRPC

# We also need a version of gRPC that is recent enough to support the Google
# Cloud Platform proto files. We manually install it using:

# ```bash
WORKDIR /var/tmp/build/grpc
RUN curl -fsSL https://github.com/grpc/grpc/archive/v1.66.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -DCMAKE_CXX_STANDARD=17 \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
        -DgRPC_RE2_PROVIDER=package \
        -DgRPC_SSL_PROVIDER=package \
        -DgRPC_ZLIB_PROVIDER=package \
        -S . -B cmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig
# ```

# #### crc32c

# crc32c is a dependency of google-cloud-cpp

# ```bash
WORKDIR /var/tmp/build/crc32c
RUN curl -fsSL https://github.com/google/crc32c/archive/1.1.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -DCRC32C_BUILD_TESTS=OFF \
        -DCRC32C_BUILD_BENCHMARKS=OFF \
        -DCRC32C_USE_GLOG=OFF \
        -S . -B cmake-out && \
    cmake --build cmake-out -- -j ${NCPU:-4} && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig
# ```

# #### nlohmann_json library

# google-cloud-cpp depends on the nlohmann_json library. We use CMake to
# install it as this installs the necessary CMake configuration files.
# Note that this is a header-only library, and often installed manually.
# This leaves your environment without support for CMake pkg-config.

# ```bash
WORKDIR /var/tmp/build/json
RUN curl -fsSL https://github.com/nlohmann/json/archive/v3.11.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -DJSON_BuildTests=OFF \
      -S . -B cmake-out && \
    cmake --build cmake-out --target install -- -j ${NCPU:-4} && \
    ldconfig
# ```


## [DONE packaging.md]

# Dependency for arrow
WORKDIR /var/tmp/bison
RUN curl -fsSL https://ftp.gnu.org/gnu/bison/bison-3.8.2.tar.gz | \
    tar -zxf - --strip-components=1 && \
    ./configure --prefix=/usr/local && \
    make -j$(nproc) && \
    make install

# Install sccache from https://github.com/mozilla/sccache
WORKDIR /var/tmp/sccache
RUN curl -fsSL https://github.com/mozilla/sccache/releases/download/v0.5.4/sccache-v0.5.4-x86_64-unknown-linux-musl.tar.gz | \
    tar -zxf - --strip-components=1 && \
    mkdir -p /usr/local/bin && \
    mv sccache /usr/local/bin/sccache && \
    chmod +x /usr/local/bin/sccache

ENV VCPKG_ROOT=/vcpkg
RUN git clone https://github.com/microsoft/vcpkg $VCPKG_ROOT
WORKDIR $VCPKG_ROOT
RUN ./bootstrap-vcpkg.sh -disableMetrics

# Some of the above libraries may have installed in /usr/local, so make sure
# those library directories will be found.
RUN ldconfig /usr/local/lib*
