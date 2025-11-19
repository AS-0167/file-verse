#!/bin/bash
# OFS Server Build and Setup Script
# BSCS24115 - Phase 1

set -e  # Exit on error

echo "=========================================="
echo "  OFS Server Build Script"
echo "  Student: BSCS24115"
echo "=========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if we're in the right directory
if [ ! -f "Makefile" ]; then
    echo -e "${RED}Error: Makefile not found${NC}"
    echo "Please run this script from the project root directory"
    exit 1
fi

# Function to print status
print_status() {
    echo -e "${GREEN}✓${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

print_info() {
    echo -e "${YELLOW}ℹ${NC} $1"
}

# Check for required files
echo "Checking project structure..."

required_files=(
    "source/include/odf_types.hpp"
    "source/include/ofs_core.hpp"
    "source/server/ofs_server.hpp"
    "source/server_main.cpp"
    "compiled/default.uconf"
)

missing_files=0
for file in "${required_files[@]}"; do
    if [ ! -f "$file" ]; then
        print_error "Missing: $file"
        missing_files=$((missing_files + 1))
    fi
done

if [ $missing_files -gt 0 ]; then
    echo ""
    print_error "Missing $missing_files required files"
    echo "Please ensure all files are in place before building"
    exit 1
fi

print_status "All required files found"
echo ""

# Check for core implementation files
echo "Checking core implementation..."
core_files=$(ls source/core/ofs_core*.cpp 2>/dev/null | wc -l)

if [ $core_files -eq 0 ]; then
    print_error "No core implementation files found in source/core/"
    echo "Expected: ofs_core.cpp or ofs_core_part*.cpp"
    exit 1
fi

print_status "Found $core_files core implementation file(s)"
echo ""

# Check compiler
echo "Checking compiler..."
if ! command -v g++ &> /dev/null; then
    print_error "g++ compiler not found"
    echo "Please install g++ (GCC) to compile this project"
    exit 1
fi

GCC_VERSION=$(g++ --version | head -n1)
print_status "Compiler found: $GCC_VERSION"
echo ""

# Build
echo "=========================================="
echo "  Building Project"
echo "=========================================="
echo ""

make clean 2>/dev/null || true
make

if [ $? -eq 0 ]; then
    print_status "Build successful"
    echo ""
else
    print_error "Build failed"
    exit 1
fi

# Check if .omni file exists
echo "=========================================="
echo "  Setup"
echo "=========================================="
echo ""

OMNI_FILE="BSCS24115.omni"

if [ -f "$OMNI_FILE" ]; then
    print_info "Existing file system found: $OMNI_FILE"
    echo ""
    echo "Choose an option:"
    echo "  1) Use existing file system"
    echo "  2) Format new file system (WARNING: Erases all data)"
    echo ""
    read -p "Enter choice (1 or 2): " choice
    echo ""
    
    case $choice in
        2)
            print_info "Formatting new file system..."
            rm -f "$OMNI_FILE"
            ./bin/ofs_server "$OMNI_FILE" compiled/default.uconf --format &
            SERVER_PID=$!
            sleep 2
            kill $SERVER_PID 2>/dev/null || true
            wait $SERVER_PID 2>/dev/null || true
            
            if [ -f "$OMNI_FILE" ]; then
                print_status "File system formatted"
            else
                print_error "Format failed"
                exit 1
            fi
            ;;
        1|*)
            print_status "Using existing file system"
            ;;
    esac
else
    print_info "No existing file system found"
    print_info "Creating new file system..."
    ./bin/ofs_server "$OMNI_FILE" compiled/default.uconf --format &
    SERVER_PID=$!
    sleep 2
    kill $SERVER_PID 2>/dev/null || true
    wait $SERVER_PID 2>/dev/null || true
    
    if [ -f "$OMNI_FILE" ]; then
        print_status "File system created"
    else
        print_error "Failed to create file system"
        exit 1
    fi
fi

echo ""
echo "=========================================="
echo "  Setup Complete!"
echo "=========================================="
echo ""
echo "To start the server:"
echo "  ./bin/ofs_server BSCS24115.omni compiled/default.uconf"
echo ""
echo "Or use the Makefile:"
echo "  make run          # Run with existing file system"
echo "  make run-format   # Run and format new file system"
echo ""
echo "Default credentials:"
echo "  Username: admin"
echo "  Password: admin123"
echo ""
echo "Server will listen on port: 8080"
echo ""
print_status "Ready to go!"