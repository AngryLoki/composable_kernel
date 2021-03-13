#ifndef CK_BLOCKWISE_GEMM_V3_HPP
#define CK_BLOCKWISE_GEMM_V3_HPP

#include "common_header.hpp"
#include "threadwise_gemm_v3.hpp"

namespace ck {

// blockwise GEMM: C[M, N] += transpose(A[K, M]) * B[K, N]
// A and B are visable to the whole block, C is distributed among each thread
// If following number are power of 2, index calculation shall be greatly reduced:
//    KPerThread, HPerThread, MLevel0ThreadCluster, NLevel0ThreadCluster,
//    MLevel1ThreadCluster, NLevel1ThreadCluster
template <index_t BlockSize,
          typename BlockMatrixA,
          typename BlockMatrixB,
          typename ThreadMatrixC,
          index_t KPerThread,
          index_t HPerThread,
          index_t WPerThread,
          index_t CYXPerThreadLoop,
          index_t HThreadCluster,
          index_t WThreadCluster,
          index_t ThreadGemmADataPerRead_K,
          index_t ThreadGemmBDataPerRead_W>
struct BlockwiseGemm_km_kn_m0m1n0n1_v3
{
    struct MatrixIndex
    {
        index_t k;
        index_t h;
        index_t w;
    };

    index_t mMyThreadOffsetA;

    __device__ BlockwiseGemm_km_kn_m0m1n0n1_v3()
    {
        static_assert(BlockMatrixA::IsKnownAtCompileTime() &&
                          BlockMatrixB::IsKnownAtCompileTime() &&
                          ThreadMatrixC::IsKnownAtCompileTime(),
                      "wrong! Desc should be known at compile-time");

        constexpr auto I0 = Number<0>{};
        constexpr auto I1 = Number<1>{};
        constexpr auto I2 = Number<2>{};
        constexpr auto I3 = Number<3>{};

        // constexpr index_t ThreadPerLevel1Cluster = MLevel0ThreadCluster * NLevel0ThreadCluster *
        // MLevel1ThreadCluster * NLevel1ThreadCluster;

        static_assert(BlockSize == HThreadCluster * WThreadCluster, "wrong! wrong blocksize\n");

        static_assert(BlockMatrixA{}.GetLength(I0) == BlockMatrixB{}.GetLength(I0),
                      "wrong! K dimension not consistent\n");

        constexpr index_t K = BlockMatrixA{}.GetLength(I1); // A is transposed
        constexpr index_t N = BlockMatrixB{}.GetLength(I1);
        constexpr index_t H = BlockMatrixB{}.GetLength(I2);
        constexpr index_t W = BlockMatrixB{}.GetLength(I3);

        static_assert(
            K % (KPerThread) == 0 &&
                (N * H * W) % (HPerThread * WPerThread * HThreadCluster * WThreadCluster) == 0,
            "wrong! Cannot evenly divide work among\n");

        auto c_thread_mtx_index = GetBeginOfThreadMatrixC(get_thread_local_1d_id());

        mMyThreadOffsetA = BlockMatrixA{}.CalculateOffset(make_tuple(0, c_thread_mtx_index.k));
    }

    __device__ static constexpr auto GetThreadMatrixCLengths()
    {
        return Sequence<KPerThread, 1, HPerThread, WPerThread>{};
    }

    __device__ static MatrixIndex GetBeginOfThreadMatrixC(index_t thread_id)
    {
        return MatrixIndex{1, 8, 8};
    }

    template <typename SrcDesc,
              typename DstDesc,
              index_t NSliceRow,
              index_t NSliceCol,
              index_t DataPerAccess>
    struct ThreadwiseSliceCopy_a
    {
        template <typename Data>
        __device__ static void Run(const Data* p_src, Data* p_dst)
        {
            static_assert(SrcDesc::IsKnownAtCompileTime() && DstDesc::IsKnownAtCompileTime(),
                          "wrong! Desc should be known at compile-time");

            using vector_t = typename vector_type<Data, DataPerAccess>::type;

            static_for<0, NSliceRow, 1>{}([&](auto i) {
                static_for<0, NSliceCol, DataPerAccess>{}([&](auto j) {
                    constexpr auto src_offset = SrcDesc{}.CalculateOffset(make_tuple(i, j));
                    constexpr auto dst_offset = DstDesc{}.CalculateOffset(make_tuple(i, j));

                    *reinterpret_cast<vector_t*>(&p_dst[dst_offset]) =
                        *reinterpret_cast<const vector_t*>(&p_src[src_offset]);
                });
            });
        }
    };

    template <typename FloatA, typename FloatB, typename FloatC>
    __device__ void
    Run_naive(const FloatA* p_a_block, const FloatB* p_b_thread, FloatC* p_c_thread) const
    {
        constexpr auto I0 = Number<0>{};
        constexpr auto I1 = Number<1>{};
        constexpr auto I2 = Number<2>{};
        constexpr auto I3 = Number<3>{};

        constexpr auto a_block_mtx = BlockMatrixA{};
        constexpr auto b_block_mtx = BlockMatrixB{};

        constexpr auto CYXPerBlock = a_block_mtx.GetLength(I0);

        // thread A, B for GEMM
        constexpr auto a_thread_mtx = make_dynamic_naive_tensor_descriptor_packed_v2(
            make_tuple(Number<CYXPerThreadLoop>{}, Number<KPerThread>{}));

        constexpr auto b_thread_mtx = make_dynamic_naive_tensor_descriptor_packed_v2(
            // make_tuple(Number<CYXPerThreadLoop>{}, Number<1>{}, Number<1>{}, Number<1>{}));
            make_tuple(Number<CYXPerThreadLoop>{}, Number<1>{}));

        constexpr auto c_thread_mtx = make_dynamic_naive_tensor_descriptor_packed_v2(
            make_tuple(Number<KPerThread>{}, Number<1>{}));

        FloatA p_a_thread[a_thread_mtx.GetElementSpaceSize()];

        constexpr auto a_thread_copy = ThreadwiseSliceCopy_a<BlockMatrixA,
                                                             decltype(a_thread_mtx),
                                                             CYXPerThreadLoop,
                                                             KPerThread,
                                                             ThreadGemmADataPerRead_K>{};

        constexpr auto threadwise_gemm = ThreadwiseGemm_km_kn_mn_v3<decltype(a_thread_mtx),
                                                                    decltype(b_thread_mtx),
                                                                    decltype(c_thread_mtx)>{};
        // loop over k
        for(index_t cyx_begin = 0; cyx_begin < CYXPerBlock; cyx_begin += CYXPerThreadLoop)
        {
            a_thread_copy.Run(p_a_block + a_block_mtx.CalculateOffset(make_tuple(cyx_begin, 0)) +
                                  mMyThreadOffsetA,
                              p_a_thread + a_thread_mtx.CalculateOffset(make_tuple(0, 0)));

            threadwise_gemm.Run(p_a_thread, p_b_thread + CYXPerThreadLoop * cyx_begin, p_c_thread);
        }
    }

    template <typename FloatA, typename FloatB, typename FloatC>
    __device__ void Run(const FloatA* p_a_block, const FloatB* p_b_block, FloatC* p_c_thread) const
    {
        Run_naive(p_a_block, p_b_block, p_c_thread);
    }
};

} // namespace ck
#endif
