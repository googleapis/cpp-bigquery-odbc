// Copyright 2023 Google LLC
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

// clang-format off
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <shellapi.h>
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
// clang-format on

#include "google/cloud/odbc/bq_client_interface/odbc_authentication.h"
#include "google/cloud/odbc/bq_client_interface/setenv.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/base64_transforms.h"
#include "google/cloud/internal/credentials_impl.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/rest_client.h"
#include "google/cloud/internal/rest_context.h"
#include "google/cloud/internal/rest_request.h"
#include "google/cloud/internal/rest_response.h"
#include "google/cloud/internal/sha256_hash.h"
#include "google/cloud/oauth2/access_token_generator.h"
#include "google/cloud/status_or.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <mutex>
#include <random>
#include <sstream>
#include <unordered_map>

namespace google::cloud::odbc_bigquery_client_interface {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::odbc_bigquery_client_interface::SetEnv;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

auto const kSelfSignedJwtEnvVar =
    "GOOGLE_CLOUD_CPP_EXPERIMENTAL_DISABLE_SELF_SIGNED_JWT";

namespace {

constexpr int kPortSearchStart = 8085;
constexpr int kPortSearchEnd = 8185;
constexpr char kStandardUserScopes[] =
    "openid https://www.googleapis.com/auth/userinfo.email "
    "https://www.googleapis.com/auth/cloud-platform "
    "https://www.googleapis.com/auth/bigquery";
constexpr char kWorkforceScopes[] =
    "https://www.googleapis.com/auth/cloud-platform";

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
void CloseSocket(SocketHandle s) {
  if (s != INVALID_SOCKET) {
    closesocket(s);
  }
}
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
void CloseSocket(SocketHandle s) {
  if (s >= 0) {
    close(s);
  }
}
#endif

std::mutex& InteractiveTokenCacheMutex() {
  static auto* mu = new std::mutex();
  return *mu;
}

std::unordered_map<std::string, OAuthSignInResult>& InteractiveTokenCache() {
  static auto* cache = new std::unordered_map<std::string, OAuthSignInResult>();
  return *cache;
}

std::atomic<bool>& InteractiveSignInCancelFlag() {
  static std::atomic<bool> cancel_flag{false};
  return cancel_flag;
}

std::string MakeInteractiveTokenCacheKey(ConnProps const& conn_props) {
  return absl::StrCat(
      conn_props.credentials_file_path, "|", conn_props.byoid_aud_url, "|",
      conn_props.gcd.universe_domain, "|", conn_props.universe_cloud_web_domain,
      "|", conn_props.client_id, "|",
      conn_props.use_default_client_credentials ? "1" : "0");
}

std::string GenerateRandomUrlSafeToken(std::size_t num_bytes) {
  std::vector<std::uint8_t> bytes(num_bytes);
  std::random_device rd;
  for (std::size_t i = 0; i < num_bytes;) {
    unsigned int word = rd();
    for (int shift = 0; shift < 32 && i < num_bytes; shift += 8, ++i) {
      bytes[i] = static_cast<std::uint8_t>((word >> shift) & 0xFF);
    }
  }
  return ::google::cloud::internal::UrlsafeBase64Encode(bytes);
}

std::string UrlEncodeComponent(std::string const& input) {
  std::ostringstream escaped;
  escaped << std::hex << std::uppercase << std::setfill('0');
  for (unsigned char c : input) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      escaped << static_cast<char>(c);
    } else {
      escaped << '%' << std::setw(2) << static_cast<int>(c);
    }
  }
  return escaped.str();
}

std::string UrlDecodeComponent(std::string const& input) {
  std::string out;
  out.reserve(input.size());
  for (std::size_t i = 0; i < input.size(); ++i) {
    if (input[i] == '+') {
      out.push_back(' ');
    } else if (input[i] == '%' && i + 2 < input.size() &&
               std::isxdigit(static_cast<unsigned char>(input[i + 1])) &&
               std::isxdigit(static_cast<unsigned char>(input[i + 2]))) {
      int value = std::stoi(input.substr(i + 1, 2), nullptr, 16);
      out.push_back(static_cast<char>(value));
      i += 2;
    } else {
      out.push_back(input[i]);
    }
  }
  return out;
}

std::unordered_map<std::string, std::string> ParseQueryString(
    std::string const& query) {
  std::unordered_map<std::string, std::string> params;
  std::size_t start = 0;
  while (start <= query.size()) {
    std::size_t amp = query.find('&', start);
    std::string pair = (amp == std::string::npos)
                           ? query.substr(start)
                           : query.substr(start, amp - start);
    if (!pair.empty()) {
      std::size_t eq = pair.find('=');
      if (eq == std::string::npos) {
        params[UrlDecodeComponent(pair)] = "";
      } else {
        params[UrlDecodeComponent(pair.substr(0, eq))] =
            UrlDecodeComponent(pair.substr(eq + 1));
      }
    }
    if (amp == std::string::npos) break;
    start = amp + 1;
  }
  return params;
}

std::string BuildUrlWithQuery(
    std::string const& base_url,
    std::vector<std::pair<std::string, std::string>> const& params) {
  std::ostringstream oss;
  oss << base_url;
  char sep = (base_url.find('?') == std::string::npos) ? '?' : '&';
  for (auto const& [k, v] : params) {
    oss << sep << UrlEncodeComponent(k) << '=' << UrlEncodeComponent(v);
    sep = '&';
  }
  return oss.str();
}

std::string NormalizeWorkforceAudience(std::string const& audience) {
  if (audience.rfind("locations/", 0) == 0) {
    return "//iam.googleapis.com/" + audience;
  }
  return audience;
}

bool IsAllowedEndpointUrl(std::string const& url) {
  if (url.empty()) return false;
  for (unsigned char c : url) {
    if (c < 0x20 || c == 0x7F) return false;
  }
  return url.rfind("https://", 0) == 0 ||
         url.rfind("http://localhost:", 0) == 0 ||
         url.rfind("http://127.0.0.1:", 0) == 0;
}

StatusRecord LaunchSystemBrowser(std::string const& url) {
  if (!IsAllowedEndpointUrl(url)) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Refusing to launch browser for non-HTTPS URL"};
  }
#ifdef _WIN32
  HINSTANCE res = ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr,
                                SW_SHOWNORMAL);
  if (reinterpret_cast<INT_PTR>(res) <= 32) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Failed to open system browser for authentication"};
  }
  return StatusRecord::Ok();
#else
  pid_t pid = fork();
  if (pid < 0) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Failed to fork process to open system browser"};
  }
  if (pid == 0) {
#ifdef __APPLE__
    execlp("open", "open", url.c_str(), static_cast<char*>(nullptr));
#else
    execlp("xdg-open", "xdg-open", url.c_str(), static_cast<char*>(nullptr));
#endif
    _exit(127);
  }
  int status = 0;
  waitpid(pid, &status, WNOHANG);
  return StatusRecord::Ok();
#endif
}

struct LoopbackServer {
  SocketHandle listen_sock = kInvalidSocket;
  int port = 0;
#ifdef _WIN32
  bool wsa_initialized = false;
#endif
  ~LoopbackServer() {
    CloseSocket(listen_sock);
#ifdef _WIN32
    if (wsa_initialized) {
      WSACleanup();
    }
#endif
  }
};

StatusRecord StartLoopbackServer(LoopbackServer& server) {
#ifdef _WIN32
  WSADATA wsa_data;
  if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Failed to initialize Winsock for OAuth callback"};
  }
  server.wsa_initialized = true;
#endif

  auto try_bind = [&](int candidate_port) -> bool {
    SocketHandle s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == kInvalidSocket) return false;
#ifdef _WIN32
    int exclusive = 1;
    setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
               reinterpret_cast<char const*>(&exclusive), sizeof(exclusive));
#endif
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<std::uint16_t>(candidate_port));
    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
        listen(s, 8) != 0) {
      CloseSocket(s);
      return false;
    }
    socklen_t addr_len = sizeof(addr);
    if (getsockname(s, reinterpret_cast<sockaddr*>(&addr), &addr_len) != 0) {
      CloseSocket(s);
      return false;
    }
    server.listen_sock = s;
    server.port = ntohs(addr.sin_port);
    return true;
  };

  for (int p = kPortSearchStart; p < kPortSearchEnd; ++p) {
    if (try_bind(p)) return StatusRecord::Ok();
  }
  if (try_bind(0)) return StatusRecord::Ok();

  return StatusRecord{
      SQLStates::k_HY000(),
      "Failed to bind local loopback server for OAuth redirect"};
}

void SendHttpHtmlResponse(SocketHandle client_sock, int status_code,
                          char const* status_text, std::string const& body) {
  std::ostringstream resp;
  resp << "HTTP/1.1 " << status_code << ' ' << status_text << "\r\n"
       << "Content-Type: text/html; charset=utf-8\r\n"
       << "Cache-Control: no-store\r\n"
       << "Content-Length: " << body.size() << "\r\n"
       << "Connection: close\r\n\r\n"
       << body;
  std::string wire = resp.str();
  send(client_sock, wire.data(), static_cast<int>(wire.size()), 0);
#ifdef _WIN32
  shutdown(client_sock, SD_SEND);
#else
  shutdown(client_sock, SHUT_WR);
#endif
}

StatusRecordOr<std::string> WaitForOAuthAuthorizationCode(
    SocketHandle listen_sock, std::string const& expected_state,
    std::chrono::seconds timeout) {
  auto const deadline = std::chrono::steady_clock::now() + timeout;
  std::vector<SocketHandle> clients;
  auto cleanup_clients = [&]() {
    for (SocketHandle c : clients) {
      CloseSocket(c);
    }
    clients.clear();
  };

  while (true) {
    if (InteractiveSignInCancelFlag().load(std::memory_order_relaxed)) {
      cleanup_clients();
      return StatusRecord{SQLStates::k_HY000(),
                          "Interactive OAuth sign-in was cancelled"};
    }
    auto const now = std::chrono::steady_clock::now();
    if (now >= deadline) {
      cleanup_clients();
      return StatusRecord{
          SQLStates::k_HY000(),
          "Timed out waiting for browser authentication callback"};
    }
    auto const remaining_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now)
            .count();
    auto const wait_ms =
        std::min<long long>(std::max<long long>(remaining_ms, 1), 100);

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(listen_sock, &read_fds);
    SocketHandle max_fd = listen_sock;
    for (SocketHandle c : clients) {
      FD_SET(c, &read_fds);
      if (c > max_fd) max_fd = c;
    }

    timeval tv{};
    tv.tv_sec = static_cast<long>(wait_ms / 1000);
    tv.tv_usec = static_cast<long>((wait_ms % 1000) * 1000);

    int ready =
        select(static_cast<int>(max_fd) + 1, &read_fds, nullptr, nullptr, &tv);
    if (ready < 0) {
#ifndef _WIN32
      if (errno == EINTR) continue;
#endif
      cleanup_clients();
      return StatusRecord{SQLStates::k_HY000(),
                          "Error waiting for OAuth redirect on local socket"};
    }
    if (ready == 0) {
      continue;
    }

    if (FD_ISSET(listen_sock, &read_fds)) {
      SocketHandle c = accept(listen_sock, nullptr, nullptr);
      if (c != kInvalidSocket) {
        clients.push_back(c);
      }
    }

    for (auto it = clients.begin(); it != clients.end();) {
      SocketHandle c = *it;
      if (!FD_ISSET(c, &read_fds)) {
        ++it;
        continue;
      }

      char buf[8192];
      int n = recv(c, buf, sizeof(buf) - 1, 0);
      if (n <= 0) {
        CloseSocket(c);
        it = clients.erase(it);
        continue;
      }
      buf[n] = '\0';
      std::string req(buf, static_cast<std::size_t>(n));

      std::size_t line_end = req.find("\r\n");
      std::string req_line =
          (line_end == std::string::npos) ? req : req.substr(0, line_end);
      std::istringstream iss(req_line);
      std::string method;
      std::string target;
      iss >> method >> target;

      if (target.rfind("/favicon.ico", 0) == 0) {
        SendHttpHtmlResponse(c, 404, "Not Found", "");
        CloseSocket(c);
        it = clients.erase(it);
        continue;
      }

      std::size_t qpos = target.find('?');
      std::string query =
          (qpos == std::string::npos) ? "" : target.substr(qpos + 1);
      auto params = ParseQueryString(query);

      if (params.count("error") && !params["error"].empty()) {
        std::string err_msg = "OAuth authorization failed: " + params["error"];
        if (!params["error_description"].empty()) {
          err_msg += " (" + params["error_description"] + ")";
        }
        SendHttpHtmlResponse(
            c, 200, "OK",
            "<html><body><h3>Authentication failed</h3><p>You may close this "
            "window and return to your application.</p></body></html>");
        cleanup_clients();
        return StatusRecord{SQLStates::k_28000(), err_msg};
      }

      auto code_it = params.find("code");
      if (code_it == params.end() || code_it->second.empty()) {
        SendHttpHtmlResponse(
            c, 400, "Bad Request",
            "<html><body>Invalid OAuth callback.</body></html>");
        CloseSocket(c);
        it = clients.erase(it);
        continue;
      }

      if (params["state"] != expected_state) {
        SendHttpHtmlResponse(
            c, 400, "Bad Request",
            "<html><body>Invalid OAuth state parameter.</body></html>");
        cleanup_clients();
        return StatusRecord{SQLStates::k_28000(),
                            "OAuth state mismatch in authorization callback"};
      }

      SendHttpHtmlResponse(
          c, 200, "OK",
          "<html><body><h3>Authentication complete</h3><p>You have signed in "
          "successfully. You may close this browser window and return to your "
          "application.</p></body></html>");
      std::string auth_code = code_it->second;
      cleanup_clients();
      return auth_code;
    }
  }
}

StatusOr<std::string> DefaultHttpPostForm(
    ConnProps const& conn_props, Options options, std::string const& url,
    std::vector<std::pair<std::string, std::string>> const& headers,
    std::vector<std::pair<std::string, std::string>> const& form_data) {
  if (!options.has<::google::cloud::CARootsFilePathOption>() &&
      !conn_props.ssl_credentials.pem_root_certs.empty()) {
    options.set<::google::cloud::CARootsFilePathOption>(
        conn_props.ssl_credentials.pem_root_certs);
  }
  if (!options.has<::google::cloud::ProxyOption>() &&
      !conn_props.proxy_options.hostname.empty()) {
    options.set<::google::cloud::ProxyOption>(
        ::google::cloud::ProxyConfig()
            .set_hostname(conn_props.proxy_options.hostname)
            .set_port(conn_props.proxy_options.port)
            .set_username(conn_props.proxy_options.username)
            .set_password(conn_props.proxy_options.password)
            .set_scheme("http"));
  }
  auto client =
      ::google::cloud::rest_internal::MakeDefaultRestClient("", options);
  ::google::cloud::rest_internal::RestRequest request(url);
  request.AddHeader("content-type", "application/x-www-form-urlencoded");
  for (auto const& [k, v] : headers) {
    request.AddHeader(k, v);
  }
  ::google::cloud::rest_internal::RestContext context;
  auto response = client->Post(context, request, form_data);
  if (!response) return std::move(response).status();
  if (::google::cloud::rest_internal::IsHttpError(**response)) {
    return ::google::cloud::rest_internal::AsStatus(std::move(**response));
  }
  return ::google::cloud::rest_internal::ReadAll(
      std::move(**response).ExtractPayload());
}

std::string EncodeBasicAuthHeader(std::string const& client_id,
                                  std::string const& client_secret) {
  ::google::cloud::internal::Base64Encoder encoder;
  for (unsigned char const c : absl::StrCat(client_id, ":", client_secret)) {
    encoder.PushBack(c);
  }
  return absl::StrCat("Basic ", std::move(encoder).FlushAndPad());
}

struct ResolvedOAuthFlowConfig {
  bool is_workforce = false;
  std::string auth_url;
  std::string token_url;
  std::string client_id;
  std::string client_secret;
  std::string audience;
  std::string provider_name;
  std::string universe_domain;
};

StatusRecordOr<std::string> ReadTextFile(std::string const& path,
                                         char const* label) {
  std::ifstream stream(path);
  if (!stream) {
    return StatusRecord{
        SQLStates::k_HY000(),
        absl::StrCat("Could not open ", label, " key file: ", path)};
  }
  std::string contents((std::istreambuf_iterator<char>(stream)),
                       std::istreambuf_iterator<char>());
  if (contents.empty()) {
    return StatusRecord{
        SQLStates::k_HY000(),
        absl::StrCat(label, " key file is empty or could not be read: ", path)};
  }
  return contents;
}

StatusRecord ResolveClientCredentials(ConnProps const& conn_props,
                                      bool is_workforce,
                                      std::string& client_id_out,
                                      std::string& client_secret_out) {
  if (!conn_props.use_default_client_credentials) {
    if (conn_props.client_id.empty() || conn_props.client_secret.empty()) {
      return StatusRecord{SQLStates::k_HY000(),
                          "ClientId and ClientSecret must be provided when "
                          "UseDefaultClientCredentials is disabled"};
    }
    client_id_out = conn_props.client_id;
    client_secret_out = conn_props.client_secret;
    return StatusRecord::Ok();
  }
  if (!conn_props.client_id.empty() && !conn_props.client_secret.empty()) {
    client_id_out = conn_props.client_id;
    client_secret_out = conn_props.client_secret;
    return StatusRecord::Ok();
  }
  if (is_workforce) {
    client_id_out = kDefaultWifOAuthClientId;
    client_secret_out = kDefaultWifOAuthClientSecret;
  } else {
    client_id_out = kDefaultOAuthClientId;
    client_secret_out = kDefaultOAuthClientSecret;
  }
  return StatusRecord::Ok();
}

StatusRecordOr<ResolvedOAuthFlowConfig> ResolveOAuthFlowConfig(
    ConnProps const& conn_props) {
  ResolvedOAuthFlowConfig cfg;
  cfg.universe_domain = !conn_props.gcd.universe_domain.empty()
                            ? conn_props.gcd.universe_domain
                            : kDefaultUniverseDomain;
  std::string web_domain = !conn_props.universe_cloud_web_domain.empty()
                               ? conn_props.universe_cloud_web_domain
                               : kDefaultUniverseCloudWebDomain;

  if (!conn_props.credentials_file_path.empty()) {
    auto contents =
        ReadTextFile(conn_props.credentials_file_path, "External Account");
    if (!contents) return contents.GetStatusRecord();
    auto json = nlohmann::json::parse(*contents, nullptr, false);
    if (!json.is_object()) {
      return StatusRecord{
          SQLStates::k_HY000(),
          "Login configuration file is not a valid JSON object"};
    }
    if (json.value("type", "") ==
        "external_account_authorized_user_login_config") {
      cfg.is_workforce = true;
      std::string ud_json = json.value("universe_domain", "");
      if (!ud_json.empty()) cfg.universe_domain = ud_json;
      std::string wd_json = json.value("universe_cloud_web_domain", "");
      if (!wd_json.empty()) web_domain = wd_json;

      cfg.audience = NormalizeWorkforceAudience(json.value("audience", ""));
      auto provider_or = ExtractWorkforceProviderName(cfg.audience);
      if (!provider_or) return provider_or.GetStatusRecord();
      cfg.provider_name = *provider_or;

      cfg.auth_url = json.value(
          "auth_url", absl::StrCat("https://auth.", web_domain, "/authorize"));
      cfg.token_url = json.value(
          "token_url",
          absl::StrCat("https://sts.", cfg.universe_domain, "/v1/oauthtoken"));

      auto status = ResolveClientCredentials(conn_props, /*is_workforce=*/true,
                                             cfg.client_id, cfg.client_secret);
      if (!status.ok()) return status;
      return cfg;
    }
  }

  if (IsWorkforcePoolAudience(conn_props.byoid_aud_url)) {
    cfg.is_workforce = true;
    cfg.audience = NormalizeWorkforceAudience(conn_props.byoid_aud_url);
    auto provider_or = ExtractWorkforceProviderName(cfg.audience);
    if (!provider_or) return provider_or.GetStatusRecord();
    cfg.provider_name = *provider_or;
    cfg.auth_url = absl::StrCat("https://auth.", web_domain, "/authorize");
    if (!conn_props.byoid_token_url.empty() &&
        conn_props.byoid_token_url != kDefaultTokenUrl) {
      cfg.token_url = conn_props.byoid_token_url;
    } else {
      cfg.token_url =
          absl::StrCat("https://sts.", cfg.universe_domain, "/v1/oauthtoken");
    }
    auto status = ResolveClientCredentials(conn_props, /*is_workforce=*/true,
                                           cfg.client_id, cfg.client_secret);
    if (!status.ok()) return status;
    return cfg;
  }

  if (cfg.universe_domain != kDefaultUniverseDomain) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "Interactive user authentication in custom universe domain '" +
            cfg.universe_domain +
            "' requires Workforce Identity Federation configuration "
            "(BYOID_AudienceUri or login config file)."};
  }

  cfg.is_workforce = false;
  cfg.auth_url = kDefaultOAuthAuthUrl;
  cfg.token_url = kDefaultOAuthTokenUrl;
  auto status = ResolveClientCredentials(conn_props, /*is_workforce=*/false,
                                         cfg.client_id, cfg.client_secret);
  if (!status.ok()) return status;
  return cfg;
}

std::string MakeExternalAuthorizedUserJson(std::string const& audience,
                                           std::string const& client_id,
                                           std::string const& client_secret,
                                           std::string const& refresh_token,
                                           std::string const& token_url,
                                           std::string const& universe_domain) {
  nlohmann::json json = {
      {"type", "external_account_authorized_user"},
      {"client_id", client_id},
      {"client_secret", client_secret},
      {"refresh_token", refresh_token},
      {"token_url", token_url},
  };
  if (!audience.empty()) {
    json["audience"] = audience;
  }
  if (!universe_domain.empty()) {
    json["universe_domain"] = universe_domain;
  }
  return json.dump();
}

StatusRecordOr<std::shared_ptr<Credentials>> CredentialsFromSignInResult(
    OAuthSignInResult const& sign_in, Options const& options) {
  if (sign_in.is_workforce) {
    if (!sign_in.refresh_token.empty()) {
      return ::google::cloud::MakeExternalAccountCredentials(
          MakeExternalAuthorizedUserJson(
              sign_in.audience, sign_in.client_id, sign_in.client_secret,
              sign_in.refresh_token, sign_in.token_url,
              sign_in.universe_domain),
          options);
    }
    if (!sign_in.access_token.empty()) {
      auto const exp = std::chrono::system_clock::now() +
                       std::chrono::seconds(
                           sign_in.expires_in > 0 ? sign_in.expires_in : 3600);
      return ::google::cloud::MakeAccessTokenCredentials(sign_in.access_token,
                                                         exp);
    }
    return StatusRecord{
        SQLStates::k_28000(),
        "Token endpoint did not return a refresh_token or access_token"};
  }

  if (sign_in.refresh_token.empty()) {
    return StatusRecord{SQLStates::k_28000(),
                        "Token endpoint did not return a refresh_token"};
  }
  nlohmann::json json = {
      {"type", "authorized_user"},
      {"client_id", sign_in.client_id},
      {"client_secret", sign_in.client_secret},
      {"refresh_token", sign_in.refresh_token},
  };
  return ::google::cloud::internal::MakeUserAccountCredentials(json.dump(),
                                                               options);
}

StatusRecordOr<std::shared_ptr<Credentials>>
GetOrRunCachedInteractiveCredentials(ConnProps const& conn_props,
                                     Options const& options) {
  std::string const cache_key = MakeInteractiveTokenCacheKey(conn_props);
  {
    std::lock_guard<std::mutex> lk(InteractiveTokenCacheMutex());
    auto it = InteractiveTokenCache().find(cache_key);
    if (it != InteractiveTokenCache().end() &&
        !it->second.refresh_token.empty()) {
      return CredentialsFromSignInResult(it->second, options);
    }
  }
  auto sign_in = RunInteractiveOAuthSignIn(conn_props, options);
  if (!sign_in) return sign_in.GetStatusRecord();
  return CredentialsFromSignInResult(*sign_in, options);
}

std::string FindAdcCredentialsFilePath() {
  if (auto env_adc = GetEnv("GOOGLE_APPLICATION_CREDENTIALS");
      env_adc && !env_adc->empty()) {
    return *env_adc;
  }
  if (auto override_adc = GetEnv("GOOGLE_GCLOUD_ADC_PATH_OVERRIDE");
      override_adc && !override_adc->empty()) {
    return *override_adc;
  }
#ifdef _WIN32
  if (auto appdata = GetEnv("APPDATA"); appdata && !appdata->empty()) {
    return *appdata + "\\gcloud\\application_default_credentials.json";
  }
#else
  if (auto home = GetEnv("HOME"); home && !home->empty()) {
    return *home + "/.config/gcloud/application_default_credentials.json";
  }
#endif
  return {};
}

bool TryParseJsonFile(std::string const& path, nlohmann::json& out) {
  if (path.empty()) return false;
  std::ifstream ifs(path);
  if (!ifs) return false;
  std::string contents((std::istreambuf_iterator<char>(ifs)),
                       std::istreambuf_iterator<char>());
  out = nlohmann::json::parse(contents, nullptr, false);
  return out.is_object();
}

StatusRecordOr<std::shared_ptr<Credentials>>
CreateWorkforceRefreshTokenCredentials(ConnProps const& conn_props,
                                       Options const& options) {
  auto cfg_or = ResolveOAuthFlowConfig(conn_props);
  if (!cfg_or) return cfg_or.GetStatusRecord();
  return ::google::cloud::MakeExternalAccountCredentials(
      MakeExternalAuthorizedUserJson(
          cfg_or->audience, cfg_or->client_id, cfg_or->client_secret,
          conn_props.refresh_token, cfg_or->token_url, cfg_or->universe_domain),
      options);
}

}  // namespace

std::string GeneratePkceCodeVerifier() {
  // 48 random bytes -> 64 unpadded base64url chars (RFC 7636 requires 43..128).
  return GenerateRandomUrlSafeToken(48);
}

std::string ComputePkceCodeChallenge(std::string const& code_verifier) {
  auto const digest = ::google::cloud::internal::Sha256Hash(code_verifier);
  return ::google::cloud::internal::UrlsafeBase64Encode(digest);
}

StatusRecordOr<std::string> ExtractWorkforceProviderName(
    std::string const& audience) {
  std::string provider_name;
  std::size_t const pos = audience.find("/locations/");
  if (pos != std::string::npos) {
    provider_name = audience.substr(pos + 1);
  } else if (audience.rfind("locations/", 0) == 0) {
    provider_name = audience;
  }
  if (provider_name.empty() || !IsWorkforcePoolAudience(provider_name)) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "Invalid Workforce Identity Federation audience URI: " + audience};
  }
  return provider_name;
}

void ClearInteractiveOAuthTokenCache() {
  std::lock_guard<std::mutex> lk(InteractiveTokenCacheMutex());
  InteractiveTokenCache().clear();
}

void CancelInteractiveOAuthSignIn() {
  InteractiveSignInCancelFlag().store(true, std::memory_order_relaxed);
}

StatusRecordOr<OAuthSignInResult> RunInteractiveOAuthSignIn(
    ConnProps const& conn_props, Options const& options,
    BrowserLauncher browser_launcher, HttpPostFormFn http_post_fn,
    std::chrono::seconds timeout) {
  InteractiveSignInCancelFlag().store(false, std::memory_order_relaxed);
  auto cfg_or = ResolveOAuthFlowConfig(conn_props);
  if (!cfg_or) return cfg_or.GetStatusRecord();
  auto const& cfg = *cfg_or;

  if (!IsAllowedEndpointUrl(cfg.auth_url) ||
      !IsAllowedEndpointUrl(cfg.token_url)) {
    return StatusRecord{SQLStates::k_HY000(),
                        "OAuth authorization and token URLs must use HTTPS"};
  }

  LoopbackServer server;
  auto server_status = StartLoopbackServer(server);
  if (!server_status.ok()) return server_status;

  std::string const redirect_uri =
      absl::StrCat("http://localhost:", server.port, "/");
  std::string const code_verifier = GeneratePkceCodeVerifier();
  std::string const code_challenge = ComputePkceCodeChallenge(code_verifier);
  std::string const state = GenerateRandomUrlSafeToken(24);

  std::vector<std::pair<std::string, std::string>> auth_params = {
      {"response_type", "code"},
      {"client_id", cfg.client_id},
      {"redirect_uri", redirect_uri},
      {"scope", cfg.is_workforce ? kWorkforceScopes : kStandardUserScopes},
      {"state", state},
      {"code_challenge", code_challenge},
      {"code_challenge_method", "S256"},
  };
  if (cfg.is_workforce) {
    auth_params.emplace_back("provider_name", cfg.provider_name);
  } else {
    auth_params.emplace_back("access_type", "offline");
    auth_params.emplace_back("prompt", "consent");
  }

  std::string const full_auth_url =
      BuildUrlWithQuery(cfg.auth_url, auth_params);
  auto launcher = browser_launcher ? std::move(browser_launcher)
                                   : BrowserLauncher(LaunchSystemBrowser);
  auto launch_status = launcher(full_auth_url);
  if (!launch_status.ok()) return launch_status;

  auto code_or =
      WaitForOAuthAuthorizationCode(server.listen_sock, state, timeout);
  if (!code_or) return code_or.GetStatusRecord();

  std::vector<std::pair<std::string, std::string>> headers;
  std::vector<std::pair<std::string, std::string>> form_data = {
      {"grant_type", "authorization_code"},
      {"code", *code_or},
      {"redirect_uri", redirect_uri},
      {"code_verifier", code_verifier},
  };
  if (cfg.is_workforce) {
    headers.emplace_back(
        "Authorization",
        EncodeBasicAuthHeader(cfg.client_id, cfg.client_secret));
  } else {
    form_data.emplace_back("client_id", cfg.client_id);
    form_data.emplace_back("client_secret", cfg.client_secret);
  }

  auto post_fn =
      http_post_fn
          ? std::move(http_post_fn)
          : HttpPostFormFn([&conn_props, &options](auto const& u, auto const& h,
                                                   auto const& f) {
              return DefaultHttpPostForm(conn_props, options, u, h, f);
            });

  StatusOr<std::string> token_payload =
      post_fn(cfg.token_url, headers, form_data);
  if (!token_payload) {
    return StatusRecord::ConvertFrom(token_payload.status());
  }

  auto token_json = nlohmann::json::parse(*token_payload, nullptr, false);
  if (!token_json.is_object()) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "OAuth token exchange response could not be parsed as JSON"};
  }
  if (token_json.contains("error")) {
    std::string err = token_json.value("error", "oauth_error");
    std::string desc = token_json.value("error_description", "");
    return StatusRecord{SQLStates::k_28000(),
                        desc.empty() ? ("OAuth token exchange failed: " + err)
                                     : ("OAuth token exchange failed: " + err +
                                        " (" + desc + ")")};
  }

  OAuthSignInResult result;
  result.is_workforce = cfg.is_workforce;
  result.client_id = cfg.client_id;
  result.client_secret = cfg.client_secret;
  result.token_url = cfg.token_url;
  result.audience = cfg.audience;
  result.universe_domain = cfg.universe_domain;
  result.refresh_token = token_json.value("refresh_token", "");
  result.access_token = token_json.value("access_token", "");
  if (token_json.contains("expires_in") &&
      token_json["expires_in"].is_number_integer()) {
    result.expires_in = token_json["expires_in"].get<std::int64_t>();
  }

  if (!cfg.is_workforce && result.refresh_token.empty()) {
    return StatusRecord{SQLStates::k_28000(),
                        "OAuth token exchange did not return a refresh_token"};
  }
  if (cfg.is_workforce && result.refresh_token.empty() &&
      result.access_token.empty()) {
    return StatusRecord{SQLStates::k_28000(),
                        "Workforce OAuth token exchange did not return tokens"};
  }
  if (!result.refresh_token.empty()) {
    std::lock_guard<std::mutex> lk(InteractiveTokenCacheMutex());
    InteractiveTokenCache()[MakeInteractiveTokenCacheKey(conn_props)] = result;
  }
  return result;
}

void ResolveCredentialMetadata(ConnProps const& conn_props,
                               std::string& universe_domain_out,
                               std::string& user_project_out,
                               bool& uses_rest_token_for_grpc_out) {
  universe_domain_out = conn_props.gcd.universe_domain;
  user_project_out = !conn_props.quota_project_id.empty()
                         ? conn_props.quota_project_id
                         : conn_props.byoid_pool_user_project;
  uses_rest_token_for_grpc_out =
      (conn_props.auth_mechanism == OauthMechanism::kUserAccount);

  if (conn_props.auth_mechanism == OauthMechanism::kExternalUser &&
      conn_props.credentials_file_path.empty() &&
      conn_props.byoid_creds_src.empty() &&
      IsWorkforcePoolAudience(conn_props.byoid_aud_url)) {
    uses_rest_token_for_grpc_out = true;
  }

  std::string json_path = conn_props.credentials_file_path;
  if (json_path.empty() &&
      conn_props.auth_mechanism == OauthMechanism::kApplicationDefault) {
    json_path = FindAdcCredentialsFilePath();
  }
  nlohmann::json json;
  if (!TryParseJsonFile(json_path, json)) return;

  std::string const cred_type = json.value("type", "");
  if (cred_type == "authorized_user" ||
      cred_type == "external_account_authorized_user" ||
      cred_type == "external_account_authorized_user_login_config") {
    uses_rest_token_for_grpc_out = true;
  }
  if (universe_domain_out.empty()) {
    universe_domain_out = json.value("universe_domain", "");
  }
  if (user_project_out.empty()) {
    user_project_out = json.value("quota_project_id", "");
    if (user_project_out.empty()) {
      user_project_out = json.value("workforce_pool_user_project", "");
    }
  }
}

StatusRecordOr<std::shared_ptr<Credentials>> CreateServiceCredentials(
    std::string const& credentials_file_path, Options const& options) {
  if (credentials_file_path.empty()) {
    LOG(ERROR)
        << "CreateServiceCredentials:: The path to the file can't be empty";
    return StatusRecord{SQLStates::k_HY000(),
                        "The path to the file can't be empty"};
  }
  auto contents = ReadTextFile(credentials_file_path, "Service Account");
  if (!contents) {
    LOG(ERROR) << "CreateServiceCredentials:: "
               << contents.GetStatusRecord().message;
    return contents.GetStatusRecord();
  }

  return ::google::cloud::MakeServiceAccountCredentials(*contents, options);
}

StatusRecordOr<std::shared_ptr<Credentials>> CreateUserCredentials(
    ConnProps const& conn_props, Options const& options) {
  bool const has_wif_audience =
      IsWorkforcePoolAudience(conn_props.byoid_aud_url);
  if (!conn_props.refresh_token.empty()) {
    if (has_wif_audience ||
        (!conn_props.gcd.universe_domain.empty() &&
         conn_props.gcd.universe_domain != kDefaultUniverseDomain)) {
      return CreateWorkforceRefreshTokenCredentials(conn_props, options);
    }

    std::string client_id;
    std::string client_secret;
    auto client_status = ResolveClientCredentials(
        conn_props, /*is_workforce=*/false, client_id, client_secret);
    if (!client_status.ok()) return client_status;

    nlohmann::json json = {
        {"type", "authorized_user"},
        {"client_id", client_id},
        {"client_secret", client_secret},
        {"refresh_token", conn_props.refresh_token},
    };
    return ::google::cloud::internal::MakeUserAccountCredentials(json.dump(),
                                                                 options);
  }

  if (has_wif_audience) {
    return GetOrRunCachedInteractiveCredentials(conn_props, options);
  }

  if (!conn_props.gcd.universe_domain.empty() &&
      conn_props.gcd.universe_domain != kDefaultUniverseDomain &&
      conn_props.credentials_file_path.empty()) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "Interactive user authentication in custom universe domain '" +
            conn_props.gcd.universe_domain +
            "' requires Workforce Identity Federation configuration "
            "(BYOID_AudienceUri or login config file)."};
  }

  if (conn_props.credentials_file_path.empty()) {
    if (conn_props.allow_interactive_browser) {
      return GetOrRunCachedInteractiveCredentials(conn_props, options);
    }
    LOG(ERROR) << "CreateUserCredentials:: The path to the file can't be empty";
    return StatusRecord{SQLStates::k_HY000(),
                        "The path to the file can't be empty"};
  }

  auto contents =
      ReadTextFile(conn_props.credentials_file_path, "User Account");
  if (!contents) {
    LOG(ERROR) << "CreateUserCredentials:: "
               << contents.GetStatusRecord().message;
    return contents.GetStatusRecord();
  }

  auto json = nlohmann::json::parse(*contents, nullptr, false);
  if (json.is_object()) {
    std::string const cred_type = json.value("type", "");
    if (cred_type == "external_account_authorized_user_login_config") {
      return GetOrRunCachedInteractiveCredentials(conn_props, options);
    }
    if (cred_type == "external_account" ||
        cred_type == "external_account_authorized_user") {
      return ::google::cloud::MakeExternalAccountCredentials(*contents,
                                                             options);
    }
  }

  return ::google::cloud::internal::MakeUserAccountCredentials(*contents,
                                                               options);
}

StatusRecordOr<std::shared_ptr<Credentials>>
CreateApplicationDefaultCredentials(ConnProps const& conn_props,
                                    Options const& options) {
  std::string const adc_path = FindAdcCredentialsFilePath();
  nlohmann::json json;
  if (TryParseJsonFile(adc_path, json) &&
      json.value("type", "") ==
          "external_account_authorized_user_login_config") {
    ConnProps login_props = conn_props;
    login_props.credentials_file_path = adc_path;
    return GetOrRunCachedInteractiveCredentials(login_props, options);
  }
  return ::google::cloud::MakeGoogleDefaultCredentials(options);
}

StatusRecordOr<std::shared_ptr<Credentials>> CreateExternalAuthCredentialsJSON(
    ConnProps const& conn_props, Options const& options) {
  if (conn_props.credentials_file_path.empty()) {
    LOG(ERROR) << "CreateExternalAuthCredentialsJSON:: The path to the "
                  "external auth JSON file can't be empty";
    return StatusRecord{
        SQLStates::k_HY000(),
        "The path to the external auth JSON file can't be empty"};
  }

  auto contents =
      ReadTextFile(conn_props.credentials_file_path, "External Account");
  if (!contents) {
    return contents.GetStatusRecord();
  }

  auto json = nlohmann::json::parse(*contents, nullptr, false);
  if (json.is_object() && json.value("type", "") ==
                              "external_account_authorized_user_login_config") {
    return !conn_props.refresh_token.empty()
               ? CreateWorkforceRefreshTokenCredentials(conn_props, options)
               : GetOrRunCachedInteractiveCredentials(conn_props, options);
  }

  return ::google::cloud::MakeExternalAccountCredentials(*contents, options);
}

StatusRecordOr<nlohmann::json> CreateJsonCredsObject(
    std::string const& byoid_aud_url, std::string const& byoid_creds_source,
    std::string const& byoid_pool_user_project,
    std::string const& byoid_sub_token_type,
    std::string const& byoid_token_url) {
  nlohmann::json json;
  json["type"] = "external_account";
  json["audience"] = byoid_aud_url;
  try {
    json["credential_source"] = nlohmann::json::parse(byoid_creds_source);
  } catch (nlohmann::json::parse_error const&) {
    return StatusRecord{SQLStates::k_HY000(),
                        "BYOID_CREDENTIALSOURCE is not a valid JSON string"};
  }
  json["subject_token_type"] = byoid_sub_token_type;
  json["token_url"] = byoid_token_url;
  if (!byoid_pool_user_project.empty()) {
    json["workforce_pool_user_project"] = byoid_pool_user_project;
  }
  return json;
}

StatusRecordOr<std::shared_ptr<Credentials>>
CreateExternalAccountAuthenticationBYOID(ConnProps const& conn_props,
                                         Options const& options) {
  if (!IsBYOIDPropsSet(conn_props)) {
    LOG(ERROR)
        << "CreateExternalAccountAuthenticationBYOID:: Unable to create "
           "external auth credentials: Required BYOID Properties are not set ";
    return StatusRecord{SQLStates::k_HY000(),
                        "Unable to create external auth credentials: Required "
                        "BYOID Properties are not set "};
  }
  StatusRecordOr<nlohmann::json> json_creds = CreateJsonCredsObject(
      conn_props.byoid_aud_url, conn_props.byoid_creds_src,
      conn_props.byoid_pool_user_project, conn_props.byoid_subj_token_type,
      conn_props.byoid_token_url);
  if (!json_creds) {
    LOG(ERROR)
        << "CreateExternalAccountAuthenticationBYOID::CreateJsonCredsObject:: "
        << json_creds.GetStatusRecord().message;
    return json_creds.GetStatusRecord();
  }
  return ::google::cloud::MakeExternalAccountCredentials((*json_creds).dump(),
                                                         options);
}

StatusRecordOr<std::shared_ptr<Credentials>> CreateCredentials(
    ConnProps const& conn_props, Options const& options) {
  switch (conn_props.auth_mechanism) {
    case OauthMechanism::kServiceAccount:
      return CreateServiceCredentials(conn_props.credentials_file_path,
                                      options);
    case OauthMechanism::kUserAccount:
      return CreateUserCredentials(conn_props, options);
    case OauthMechanism::kApplicationDefault:
      return CreateApplicationDefaultCredentials(conn_props, options);
    case OauthMechanism::kExternalUser: {
      if (!conn_props.credentials_file_path.empty()) {
        return CreateExternalAuthCredentialsJSON(conn_props, options);
      }
      if (IsBYOIDPropsSet(conn_props)) {
        return CreateExternalAccountAuthenticationBYOID(conn_props, options);
      }
      if (IsWorkforcePoolAudience(conn_props.byoid_aud_url)) {
        return !conn_props.refresh_token.empty()
                   ? CreateWorkforceRefreshTokenCredentials(conn_props, options)
                   : GetOrRunCachedInteractiveCredentials(conn_props, options);
      }
      LOG(ERROR) << "CreateCredentials:: The path to the external auth JSON "
                    "file can't be empty";
      return StatusRecord{
          SQLStates::k_HY000(),
          "The path to the external auth JSON file can't be empty"};
    }
  }
  LOG(ERROR) << "CreateCredentials:: OauthMechanism enum is invalid";
  return StatusRecord{SQLStates::k_HY000(), "OauthMechanism enum is invalid"};
}

StatusRecordOr<AccessToken> GetOAuth2Token(
    std::shared_ptr<::google::cloud::oauth2::AccessTokenGenerator> const&
        generator) {
  // We need to set env var for service account to force it to make a request
  // to Google Cloud. Then we return the value of this env var to the previous
  // state. If the env var is unset, token will be created locally, without
  // any request to Google Cloud.
  auto self_signed_jwt_disabled = GetEnv(kSelfSignedJwtEnvVar);
  SetEnv(kSelfSignedJwtEnvVar, "true");
  StatusOr<AccessToken> access_token = generator->GetToken();
  SetEnv(kSelfSignedJwtEnvVar, self_signed_jwt_disabled);
  return StatusRecordOr<AccessToken>::ConvertFromStatusOr(access_token);
}

}  // namespace google::cloud::odbc_bigquery_client_interface
