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
        openssl-devel patch perl-IPC-Cmd kernel-headers \
        libffi-devel  glibc-headers perl \
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
RUN curl -fsSL https://github.com/Kitware/CMake/releases/download/v3.31.10/cmake-3.31.10.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./bootstrap && \
    make -j$(nproc) && \
    make install
# ```

# Download and build Python 3.10
RUN wget https://www.python.org/ftp/python/3.10.13/Python-3.10.13.tgz && \
    tar -xzf Python-3.10.13.tgz && \
    cd Python-3.10.13 && \
    ./configure && \
    make -j$(nproc) && \
    make altinstall

# Fix shared library path
RUN echo "/usr/local/lib" > /etc/ld.so.conf.d/python3.10.conf && ldconfig

# Make python3 default
RUN ln -sf /usr/local/bin/python3.10 /usr/bin/python && \
    ln -sf /usr/local/bin/python3.10 /usr/bin/python3

# Bison and Flex are not included in Red Hat UBI 8, 
# so they need to be downloaded and installed manually
# Dependency for arrow
# ```bash
WORKDIR /var/tmp/bison
RUN curl -fsSL https://ftp.gnu.org/gnu/bison/bison-3.8.2.tar.gz | \
    tar -zxf - --strip-components=1 && \
    ./configure --prefix=/usr/local && \
    make -j$(nproc) && \
    make install
# ```

# Dependency for arrow
# ```bash
WORKDIR /var/tmp/flex
RUN curl -fsSL https://github.com/westes/flex/releases/download/v2.6.4/flex-2.6.4.tar.gz | \
    tar -zxf - --strip-components=1 && \
    sed -i '1i #define _GNU_SOURCE' src/flexdef.h && \
    ./configure --prefix=/usr/local \
    CFLAGS="-D_GNU_SOURCE -Wno-int-conversion -Wno-implicit-function-declaration" && \
    make -j$(nproc) && \
    make install
# ```

COPY ./etc/vcpkg-version.txt /tmp/vcpkg-version.txt
COPY ./gha/builds/release/googlebigqueryodbc.ini /opt/odbc-driver/googlebigqueryodbc.ini

RUN ldconfig /usr/local/lib*
RUN echo 'Dockerfile Done!'


# Install the Cloud SDK
COPY ./dependencies/cloud-sdk.sh /var/tmp/ci/dependencies/cloud-sdk.sh
WORKDIR /var/tmp/downloads
RUN /var/tmp/ci/dependencies/cloud-sdk.sh
ENV CLOUD_SDK_LOCATION=/usr/local/google-cloud-sdk
ENV PATH=${CLOUD_SDK_LOCATION}/bin:${PATH}
