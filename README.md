# Simple Garbage Collector

A simple mark-and-sweep garbage collector implemented in C.

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Running Tests
```bash
cd build
ctest
```

or run the test binary directly:

```bash
./test/test_simple_gc
```

### Running the first test

To build and test this initial setup:

```bash
mkdir -p build && cd build
cmake ..
make
ctest
```
