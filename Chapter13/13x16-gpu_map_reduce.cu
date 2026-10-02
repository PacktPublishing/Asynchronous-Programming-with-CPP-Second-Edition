// 13x16-gpu_map_reduce.cu
// The SAME map-reduce as 13x14, retargeted to a GPU. The algorithm is
// unchanged -- bulk maps a function over every element, then the results are
// reduced. Only two things differ: the scheduler is nvexec::stream_scheduler
// (so the work runs on the GPU), and the reduction uses the GPU-accelerated
// nvexec::reduce instead of a host-side std::reduce. This is the headline of
// the execution model: write the pipeline once, choose where it runs.
//
// PREVIEW: C++26 std::execution (P2300) on the GPU via NVIDIA stdexec/nvexec.
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
//
// Build by hand with:
//   nvc++ -std=c++23 -stdpar=gpu -I<path-to-stdexec>/include \
//         13x16-gpu_map_reduce.cu -o 13x16-gpu_map_reduce
// See the root CMakeLists.txt (ENABLE_NVEXEC_GPU) and README.md.

#include <cstddef>
#include <cstdio>
#include <span>

#include <stdexec/execution.hpp>
#include <nvexec/stream_context.cuh>

#include <thrust/device_vector.h>
#include <thrust/sequence.h>

namespace ex = stdexec;

int main() {
    constexpr std::size_t N = 1u << 20;                  // ~1M elements

    // Device memory: 1, 2, 3, ... on the GPU.
    thrust::device_vector<double> data(N);
    thrust::sequence(data.begin(), data.end(), 1.0);
    double* first = thrust::raw_pointer_cast(data.data());

    nvexec::stream_context stream_ctx{};
    ex::scheduler auto gpu = stream_ctx.get_scheduler();

    ex::sender auto pipeline =
          ex::just(std::span<double>{first, N})
        | ex::continues_on(gpu)                          // move onto the GPU
        | ex::bulk(ex::par, N, [first](std::size_t i) {  // map: square, on the GPU
              first[i] = first[i] * first[i];
          })
        | nvexec::reduce(0.0);                           // reduce: sum, on the GPU

    auto [sum] = ex::sync_wait(std::move(pipeline)).value();
    std::printf("sum of the first %zu squares (GPU) = %f\n", N, sum);
}
