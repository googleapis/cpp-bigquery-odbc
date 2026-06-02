# Patch script to fix MSVC compilation errors in google-cloud-cpp dependency.
# This script is executed via CMake -P during the FetchContent patch step.

macro (patch_file file_path search_str replace_str)
    if (EXISTS "${file_path}")
        file(READ "${file_path}" contents)
        string(FIND "${contents}" "${search_str}" index)
        if (NOT index EQUAL -1)
            string(REPLACE "${search_str}" "${replace_str}" contents
                           "${contents}")
            file(WRITE "${file_path}" "${contents}")
            message(
                STATUS
                    "[google-cloud-cpp Patch] Successfully patched ${file_path}"
            )
        else ()
            message(
                STATUS
                    "[google-cloud-cpp Patch] ${file_path} is already patched or target string not found"
            )
        endif ()
    else ()
        message(
            FATAL_ERROR
                "[google-cloud-cpp Patch] Required file ${file_path} does not exist"
        )
    endif ()
endmacro ()

# Patch both affected files in the extracted source directory
patch_file("google/cloud/internal/rest_opentelemetry.cc"
           "std::string{kv.first}" "kv.first.name()")
patch_file("google/cloud/internal/tracing_rest_client.cc"
           "std::string{kv.first}" "kv.first.name()")

# Patch oauth2_regional_access_boundary_token_manager.h to fix MSVC __func__
# capture issue
set(search_target
    "    auto constexpr kLocation = __func__;
    auto pending_refresh_fn = [p = std::move(pending_refresh),
                               weak = weak_from_this(), request,
                               stub = iam_stub_,
                               retry_policy = retry_policy_->clone(),
                               backoff_policy = backoff_policy_->clone(),
                               options = options_]() mutable {")

set(replace_target
    "    auto pending_refresh_fn = [p = std::move(pending_refresh),
                               weak = weak_from_this(), request,
                               stub = iam_stub_,
                               retry_policy = retry_policy_->clone(),
                               backoff_policy = backoff_policy_->clone(),
                               options = options_]() mutable {
      auto constexpr kLocation = \"RefreshToken\";")

patch_file(
    "google/cloud/internal/oauth2_regional_access_boundary_token_manager.h"
    "${search_target}" "${replace_target}")

# Patch win32/sign_using_sha256.cc to match the 3-argument signature in
# sign_using_sha256.h
set(win32_sign_search "StatusOr<std::vector<std::uint8_t>> SignUsingSha256(
    std::string const& str, std::string const& pem_contents) {")

set(win32_sign_replace
    "StatusOr<std::vector<std::uint8_t>> SignUsingSha256(
    std::string const& str, std::string const& pem_contents,
    SignatureFormat /*format*/) {")

patch_file("google/cloud/internal/win32/sign_using_sha256.cc"
           "${win32_sign_search}" "${win32_sign_replace}")
