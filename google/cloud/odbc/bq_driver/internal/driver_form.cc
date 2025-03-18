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
#include "google/cloud/odbc/bq_client_interface/odbc_authentication.h"
#include "google/cloud/odbc/bq_client_interface/odbc_bq_client.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_tables.h"
#include "google/cloud/odbc/bq_driver/internal/driver_adv_opt_form.h"
#include <regex>
#include <shellapi.h>

namespace google::cloud::odbc_bq_driver_internal {
using google::cloud::odbc_bigquery_client_interface::Oauth;
using google::cloud::odbc_bigquery_client_interface::OauthMechanism;
using google::cloud::odbc_bigquery_client_interface::ODBCBQClient;
using google::cloud::odbc_bq_driver_internal::Authentication;
using google::cloud::odbc_bq_driver_internal::GetResultSetForDatasets;
using google::cloud::odbc_bq_driver_internal::GetResultSetForProjects;
using google::cloud::odbc_bq_driver_internal::ResultSet;
using google::cloud::odbc_bq_driver_internal::Section;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

char const DriverForm::CLASS_NAME[] = "DriverFormClass";
constexpr char kBigQueryDocsURL[] = "https://cloud.google.com/bigquery/docs/reference/odbc-jdbc-drivers?hl=en";
std::string DriverForm::dsn_name_;
std::string DriverForm::key_file_path_;
std::string DriverForm::o_auth_mechanism_;
std::string DriverForm::catalog_;
std::string DriverForm::dataset_;
std::string DriverForm::encrypt_data_;
std::string DriverForm::min_tls_version_;
std::string DriverForm::trusted_cert_;
std::string DriverForm::description_;
std::string const kDsnName = "DSN";
std::string const kEmail = "Email";
std::string const kOAuthMechanism = "OAuthMechanism";
std::string const kKeyFilePath = "KeyFilePath";
std::string const kCatalog = "Catalog";
std::string const kDataset = "Dataset";
std::string const kEncryptData = "EncryptData";
std::string const kDescription = "Description";
std::string const kMinTlsVersion = "Min_TLS";
std::string const kTrustedCerts = "TrustedCerts";
std::string const kRefreshToken = "RefreshToken";

// Control dimensions and positions
int const kBtnWidth = 68;
int const kBtnHeight = 17;
int const kEditComboBoxWidth = 203;
int const kEditComboBoxHeight = 17;
int const kLabelWidth = 150;
int const kLabelHeight = 20;
int const kCheckboxWidth = 15;
int const kCheckboxHeight = 15;
int const kAxisY = 15;
int const kAxisX = 15;
int const KOptionsBtnHeight = 105;
int const KGroupBoxWidth = 478;
int const KGroupBoxHeight = 129;


SQLRETURN ConnectUsingRegistryDsn(Authentication auth) {
  StatusRecordOr<std::shared_ptr<ODBCBQClient>> response =
      ODBCBQClient::CreateBQClient(auth.oauth);
  if (!response) {
    return SQL_ERROR;
  }
  auto client = *response;

  StatusRecordOr<AccessToken> access_token_resp = client->GetOAuth2Token();
  if (!access_token_resp) {
    return SQL_ERROR;
  }
  return SQL_SUCCESS;
}
Authentication CreateAuthentication(Section& dsn_section) {
  Authentication auth;
  int auth_int;
  try {
    auth_int = stoi(dsn_section[kOAuthMechanism]);
  } catch (std::exception const& ex) {
    auth_int = 0;
  }
  auth.oauth.auth_mechanism = static_cast<OauthMechanism>(auth_int);
  // TODO(shivamd-gpartner): DSN section entries should be capitalized
  // to be consistent with ConnectionHandle::SetUp function.
  auth.oauth.credentials_file_path = dsn_section[kKeyFilePath];
  // TODO(shivamd-gpartner): DSN section entries should be capitalized to be
  // consistent with ConnectionHandle::SetUp function.
  auth.refresh_token = dsn_section[kRefreshToken];
  return auth;
}

StatusRecord DriverForm::TestODBCConnection(
    std::shared_ptr<Section> const& section) {
  if (!section) {
    return StatusRecord{SQLStates::k_HY000(), "The provided section is null."};
  }

  if (section->find(kKeyFilePath) == section->end() ||
      (*section)[kKeyFilePath].empty()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "KeyFilePath is missing or empty."};
  }

  if (section->find(kOAuthMechanism) == section->end() ||
      (*section)[kOAuthMechanism].empty()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "OAuthMechanism is missing or empty."};
  }

  std::string oauth_mechanism = (*section)[kOAuthMechanism];
  std::string oauth_value;
  if (oauth_mechanism == "Service Authentication") {
    oauth_value = std::to_string(
        static_cast<int>(OauthMechanism::kServiceAndUserAccount));
  } else if (oauth_mechanism == "Application Default Credentials") {
    oauth_value =
        std::to_string(static_cast<int>(OauthMechanism::kApplicationDefault));
  } else {
    return StatusRecord{SQLStates::k_HY000(),
                        "OAuthMechanism must be 'Service Authentication' or "
                        "'Application Default Credentials'."};
  }

  (*section)[kOAuthMechanism] = oauth_value;

  std::string key_file_path = (*section)[kKeyFilePath];
  std::string key_file_path_up;
  for (char ch : key_file_path) {
    if (ch == '\\') {
      key_file_path_up += "\\\\";
    } else {
      key_file_path_up += ch;
    }
  }

  Authentication auth = CreateAuthentication(*section);

  SQLRETURN ret = ConnectUsingRegistryDsn(auth);

  if (!SQL_SUCCEEDED(ret)) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Failed to establish ODBC connection."};
  }

  return StatusRecord::Ok();
}

bool containsAlphanumeric(std::string const& str) {
  return std::any_of(str.begin(), str.end(),
                     [](unsigned char c) { return std::isalnum(c); });
}

StatusRecordOr<std::string> DriverForm::GetCatalogAndDataset(
    std::string const& action, std::string const& key_file_path,
    std::string const& oauth_token, std::string const& catalog_name) {
  google::cloud::odbc_bigquery_client_interface::OauthMechanism oauth_value;

  // TODO(b/383592420): Add call to user auth once its tested
  if (oauth_token == "Service Authentication") {
    oauth_value = google::cloud::odbc_bigquery_client_interface::
        OauthMechanism::kServiceAndUserAccount;
  } else if (oauth_token == "Application Default Credentials") {
    oauth_value = google::cloud::odbc_bigquery_client_interface::
        OauthMechanism::kApplicationDefault;
  } else {
    oauth_value = google::cloud::odbc_bigquery_client_interface::
        OauthMechanism::kExternalUser;
  }

  SQLULEN metadata_id = 0;
  auto bq_client_ptr =
      ODBCBQClient::CreateBQClient({oauth_value, key_file_path});
  if (!bq_client_ptr) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Failed to create BigQuery client."};
  }

  ODBCBQClient& bq_client = **bq_client_ptr;

  StatusRecordOr<ResultSet> result_set_status;
  if (action == "Catalog") {
    result_set_status = GetResultSetForProjects(bq_client, metadata_id);
  } else if (action == "Dataset") {
    result_set_status =
        GetResultSetForDatasets(bq_client, metadata_id, catalog_name);
  }

  if (!result_set_status.Ok()) {
    return StatusRecord{SQLStates::k_HY000(), "Failed to fetch result set."};
  }

  ResultSet const& result_set = *result_set_status;
  std::string row_string;
  for (auto const& row : result_set.rows) {
    for (auto const& value : row) {
      std::string value_string(value.begin(), value.end());
      value_string.erase(remove(value_string.begin(), value_string.end(), ' '),
                         value_string.end());
      if (!value_string.empty() && containsAlphanumeric(value_string)) {
        row_string += value_string + ";";
      }
    }
  }

  return row_string;
}

int WINAPI wWinMain(HINSTANCE h_instance, HINSTANCE h_prev_instance,
                    PWSTR p_cmd_line, int n_cmd_show) {
  DriverForm DriverForm;
  DriverForm.Show();

  // Run the message loop
  MSG msg = {};
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return 0;
}

HWND DriverForm::GetHwnd() const { return m_hwnd; }
DriverForm::DriverForm(HWND parent_hwnd)
    : m_hwnd(NULL), m_parent_hwnd(parent_hwnd) {}

DriverForm::~DriverForm() {
  if (m_hwnd) {
    DestroyWindow(m_hwnd);
  }
}

void OpenFileDialog(HWND hwnd, HWND h_edit,
                    char const* mock_file_path = nullptr) {
  if (mock_file_path) {
    // Directly set the test file path to the edit control if provided
    SetWindowText(h_edit, mock_file_path);
    return;
  }
  OPENFILENAME ofn;
  char sz_file[260] = {0};  // Buffer for file path

  // Initialize OPENFILENAME structure
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hwnd;
  ofn.lpstrFile = sz_file;
  ofn.nMaxFile = sizeof(sz_file);
  ofn.lpstrFilter = "JSON Files\0*.JSON\0All Files\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.lpstrFileTitle = NULL;
  ofn.nMaxFileTitle = 0;
  ofn.lpstrInitialDir = NULL;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

  // Display the Open File dialog
  if (GetOpenFileName(&ofn)) {
    // Set the selected file path to the edit control
    SetWindowText(h_edit, sz_file);
  }
}

void DriverForm::SetValues(Section const& attributes_map) {
  dsn_name_ = GetValueOrDefault(attributes_map, kDsnName);
  key_file_path_ = GetValueOrDefault(attributes_map, kKeyFilePath);
  catalog_ = GetValueOrDefault(attributes_map, kCatalog);
  dataset_ = GetValueOrDefault(attributes_map, kDataset);
  encrypt_data_ = GetValueOrDefault(attributes_map, kEncryptData);
  description_ = GetValueOrDefault(attributes_map, kDescription);
  min_tls_version_ = GetValueOrDefault(attributes_map, kMinTlsVersion);
  trusted_cert_ = GetValueOrDefault(attributes_map, kTrustedCerts);
  // Construct a message string to display
std::string message = "Catalog Retrieved: " + catalog_ + "\nDataset Retrieved: " + dataset_;

// Convert to LPCSTR for MessageBox
MessageBox(NULL, message.c_str(), "Debug Info", MB_OK | MB_ICONINFORMATION);


  std::string oauth_value = GetValueOrDefault(attributes_map, kOAuthMechanism);
  if (oauth_value == std::to_string(static_cast<int>(
                         OauthMechanism::kServiceAndUserAccount))) {
    o_auth_mechanism_ = "Service Authentication";
  } else if (oauth_value == std::to_string(static_cast<int>(
                                OauthMechanism::kApplicationDefault))) {
    o_auth_mechanism_ = "Application Default Credentials";
  } else {
    o_auth_mechanism_ = "";
  }
}

HFONT CreateCustomFont(int font_size) {
  LOGFONT log_font = {};
  HFONT h_font = NULL;
  log_font.lfHeight = -MulDiv(font_size, GetDeviceCaps(GetDC(NULL), LOGPIXELSY),
                              72);        // Negative height for screen fonts
  lstrcpy(log_font.lfFaceName, "Inter");  // Font face name

  h_font = CreateFontIndirect(&log_font);
  return h_font;
}

// Helper function to set font for controls
void SetControlFont(HWND hwnd, HFONT font) {
  SendMessage(hwnd, WM_SETFONT, (WPARAM)font, TRUE);
}
void DriverForm::InitControls() {
  HFONT h_font = CreateFont(
    -10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
    OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
    DEFAULT_PITCH | FF_SWISS, "Inter");
  HBRUSH hBrushGrey = CreateSolidBrush(RGB(224, 224, 224)); // -10 is equal to 10px

  HWND h_dsn_name_header = CreateLabel(m_hwnd, "Data source name:", kAxisX, kAxisY, kLabelWidth,
    kLabelHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_dsn_name_header, WM_SETFONT, (WPARAM)h_font, TRUE);                                    
  HWND h_dsn_name_edit = CreateEditBox(m_hwnd, kAxisX+170, kAxisY, kEditComboBoxWidth, kEditComboBoxHeight, kIdcDSNEdit);
  SendMessage(h_dsn_name_edit, WM_SETFONT, (WPARAM)h_font, TRUE);                                    
  
  SetWindowText(h_dsn_name_edit, dsn_name_.c_str());
  if (!dsn_name_.empty()) {
    SendMessage(h_dsn_name_edit, EM_SETREADONLY, TRUE, 0);
  }

  HWND h_description_header = CreateLabel(m_hwnd, "Description:", kAxisX,kAxisY+28, kLabelWidth-50,
    kLabelHeight, WS_VISIBLE | SS_LEFT);
   SendMessage(h_description_header, WM_SETFONT, (WPARAM)h_font, TRUE);                                    
                                       
  HWND h_description_edit =
      CreateEditBox(m_hwnd, kAxisX+170, kAxisY+28, kEditComboBoxWidth, kEditComboBoxHeight, kIdcDescriptionEdit);
      SendMessage(h_description_edit, WM_SETFONT, (WPARAM)h_font, TRUE);                                    

  HWND h_encrypt_data_header =
      CreateLabel(m_hwnd, "Encrypt sensitive data:", kAxisX, kAxisY+56, kLabelWidth, kLabelHeight,
                  WS_VISIBLE | SS_LEFT);
      SendMessage(h_encrypt_data_header, WM_SETFONT, (WPARAM)h_font, TRUE);                                    
            
  HWND h_encrypt_data_combo_box =
      CreateComboBox(m_hwnd, kAxisX+170, kAxisY+56, kEditComboBoxWidth, kEditComboBoxHeight, kIdcEncryptDataComboBox);
      SendMessage(h_encrypt_data_combo_box, WM_SETFONT, (WPARAM)h_font, TRUE);                                    
   
      HWND h_group_box = CreateGroupBox(
        m_hwnd, "Authentication",
        kAxisX-5, kAxisY+90, KGroupBoxWidth, KGroupBoxHeight,
        kIdcGroupBox);    
        SendMessage(h_group_box, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_auth_header = CreateLabel(m_hwnd, "OAuth mechanism:", kAxisX+5, kAxisY+115, kLabelWidth, kLabelHeight,
                                   WS_VISIBLE | SS_LEFT);
      SendMessage(h_auth_header, WM_SETFONT, (WPARAM)h_font, TRUE);                                    

  HWND h_auth_combo_box =
      CreateComboBox(m_hwnd, kAxisX+170, kAxisY+115, kEditComboBoxWidth, kEditComboBoxHeight, kIdcAuthBox);
      SendMessage(h_auth_combo_box, WM_SETFONT, (WPARAM)h_font, TRUE);                                                                              

  HWND h_key_file_path_header = CreateLabel(m_hwnd, "Key file path:", kAxisX+5, kAxisY+145,
    kLabelWidth-50, kLabelHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_key_file_path_header, WM_SETFONT, (WPARAM)h_font, TRUE);                                    
                                         
  HWND h_key_file_edit =
      CreateEditBox(m_hwnd,kAxisX+170, kAxisY+145, kEditComboBoxWidth, kEditComboBoxHeight, kIdcKeyfileEdit);
      SendMessage(h_key_file_edit, WM_SETFONT, (WPARAM)h_font, TRUE);                                    

  HWND h_browse_button =
      CreateButton(m_hwnd, "Browse...",kAxisX+170, kAxisY+170, kBtnWidth+8, kBtnHeight, kIdcBrowseButton);
      SendMessage(h_browse_button, WM_SETFONT, (WPARAM)h_font, TRUE);
      
  // New Checkbox for Google Drive Scope Access
  HWND h_drive_scope_checkbox = CreateCheckBox(
    m_hwnd, "", kAxisX + 5, kAxisY + 195, kCheckboxWidth, kCheckboxHeight, kIdcDriveScopeCheckbox);
SendMessage(h_drive_scope_checkbox, WM_SETFONT, (WPARAM)h_font, TRUE);

// New Label for Google Drive Scope Access
HWND h_drive_scope_label = CreateLabel(m_hwnd, "Request Google Drive scope access", 
  kAxisX+25, kAxisY + 195, kLabelWidth+70, kLabelHeight, WS_VISIBLE | SS_LEFT);
SendMessage(h_drive_scope_label, WM_SETFONT, (WPARAM)h_font, TRUE);   

HWND h_ssl_header = CreateGroupBox(
  m_hwnd, "SSL Options", kAxisX - 5, kAxisY + 250, KGroupBoxWidth, KGroupBoxHeight - 4, kIdcGroupBox);
 SendMessage(h_ssl_header, WM_SETFONT, (WPARAM)h_font, TRUE);                                        

  HWND h_min_tls_header = CreateLabel(m_hwnd, "Minimum TLS version:", kAxisX+5, kAxisY+275,
    kLabelWidth, kLabelHeight, WS_VISIBLE | SS_LEFT);
       SendMessage(h_min_tls_header, WM_SETFONT, (WPARAM)h_font, TRUE);                                    
                               
  HWND h_min_tls_combo_box =
      CreateComboBox(m_hwnd, kAxisX+170, kAxisY+275, kEditComboBoxWidth, kEditComboBoxHeight, kIdcMinTLSComboBox);
      SendMessage(h_min_tls_combo_box, WM_SETFONT, (WPARAM)h_font, TRUE);
      
// New Checkbox for Use system trust store
HWND h_system_trust_store_checkbox = CreateCheckBox(
  m_hwnd, "", kAxisX+5, kAxisY+300, kCheckboxWidth, kCheckboxHeight, kIdcSystemTrustStoreCheckbox);

SendMessage(h_system_trust_store_checkbox, WM_SETFONT, (WPARAM)h_font, TRUE);

// New Label for Use system trust store
HWND h_system_trust_label = CreateLabel(m_hwnd, "Use system trust store", 
  kAxisX+25, kAxisY+300, kLabelWidth+70, kLabelHeight, WS_VISIBLE | SS_LEFT);
SendMessage(h_system_trust_label, WM_SETFONT, (WPARAM)h_font, TRUE);
      
  HWND h_trusted_cert_header = CreateLabel(m_hwnd, "Trusted certificate:", kAxisX+5,
    kAxisY+325, kLabelWidth, kLabelHeight, WS_VISIBLE | SS_LEFT);
      SendMessage(h_trusted_cert_header, WM_SETFONT, (WPARAM)h_font, TRUE);                                    
                                    
  HWND h_trusted_cert_edit =
      CreateEditBox(m_hwnd, kAxisX+170, kAxisY+325, kEditComboBoxWidth, kEditComboBoxHeight, kIdcTrustedCertEdit);
      SendMessage(h_trusted_cert_edit, WM_SETFONT, (WPARAM)h_font, TRUE);                                    


  HWND h_trusted_cert_browse_button = CreateButton(
      m_hwnd, "Browse...", kAxisX+170, kAxisY+350, kBtnWidth+8, kBtnHeight, kIdcTrustedCertBrowseButton);
      SendMessage(h_trusted_cert_browse_button, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_catalog_header =
      CreateLabel(m_hwnd, "Project* :", kAxisX, kAxisY+385, kLabelWidth, kLabelHeight, WS_VISIBLE | SS_LEFT);
      SendMessage(h_catalog_header, WM_SETFONT, (WPARAM)h_font, TRUE);                                    

  HWND h_catalog_box =
      CreateComboBox(m_hwnd, kAxisX+170, kAxisY+385, kEditComboBoxWidth, kEditComboBoxHeight, kIdcCatlogBOX);
      SendMessage(h_catalog_box, WM_SETFONT, (WPARAM)h_font, TRUE);                                    

  HWND h_dataset_header =
      CreateLabel(m_hwnd, "Dataset:", kAxisX, kAxisY+413, kLabelWidth, kLabelHeight, WS_VISIBLE | SS_LEFT);
      SendMessage(h_dataset_header, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_dataset_box =
      CreateComboBox(m_hwnd, kAxisX+170, kAxisY+413, kEditComboBoxWidth, kEditComboBoxHeight, kIdcDatasetBOX);
      SendMessage(h_dataset_box, WM_SETFONT, (WPARAM)h_font, TRUE);  
  
    // Documentation Hyperlink
    HWND h_doc_text = CreateLabel(m_hwnd, "Not sure what to select? See", kAxisX,kAxisY+443 , kLabelWidth+10, kLabelHeight, 0);
    SendMessage(h_doc_text, WM_SETFONT, (WPARAM)h_font, TRUE);
    
    HWND h_hyperlink = CreateHyperlinkLabel(m_hwnd, "BigQuery documentation",
      kAxisX+135, kAxisY+443, kLabelWidth, kLabelHeight, 
      kIdcHyperlink);
    SendMessage(h_hyperlink, WM_SETFONT, (WPARAM)h_font, TRUE);    

  HWND h_proxy_options_button = CreateButton(
      m_hwnd, "Proxy options...", kAxisX, kAxisY+473, KOptionsBtnHeight, kBtnHeight, kIdcProxyOptionsButton);
      SendMessage(h_proxy_options_button, WM_SETFONT, (WPARAM)h_font, TRUE);                                    

  HWND h_login_button = CreateButton(m_hwnd, "Logging options...", kAxisX+154, kAxisY+473,
                                     KOptionsBtnHeight, kBtnHeight, kIdcLoggingBtn);     
      SendMessage(h_login_button, WM_SETFONT, (WPARAM)h_font, TRUE); 
                                        
  HWND h_advance_opt_button = CreateButton(m_hwnd, "Advance options...", kAxisX+308,
    kAxisY+473, KOptionsBtnHeight, kBtnHeight, kIdcAdvanceOptBtn);
      SendMessage(h_advance_opt_button, WM_SETFONT, (WPARAM)h_font, TRUE);
      
  HWND h_version_text = CreateLabel(m_hwnd, "v3.0.5.1011 (64 bit)", kAxisX,kAxisY+505 , kLabelWidth-55, kLabelHeight-8, 0);
       SendMessage(h_version_text, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_test_button =
      CreateButton(m_hwnd, "Test...", kAxisX+201, kAxisY+505, kBtnWidth, kBtnHeight, kIdcButtonTest);
      SendMessage(h_test_button, WM_SETFONT, (WPARAM)h_font, TRUE);                                    
      EnableWindow(h_test_button, FALSE);
      
  HWND h_ok_button = CreateButton(m_hwnd, "OK",kAxisX+291, kAxisY+505, kBtnWidth, kBtnHeight, kIdcButtonOk);
  SendMessage(h_ok_button, WM_SETFONT, (WPARAM)h_font, TRUE);                           
  EnableWindow(h_ok_button, FALSE);

  HWND h_cancel_button =
      CreateButton(m_hwnd, "Cancel", kAxisX+381, kAxisY+505, kBtnWidth, kBtnHeight, kIdcButtonCancel);
      SendMessage(h_cancel_button, WM_SETFONT, (WPARAM)h_font, TRUE); 

  SendMessage(h_encrypt_data_combo_box, CB_ADDSTRING, 0,
              (LPARAM) "For Current User Only");
  SendMessage(h_encrypt_data_combo_box, CB_ADDSTRING, 0,
              (LPARAM) "For All Users");
  SendMessage(h_encrypt_data_combo_box, CB_SETCURSEL, 0, 0);

  SendMessage(h_auth_combo_box, CB_ADDSTRING, 0,
              (LPARAM) "Service Authentication");
  SendMessage(h_auth_combo_box, CB_ADDSTRING, 0,
              (LPARAM) "Application Default Credentials");
  SendMessage(h_auth_combo_box, CB_SETCURSEL, 0, 0);

  SendMessage(h_min_tls_combo_box, CB_ADDSTRING, 0, (LPARAM) "1.0");
  SendMessage(h_min_tls_combo_box, CB_ADDSTRING, 0, (LPARAM) "1.1");
  SendMessage(h_min_tls_combo_box, CB_ADDSTRING, 0, (LPARAM) "1.2");
  SendMessage(h_min_tls_combo_box, CB_SETCURSEL, 2, 0);

  SetWindowText(h_key_file_edit, key_file_path_.c_str());
  SetWindowText(h_catalog_box, catalog_.c_str());
  SetWindowText(h_dataset_box, dataset_.c_str());
  SetWindowText(h_auth_combo_box, o_auth_mechanism_.c_str());
  SetWindowText(h_description_edit, description_.c_str());
  SetWindowText(h_trusted_cert_edit, trusted_cert_.c_str());
  SetWindowText(h_min_tls_combo_box, min_tls_version_.c_str());
  SetWindowText(h_encrypt_data_combo_box, encrypt_data_.c_str());
}
// Function to initialize and display the form
void DriverForm::Show() {
  if (m_hwnd) {
    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
    return;
  }

  WNDCLASS wc = {};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.lpszClassName = CLASS_NAME;

  RegisterClass(&wc);
  int window_width = 506;
  int window_height = 588;

  int screen_width = GetSystemMetrics(SM_CXSCREEN);
  int screen_height = GetSystemMetrics(SM_CYSCREEN);

  int xPos = (screen_width - window_width) / 2;
  int yPos = (screen_height - window_height) / 2;

  m_hwnd = CreateWindowEx(
    WS_EX_TOPMOST, CLASS_NAME, "BigQuery ODBC Driver data source setup",
      WS_OVERLAPPEDWINDOW, xPos, yPos, window_width, window_height, NULL, NULL,
      GetModuleHandle(NULL), this);

  if (m_hwnd) {
    InitControls();

    // Create and position OK and Cancel buttons at the bottom
    RECT rc_client;
    GetClientRect(m_hwnd, &rc_client);

    int button_width = 100;
    int button_height = 30;
    int button_y = rc_client.bottom - button_height -
                   20;        // Position 20 pixels from the bottom
    int button_spacing = 20;  // Space between buttons

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
  }
}

StatusRecord DriverForm::IsValidEmail(std::string const& email) {
  std::regex const pattern(R"((\w+)(\.|\-)?(\w*)@(\w+)(\.\w+)+)");
  if (std::regex_match(email, pattern)) {
    return StatusRecord::Ok();
  } else {
    return StatusRecord{SQLStates::k_HY000(),
                        "Email does not match the required pattern."};
  }
}

void EvaluateFields(HWND hwnd) {
  char dsn_buffer[256] = {0};
  char key_buffer[256] = {0};
  char auth_buffer[256] = {0};
  char catalog_buffer[256] = {0};

  GetWindowText(GetDlgItem(hwnd, kIdcDSNEdit), dsn_buffer, sizeof(dsn_buffer));
  GetWindowText(GetDlgItem(hwnd, kIdcKeyfileEdit), key_buffer,
                sizeof(key_buffer));
  GetWindowText(GetDlgItem(hwnd, kIdcAuthBox), auth_buffer,
                sizeof(auth_buffer));
  GetWindowText(GetDlgItem(hwnd, kIdcCatlogBOX), catalog_buffer,
                sizeof(catalog_buffer));
  BOOL enable = dsn_buffer[0] != '\0' && key_buffer[0] != '\0' &&
                auth_buffer[0] != '\0' && catalog_buffer[0] != '\0';

  EnableWindow(GetDlgItem(hwnd, kIdcButtonOk), enable);
  EnableWindow(GetDlgItem(hwnd, kIdcButtonTest), enable);
}

void PopulateDropdown(HWND h_dataset_box, std::string text,
                      std::string key_file, std::string oauth,
                      std::string catalog) {
  SendMessage(h_dataset_box, CB_RESETCONTENT, 0, 0);

  StatusRecordOr<std::string> status_record =
      DriverForm::GetCatalogAndDataset(text, key_file, oauth, catalog);

  if (!status_record.Ok()) {
    MessageBox(h_dataset_box, status_record.GetStatusRecord().message.c_str(),
               "Error", MB_OK | MB_ICONERROR);
    return;
  }

  std::string row_string = status_record.GetValue();

  std::vector<std::string> values = Split(row_string, ";");

  for (auto const& value : values) {
    if (!value.empty()) {
      SendMessage(h_dataset_box, CB_ADDSTRING, 0,
                  reinterpret_cast<LPARAM>(value.c_str()));
    }
  }
}

void RetrieveFieldText(HWND hwnd, int control_id, char* buffer,
                       size_t buffer_size) {
  HWND h_control = GetDlgItem(hwnd, control_id);
  GetWindowText(h_control, buffer, buffer_size);
}

void HandleDropdown(HWND hwnd, int control_id, char const* field_type,
                    char const* key_buffer, char const* auth_buffer,
                    char const* catalog_buffer = "") {
  HWND h_control = GetDlgItem(hwnd, control_id);
  if (key_buffer[0] && auth_buffer[0] &&
      (strcmp(field_type, "Catalog") == 0 || catalog_buffer[0])) {
    PopulateDropdown(h_control, field_type, key_buffer, auth_buffer,
                     catalog_buffer);
  }
}

void HandleSelectionChange(HWND hwnd, int control_id) {
  HWND h_control = GetDlgItem(hwnd, control_id);
  int selected_index = SendMessage(h_control, CB_GETCURSEL, 0, 0);
  if (selected_index != CB_ERR) {
    char selected_value[256];
    SendMessage(h_control, CB_GETLBTEXT, selected_index,
                reinterpret_cast<LPARAM>(selected_value));
    SetWindowText(h_control, selected_value);
  }
}
LRESULT CALLBACK DriverForm::WindowProc(HWND hwnd, UINT u_msg, WPARAM w_param,
                                        LPARAM l_param) {
  DriverForm* p_this =
      reinterpret_cast<DriverForm*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
      static HBRUSH hBrush = CreateSolidBrush(RGB(224, 224, 224)); // #E0E0E0 color         
      
  switch (u_msg) {
    case WM_CREATE:
      // Set the instance pointer in the window's user data
      SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(p_this));
      break;
    case WM_CTLCOLORBTN: {
        HDC hdcButton = (HDC)w_param;
        SetBkMode(hdcButton, TRANSPARENT);
        return (LRESULT)hBrush; // Set button background color
      }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)w_param;
        RECT rc;
        GetClientRect(hwnd, &rc);
    
        // Define a custom background color (#F0F0F0)
        HBRUSH hBrush = CreateSolidBrush(RGB(240, 240, 240));
    
        FillRect(hdc, &rc, hBrush);
        DeleteObject(hBrush);
        return 1; // Indicate we handled the background redraw
    }
    case WM_LBUTTONDOWN: {
      POINT pt;
      GetCursorPos(&pt);
      ScreenToClient(hwnd, &pt);
  
      HWND h_hyperlink = GetDlgItem(hwnd, kIdcHyperlink);
      RECT rect;
      GetClientRect(h_hyperlink, &rect);
      MapWindowPoints(h_hyperlink, hwnd, (LPPOINT)&rect, 2);
  
      if (PtInRect(&rect, pt)) {
        ShellExecute(NULL, "open", kBigQueryDocsURL, NULL, NULL, SW_SHOWNORMAL);
      }
      break;
    }
    case WM_CTLCOLORSTATIC: {
      HDC hdcStatic = (HDC)w_param;
      HWND hwndStatic = (HWND)l_param;
  
      if (GetDlgCtrlID(hwndStatic) == kIdcHyperlink) {
          SetTextColor(hdcStatic, RGB(0, 0, 255));  // Set text color to blue
          SetBkMode(hdcStatic, TRANSPARENT);  // Transparent background
  
          static HFONT hFontUnderline = NULL;
          if (!hFontUnderline) {
              LOGFONT lf;
              HFONT hFont = (HFONT)SendMessage(hwndStatic, WM_GETFONT, 0, 0);
              if (hFont && GetObject(hFont, sizeof(LOGFONT), &lf)) {
                  lf.lfUnderline = TRUE;  // Enable underline
                  hFontUnderline = CreateFontIndirect(&lf);
              }
          }
  
          if (hFontUnderline) {
              SelectObject(hdcStatic, hFontUnderline);
          }
  
          return (LRESULT)GetSysColorBrush(COLOR_3DFACE);
      }
      break;
  }
    case WM_KEYDOWN:
      if (w_param == VK_ESCAPE) {
        SendMessage(hwnd, WM_CLOSE, 0, 0);
        return 0;
      }
      if (w_param == VK_RETURN) {
        SendMessage(hwnd, WM_COMMAND, kIdcButtonOk, 0);
        return 0;
      }
      if (w_param == VK_TAB) {
        return 0;
      }
      break;  

    case WM_COMMAND:
      if (HIWORD(w_param) == EN_UPDATE || HIWORD(w_param) == EN_CHANGE) {
        EvaluateFields(hwnd);
      }
     // Handle hyperlink click event
     if (LOWORD(w_param) == kIdcHyperlink && HIWORD(w_param) == STN_CLICKED) {
      ShellExecute(NULL, "open", kBigQueryDocsURL, NULL, NULL, SW_SHOWNORMAL);
      break;
      }
      switch (LOWORD(w_param)) {
        case kIdcAuthBox:
        case kIdcDSNEdit:
        case kIdcKeyfileEdit: {
          EvaluateFields(hwnd);
        } break;
        case kIdcBrowseButton: {
          HWND h_edit = GetDlgItem(hwnd, kIdcKeyfileEdit);
          OpenFileDialog(hwnd, h_edit);
        } break;
        case kIdcTrustedCertBrowseButton: {
          HWND h_edit = GetDlgItem(hwnd, kIdcTrustedCertEdit);
          OpenFileDialog(hwnd, h_edit);
        } break;
        case kIdcLoggingBtn: {
          SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
          LogTraceDialog log_form;
          if (IsWindowVisible(log_form.GetHwnd())) {
            SetForegroundWindow(log_form.GetHwnd());
            EnableWindow(hwnd, FALSE);
            break;
          }
          if (!log_form.GetHwnd()) {
            log_form = LogTraceDialog();
            EnableWindow(hwnd, FALSE);
          }
          log_form.Show();
          SetWindowPos(log_form.GetHwnd(), HWND_TOPMOST, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE);
          MSG msg = {};
          while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
          }
          EnableWindow(hwnd, TRUE);
          SetForegroundWindow(hwnd);
          SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
          break;
        }
        case kIdcButtonOk: {
          HWND h_dsn = GetDlgItem(hwnd, kIdcDSNEdit);
          char dsn_buffer[256];
          GetWindowText(h_dsn, dsn_buffer, sizeof(dsn_buffer));
          dsn_name_ = dsn_buffer;

          HWND h_key = GetDlgItem(hwnd, kIdcKeyfileEdit);
          char key_buffer[256];
          GetWindowText(h_key, key_buffer, sizeof(key_buffer));
          key_file_path_ = key_buffer;

          HWND h_auth_box = GetDlgItem(hwnd, kIdcAuthBox);
          char auth_buffer[256];
          GetWindowText(h_auth_box, auth_buffer, sizeof(auth_buffer));
          o_auth_mechanism_ = auth_buffer;

          HWND h_catalog_box = GetDlgItem(hwnd, kIdcCatlogBOX);
          char catalog_buffer[256];
          GetWindowText(h_catalog_box, catalog_buffer, sizeof(catalog_buffer));
          catalog_ = catalog_buffer;

          HWND h_dataset_box = GetDlgItem(hwnd, kIdcDatasetBOX);
          char data_buffer[256];
          GetWindowText(h_dataset_box, data_buffer, sizeof(data_buffer));
          dataset_ = data_buffer;

          HWND h_encrypt_combo_box = GetDlgItem(hwnd, kIdcEncryptDataComboBox);
          char encrypt_buffer[256];
          GetWindowText(h_encrypt_combo_box, encrypt_buffer,
                        sizeof(encrypt_buffer));
          encrypt_data_ = encrypt_buffer;

          HWND h_min_tls_box = GetDlgItem(hwnd, kIdcMinTLSComboBox);
          char min_tls_buffer[256];
          GetWindowText(h_min_tls_box, min_tls_buffer, sizeof(min_tls_buffer));
          min_tls_version_ = min_tls_buffer;
          if (min_tls_version_ != "1.2") {
            MessageBox(hwnd, "Invalid MIN TLS Version!", "Error",
                       MB_OK | MB_ICONERROR);
            min_tls_version_ = "";
            return 0;
          }

          HWND h_trusted_cert_box = GetDlgItem(hwnd, kIdcTrustedCertEdit);
          char trusted_cert_buffer[256];
          GetWindowText(h_trusted_cert_box, trusted_cert_buffer,
                        sizeof(trusted_cert_buffer));
          trusted_cert_ = trusted_cert_buffer;

          HWND h_description_box = GetDlgItem(hwnd, kIdcDescriptionEdit);
          char description_buffer[256];
          GetWindowText(h_description_box, description_buffer,
                        sizeof(description_buffer));
          description_ = description_buffer;

          DestroyWindow(hwnd);  // Close the window
          break;
        }
        case kIdcButtonTest: {
          HWND h_dsn = GetDlgItem(hwnd, kIdcDSNEdit);
          char dsn_buffer[256];
          GetWindowText(h_dsn, dsn_buffer, sizeof(dsn_buffer));

          HWND h_key = GetDlgItem(hwnd, kIdcKeyfileEdit);
          char key_buffer[256];
          GetWindowText(h_key, key_buffer, sizeof(key_buffer));

          HWND h_auth_box = GetDlgItem(hwnd, kIdcAuthBox);
          char auth_buffer[256];
          GetWindowText(h_auth_box, auth_buffer, sizeof(auth_buffer));

          Section attributes_map;
          attributes_map[kDsnName] = dsn_buffer;
          attributes_map[kKeyFilePath] = key_buffer;
          attributes_map[kOAuthMechanism] = auth_buffer;
          attributes_map[kDataset] = dataset_;

          auto status =
              TestODBCConnection(std::make_shared<Section>(attributes_map));
          if (status.ok()) {
            std::string message_text =
                "SUCCESS!\n\nSuccessfully connected to data source!\n\n";
            MessageBox(hwnd, message_text.c_str(), "Test Results",
                       MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
            return 0;
          } else {
            MessageBox(hwnd, "Connection Failed!", "Error",
                       MB_OK | MB_ICONERROR);
            return 0;
          }
        }
        case kIdcProxyOptionsButton: {
          SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

          ProxyOptions proxy_form;
          if (IsWindowVisible(proxy_form.GetHwnd())) {
            SetForegroundWindow(proxy_form.GetHwnd());
            EnableWindow(hwnd, FALSE);
            break;
          }
          if (!proxy_form.GetHwnd()) {
            proxy_form = ProxyOptions();
            EnableWindow(hwnd, FALSE);
          }
          proxy_form.Show(hwnd);
          SetWindowPos(proxy_form.GetHwnd(), HWND_TOPMOST, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE);

          MSG msg = {};
          while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
          }
          EnableWindow(hwnd, TRUE);
          SetForegroundWindow(hwnd);
          SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
          break;
        }
        case kIdcAdvanceOptBtn: {
          SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
          AdvanceOptions adv_form;
          if (IsWindowVisible(adv_form.GetHwnd())) {
            SetForegroundWindow(adv_form.GetHwnd());
            EnableWindow(hwnd, FALSE);
            break;
          }
          if (!adv_form.GetHwnd()) {
            adv_form = AdvanceOptions();
            EnableWindow(hwnd, FALSE);
          }
          adv_form.Show(hwnd);
          SetWindowPos(adv_form.GetHwnd(), HWND_TOPMOST, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE);

          MSG msg = {};
          while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
          }
          EnableWindow(hwnd, TRUE);
          SetForegroundWindow(hwnd);
          SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

          break;
        }
        case kIdcCatlogBOX:
        case kIdcDatasetBOX: {
          EvaluateFields(hwnd);
          char catalog_buffer[256] = {};
          char dsn_buffer[256] = {};
          char key_buffer[256] = {};
          char auth_buffer[256] = {};
          char data_buffer[256] = {};  // Only used for kIdcDatasetBOX

          RetrieveFieldText(hwnd, kIdcCatlogBOX, catalog_buffer,
                            sizeof(catalog_buffer));
          RetrieveFieldText(hwnd, kIdcDSNEdit, dsn_buffer, sizeof(dsn_buffer));
          RetrieveFieldText(hwnd, kIdcKeyfileEdit, key_buffer,
                            sizeof(key_buffer));
          RetrieveFieldText(hwnd, kIdcAuthBox, auth_buffer,
                            sizeof(auth_buffer));

          if (LOWORD(w_param) == kIdcDatasetBOX) {
            RetrieveFieldText(hwnd, kIdcDatasetBOX, data_buffer,
                              sizeof(data_buffer));
          }

          switch (HIWORD(w_param)) {
            case CBN_DROPDOWN:
              if (LOWORD(w_param) == kIdcCatlogBOX) {
                HandleDropdown(hwnd, kIdcCatlogBOX, "Catalog", key_buffer,
                               auth_buffer);
              } else if (LOWORD(w_param) == kIdcDatasetBOX) {
                HandleDropdown(hwnd, kIdcDatasetBOX, "Dataset", key_buffer,
                               auth_buffer, catalog_buffer);
              }
              break;

            case CBN_SELCHANGE:
              if (LOWORD(w_param) == kIdcCatlogBOX) {
                HandleSelectionChange(hwnd, kIdcCatlogBOX);
              } else if (LOWORD(w_param) == kIdcDatasetBOX) {
                HandleSelectionChange(hwnd, kIdcDatasetBOX);
              }
              break;
          }
          break;
        }
        case kIdcButtonCancel: {
          // Retains previous values in the child form if OK is clicked there
          // but Cancel is clicked in the main form. Ensures no data entered in
          // either form is saved in such cases.
          HWND h_dsn = GetDlgItem(hwnd, kIdcDSNEdit);
          char dsn_buffer[256];
          GetWindowText(h_dsn, dsn_buffer, sizeof(dsn_buffer));

          std::string registry_key = GetPathToOdbcIni() + "\\" + dsn_buffer;
          auto res = GetSectionWin(registry_key);
          Section prev_section = *res.GetValue();
          AdvanceOptions adv_form = AdvanceOptions();
          ProxyOptions proxy_form = ProxyOptions();
          adv_form.SetValues(prev_section);
          proxy_form.SetValues(prev_section);
          DestroyWindow(hwnd);
        }
      }
      break;
    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;

    case WM_DESTROY:
      if (p_this) {
        p_this->m_hwnd = NULL;  // Set the window handle to NULL
      }
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProc(hwnd, u_msg, w_param, l_param);
}

}  // namespace google::cloud::odbc_bq_driver_internal
