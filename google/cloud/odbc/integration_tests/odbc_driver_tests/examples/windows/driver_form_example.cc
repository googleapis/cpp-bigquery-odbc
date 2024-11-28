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

namespace google::cloud::odbc_tests {

namespace {
using google::cloud::odbc_bq_driver_internal::DriverForm;
using google::cloud::odbc_bq_driver_internal::kIdcBrowseButton;
using google::cloud::odbc_bq_driver_internal::kIdcButtonOk;
using google::cloud::odbc_bq_driver_internal::kIdcKeyfileEdit;
using google::cloud::odbc_bq_driver_internal::OpenFileDialog;

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

}  // namespace

TEST_F(DriverFormTest, TestUIOpenAndClose) {
  ASSERT_NE(form->GetHwnd(), nullptr) << "Form window should be created.";

  ProcessMessages();
  std::this_thread::sleep_for(
      std::chrono::milliseconds(500));  // Wait for 500ms

  ASSERT_TRUE(IsWindowVisible(form->GetHwnd()))
      << "Form window should be visible.";

  HWND hOkButton = GetDlgItem(form->GetHwnd(), kIdcButtonOk);
  ASSERT_NE(hOkButton, nullptr) << "OK button should be found.";
  SendMessage(form->GetHwnd(), WM_COMMAND, MAKEWPARAM(kIdcButtonOk, BN_CLICKED),
              (LPARAM)hOkButton);

  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  ASSERT_FALSE(IsWindow(form->GetHwnd()))
      << "Form window should be closed after \
         clicking OK or Cancel.";
}

TEST_F(DriverFormTest, TestFileDialogButton) {
  HWND hBrowseButton = GetDlgItem(form->GetHwnd(), kIdcBrowseButton);
  ASSERT_NE(hBrowseButton, nullptr) << "Browse button should be created.";

  HWND hKeyFileEdit = GetDlgItem(form->GetHwnd(), kIdcKeyfileEdit);
  ASSERT_NE(hKeyFileEdit, nullptr) << "Key file path edit control should \
    be created.";

  SendMessage(form->GetHwnd(), WM_COMMAND,
              MAKEWPARAM(kIdcBrowseButton, BN_CLICKED), (LPARAM)hBrowseButton);

  char const* simulatedFilePath = "C:\\path\\to\\selected\\file.json";
  MockOpenFileDialog(form->GetHwnd(), hKeyFileEdit, simulatedFilePath);

  char buffer[256];
  SendMessage(hKeyFileEdit, WM_GETTEXT, sizeof(buffer), (LPARAM)buffer);
  ASSERT_STREQ(buffer, simulatedFilePath) << "Key file path edit control \
    should have the correct file path after browsing.";
}

}  // namespace google::cloud::odbc_tests

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
