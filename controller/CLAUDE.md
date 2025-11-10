# Project Guide for Claude

## Building

This is an ESP-IDF project. To build:

```bash
export IDF_PATH=$HOME/esp/v5.1.5/esp-idf
export PATH=$HOME/.espressif/python_env/idf5.1_py3.13_env/bin:$HOME/esp/v5.1.5/esp-idf/tools:$PATH
python $HOME/esp/v5.1.5/esp-idf/tools/idf.py build
```

## Testing

Testing is via the GoogleTest framework. For complex, self-contained logic,
create a new test suite for that class/logic. For most other testing, add
to test_controller_app.cpp.

- Run tests: `test/run_tests.sh --gmock_verbose=error`. Arguments are passed through to GoogleTest
- Run a single test by name: `test/run_tests.sh --gmock_verbose=error --gtest_filter=ControllerAppTest.Precooling`

## After Making Changes

Always run both tests and build after making code changes:
1. Run tests: `test/run_tests.sh --gmock_verbose=error`
2. Run build: `python $HOME/esp/v5.1.5/esp-idf/tools/idf.py build` (with ESP-IDF environment set up as shown above)
