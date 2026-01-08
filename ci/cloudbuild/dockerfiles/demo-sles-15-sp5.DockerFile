# Copyright 2025 Google LLC
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
# limitations under the License

FROM opensuse/leap:15.5

ARG NCPU=4
RUN echo "my testing file"
# ------------------------------------------------------------
# Base build tools (Leap 15.5 = glibc 2.31)
# ------------------------------------------------------------
RUN zypper refresh && \
    zypper install -y \
        automake \
        awk \
        curl \
        gcc \
        gcc-c++ \
        git \
        gzip \
        libcurl-devel \
        libopenssl-devel \
        libtool \
        make \
        patch \
        tar \
        wget \
        which \
        zlib \
        zlib-devel \
        zip \
        unzip \
        flex \
        python3 \
    && zypper clean

# Use system compiler (gcc 11)
ENV CC=/usr/bin/gcc
ENV CXX=/usr/bin/g++

# ------------------------------------------------------------
# Runtime linker + pkg-config paths
# ------------------------------------------------------------
RUN (echo "/usr/local/lib" ; echo "/usr/local/lib64") | \
    tee /etc/ld.so.conf.d/usrlocal.conf

ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig
ENV PATH=/usr/local/bin:${PATH}

# ------------------------------------------------------------
# CMake (build from source – as in original)
# ------------------------------------------------------------
WORKDIR /var/tmp/build/cmake
RUN curl -fsSL https://github.com/Kitware/CMake/releases/download/v3.21.1/cmake-3.21.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./bootstrap && \
    make -j$(nproc) && \
    make install

# ------------------------------------------------------------
# Abseil
# ------------------------------------------------------------
WORKDIR /var/tmp/build/abseil-cpp
RUN curl -fsSL https://github.com/abseil/abseil-cpp/archive/20230802.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    sed -i 's/^#define ABSL_OPTION_USE_\(.*\) 2/#define ABSL_OPTION_USE_\1 0/' absl/base/options.h && \
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DABSL_BUILD_TESTING=OFF \
          -DBUILD_SHARED_LIBS=ON \
          -S . -B cmake-out && \
    cmake --build cmake-out -j ${NCPU} && \
    cmake --build cmake-out --target install -j ${NCPU} && \
    ldconfig

# ------------------------------------------------------------
# Protobuf
# ------------------------------------------------------------
WORKDIR /var/tmp/build/protobuf
RUN curl -fsSL https://github.com/protocolbuffers/protobuf/archive/v23.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_SHARED_LIBS=ON \
          -Dprotobuf_BUILD_TESTS=OFF \
          -Dprotobuf_ABSL_PROVIDER=package \
          -S . -B cmake-out && \
    cmake --build cmake-out -j ${NCPU} && \
    cmake --build cmake-out --target install -j ${NCPU} && \
    ldconfig

# ------------------------------------------------------------
# c-ares
# ------------------------------------------------------------
WORKDIR /var/tmp/build/c-ares
RUN curl -fsSL https://github.com/c-ares/c-ares/archive/cares-1_14_0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./buildconf && \
    ./configure && \
    make -j ${NCPU} && \
    make install && \
    ldconfig

# ------------------------------------------------------------
# re2
# ------------------------------------------------------------
WORKDIR /var/tmp/build/re2
RUN curl -fsSL https://github.com/google/re2/archive/2023-06-02.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_SHARED_LIBS=ON \
          -DRE2_BUILD_TESTING=OFF \
          -S . -B cmake-out && \
    cmake --build cmake-out -j ${NCPU} && \
    cmake --build cmake-out --target install -j ${NCPU} && \
    ldconfig

# ------------------------------------------------------------
# gRPC
# ------------------------------------------------------------
WORKDIR /var/tmp/build/grpc
RUN curl -fsSL https://github.com/grpc/grpc/archive/v1.55.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_SHARED_LIBS=ON \
          -DgRPC_INSTALL=ON \
          -DgRPC_BUILD_TESTS=OFF \
          -DgRPC_ABSL_PROVIDER=package \
          -DgRPC_CARES_PROVIDER=package \
          -DgRPC_PROTOBUF_PROVIDER=package \
          -DgRPC_RE2_PROVIDER=package \
          -DgRPC_SSL_PROVIDER=package \
          -DgRPC_ZLIB_PROVIDER=package \
          -S . -B cmake-out && \
    cmake --build cmake-out -j ${NCPU} && \
    cmake --build cmake-out --target install -j ${NCPU} && \
    ldconfig

# ------------------------------------------------------------
# crc32c
# ------------------------------------------------------------
WORKDIR /var/tmp/build/crc32c
RUN curl -fsSL https://github.com/google/crc32c/archive/1.1.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_SHARED_LIBS=ON \
          -DCRC32C_BUILD_TESTS=OFF \
          -DCRC32C_BUILD_BENCHMARKS=OFF \
          -DCRC32C_USE_GLOG=OFF \
          -S . -B cmake-out && \
    cmake --build cmake-out -j ${NCPU} && \
    cmake --build cmake-out --target install -j ${NCPU} && \
    ldconfig

# ------------------------------------------------------------
# nlohmann/json
# ------------------------------------------------------------
WORKDIR /var/tmp/build/json
RUN curl -fsSL https://github.com/nlohmann/json/archive/v3.11.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_SHARED_LIBS=ON \
          -DBUILD_TESTING=OFF \
          -DJSON_BuildTests=OFF \
          -S . -B cmake-out && \
    cmake --build cmake-out --target install -j ${NCPU} && \
    ldconfig

# ------------------------------------------------------------
# Bison (Arrow dependency)
# ------------------------------------------------------------
WORKDIR /var/tmp/bison
RUN curl -fsSL https://ftp.gnu.org/gnu/bison/bison-3.8.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./configure --prefix=/usr/local && \
    make -j$(nproc) && \
    make install

# ------------------------------------------------------------
# sccache
# ------------------------------------------------------------
WORKDIR /var/tmp/sccache
RUN curl -fsSL https://github.com/mozilla/sccache/releases/download/v0.5.4/sccache-v0.5.4-x86_64-unknown-linux-musl.tar.gz | \
    tar -xzf - --strip-components=1 && \
    install -m 0755 sccache /usr/local/bin/sccache

# ------------------------------------------------------------
# vcpkg
# ------------------------------------------------------------
ENV VCPKG_ROOT=/vcpkg
RUN git clone https://github.com/microsoft/vcpkg ${VCPKG_ROOT}
WORKDIR ${VCPKG_ROOT}
RUN ./bootstrap-vcpkg.sh -disableMetrics

RUN ldconfig /usr/local/lib*
