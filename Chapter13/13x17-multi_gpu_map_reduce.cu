// 13x17-multi_gpu_map_reduce.cu
// The same pipeline again, now spread across every GPU in the machine. The only
// change from 13x16 is the context type: nvexec::multi_gpu_stream_context in
// place of nvexec::stream_context. Its scheduler partitions the bulk work over
// all visible devices. The data lives in managed (universal) memory so every
// GPU can reach it.
//
// PREVIEW: C++26 std::execution (P2300) on multiple GPUs via NVIDIA nvexec.
//
// NOT built by this book's CI and NOT machine-verified: no available machine met
// the requirements below, and nvc++ 26.5 additionally cannot compile the stdexec
// commit pinned in the root CMakeLists.txt (see README.md, Known issue).
// Requirements (any one of these missing stops the build, not just the run):
//   - NVIDIA HPC SDK, nvc++ 25.9 or newer.
//   - A CUDA GPU of compute capability 6.0 or newer (Pascal or later).
//     -stdpar refuses cc52 and older outright, so a Maxwell card cannot even
//     compile this file.
//   - A CUDA toolkit in the SDK that the installed driver supports; check with
//     nvidia-smi and `ls $NVHPC_ROOT/cuda`.
//   - Two or more such GPUs, plus managed (universal) memory support.
//
// Build by hand with:
//   nvc++ -std=c++23 -stdpar=gpu -I<path-to-stdexec>/include \
//         13x17-multi_gpu_map_reduce.cu -o 13x17-multi_gpu_map_reduce
// See the root CMakeLists.txt (ENABLE_NVEXEC_GPU) and README.md.

#include <cstddef>
#include <cstdio>
#include <span>

#include <stdexec/execution.hpp>
#include <nvexec/multi_gpu_context.cuh>

#include <thrust/sequence.h>
#include <thrust/universal_vector.h>

namespace ex = stdexec;

int main() {
    constexpr std::size_t N = 1u << 22;                  // ~4M elements

    // Managed memory: reachable from every GPU (and the host).
    thrust::universal_vector<double> data(N);
    thrust::sequence(data.begin(), data.end(), 1.0);
    double* first = thrust::raw_pointer_cast(data.data());

    nvexec::multi_gpu_stream_context stream_ctx{};
    ex::scheduler auto gpus = stream_ctx.get_scheduler();

    ex::sender auto pipeline =
          ex::just(std::span<double>{first, N})
        | ex::continues_on(gpus)                         // spread across all GPUs
        | ex::bulk(ex::par, N, [first](std::size_t i) {  // map: square
              first[i] = first[i] * first[i];
          })
        | nvexec::reduce(0.0);                           // reduce: sum

    auto [sum] = ex::sync_wait(std::move(pipeline)).value();
    std::printf("sum of the first %zu squares (multi-GPU) = %f\n", N, sum);
}
