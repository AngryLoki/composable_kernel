// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2023, Advanced Micro Devices, Inc. All rights reserved.

#include <cstdlib>

#include "ck/ck.hpp"
#include "ck/tensor_operation/gpu/device/tensor_layout.hpp"
#include "ck/tensor_operation/gpu/device/gemm_specialization.hpp"
#include "ck/tensor_operation/gpu/device/impl/device_gemm_xdl_cshuffle_v3.hpp"

#include "ck/library/tensor_operation_instance/add_device_operation_instance.hpp"

namespace ck {
namespace tensor_operation {
namespace device {
namespace instance {

using F8  = ck::f8_t;
using F16 = ck::half_t;
using F32 = float;

using Row = ck::tensor_layout::gemm::RowMajor;
using Col = ck::tensor_layout::gemm::ColumnMajor;

template <ck::index_t... Is>
using S = ck::Sequence<Is...>;

using PassThrough = ck::tensor_operation::element_wise::PassThrough;

static constexpr auto GemmDefault = ck::tensor_operation::device::GemmSpecialization::Default;

template <ck::tensor_operation::device::GemmSpecialization GemmSpec>
using device_gemm_xdl_universal_f8_f8_f16_mk_nk_mn_instances = std::tuple<
    // clang-format off
        //#########################| ALayout| BLayout| CLayout|AData| BData| CData| AccData| Cshuffle|           A|           B|           C|          GEMM| Block|  MPer|  NPer|  KPer| AK1| BK1|MPer| NPer| MXdl| NXdl|  ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockLds|  BBlockTransfer| BBlockTransfer| BBlockTransfer| BlockTransfer| BBlockTransfer| BBlockTransfer| BBlockLds|    CShuffle|    CShuffle|     CBlockTransferClusterLengths|  CBlockTransfer|                         Block-wiseGemm|               Block-wiseGemm|
        //#########################|        |        |        | Type|  Type|  Type|    Type|     Type| Elementwise| Elementwise| Elementwise|Specialization|  Size| Block| Block| Block|    |    | XDL|  XDL|  Per|  Per|   ThreadCluster|  ThreadCluster| SrcAccessOrder|   SrcVectorDim|      SrcScalar|      DstScalar| AddExtraM|   ThreadCluster|  ThreadCluster| SrcAccessOrder|  SrcVectorDim|      SrcScalar|      DstScalar| AddExtraN| MXdlPerWave| NXdlPerWave| _MBlock_MXdlPerWave_MWaveMPerXdl| ScalarPerVector|                               Pipeline|                     Pipeline|
        //#########################|        |        |        |     |      |      |        |         |   Operation|   Operation|   Operation|              |      |      |      |      |    |    |    |     | Wave| Wave| Lengths_K0_M_K1|   ArrangeOrder|               |               |      PerVector|   PerVector_K1|          | Lengths_K0_N_K1|   ArrangeOrder|               |              |      PerVector|   PerVector_K1|          |  PerShuffle|  PerShuffle| _NBlock_NXdlPerWave_NWaveNPerXdl|   _NWaveNPerXdl|                              Scheduler|                     Verision|
        //#########################|        |        |        |     |      |      |        |         |            |            |            |              |      |      |      |      |    |    |    |     |     |     |                |               |               |               |               |               |          |                |               |               |              |               |               |          |            |            |                                 |                |                                       |                             |
        
        // Compute friendly
        DeviceGemm_Xdl_CShuffleV3<  Row,     Col,     Row,      F8,    F8,  F16,   F32,     F16,      PassThrough, PassThrough, PassThrough,       GemmSpec,   256,   256,   256,    64,  16,  16,  32,   32,    4,    4,     S<4, 64, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,             16,             16,          0,    S<4, 64, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,             16,             16,          0,          1,           1,                   S<1, 32, 1, 8>,               8,  BlockGemmPipelineScheduler::Intrawave, BlockGemmPipelineVersion::v4,    F8>,
        DeviceGemm_Xdl_CShuffleV3<  Row,     Col,     Row,      F8,    F8,  F16,   F32,     F16,      PassThrough, PassThrough, PassThrough,       GemmSpec,   256,   128,   128,    64,  16,  16,  32,   32,    2,    2,     S<4, 64, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,             16,             16,          0,    S<4, 64, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,             16,             16,          0,          1,           1,                   S<1, 32, 1, 8>,               8,  BlockGemmPipelineScheduler::Intrawave, BlockGemmPipelineVersion::v4,    F8>,
        DeviceGemm_Xdl_CShuffleV3<  Row,     Col,     Row,      F8,    F8,  F16,   F32,     F16,      PassThrough, PassThrough, PassThrough,       GemmSpec,   256,   128,   256,    64,  16,  16,  32,   32,    2,    4,     S<4, 64, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,             16,             16,          1,    S<4, 64, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,             16,             16,          1,          1,           1,                   S<1, 32, 1, 8>,               8,  BlockGemmPipelineScheduler::Interwave, BlockGemmPipelineVersion::v2,    F8>,
        DeviceGemm_Xdl_CShuffleV3<  Row,     Col,     Row,      F8,    F8,  F16,   F32,     F16,      PassThrough, PassThrough, PassThrough,       GemmSpec,   256,   128,   256,    64,  16,  16,  32,   32,    2,    4,     S<4, 64, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,             16,             16,          1,    S<4, 64, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,             16,             16,          1,          1,           1,                   S<1, 32, 1, 8>,               8,  BlockGemmPipelineScheduler::Interwave, BlockGemmPipelineVersion::v1,    F8>,
        DeviceGemm_Xdl_CShuffleV3<  Row,     Col,     Row,      F8,    F8,  F16,   F32,     F16,      PassThrough, PassThrough, PassThrough,       GemmSpec,   256,   256,   128,    64,  16,  16,  32,   32,    4,    2,     S<4, 64, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,             16,             16,          1,    S<4, 64, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,             16,             16,          1,          1,           1,                   S<1, 32, 1, 8>,               8,  BlockGemmPipelineScheduler::Interwave, BlockGemmPipelineVersion::v2,    F8>,
        DeviceGemm_Xdl_CShuffleV3<  Row,     Col,     Row,      F8,    F8,  F16,   F32,     F16,      PassThrough, PassThrough, PassThrough,       GemmSpec,   256,   256,   128,    64,  16,  16,  32,   32,    4,    2,     S<4, 64, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,             16,             16,          1,    S<4, 64, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,             16,             16,          1,          1,           1,                   S<1, 32, 1, 8>,               8,  BlockGemmPipelineScheduler::Interwave, BlockGemmPipelineVersion::v1,    F8>,
        DeviceGemm_Xdl_CShuffleV3<  Row,     Col,     Row,      F8,    F8,  F16,   F32,     F16,      PassThrough, PassThrough, PassThrough,       GemmSpec,   256,   128,   128,   128,  16,  16,  32,   32,    2,    2,     S<8, 32, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,             16,             16,          1,    S<8, 32, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,             16,             16,          1,          1,           1,                   S<1, 32, 1, 8>,               8,  BlockGemmPipelineScheduler::Interwave, BlockGemmPipelineVersion::v2,    F8>,
        DeviceGemm_Xdl_CShuffleV3<  Row,     Col,     Row,      F8,    F8,  F16,   F32,     F16,      PassThrough, PassThrough, PassThrough,       GemmSpec,   256,   128,   128,   128,  16,  16,  32,   32,    2,    2,     S<8, 32, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,             16,             16,          1,    S<8, 32, 1>,     S<1, 0, 2>,    S<1, 0, 2>,               2,             16,             16,          1,          1,           1,                   S<1, 32, 1, 8>,               8,  BlockGemmPipelineScheduler::Interwave, BlockGemmPipelineVersion::v1,    F8>
    // clang-format on
    >;

void add_device_gemm_xdl_universal_f8_f8_f16_mk_nk_mn_comp_default_instances(
    std::vector<std::unique_ptr<
        DeviceGemmV2<Row, Col, Row, F8, F8, F16, PassThrough, PassThrough, PassThrough>>>&
        instances)
{
    add_device_operation_instances(
        instances, device_gemm_xdl_universal_f8_f8_f16_mk_nk_mn_instances<GemmDefault>{});
}

} // namespace instance
} // namespace device
} // namespace tensor_operation
} // namespace ck
