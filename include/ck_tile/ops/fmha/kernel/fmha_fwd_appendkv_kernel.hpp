// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2024, Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "ck_tile/core.hpp"
#include "ck_tile/ops/common.hpp"
#include <string>
#include <type_traits>

namespace ck_tile {

template <typename TilePartitioner_, typename FmhaPipeline_>
struct FmhaFwdAppendKVKernel
{
    using TilePartitioner                         = ck_tile::remove_cvref_t<TilePartitioner_>;
    using FmhaPipeline                            = ck_tile::remove_cvref_t<FmhaPipeline_>;
    static constexpr ck_tile::index_t kBlockSize  = FmhaPipeline::kBlockSize;
    static constexpr ck_tile::index_t kBlockPerCu = FmhaPipeline::kBlockPerCu;
    static_assert(kBlockPerCu > 0);
    static constexpr ck_tile::index_t kBlockPerCuInput = FmhaPipeline::Problem::kBlockPerCu;

    using QDataType = ck_tile::remove_cvref_t<typename FmhaPipeline::QDataType>;
    using KDataType = ck_tile::remove_cvref_t<typename FmhaPipeline::KDataType>;
    using VDataType = ck_tile::remove_cvref_t<typename FmhaPipeline::VDataType>;

    using VLayout = ck_tile::remove_cvref_t<typename FmhaPipeline::VLayout>;

    static constexpr bool kIsGroupMode = FmhaPipeline::kIsGroupMode;
    static constexpr bool kPadSeqLenQ  = FmhaPipeline::kPadSeqLenQ;
    static constexpr bool kPadSeqLenK  = FmhaPipeline::kPadSeqLenK;
    static constexpr bool kPadHeadDimQ = FmhaPipeline::kPadHeadDimQ;
    static constexpr bool kPadHeadDimV = FmhaPipeline::kPadHeadDimV;

    // clang-format off
    template <typename T> struct t2s;
    template <> struct t2s<float> { static constexpr const char * name = "fp32"; };
    template <> struct t2s<ck_tile::fp16_t> { static constexpr const char * name = "fp16"; };
    template <> struct t2s<ck_tile::bf16_t> { static constexpr const char * name = "bf16"; };
    template <> struct t2s<ck_tile::fp8_t> { static constexpr const char * name = "fp8"; };
    template <> struct t2s<ck_tile::bf8_t> { static constexpr const char * name = "bf8"; };
    // clang-format on

    __host__ static std::string GetName()
    {
        // sync with generate.py
        // clang-format off
        using bfs = typename FmhaPipeline::BlockFmhaShape;
        using gbr = typename bfs::Gemm0BlockWarps;
        using gwt = typename bfs::Gemm0WarpTile;
        #define _SS_  std::string
        #define _TS_  std::to_string
        auto pn = [&] () {
            std::string n;
            if (kPadSeqLenQ) n += "s";
            if (kPadSeqLenK) n += "sk";
            if (kPadHeadDimQ) n += "d";
            if (kPadHeadDimV) n += "dv";
            return n.empty() ? n : std::string("p") + n; }();
        return
            _SS_("fmha_fwd_appendkv_d") + _TS_(bfs::kK0BlockLength) + "_" + _SS_(t2s<QDataType>::name) +
            "_" + (kIsGroupMode ? "group" : "batch") + "_" + _SS_(TilePartitioner::name) + "_"
            "b" + _TS_(bfs::kM0) + "x" + _TS_(bfs::kN0) + "x" + _TS_(bfs::kK0) + "x" +
                    _TS_(bfs::kN1) + "x" + _TS_(bfs::kK1) + "x" + _TS_(bfs::kK0BlockLength) + "_" +
            "r" + _TS_(gbr::at(ck_tile::number<0>{})) + "x" + _TS_(gbr::at(ck_tile::number<1>{})) + "x" + _TS_(gbr::at(ck_tile::number<2>{})) + "_" +
            "w" + _TS_(gwt::at(ck_tile::number<0>{})) + "x" + _TS_(gwt::at(ck_tile::number<1>{})) + "x" + _TS_(gwt::at(ck_tile::number<2>{})) + "_" +
            (kBlockPerCuInput == -1 ? "" : ("o" + _TS_(kBlockPerCu) + "_")) + _SS_(FmhaPipeline::name) + "_" +
            "v" + (std::is_same_v<VLayout, ck_tile::tensor_layout::gemm::RowMajor> ? "r" : "c") + (pn.empty() ? "" : "_" + pn);
        #undef _SS_
        #undef _TS_
        // clang-format on
    }

    template <ck_tile::index_t I> // to avoid duplicated base class prblem, introduce an template
                                  // arg
    struct EmptyKargs
    {
    };

    // kargs use aggregate initializer, so no constructor will provided
    // use inheritance to minimize karg size
    // user need to use MakeKargs() function to create kargs.
    struct CommonKargs
    {
        const void* q_ptr;
        const void* k_ptr;
        const void* v_ptr;

        ck_tile::index_t seqlen_q;
        ck_tile::index_t seqlen_k;
        ck_tile::index_t hdim_q;
        ck_tile::index_t hdim_v;

        ck_tile::index_t num_head_q;
        // for MQA/GQA, nhead could be different. This parameter is nhead_q / nhead_k
        // if this param is larger than 1, indicate MQA/GQA case
        ck_tile::index_t nhead_ratio_qk;

        ck_tile::index_t stride_q;
        ck_tile::index_t stride_k;
        ck_tile::index_t stride_v;

        ck_tile::index_t nhead_stride_q;
        ck_tile::index_t nhead_stride_k;
        ck_tile::index_t nhead_stride_v;
    };

    struct BatchModeKargs : CommonKargs
    {
        ck_tile::index_t batch_stride_q;
        ck_tile::index_t batch_stride_k;
        ck_tile::index_t batch_stride_v;
    };

    struct GroupModeKargs : CommonKargs
    {
        const int32_t* seqstart_q_ptr;
        const int32_t* seqstart_k_ptr;
        const int32_t* seqlen_k_ptr;
    };

    using Kargs = std::conditional_t<kIsGroupMode, GroupModeKargs, BatchModeKargs>;

    template <bool Cond = !kIsGroupMode>
    __host__ static constexpr std::enable_if_t<Cond, Kargs>
    MakeKargs(const void* q_ptr,
              const void* k_ptr,
              const void* v_ptr,
              ck_tile::index_t seqlen_q,
              ck_tile::index_t seqlen_k,
              ck_tile::index_t hdim_q,
              ck_tile::index_t hdim_v,
              ck_tile::index_t num_head_q,
              ck_tile::index_t nhead_ratio_qk,
              ck_tile::index_t stride_q,
              ck_tile::index_t stride_k,
              ck_tile::index_t stride_v,
              ck_tile::index_t nhead_stride_q,
              ck_tile::index_t nhead_stride_k,
              ck_tile::index_t nhead_stride_v,
              ck_tile::index_t batch_stride_q,
              ck_tile::index_t batch_stride_k,
              ck_tile::index_t batch_stride_v)
    {
        Kargs kargs{{q_ptr,
                     k_ptr,
                     v_ptr,
                     seqlen_q,
                     seqlen_k,
                     hdim_q,
                     hdim_v,
                     num_head_q,
                     nhead_ratio_qk,
                     stride_q,
                     stride_k,
                     stride_v,
                     nhead_stride_q,
                     nhead_stride_k,
                     nhead_stride_v}, // args for common karg
                    batch_stride_q,
                    batch_stride_k,
                    batch_stride_v};

        return kargs;
    }

    template <bool Cond = kIsGroupMode>
    __host__ static constexpr std::enable_if_t<Cond, Kargs>
    MakeKargs(const void* q_ptr,
              const void* k_ptr,
              const void* v_ptr,
              const void* seqstart_q_ptr,
              const void* seqstart_k_ptr,
              const void* seqlen_k_ptr,
              ck_tile::index_t hdim_q,
              ck_tile::index_t hdim_v,
              ck_tile::index_t num_head_q,
              ck_tile::index_t nhead_ratio_qk,
              ck_tile::index_t stride_q,
              ck_tile::index_t stride_k,
              ck_tile::index_t stride_v,
              ck_tile::index_t nhead_stride_q,
              ck_tile::index_t nhead_stride_k,
              ck_tile::index_t nhead_stride_v)
    {
        Kargs kargs{{q_ptr,
                     k_ptr,
                     v_ptr,
                     -1, // seqlen will be updated by another pointer
                     -1, //
                     hdim_q,
                     hdim_v,
                     num_head_q,
                     nhead_ratio_qk,
                     stride_q,
                     stride_k,
                     stride_v,
                     nhead_stride_q,
                     nhead_stride_k,
                     nhead_stride_v}, // args for common karg
                    reinterpret_cast<const int32_t*>(seqstart_q_ptr),
                    reinterpret_cast<const int32_t*>(seqstart_k_ptr),
                    reinterpret_cast<const int32_t*>(seqlen_k_ptr)};

        return kargs;
    }

    __host__ static constexpr auto GridSize(ck_tile::index_t batch_size,
                                            ck_tile::index_t nhead,
                                            ck_tile::index_t seqlen_q,
                                            ck_tile::index_t hdim_v)
    {
        return TilePartitioner::GridSize(batch_size, nhead, seqlen_q, hdim_v);
    }

    __host__ static constexpr auto BlockSize() { return dim3(kBlockSize); }

    CK_TILE_HOST_DEVICE static constexpr ck_tile::index_t GetSmemSize()
    {
        return ck_tile::max(FmhaPipeline::GetSmemSize());
    }

    CK_TILE_DEVICE void operator()(Kargs kargs) const
    {
        __shared__ char smem_ptr[GetSmemSize()];

        // divide problem
        const auto [i_tile_m, i_tile_n, i_nhead, i_batch] =
            TilePartitioner{}(kargs.seqlen_q, kargs.hdim_v);

        const index_t i_m0 = __builtin_amdgcn_readfirstlane(i_tile_m * FmhaPipeline::kM0);
        const index_t i_n1 = __builtin_amdgcn_readfirstlane(i_tile_n * FmhaPipeline::kN1);

        long_index_t batch_offset_q = 0;
        long_index_t batch_offset_k = 0;
        long_index_t batch_offset_v = 0;

        if constexpr(kIsGroupMode)
        {
            // get starting offset for each batch
            const long_index_t query_start = kargs.seqstart_q_ptr[i_batch];
            const long_index_t key_start   = kargs.seqstart_k_ptr[i_batch];

            batch_offset_q = query_start * kargs.stride_q;
            batch_offset_k = key_start * kargs.stride_k;
            if constexpr(std::is_same_v<VLayout, ck_tile::tensor_layout::gemm::RowMajor>)
            {
                batch_offset_v = key_start * kargs.stride_v;
            }
            else
            {
                batch_offset_v = key_start;
            }

            // get real # queries & # keys under group mode
            const auto adjusted_seqstart_q_ptr = kargs.seqstart_q_ptr + i_batch;
            kargs.seqlen_q = adjusted_seqstart_q_ptr[1] - adjusted_seqstart_q_ptr[0];

            // # of required blocks is different in each groups, terminate unnecessary blocks
            // earlier
            if(kargs.seqlen_q <= i_m0)
            {
                return;
            }

            if(kargs.seqlen_k_ptr != nullptr)
            {
                kargs.seqlen_k = kargs.seqlen_k_ptr[i_batch];
            }
            else
            {
                const auto adjusted_seqstart_k_ptr = kargs.seqstart_k_ptr + i_batch;
                kargs.seqlen_k = adjusted_seqstart_k_ptr[1] - adjusted_seqstart_k_ptr[0];
            }
        }
        else
        {
            batch_offset_q = static_cast<long_index_t>(i_batch) * kargs.batch_stride_q;
            batch_offset_k = static_cast<long_index_t>(i_batch) * kargs.batch_stride_k;
            batch_offset_v = static_cast<long_index_t>(i_batch) * kargs.batch_stride_v;
        }

        // for simplicity, batch stride we just modify the pointer
        const QDataType* q_ptr = reinterpret_cast<const QDataType*>(kargs.q_ptr) +
                                 static_cast<long_index_t>(i_nhead) * kargs.nhead_stride_q +
                                 batch_offset_q;
        const KDataType* k_ptr =
            reinterpret_cast<const KDataType*>(kargs.k_ptr) +
            static_cast<long_index_t>(i_nhead / kargs.nhead_ratio_qk) * kargs.nhead_stride_k +
            batch_offset_k;
        const VDataType* v_ptr =
            reinterpret_cast<const VDataType*>(kargs.v_ptr) +
            static_cast<long_index_t>(i_nhead / kargs.nhead_ratio_qk) * kargs.nhead_stride_v +
            batch_offset_v;

        // Q/K/V DRAM and DRAM window
        const auto q_dram = [&]() {
            const auto q_dram_naive = make_naive_tensor_view<address_space_enum::global>(
                q_ptr,
                make_tuple(kargs.seqlen_q, kargs.hdim_q),
                make_tuple(kargs.stride_q, 1),
                number<FmhaPipeline::kAlignmentQ>{},
                number<1>{});

            return pad_tensor_view(
                q_dram_naive,
                make_tuple(number<FmhaPipeline::kM0>{}, number<FmhaPipeline::kK0>{}),
                sequence<kPadSeqLenQ, kPadHeadDimQ>{});
        }();
        const auto k_dram = [&]() {
            const auto k_dram_naive = make_naive_tensor_view<address_space_enum::global>(
                k_ptr,
                make_tuple(kargs.seqlen_k, kargs.hdim_q),
                make_tuple(kargs.stride_k, 1),
                number<FmhaPipeline::kAlignmentK>{},
                number<1>{});

            return pad_tensor_view(
                k_dram_naive,
                make_tuple(number<FmhaPipeline::kN0>{}, number<FmhaPipeline::kK0>{}),
                sequence<kPadSeqLenK, kPadHeadDimQ>{});
        }();
        const auto v_dram = [&]() {
            if constexpr(std::is_same_v<VLayout, ck_tile::tensor_layout::gemm::RowMajor>)
            {
                const auto v_dram_naive = make_naive_tensor_view<address_space_enum::global>(
                    v_ptr,
                    make_tuple(kargs.seqlen_k, kargs.hdim_v),
                    make_tuple(kargs.stride_v, 1),
                    number<FmhaPipeline::kAlignmentV>{},
                    number<1>{});

                const auto v_dram_transposed =
                    transform_tensor_view(v_dram_naive,
                                          make_tuple(make_pass_through_transform(kargs.hdim_v),
                                                     make_pass_through_transform(kargs.seqlen_k)),
                                          make_tuple(sequence<1>{}, sequence<0>{}),
                                          make_tuple(sequence<0>{}, sequence<1>{}));

                return pad_tensor_view(
                    v_dram_transposed,
                    make_tuple(number<FmhaPipeline::kN1>{}, number<FmhaPipeline::kK1>{}),
                    sequence<kPadHeadDimV, kPadSeqLenK>{});
            }
            else
            {
                const auto v_dram_naive = make_naive_tensor_view<address_space_enum::global>(
                    v_ptr,
                    make_tuple(kargs.hdim_v, kargs.seqlen_k),
                    make_tuple(kargs.stride_v, 1),
                    number<FmhaPipeline::kAlignmentV>{},
                    number<1>{});

                return pad_tensor_view(
                    v_dram_naive,
                    make_tuple(number<FmhaPipeline::kN1>{}, number<FmhaPipeline::kK1>{}),
                    sequence<kPadHeadDimV, kPadSeqLenK>{});
            }
        }();

        auto q_dram_window =
            make_tile_window(q_dram,
                             make_tuple(number<FmhaPipeline::kM0>{}, number<FmhaPipeline::kK0>{}),
                             {i_m0, 0});

        auto k_dram_window = make_tile_window(
            k_dram, make_tuple(number<FmhaPipeline::kN0>{}, number<FmhaPipeline::kK0>{}), {0, 0});

        auto v_dram_window =
            make_tile_window(v_dram,
                             make_tuple(number<FmhaPipeline::kN1>{}, number<FmhaPipeline::kK1>{}),
                             {i_n1, 0});

        FmhaPipeline{}(q_dram_window, k_dram_window, v_dram_window, smem_ptr);
    }
};

} // namespace ck_tile
