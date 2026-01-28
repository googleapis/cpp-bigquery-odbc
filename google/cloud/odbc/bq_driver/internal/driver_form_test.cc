// Copyright 2024 Google LLC
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

#include "google/cloud/odbc/bq_driver/internal/driver_form.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_testing_utils::StatusRecIs;
using ::testing::HasSubstr;

class DriverFormTest : public ::testing::Test {
 protected:
  DriverForm* form;

  void SetUp() override {
    form = new DriverForm();
    form->Show();
  }

  void TearDown() override {
    if (form->GetHwnd() != nullptr) {
      DestroyWindow(form->GetHwnd());
    }
    Sleep(600);
    delete form;
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
    ProcessMessages();  // Process any messages resulting from the click
  }
};

void MockOpenFileDialog(HWND hwnd, HWND h_edit, char const* simulated_path) {
  OpenFileDialog(hwnd, h_edit, simulated_path);
}

TEST_F(DriverFormTest, TestUIOpens) {
  ASSERT_NE(form->GetHwnd(), nullptr) << "Form window should be created.";
  ProcessMessages();
  std::this_thread::sleep_for(
      std::chrono::milliseconds(500));  // Wait for 500ms

  ASSERT_TRUE(IsWindowVisible(form->GetHwnd()))
      << "Form window should be visible.";
}

TEST_F(DriverFormTest, TestButtonClickCancel) {
  form->Show();
  ASSERT_NE(form->GetHwnd(), nullptr)
      << "Form window handle should not be null after showing the form.";

  ClickButton(form->GetHwnd(), kIdcButtonCancel);

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  MSG msg;
  while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  EXPECT_EQ(IsWindow(form->GetHwnd()), FALSE)
      << "Form should be closed when Cancel button is clicked.";
}

TEST_F(DriverFormTest, TestAuthDropdown) {
  HWND h_combo_box = GetDlgItem(form->GetHwnd(), kIdcAuthBox);
  ASSERT_NE(h_combo_box, nullptr) << "Auth dropdown should be created.";

  ASSERT_EQ(SendMessage(h_combo_box, CB_GETCOUNT, 0, 0), 2)
      << "Auth dropdown should have 2 items.";

  int selected_index = SendMessage(h_combo_box, CB_GETCURSEL, 0, 0);
  ASSERT_EQ(selected_index, 0) << "First item should be selected by default.";

  char buffer[256];
  SendMessage(h_combo_box, CB_GETLBTEXT, selected_index, (LPARAM)buffer);
  ASSERT_STREQ(buffer, "Service Authentication")
      << "First item text should be 'Service Authentication'.";
}

TEST_F(DriverFormTest, SetValuesValidinput) {
  Section attributes = {{"DSN", "test"},
                        {"OAuthMechanism", "0"},
                        {"KeyFilePath", "/path/to/key"},
                        {"Catalog", "test_catalog"},
                        {"DefaultDataset", "test_dataset"}};

  Section trace_log_attributes = {{"LogLevel", "6"},
                                  {"LogFile", "/path/to/file"}};
  form->SetValues(attributes);
  form->SetLogTraceValues(trace_log_attributes);

  EXPECT_EQ(form->GetOAuthMechanism(), "Service Authentication");
  EXPECT_EQ(form->GetKeyFilePath(), "/path/to/key");
  EXPECT_EQ(form->GetCatalogName(), "test_catalog");
  EXPECT_EQ(form->GetDatasetName(), "test_dataset");
  EXPECT_EQ(form->GetLogLevel(), "LOG_TRACE");
  EXPECT_EQ(form->GetLogFilePath(), "/path/to/file");
}

TEST_F(DriverFormTest, SetValuesCheckcaseinsensitive) {
  Section attributes = {{"DSN", "test"},
                        {"OAuthMechanISM", "0"},
                        {"KeyFilePATH", "/path/to/key"},
                        {"CaTaLoG", "test_catalog"},
                        {"DefAUltDAtaSET", "test_dataset"}};

  form->SetValues(attributes);

  EXPECT_EQ(form->GetOAuthMechanism(), "Service Authentication");
  EXPECT_EQ(form->GetKeyFilePath(), "/path/to/key");
  EXPECT_EQ(form->GetCatalogName(), "test_catalog");
  EXPECT_EQ(form->GetDatasetName(), "test_dataset");
}

TEST_F(DriverFormTest, SetValuesMissingattributes) {
  Section attributes = {
      {"OAuthMechanism", "0"},
  };

  Section trace_log_attributes = {{"LogFile", "/path/to/file"}};
  form->SetValues(attributes);
  form->SetLogTraceValues(trace_log_attributes);

  EXPECT_EQ(form->GetOAuthMechanism(), "Service Authentication");
  EXPECT_EQ(form->GetKeyFilePath(), "");
  EXPECT_EQ(form->GetCatalogName(), "");
  EXPECT_EQ(form->GetDatasetName(), "");
  EXPECT_EQ(form->GetLogLevel(), "");
  EXPECT_EQ(form->GetLogFilePath(), "/path/to/file");
}

TEST_F(DriverFormTest, SetValuesEmptyinput) {
  Section attributes = {};

  form->SetValues(attributes);
  form->SetLogTraceValues({});

  EXPECT_EQ(form->GetOAuthMechanism(), "");
  EXPECT_EQ(form->GetKeyFilePath(), "");
  EXPECT_EQ(form->GetCatalogName(), "");
  EXPECT_EQ(form->GetDatasetName(), "");
  EXPECT_EQ(form->GetLogLevel(), "");
  EXPECT_EQ(form->GetLogFilePath(), "");
}

TEST_F(DriverFormTest, TestConnectionSectionisnull) {
  auto status = DriverForm::TestODBCConnection(nullptr);
  EXPECT_THAT(status, StatusRecIs(SQLStates::k_HY000(),
                                  HasSubstr("The provided section is null.")));
}

TEST_F(DriverFormTest, TestConnectionOauthmechanismismissing) {
  auto section = std::make_shared<Section>();
  (*section)["KeyFilePath"] = "ValidKeyFilePath";
  (*section)["Catalog"] = "CatalogValue";
  auto status = DriverForm::TestODBCConnection(section);
  EXPECT_THAT(status,
              StatusRecIs(SQLStates::k_HY000(),
                          HasSubstr("OAuthMechanism is missing or empty")));
}

TEST_F(DriverFormTest, TestConnectionWrongoauth) {
  auto section = std::make_shared<Section>();
  (*section)["KeyFilePath"] = "ValidKeyFilePath";
  (*section)["OAuthMechanism"] = "OAuthMechanismValue";
  auto status = DriverForm::TestODBCConnection(section);
  EXPECT_THAT(
      status,
      StatusRecIs(SQLStates::k_HY000(),
                  HasSubstr("OAuthMechanism must be 'Service Authentication' "
                            "or 'Application Default Credentials'")));
}
TEST_F(DriverFormTest, GetCatalogAndDatasetInvalidinputforcatalog) {
  auto result = DriverForm::GetCatalogAndDataset("Catalog", "", "");
  EXPECT_FALSE(result.Ok());
  EXPECT_EQ(result.GetStatusRecord().message,
            "Failed to create BigQuery client.");
}
TEST_F(DriverFormTest, GetCatalogAndDatasetInvalidinputfordataset) {
  auto result = DriverForm::GetCatalogAndDataset("DefaultDataset", "", "");
  EXPECT_FALSE(result.Ok());
  EXPECT_EQ(result.GetStatusRecord().message,
            "Failed to create BigQuery client.");
}

TEST_F(DriverFormTest, TestEncryptDataDropdown) {
  HWND h_encrypt_data_combo_box =
      GetDlgItem(form->GetHwnd(), kIdcEncryptDataComboBox);
  ASSERT_NE(h_encrypt_data_combo_box, nullptr)
      << "Encrypt Data dropdown should be created.";

  ASSERT_EQ(SendMessage(h_encrypt_data_combo_box, CB_GETCOUNT, 0, 0), 2)
      << "Encrypt Data dropdown should have 2 items.";

  int selected_index =
      SendMessage(h_encrypt_data_combo_box, CB_GETCURSEL, 0, 0);
  ASSERT_EQ(selected_index, 0) << "First item should be selected by default.";

  char buffer[256];
  SendMessage(h_encrypt_data_combo_box, CB_GETLBTEXT, selected_index,
              (LPARAM)buffer);
  ASSERT_STREQ(buffer, "For Current User Only")
      << "First item text should be 'For Current User Only'.";
}

TEST_F(DriverFormTest, TestMinTLSVersionDropdown) {
  HWND h_min_tls_combo_box = GetDlgItem(form->GetHwnd(), kIdcMinTLSComboBox);
  ASSERT_NE(h_min_tls_combo_box, nullptr)
      << "Minimum TLS Version dropdown should be created.";

  ASSERT_EQ(SendMessage(h_min_tls_combo_box, CB_GETCOUNT, 0, 0), 3)
      << "Minimum TLS Version dropdown should have 3 items.";

  int selected_index = SendMessage(h_min_tls_combo_box, CB_GETCURSEL, 0, 0);
  ASSERT_EQ(selected_index, 2) << "Third item should be selected by default.";

  char buffer[256];
  SendMessage(h_min_tls_combo_box, CB_GETLBTEXT, selected_index,
              (LPARAM)buffer);
  ASSERT_STREQ(buffer, "1.2") << "Selected TLS version should be '1.2'.";
}

TEST_F(DriverFormTest, TestProxyOptionsButton) {
  HWND h_proxy_button = GetDlgItem(form->GetHwnd(), kIdcProxyOptionsButton);
  ASSERT_NE(h_proxy_button, nullptr)
      << "Proxy Options button should be created.";

  char buffer[256];
  GetWindowText(h_proxy_button, buffer, sizeof(buffer));
  ASSERT_STREQ(buffer, "Proxy Options...")
      << "Proxy Options button text should be 'Proxy Options...'.";
}

TEST_F(DriverFormTest, TestTestButtonDisabled) {
  HWND h_test_button = GetDlgItem(form->GetHwnd(), kIdcButtonTest);
  ASSERT_NE(h_test_button, nullptr) << "Test button should be created.";

  ASSERT_FALSE(IsWindowEnabled(h_test_button))
      << "Test button should be disabled by default.";
}

TEST_F(DriverFormTest, TestOKButtonDisabled) {
  HWND h_ok_button = GetDlgItem(form->GetHwnd(), kIdcButtonOk);
  ASSERT_NE(h_ok_button, nullptr) << "OK button should be created.";

  ASSERT_FALSE(IsWindowEnabled(h_ok_button))
      << "OK button should be disabled by default.";
}
TEST_F(DriverFormTest, TestAdvanceOptionsButton) {
  HWND h_advance_opt_button = GetDlgItem(form->GetHwnd(), kIdcAdvanceOptBtn);
  ASSERT_NE(h_advance_opt_button, nullptr)
      << "Advance Options button should be created.";

  ASSERT_TRUE(IsWindowVisible(h_advance_opt_button))
      << "Advance Options button should be visible.";
  ASSERT_TRUE(IsWindowEnabled(h_advance_opt_button))
      << "Advance Options button should be enabled.";

  char buffer[256];
  GetWindowText(h_advance_opt_button, buffer, sizeof(buffer));
  ASSERT_STREQ(buffer, "Advance Options...")
      << "Advance Options button text should be 'Advance Options...'.";
}
TEST_F(DriverFormTest, TestLoggingOptionsButton) {
  HWND h_logging_button = GetDlgItem(form->GetHwnd(), kIdcLoggingBtn);
  ASSERT_NE(h_logging_button, nullptr)
      << "Logging Options button should be created.";

  ASSERT_TRUE(IsWindowVisible(h_logging_button))
      << "Logging Options button should be visible.";
  ASSERT_TRUE(IsWindowEnabled(h_logging_button))
      << "Logging Options button should be enabled.";

  char buffer[256];
  GetWindowText(h_logging_button, buffer, sizeof(buffer));
  ASSERT_STREQ(buffer, "Logging Options...")
      << "Logging Options button text should be 'Logging Options...'.";
}

}  // namespace google::cloud::odbc_bq_driver_internal
