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

#include "google/cloud/odbc/bq_driver/internal/driver_adv_opt_form.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

class AdvanceOptionsTest : public ::testing::Test {
 protected:
  AdvanceOptions* advance_options;

  void SetUp() override {
    advance_options = new AdvanceOptions();
    HFONT h_font =
        CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                   DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    advance_options->CreateLanguageControls(h_font);
    advance_options->CreateLargeResultsControls(h_font);
    advance_options->CreateHighThroughputControls(h_font);
    advance_options->CreateEncryptionControls(h_font);
    advance_options->CreateSessionControls(h_font);
    advance_options->CreateAdditionalControls(h_font);
    advance_options->CreateButtons(h_font);
  }

  void TearDown() override {
    if (advance_options->GetHwnd() != nullptr) {
      DestroyWindow(advance_options->GetHwnd());
    }
    delete advance_options;
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
TEST_F(AdvanceOptionsTest, ShowWindow) {
  HWND hwnd = advance_options->GetHwnd();
  ASSERT_EQ(hwnd, nullptr) << "Window should not be shown initially.";

  advance_options->Show(nullptr);

  hwnd = advance_options->GetHwnd();
  ASSERT_NE(hwnd, nullptr) << "Window should be created and displayed.";

  ShowWindow(hwnd, SW_SHOWNORMAL);
  ASSERT_EQ(IsWindow(hwnd), TRUE)
      << "Window should be visible after calling Show.";
}

TEST_F(AdvanceOptionsTest, SetValues_ValidInput) {
  Section attribute_map = {{"SQLDialect", "1"},
                           {"LargeResultsDatasetId", "dataset1"},
                           {"KMSKeyName", "key123"},
                           {"RowsFetchedPerBlock", "500"},
                           {"DefaultStringColumnLength", "10000"},
                           {"LargeResultsTempTableExpirationTime", "3600000"},
                           {"SessionLocation", "USA"},
                           {"AdditionalProjects", "projectA,projectB"},
                           {"QueryProperties", "property1=value1"},
                           {"HTAPI_ActivationThreshold", "10000"}};

  AdvanceOptions options;
  options.SetValues(attribute_map);

  EXPECT_EQ(options.GetLanguageDialect(), "GoogleSQL");
  EXPECT_EQ(options.GetDatasetName(), "dataset1");
  EXPECT_EQ(options.GetEncryptionKey(), "key123");
  EXPECT_EQ(options.GetRowsPerBlock(), "500");
  EXPECT_EQ(options.GetDefaultStringLength(), "10000");
  EXPECT_EQ(options.GetTempTableExpiration(), "3600000");
  EXPECT_EQ(options.GetSessionLocation(), "USA");
  EXPECT_EQ(options.GetAdditionalProjects(), "projectA,projectB");
  EXPECT_EQ(options.GetQueryProperties(), "property1=value1");
  EXPECT_EQ(options.GetActivationThreshold(), "10000");
}
TEST_F(AdvanceOptionsTest, SetValues_MissingKeys) {
  Section attribute_map = {
      {"SQLDialect", "1"},
  };

  AdvanceOptions options;
  options.SetValues(attribute_map);

  EXPECT_EQ(options.GetLanguageDialect(), "GoogleSQL");
  EXPECT_EQ(options.GetDatasetName(), "");
  EXPECT_EQ(options.GetEncryptionKey(), "");
  EXPECT_EQ(options.GetRowsPerBlock(), "");
  EXPECT_EQ(options.GetDefaultStringLength(), "");
  EXPECT_EQ(options.GetTempTableExpiration(), "");
  EXPECT_EQ(options.GetSessionLocation(), "");
  EXPECT_EQ(options.GetAdditionalProjects(), "");
  EXPECT_EQ(options.GetQueryProperties(), "");
  EXPECT_EQ(options.GetActivationThreshold(), "");
}

}  // namespace google::cloud::odbc_bq_driver_internal
