// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_ODBC_BQ_DRIVER_CLIENT_INTERFACE_BQ_CLIENT_H
#define GOOGLE_CLOUD_ODBC_BQ_DRIVER_CLIENT_INTERFACE_BQ_CLIENT_H

namespace google {
namespace cloud {
namespace odbc_bigquery_client_interface {

/// ODBC BigQuery Client
///
/// This Client is responsible for interacting with BigQuery API.
/// It should be used for all kinds of such interactions.
///
/// @par Performance
///
/// Creating a new instance of this class is a relatively efficient operation,
/// as there is no connection with a service. Copy-construction, move-construction,
/// and the corresponding assignment operations are also relatively efficient
/// as the copies share all underlying resources.
///
/// @par Thread Safety
///
/// Concurrent access to different instances of this class, even if they compare
/// equal, is guaranteed to work. Two or more threads operating on the same
/// instance of this class is not guaranteed to work. Since copy-construction
/// and move-construction is a relatively efficient operation, consider using
/// such a copy when using this class from multiple threads.
///
class ODBCBQClient {
  public:
    ODBCBQClient() = default;
    ~ODBCBQClient() = default;

    ODBCBQClient(ODBCBQClient const &) = default;
    ODBCBQClient &operator=(ODBCBQClient const &) = default;

    ODBCBQClient(ODBCBQClient &&) = default;
    ODBCBQClient &operator=(ODBCBQClient &&) = default;

};

}  // namespace odbc_bigquery_client_interface
}  // namespace cloud
}  // namespace google

#endif //GOOGLE_CLOUD_ODBC_BQ_DRIVER_CLIENT_INTERFACE_BQ_CLIENT_H
