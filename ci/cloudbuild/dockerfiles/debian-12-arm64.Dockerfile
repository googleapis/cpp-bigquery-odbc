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
FROM --platform=linux/arm64 debian:12

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get --no-install-recommends install -y \
        automake autotools-dev build-essential \
        bison clang cmake curl flex gawk git \
        gcc g++ libc++-dev libc++abi-dev \
        libcurl4-openssl-dev libltdl-dev libssl-dev \
        libtool llvm locales lsb-release make \
        ninja-build patch perl pkg-config \
        python3 python3-dev python3-pip python-is-python3 \
        tar unzip zip wget zlib1g-dev \
        apt-utils ca-certificates apt-transport-https \
        clang-tidy libc6 \
        bash xz-utils \
    && rm -rf /var/lib/apt/lists/*


RUN locale-gen en_US.UTF-8
ENV LANG=en_US.UTF-8 LANGUAGE=en_US:en LC_ALL=en_US.UTF-8

# Python alias
RUN update-alternatives --install /usr/bin/python python /usr/bin/python3 10

COPY ./requirements.txt /var/tmp/ci/requirements.txt
RUN pip3 install --break-system-packages --require-hashes --no-deps \
    -r /var/tmp/ci/requirements.txt

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
# m4 (needed by autotools)
# ------------------------------------------------------------
WORKDIR /tmp/m4
RUN curl -fsSL https://ftp.gnu.org/gnu/m4/m4-1.4.19.tar.gz | \
    tar -xzf - --strip-components=1 && \
    ./configure && \
    make -j$(nproc) && \
    make install && \
    rm -rf /tmp/m4

ENV VCPKG_ROOT=/vcpkg

COPY ./etc/vcpkg-version.txt /tmp/vcpkg-version.txt
RUN VCPKG_VERSION=$(cat /tmp/vcpkg-version.txt) && \
    git clone --branch $VCPKG_VERSION https://github.com/microsoft/vcpkg.git $VCPKG_ROOT
WORKDIR $VCPKG_ROOT
RUN ./bootstrap-vcpkg.sh -disableMetrics

# Install the Cloud SDK
COPY ./dependencies/cloud-sdk.sh /var/tmp/ci/dependencies/cloud-sdk.sh
WORKDIR /var/tmp/downloads
RUN /var/tmp/ci/dependencies/cloud-sdk.sh
ENV CLOUD_SDK_LOCATION=/usr/local/google-cloud-sdk
ENV PATH=${CLOUD_SDK_LOCATION}/bin:${PATH}

## BEGIN Installs pre-requisites for the ODBC Driver.

COPY ./gha/builds/lib/odbc.ini /opt/odbc-driver/odbc.ini
COPY ./gha/builds/lib/odbcinst.ini /opt/odbc-driver/odbcinst.ini
COPY ./gha/builds/lib/lsan.supp /opt/odbc-driver/lsan.supp
COPY ./gha/builds/release/odbc.ini /opt/odbc-driver/odbc_template.ini
COPY ./gha/builds/release/odbcinst.ini /opt/odbc-driver/odbcinst_template.ini
COPY ./gha/builds/release/googlebigqueryodbc.ini /opt/odbc-driver/googlebigqueryodbc.ini
COPY ./etc/googlebigqueryodbc_utf16.ini /opt/odbc-driver/googlebigqueryodbc_utf16.ini
COPY ./etc/googlebigqueryodbc_utf8.ini /opt/odbc-driver/googlebigqueryodbc_utf8.ini

# glibc 2.17 or later
RUN echo 'Installing glibc...'
RUN apt-get install -y --no-install-recommends libc6
RUN echo 'Verifying glibc version...'
RUN dpkg -l libc6
RUN if [ $(ldd --version | grep GLIBC | awk '{print $5}') -lt 2.17 ] ; \
    then echo 'glibc version is < 2.17: exiting...' ; exit 1 ; fi
