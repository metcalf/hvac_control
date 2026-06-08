#!/usr/bin/env bash
set -e
export IDF_PATH=$HOME/.espressif/v5.5.4/esp-idf
export PATH=$HOME/.espressif/tools/xtensa-esp-elf/esp-14.2.0_20260121/xtensa-esp-elf/bin:$HOME/.espressif/python_env/idf5.5_py3.14_env/bin:$HOME/.espressif/v5.5.4/esp-idf/tools:$PATH
exec python $HOME/.espressif/v5.5.4/esp-idf/tools/idf.py "$@"
