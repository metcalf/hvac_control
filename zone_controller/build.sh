#!/usr/bin/env bash
set -e
export IDF_PATH=$HOME/.espressif/v5.5.4/esp-idf
export IDF_PYTHON_ENV_PATH=$HOME/.espressif/tools/python/v5.5.4/venv
export PATH=$HOME/.espressif/tools/xtensa-esp-elf/esp-14.2.0_20260121/xtensa-esp-elf/bin:$IDF_PYTHON_ENV_PATH/bin:$HOME/.espressif/v5.5.4/esp-idf/tools:$PATH
exec python $HOME/.espressif/v5.5.4/esp-idf/tools/idf.py "$@"
