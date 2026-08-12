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
# limitations under the License.

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get --no-install-recommends install -y \
        automake \
        autotools-dev \
        build-essential \
        # Dependency for arrow
        bison \
        clang \
        cmake \
        curl \
        # Dependency for arrow
        flex \
        gawk \
        git \
        gcc \
        g++ \
        libc++-dev \
        libc++abi-dev \
        libcurl4-openssl-dev \
        # Needed to use autoreconf
        libltdl-dev \
        libssl-dev \
        libtool \
        llvm \
        locales \
        lsb-release \
        make \
        ninja-build \
        patch \
        # Needed to use autoreconf
        perl \
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

# Set Clang 12 as default
RUN update-alternatives --install /usr/bin/cc cc /usr/bin/clang 100 && \
    update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++ 100

# Set the compiler environment variables
ENV CC=/usr/bin/clang
ENV CXX=/usr/bin/clang++

# Needed for the existing driver v3.1.2.1004+
RUN locale-gen en_US.UTF-8
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US.UTF-8
ENV LC_ALL en_US.UTF-8

# clang-tidy-cache needs python
RUN update-alternatives --install /usr/bin/python python $(which python3) 10

COPY ./requirements.txt /var/tmp/ci/requirements.txt
WORKDIR /var/tmp/downloads
RUN if [ $(ls /var/tmp/ci/requirements.txt | grep -c requirements.txt) -eq 0 ] ; \
    then echo 'Unable to find requirements.txt for python...' ; exit 1 ; fi
RUN pip3 install --require-hashes --no-deps -r /var/tmp/ci/requirements.txt
# Install the Cloud SDK
COPY ./dependencies/cloud-sdk.sh /var/tmp/ci/dependencies/cloud-sdk.sh
WORKDIR /var/tmp/downloads
RUN /var/tmp/ci/dependencies/cloud-sdk.sh
ENV CLOUD_SDK_LOCATION=/usr/local/google-cloud-sdk
ENV PATH=${CLOUD_SDK_LOCATION}/bin:${PATH}

## BEGIN Installs pre-requisites for the ODBC Driver.
COPY ./etc/vcpkg-version.txt /tmp/vcpkg-version.txt
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
