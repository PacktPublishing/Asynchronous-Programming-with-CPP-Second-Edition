# Asynchronous Programming with C++ — 2nd Edition

Source code examples for the book published by Packt Publishing.

## Requirements

- **Compiler**: GCC 13+ or Clang 16+ with C++23 support
- **Build system**: CMake 3.25+, Ninja (recommended)
- **OS**: Linux (tested on Ubuntu 24.04)

## Project structure

```
.
├── CMakeLists.txt          Root build configuration
├── CMakePresets.json       Build presets (gcc/clang, debug/release, sanitizers)
├── Chapter03/              Chapter 3 examples
│   ├── CMakeLists.txt
│   └── 3x01-*.cpp …        One source file per example
├── Chapter04/              (added as chapters are written)
│   └── …
└── bin/                    Compiled binaries (created by build)
    └── Chapter03/
        ├── 3x01-concurrent_file_processor
        └── …
```

New chapters are added by creating a `ChapterNN/` directory with a `CMakeLists.txt`
and registering it with `add_subdirectory(ChapterNN)` in the root `CMakeLists.txt`.

## Building locally

### Quick start (default preset = debug with GCC)

```bash
cmake --preset debug
cmake --build --preset debug
```

Binaries are written to `bin/ChapterNN/`.

### Available presets

| Preset           | Compiler | Build type | Notes               |
|------------------|----------|------------|---------------------|
| `debug-gcc`      | GCC      | Debug      |                     |
| `release-gcc`    | GCC      | Release    |                     |
| `debug-clang`    | Clang    | Debug      |                     |
| `release-clang`  | Clang    | Release    |                     |
| `asan-gcc`       | GCC      | Debug      | AddressSanitizer    |
| `tsan-gcc`       | GCC      | Debug      | ThreadSanitizer     |
| `debug`          | GCC      | Debug      | Alias for debug-gcc |
| `release`        | GCC      | Release    | Alias for release-gcc |

Examples:

```bash
# Release build with Clang
cmake --preset release-clang
cmake --build --preset release-clang

# Debug build with AddressSanitizer
cmake --preset asan-gcc
cmake --build --preset asan-gcc

# Debug build with ThreadSanitizer (useful for concurrency examples)
cmake --preset tsan-gcc
cmake --build --preset tsan-gcc
```

### Without presets (plain CMake)

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build .
```

## GitHub Actions CI

The CI pipeline lives at `.github/workflows/ci.yml` and runs on every push and
pull request to `main`. It can also be triggered manually via
`workflow_dispatch`. It builds the project across a matrix of compilers:

| Compiler   | Presets                          |
|------------|----------------------------------|
| GCC 14     | `debug-gcc`, `release-gcc`       |
| Clang 18   | `debug-clang`, `release-clang`   |

The workflow installs Ninja and the requested compiler on Ubuntu 24.04, then
runs `cmake --preset` and `cmake --build --preset` for each matrix entry.

To trigger manually from the CLI:

```bash
gh workflow run CI
```

Or use the **Actions** tab > **CI** > **Run workflow** button on GitHub.

## Optional examples

`3x19-cpp26_std_execution.cpp` requires the [NVIDIA stdexec](https://github.com/NVIDIA/stdexec)
reference implementation of P2300 and a C++26-capable toolchain. It is excluded
from the default build.

### Build via CMake (recommended)

Enable the `BUILD_STDEXEC_EXAMPLES` option — CMake will fetch stdexec automatically:

```bash
cmake --preset release-clang -DBUILD_STDEXEC_EXAMPLES=ON
cmake --build --preset release-clang
```

The first configure pulls stdexec from GitHub via `FetchContent`, so an internet
connection is required.

> **Note on the C++ standard flag.** The rest of the project builds at C++23,
> set globally in the root `CMakeLists.txt`. The 3x19 target overrides this by
> appending `-std=c++26` directly via `target_compile_options`, rather than
> using `target_compile_features(... cxx_std_26)`. CMake 3.25 does not yet tag
> Clang 18 / GCC 14 as `cxx_std_26`-capable, so the feature-based form fails
> at configure time. The raw flag bypasses that check and works on any
> compiler that accepts `-std=c++26`.

### Build manually (no CMake)

```bash
clang++ -std=c++26 -I<path-to-stdexec>/include \
    Chapter03/3x19-cpp26_std_execution.cpp -o bin/Chapter03/3x19-cpp26_std_execution
```
