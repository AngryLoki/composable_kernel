// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2022, Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "ck/utility/common_header.hpp"
#include "ck/tensor_description/multi_index_transform_helper.hpp"
#include "ck/tensor_description/tensor_descriptor.hpp"
#include "ck/tensor_description/tensor_descriptor_helper.hpp"
#include "ck/tensor_operation/gpu/grid/block_to_ctile_map.hpp"
#include "ck/tensor_operation/gpu/grid/gridwise_gemm_pipeline_selector.hpp"
#include "ck/tensor_operation/gpu/grid/gridwise_gemm_pipeline_v1.hpp"
#include "ck/tensor_operation/gpu/block/blockwise_gemm_xdlops.hpp"
#include "ck/tensor_operation/gpu/block/thread_group_tensor_slice_transfer_v4r1.hpp"
#include "ck/tensor_operation/gpu/block/thread_group_tensor_slice_transfer_v6r1.hpp"
#include "ck/tensor_operation/gpu/thread/threadwise_tensor_slice_transfer.hpp"
#include "ck/tensor_operation/gpu/element/element_wise_operation.hpp"
#include "ck/tensor_operation/gpu/device/matrix_padder.hpp"

namespace ck {

template <typename GridwiseGemm,
          bool HasMainKBlockLoop,
          InMemoryDataOperationEnum CGlobalMemoryDataOperation,
          typename Block2CTileMap>
__global__ void
#if CK_USE_LAUNCH_BOUNDS
    __launch_bounds__(CK_MAX_THREAD_PER_BLOCK, CK_MIN_BLOCK_PER_CU)
#endif
        kernel_gemm_xdlops_splitk_simplified(typename GridwiseGemm::Argument karg,
                                             const Block2CTileMap& b2c_map)
{
#if(!defined(__HIP_DEVICE_COMPILE__) || defined(__gfx908__) || defined(__gfx90a__) || \
    defined(__gfx940__))
    constexpr index_t shared_size = GridwiseGemm::GetSharedMemoryNumberOfByte();

    __shared__ uint8_t p_shared[shared_size];

    GridwiseGemm::template Run<HasMainKBlockLoop, CGlobalMemoryDataOperation>(
        karg, static_cast<void*>(p_shared), b2c_map);
#else
    ignore = karg;
    ignore = b2c_map;
#endif // end of if (defined(__gfx908__) || defined(__gfx90a__) || defined(__gfx940__))
}

template <index_t BlockSize,
          typename FloatAB,
          typename FloatAcc,
          typename FloatC,
          typename ALayout,
          typename BLayout,
          typename CLayout,
          typename AElementwiseOperation,
          typename BElementwiseOperation,
          typename CElementwiseOperation,
          tensor_operation::device::GemmSpecialization GemmSpec,
          index_t NumGemmKPrefetchStage,
          index_t MPerBlock,
          index_t NPerBlock,
          index_t K0PerBlock,
          index_t MPerXDL,
          index_t NPerXDL,
          index_t K1Value,
          index_t MXdlPerWave,
          index_t NXdlPerWave,
          typename ABlockTransferThreadClusterLengths_K0_M_K1,
          typename ABlockTransferThreadClusterArrangeOrder,
          typename ABlockTransferSrcAccessOrder,
          index_t ABlockTransferSrcVectorDim,
          index_t ABlockTransferSrcScalarPerVector,
          index_t ABlockTransferDstScalarPerVector_K1,
          bool AThreadTransferSrcResetCoordinateAfterRun,
          bool ABlockLdsExtraM,
          typename BBlockTransferThreadClusterLengths_K0_N_K1,
          typename BBlockTransferThreadClusterArrangeOrder,
          typename BBlockTransferSrcAccessOrder,
          index_t BBlockTransferSrcVectorDim,
          index_t BBlockTransferSrcScalarPerVector,
          index_t BBlockTransferDstScalarPerVector_K1,
          bool BThreadTransferSrcResetCoordinateAfterRun,
          bool BBlockLdsExtraN,
          LoopScheduler LoopSched     = make_default_loop_scheduler(),
          PipelineVersion PipelineVer = PipelineVersion::v1>
struct GridwiseGemm_bk0mk1_bk0nk1_mn_xdlops_splitk_direct_c_write_out
{
    static constexpr auto I0 = Number<0>{};
    static constexpr auto I1 = Number<1>{};
    static constexpr auto I2 = Number<2>{};
    static constexpr auto I3 = Number<3>{};
    static constexpr auto I4 = Number<4>{};
    static constexpr auto I5 = Number<5>{};
    static constexpr auto I6 = Number<6>{};
    static constexpr auto I7 = Number<7>{};

    // K1 should be Number<...>
    static constexpr auto K1        = Number<K1Value>{};
    static constexpr auto KPerBlock = K1Value * K0PerBlock;

    using ThisThreadBlock  = ThisThreadBlock<BlockSize>;
    using GridwiseGemmPipe = remove_cvref_t<decltype(
        GridwiseGemmPipeline_Selector<PipelineVer, NumGemmKPrefetchStage, LoopSched>())>;

    struct Argument : public ck::tensor_operation::device::BaseArgument
    {
        const FloatAB* p_a_grid;
        const FloatAB* p_b_grid;
        FloatC* p_c_grid;
        index_t M;
        index_t N;
        index_t K;
        index_t StrideA;
        index_t StrideB;
        index_t StrideC;
        index_t K0;
        index_t k_batch;

        Argument(const FloatAB* p_a_grid_,
                 const FloatAB* p_b_grid_,
                 FloatC* p_c_grid_,
                 index_t M_,
                 index_t N_,
                 index_t K_,
                 index_t StrideA_,
                 index_t StrideB_,
                 index_t StrideC_,
                 index_t K0_,
                 index_t k_batch_)
            : p_a_grid(p_a_grid_),
              p_b_grid(p_b_grid_),
              p_c_grid(p_c_grid_),
              M(M_),
              N(N_),
              K(K_),
              StrideA(StrideA_),
              StrideB(StrideB_),
              StrideC(StrideC_),
              K0(K0_),
              k_batch(k_batch_)
        {
        }

        void Print() const
        {
            std::cout << "arg {"
                      << "M:" << M << ", "
                      << "N:" << N << ", "
                      << "K:" << K << ", "
                      << "SA:" << StrideA << ", "
                      << "SB:" << StrideB << ", "
                      << "SC:" << StrideC << ", "
                      << "K0:" << K0 << ", "
                      << "KB:" << k_batch << "}" << std::endl;
        }
    };

    __host__ __device__ static auto CalculateGridSize(const Argument& karg)
    {
        return std::make_tuple(math::integer_divide_ceil(karg.N, NPerBlock),
                               math::integer_divide_ceil(karg.M, MPerBlock),
                               karg.k_batch);
    }

    // prefer this to be called on host
    __host__ __device__ static auto CalculateMPadded(index_t M)
    {
        return math::integer_least_multiple(M, MPerBlock);
    }

    __host__ __device__ static auto CalculateNPadded(index_t N)
    {
        return math::integer_least_multiple(N, NPerBlock);
    }

    __host__ __device__ static auto CalculateK0(index_t K, index_t K_Batch = 1)
    {
        // k_batch * k0 * k0_per_block * k1
        auto K_t = K_Batch * K0PerBlock * K1;
        return (K + K_t - 1) / K_t * K0PerBlock;
    }

    __host__ __device__ static auto CalculateKPadded(index_t K, index_t K_Batch = 1)
    {
        auto K0 = CalculateK0(K, K_Batch);
        return K_Batch * K0 * K1;
    }

    template <typename ABlockDesc_AK0_M_AK1>
    __device__ static constexpr auto
    MakeGemmAMmaTileDescriptor_M0_M1_M2_K(const ABlockDesc_AK0_M_AK1&)
    {
        constexpr index_t MWaves = MPerBlock / (MXdlPerWave * MPerXDL);

        return MakeGemmMmaTileDescriptor_MN0_MN1_MN2_K<MXdlPerWave, MWaves, MPerXDL>(
            ABlockDesc_AK0_M_AK1{});
    }

    template <typename BBlockDesc_BK0_N_BK1>
    __device__ static constexpr auto
    MakeGemmBMmaTileDescriptor_N0_N1_N2_K(const BBlockDesc_BK0_N_BK1&)
    {
        constexpr index_t NWaves = NPerBlock / (NXdlPerWave * NPerXDL);

        return MakeGemmMmaTileDescriptor_MN0_MN1_MN2_K<NXdlPerWave, NWaves, NPerXDL>(
            BBlockDesc_BK0_N_BK1{});
    }

    __device__ static constexpr auto GetABlockDescriptor_AK0PerBlock_MPerBlock_AK1()
    {
        constexpr auto max_lds_align = K1;

        // A matrix in LDS memory, dst of blockwise copy
        if constexpr(ABlockLdsExtraM)
        {
            return make_naive_tensor_descriptor(
                make_tuple(Number<K0PerBlock>{}, Number<MPerBlock>{}, K1),
                make_tuple(Number<MPerBlock + 1>{} * K1, K1, I1));
        }
        else
        {
            return make_naive_tensor_descriptor_aligned(
                make_tuple(Number<K0PerBlock>{}, Number<MPerBlock>{}, K1), max_lds_align);
        }
    }

    __device__ static constexpr auto GetABlockDescriptor_KBatch_AK0PerBlock_MPerBlock_AK1()
    {
        // lds max alignment
        constexpr auto max_lds_align = K1;

        if constexpr(ABlockLdsExtraM)
        {
            return make_naive_tensor_descriptor(
                make_tuple(Number<1>{}, Number<K0PerBlock>{}, Number<MPerBlock>{}, K1),
                make_tuple(Number<K0PerBlock>{} * Number<MPerBlock + 1>{} * K1,
                           Number<MPerBlock + 1>{} * K1,
                           K1,
                           I1));
        }
        else
        {
            return make_naive_tensor_descriptor_aligned(
                make_tuple(Number<1>{}, Number<K0PerBlock>{}, Number<MPerBlock>{}, K1),
                max_lds_align);
        }
    }

    __device__ static constexpr auto GetBBlockDescriptor_BK0PerBlock_NPerBlock_BK1()
    {
        constexpr auto max_lds_align = K1;

        // B matrix in LDS memory, dst of blockwise copy
        if constexpr(BBlockLdsExtraN)
        {
            return make_naive_tensor_descriptor(
                make_tuple(Number<K0PerBlock>{}, Number<NPerBlock>{}, K1),
                make_tuple(Number<NPerBlock + 1>{} * K1, K1, I1));
        }
        else
        {
            return make_naive_tensor_descriptor_aligned(
                make_tuple(Number<K0PerBlock>{}, Number<NPerBlock>{}, K1), max_lds_align);
        }
    }

    __device__ static constexpr auto GetBBlockDescriptor_KBatch_BK0PerBlock_NPerBlock_BK1()
    {
        constexpr auto max_lds_align = K1;

        if constexpr(BBlockLdsExtraN)
        {
            return make_naive_tensor_descriptor(
                make_tuple(Number<1>{}, Number<K0PerBlock>{}, Number<NPerBlock>{}, K1),
                make_tuple(Number<K0PerBlock>{} * Number<NPerBlock + 1>{} * K1,
                           Number<NPerBlock + 1>{} * K1,
                           K1,
                           I1));
        }
        else
        {
            return make_naive_tensor_descriptor_aligned(
                make_tuple(Number<1>{}, Number<K0PerBlock>{}, Number<NPerBlock>{}, K1),
                max_lds_align);
        }
    }

    __host__ __device__ static auto MakeAGridDescriptor_KBatch_K0_M_K1(
        index_t M, index_t K, index_t StrideA, index_t KBatch, index_t K0)
    {
        const auto a_grid_desc_m_k = [&]() {
            if constexpr(is_same<tensor_layout::gemm::RowMajor, ALayout>::value)
            {
                return make_naive_tensor_descriptor(make_tuple(M, K), make_tuple(StrideA, I1));
            }
            else if constexpr(is_same<tensor_layout::gemm::ColumnMajor, ALayout>::value)
            {
                return make_naive_tensor_descriptor(make_tuple(M, K), make_tuple(I1, StrideA));
            }
        }();

        using DoPads = Sequence<tensor_operation::device::GemmPadM<GemmSpec>::PadM, true>;
        const auto a_grid_desc_mpad_kpad = tensor_operation::device::PadTensorDescriptor(
            a_grid_desc_m_k, make_tuple(MPerBlock, K0 * K1), DoPads{});

        return transform_tensor_descriptor(
            a_grid_desc_mpad_kpad,
            make_tuple(make_unmerge_transform(make_tuple(KBatch, K0, K1)),
                       make_pass_through_transform(a_grid_desc_mpad_kpad.GetLength(I0))),
            make_tuple(Sequence<1>{}, Sequence<0>{}),
            make_tuple(Sequence<0, 1, 3>{}, Sequence<2>{}));
    }

    __host__ __device__ static auto MakeBGridDescriptor_KBatch_K0_N_K1(
        index_t K, index_t N, index_t StrideB, index_t KBatch, index_t K0)
    {
        const auto b_grid_desc_k_n = [&]() {
            if constexpr(is_same<tensor_layout::gemm::RowMajor, BLayout>::value)
            {
                return make_naive_tensor_descriptor(make_tuple(K, N), make_tuple(StrideB, I1));
            }
            else if constexpr(is_same<tensor_layout::gemm::ColumnMajor, BLayout>::value)
            {
                return make_naive_tensor_descriptor(make_tuple(K, N), make_tuple(I1, StrideB));
            }
        }();

        using DoPads = Sequence<true, tensor_operation::device::GemmPadN<GemmSpec>::PadN>;
        const auto b_grid_desc_kpad_npad = tensor_operation::device::PadTensorDescriptor(
            b_grid_desc_k_n, make_tuple(K0 * K1, NPerBlock), DoPads{});

        return transform_tensor_descriptor(
            b_grid_desc_kpad_npad,
            make_tuple(make_unmerge_transform(make_tuple(KBatch, K0, K1)),
                       make_pass_through_transform(b_grid_desc_kpad_npad.GetLength(I1))),
            make_tuple(Sequence<0>{}, Sequence<1>{}),
            make_tuple(Sequence<0, 1, 3>{}, Sequence<2>{}));
    }

    __host__ __device__ static auto MakeCGridDescriptor_M_N(index_t M, index_t N, index_t StrideC)
    {
        const auto c_grid_desc_m_n = [&]() {
            if constexpr(is_same<tensor_layout::gemm::RowMajor, CLayout>::value)
            {
                return make_naive_tensor_descriptor(make_tuple(M, N), make_tuple(StrideC, I1));
            }
            else if constexpr(is_same<tensor_layout::gemm::ColumnMajor, CLayout>::value)
            {
                return make_naive_tensor_descriptor(make_tuple(M, N), make_tuple(I1, StrideC));
            }
        }();

        using DoPads = Sequence<tensor_operation::device::GemmPadM<GemmSpec>::PadM,
                                tensor_operation::device::GemmPadN<GemmSpec>::PadN>;
        return tensor_operation::device::PadTensorDescriptor(
            c_grid_desc_m_n, make_tuple(MPerBlock, NPerBlock), DoPads{});
    }

    __host__ __device__ static constexpr index_t GetSharedMemoryNumberOfByte()
    {
        constexpr auto max_lds_align = K1;

        // LDS allocation for A and B: be careful of alignment
        constexpr auto a_k0_m_k1_block_desc = GetABlockDescriptor_AK0PerBlock_MPerBlock_AK1();
        constexpr auto b_k0_n_k1_block_desc = GetBBlockDescriptor_BK0PerBlock_NPerBlock_BK1();

        // LDS allocation for A and B: be careful of alignment
        constexpr auto a_block_space_size =
            math::integer_least_multiple(a_k0_m_k1_block_desc.GetElementSpaceSize(), max_lds_align);

        constexpr auto b_block_space_size =
            math::integer_least_multiple(b_k0_n_k1_block_desc.GetElementSpaceSize(), max_lds_align);

        return (a_block_space_size + b_block_space_size) * sizeof(FloatAB);
    }

    __host__ __device__ static constexpr bool CheckValidity(const Argument& karg)
    {
        if constexpr(!(GemmSpec == tensor_operation::device::GemmSpecialization::MPadding ||
                       GemmSpec == tensor_operation::device::GemmSpecialization::MNPadding ||
                       GemmSpec == tensor_operation::device::GemmSpecialization::MKPadding ||
                       GemmSpec == tensor_operation::device::GemmSpecialization::MNKPadding))
        {
            if(!(karg.M % MPerBlock == 0))
            {
#if DEBUG_LOG
                std::cout << "Arg M value is not a multiple of MPerBlock! M: " << karg.M << " "
                          << __FILE__ << ":" << __LINE__ << ", in function: " << __func__
                          << std::endl;

#endif // DEBUG_LOG
                return false;
            }
        }
        if constexpr(!(GemmSpec == tensor_operation::device::GemmSpecialization::NPadding ||
                       GemmSpec == tensor_operation::device::GemmSpecialization::MNPadding ||
                       GemmSpec == tensor_operation::device::GemmSpecialization::NKPadding ||
                       GemmSpec == tensor_operation::device::GemmSpecialization::MNKPadding))
        {
            if(!(karg.N % NPerBlock == 0))
            {
#if DEBUG_LOG
                std::cout << "Arg N value is not a multiple of NPerBlock! N: " << karg.N << " "
                          << __FILE__ << ":" << __LINE__ << ", in function: " << __func__
                          << std::endl;

#endif // DEBUG_LOG
                return false;
            }
        }

        if constexpr(is_same<tensor_layout::gemm::RowMajor, ALayout>::value)
        {
            if(karg.K % ABlockTransferSrcScalarPerVector != 0)
            {
#if DEBUG_LOG
                std::cout << "Arg K (" << karg.K
                          << ") value is not a multiple of ABlockTransferSrcScalarPerVector ("
                          << ABlockTransferSrcScalarPerVector << " )! " << __FILE__ << ":"
                          << __LINE__ << ", in function: " << __func__ << std::endl;

#endif // DEBUG_LOG
                return false;
            }
        }
        else
        {
            if(karg.M % ABlockTransferSrcScalarPerVector != 0)
            {
#if DEBUG_LOG
                std::cout << "Arg M (" << karg.M
                          << ") value is not a multiple of ABlockTransferSrcScalarPerVector ("
                          << ABlockTransferSrcScalarPerVector << " )! " << __FILE__ << ":"
                          << __LINE__ << ", in function: " << __func__ << std::endl;

#endif // DEBUG_LOG
                return false;
            }
        }

        if constexpr(is_same<tensor_layout::gemm::RowMajor, BLayout>::value)
        {
            if(karg.N % BBlockTransferSrcScalarPerVector != 0)
            {
#if DEBUG_LOG
                std::cout << "Arg N (" << karg.N
                          << ") value is not a multiple of BBlockTransferSrcScalarPerVector ("
                          << BBlockTransferSrcScalarPerVector << " )! " << __FILE__ << ":"
                          << __LINE__ << ", in function: " << __func__ << std::endl;

#endif // DEBUG_LOG
                return false;
            }
        }
        else
        {
            if(karg.K % BBlockTransferSrcScalarPerVector != 0)
            {
#if DEBUG_LOG
                std::cout << "Arg K (" << karg.K
                          << ") value is not a multiple of BBlockTransferSrcScalarPerVector ("
                          << BBlockTransferSrcScalarPerVector << " )! " << __FILE__ << ":"
                          << __LINE__ << ", in function: " << __func__ << std::endl;

#endif // DEBUG_LOG
                return false;
            }
        }

        const auto num_k_loop = karg.K0 / K0PerBlock;
        if(!GridwiseGemmPipe::IsSupported(num_k_loop))
        {
#if DEBUG_LOG
            std::cout << "The number of k loops (" << num_k_loop
                      << ") value is not supported by GridwiseGemm Pipeline."
                      << " K0: " << karg.K0 << ", K0PerBlock: " << K0PerBlock << " " << __FILE__
                      << ":" << __LINE__ << ", in function: " << __func__ << std::endl;
#endif // DEBUG_LOG
            return false;
        }

        return true;
    }

    __host__ __device__ static constexpr bool CalculateHasMainK0BlockLoop(index_t K0)
    {
        const index_t num_loop = K0 / K0PerBlock;
        return GridwiseGemmPipe::CalculateHasMainLoop(num_loop);
    }

    template <typename CGridDesc>
    __host__ __device__ static constexpr auto
    MakeCGridDescriptor_M0_N0_M1_N1_M2_N2_N3_N4(const CGridDesc& c_grid_desc_m_n)
    {
        using ABlockDesc_AK0_M_AK1 =
            remove_cvref_t<decltype(GetABlockDescriptor_AK0PerBlock_MPerBlock_AK1())>;
        using BBlockDesc_AK0_N_AK1 =
            remove_cvref_t<decltype(GetBBlockDescriptor_BK0PerBlock_NPerBlock_BK1())>;

        using GemmAMmaTileDesc =
            remove_cvref_t<decltype(MakeGemmAMmaTileDescriptor_M0_M1_M2_K(ABlockDesc_AK0_M_AK1{}))>;
        using GemmBMmaTileDesc =
            remove_cvref_t<decltype(MakeGemmBMmaTileDescriptor_N0_N1_N2_K(BBlockDesc_AK0_N_AK1{}))>;

        constexpr index_t KPack =
            math::max(K1, MfmaSelector<FloatAB, MPerXDL, NPerXDL>::selected_mfma.k_per_blk);

        using BlockwiseGemm = BlockwiseGemmXdlops_v2<BlockSize,
                                                     FloatAB,
                                                     FloatAcc,
                                                     ABlockDesc_AK0_M_AK1,
                                                     BBlockDesc_AK0_N_AK1,
                                                     GemmAMmaTileDesc,
                                                     GemmBMmaTileDesc,
                                                     MPerBlock,
                                                     NPerBlock,
                                                     KPerBlock,
                                                     MPerXDL,
                                                     NPerXDL,
                                                     MXdlPerWave,
                                                     NXdlPerWave,
                                                     KPack,
                                                     true>; // TransposeC
                                                            // A MMaTileKStride
                                                            // B MMaTileKStride

        return BlockwiseGemm::MakeCGridDescriptor_M0_N0_M1_N1_M2_N2_N3_N4(c_grid_desc_m_n);
    }

    // return block_id to C matrix tile idx (m0, n0) mapping
    template <typename CGridDesc>
    __host__ __device__ static constexpr auto MakeCBlockClusterAdaptor(
        const CGridDesc& c_m_n_grid_desc, index_t /* M01 */, index_t /* N01 */, index_t KBatch)
    {
        return BlockToCTileMap_KSplit_M00_N0_M01Adapt<MPerBlock, NPerBlock, CGridDesc>(
            c_m_n_grid_desc, 8, KBatch);
    }

    // return block_id to C matrix tile idx (m0, n0, k_split) mapping
    __host__ __device__ static constexpr auto MakeDefaultBlock2CTileMap()
    {
        return BlockToCTileMap_3DGrid_KSplit<MPerBlock, NPerBlock>();
    }

    using CGridDesc_M_N = remove_cvref_t<decltype(MakeCGridDescriptor_M_N(1, 1, 1))>;
    using CGridDescriptor_M0_N0_M1_N1_M2_N2_N3_N4 =
        remove_cvref_t<decltype(MakeCGridDescriptor_M0_N0_M1_N1_M2_N2_N3_N4(CGridDesc_M_N{}))>;
    using DefaultBlock2CTileMap = remove_cvref_t<decltype(MakeDefaultBlock2CTileMap())>;

    template <bool HasMainKBlockLoop,
              InMemoryDataOperationEnum CGlobalMemoryDataOperation,
              typename Block2CTileMap>
    __device__ static void Run(const Argument& karg,
                               void* __restrict__ p_shared_block,
                               const Block2CTileMap& block_2_ctile_map)
    {
        const FloatAB* p_a_grid = karg.p_a_grid;
        const FloatAB* p_b_grid = karg.p_b_grid;
        FloatC* p_c_grid        = karg.p_c_grid;
        const auto a_b_k0_m_k1_grid_desc =
            MakeAGridDescriptor_KBatch_K0_M_K1(karg.M, karg.K, karg.StrideA, karg.k_batch, karg.K0);
        const auto b_b_k0_n_k1_grid_desc =
            MakeBGridDescriptor_KBatch_K0_N_K1(karg.K, karg.N, karg.StrideB, karg.k_batch, karg.K0);
        const auto c_grid_desc_m_n = MakeCGridDescriptor_M_N(karg.M, karg.N, karg.StrideC);
        const auto c_grid_desc_m0_n0_m1_n1_m2_n2_n3_n4 =
            MakeCGridDescriptor_M0_N0_M1_N1_M2_N2_N3_N4(c_grid_desc_m_n);

        const AElementwiseOperation a_element_op = AElementwiseOperation{};
        const BElementwiseOperation b_element_op = BElementwiseOperation{};
        const CElementwiseOperation c_element_op = CElementwiseOperation{};

        const auto a_grid_buf = make_dynamic_buffer<AddressSpaceEnum::Global>(
            p_a_grid, a_b_k0_m_k1_grid_desc.GetElementSpaceSize());
        const auto b_grid_buf = make_dynamic_buffer<AddressSpaceEnum::Global>(
            p_b_grid, b_b_k0_n_k1_grid_desc.GetElementSpaceSize());
        auto c_grid_buf = make_dynamic_buffer<AddressSpaceEnum::Global>(
            p_c_grid, c_grid_desc_m0_n0_m1_n1_m2_n2_n3_n4.GetElementSpaceSize());

        // divide block work by [KBatch, M, N]
        const auto block_work_idx =
            block_2_ctile_map.CalculateBottomIndex(make_multi_index(get_block_1d_id()));

        if(!block_2_ctile_map.ValidCTileIndex(
               block_work_idx,
               make_tuple(a_b_k0_m_k1_grid_desc.GetLength(I2) / MPerBlock,
                          b_b_k0_n_k1_grid_desc.GetLength(I2) / NPerBlock)))
        {
            return;
        }

        const index_t block_m_id = __builtin_amdgcn_readfirstlane(block_work_idx[I1]);
        const index_t block_n_id = __builtin_amdgcn_readfirstlane(block_work_idx[I2]);
        const index_t k_batch_id = __builtin_amdgcn_readfirstlane(block_work_idx[I0]);

        // HACK: this force m/n_block_data_idx_on_grid into SGPR
        const index_t m_block_data_idx_on_grid =
            __builtin_amdgcn_readfirstlane(block_m_id * MPerBlock);

        const index_t n_block_data_idx_on_grid =
            __builtin_amdgcn_readfirstlane(block_n_id * NPerBlock);

        // A matrix in LDS memory, dst of blockwise copy
        constexpr auto a_k0_m_k1_block_desc = GetABlockDescriptor_AK0PerBlock_MPerBlock_AK1();
        constexpr auto a_b_k0_m_k1_block_desc =
            GetABlockDescriptor_KBatch_AK0PerBlock_MPerBlock_AK1();

        // B matrix in LDS memory, dst of blockwise copy
        constexpr auto b_k0_n_k1_block_desc = GetBBlockDescriptor_BK0PerBlock_NPerBlock_BK1();
        constexpr auto b_b_k0_n_k1_block_desc =
            GetBBlockDescriptor_KBatch_BK0PerBlock_NPerBlock_BK1();

        // A matrix blockwise copy
        auto a_blockwise_copy =
            ThreadGroupTensorSliceTransfer_v4r1<ThisThreadBlock,
                                                AElementwiseOperation,
                                                ck::tensor_operation::element_wise::PassThrough,
                                                InMemoryDataOperationEnum::Set,
                                                Sequence<1, K0PerBlock, MPerBlock, K1>,
                                                ABlockTransferThreadClusterLengths_K0_M_K1,
                                                ABlockTransferThreadClusterArrangeOrder,
                                                FloatAB,
                                                FloatAB,
                                                decltype(a_b_k0_m_k1_grid_desc),
                                                decltype(a_b_k0_m_k1_block_desc),
                                                ABlockTransferSrcAccessOrder,
                                                Sequence<0, 2, 1, 3>,
                                                ABlockTransferSrcVectorDim,
                                                3,
                                                ABlockTransferSrcScalarPerVector,
                                                ABlockTransferDstScalarPerVector_K1,
                                                1,
                                                1,
                                                AThreadTransferSrcResetCoordinateAfterRun,
                                                true>(
                a_b_k0_m_k1_grid_desc,
                make_multi_index(k_batch_id, 0, m_block_data_idx_on_grid, 0),
                a_element_op,
                a_b_k0_m_k1_block_desc,
                make_multi_index(0, 0, 0, 0),
                ck::tensor_operation::element_wise::PassThrough{});

        // B matrix blockwise copy
        auto b_blockwise_copy =
            ThreadGroupTensorSliceTransfer_v4r1<ThisThreadBlock,
                                                BElementwiseOperation,
                                                ck::tensor_operation::element_wise::PassThrough,
                                                InMemoryDataOperationEnum::Set,
                                                Sequence<1, K0PerBlock, NPerBlock, K1>,
                                                BBlockTransferThreadClusterLengths_K0_N_K1,
                                                BBlockTransferThreadClusterArrangeOrder,
                                                FloatAB,
                                                FloatAB,
                                                decltype(b_b_k0_n_k1_grid_desc),
                                                decltype(b_b_k0_n_k1_block_desc),
                                                BBlockTransferSrcAccessOrder,
                                                Sequence<0, 2, 1, 3>,
                                                BBlockTransferSrcVectorDim,
                                                3,
                                                BBlockTransferSrcScalarPerVector,
                                                BBlockTransferDstScalarPerVector_K1,
                                                1,
                                                1,
                                                BThreadTransferSrcResetCoordinateAfterRun,
                                                true>(
                b_b_k0_n_k1_grid_desc,
                make_multi_index(k_batch_id, 0, n_block_data_idx_on_grid, 0),
                b_element_op,
                b_b_k0_n_k1_block_desc,
                make_multi_index(0, 0, 0, 0),
                ck::tensor_operation::element_wise::PassThrough{});

        // GEMM definition
        //   c_mtx += transpose(a_mtx) * b_mtx
        //     a_mtx[K0PerBlock, MPerBlock] is in LDS
        //     b_mtx[K0PerBlock, NPerBlock] is in LDS
        //     c_mtx[MPerBlock, NPerBlock] is distributed among threads, and saved in
        //       register
        // sanity check

        constexpr index_t KPack =
            math::max(K1, MfmaSelector<FloatAB, MPerXDL, NPerXDL>::selected_mfma.k_per_blk);

        auto blockwise_gemm = BlockwiseGemmXdlops_v2<
            BlockSize,
            FloatAB,
            FloatAcc,
            decltype(a_k0_m_k1_block_desc),
            decltype(b_k0_n_k1_block_desc),
            decltype(MakeGemmAMmaTileDescriptor_M0_M1_M2_K(a_k0_m_k1_block_desc)),
            decltype(MakeGemmBMmaTileDescriptor_N0_N1_N2_K(b_k0_n_k1_block_desc)),
            MPerBlock,
            NPerBlock,
            KPerBlock,
            MPerXDL,
            NPerXDL,
            MXdlPerWave,
            NXdlPerWave,
            KPack,
            true>{}; // TransposeC
        // A MMaTileKStride
        // B MMaTileKStride

        auto c_thread_buf = blockwise_gemm.GetCThreadBuffer();

        constexpr auto max_lds_align = K1;
        // LDS allocation for A and B: be careful of alignment
        constexpr auto a_block_space_size =
            math::integer_least_multiple(a_k0_m_k1_block_desc.GetElementSpaceSize(), max_lds_align);

        FloatAB* p_a_block = static_cast<FloatAB*>(p_shared_block);
        FloatAB* p_b_block = static_cast<FloatAB*>(p_shared_block) + a_block_space_size;

        constexpr auto a_block_slice_copy_step = make_multi_index(0, K0PerBlock, 0, 0);
        constexpr auto b_block_slice_copy_step = make_multi_index(0, K0PerBlock, 0, 0);

        auto a_block_buf = make_dynamic_buffer<AddressSpaceEnum::Lds>(
            p_a_block, a_k0_m_k1_block_desc.GetElementSpaceSize());
        auto b_block_buf = make_dynamic_buffer<AddressSpaceEnum::Lds>(
            p_b_block, b_k0_n_k1_block_desc.GetElementSpaceSize());

        // gridwise GEMM pipeline
        const index_t num_k_block_main_loop = __builtin_amdgcn_readfirstlane(
            (a_b_k0_m_k1_grid_desc.GetLength(I1) * a_b_k0_m_k1_grid_desc.GetLength(I3)) /
            KPerBlock);

        const auto gridwise_gemm_pipeline = GridwiseGemmPipe{};

        gridwise_gemm_pipeline.template Run<HasMainKBlockLoop>(a_b_k0_m_k1_grid_desc,
                                                               a_b_k0_m_k1_block_desc,
                                                               a_blockwise_copy,
                                                               a_grid_buf,
                                                               a_block_buf,
                                                               a_block_slice_copy_step,
                                                               b_b_k0_n_k1_grid_desc,
                                                               b_b_k0_n_k1_block_desc,
                                                               b_blockwise_copy,
                                                               b_grid_buf,
                                                               b_block_buf,
                                                               b_block_slice_copy_step,
                                                               blockwise_gemm,
                                                               c_thread_buf,
                                                               num_k_block_main_loop);

        // output: register to global memory
        {
            constexpr auto c_thread_desc_m0_n0_m1_n1_m2_n2_n3_n4 =
                blockwise_gemm.GetCThreadDescriptor_M0_N0_M1_N1_M2_N2_N3_N4();

            // c_block_desc_m0_n0_m1_n1_m2_n2_n3_n4 is only used to get lengths
            constexpr auto c_block_desc_m0_n0_m1_n1_m2_n2_n3_n4 =
                blockwise_gemm.GetCBlockDescriptor_M0_N0_M1_N1_M2_N2_N3_N4();

            constexpr auto M0 = c_block_desc_m0_n0_m1_n1_m2_n2_n3_n4.GetLength(I0);
            constexpr auto N0 = c_block_desc_m0_n0_m1_n1_m2_n2_n3_n4.GetLength(I1);
            constexpr auto M1 = c_block_desc_m0_n0_m1_n1_m2_n2_n3_n4.GetLength(I2);
            constexpr auto N1 = c_block_desc_m0_n0_m1_n1_m2_n2_n3_n4.GetLength(I3);
            constexpr auto M2 = c_block_desc_m0_n0_m1_n1_m2_n2_n3_n4.GetLength(I4);
            constexpr auto N2 = c_block_desc_m0_n0_m1_n1_m2_n2_n3_n4.GetLength(I5);
            constexpr auto N3 = c_block_desc_m0_n0_m1_n1_m2_n2_n3_n4.GetLength(I6);
            constexpr auto N4 = c_block_desc_m0_n0_m1_n1_m2_n2_n3_n4.GetLength(I7);

            // calculate origin of thread output tensor on global memory
            // blockwise GEMM c matrix starting index
            const auto c_thread_mtx_on_block =
                blockwise_gemm.CalculateCThreadOriginDataIndex(I0, I0, I0, I0);

            const index_t m_thread_data_on_grid =
                m_block_data_idx_on_grid + c_thread_mtx_on_block[I0];

            const index_t n_thread_data_on_grid =
                n_block_data_idx_on_grid + c_thread_mtx_on_block[I1];

            const auto m_thread_data_on_grid_to_m0_m1_m2_adaptor = make_single_stage_tensor_adaptor(
                make_tuple(make_merge_transform(make_tuple(M0, M1, M2))),
                make_tuple(Sequence<0, 1, 2>{}),
                make_tuple(Sequence<0>{}));

            const auto m_thread_data_on_grid_idx =
                m_thread_data_on_grid_to_m0_m1_m2_adaptor.CalculateBottomIndex(
                    make_multi_index(m_thread_data_on_grid));

            const auto n_thread_data_on_grid_to_n0_n1_n2_n3_n4_adaptor =
                make_single_stage_tensor_adaptor(
                    make_tuple(make_merge_transform(make_tuple(N0, N1, N2, N3, N4))),
                    make_tuple(Sequence<0, 1, 2, 3, 4>{}),
                    make_tuple(Sequence<0>{}));

            const auto n_thread_data_on_grid_idx =
                n_thread_data_on_grid_to_n0_n1_n2_n3_n4_adaptor.CalculateBottomIndex(
                    make_multi_index(n_thread_data_on_grid));

            auto c_thread_copy = ThreadwiseTensorSliceTransfer_v1r3<
                FloatAcc,
                FloatC,
                decltype(c_thread_desc_m0_n0_m1_n1_m2_n2_n3_n4),
                decltype(c_grid_desc_m0_n0_m1_n1_m2_n2_n3_n4),
                CElementwiseOperation,
                Sequence<M0, N0, I1, I1, I1, N2, I1, N4>,
                Sequence<0, 2, 4, 1, 3, 5, 6, 7>, // CThreadTransferDstAccessOrder,
                7,                                // CThreadTransferDstVectorDim,
                N4.value,                         // CThreadTransferDstScalarPerVector,
                CGlobalMemoryDataOperation,
                1,
                true>{c_grid_desc_m0_n0_m1_n1_m2_n2_n3_n4,
                      make_multi_index(m_thread_data_on_grid_idx[I0],
                                       n_thread_data_on_grid_idx[I0],
                                       m_thread_data_on_grid_idx[I1],
                                       n_thread_data_on_grid_idx[I1],
                                       m_thread_data_on_grid_idx[I2],
                                       n_thread_data_on_grid_idx[I2],
                                       n_thread_data_on_grid_idx[I3],
                                       n_thread_data_on_grid_idx[I4]),
                      c_element_op};

            c_thread_copy.Run(c_thread_desc_m0_n0_m1_n1_m2_n2_n3_n4,
                              make_tuple(I0, I0, I0, I0, I0, I0, I0, I0),
                              c_thread_buf,
                              c_grid_desc_m0_n0_m1_n1_m2_n2_n3_n4,
                              c_grid_buf);
        }
    }

    static std::string GetTypeString()
    {
        auto str = std::stringstream();

        // clang-format off
        str << "GridwiseGemmXdlSplitKDirectCWriteOut"
            << getGemmSpecializationString(GemmSpec) << "_"
            << std::string(ALayout::name)[0]
            << std::string(BLayout::name)[0]
            << std::string(CLayout::name)[0]
            << "_"
            << "B" << BlockSize << "_"
            << "Vec" << ABlockTransferSrcScalarPerVector << "x"
            << BBlockTransferSrcScalarPerVector << "x"
            << MPerBlock << "x"
            << NPerBlock << "x"
            << K0PerBlock << "x"
            << K1 ;
        // clang-format on

        return str.str();
    }
};

} // namespace ck
