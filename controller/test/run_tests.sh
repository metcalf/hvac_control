#!/bin/bash

# Exit on any error
set -e

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color
BOLD='\033[1m'

# Print with color
print_status() {
    echo -e "${GREEN}${BOLD}==>${NC} $1"
}

print_error() {
    echo -e "${RED}${BOLD}Error:${NC} $1"
}

# Create clean build directory if --clean flag is passed
prepare_build() {
    if [[ " $@ " =~ " --clean " ]]; then
        print_status "Cleaning up build directory..."
        rm -rf build
    fi
    mkdir -p build
}

# Run CMake configuration
configure_cmake() {
    print_status "Configuring CMake..."
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Debug
}

# Build the tests
build_tests() {
    print_status "Building tests..."
    make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
}

# Run the tests
run_tests() {
    print_status "Running tests..."
    ./unit_tests $@
}

# Main execution
main() {
    # Store original directory
    ORIGINAL_DIR=$(pwd)

    # Ensure we're in the script's directory
    cd "$(dirname "$0")"

    prepare_build "$@"
    configure_cmake
    build_tests
    run_tests "$@"

    # Return to original directory
    cd "$ORIGINAL_DIR"
}

# Handle errors
handle_error() {
    print_error "An error occurred. Check the output above for details."
    exit 1
}

# Set up error handling
trap handle_error ERR

# Run main function with all script arguments
main "$@"
