# Project Guide for Claude

## Building

This is an ESP-IDF project (ESP-IDF v5.5.1). To build:

```bash
bash build.sh build
```

## Testing

Testing is via the GoogleTest framework. For complex, self-contained logic,
create a new test suite for that class/logic. For most other testing, add
to test_controller_app.cpp.

- Run tests: `test/run_tests.sh --gmock_verbose=error`. Arguments are passed through to GoogleTest
- Run a single test by name: `test/run_tests.sh --gmock_verbose=error --gtest_filter=ZCAppTest.TurnsOnZoneValveAndPump`

## After Making Changes

Always run both tests and build after making code changes:

1. Run tests: `test/run_tests.sh --gmock_verbose=error`
2. Run build: `bash build.sh build`

## Releasing (OTA)

Devices update themselves over the air. To release, commit your changes and
run from this directory:

```bash
../components/esp-iot-core/release.sh
```

The script refuses to run with uncommitted changes (the version is
`git describe`), runs the tests, builds, and uploads the firmware to the home
server over SSH. It works from a plain shell, using `build.sh` to set up the
IDF environment. The device checks for a new version every minute, then
downloads it and reboots.
