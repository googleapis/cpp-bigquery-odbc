

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_INTERNAL_COMMONS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_INTERNAL_COMMONS_H

#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"

namespace google::cloud::odbc_internal {
using ::google::cloud::odbc_bq_driver_internal::LogLevel;



bool ShouldLog(LogLevel level);
}  // namespace google::cloud::odbc_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_INTERNAL_COMMONS_H
