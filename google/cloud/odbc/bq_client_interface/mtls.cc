// Copyright 2026 Google LLC
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

#include "google/cloud/odbc/bq_client_interface/mtls.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/internal/getenv.h"
#include "absl/log/log.h"
#include "absl/strings/match.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/wait.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <spawn.h>
#include <unistd.h>
#ifdef __APPLE__
#include <crt_externs.h>
#else
extern char** environ;  // NOLINT(readability-redundant-declaration)
#endif
#endif

namespace google::cloud::odbc_bigquery_client_interface {

using ::google::cloud::experimental::SslCertificate;
using ::google::cloud::internal::GetEnv;
using ::google::cloud::oauth2::AccessTokenGenerator;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

namespace {

auto constexpr kDefaultUniverseDomain = "googleapis.com";

enum class MtlsEndpointMode { kAuto, kAlways, kNever };

StatusRecord MakeError(std::string message) {
  LOG(ERROR) << "mTLS:: " << message;
  return StatusRecord{SQLStates::k_HY000(), std::move(message)};
}

std::string ToLower(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

StatusRecordOr<std::string> ReadFileContents(std::string const& path) {
  std::ifstream is(path, std::ios::binary);
  if (!is) return MakeError("Could not open file: " + path);
  return std::string(std::istreambuf_iterator<char>(is),
                     std::istreambuf_iterator<char>());
}

// Returns the DSN value when set, otherwise the environment variable value.
absl::optional<std::string> SettingOrEnv(std::string const& dsn_value,
                                         char const* env_var) {
  if (!dsn_value.empty()) return dsn_value;
#ifdef _WIN32
  char buf[4096];
  DWORD ret = GetEnvironmentVariableA(env_var, buf, sizeof(buf));
  if (ret > 0 && ret < sizeof(buf)) return std::string(buf, ret);
#endif
  auto env = GetEnv(env_var);
  if (env.has_value() && !env->empty()) return env;
  return absl::nullopt;
}

std::string HomeDirectory() {
#ifdef _WIN32
  char buf[4096];
  DWORD ret = GetEnvironmentVariableA("USERPROFILE", buf, sizeof(buf));
  if (ret > 0 && ret < sizeof(buf)) return std::string(buf, ret);
  return GetEnv("USERPROFILE").value_or("");
#else
  return GetEnv("HOME").value_or("");
#endif
}

StatusRecordOr<SslCertificate> MakeSslCertificate(PemBundle const& bundle,
                                                  std::string const& source) {
  if (bundle.certificates.empty()) {
    return MakeError("No PEM CERTIFICATE block found in " + source);
  }
  if (bundle.private_key.empty()) {
    return MakeError("No PEM PRIVATE KEY block found in " + source);
  }
  if (bundle.encrypted_private_key) {
    return MakeError("Encrypted private keys are not supported, found in " +
                     source);
  }
  return SslCertificate(bundle.certificates, bundle.private_key);
}

// Attaches `Authorization: Bearer <token>` to every call using the (cached)
// tokens produced by an `AccessTokenGenerator`. This works for all credential
// types supported by the driver, including impersonated and external accounts.
class AccessTokenGeneratorPlugin : public grpc::MetadataCredentialsPlugin {
 public:
  explicit AccessTokenGeneratorPlugin(
      std::shared_ptr<AccessTokenGenerator> generator)
      : generator_(std::move(generator)) {}

  // Fetching a token may perform HTTP requests, so let gRPC call us from a
  // thread where blocking is allowed.
  [[nodiscard]] bool IsBlocking() const override { return true; }

  [[nodiscard]] char const* GetType() const override {
    return "odbc-oauth2-access-token";
  }

  grpc::Status GetMetadata(
      grpc::string_ref /*service_url*/, grpc::string_ref /*method_name*/,
      grpc::AuthContext const& /*channel_auth_context*/,
      std::multimap<std::string, std::string>* metadata) override {
    auto token = generator_->GetToken();
    if (!token) {
      return grpc::Status(grpc::StatusCode::UNAUTHENTICATED,
                          token.status().message());
    }
    metadata->insert({"authorization", "Bearer " + token->token});
    return grpc::Status::OK;
  }

  [[nodiscard]] std::string DebugString() override {
    return "AccessTokenGeneratorPlugin";
  }

 private:
  std::shared_ptr<AccessTokenGenerator> generator_;
};

#ifdef _WIN32
// Quotes a single argument following the rules used by the Microsoft C runtime
// to parse command lines, see
// https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments
std::string QuoteWindowsArgument(std::string const& argument) {
  if (!argument.empty() &&
      argument.find_first_of(" \t\n\v\"") == std::string::npos) {
    return argument;
  }
  std::string quoted = "\"";
  for (auto it = argument.begin();; ++it) {
    std::size_t backslashes = 0;
    while (it != argument.end() && *it == '\\') {
      ++it;
      ++backslashes;
    }
    if (it == argument.end()) {
      // Escape all backslashes, but let the terminating double quote be
      // interpreted as a metacharacter.
      quoted.append(backslashes * 2, '\\');
      break;
    }
    if (*it == '"') {
      // Escape all backslashes and the following double quote.
      quoted.append(backslashes * 2 + 1, '\\');
    } else {
      // Backslashes are not special here.
      quoted.append(backslashes, '\\');
    }
    quoted.push_back(*it);
  }
  quoted.push_back('"');
  return quoted;
}
#else
char** Environ() {
#ifdef __APPLE__
  return *_NSGetEnviron();
#else
  return environ;
#endif
}
#endif  // _WIN32

}  // namespace

PemBundle SplitPemBundle(std::string const& contents) {
  static constexpr char kBegin[] = "-----BEGIN ";
  static constexpr char kDashes[] = "-----";
  PemBundle bundle;
  std::string::size_type pos = 0;
  while (true) {
    auto const begin = contents.find(kBegin, pos);
    if (begin == std::string::npos) break;
    auto const label_start = begin + sizeof(kBegin) - 1;
    auto const label_end = contents.find(kDashes, label_start);
    if (label_end == std::string::npos) break;
    std::string const label =
        contents.substr(label_start, label_end - label_start);
    std::string const end_marker = "-----END " + label + kDashes;
    auto end = contents.find(end_marker, label_end);
    if (end == std::string::npos) break;
    end += end_marker.size();
    std::string block = contents.substr(begin, end - begin) + "\n";
    if (label == "CERTIFICATE") {
      bundle.certificates += block;
    } else if (absl::StrContains(label, "PRIVATE KEY") &&
               bundle.private_key.empty()) {
      bundle.private_key = std::move(block);
      bundle.encrypted_private_key = absl::StrContains(label, "ENCRYPTED");
    }
    pos = end;
  }
  return bundle;
}

StatusRecordOr<std::vector<std::string>> ParseCertProviderCommand(
    std::string const& metadata_json) {
  auto const json = nlohmann::json::parse(metadata_json, /*cb=*/nullptr,
                                          /*allow_exceptions=*/false);
  if (json.is_discarded() || !json.is_object()) {
    return MakeError("The SecureConnect metadata is not a valid JSON object");
  }
  auto const command = json.find("cert_provider_command");
  if (command == json.end() || !command->is_array() || command->empty()) {
    return MakeError(
        "The SecureConnect metadata does not contain a valid "
        "`cert_provider_command` array");
  }
  std::vector<std::string> argv;
  for (auto const& argument : *command) {
    if (!argument.is_string()) {
      return MakeError("`cert_provider_command` must be an array of strings");
    }
    argv.push_back(argument.get<std::string>());
  }
  if (argv.front().empty()) {
    return MakeError("`cert_provider_command` has an empty program name");
  }
  return argv;
}

#ifdef _WIN32
StatusRecordOr<std::string> RunCommandCaptureStdout(
    std::vector<std::string> const& argv) {
  if (argv.empty() || argv.front().empty()) {
    return MakeError("Cannot run an empty command");
  }
  std::string command_line;
  for (auto const& argument : argv) {
    if (!command_line.empty()) command_line.push_back(' ');
    command_line += QuoteWindowsArgument(argument);
  }
  std::vector<char> mutable_command_line(command_line.begin(),
                                         command_line.end());
  mutable_command_line.push_back('\0');

  SECURITY_ATTRIBUTES security_attributes{};
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.bInheritHandle = TRUE;
  HANDLE read_end = nullptr;
  HANDLE write_end = nullptr;
  if (!CreatePipe(&read_end, &write_end, &security_attributes, 0)) {
    return MakeError("Could not create a pipe for the command output");
  }
  // The read end must not be inherited by the child, or we would never see the
  // end of its output.
  SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFOA startup_info{};
  startup_info.cb = sizeof(startup_info);
  startup_info.dwFlags = STARTF_USESTDHANDLES;
  startup_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  startup_info.hStdOutput = write_end;
  startup_info.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  PROCESS_INFORMATION process_info{};
  BOOL const created =
      CreateProcessA(nullptr, mutable_command_line.data(), nullptr, nullptr,
                     /*bInheritHandles=*/TRUE, CREATE_NO_WINDOW, nullptr,
                     nullptr, &startup_info, &process_info);
  DWORD const create_error = created ? 0 : GetLastError();
  CloseHandle(write_end);
  if (!created) {
    CloseHandle(read_end);
    return MakeError("Could not start the command `" + argv.front() +
                     "`, error code " + std::to_string(create_error));
  }

  std::string output;
  char buffer[4096];
  DWORD bytes_read = 0;
  while (::ReadFile(read_end, buffer, sizeof(buffer), &bytes_read, nullptr) &&
         bytes_read != 0) {
    output.append(buffer, bytes_read);
  }
  CloseHandle(read_end);

  WaitForSingleObject(process_info.hProcess, INFINITE);
  DWORD exit_code = 1;
  GetExitCodeProcess(process_info.hProcess, &exit_code);
  CloseHandle(process_info.hThread);
  CloseHandle(process_info.hProcess);
  if (exit_code != 0) {
    return MakeError("The command `" + argv.front() +
                     "` failed with exit code " + std::to_string(exit_code));
  }
  return output;
}
#else
StatusRecordOr<std::string> RunCommandCaptureStdout(
    std::vector<std::string> const& argv) {
  if (argv.empty() || argv.front().empty()) {
    return MakeError("Cannot run an empty command");
  }
  int fds[2];
#ifdef __linux__
  if (pipe2(fds, O_CLOEXEC) != 0) {
#else
  if (pipe(fds) != 0) {
#endif
    return MakeError(std::string("Could not create a pipe for the command "
                                 "output: ") +
                     std::strerror(errno));
  }
  // Do not leak the pipe into other processes the application may spawn.
  static_cast<void>(fcntl(fds[0], F_SETFD, FD_CLOEXEC));
  static_cast<void>(fcntl(fds[1], F_SETFD, FD_CLOEXEC));

  std::vector<char*> args;
  args.reserve(argv.size() + 1);
  for (auto const& argument : argv) {
    args.push_back(const_cast<char*>(argument.c_str()));
  }
  args.push_back(nullptr);

  // posix_spawn() is safe to use in multi-threaded processes (such as the
  // applications loading this driver) and does not involve a shell.
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO);
  pid_t pid = 0;
  int const rc =
      posix_spawnp(&pid, args[0], &actions, nullptr, args.data(), Environ());
  posix_spawn_file_actions_destroy(&actions);
  close(fds[1]);
  if (rc != 0) {
    close(fds[0]);
    return MakeError("Could not start the command `" + argv.front() +
                     "`: " + std::strerror(rc));
  }

  std::string output;
  char buffer[4096];
  while (true) {
    ssize_t const n = read(fds[0], buffer, sizeof(buffer));
    if (n > 0) {
      output.append(buffer, static_cast<std::size_t>(n));
      continue;
    }
    if (n < 0 && errno == EINTR) continue;
    break;
  }
  close(fds[0]);

  int status = 0;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) {
      return MakeError("Could not wait for the command `" + argv.front() +
                       "`: " + std::strerror(errno));
    }
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    return MakeError(
        "The command `" + argv.front() + "` failed" +
        (WIFEXITED(status)
             ? " with exit code " + std::to_string(WEXITSTATUS(status))
             : std::string(" (terminated)")));
  }
  return output;
}
#endif  // _WIN32

StatusRecordOr<SslCertificate> LoadClientCertificateFromFiles(
    std::string const& certificate_path, std::string const& private_key_path) {
  auto certificate_contents = ReadFileContents(certificate_path);
  if (!certificate_contents) return certificate_contents.GetStatusRecord();
  auto bundle = SplitPemBundle(*certificate_contents);
  if (!private_key_path.empty()) {
    auto key_contents = ReadFileContents(private_key_path);
    if (!key_contents) return key_contents.GetStatusRecord();
    auto const key_bundle = SplitPemBundle(*key_contents);
    bundle.private_key = key_bundle.private_key;
    bundle.encrypted_private_key = key_bundle.encrypted_private_key;
  }
  return MakeSslCertificate(bundle, "the client certificate file(s)");
}

StatusRecordOr<absl::optional<SslCertificate>> LoadDefaultClientCertificate() {
  auto const home = HomeDirectory();
  if (home.empty()) {
    LOG(INFO) << "mTLS:: The home directory is unknown, no default client "
                 "certificate is available";
    return absl::optional<SslCertificate>{};
  }
  auto const metadata_path =
      (std::filesystem::path(home) / kSecureConnectMetadataPath).string();
  std::ifstream is(metadata_path, std::ios::binary);
  if (!is) {
    LOG(INFO) << "mTLS:: No SecureConnect metadata found at " << metadata_path
              << ", no default client certificate is available";
    return absl::optional<SslCertificate>{};
  }
  std::string const metadata((std::istreambuf_iterator<char>(is)),
                             std::istreambuf_iterator<char>());
  auto argv = ParseCertProviderCommand(metadata);
  if (!argv) return argv.GetStatusRecord();
  LOG(INFO) << "mTLS:: Running the SecureConnect cert provider command: "
            << argv->front();
  auto output = RunCommandCaptureStdout(*argv);
  if (!output) return output.GetStatusRecord();
  auto certificate = MakeSslCertificate(SplitPemBundle(*output),
                                        "the cert provider command output");
  if (!certificate) return certificate.GetStatusRecord();
  return absl::optional<SslCertificate>(*std::move(certificate));
}

StatusRecordOr<MtlsConfig> ResolveMtlsConfig(MtlsOptions const& options,
                                             bool is_default_universe) {
  // 1. Determine whether a client certificate should be used (AIP-4114).
  bool use_client_certificate = !options.client_certificate_path.empty();
  if (auto value = SettingOrEnv(options.use_client_certificate,
                                kUseClientCertificateEnvVar)) {
    auto const lower = ToLower(*value);
    if (lower == "true" || lower == "1") {
      use_client_certificate = true;
    } else if (lower == "false" || lower == "0") {
      use_client_certificate = false;
    } else {
      return MakeError(
          "Invalid value `" + *value + "` for UseClientCertificate / " +
          kUseClientCertificateEnvVar + ", expected true or false");
    }
  }

  // 2. Which endpoint should be used
  auto mode = MtlsEndpointMode::kAuto;
  if (auto value =
          SettingOrEnv(options.use_mtls_endpoint, kUseMtlsEndpointEnvVar)) {
    auto const lower = ToLower(*value);
    if (lower == "auto") {
      mode = MtlsEndpointMode::kAuto;
    } else if (lower == "always") {
      mode = MtlsEndpointMode::kAlways;
    } else if (lower == "never") {
      mode = MtlsEndpointMode::kNever;
    } else {
      return MakeError("Invalid value `" + *value + "` for UseMtlsEndpoint / " +
                       kUseMtlsEndpointEnvVar +
                       ", expected auto, always or never");
    }
  }

  // 3. Load client certificate (DSN path takes priority over default device
  // cert).
  MtlsConfig config;
  if (use_client_certificate) {
    if (!options.client_certificate_path.empty()) {
      LOG(INFO) << "mTLS:: Loading the client certificate from "
                << options.client_certificate_path;
      auto certificate = LoadClientCertificateFromFiles(
          options.client_certificate_path, options.client_private_key_path);
      if (!certificate) return certificate.GetStatusRecord();
      config.client_certificate = *std::move(certificate);
    } else {
      auto certificate = LoadDefaultClientCertificate();
      if (!certificate) return certificate.GetStatusRecord();
      config.client_certificate = *std::move(certificate);
    }
  }

  // 4. Resolve mTLS endpoint routing.
  config.use_mtls_endpoint = mode == MtlsEndpointMode::kAlways ||
                             (mode == MtlsEndpointMode::kAuto &&
                              config.client_certificate.has_value());
  if (config.use_mtls_endpoint && !is_default_universe) {
    return MakeError(std::string("mTLS is not supported in any universe other "
                                 "than ") +
                     kDefaultUniverseDomain);
  }
  LOG(INFO) << "mTLS:: client certificate "
            << (config.client_certificate ? "enabled" : "disabled")
            << ", mTLS endpoints "
            << (config.use_mtls_endpoint ? "enabled" : "disabled");
  return config;
}

std::shared_ptr<grpc::ChannelCredentials> MakeMtlsGrpcChannelCredentials(
    SslCertificate const& certificate, std::string const& ca_roots_file_path,
    std::shared_ptr<AccessTokenGenerator> generator) {
  grpc::SslCredentialsOptions ssl_options;
  if (!ca_roots_file_path.empty()) {
    auto roots = ReadFileContents(ca_roots_file_path);
    if (roots) ssl_options.pem_root_certs = *std::move(roots);
  }
  ssl_options.pem_cert_chain = certificate.ssl_certificate();
  ssl_options.pem_private_key = certificate.ssl_private_key();
  return grpc::CompositeChannelCredentials(
      grpc::SslCredentials(ssl_options),
      grpc::MetadataCredentialsFromPlugin(
          std::make_unique<AccessTokenGeneratorPlugin>(std::move(generator))));
}

}  // namespace google::cloud::odbc_bigquery_client_interface
