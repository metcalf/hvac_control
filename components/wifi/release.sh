#!/bin/bash -euo pipefail

if [[ ! -f "sdkconfig" ]]; then
    echo "Please run this script from the root of the ESP IDF project"
    exit 1
fi

if ! command -v idf.py &> /dev/null; then
    echo "Please run this script within an ESP IDF shell"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# NB: This relies on the project name being the same as the directory name
PROJECT_NAME="$(basename "$PWD")"
FIRMWARE_PATH="build/${PROJECT_NAME}.bin"
REMOTE_ROOT="/var/www/local/esp-ota"
REMOTE_PATH="${REMOTE_ROOT}/hvac_control_${PROJECT_NAME}"
# This should contain a single string with an SCP target of the form user@host
# Newlines stripped for convenience.
REMOTE_SPEC=$(cat ${SCRIPT_DIR}/remote_spec.cfg | tr -d '\n')

get_version() {
    git describe --always --tags --dirty
}

version=$(get_version)
if [[ $version == *"-dirty" ]]; then
  echo "git repo is dirty, please commit changes"
  exit 1
fi

git add --all
version=$(get_version)
if [[ $version == *"-dirty" ]]; then
  echo "git repo had untracked files, added those files. Please commit changes"
  exit 1
fi

echo "removing old firmware and running build"
# Remove old file so we ensure we're building to the right place
rm -f "${FIRMWARE_PATH}"
idf.py build

if ! strings "${FIRMWARE_PATH}" | grep "${version}"; then
    echo "version ${version} not found in ${FIRMWARE_PATH}"
    exit 1
fi

# Note this doesn't clear out old files but they're only 1.5M
ssh "${REMOTE_SPEC}" "mkdir -p ${REMOTE_PATH} && chown -R www-data:adm ${REMOTE_ROOT}"
scp "${FIRMWARE_PATH}" "$REMOTE_SPEC:${REMOTE_PATH}/${version}.bin"
ssh "${REMOTE_SPEC}" "echo -n ${version} > ${REMOTE_PATH}/latest_version"
