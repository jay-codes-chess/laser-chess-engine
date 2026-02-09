#!/bin/bash
# Human Chess Engine - macOS Build Script
# Builds universal binary (ARM64 + x86-64)

set -e

echo "Building Human Chess Engine for macOS..."

cd "$(dirname "$0")"
cd src

# Clean
rm -f *.o syzygy/*.o laser 2>/dev/null || true

# Detect architecture
ARCH=$(uname -m)

if [ "$ARCH" = "arm64" ]; then
    echo "Building for Apple Silicon (ARM64)..."
    CXX="g++"
    CFLAGS="-O3 -std=c++11"
    LDFLAGS="-lpthread"
elif [ "$ARCH" = "x86_64" ]; then
    echo "Building for Intel (x86-64)..."
    CXX="g++"
    CFLAGS="-O3 -march=x86-64 -std=c++11"
    LDFLAGS="-lpthread"
else
    echo "Unknown architecture: $ARCH"
    exit 1
fi

# Compile
echo "Compiling..."
$CXX $CFLAGS -c bbinit.cpp -o bbinit.o
$CXX $CFLAGS -c board.cpp -o board.o
$CXX $CFLAGS -c common.cpp -o common.o
$CXX $CFLAGS -c eval.cpp -o eval.o
$CXX $CFLAGS -c hash.cpp -o hash.o
$CXX $CFLAGS -c search.cpp -o search.o
$CXX $CFLAGS -c moveorder.cpp -o moveorder.o
$CXX $CFLAGS -c uci.cpp -o uci.o
$CXX $CFLAGS -c -x c++ human_eval.cpp -o human_eval.o

# Compile Syzygy stub (no tablebases - just return 0)
$CXX $CFLAGS -x c++ -c syzygy/stub.cpp -o syzygy/tbprobe.o

# Link
$CXX -O3 -o laser bbinit.o board.o common.o eval.o hash.o search.o moveorder.o syzygy/tbprobe.o human_eval.o uci.o $LDFLAGS

if [ -f "laser" ]; then
    echo ""
    echo "✅ Build SUCCESS!"
    echo "Binary: src/laser"
    file src/laser
    echo ""
    echo "To run:"
    echo "  cd src && ./laser"
    echo ""
    echo "Or use with chess GUI (Arena, Cute Chess, etc.)"
else
    echo "❌ Build FAILED!"
    exit 1
fi
