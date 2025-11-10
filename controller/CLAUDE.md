# Project Guide for Claude

## Testing

Testing is via the GoogleTest framework. For complex, self-contained logic,
create a new test suite for that class/logic. For most other testing, add
to test_controller_app.cpp.

- Run tests: `test/run_tests.sh --gmock_verbose=error`. Arguments are passed through to GoogleTest
- Run a single test by name: `test/run_tests.sh --gmock_verbose=error --gtest_filter=ControllerAppTest.Precooling`
