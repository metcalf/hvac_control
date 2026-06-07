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
