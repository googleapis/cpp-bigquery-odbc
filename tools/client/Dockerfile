# Copyright 2026 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

FROM ubuntu:18.04

ENV DEBIAN_FRONTEND=noninteractive

# Install minimal packages needed to compile the client and run it with unixodbc
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        unixodbc \
        unixodbc-dev \
        ca-certificates \
        libssl1.1 \
        libcurl4 \
    && rm -rf /var/lib/apt/lists/*

# Copy client source code and CA certificates
WORKDIR /workspace
COPY tools/client/ /workspace/tools/client/
COPY ci/etc/roots.pem /usr/local/lib/cacerts.pem
COPY ci/etc/roots.pem /usr/local/lib/roots.pem

# Compile the client program
RUN g++ -std=c++17 -O3 -I/workspace/tools/client \
    /workspace/tools/client/main.cc \
    /workspace/tools/client/commons.cc \
    -lodbc -o /usr/local/bin/odbc_client

ENV ODBCSYSINI=/etc
ENTRYPOINT ["/usr/local/bin/odbc_client"]
