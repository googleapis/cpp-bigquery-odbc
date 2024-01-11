# Google Cloud Build

This directory contains the files needed for our GCB presubmit ("PR builds") and
postsubmit ("CI builds") builds. The `cloudbuild.yaml` file is the main config
file for Google Cloud Build (GCB). Cloud builds may be managed from the command
line with the `gcloud builds` command. To make this process easier, the
`build.sh` script can be used to submit builds to GCB, run builds locally in a
docker container, or even run builds directly in your local environment. See
`build.sh --help` for more details.

A **build** is defined by the linux distribution it will run on, and the steps
performed after the dependencies are installed by the corresponding
_Dockerfile_(`dockerfiles/`). We try to keep all the generic installation steps
in the _Dockerfile_ and the build specific instructions in a shell script. Some
Dockerfiles are used by multiple builds, some only one. Build scripts and
Dockerfiles are associated in a trigger file.

Build scripts live in the `builds/` directory. We want to keep these scripts as
simple as possible to make debugging easier. Ideally a developer should be
easily able to test the build locally.

GCB triggers can be configured in the http://console.cloud.google.com/ UI, but
we prefer to configure them with version controlled YAML files that live in the
`triggers/` directory. Managing triggers can be done with the `gcloud` command
also, but we have the local `trigger.sh` script to make this process a bit
easier. See `trigger.sh --help` for more details.

## Prerequisites for adding a new build

It is more efficient to test a new build locally before adding it. These steps
can be skipped if you are planning to test the build on gcb through pr checks,
but that is not recommended.

1. For testing a build locally, you will need to install docker and run it. See
   the internal-only [doc](http://go/installdocker).
2. Some builds(e.g. `integration-production`) might require gcloud
   authentication within docker. An example of how this can be done can be seen
   from the internal-only
   [doc](https://g3doc.corp.google.com/company/teams/bigquery-developer-tools/odbc/odbc_tests.md#running-odbc-tests-on-docker).
3. For creating triggers you need to
   [install gcloud cli](https://cloud.google.com/sdk/docs/install). Make sure to
   authenticate it with your service/user account and point it to the project
   where the triggers would reside and cloudbuild will run.

## Types of builds

Before adding a new build, it is important to understand whether you can use an
existing `Dockerfile` or an existing `build script`. Broadly speaking, we have 2
types of dockerfiles:

1. [demo\*.Dockerfile](https://github.com/googleapis/cpp-bigquery-odbc/blob/main/ci/cloudbuild/dockerfiles/demo-ubuntu-20.Dockerfile):
   The sole purpose of `demo-*` builds is to help users build our library
   targets, the driver and unit tests for their linux distribution. They don't
   need to install dependencies for running our integration tests. Any change to
   a particular `demo*.Dockerfile` should ideally be duplicated across all of
   them, unless it is a distribution specific change.

2. [\*install.Dockerfile](https://github.com/googleapis/cpp-bigquery-odbc/blob/main/ci/cloudbuild/dockerfiles/ubuntu-22.04-install.Dockerfile):
   These dockerfiles install additional dependencies for running our integration
   tests(ODBC Driver Manager, gcloud sdk etc.). Use this if your build really
   needs these extra dependencies.

If you have additional requirements and it is non-trivial to add those in your
build script, you can always create a new dockerfile.

## Adding a new build

Adding a new build can be done in a single PR. For example, see
https://github.com/googleapis/cpp-bigquery-odbc/pull/45, which adds the build
for running integration tests. The steps to add a new build are:

1. (Optional) If your build needs to run in a unique environment, you should
   start by creating a new Dockerfile in `dockerfiles/`. If you don't need a
   specific environment, you can use the `dockerfiles/demo-ubuntu-20.Dockerfile`
   image, which install all the dependencies for our library targets. Here's an
   [example PR](https://github.com/googleapis/cpp-bigquery-odbc/pull/45) that
   adds a build with a custom Dockerfile.
2. (Optional) If you just need to add an existing build running on a new distro,
   this step is not needed. Create the new build script in `builds/` that runs
   the commands you want.
   - You can test this script right away with `build.sh` by explicitly
     specifying the `--distro` you want your build to run in. For example:
   ```
   $ ./build.sh --distro ubuntu-22.04-install --build <new-build-name> # or ...
   $ ./build.sh --distro ubuntu-22.04-install --build <new-build-name> --docker
   ```
3. Create your trigger file(s) in the `triggers/` directory. If you want both PR
   (presubmit) and CI (postsubmit) builds you can generate the trigger files
   with the command `./trigger.sh --generate <new-build-name>`, which will write
   the new files in the `triggers/` directory. You may need to tweak the files
   at this point, for example to change the distro (ubuntu-20 is the default).
4. At this point, you're pretty much done. You can now test your build using the
   trigger name as shown here:
   ```
   $ build.sh -t <new-build-name>-pr # or ...
   $ build.sh -t <new-build-name>-pr --docker # or ...
   $ build.sh -t <new-build-name>-pr --project <project-name>
   ```
5. FINAL STEP: Now that the code for your new build is checked in, tell GCB
   about the triggers so the can be run automatically for all future PRs and
   merges.
   ```
   $ ./trigger.sh --import triggers/<new-build-name>-pr.yaml
   $ ./trigger.sh --import triggers/<new-build-name>-ci.yaml
   ```

## Testing principles

We want our code to work for our customers. We don't know their exact
environment and configuration, so we need to test our code in a variety of
different environments and configurations. The main dimensions that we need to
test are:

- OS Platform: Linux x `N` different distros (We use Github Actions for Windows
  and macOS)
- Compilers: Clang, GCC
- Build tool: CMake (#TODO: Bazel should also be included for library targets)
- C++ Language: C++17, ..., C++20

In addition to these main dimensions, we also want to use tools and analyzers to
help us catch bugs: clang-tidy, asan, msan, tsan, ubsan, etc. The full matrix of
all combinations is infeasible to test completely, so we follow the following
principles to minimize the test space while achieving high likelihood of the
code working for our customers.

- For simple dimensions (e.g. things that are "on/off") we want at least one
  build for each 'value' of the setting.
- On dimensions with versions, we want to test something _old_ and something
  _new_ (specific versions will change over time)
- Integration tests hit production, but in the future, a subset of these may use
  an emulator
- Sanitizer builds need to run integration tests
- *Demo* builds don't run tests, just build the library targets

## GCB Worker Pool

Our GCB builds are run in a [custom worker pool][custom-worker-pool] to make
scaling up resources easier. You can see our worker pool(s) in the
[web UI](https://console.cloud.google.com/cloud-build/settings/worker-pool?project=bigquery-devtools-drivers).
See `gcloud beta builds worker-pools --help` for more info about worker pools.

We initially created the pool with the following command:

```
$ gcloud beta builds worker-pools create \
  --region=us-east1 \
  --project=bigquery-devtools-drivers \
  --worker-machine-type=e2-standard-16 \
  --worker-disk-size=100 \
  cpp-bigquery-odbc-pool
```

Details of the pool can be changed with the **`update`** (rather than `create`)
command.

[custom-worker-pool]: https://cloud.google.com/build/docs/custom-workers/run-builds-in-custom-worker-pool
