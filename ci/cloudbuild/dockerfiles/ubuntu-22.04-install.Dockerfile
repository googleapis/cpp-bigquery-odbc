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

FROM ubuntu:22.04

# ENV for iODBC driver manager
ENV GCS_BUCKET=bq-dev-tools-testing-drivers

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get --no-install-recommends install -y \
        automake \
        build-essential \
        clang \
        cmake \
        curl \
        gawk \
        git \
        gcc \
        g++ \
        libc++-dev \
        libc++abi-dev \
        libcurl4-openssl-dev \
        libssl-dev \
        libtool \
        llvm \
        lsb-release \
        make \
        ninja-build \
        patch \
        pkg-config \
        python3 \
        python3-dev \
        python3-pip \
        tar \
        unixodbc \
        unixodbc-dev \
        unzip \
        zip \
        wget \
        zlib1g-dev \
        apt-utils \
        ca-certificates \
        apt-transport-https \
        clang-tidy

# clang-tidy-cache needs python
RUN update-alternatives --install /usr/bin/python python $(which python3) 10

COPY ./requirements.txt /var/tmp/ci/requirements.txt
WORKDIR /var/tmp/downloads
RUN if [ $(ls /var/tmp/ci/requirements.txt | grep -c requirements.txt) -eq 0 ] ; \
    then echo 'Unable to find requirements.txt for python...' ; exit 1 ; fi
RUN pip3 install --require-hashes -r /var/tmp/ci/requirements.txt

# Install all the direct (and indirect) dependencies for cpp-bigquery-odbc.
# Use a different directory for each build, and remove the downloaded
# files and any temporary artifacts after a successful build to keep the
# image smaller (and with fewer layers)

WORKDIR /var/tmp/build/abseil-cpp
RUN curl -fsSL https://github.com/abseil/abseil-cpp/archive/20230125.3.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DABSL_BUILD_TESTING=OFF \
      -DABSL_PROPAGATE_CXX_STD=ON \
      -DBUILD_SHARED_LIBS=yes \
      -S . -B cmake-out -GNinja && \
    cmake --build cmake-out --target install && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/googletest
RUN curl -fsSL https://github.com/google/googletest/archive/v1.13.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_SHARED_LIBS=yes \
      -S . -B cmake-out -GNinja  && \
    cmake --build cmake-out --target install && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/benchmark
RUN curl -fsSL https://github.com/google/benchmark/archive/v1.8.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE="Release" \
        -DBUILD_SHARED_LIBS=yes \
        -DBENCHMARK_ENABLE_TESTING=OFF \
        -S . -B cmake-out -GNinja  && \
    cmake --build cmake-out --target install && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/crc32c
RUN curl -fsSL https://github.com/google/crc32c/archive/1.1.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_SHARED_LIBS=yes \
      -DCRC32C_BUILD_TESTS=OFF \
      -DCRC32C_BUILD_BENCHMARKS=OFF \
      -DCRC32C_USE_GLOG=OFF \
      -S . -B cmake-out -GNinja && \
    cmake --build cmake-out --target install && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/nlohmann-json
RUN curl -fsSL https://github.com/nlohmann/json/archive/v3.11.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
      -DCMAKE_BUILD_TYPE="Release" \
      -DBUILD_SHARED_LIBS=yes \
      -DBUILD_TESTING=OFF \
      -DJSON_BuildTests=OFF \
      -S . -B cmake-out -GNinja && \
    cmake --build cmake-out --target install && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/protobuf
RUN curl -fsSL https://github.com/protocolbuffers/protobuf/archive/v23.2.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -Dprotobuf_BUILD_TESTS=OFF \
        -Dprotobuf_ABSL_PROVIDER=package \
        -S . -B cmake-out -GNinja && \
    cmake --build cmake-out --target install && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/c-ares
RUN curl -fsSL https://github.com/c-ares/c-ares/archive/refs/tags/cares-1_17_1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=yes \
        -S . -B cmake-out -GNinja && \
    cmake --build cmake-out --target install && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/re2
RUN curl -fsSL https://github.com/google/re2/archive/2023-06-02.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=ON \
        -DRE2_BUILD_TESTING=OFF \
        -S . -B cmake-out -GNinja && \
    cmake --build cmake-out --target install && \
    ldconfig && \
    cd /var/tmp && rm -fr build

WORKDIR /var/tmp/build/grpc
RUN curl -fsSL https://github.com/grpc/grpc/archive/v1.55.0.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=ON \
        -DgRPC_INSTALL=ON \
        -DgRPC_BUILD_TESTS=OFF \
        -DgRPC_ABSL_PROVIDER=package \
        -DgRPC_CARES_PROVIDER=package \
        -DgRPC_PROTOBUF_PROVIDER=package \
        -DgRPC_RE2_PROVIDER=package \
        -DgRPC_SSL_PROVIDER=package \
        -DgRPC_ZLIB_PROVIDER=package \
        -S . -B cmake-out -GNinja && \
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

WORKDIR /var/tmp/google-cloud-cpp
# Temporary points to a specific commit with a fix which we need. Later it should be updated with a releaze commit
RUN curl -fsSL https://github.com/googleapis/google-cloud-cpp/archive/965a52ba9665a691682ed757ed77496c512ccbbe.tar.gz | \
    tar -zxf - --strip-components=1 && \
    cmake \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DGOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND=ON \
        -DBUILD_TESTING=OFF \
        -DGOOGLE_CLOUD_CPP_ENABLE_EXAMPLES=OFF \
        -DGOOGLE_CLOUD_CPP_ENABLE=experimental-bigquery_rest,oauth2,bigquery \
        -S . -B cmake-out -GNinja && \
    cmake --build cmake-out -- -j $(nproc) && \
    cmake --build cmake-out --target install

# Install the Cloud SDK
COPY ./install-cloud-sdk.sh /var/tmp/ci/install-cloud-sdk.sh
WORKDIR /var/tmp/downloads
RUN /var/tmp/ci/install-cloud-sdk.sh
ENV CLOUD_SDK_LOCATION=/usr/local/google-cloud-sdk
ENV PATH=${CLOUD_SDK_LOCATION}/bin:${PATH}

RUN curl -o /usr/bin/bazelisk -sSL "https://github.com/bazelbuild/bazelisk/releases/download/v1.18.0/bazelisk-linux-amd64" && \
    chmod +x /usr/bin/bazelisk && \
    ln -s /usr/bin/bazelisk /usr/bin/bazel

#>>>>>>>>>>>>>>>>> ODBC Driver setup >>>>>>>>>>>>>>>

RUN echo '**** ODBC Driver installation START ****'

## BEGIN Installs pre-requisites for the ODBC Driver.

# glibc 2.17 or later
RUN echo 'Installing glibc...'
RUN apt-get install -y --no-install-recommends libc6
RUN echo 'Verifying glibc version...'
RUN dpkg -l libc6
RUN if [ $(ldd --version | grep GLIBC | awk '{print $5}') -lt 2.17 ] ; \
    then echo 'glibc version is < 2.17: exiting...' ; exit 1 ; fi

# iODBC Driver Manager
# RUN echo 'Installing iODBC Driver Manager...'
# WORKDIR /var/tmp/iODBC
# RUN curl -fsSL https://github.com/openlink/iODBC/releases/download/v3.52.16/libiodbc-3.52.16.tar.gz | \
#     tar -zxf - --strip-components=1 && \
#     autoreconf --install && \
#     ./configure && \
#     make install -j $(nproc)

RUN echo 'Installing iODBC Driver Manager...'
WORKDIR /var/tmp/iODBC
RUN curl -fsSL https://github.com/openlink/iODBC/releases/download/v3.52.16/libiodbc-3.52.16.tar.gz | \
    tar -zxf - --strip-components=1 && \
    autoreconf --install && \
    ./configure && \
    make install -j $(nproc)

## END Installs pre-requisites for the ODBC Driver.

# Check gcloud is installed.
RUN echo "Verifying google cloud SDK is installed using GCS Bucket: "${GCS_BUCKET}
RUN if [ $(gsutil ls gs://${GCS_BUCKET}/odbc | grep -c odbc-driver.zip) -eq 0 ] ; \
    then echo 'ODBC driver not found for download: exiting...' ; exit 1 ; fi


# Configure connection credentials for the driver.
RUN echo 'Configuring Connection Credentials...'
RUN mkdir -p /opt/odbc-driver/connection
WORKDIR /opt/odbc-driver
RUN gcloud secrets versions access latest --secret=service-account-auth-keys --out-file="/opt/odbc-driver/connection/key.json"
RUN echo 'Verifying Connection Keys File Size...'
RUN if [ $(stat -c%s /opt/odbc-driver/connection/key.json) -lt 100 ] ; \
    then echo 'Invalid connection keys: exiting...' ; exit 1 ; fi

# Install the ODBC Driver
RUN echo 'Installing ODBC Driver...'
RUN gsutil -m cp gs://${GCS_BUCKET}/odbc/odbc-driver.zip .
RUN unzip -qq odbc-driver.zip
RUN echo 'Verifying Driver Install Directory...'
RUN if [ $(ls /opt/odbc-driver/ | grep -c googlebigqueryodbc) -eq 0 ] ; \
    then echo 'ODBC driver not installed: exiting...' ; exit 1 ; fi

# Configure environment variables
RUN echo 'Configuring Environment Variables For ODBC Driver...'
ENV LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:/usr/local/lib/
ENV ODBCINI=/opt/odbc-driver/googlebigqueryodbc/odbc.ini
ENV ODBCINSTINI=/opt/odbc-driver/googlebigqueryodbc/odbcinst.ini
ENV SIMBAGOOGLEBIGQUERYODBCINI=/opt/odbc-driver/googlebigqueryodbc/lib/simba.googlebigqueryodbc.ini
RUN echo 'Verifying Environment Variables...'
RUN echo 'LD_LIBRARY_PATH='${LD_LIBRARY_PATH}
RUN echo 'ODBCINI='${ODBCINI}
RUN echo 'ODBCINSTINI='${ODBCINSTINI}
RUN echo 'SIMBAGOOGLEBIGQUERYODBCINI='${SIMBAGOOGLEBIGQUERYODBCINI}

RUN echo '**** ODBC Driver installation END****'
