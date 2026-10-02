# Asynchronous Programming with C++ — 2nd Edition

Source code examples for the book published by Packt Publishing.

## Requirements

- **Compiler**: GCC 13+ or Clang 16+ with C++23 support for most chapters.
  **Chapters 12 and 13 need GCC 14+ or Clang 18+**, because every example in
  them uses `<print>`, which reached libstdc++ in GCC 14.
- **Build system**: CMake 3.25+, Ninja (recommended)
- **OS**: Linux (tested on Ubuntu 24.04)
- **GPU (optional)**: the two Chapter 13 `.cu` examples need an NVIDIA GPU of
  **compute capability 6.0 or newer** (Pascal, 2016, or later). See
  *GPU examples (Chapter 13, nvexec)* below.

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

## The std::execution examples

`3x19-cpp26_std_execution.cpp` and **every example in `Chapter12/` and
`Chapter13/`** require the
[NVIDIA stdexec](https://github.com/NVIDIA/stdexec) reference implementation of
P2300. They are built by default, no flags needed:

```bash
cmake --preset release-clang
cmake --build --preset release-clang
```

The first configure downloads stdexec from GitHub via `FetchContent`, pinned to
the commit in `STDEXEC_GIT_TAG` (the single source of truth for the version
these examples were validated against, CI uses the same value). An internet
connection is therefore required the first time.

### Using a local stdexec checkout

To build against an already existing checkout, point `STDEXEC_INCLUDE_DIR` at its
`include/` directory. That takes priority and no download occurs:

```bash
git clone https://github.com/NVIDIA/stdexec.git
cmake --preset release-clang \
    -DSTDEXEC_INCLUDE_DIR=$PWD/stdexec/include \
    -DSTDEXEC_SRC_DIR=$PWD/stdexec/src
```

`STDEXEC_SRC_DIR` (stdexec's `src/` directory) is needed only for
`12x04-parallel_scheduler`. Without it, that one example is skipped.

### Building offline

Configure with `-DFETCH_STDEXEC=OFF`. The rest of the project builds normally
and every `std::execution` example is skipped, with a `STATUS` line saying so.

> **Note on `12x04-parallel_scheduler`.** `get_parallel_scheduler()` needs
> stdexec's default backend, which lives in
> `src/parallel_scheduler/parallel_scheduler.cpp`. That translation unit is
> compiled directly into the executable rather than linked as a library: the
> backend registers itself through a static initializer, so a static-library
> link lets the linker discard the object file and the program segfaults at
> run time.

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

The Chapter 12 and 13 examples compile the same way at `-std=c++23`, since
stdexec is a library that provides the C++26 feature:

```bash
g++ -std=c++23 -I<path-to-stdexec>/include \
    Chapter13/13x01-motivating_pipeline.cpp -o bin/Chapter13/13x01-motivating_pipeline
```

### GPU examples (Chapter 13, nvexec)

`Chapter13/13x16-gpu_map_reduce.cu` and `Chapter13/13x17-multi_gpu_map_reduce.cu`
run sender pipelines on the GPU through stdexec's `nvexec` schedulers. They are
**off by default and never built by CI**, since no hosted runner has an NVIDIA
GPU. They require all of the following:

- **NVIDIA HPC SDK (`nvc++` 25.9+)**: not on apt. Download it from
  <https://developer.nvidia.com/hpc-sdk> and add its `compilers/bin` to `PATH`.
- **An NVIDIA GPU of compute capability 6.0 or newer** (Pascal, 2016, or later).
  This is a hard floor. `-stdpar` refuses anything older, with
  `nvc++-Fatal-The -stdpar option is available only on systems with NVIDIA GPUs
  with compute capability '>= cc60`. A Maxwell card such as a GTX 900 series
  (cc5.2) cannot build these examples at all, even to check that they compile.
- **A CUDA toolkit matching the installed driver**: `nvc++` rejects a bundled
  toolkit newer than the driver supports. Check the driver's CUDA version with
  `nvidia-smi` and the bundled toolkits with `ls $NVHPC_ROOT/cuda`.
- **Two or more GPUs** for `13x17` only, plus managed-memory support.

Enable them with `ENABLE_NVEXEC_GPU` and a GPU-capable compiler:

```bash
cmake --preset debug \
    -DCMAKE_CXX_COMPILER=nvc++ \
    -DENABLE_NVEXEC_GPU=ON \
    -DSTDEXEC_INCLUDE_DIR=/path/to/stdexec/include
cmake --build --preset debug
```

The CMake build passes `-stdpar=gpu` when it detects `nvc++`. To build one by
hand:

```bash
nvc++ -std=c++23 -stdpar=gpu -I<path-to-stdexec>/include \
    Chapter13/13x16-gpu_map_reduce.cu -o bin/Chapter13/13x16-gpu_map_reduce
```

> **Known issue.** `nvc++` 26.5 cannot compile the stdexec commit pinned in the
> root `CMakeLists.txt`. Even a minimal translation unit containing only
> `#include <stdexec/execution.hpp>` and `namespace ex = stdexec;` fails with
> `error: name must be a namespace name`. The problem is an incompatibility
> between `nvc++` and stdexec, not the examples, which compile cleanly under
> GCC 14/15 and Clang 18/19. If you hit it, try an older `nvc++` or an older
> stdexec commit. **These two examples are therefore the only ones in this
> repository that have not been machine-verified.**
