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

FROM redhat/ubi8
ARG NCPU=4
ARG ARCH=amd64

RUN dnf makecache && \
    dnf install -y autoconf automake \
        xz clang clang-analyzer clang-tools-extra \
        diffutils findutils \
        # Using gcc-toolset-12 as provided by the base image
        git libtool libcurl-devel llvm make ninja-build \
        openssl-devel patch perl-IPC-Cmd \
        tar unzip wget which zip zlib-devel && \
        dnf module install -y llvm-toolset && \
        dnf install -y lld compiler-rt llvm-devel clang-devel && \
    dnf clean all

RUN dnf install -y pkgconf-pkg-config && \
    dnf clean all

SHELL ["/bin/bash", "-c"]
ENV CC=clang
ENV CXX=clang++
ENV LDFLAGS="-fuse-ld=lld"

# Sets root's password to the empty string to enable users to get a root shell
# inside the container with `su -` and no password. Sudo would not work because
# we run these containers as the invoking user's uid, which does not exist in
# the container's /etc/passwd file.
RUN echo 'root:' | chpasswd

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

# Abseil is a dependency of google-cloud-cpp
WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/abseil/abseil-cpp/archive/20240116.3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DABSL_BUILD_TESTING=OFF \
      -DCMAKE_CXX_STANDARD=17 \
        -DCMAKE_CXX_STANDARD_REQUIRED=ON \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
      -DBUILD_SHARED_LIBS=ON \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/google/googletest/archive/v1.15.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_SHARED_LIBS=yes \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build


# benchmark is a dependency of google-cloud-cpp
WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/google/benchmark/archive/v1.8.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_SHARED_LIBS=yes \
      -DBENCHMARK_ENABLE_TESTING=OFF \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

# crc32c is a dependency of google-cloud-cpp
WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/google/crc32c/archive/1.1.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DCRC32C_BUILD_TESTS=OFF \
      -DCRC32C_BUILD_BENCHMARKS=OFF \
      -DCRC32C_USE_GLOG=OFF \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build


# nlohmann_json is a dependency of google-cloud-cpp
WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/nlohmann/json/archive/v3.11.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -DJSON_BuildTests=OFF \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

# protobuf is a dependency of google-cloud-cpp
WORKDIR /var/tmp/build/protobuf
RUN curl -fsSL https://github.com/protocolbuffers/protobuf/archive/v29.3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=ON \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_CXX_STANDARD_REQUIRED=ON \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

# re2 is a dependency of google-cloud-cpp
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

# #### c-ares

# ```bash
WORKDIR /var/tmp/build/c-ares
RUN curl -fsSL https://github.com/c-ares/c-ares/archive/cares-1_14_0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./buildconf && ./configure && make -j ${NCPU:-4} && \
    make install && \
    ldconfig
# ```

# grpc is a dependency of google-cloud-cpp
WORKDIR /var/tmp/build/grpc
RUN curl -fsSL https://github.com/grpc/grpc/archive/v1.66.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=ON \
      -DgRPC_INSTALL=ON \
        -DCMAKE_CXX_STANDARD=17 \
      -DgRPC_BUILD_TESTS=OFF \
      -DgRPC_ABSL_PROVIDER=package \
      -DgRPC_CARES_PROVIDER=package \
      -DgRPC_PROTOBUF_PROVIDER=package \
      -DgRPC_PROTOBUF_PACKAGE_TYPE=CONFIG \
      -DgRPC_RE2_PROVIDER=package \
      -DgRPC_SSL_PROVIDER=package \
      -DgRPC_ZLIB_PROVIDER=package \
      -GNinja -S . -B cmake-out && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

# #### opentelemetry library
# ```bash
WORKDIR /var/tmp/build/opentelemetry
RUN curl -fsSL https://github.com/open-telemetry/opentelemetry-cpp/archive/refs/tags/v1.23.0.tar.gz | \
     tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=ON \
        -DCMAKE_CXX_STANDARD=17 \
        -DWITH_OTLP_GRPC=ON \
        -DWITH_OTLP_HTTP=ON \
        -DWITH_ABSEIL=OFF \
        -DWITH_EXAMPLES=OFF \
        -DWITH_TEST=OFF \
        -GNinja \
        -B cmake-out -S . && \
    export LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib64:$LD_LIBRARY_PATH && \
    cmake --build cmake-out --target install && \
    ldconfig && \
    cd /var/tmp && rm -fr build
# ```

# Dependency for arrow
WORKDIR /var/tmp/bison
RUN curl -fsSL https://ftp.gnu.org/gnu/bison/bison-3.8.2.tar.gz | \
    tar -zxf - --strip-components=1 && \
    ./configure --prefix=/usr/local && \
    make -j$(nproc) && \
    make install

# Dependency for arrow
WORKDIR /var/tmp/flex
RUN curl -fsSL https://github.com/westes/flex/releases/download/v2.6.4/flex-2.6.4.tar.gz | \
    tar -zxf - --strip-components=1 && \
    sed -i '1i #define _GNU_SOURCE' src/flexdef.h && \
    ./configure --prefix=/usr/local \
    CFLAGS="-D_GNU_SOURCE -Wno-int-conversion -Wno-implicit-function-declaration" && \
    make -j$(nproc) && \
    make install

# Install sccache from https://github.com/mozilla/sccache
WORKDIR /var/tmp/sccache
RUN curl -fsSL https://github.com/mozilla/sccache/releases/download/v0.5.4/sccache-v0.5.4-x86_64-unknown-linux-musl.tar.gz | \
    tar -zxf - --strip-components=1 && \
    mkdir -p /usr/local/bin && \
    mv sccache /usr/local/bin/sccache && \
    chmod +x /usr/local/bin/sccache

# ENV VCPKG_ROOT=/vcpkg
# RUN git clone https://github.com/microsoft/vcpkg $VCPKG_ROOT
# WORKDIR $VCPKG_ROOT
# RUN ./bootstrap-vcpkg.sh

# Some of the above libraries may have installed in /usr/local, so make sure
# those library directories will be found.
RUN ldconfig /usr/local/lib*

RUN echo 'Dockerfile Done!'
