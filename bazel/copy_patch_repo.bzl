def _copy_patch_repo_impl(ctx):
    # ✅ Read patch from google-cloud-cpp
    patch = ctx.read("@com_google_cloud_cpp//bazel:remove_upb_c_rules.patch")

    # ✅ Write it locally in this repo
    ctx.file("remove_upb_c_rules.patch", patch)

    ctx.file("BUILD.bazel", """
exports_files(["remove_upb_c_rules.patch"])
""")

copy_patch_repo = repository_rule(
    implementation = _copy_patch_repo_impl,
)