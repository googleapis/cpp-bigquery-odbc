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

#include "google/cloud/odbc/bq_driver/internal/driver_form_proxy.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

class ProxyOptionsTest : public ::testing::Test {
 protected:
  ProxyOptions* proxy_options;

  void SetUp() override {
    proxy_options = new ProxyOptions();
    proxy_options->InitControls();
  }

  void TearDown() override {
    if (proxy_options->GetHwnd() != nullptr) {
      DestroyWindow(proxy_options->GetHwnd());
    }
    Sleep(600);
    delete proxy_options;
  }

  void ProcessMessages() {
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  void ClickButton(HWND hwnd, int button_id) {
    HWND button = GetDlgItem(hwnd, button_id);
    ASSERT_NE(button, nullptr) << "Button should be created.";
    SendMessage(button, BM_CLICK, 0, 0);
    ProcessMessages();
  }
};

TEST_F(ProxyOptionsTest, ShowWindow) {
  HWND hwnd = proxy_options->GetHwnd();
  ASSERT_EQ(hwnd, nullptr) << "Window should not be shown initially.";

  proxy_options->Show(nullptr);

  hwnd = proxy_options->GetHwnd();
  ASSERT_NE(hwnd, nullptr) << "Window should be created and displayed.";

  ShowWindow(hwnd, SW_SHOWNORMAL);
  ASSERT_EQ(IsWindow(hwnd), TRUE)
      << "Window should be visible after calling Show.";
}
}  // namespace google::cloud::odbc_bq_driver_internal
