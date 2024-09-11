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
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

#ifdef _WIN32

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

  void ClickButton(HWND hwnd, int buttonId) {
    HWND button = GetDlgItem(hwnd, buttonId);
    ASSERT_NE(button, nullptr) << "Button should be created.";
    SendMessage(button, BM_CLICK, 0, 0);
    ProcessMessages();  // Process any messages resulting from the click
  }
};

void MockOpenFileDialog(HWND hwnd, HWND hEdit, char const* simulatedPath) {
  OpenFileDialog(hwnd, hEdit, simulatedPath);
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
  ClickButton(form->GetHwnd(), IDC_BUTTON_OK);
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

  ClickButton(form->GetHwnd(), IDC_BUTTON_CANCEL);

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
  HWND hComboBox = GetDlgItem(form->GetHwnd(), IDC_COMBOBOX);
  ASSERT_NE(hComboBox, nullptr) << "Auth dropdown should be created.";

  ASSERT_EQ(SendMessage(hComboBox, CB_GETCOUNT, 0, 0), 2)
      << "Auth dropdown should have 2 items.";

  int selectedIndex = SendMessage(hComboBox, CB_GETCURSEL, 0, 0);
  ASSERT_EQ(selectedIndex, 0) << "First item should be selected by default.";

  char buffer[256];
  SendMessage(hComboBox, CB_GETLBTEXT, selectedIndex, (LPARAM)buffer);
  ASSERT_STREQ(buffer, "For Current User")
      << "First item text should be 'For Current User'.";
}

TEST_F(DriverFormTest, TestCatalogDropdown) {
  HWND hCatlogBox = GetDlgItem(form->GetHwnd(), IDC_Catlog_BOX);
  ASSERT_NE(hCatlogBox, nullptr) << "Catalog dropdown should be created.";

  ASSERT_EQ(SendMessage(hCatlogBox, CB_GETCOUNT, 0, 0), 2)
      << "Catalog dropdown should have 2 items.";

  int selectedIndex = SendMessage(hCatlogBox, CB_GETCURSEL, 0, 0);
  ASSERT_EQ(selectedIndex, 0) << "First item should be selected by default.";

  char buffer[256];
  SendMessage(hCatlogBox, CB_GETLBTEXT, selectedIndex, (LPARAM)buffer);
  ASSERT_STREQ(buffer, "Project 1") << "First item text should be 'Project 1'.";
}

TEST_F(DriverFormTest, TestDatasetDropdown) {
  HWND hDatasetBox = GetDlgItem(form->GetHwnd(), IDC_Dataset_BOX);
  ASSERT_NE(hDatasetBox, nullptr) << "Dataset dropdown should be created.";

  ASSERT_EQ(SendMessage(hDatasetBox, CB_GETCOUNT, 0, 0), 2)
      << "Dataset dropdown should have 2 items.";

  int selectedIndex = SendMessage(hDatasetBox, CB_GETCURSEL, 0, 0);
  ASSERT_EQ(selectedIndex, 0) << "First item should be selected by default.";

  char buffer[256];
  SendMessage(hDatasetBox, CB_GETLBTEXT, selectedIndex, (LPARAM)buffer);
  ASSERT_STREQ(buffer, "Dataset 1") << "First item text should be 'Dataset 1'.";
}

TEST_F(DriverFormTest, TestEmailField) {
  HWND hEmailEdit = GetDlgItem(form->GetHwnd(), IDC_EMAIL_EDIT);
  ASSERT_NE(hEmailEdit, nullptr) << "Email edit control should be created.";

  char const* testEmail = "test@example.com";
  SendMessage(hEmailEdit, WM_SETTEXT, 0, (LPARAM)testEmail);

  char buffer[256];
  SendMessage(hEmailEdit, WM_GETTEXT, sizeof(buffer), (LPARAM)buffer);
  ASSERT_STREQ(buffer, testEmail)
      << "Email edit control should contain the correct text.";
}

#endif /* WIN32*/
}  // namespace google::cloud::odbc_bq_driver_internal
