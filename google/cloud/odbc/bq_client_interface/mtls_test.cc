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
#include "google/cloud/odbc/bq_client_interface/setenv.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/oauth2/access_token_generator.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace google::cloud::odbc_bigquery_client_interface {
namespace {

using ::google::cloud::experimental::SslCertificate;
using ::google::cloud::internal::GetEnv;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_testing_utils::StatusRecordIs;
using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

// These are not real keys, just syntactically valid PEM blocks.
auto constexpr kCertificate =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBfakeleafcertificate\n"
    "-----END CERTIFICATE-----\n";
auto constexpr kIntermediateCertificate =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBfakeintermediatecertificate\n"
    "-----END CERTIFICATE-----\n";
auto constexpr kPrivateKey =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIIEfakeprivatekey\n"
    "-----END PRIVATE KEY-----\n";
auto constexpr kRsaPrivateKey =
    "-----BEGIN RSA PRIVATE KEY-----\n"
    "MIIEfakersaprivatekey\n"
    "-----END RSA PRIVATE KEY-----\n";
auto constexpr kEcPrivateKey =
    "-----BEGIN EC PRIVATE KEY-----\n"
    "MHcfakeecprivatekey\n"
    "-----END EC PRIVATE KEY-----\n";
auto constexpr kEncryptedPrivateKey =
    "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
    "MIIFfakeencryptedprivatekey\n"
    "-----END ENCRYPTED PRIVATE KEY-----\n";

#ifdef _WIN32
auto constexpr kHomeEnvVar = "USERPROFILE";
#else
auto constexpr kHomeEnvVar = "HOME";
#endif

// Creates a unique, empty directory that is removed on destruction.
class ScopedTempDir {
 public:
  ScopedTempDir() {
    std::random_device rd;
    path_ = std::filesystem::temp_directory_path() /
            ("odbc-mtls-test-" + std::to_string(rd()));
    std::filesystem::create_directories(path_);
  }
  ~ScopedTempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }
  ScopedTempDir(ScopedTempDir const&) = delete;
  ScopedTempDir& operator=(ScopedTempDir const&) = delete;

  [[nodiscard]] std::filesystem::path const& path() const { return path_; }

  // Writes @p contents to @p relative_path (creating parent directories) and
  // returns the full path.
  [[nodiscard]] std::string WriteFile(std::string const& relative_path,
                                      std::string const& contents) const {
    auto const full_path = path_ / relative_path;
    std::filesystem::create_directories(full_path.parent_path());
    std::ofstream(full_path, std::ios::binary) << contents;
    return full_path.string();
  }

 private:
  std::filesystem::path path_;
};

// Sets (or unsets) an environment variable and restores it on destruction.
class ScopedEnv {
 public:
  ScopedEnv(char const* variable, absl::optional<std::string> value)
      : variable_(variable), previous_(GetEnv(variable)) {
    SetEnv(variable_, std::move(value));
  }
  ~ScopedEnv() { SetEnv(variable_, previous_); }
  ScopedEnv(ScopedEnv const&) = delete;
  ScopedEnv& operator=(ScopedEnv const&) = delete;

 private:
  char const* variable_;
  absl::optional<std::string> previous_;
};

// A command (without shell) printing the contents of @p path to stdout.
std::vector<std::string> CatCommand(std::string const& path) {
#ifdef _WIN32
  return {"cmd", "/c", "type", path};
#else
  return {"cat", path};
#endif
}

std::string JsonArray(std::vector<std::string> const& values) {
  std::string json = "[";
  for (auto const& value : values) {
    if (json.size() > 1) json += ", ";
    json += "\"";
    for (auto c : value) {
      if (c == '\\' || c == '"') json += '\\';
      json += c;
    }
    json += "\"";
  }
  return json + "]";
}

// Writes a SecureConnect metadata file under @p home whose cert provider
// command prints @p pem_contents.
void WriteSecureConnectMetadata(ScopedTempDir const& home,
                                std::string const& pem_contents) {
  auto const pem_path = home.WriteFile("device.pem", pem_contents);
  home.WriteFile(kSecureConnectMetadataPath,
                 R"({"cert_provider_command": )" +
                     JsonArray(CatCommand(pem_path)) +
                     R"(, "device_resource_ids": ["id"]})");
}

class FakeAccessTokenGenerator : public oauth2::AccessTokenGenerator {
 public:
  StatusOr<AccessToken> GetToken() override {
    return AccessToken{
        "fake-token", std::chrono::system_clock::now() + std::chrono::hours(1)};
  }
};

TEST(SplitPemBundle, CertificateFollowedByKey) {
  auto const bundle = SplitPemBundle(std::string(kCertificate) + kPrivateKey);
  EXPECT_EQ(bundle.certificates, kCertificate);
  EXPECT_EQ(bundle.private_key, kPrivateKey);
  EXPECT_FALSE(bundle.encrypted_private_key);
}

TEST(SplitPemBundle, KeyFollowedByCertificateChain) {
  auto const bundle = SplitPemBundle(std::string(kPrivateKey) + kCertificate +
                                     kIntermediateCertificate);
  EXPECT_EQ(bundle.certificates,
            std::string(kCertificate) + kIntermediateCertificate);
  EXPECT_EQ(bundle.private_key, kPrivateKey);
  EXPECT_FALSE(bundle.encrypted_private_key);
}

TEST(SplitPemBundle, RecognizesLegacyKeyLabels) {
  EXPECT_EQ(SplitPemBundle(kRsaPrivateKey).private_key, kRsaPrivateKey);
  EXPECT_EQ(SplitPemBundle(kEcPrivateKey).private_key, kEcPrivateKey);
}

TEST(SplitPemBundle, DetectsEncryptedKey) {
  auto const bundle = SplitPemBundle(kEncryptedPrivateKey);
  EXPECT_EQ(bundle.private_key, kEncryptedPrivateKey);
  EXPECT_TRUE(bundle.encrypted_private_key);
}

TEST(SplitPemBundle, OnlyFirstKeyIsUsed) {
  auto const bundle = SplitPemBundle(std::string(kPrivateKey) + kRsaPrivateKey);
  EXPECT_EQ(bundle.private_key, kPrivateKey);
}

TEST(SplitPemBundle, IgnoresNoiseAndTruncatedBlocks) {
  auto const bundle = SplitPemBundle(
      "Bag Attributes\n    friendlyName: device\n" + std::string(kCertificate) +
      "-----BEGIN PRIVATE KEY-----\nMIIEtruncated");
  EXPECT_EQ(bundle.certificates, kCertificate);
  EXPECT_THAT(bundle.private_key, IsEmpty());
}

TEST(SplitPemBundle, Empty) {
  auto const bundle = SplitPemBundle("");
  EXPECT_THAT(bundle.certificates, IsEmpty());
  EXPECT_THAT(bundle.private_key, IsEmpty());
  EXPECT_FALSE(bundle.encrypted_private_key);
}

TEST(ParseCertProviderCommand, Success) {
  auto const argv = ParseCertProviderCommand(R"({
    "cert_provider_command": ["/opt/ev/bin/apihelper", "--print_certificate"],
    "device_resource_ids": ["projects/1234/devices/abcd"]
  })");
  ASSERT_STATUS_RECORD_OK(argv);
  EXPECT_THAT(*argv,
              ElementsAre("/opt/ev/bin/apihelper", "--print_certificate"));
}

TEST(ParseCertProviderCommand, InvalidJson) {
  auto const argv = ParseCertProviderCommand("{not json");
  ASSERT_FALSE(argv.Ok());
  EXPECT_THAT(argv, StatusRecordIs(SQLStates::k_HY000(),
                                   HasSubstr("not a valid JSON object")));
}

TEST(ParseCertProviderCommand, NotAnObject) {
  auto const argv = ParseCertProviderCommand(R"(["cmd"])");
  ASSERT_FALSE(argv.Ok());
  EXPECT_THAT(argv, StatusRecordIs(SQLStates::k_HY000(),
                                   HasSubstr("not a valid JSON object")));
}

TEST(ParseCertProviderCommand, MissingCommand) {
  auto const argv = ParseCertProviderCommand(R"({"device_resource_ids": []})");
  ASSERT_FALSE(argv.Ok());
  EXPECT_THAT(argv, StatusRecordIs(SQLStates::k_HY000(),
                                   HasSubstr("cert_provider_command")));
}

TEST(ParseCertProviderCommand, CommandIsNotAnArray) {
  auto const argv =
      ParseCertProviderCommand(R"({"cert_provider_command": "cmd --flag"})");
  ASSERT_FALSE(argv.Ok());
  EXPECT_THAT(argv, StatusRecordIs(SQLStates::k_HY000(),
                                   HasSubstr("cert_provider_command")));
}

TEST(ParseCertProviderCommand, EmptyArray) {
  auto const argv =
      ParseCertProviderCommand(R"({"cert_provider_command": []})");
  ASSERT_FALSE(argv.Ok());
  EXPECT_THAT(argv, StatusRecordIs(SQLStates::k_HY000(),
                                   HasSubstr("cert_provider_command")));
}

TEST(ParseCertProviderCommand, NonStringArgument) {
  auto const argv =
      ParseCertProviderCommand(R"({"cert_provider_command": ["cmd", 1]})");
  ASSERT_FALSE(argv.Ok());
  EXPECT_THAT(argv, StatusRecordIs(SQLStates::k_HY000(),
                                   HasSubstr("array of strings")));
}

TEST(ParseCertProviderCommand, EmptyProgramName) {
  auto const argv =
      ParseCertProviderCommand(R"({"cert_provider_command": ["", "--flag"]})");
  ASSERT_FALSE(argv.Ok());
  EXPECT_THAT(argv, StatusRecordIs(SQLStates::k_HY000(),
                                   HasSubstr("empty program name")));
}

TEST(RunCommandCaptureStdout, EmptyCommand) {
  auto const output = RunCommandCaptureStdout({});
  ASSERT_FALSE(output.Ok());
  EXPECT_THAT(output,
              StatusRecordIs(SQLStates::k_HY000(), HasSubstr("empty command")));
}

TEST(RunCommandCaptureStdout, CapturesOutput) {
  ScopedTempDir dir;
  auto const path = dir.WriteFile("input.txt", "hello world");
  auto const output = RunCommandCaptureStdout(CatCommand(path));
  ASSERT_STATUS_RECORD_OK(output);
  EXPECT_EQ(*output, "hello world");
}

TEST(RunCommandCaptureStdout, ArgumentsWithSpacesAndQuotes) {
  ScopedTempDir dir;
  auto const path = dir.WriteFile("with space.txt", "content \"quoted\"");
  auto const output = RunCommandCaptureStdout(CatCommand(path));
  ASSERT_STATUS_RECORD_OK(output);
  EXPECT_EQ(*output, "content \"quoted\"");
}

TEST(RunCommandCaptureStdout, LargeOutputDoesNotDeadlock) {
  // Much more than the pipe buffer (typically 64KiB).
  auto constexpr kSize = std::size_t{1} << 20;
  ScopedTempDir dir;
  auto const path = dir.WriteFile("large.txt", std::string(kSize, 'x'));
  auto const output = RunCommandCaptureStdout(CatCommand(path));
  ASSERT_STATUS_RECORD_OK(output);
  EXPECT_EQ(output->size(), kSize);
}

TEST(RunCommandCaptureStdout, NonZeroExitCode) {
#ifdef _WIN32
  auto const output = RunCommandCaptureStdout({"cmd", "/c", "exit", "3"});
#else
  auto const output = RunCommandCaptureStdout({"sh", "-c", "exit 3"});
#endif
  ASSERT_FALSE(output.Ok());
  EXPECT_THAT(output,
              StatusRecordIs(SQLStates::k_HY000(), HasSubstr("exit code 3")));
}

TEST(RunCommandCaptureStdout, MissingProgram) {
  auto const output = RunCommandCaptureStdout(
      {"odbc-mtls-test-this-program-does-not-exist", "--flag"});
  ASSERT_FALSE(output.Ok());
  // Depending on the platform the failure is detected when spawning the
  // process or reported as a non-zero exit code.
  EXPECT_THAT(output,
              StatusRecordIs(SQLStates::k_HY000(),
                             AnyOf(HasSubstr("Could not start"),
                                   HasSubstr("failed with exit code"))));
}

TEST(LoadClientCertificateFromFiles, CombinedFile) {
  ScopedTempDir dir;
  auto const path =
      dir.WriteFile("client.pem", std::string(kCertificate) + kPrivateKey);
  auto const certificate = LoadClientCertificateFromFiles(path, "");
  ASSERT_STATUS_RECORD_OK(certificate);
  EXPECT_EQ(certificate->ssl_certificate(), kCertificate);
  EXPECT_EQ(certificate->ssl_private_key(), kPrivateKey);
  EXPECT_EQ(certificate->ssl_certificate_type(),
            SslCertificate::SslCertificateType::kPEM);
}

TEST(LoadClientCertificateFromFiles, SeparateFiles) {
  ScopedTempDir dir;
  auto const certificate_path = dir.WriteFile(
      "client.crt", std::string(kCertificate) + kIntermediateCertificate);
  auto const key_path = dir.WriteFile("client.key", kEcPrivateKey);
  auto const certificate =
      LoadClientCertificateFromFiles(certificate_path, key_path);
  ASSERT_STATUS_RECORD_OK(certificate);
  EXPECT_EQ(certificate->ssl_certificate(),
            std::string(kCertificate) + kIntermediateCertificate);
  EXPECT_EQ(certificate->ssl_private_key(), kEcPrivateKey);
}

TEST(LoadClientCertificateFromFiles, SeparateKeyFileOverridesEmbeddedKey) {
  ScopedTempDir dir;
  auto const certificate_path =
      dir.WriteFile("client.pem", std::string(kCertificate) + kPrivateKey);
  auto const key_path = dir.WriteFile("client.key", kRsaPrivateKey);
  auto const certificate =
      LoadClientCertificateFromFiles(certificate_path, key_path);
  ASSERT_STATUS_RECORD_OK(certificate);
  EXPECT_EQ(certificate->ssl_private_key(), kRsaPrivateKey);
}

TEST(LoadClientCertificateFromFiles, MissingCertificateFile) {
  ScopedTempDir dir;
  auto const certificate =
      LoadClientCertificateFromFiles((dir.path() / "missing.pem").string(), "");
  ASSERT_FALSE(certificate.Ok());
  EXPECT_THAT(certificate, StatusRecordIs(SQLStates::k_HY000(),
                                          HasSubstr("Could not open file")));
}

TEST(LoadClientCertificateFromFiles, MissingKeyFile) {
  ScopedTempDir dir;
  auto const certificate_path = dir.WriteFile("client.crt", kCertificate);
  auto const certificate = LoadClientCertificateFromFiles(
      certificate_path, (dir.path() / "missing.key").string());
  ASSERT_FALSE(certificate.Ok());
  EXPECT_THAT(certificate, StatusRecordIs(SQLStates::k_HY000(),
                                          HasSubstr("Could not open file")));
}

TEST(LoadClientCertificateFromFiles, NoCertificateBlock) {
  ScopedTempDir dir;
  auto const path = dir.WriteFile("client.pem", kPrivateKey);
  auto const certificate = LoadClientCertificateFromFiles(path, "");
  ASSERT_FALSE(certificate.Ok());
  EXPECT_THAT(certificate,
              StatusRecordIs(SQLStates::k_HY000(),
                             HasSubstr("No PEM CERTIFICATE block")));
}

TEST(LoadClientCertificateFromFiles, NoPrivateKeyBlock) {
  ScopedTempDir dir;
  auto const path = dir.WriteFile("client.pem", kCertificate);
  auto const certificate = LoadClientCertificateFromFiles(path, "");
  ASSERT_FALSE(certificate.Ok());
  EXPECT_THAT(certificate,
              StatusRecordIs(SQLStates::k_HY000(),
                             HasSubstr("No PEM PRIVATE KEY block")));
}

TEST(LoadClientCertificateFromFiles, EncryptedPrivateKey) {
  ScopedTempDir dir;
  auto const certificate_path = dir.WriteFile("client.crt", kCertificate);
  auto const key_path = dir.WriteFile("client.key", kEncryptedPrivateKey);
  auto const certificate =
      LoadClientCertificateFromFiles(certificate_path, key_path);
  ASSERT_FALSE(certificate.Ok());
  EXPECT_THAT(certificate, StatusRecordIs(SQLStates::k_HY000(),
                                          HasSubstr("Encrypted private keys")));
}

TEST(LoadDefaultClientCertificate, NoMetadataFile) {
  ScopedTempDir home;
  ScopedEnv home_env(kHomeEnvVar, home.path().string());
  auto const certificate = LoadDefaultClientCertificate();
  ASSERT_STATUS_RECORD_OK(certificate);
  EXPECT_FALSE(certificate->has_value());
}

TEST(LoadDefaultClientCertificate, UnknownHomeDirectory) {
  ScopedEnv home_env(kHomeEnvVar, absl::nullopt);
  auto const certificate = LoadDefaultClientCertificate();
  ASSERT_STATUS_RECORD_OK(certificate);
  EXPECT_FALSE(certificate->has_value());
}

TEST(LoadDefaultClientCertificate, RunsCertProviderCommand) {
  ScopedTempDir home;
  ScopedEnv home_env(kHomeEnvVar, home.path().string());
  WriteSecureConnectMetadata(home, std::string(kCertificate) + kPrivateKey);
  auto const certificate = LoadDefaultClientCertificate();
  ASSERT_STATUS_RECORD_OK(certificate);
  ASSERT_TRUE(certificate->has_value());
  EXPECT_EQ((*certificate)->ssl_certificate(), kCertificate);
  EXPECT_EQ((*certificate)->ssl_private_key(), kPrivateKey);
}

TEST(LoadDefaultClientCertificate, InvalidMetadata) {
  ScopedTempDir home;
  ScopedEnv home_env(kHomeEnvVar, home.path().string());
  home.WriteFile(kSecureConnectMetadataPath, "{}");
  auto const certificate = LoadDefaultClientCertificate();
  ASSERT_FALSE(certificate.Ok());
  EXPECT_THAT(certificate, StatusRecordIs(SQLStates::k_HY000(),
                                          HasSubstr("cert_provider_command")));
}

TEST(LoadDefaultClientCertificate, CommandOutputWithoutKey) {
  ScopedTempDir home;
  ScopedEnv home_env(kHomeEnvVar, home.path().string());
  WriteSecureConnectMetadata(home, kCertificate);
  auto const certificate = LoadDefaultClientCertificate();
  ASSERT_FALSE(certificate.Ok());
  EXPECT_THAT(certificate,
              StatusRecordIs(SQLStates::k_HY000(),
                             HasSubstr("No PEM PRIVATE KEY block")));
}

TEST(LoadDefaultClientCertificate, CommandFails) {
  ScopedTempDir home;
  ScopedEnv home_env(kHomeEnvVar, home.path().string());
  home.WriteFile(
      kSecureConnectMetadataPath,
      R"({"cert_provider_command": )" +
          JsonArray(CatCommand((home.path() / "missing.pem").string())) + "}");
  auto const certificate = LoadDefaultClientCertificate();
  ASSERT_FALSE(certificate.Ok());
  EXPECT_THAT(certificate, StatusRecordIs(SQLStates::k_HY000(),
                                          HasSubstr("failed with exit code")));
}

// Isolates `ResolveMtlsConfig()` from the environment of the machine running
// the tests: the AIP-4114 variables are unset and the home directory is empty.
class ResolveMtlsConfigTest : public ::testing::Test {
 protected:
  ResolveMtlsConfigTest()
      : home_env_(kHomeEnvVar, home_.path().string()),
        use_client_certificate_env_(kUseClientCertificateEnvVar, absl::nullopt),
        use_mtls_endpoint_env_(kUseMtlsEndpointEnvVar, absl::nullopt) {}

  [[nodiscard]] std::string WriteClientCertificate() const {
    return home_.WriteFile("client.pem",
                           std::string(kCertificate) + kPrivateKey);
  }

  ScopedTempDir home_;
  ScopedEnv home_env_;
  ScopedEnv use_client_certificate_env_;
  ScopedEnv use_mtls_endpoint_env_;
};

TEST_F(ResolveMtlsConfigTest, DisabledByDefault) {
  auto const config = ResolveMtlsConfig(MtlsOptions{}, true);
  ASSERT_STATUS_RECORD_OK(config);
  EXPECT_FALSE(config->client_certificate.has_value());
  EXPECT_FALSE(config->use_mtls_endpoint);
}

TEST_F(ResolveMtlsConfigTest, DsnCertificateEnablesMtls) {
  MtlsOptions options;
  options.client_certificate_path = WriteClientCertificate();
  auto const config = ResolveMtlsConfig(options, true);
  ASSERT_STATUS_RECORD_OK(config);
  ASSERT_TRUE(config->client_certificate.has_value());
  EXPECT_EQ(config->client_certificate->ssl_certificate(), kCertificate);
  EXPECT_EQ(config->client_certificate->ssl_private_key(), kPrivateKey);
  EXPECT_TRUE(config->use_mtls_endpoint);
}

TEST_F(ResolveMtlsConfigTest, DsnSeparateKeyFile) {
  MtlsOptions options;
  options.client_certificate_path = home_.WriteFile("client.crt", kCertificate);
  options.client_private_key_path =
      home_.WriteFile("client.key", kEcPrivateKey);
  auto const config = ResolveMtlsConfig(options, true);
  ASSERT_STATUS_RECORD_OK(config);
  ASSERT_TRUE(config->client_certificate.has_value());
  EXPECT_EQ(config->client_certificate->ssl_private_key(), kEcPrivateKey);
  EXPECT_TRUE(config->use_mtls_endpoint);
}

TEST_F(ResolveMtlsConfigTest, DsnMissingCertificateFileIsAnError) {
  MtlsOptions options;
  options.client_certificate_path = (home_.path() / "missing.pem").string();
  auto const config = ResolveMtlsConfig(options, true);
  ASSERT_FALSE(config.Ok());
  EXPECT_THAT(config, StatusRecordIs(SQLStates::k_HY000(),
                                     HasSubstr("Could not open file")));
}

TEST_F(ResolveMtlsConfigTest, ExplicitFalseDisablesDsnCertificate) {
  MtlsOptions options;
  options.use_client_certificate = "false";
  options.client_certificate_path = WriteClientCertificate();
  auto const config = ResolveMtlsConfig(options, true);
  ASSERT_STATUS_RECORD_OK(config);
  EXPECT_FALSE(config->client_certificate.has_value());
  EXPECT_FALSE(config->use_mtls_endpoint);
}

TEST_F(ResolveMtlsConfigTest, EnvironmentFalseDisablesDsnCertificate) {
  ScopedEnv env(kUseClientCertificateEnvVar, "0");
  MtlsOptions options;
  options.client_certificate_path = WriteClientCertificate();
  auto const config = ResolveMtlsConfig(options, true);
  ASSERT_STATUS_RECORD_OK(config);
  EXPECT_FALSE(config->client_certificate.has_value());
  EXPECT_FALSE(config->use_mtls_endpoint);
}

TEST_F(ResolveMtlsConfigTest, DsnOverridesEnvironment) {
  ScopedEnv use_client_certificate_env(kUseClientCertificateEnvVar, "false");
  ScopedEnv use_mtls_endpoint_env(kUseMtlsEndpointEnvVar, "always");
  MtlsOptions options;
  options.use_client_certificate = "TRUE";
  options.use_mtls_endpoint = "Never";
  options.client_certificate_path = WriteClientCertificate();
  auto const config = ResolveMtlsConfig(options, true);
  ASSERT_STATUS_RECORD_OK(config);
  EXPECT_TRUE(config->client_certificate.has_value());
  EXPECT_FALSE(config->use_mtls_endpoint);
}

TEST_F(ResolveMtlsConfigTest, InvalidUseClientCertificate) {
  MtlsOptions options;
  options.use_client_certificate = "maybe";
  auto const config = ResolveMtlsConfig(options, true);
  ASSERT_FALSE(config.Ok());
  EXPECT_THAT(config, StatusRecordIs(SQLStates::k_HY000(),
                                     HasSubstr("UseClientCertificate")));
}

TEST_F(ResolveMtlsConfigTest, InvalidUseClientCertificateEnvironment) {
  ScopedEnv env(kUseClientCertificateEnvVar, "yes");
  auto const config = ResolveMtlsConfig(MtlsOptions{}, true);
  ASSERT_FALSE(config.Ok());
  EXPECT_THAT(config, StatusRecordIs(SQLStates::k_HY000(),
                                     HasSubstr(kUseClientCertificateEnvVar)));
}

TEST_F(ResolveMtlsConfigTest, InvalidUseMtlsEndpoint) {
  MtlsOptions options;
  options.use_mtls_endpoint = "sometimes";
  auto const config = ResolveMtlsConfig(options, true);
  ASSERT_FALSE(config.Ok());
  EXPECT_THAT(config, StatusRecordIs(SQLStates::k_HY000(),
                                     HasSubstr("UseMtlsEndpoint")));
}

TEST_F(ResolveMtlsConfigTest, AlwaysWithoutCertificate) {
  MtlsOptions options;
  options.use_mtls_endpoint = "always";
  auto const config = ResolveMtlsConfig(options, true);
  ASSERT_STATUS_RECORD_OK(config);
  EXPECT_FALSE(config->client_certificate.has_value());
  EXPECT_TRUE(config->use_mtls_endpoint);
}

TEST_F(ResolveMtlsConfigTest, EnvironmentAlways) {
  ScopedEnv env(kUseMtlsEndpointEnvVar, "always");
  auto const config = ResolveMtlsConfig(MtlsOptions{}, true);
  ASSERT_STATUS_RECORD_OK(config);
  EXPECT_FALSE(config->client_certificate.has_value());
  EXPECT_TRUE(config->use_mtls_endpoint);
}

TEST_F(ResolveMtlsConfigTest, NeverWithCertificate) {
  MtlsOptions options;
  options.use_mtls_endpoint = "never";
  options.client_certificate_path = WriteClientCertificate();
  auto const config = ResolveMtlsConfig(options, true);
  ASSERT_STATUS_RECORD_OK(config);
  EXPECT_TRUE(config->client_certificate.has_value());
  EXPECT_FALSE(config->use_mtls_endpoint);
}

TEST_F(ResolveMtlsConfigTest, MtlsEndpointRequiresDefaultUniverse) {
  MtlsOptions options;
  options.use_mtls_endpoint = "always";
  auto const config = ResolveMtlsConfig(options, false);
  ASSERT_FALSE(config.Ok());
  EXPECT_THAT(config,
              StatusRecordIs(SQLStates::k_HY000(), HasSubstr("universe")));
}

TEST_F(ResolveMtlsConfigTest, CertificateWithoutMtlsEndpointInOtherUniverse) {
  MtlsOptions options;
  options.use_mtls_endpoint = "never";
  options.client_certificate_path = WriteClientCertificate();
  auto const config = ResolveMtlsConfig(options, false);
  ASSERT_STATUS_RECORD_OK(config);
  EXPECT_TRUE(config->client_certificate.has_value());
  EXPECT_FALSE(config->use_mtls_endpoint);
}

TEST_F(ResolveMtlsConfigTest, TrueWithoutDefaultCertificate) {
  MtlsOptions options;
  options.use_client_certificate = "true";
  auto const config = ResolveMtlsConfig(options, true);
  ASSERT_STATUS_RECORD_OK(config);
  EXPECT_FALSE(config->client_certificate.has_value());
  EXPECT_FALSE(config->use_mtls_endpoint);
}

TEST_F(ResolveMtlsConfigTest, TrueUsesDefaultCertificate) {
  WriteSecureConnectMetadata(home_, std::string(kCertificate) + kPrivateKey);
  ScopedEnv env(kUseClientCertificateEnvVar, "true");
  auto const config = ResolveMtlsConfig(MtlsOptions{}, true);
  ASSERT_STATUS_RECORD_OK(config);
  ASSERT_TRUE(config->client_certificate.has_value());
  EXPECT_EQ(config->client_certificate->ssl_certificate(), kCertificate);
  EXPECT_EQ(config->client_certificate->ssl_private_key(), kPrivateKey);
  EXPECT_TRUE(config->use_mtls_endpoint);
}

TEST_F(ResolveMtlsConfigTest, DsnCertificateTakesPriorityOverDefault) {
  WriteSecureConnectMetadata(
      home_, std::string(kIntermediateCertificate) + kRsaPrivateKey);
  MtlsOptions options;
  options.use_client_certificate = "1";
  options.client_certificate_path = WriteClientCertificate();
  auto const config = ResolveMtlsConfig(options, true);
  ASSERT_STATUS_RECORD_OK(config);
  ASSERT_TRUE(config->client_certificate.has_value());
  EXPECT_EQ(config->client_certificate->ssl_certificate(), kCertificate);
  EXPECT_EQ(config->client_certificate->ssl_private_key(), kPrivateKey);
}

TEST(MakeMtlsGrpcChannelCredentials, WithoutRoots) {
  auto const credentials = MakeMtlsGrpcChannelCredentials(
      SslCertificate(kCertificate, kPrivateKey), "",
      std::make_shared<FakeAccessTokenGenerator>());
  EXPECT_NE(credentials, nullptr);
}

TEST(MakeMtlsGrpcChannelCredentials, WithRoots) {
  ScopedTempDir dir;
  auto const roots = dir.WriteFile("roots.pem", kIntermediateCertificate);
  auto const credentials = MakeMtlsGrpcChannelCredentials(
      SslCertificate(kCertificate, kPrivateKey), roots,
      std::make_shared<FakeAccessTokenGenerator>());
  EXPECT_NE(credentials, nullptr);
}

}  // namespace
}  // namespace google::cloud::odbc_bigquery_client_interface
