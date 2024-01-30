// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2023, Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include <iostream>
#include <sstream>

#include "ck/utility/common_header.hpp"
#include "ck/tensor_description/tensor_descriptor.hpp"
#include "ck/tensor_description/tensor_descriptor_helper.hpp"
#include "ck/tensor_operation/gpu/device/tensor_layout.hpp"
#include "ck/tensor_operation/gpu/device/device_gemm_v2.hpp"
#include "ck/tensor_operation/gpu/device/gemm_specialization.hpp"
#include "ck/tensor_operation/gpu/grid/gridwise_gemm_xdl_cshuffle_v3.hpp"
#include "ck/host_utility/device_prop.hpp"
#include "ck/host_utility/kernel_launch.hpp"

namespace ck {
namespace tensor_operation {
namespace device {

// Note: inter-wave loop scheduler is rolled out to c-shuffle version first. Becuase non c-shuffle
// version currently has compiler issues with register spill which further causes validation
// failures.
template <typename ALayout,
          typename BLayout,
          typename CLayout,
          typename ADataType,
          typename BDataType,
          typename CDataType,
          typename GemmAccDataType,
          typename CShuffleDataType,
          typename AElementwiseOperation,
          typename BElementwiseOperation,
          typename CElementwiseOperation,
          GemmSpecialization GemmSpec,
          index_t NumGemmKPrefetchStage,
          index_t BlockSize,
          index_t MPerBlock,
          index_t NPerBlock,
          index_t KPerBlock,
          index_t AK1,
          index_t BK1,
          index_t MPerXDL,
          index_t NPerXDL,
          index_t MXdlPerWave,
          index_t NXdlPerWave,
          typename ABlockTransferThreadClusterLengths_AK0_M_AK1,
          typename ABlockTransferThreadClusterArrangeOrder,
          typename ABlockTransferSrcAccessOrder,
          index_t ABlockTransferSrcVectorDim,
          index_t ABlockTransferSrcScalarPerVector,
          index_t ABlockTransferDstScalarPerVector_AK1,
          bool ABlockLdsExtraM,
          typename BBlockTransferThreadClusterLengths_BK0_N_BK1,
          typename BBlockTransferThreadClusterArrangeOrder,
          typename BBlockTransferSrcAccessOrder,
          index_t BBlockTransferSrcVectorDim,
          index_t BBlockTransferSrcScalarPerVector,
          index_t BBlockTransferDstScalarPerVector_BK1,
          bool BBlockLdsExtraN,
          index_t CShuffleMXdlPerWavePerShuffle,
          index_t CShuffleNXdlPerWavePerShuffle,
          typename CShuffleBlockTransferClusterLengths_MBlock_MPerBlock_NBlock_NPerBlock,
          index_t CShuffleBlockTransferScalarPerVector_NPerBlock,
          BlockGemmPipelineScheduler BlkGemmPipeSched = BlockGemmPipelineScheduler::Intrawave,
          BlockGemmPipelineVersion BlkGemmPipelineVer = BlockGemmPipelineVersion::v4,
          typename ComputeTypeA                       = CDataType,
          typename ComputeTypeB                       = ComputeTypeA>
struct DeviceGemm_Xdl_CShuffleV3 : public DeviceGemmV2<ALayout,
                                                       BLayout,
                                                       CLayout,
                                                       ADataType,
                                                       BDataType,
                                                       CDataType,
                                                       AElementwiseOperation,
                                                       BElementwiseOperation,
                                                       CElementwiseOperation>
{
    // GridwiseGemm
    using GridwiseGemm = GridwiseGemm_xdl_cshuffle_v3<
        ALayout,
        BLayout,
        CLayout,
        ADataType,
        BDataType,
        GemmAccDataType,
        CShuffleDataType,
        CDataType,
        AElementwiseOperation,
        BElementwiseOperation,
        CElementwiseOperation,
        GemmSpec,
        NumGemmKPrefetchStage,
        BlockSize,
        MPerBlock,
        NPerBlock,
        KPerBlock,
        AK1,
        BK1,
        MPerXDL,
        NPerXDL,
        MXdlPerWave,
        NXdlPerWave,
        ABlockTransferThreadClusterLengths_AK0_M_AK1,
        ABlockTransferThreadClusterArrangeOrder,
        ABlockTransferSrcAccessOrder,
        ABlockTransferSrcVectorDim,
        ABlockTransferSrcScalarPerVector,
        ABlockTransferDstScalarPerVector_AK1,
        false,
        ABlockLdsExtraM,
        BBlockTransferThreadClusterLengths_BK0_N_BK1,
        BBlockTransferThreadClusterArrangeOrder,
        BBlockTransferSrcAccessOrder,
        BBlockTransferSrcVectorDim,
        BBlockTransferSrcScalarPerVector,
        BBlockTransferDstScalarPerVector_BK1,
        false,
        BBlockLdsExtraN,
        CShuffleMXdlPerWavePerShuffle,
        CShuffleNXdlPerWavePerShuffle,
        CShuffleBlockTransferClusterLengths_MBlock_MPerBlock_NBlock_NPerBlock,
        CShuffleBlockTransferScalarPerVector_NPerBlock,
        BlkGemmPipeSched,
        BlkGemmPipelineVer,
        ComputeTypeA,
        ComputeTypeB>;

    using Argument = typename GridwiseGemm::Argument;

    // Invoker
    struct Invoker : public BaseInvoker
    {
        float Run(const Argument& arg, const StreamConfig& stream_config = StreamConfig{})
        {
            if(stream_config.log_level_ > 0)
            {
                arg.Print();
            }

            if(!GridwiseGemm::CheckValidity(arg))
            {
                throw std::runtime_error("wrong! GridwiseGemm has invalid setting");
            }

            index_t gdx, gdy, gdz;
            std::tie(gdx, gdy, gdz) = GridwiseGemm::CalculateGridSize(arg.M, arg.N, arg.k_batch);

            float ave_time = 0;

            index_t K_split = 0;
            if constexpr(GemmSpec == GemmSpecialization::KPadding ||
                         GemmSpec == GemmSpecialization::MKPadding ||
                         GemmSpec == GemmSpecialization::NKPadding ||
                         GemmSpec == GemmSpecialization::MNKPadding)
            {
                index_t k_grain = arg.k_batch * KPerBlock;
                K_split         = (arg.K + k_grain - 1) / k_grain * KPerBlock;
            }
            else
            {
                K_split = arg.K / arg.k_batch;
            }

            // Tail number always 1
            if constexpr(BlkGemmPipelineVer == BlockGemmPipelineVersion::v1)
            {
                if(arg.k_batch > 1)
                {
                    hipGetErrorString(hipMemsetAsync(arg.p_c_grid,
                                                     0,
                                                     arg.M * arg.N * sizeof(CDataType),
                                                     stream_config.stream_id_));

                    const auto kernel =
                        kernel_gemm_xdl_cshuffle_v3<GridwiseGemm,
                                                    true,
                                                    InMemoryDataOperationEnum::AtomicAdd>;
                    ave_time = launch_and_time_kernel(
                        stream_config, kernel, dim3(gdx, gdy, gdz), dim3(BlockSize), 0, arg);
                }
                else
                {
                    const auto kernel = kernel_gemm_xdl_cshuffle_v3<GridwiseGemm,
                                                                    true,
                                                                    InMemoryDataOperationEnum::Set>;
                    ave_time          = launch_and_time_kernel(
                        stream_config, kernel, dim3(gdx, gdy, gdz), dim3(BlockSize), 0, arg);
                }
            }
            // Tail number always 2
            else if constexpr(BlkGemmPipelineVer == BlockGemmPipelineVersion::v2)
            {
                if(arg.k_batch > 1)
                {
                    hipGetErrorString(hipMemsetAsync(arg.p_c_grid,
                                                     0,
                                                     arg.M * arg.N * sizeof(CDataType),
                                                     stream_config.stream_id_));

                    const auto kernel =
                        kernel_gemm_xdl_cshuffle_v3<GridwiseGemm,
                                                    true,
                                                    InMemoryDataOperationEnum::AtomicAdd,
                                                    TailNumber::Even>;
                    ave_time = launch_and_time_kernel(
                        stream_config, kernel, dim3(gdx, gdy, gdz), dim3(BlockSize), 0, arg);
                }
                else
                {
                    const auto kernel = kernel_gemm_xdl_cshuffle_v3<GridwiseGemm,
                                                                    true,
                                                                    InMemoryDataOperationEnum::Set,
                                                                    TailNumber::Even>;
                    ave_time          = launch_and_time_kernel(
                        stream_config, kernel, dim3(gdx, gdy, gdz), dim3(BlockSize), 0, arg);
                }
            }
            // Tail number could be Odd or Even, double lds buffer have blockers on unified with
            // single buffer
            else if constexpr(BlkGemmPipelineVer == BlockGemmPipelineVersion::v4)
            {
                if(arg.k_batch > 1)
                {
                    hipGetErrorString(hipMemsetAsync(arg.p_c_grid,
                                                     0,
                                                     arg.M * arg.N * sizeof(CDataType),
                                                     stream_config.stream_id_));
                    if(GridwiseGemm::CalculateKBlockLoopTailNum(K_split) == TailNumber::Odd)
                    {
                        const auto kernel =
                            kernel_gemm_xdl_cshuffle_v3_2lds<GridwiseGemm,
                                                             true,
                                                             InMemoryDataOperationEnum::AtomicAdd>;
                        ave_time = launch_and_time_kernel(
                            stream_config, kernel, dim3(gdx, gdy, gdz), dim3(BlockSize), 0, arg);
                    }
                    else
                    {
                        const auto kernel =
                            kernel_gemm_xdl_cshuffle_v3_2lds<GridwiseGemm,
                                                             true,
                                                             InMemoryDataOperationEnum::AtomicAdd,
                                                             TailNumber::Even>;
                        ave_time = launch_and_time_kernel(
                            stream_config, kernel, dim3(gdx, gdy, gdz), dim3(BlockSize), 0, arg);
                    }
                }
                else
                {
                    if(GridwiseGemm::CalculateKBlockLoopTailNum(K_split) == TailNumber::Odd)
                    {
                        const auto kernel =
                            kernel_gemm_xdl_cshuffle_v3_2lds<GridwiseGemm,
                                                             true,
                                                             InMemoryDataOperationEnum::Set>;
                        ave_time = launch_and_time_kernel(
                            stream_config, kernel, dim3(gdx, gdy, gdz), dim3(BlockSize), 0, arg);
                    }
                    else
                    {
                        const auto kernel =
                            kernel_gemm_xdl_cshuffle_v3_2lds<GridwiseGemm,
                                                             true,
                                                             InMemoryDataOperationEnum::Set,
                                                             TailNumber::Even>;
                        ave_time = launch_and_time_kernel(
                            stream_config, kernel, dim3(gdx, gdy, gdz), dim3(BlockSize), 0, arg);
                    }
                }
            }
            // Tail number could be Odd or Even
            else
            {
                if(arg.k_batch > 1)
                {
                    hipGetErrorString(hipMemsetAsync(arg.p_c_grid,
                                                     0,
                                                     arg.M * arg.N * sizeof(CDataType),
                                                     stream_config.stream_id_));
                    if(GridwiseGemm::CalculateKBlockLoopTailNum(K_split) == TailNumber::Odd)
                    {
                        const auto kernel =
                            kernel_gemm_xdl_cshuffle_v3<GridwiseGemm,
                                                        true,
                                                        InMemoryDataOperationEnum::AtomicAdd>;
                        ave_time = launch_and_time_kernel(
                            stream_config, kernel, dim3(gdx, gdy, gdz), dim3(BlockSize), 0, arg);
                    }
                    else
                    {
                        const auto kernel =
                            kernel_gemm_xdl_cshuffle_v3<GridwiseGemm,
                                                        true,
                                                        InMemoryDataOperationEnum::AtomicAdd,
                                                        TailNumber::Even>;
                        ave_time = launch_and_time_kernel(
                            stream_config, kernel, dim3(gdx, gdy, gdz), dim3(BlockSize), 0, arg);
                    }
                }
                else
                {
                    if(GridwiseGemm::CalculateKBlockLoopTailNum(K_split) == TailNumber::Odd)
                    {
                        const auto kernel =
                            kernel_gemm_xdl_cshuffle_v3<GridwiseGemm,
                                                        true,
                                                        InMemoryDataOperationEnum::Set>;
                        ave_time = launch_and_time_kernel(
                            stream_config, kernel, dim3(gdx, gdy, gdz), dim3(BlockSize), 0, arg);
                    }
                    else
                    {
                        const auto kernel =
                            kernel_gemm_xdl_cshuffle_v3<GridwiseGemm,
                                                        true,
                                                        InMemoryDataOperationEnum::Set,
                                                        TailNumber::Even>;
                        ave_time = launch_and_time_kernel(
                            stream_config, kernel, dim3(gdx, gdy, gdz), dim3(BlockSize), 0, arg);
                    }
                }
            }

            return ave_time;
        }

        // polymorphic
        float Run(const BaseArgument* p_arg,
                  const StreamConfig& stream_config = StreamConfig{}) override
        {
            return Run(*dynamic_cast<const Argument*>(p_arg), stream_config);
        }
    };

    static constexpr bool IsValidCompilationParameter()
    {
        // TODO: properly implement this check
        return true;
    }

    static bool IsSupportedArgument(const Argument& arg)
    {
        if(!ck::is_xdl_supported())
        {
            return false;
        }

        if((arg.K % AK1 != 0 || arg.K % BK1 != 0) && !(GemmSpec == GemmSpecialization::MKPadding ||
                                                       GemmSpec == GemmSpecialization::NKPadding ||
                                                       GemmSpec == GemmSpecialization::MNKPadding ||
                                                       GemmSpec == GemmSpecialization::KPadding))
        {
            return false;
        }

        return GridwiseGemm::CheckValidity(arg);
    }

    // polymorphic
    bool IsSupportedArgument(const BaseArgument* p_arg) override
    {
        return IsSupportedArgument(*dynamic_cast<const Argument*>(p_arg));
    }

    static auto MakeArgument(const ADataType* p_a,
                             const BDataType* p_b,
                             CDataType* p_c,
                             index_t M,
                             index_t N,
                             index_t K,
                             index_t StrideA,
                             index_t StrideB,
                             index_t StrideC,
                             index_t KBatch,
                             AElementwiseOperation,
                             BElementwiseOperation,
                             CElementwiseOperation)
    {
        return Argument{p_a, p_b, p_c, M, N, K, StrideA, StrideB, StrideC, KBatch};
    }

    static auto MakeInvoker() { return Invoker{}; }

    // polymorphic
    std::unique_ptr<BaseArgument> MakeArgumentPointer(const void* p_a,
                                                      const void* p_b,
                                                      void* p_c,
                                                      index_t M,
                                                      index_t N,
                                                      index_t K,
                                                      index_t StrideA,
                                                      index_t StrideB,
                                                      index_t StrideC,
                                                      index_t KBatch,
                                                      AElementwiseOperation,
                                                      BElementwiseOperation,
                                                      CElementwiseOperation) override
    {
        return std::make_unique<Argument>(static_cast<const ADataType*>(p_a),
                                          static_cast<const BDataType*>(p_b),
                                          static_cast<CDataType*>(p_c),
                                          M,
                                          N,
                                          K,
                                          StrideA,
                                          StrideB,
                                          StrideC,
                                          KBatch);
    }

    // polymorphic
    std::unique_ptr<BaseInvoker> MakeInvokerPointer() override
    {
        return std::make_unique<Invoker>(Invoker{});
    }

    // polymorphic
    std::string GetTypeString() const override
    {
        auto str = std::stringstream();

        std::map<BlockGemmPipelineScheduler, std::string> BlkGemmPipelineSchedulerToString{
            {BlockGemmPipelineScheduler::Intrawave, "Intrawave"},
            {BlockGemmPipelineScheduler::Interwave, "Interwave"},
            {BlockGemmPipelineScheduler::Hybrid, "Hybrid"}};

        std::map<BlockGemmPipelineVersion, std::string> BlkGemmPipelineVersionToString{
            {BlockGemmPipelineVersion::v1, "v1"},
            {BlockGemmPipelineVersion::v2, "v2"},
            {BlockGemmPipelineVersion::v3, "v3"},
            {BlockGemmPipelineVersion::v4, "v4"}};

        // clang-format off
        str << "OpName: DeviceGemm_Xdl_CShuffleV3"
            << "<"
            << getGemmSpecializationString(GemmSpec) << ", "
            << std::string(ALayout::name)[0]
            << std::string(BLayout::name)[0]
            << std::string(CLayout::name)[0]
            << ">"
            << " BlkSize: "
            << BlockSize << ", "
            << "BlkTile: "
            << MPerBlock<<"x"<<NPerBlock<<"x"<<KPerBlock << ", "
            << "WaveTile: "
            << MPerXDL<<"x"<<NPerXDL << ", "
            << "WaveMap: "
            << MXdlPerWave<<"x" << NXdlPerWave<<", "
            << "VmemReadVec: "
            << ABlockTransferSrcScalarPerVector<<"x"<<BBlockTransferSrcScalarPerVector<<", "
            << "BlkGemmPipelineScheduler: "
            << BlkGemmPipelineSchedulerToString[BlkGemmPipeSched] << ", "
            << "BlkGemmPipelineVersion: "
            << BlkGemmPipelineVersionToString[BlkGemmPipelineVer];
        // clang-format on

        return str.str();
    }
};

} // namespace device
} // namespace tensor_operation
} // namespace ck
