# Copyright 2021 Google LLC
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
        ccache \
        clang \
        cmake \
        curl \
        gawk \
        git \
        gcc \
        g++ \
        cmake \
        libcurl4-openssl-dev \
        libssl-dev \
        libtool \
        lsb-release \
        make \
        ninja-build \
        patch \
        pkg-config \
        python3 \
        python3-dev \
        python3-pip \
        tar \
        unzip \
        zip \
        wget \
        zlib1g-dev \
        apt-utils \
        ca-certificates \
        apt-transport-https \
        clang-tidy
# Install Python packages used in the integration tests.
RUN update-alternatives --install /usr/bin/python python $(which python3) 10
RUN pip3 install setuptools wheel requests
# The Cloud Pub/Sub emulator needs Java :shrug:
RUN apt update && apt install -y openjdk-13-jre

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

WORKDIR /var/tmp/build/
RUN curl -fsSL https://github.com/open-telemetry/opentelemetry-cpp/archive/v1.9.1.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cmake \
        -DCMAKE_CXX_STANDARD=14 \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_POSITION_INDEPENDENT_CODE=TRUE \
        -DBUILD_SHARED_LIBS=ON \
        -DWITH_EXAMPLES=OFF \
        -DWITH_ABSEIL=ON \
        -DBUILD_TESTING=OFF \
        -DOPENTELEMETRY_INSTALL=ON \
        -S . -B cmake-out -GNinja && \
    cmake --build cmake-out --target install && \
    ldconfig && cd /var/tmp && rm -fr build

# Install ctcache to speed up our clang-tidy build
WORKDIR /var/tmp/build
RUN curl -fsSL https://github.com/matus-chochlik/ctcache/archive/0ad2e227e8a981a9c1a6060ee6c8ec144bb976c6.tar.gz | \
    tar -xzf - --strip-components=1 && \
    cp clang-tidy /usr/local/bin/clang-tidy-wrapper && \
    cp clang-tidy-cache /usr/local/bin/clang-tidy-cache && \
    cd /var/tmp && rm -fr build

# Install the Cloud SDK and some of the emulators. We use the emulators to run
# integration tests for the client libraries.
COPY . /var/tmp/ci
WORKDIR /var/tmp/downloads
RUN /var/tmp/ci/install-cloud-sdk.sh
ENV CLOUD_SDK_LOCATION=/usr/local/google-cloud-sdk
ENV PATH=${CLOUD_SDK_LOCATION}/bin:${PATH}





#>>>>>>>>>>>>>>>>> iODBC Set UP >>>>>>>>>>>>>>>

RUN echo '**** iODBC installation START ****'

# Build argument obtained from cloudbuild-odbc.yaml
ARG gcs_odbc_bucket
ENV GCS_BUCKET=${gcs_odbc_bucket}
ARG odbc_secret
ENV ODBC_CONN_KEYS=${odbc_secret}

## BEGIN Installs pre-requisits for the Simba ODBC Driver.

# glibc 2.17 or later
RUN echo 'Installing glibc...'
RUN apt-get update && \
    apt-get -y upgrade && \
    apt-get install -y --no-install-recommends libc6
RUN echo 'Verifying glibc version...'
RUN dpkg -l libc6
RUN if [ $(ldd --version | grep GLIBC | awk '{print $5}') -lt 2.17 ] ; \
    then echo 'glibc version is < 2.17: exiting...' ; exit 1 ; fi

# iODBC Driver Manager
RUN echo 'Installing iODBC Driver Manager...'
RUN apt-get install -y --no-install-recommends iodbc
RUN echo 'Verifying iodbc is installed...'
RUN dpkg -l iodbc
RUN echo 'Verifying iODBC Driver Manager libraries are installed...'
RUN if [ $(dpkg --search libiodbc*.so | grep -c libiodbc.so) -eq 0 ] ; \
    then echo 'iodbc installation failed: exiting...' ; exit 1 ; fi

# Configure iODBC Driver Manager
RUN echo "Creating Symlinks For iODBC Driver Manager..."
RUN mkdir -p /usr/local/lib/odbc
RUN ln -s /usr/lib/libiodbc.so.2 /usr/local/lib/odbc/libiodbc.so.2
RUN ln -s /usr/lib/libiodbcinst.so.2 /usr/local/lib/odbc/libiodbcinst.so.2
RUN ln -s /usr/lib/libiodbcadm.so.2 /usr/local/lib/odbc/libiodbcadm.so.2
RUN ln -s /usr/lib/libiodbc.so.2 /usr/local/lib/odbc/libodbc.so
RUN ln -s /usr/lib/libiodbcinst.so.2 /usr/local/lib/odbc/libodbcinst.so
RUN echo "Verifying Symlinks For iODBC Driver Manager..."
RUN if [ $(ls -l /usr/local/lib/odbc | grep -c "libodbc.*.so ->") -eq 0 ] ; \
    then echo 'iOdbc symlink creation failed for libodbc: exiting...' ; \
    exit 1 ; fi
RUN if [ $(ls -l /usr/local/lib/odbc | grep -c "libiodbc.*.so.2 ->") -eq 0 ] ; \
    then echo 'iOdbc symlink creation failed for libiodbc: exiting...' ; \
    exit 1 ; fi

## END Installs pre-requisits for the Simba ODBC Driver.

# Install GCloud SDK - Needed for downloading Simba deliverables.
#RUN echo "Installing gcloud sdk..."
#RUN echo "deb [signed-by=/usr/share/keyrings/cloud.google.gpg] http://packages.cloud.google.com/apt cloud-sdk main" | \
#    tee -a /etc/apt/sources.list.d/google-cloud-sdk.list && \
#    curl https://packages.cloud.google.com/apt/doc/apt-key.gpg | \
#    tee /usr/share/keyrings/cloud.google.gpg && \
#    apt-get -y --force-yes update && \
#    apt-get install -y --force-yes google-cloud-sdk

# Install unzip - Needed for unzipping Simba deliverables.
RUN echo "Installing unzip..."
RUN apt-get install -y unzip
RUN echo "Verifying unzip is installed..."
RUN if [ $(unzip --help | grep -c Usage:) -eq 0 ] ; \
    then echo 'Unzip installation failed: exiting...' ; exit 1 ; fi


# Check gcloud is installed.
RUN echo "Verifying google cloud SDK is installed using GCS Bucket: "${GCS_BUCKET}
RUN if [ $(gsutil ls gs://${GCS_BUCKET}/simba-odbc | grep -c simba.zip) -eq 0 ] ; \
    then echo 'Simba deliverables not found for download: exiting...' ; exit 1 ; fi

# Configure connection credentials for the driver.
RUN echo 'Configuring Connection Credentials...'
RUN mkdir -p /opt/simba/connection
WORKDIR /opt/simba
RUN echo ${ODBC_CONN_KEYS} | tee /opt/simba/connection/key.json > /dev/null
RUN echo 'Verifying Connection Keys File Size...'
RUN if [ $(stat -c%s /opt/simba/connection/key.json) -lt 100 ] ; \
    then echo 'Invalid connection keys: exiting...' ; exit 1 ; fi

# Install Simba ODBC Driver
RUN echo 'Installing Simba ODBC Driver...'
RUN gsutil -m cp gs://${GCS_BUCKET}/simba-odbc/simba.zip .
RUN unzip -qq simba.zip
RUN echo 'Verifying Simba Install Directory...'
RUN if [ $(ls /opt/simba/ | grep -c googlebigqueryodbc) -eq 0 ] ; \
    then echo 'Simba driver not installed: exiting...' ; exit 1 ; fi

# Configure environment variables
RUN echo 'Configuring Environment Variables For Simba Driver...'
ENV LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:/usr/local/lib/odbc
ENV LD_PRELOAD=/usr/local/lib/odbc/libodbc.so:/usr/local/lib/odbc/libodbcinst.so
ENV ODBCINI=/opt/simba/googlebigqueryodbc/odbc.ini
ENV ODBCINSTINI=/opt/simba/googlebigqueryodbc/odbcinst.ini
ENV SIMBAGOOGLEBIGQUERYODBCINI=/opt/simba/googlebigqueryodbc/lib/simba.googlebigqueryodbc.ini
RUN echo 'Verifying Environment Variables...'
RUN echo 'LD_LIBRARY_PATH='${LD_LIBRARY_PATH}
RUN echo 'LD_PRELOAD='${LD_PRELOAD}
RUN echo 'ODBCINI='${ODBCINI}
RUN echo 'ODBCINSTINI='${ODBCINSTINI}
RUN echo 'SIMBAGOOGLEBIGQUERYODBCINI='${SIMBAGOOGLEBIGQUERYODBCINI}
#RUN echo 'JAVA_HOME='${JAVA_HOME}    

RUN echo '****iODBC installation END****'
