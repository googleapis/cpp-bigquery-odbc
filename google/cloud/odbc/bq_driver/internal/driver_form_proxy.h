// Copyright 2025 Google LLC
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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DRIVER_FORM_PROXY_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DRIVER_FORM_PROXY_H

#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include <windows.h>  // Required for SetWindowSubclass and DefSubclassProc


namespace google::cloud::odbc_bq_driver_internal {
// NEXTID:135

static int const kIdcProxyOKButton = 125;
static int const kIdcProxyCancelButton = 126;
static int const kIdcProxyHostName = 127;
static int const kIdcProxyCheckbox = 128;
static int const kIdcProxyPortEdit = 129;
static int const kIdcProxyUsernameEdit = 130;
static int const kIdcProxyPasswordEdit = 131;
static int const kIdcProxyGroupBox = 132;  
static int const kIdcProxyPortErrorLabel = 133;  // Unique ID for the error label
static int const hFontHyperlink = 134;  


class ProxyOptions {
 public:
  ProxyOptions();
  ~ProxyOptions();
  void InitControls();
  void Show(HWND parent);
  HWND GetHwnd() const;

  void SetValues(Section const& attributes_map);
  inline std::string const& GetProxyCheck() const { return proxy_check_; }
  inline std::string const& GetProxyHost() const { return proxy_host_; }
  inline std::string const& GetProxyPort() const { return proxy_port_; }
  inline std::string const& GetProxyUsername() const { return proxy_username_; }
  inline std::string const& GetProxyPass() const { return proxy_pwd_enc_; }

 private:
  // HWND proxy_hwnd;
  HWND proxy_hwnd = nullptr;  // **Fixed: Initialized to nullptr**
  HWND parent_hwnd = nullptr; // **Fixed: Ensures parent handle is set**

  // // Static function to handle subclassing of EditBox controls
  // static LRESULT CALLBACK EditBoxProc(HWND hwnd, UINT msg, WPARAM w_param,
  //     LPARAM l_param, UINT_PTR u_idSubclass,
  //     DWORD_PTR dwRefData);
  static std::string proxy_check_;
  static std::string proxy_host_;
  static std::string proxy_username_;
  static std::string proxy_port_;
  static std::string proxy_pwd_enc_;


  static LRESULT CALLBACK ProxyOptProc(HWND hwnd, UINT u_msg, WPARAM w_param,
                                       LPARAM ld_param);
  static char const CLASS_NAME[];
};

}  // namespace google::cloud::odbc_bq_driver_internal
#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DRIVER_FORM_PROXY_H
