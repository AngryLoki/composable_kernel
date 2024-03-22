// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2023, Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "ck/tensor_operation/gpu/block/blockwise_gemm_pipeline_xdlops_base.hpp"

namespace ck {

// Compute optimized pipeline
// GlobalPrefetchStages: 2
// LocalPreFillStages: 1
// LocalPreFetchStages: 1
// LocalSharedMemoryBuffer: 1

template <BlockGemmPipelineScheduler BlkGemmPipelineVer,
          index_t BlockSize,
          typename FloatAB,
          typename FloatAcc,
          typename ATileDesc,
          typename BTileDesc,
          typename AMmaTileDesc,
          typename BMmaTileDesc,
          index_t ABlockTransferSrcScalarPerVector,
          index_t BBlockTransferSrcScalarPerVector,
          index_t MPerBlock,
          index_t NPerBlock,
          index_t KPerBlock,
          index_t MPerXDL,
          index_t NPerXDL,
          index_t MRepeat,
          index_t NRepeat,
          index_t KPacks>
struct BlockwiseGemmXdlops_pipeline_v3
{
};

template <index_t BlockSize,
          typename FloatAB,
          typename FloatAcc,
          typename ATileDesc,
          typename BTileDesc,
          typename AMmaTileDesc,
          typename BMmaTileDesc,
          index_t ABlockTransferSrcScalarPerVector,
          index_t BBlockTransferSrcScalarPerVector,
          index_t MPerBlock,
          index_t NPerBlock,
          index_t KPerBlock,
          index_t MPerXDL,
          index_t NPerXDL,
          index_t MRepeat,
          index_t NRepeat,
          index_t KPack
          // ,bool TransposeC //disable transposec right now...
          >
struct BlockwiseGemmXdlops_pipeline_v3<BlockGemmPipelineScheduler::Intrawave,
                                       BlockSize,
                                       FloatAB,
                                       FloatAcc,
                                       ATileDesc,
                                       BTileDesc,
                                       AMmaTileDesc,
                                       BMmaTileDesc,
                                       ABlockTransferSrcScalarPerVector,
                                       BBlockTransferSrcScalarPerVector,
                                       MPerBlock,
                                       NPerBlock,
                                       KPerBlock,
                                       MPerXDL,
                                       NPerXDL,
                                       MRepeat,
                                       NRepeat,
                                       KPack>
    : BlockwiseGemmXdlops_pipeline_base<BlockSize,
                                        FloatAB,
                                        FloatAcc,
                                        ATileDesc,
                                        BTileDesc,
                                        AMmaTileDesc,
                                        BMmaTileDesc,
                                        ABlockTransferSrcScalarPerVector,
                                        BBlockTransferSrcScalarPerVector,
                                        MPerBlock,
                                        NPerBlock,
                                        KPerBlock,
                                        MPerXDL,
                                        NPerXDL,
                                        MRepeat,
                                        NRepeat,
                                        KPack>

{
    using Base = BlockwiseGemmXdlops_pipeline_base<BlockSize,
                                                   FloatAB,
                                                   FloatAcc,
                                                   ATileDesc,
                                                   BTileDesc,
                                                   AMmaTileDesc,
                                                   BMmaTileDesc,
                                                   ABlockTransferSrcScalarPerVector,
                                                   BBlockTransferSrcScalarPerVector,
                                                   MPerBlock,
                                                   NPerBlock,
                                                   KPerBlock,
                                                   MPerXDL,
                                                   NPerXDL,
                                                   MRepeat,
                                                   NRepeat,
                                                   KPack>;
    using Base::I0;
    using Base::I1;
    using Base::KRepeat;
    using Base::xdlops_gemm;
    using typename Base::HotLoopInstList;

    using Base::CalculateCThreadOriginDataIndex;
    using Base::CalculateCThreadOriginDataIndex8D;
    using Base::GetCBlockDescriptor_G_M0_N0_M1_N1_M2_M3_M4_N2;
    using Base::GetCBlockDescriptor_M0_N0_M1_N1_M2_M3_M4_N2;
    using Base::GetCBlockDescriptor_M0_N0_M1_N1_M2_N2_N3_N4;
    using Base::GetCThreadBuffer;
    using Base::GetCThreadDescriptor_G_M0_N0_M1_N1_M2_M3_M4_N2;
    using Base::GetCThreadDescriptor_M0_N0_M1_N1_M2_M3_M4_N2;
    using Base::GetCThreadDescriptor_M0_N0_M1_N1_M2_N2_N3_N4;
    using Base::MakeCGridDescriptor_G_M0_N0_M1_N1_M2_M3_M4_N2;
    using Base::MakeCGridDescriptor_M0_N0_M1_N1_M2_M3_M4_N2;

    using Base::a_block_desc_m0_m1_m2_k;
    using Base::b_block_desc_n0_n1_n2_k;

    using Base::AMmaKStride;
    using Base::BMmaKStride;

    static constexpr index_t PrefetchStages  = 2;
    static constexpr index_t PrefillStages   = 1;
    static constexpr index_t GlobalBufferNum = 1;

    __host__ static constexpr bool BlockHasHotloop(index_t num_loop)
    {
        return num_loop > PrefetchStages;
    }

    __host__ static constexpr TailNumber BlockLoopTailNum(index_t num_loop)
    {
        ignore = num_loop;
        return TailNumber::Full;
    }

    __device__ static constexpr auto HotLoopScheduler()
    {
        // TODO: A/B split schedule
#if 0
        // schedule
        constexpr auto num_ds_read_inst =
            HotLoopInstList::A_LDS_Read_Inst_Num + HotLoopInstList::B_LDS_Read_Inst_Num;
        constexpr auto num_ds_write_inst =
            HotLoopInstList::A_LDS_Write_Inst_Num + HotLoopInstList::B_LDS_Write_Inst_Num;

        constexpr auto num_buffer_load_inst =
            HotLoopInstList::A_Buffer_Load_Inst_Num + HotLoopInstList::B_Buffer_Load_Inst_Num;

        constexpr auto num_mfma_inst = HotLoopInstList::C_MFMA_Inst_Num;

        // stage 1
        constexpr auto num_mfma_stage1       = num_mfma_inst - num_ds_read_inst;
        constexpr auto num_mfma_per_issue    = num_mfma_stage1 / num_buffer_load_inst;
        constexpr auto num_dswrite_per_issue = num_ds_write_inst / num_buffer_load_inst;
        constexpr auto num_issue_more = num_mfma_stage1 - num_mfma_per_issue * num_buffer_load_inst;
        constexpr auto num_issue_less = num_buffer_load_inst - num_issue_more;
        static_for<0, num_issue_more, 1>{}([&](auto i) {
            ignore = i;
            __builtin_amdgcn_sched_group_barrier(0x200, num_dswrite_per_issue, 0); // DS write
            __builtin_amdgcn_sched_group_barrier(0x008, 1, 0);                     // MFMA
            __builtin_amdgcn_sched_group_barrier(0x020, 1, 0);                     // VMEM read
            __builtin_amdgcn_sched_group_barrier(0x008, num_mfma_per_issue, 0);    // MFMA
        });
        static_for<0, num_issue_less, 1>{}([&](auto i) {
            ignore = i;
            __builtin_amdgcn_sched_group_barrier(0x200, num_dswrite_per_issue, 0);  // DS write
            __builtin_amdgcn_sched_group_barrier(0x008, 1, 0);                      // MFMA
            __builtin_amdgcn_sched_group_barrier(0x020, 1, 0);                      // VMEM read
            __builtin_amdgcn_sched_group_barrier(0x008, num_mfma_per_issue - 1, 0); // MFMA
        });

        // stage 2
        static_for<0, num_ds_read_inst, 1>{}([&](auto i) {
            ignore = i;
            __builtin_amdgcn_sched_group_barrier(0x100, 1, 0); // DS read
            __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA
        });
#elif 1
        // schedule
        constexpr auto num_ds_read_inst =
            HotLoopInstList::A_LDS_Read_Inst_Num + HotLoopInstList::B_LDS_Read_Inst_Num;
        constexpr auto num_ds_write_inst_a = HotLoopInstList::A_LDS_Write_Inst_Num;
        constexpr auto num_ds_write_inst_b = HotLoopInstList::B_LDS_Write_Inst_Num;

        constexpr auto num_buffer_load_inst_a = HotLoopInstList::A_Buffer_Load_Inst_Num;
        constexpr auto num_buffer_load_inst_b = HotLoopInstList::B_Buffer_Load_Inst_Num;

        constexpr auto num_mfma_inst = HotLoopInstList::C_MFMA_Inst_Num;

        // stage 1
        constexpr auto num_mfma_stage1 = num_mfma_inst - num_ds_read_inst;
        constexpr auto num_mfma_per_issue =
            num_mfma_stage1 / (num_buffer_load_inst_a + num_buffer_load_inst_b);
        constexpr auto num_dswrite_per_issue_a = num_ds_write_inst_a / num_buffer_load_inst_a;
        constexpr auto num_dswrite_per_issue_b = num_ds_write_inst_b / num_buffer_load_inst_b;

        static_for<0, num_buffer_load_inst_a, 1>{}([&](auto i) {
            ignore = i;
            static_for<0, num_dswrite_per_issue_a, 1>{}([&](auto idswrite) {
                ignore = idswrite;
                __builtin_amdgcn_sched_group_barrier(0x200, 1, 0); // DS write
                __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA
            });
            __builtin_amdgcn_sched_group_barrier(0x020, 1, 0); // VMEM read
            __builtin_amdgcn_sched_group_barrier(
                0x008, num_mfma_per_issue - num_dswrite_per_issue_a, 0); // MFMA
        });
        static_for<0, num_buffer_load_inst_b, 1>{}([&](auto i) {
            ignore = i;
            static_for<0, num_dswrite_per_issue_b, 1>{}([&](auto idswrite) {
                ignore = idswrite;
                __builtin_amdgcn_sched_group_barrier(0x200, 1, 0); // DS write
                __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA
            });
            __builtin_amdgcn_sched_group_barrier(0x020, 1, 0); // VMEM read
            __builtin_amdgcn_sched_group_barrier(
                0x008, num_mfma_per_issue - num_dswrite_per_issue_b, 0); // MFMA
        });

        // stage 2
        static_for<0, num_ds_read_inst, 1>{}([&](auto i) {
            ignore = i;
            __builtin_amdgcn_sched_group_barrier(0x100, 1, 0); // DS read
            __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA
        });

        __builtin_amdgcn_sched_barrier(0);
#endif
    }

    template <bool HasMainLoop,
              TailNumber TailNum,
              typename AGridDesc,
              typename ABlockDesc,
              typename ABlockTransfer,
              typename AGridBuffer,
              typename ABlockBuffer,
              typename ABlockTransferStep,
              typename BGridDesc,
              typename BBlockDesc,
              typename BBlockTransfer,
              typename BGridBuffer,
              typename BBlockBuffer,
              typename BBlockTransferStep,
              typename CThreadBuffer>
    __device__ void Run(const AGridDesc& a_grid_desc,
                        const ABlockDesc& a_block_desc,
                        ABlockTransfer& a_blockwise_copy,
                        const AGridBuffer& a_grid_buf,
                        ABlockBuffer& a_block_buf,
                        const ABlockTransferStep& a_block_copy_step,
                        const BGridDesc& b_grid_desc,
                        const BBlockDesc& b_block_desc,
                        BBlockTransfer& b_blockwise_copy,
                        const BGridBuffer& b_grid_buf,
                        BBlockBuffer& b_block_buf,
                        const BBlockTransferStep& b_block_copy_step,
                        CThreadBuffer& c_thread_buf,
                        index_t num_loop) const
    {
        __builtin_amdgcn_sched_barrier(0);
        auto a_thread_buf = make_static_buffer<AddressSpaceEnum::Vgpr, FloatAB>(
            a_thread_desc_.GetElementSpaceSize());
        auto b_thread_buf = make_static_buffer<AddressSpaceEnum::Vgpr, FloatAB>(
            b_thread_desc_.GetElementSpaceSize());

        // Global prefetch 1
        a_blockwise_copy.RunRead(a_grid_desc, a_grid_buf);
        b_blockwise_copy.RunRead(b_grid_desc, b_grid_buf);

        a_blockwise_copy.MoveSrcSliceWindow(a_grid_desc, a_block_copy_step);
        b_blockwise_copy.MoveSrcSliceWindow(b_grid_desc, b_block_copy_step);

        // Local prefill 1
        a_blockwise_copy.RunWrite(a_block_desc, a_block_buf);
        b_blockwise_copy.RunWrite(b_block_desc, b_block_buf);
        // asm volatile("s_endpgm");

        // Global prefetch 2
        a_blockwise_copy.RunRead(a_grid_desc, a_grid_buf);
        b_blockwise_copy.RunRead(b_grid_desc, b_grid_buf);

        a_blockwise_copy.MoveSrcSliceWindow(a_grid_desc, a_block_copy_step);
        b_blockwise_copy.MoveSrcSliceWindow(b_grid_desc, b_block_copy_step);

        // Initialize C
        c_thread_buf.Clear();

        // Local prefetch 1
        block_sync_lds();
        static_for<0, KRepeat, 1>{}([&](auto k0) {
            static_for<0, MRepeat, 1>{}([&](auto m0) {
                a_thread_copy_.Run(a_block_desc_m0_m1_m2_k,
                                   make_tuple(m0, I0, I0, Number<k0 * AMmaKStride>{}),
                                   a_block_buf,
                                   a_thread_desc_,
                                   make_tuple(m0, I0, k0, I0),
                                   a_thread_buf);
            });
            static_for<0, NRepeat, 1>{}([&](auto n0) {
                b_thread_copy_.Run(b_block_desc_n0_n1_n2_k,
                                   make_tuple(n0, I0, I0, Number<k0 * BMmaKStride>{}),
                                   b_block_buf,
                                   b_thread_desc_,
                                   make_tuple(n0, I0, k0, I0),
                                   b_thread_buf);
            });
        });

        // printf("Tid: %03d, b: %04x %04x %04x %04x %04x %04x %04x %04x |%04x %04x %04x %04x %04x
        // %04x %04x %04x |%04x %04x %04x %04x %04x %04x %04x %04x |%04x %04x %04x %04x %04x %04x
        // %04x %04x |\n", get_thread_global_1d_id(),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<0>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<1>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<2>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<3>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<4>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<5>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<6>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<7>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<0+8>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<1+8>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<2+8>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<3+8>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<4+8>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<5+8>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<6+8>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<7+8>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<0+8+8>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<1+8+8>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<2+8+8>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<3+8+8>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<4+8+8>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<5+8+8>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<6+8+8>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<7+8+8>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<0+64>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<1+64>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<2+64>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<3+64>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<4+64>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<5+64>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<6+64>{}))),
        // *(reinterpret_cast<uint16_t*>(&b_thread_buf(Number<7+64>{}))));
        // asm volatile("s_endpgm");

        __builtin_amdgcn_sched_barrier(0);

        // main body
        if constexpr(HasMainLoop)
        {
            index_t i = 0;
            do
            {
                block_sync_lds();

                a_blockwise_copy.RunWrite(a_block_desc, a_block_buf);
                b_blockwise_copy.RunWrite(b_block_desc, b_block_buf);

                a_blockwise_copy.RunRead(a_grid_desc, a_grid_buf);
                b_blockwise_copy.RunRead(b_grid_desc, b_grid_buf);

                a_blockwise_copy.MoveSrcSliceWindow(a_grid_desc, a_block_copy_step);
                b_blockwise_copy.MoveSrcSliceWindow(b_grid_desc, b_block_copy_step);

                static_for<0, KRepeat, 1>{}([&](auto k0) {
                    static_for<0, MRepeat, 1>{}([&](auto m0) {
                        static_for<0, NRepeat, 1>{}([&](auto n0) {
                            vector_type<FloatAB, KPack> a_thread_vec;
                            vector_type<FloatAB, KPack> b_thread_vec;

                            static_for<0, KPack, 1>{}([&](auto ik) {
                                a_thread_vec.template AsType<FloatAB>()(ik) =
                                    a_thread_buf[Number<a_thread_desc_.CalculateOffset(
                                        make_tuple(m0, I0, k0, ik))>{}];
                                b_thread_vec.template AsType<FloatAB>()(ik) =
                                    b_thread_buf[Number<b_thread_desc_.CalculateOffset(
                                        make_tuple(n0, I0, k0, ik))>{}];
                            });

                            using mfma_input_type =
                                typename vector_type<FloatAB, xdlops_gemm.K1PerXdlops>::type;

                            constexpr index_t c_offset =
                                c_thread_desc_.CalculateOffset(make_tuple(m0, n0, 0));

                            xdlops_gemm.template Run(
                                a_thread_vec.template AsType<mfma_input_type>(),
                                b_thread_vec.template AsType<mfma_input_type>(),
                                c_thread_buf.GetVectorTypeReference(Number<c_offset>{}));
                        });
                    });
                });

                block_sync_lds();

                static_for<0, KRepeat, 1>{}([&](auto k0) {
                    static_for<0, MRepeat, 1>{}([&](auto m0) {
                        a_thread_copy_.Run(a_block_desc_m0_m1_m2_k,
                                           make_tuple(m0, I0, I0, Number<k0 * AMmaKStride>{}),
                                           a_block_buf,
                                           a_thread_desc_,
                                           make_tuple(m0, I0, k0, I0),
                                           a_thread_buf);
                    });
                    static_for<0, NRepeat, 1>{}([&](auto n0) {
                        b_thread_copy_.Run(b_block_desc_n0_n1_n2_k,
                                           make_tuple(n0, I0, I0, Number<k0 * BMmaKStride>{}),
                                           b_block_buf,
                                           b_thread_desc_,
                                           make_tuple(n0, I0, k0, I0),
                                           b_thread_buf);
                    });
                });

                HotLoopScheduler();
                __builtin_amdgcn_sched_barrier(0);

                i += 1;
            } while(i < (num_loop - 1));
        }
        // tail
        if constexpr(TailNum == TailNumber::Full)
        {
            static_for<0, KRepeat, 1>{}([&](auto k0) {
                static_for<0, MRepeat, 1>{}([&](auto m0) {
                    static_for<0, NRepeat, 1>{}([&](auto n0) {
                        vector_type<FloatAB, KPack> a_thread_vec;
                        vector_type<FloatAB, KPack> b_thread_vec;

                        static_for<0, KPack, 1>{}([&](auto ik) {
                            a_thread_vec.template AsType<FloatAB>()(ik) =
                                a_thread_buf[Number<a_thread_desc_.CalculateOffset(
                                    make_tuple(m0, I0, k0, ik))>{}];
                            b_thread_vec.template AsType<FloatAB>()(ik) =
                                b_thread_buf[Number<b_thread_desc_.CalculateOffset(
                                    make_tuple(n0, I0, k0, ik))>{}];
                        });

                        using mfma_input_type =
                            typename vector_type<FloatAB, xdlops_gemm.K1PerXdlops>::type;

                        constexpr index_t c_offset =
                            c_thread_desc_.CalculateOffset(make_tuple(m0, n0, 0));

                        xdlops_gemm.template Run(
                            a_thread_vec.template AsType<mfma_input_type>(),
                            b_thread_vec.template AsType<mfma_input_type>(),
                            c_thread_buf.GetVectorTypeReference(Number<c_offset>{}));
                    });
                });
            });
            // __builtin_amdgcn_sched_barrier(0);
        }
    }

    protected:
    using Base::a_thread_copy_;
    using Base::a_thread_desc_;
    using Base::b_thread_copy_;
    using Base::b_thread_desc_;
    using Base::c_thread_desc_;
};

} // namespace ck
