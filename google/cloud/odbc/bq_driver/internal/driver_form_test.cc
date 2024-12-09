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
  HWND h_proxy_options_button =
      GetDlgItem(form->GetHwnd(), kIdcProxyOptionsButton);
  ASSERT_NE(h_proxy_options_button, nullptr)
      << "Proxy Options button should be created.";

  char buffer[256];
  SendMessage(h_proxy_options_button, WM_GETTEXT, sizeof(buffer),
              (LPARAM)buffer);
  ASSERT_STREQ(buffer, "Proxy Options...")
      << "Proxy Options button should have correct text.";
}

TEST_F(DriverFormTest, TestLoggingOptionsButton) {
  HWND h_logging_button = GetDlgItem(form->GetHwnd(), kIdcLoggingBtn);
  ASSERT_NE(h_logging_button, nullptr)
      << "Logging Options button should be created.";

  char buffer[256];
  SendMessage(h_logging_button, WM_GETTEXT, sizeof(buffer), (LPARAM)buffer);
  ASSERT_STREQ(buffer, "Logging Options...")
      << "Logging Options button should have correct text.";
}

TEST_F(DriverFormTest, TestAdvanceOptionsButton) {
  HWND h_advance_opt_button = GetDlgItem(form->GetHwnd(), kIdcAdvanceOptBtn);
  ASSERT_NE(h_advance_opt_button, nullptr)
      << "Advance Options button should be created.";

  char buffer[256];
  SendMessage(h_advance_opt_button, WM_GETTEXT, sizeof(buffer), (LPARAM)buffer);
  ASSERT_STREQ(buffer, "Advance Options...")
      << "Advance Options button should have correct text.";
}

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

class AdvanceOptionsTest : public ::testing::Test {
 protected:
  AdvanceOptions* advance_options;

  void SetUp() override {
    advance_options = new AdvanceOptions();
    advance_options->InitControls();
  }

  void TearDown() override {
    if (advance_options->GetHwnd() != nullptr) {
      DestroyWindow(advance_options->GetHwnd());
    }
    Sleep(600);
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
  Section attribute_map = {{"LanguageDialect", "Standard SQL"},
                           {"LargeResultsDatasetId", "dataset1"},
                           {"EncryptionKey", "key123"},
                           {"RowsFetchedPerBlock", "500"},
                           {"DefaultStringColumnLength", "10000"},
                           {"LargeResultsTempTableExpirationTime", "3600000"},
                           {"SessionLocation", "USA"},
                           {"AdditionalProjects", "projectA,projectB"},
                           {"QueryProperties", "property1=value1"},
                           {"HTAPI_ActivationThreshold", "10000"}};

  AdvanceOptions options;
  options.SetValues(attribute_map);

  EXPECT_EQ(options.GetLanguageDialect(), "Standard SQL");
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
      {"LanguageDialect", "Standard SQL"},
  };

  AdvanceOptions options;
  options.SetValues(attribute_map);

  EXPECT_EQ(options.GetLanguageDialect(), "Standard SQL");
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
