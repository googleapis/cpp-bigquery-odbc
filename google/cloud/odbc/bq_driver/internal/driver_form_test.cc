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

class LogTraceDialogTest : public ::testing::Test {
 protected:
  void SetUp() override {
    attributes_map_["log_level"] = "LOG_TRACE";
    attributes_map_["log_file_path"] = "C:\\temp\\log.txt";
  }
  LogTraceDialog log_trace_dialog_;
  Section attributes_map_;
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

TEST_F(DriverFormTest, TestButtonClickOK) {
  form->Show();
  ASSERT_NE(form->GetHwnd(), nullptr)
      << "Form window handle should not be null after showing the form.";
  ClickButton(form->GetHwnd(), kIdcButtonOk);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  MSG msg;
  while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  EXPECT_EQ(IsWindow(form->GetHwnd()), FALSE)
      << "Form should be closed when OK button is clicked.";
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

TEST_F(DriverFormTest, TestEmailField) {
  HWND h_email_edit = GetDlgItem(form->GetHwnd(), kIdcEmailEdit);
  ASSERT_NE(h_email_edit, nullptr) << "Email edit control should be created.";

  char const* test_email = "test@example.com";
  SendMessage(h_email_edit, WM_SETTEXT, 0, (LPARAM)test_email);

  char buffer[256];
  SendMessage(h_email_edit, WM_GETTEXT, sizeof(buffer), (LPARAM)buffer);
  ASSERT_STREQ(buffer, test_email)
      << "Email edit control should contain the correct text.";
}

TEST_F(DriverFormTest, SetValues_ValidInput) {
  Section attributes = {{"DSN", "test"},
                        {"Email", "test@example.com"},
                        {"OAuthMechanism", "0"},
                        {"KeyFilePath", "/path/to/key"},
                        {"Catalog", "test_catalog"},
                        {"Dataset", "test_dataset"}};

  form->SetValues(attributes);

  EXPECT_EQ(form->GetEmail(), "test@example.com");
  EXPECT_EQ(form->GetOAuthMechanism(), "Service Authentication");
  EXPECT_EQ(form->GetKeyFilePath(), "/path/to/key");
  EXPECT_EQ(form->GetCatalogName(), "test_catalog");
  EXPECT_EQ(form->GetDatasetName(), "test_dataset");
}

TEST_F(DriverFormTest, SetValues_MissingAttributes) {
  Section attributes = {
      {"Email", "test@example.com"},
      {"OAuthMechanism", "0"},
  };

  form->SetValues(attributes);

  EXPECT_EQ(form->GetEmail(), "test@example.com");
  EXPECT_EQ(form->GetOAuthMechanism(), "Service Authentication");
  EXPECT_EQ(form->GetKeyFilePath(), "");
  EXPECT_EQ(form->GetCatalogName(), "");
  EXPECT_EQ(form->GetDatasetName(), "");
}

TEST_F(DriverFormTest, SetValues_EmptyInput) {
  Section attributes = {};

  form->SetValues(attributes);

  EXPECT_EQ(form->GetEmail(), "");
  EXPECT_EQ(form->GetOAuthMechanism(), "");
  EXPECT_EQ(form->GetKeyFilePath(), "");
  EXPECT_EQ(form->GetCatalogName(), "");
  EXPECT_EQ(form->GetDatasetName(), "");
}

TEST_F(DriverFormTest, TestConnection_SectionIsNull) {
  auto status = DriverForm::TestODBCConnection(nullptr);
  EXPECT_THAT(status, StatusRecIs(SQLStates::k_HY000(),
                                  HasSubstr("The provided section is null.")));
}

TEST_F(DriverFormTest, TestConnection_OAuthMechanismIsMissing) {
  auto section = std::make_shared<Section>();
  (*section)["KeyFilePath"] = "ValidKeyFilePath";
  (*section)["Catalog"] = "CatalogValue";
  auto status = DriverForm::TestODBCConnection(section);
  EXPECT_THAT(status,
              StatusRecIs(SQLStates::k_HY000(),
                          HasSubstr("OAuthMechanism is missing or empty")));
}

TEST_F(DriverFormTest, TestConnection_WrongOAuth) {
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

TEST(LogTraceDialogTest, SetValues_ValidAttributes) {
  LogTraceDialog log_trace_dialog;
  ASSERT_EQ(log_trace_dialog.GetLogLevel(), "");
  ASSERT_EQ(log_trace_dialog.GetLogFilePath(), "");

  Section attributes_map;
  attributes_map["LogLevel"] = "6";
  attributes_map["LogFile"] = "C:\\temp\\log.txt";

  log_trace_dialog.SetValues(attributes_map);
  ASSERT_EQ(log_trace_dialog.GetLogLevel(), "LOG_TRACE");
  ASSERT_EQ(log_trace_dialog.GetLogFilePath(), "C:\\temp\\log.txt");
}

TEST(LogTraceDialogTest, SetValues_InvalidLogLevel) {
  LogTraceDialog log_trace_dialog;
  Section attributes_map;

  attributes_map["LogLevel"] = "999";  // Invalid level
  attributes_map["LogFile"] = "C:\\temp\\log.txt";
  log_trace_dialog.SetValues(attributes_map);

  ASSERT_EQ(log_trace_dialog.GetLogLevel(), "");
  ASSERT_EQ(log_trace_dialog.GetLogFilePath(), "C:\\temp\\log.txt");
}

TEST(LogTraceDialogTest, SetValues_EmptyAttributes) {
  LogTraceDialog log_trace_dialog;
  Section attributes_map;
  log_trace_dialog.SetValues(attributes_map);

  ASSERT_EQ(log_trace_dialog.GetLogLevel(), "");
  ASSERT_EQ(log_trace_dialog.GetLogFilePath(), "");
}
}  // namespace google::cloud::odbc_bq_driver_internal
