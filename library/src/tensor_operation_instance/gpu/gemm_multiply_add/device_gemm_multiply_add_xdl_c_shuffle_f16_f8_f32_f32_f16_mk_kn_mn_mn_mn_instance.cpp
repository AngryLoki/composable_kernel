// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2023, Advanced Micro Devices, Inc. All rights reserved.

#include <cstdlib>

#include "ck/ck.hpp"
#include "ck/tensor_operation/gpu/device/tensor_layout.hpp"
#include "ck/tensor_operation/gpu/device/gemm_specialization.hpp"
#include "ck/tensor_operation/gpu/device/impl/device_gemm_multiple_d_xdl_cshuffle.hpp"
#include "ck/tensor_operation/gpu/element/element_wise_operation.hpp"

#include "ck/library/tensor_operation_instance/add_device_operation_instance.hpp"

namespace ck {
namespace tensor_operation {
namespace device {
namespace instance {

using F8        = ck::f8_t;
using F16       = ck::half_t;
using F32       = float;
using F32_Tuple = ck::Tuple<F32, F32>;

using Row       = ck::tensor_layout::gemm::RowMajor;
using Col       = ck::tensor_layout::gemm::ColumnMajor;
using Row_Tuple = ck::Tuple<Row, Row>;

template <ck::index_t... Is>
using S = ck::Sequence<Is...>;

using PassThrough = ck::tensor_operation::element_wise::PassThrough;
using MultiplyAdd = ck::tensor_operation::element_wise::MultiplyAdd;

static constexpr auto GemmMNKPadding = ck::tensor_operation::device::GemmSpecialization::MNKPadding;

using device_gemm_multiply_add_xdl_c_shuffle_f16_f8_f32_f32_f16_mk_kn_mn_mn_mn_instances =
    std::tuple<
        // clang-format off
        // M/N/K padding
        //##############################|      A|      B|        Ds|      E| AData| BData| AccData| CShuffle|    DsData| EData|           A|           B|            CDE|           GEMM| NumGemmK| Block|  MPer|  NPer|  KPer| AK1| BK1| MPer| NPer| MXdl| NXdl|  ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockLds|  BBlockTransfer| BBlockTransfer| BBlockTransfer| BlockTransfer| BBlockTransfer| BBlockTransfer| BBlockLds|    CShuffle|    CShuffle| CBlockTransferClusterLengths|  CBlockTransfer|
        //##############################| Layout| Layout|    Layout| Layout|  Type|  Type|    Type| DataType|      Type|  Type| Elementwise| Elementwise|    Elementwise| Specialization| Prefetch|  Size| Block| Block| Block|    |    |  XDL|  XDL|  Per|  Per|   ThreadCluster|  ThreadCluster| SrcAccessOrder|   SrcVectorDim|      SrcScalar|      DstScalar| AddExtraM|   ThreadCluster|  ThreadCluster| SrcAccessOrder|  SrcVectorDim|      SrcScalar|      DstScalar| AddExtraN| MXdlPerWave| NXdlPerWave|         _MBlock_MWaveMPerXdl| ScalarPerVector|
        //##############################|       |       |          |       |      |      |        |         |          |      |   Operation|   Operation|      Operation|               |    Stage|      |      |      |      |    |    |     |     | Wave| Wave| Lengths_K0_M_K1|   ArrangeOrder|               |               |      PerVector|   PerVector_K1|          | Lengths_K0_N_K1|   ArrangeOrder|               |              |      PerVector|   PerVector_K1|          |  PerShuffle|  PerShuffle|         _NBlock_NWaveNPerXdl|   _NWaveNPerXdl|
        //##############################|       |       |          |       |      |      |        |         |          |      |            |            |               |               |         |      |      |      |      |    |    |     |     |     |     |                |               |               |               |               |               |          |                |               |               |              |               |               |          |            |            |                             |                |
        DeviceGemmMultipleD_Xdl_CShuffle<    Row,    Row, Row_Tuple,    Row,   F16,    F8,     F32,      F32, F32_Tuple,   F16, PassThrough, PassThrough,    MultiplyAdd, GemmMNKPadding,        1,   256,   256,   128,    32,   8,   2,   32,   32,    4,    2,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<8, 32, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              4,              2,         0,           1,           1,               S<1, 32, 1, 8>,               8>,
        DeviceGemmMultipleD_Xdl_CShuffle<    Row,    Row, Row_Tuple,    Row,   F16,    F8,     F32,      F32, F32_Tuple,   F16, PassThrough, PassThrough,    MultiplyAdd, GemmMNKPadding,        1,   256,   256,   128,    32,   8,   8,   32,   32,    4,    2,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<4, 64, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              2,              8,         1,           1,           1,               S<1, 32, 1, 8>,               8>,
        DeviceGemmMultipleD_Xdl_CShuffle<    Row,    Row, Row_Tuple,    Row,   F16,    F8,     F32,      F32, F32_Tuple,   F16, PassThrough, PassThrough,    MultiplyAdd, GemmMNKPadding,        1,   256,   128,   256,    32,   8,   2,   32,   32,    2,    4,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<4, 64, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              4,              2,         0,           1,           1,               S<1, 32, 1, 8>,               8>,
        DeviceGemmMultipleD_Xdl_CShuffle<    Row,    Row, Row_Tuple,    Row,   F16,    F8,     F32,      F32, F32_Tuple,   F16, PassThrough, PassThrough,    MultiplyAdd, GemmMNKPadding,        1,   256,   128,   256,    32,   8,   8,   32,   32,    2,    4,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<4, 64, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              4,              8,         1,           1,           1,               S<1, 32, 1, 8>,               8>,
        DeviceGemmMultipleD_Xdl_CShuffle<    Row,    Row, Row_Tuple,    Row,   F16,    F8,     F32,      F32, F32_Tuple,   F16, PassThrough, PassThrough,    MultiplyAdd, GemmMNKPadding,        1,   128,   128,   128,    32,   8,   2,   32,   32,    4,    2,     S<4, 32, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<4, 32, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              4,              2,         0,           1,           1,               S<1, 16, 1, 8>,               8>,
        DeviceGemmMultipleD_Xdl_CShuffle<    Row,    Row, Row_Tuple,    Row,   F16,    F8,     F32,      F32, F32_Tuple,   F16, PassThrough, PassThrough,    MultiplyAdd, GemmMNKPadding,        1,   128,   128,   128,    32,   8,   8,   32,   32,    4,    2,     S<4, 32, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<4, 32, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              4,              8,         1,           1,           1,               S<1, 16, 1, 8>,               8>,
        DeviceGemmMultipleD_Xdl_CShuffle<    Row,    Row, Row_Tuple,    Row,   F16,    F8,     F32,      F32, F32_Tuple,   F16, PassThrough, PassThrough,    MultiplyAdd, GemmMNKPadding,        1,   256,   128,   128,    32,   8,   2,   32,   32,    2,    2,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<8, 32, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              4,              2,         0,           1,           1,               S<1, 32, 1, 8>,               8>,
        DeviceGemmMultipleD_Xdl_CShuffle<    Row,    Row, Row_Tuple,    Row,   F16,    F8,     F32,      F32, F32_Tuple,   F16, PassThrough, PassThrough,    MultiplyAdd, GemmMNKPadding,        1,   256,   128,   128,    32,   8,   8,   32,   32,    2,    2,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<4, 64, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              2,              8,         1,           1,           1,               S<1, 32, 1, 8>,               8>,
        DeviceGemmMultipleD_Xdl_CShuffle<    Row,    Row, Row_Tuple,    Row,   F16,    F8,     F32,      F32, F32_Tuple,   F16, PassThrough, PassThrough,    MultiplyAdd, GemmMNKPadding,        1,   128,   128,    64,    32,   8,   2,   32,   32,    2,    2,     S<4, 32, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<8, 16, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              4,              2,         0,           1,           1,               S<1, 32, 1, 4>,               8>,
        DeviceGemmMultipleD_Xdl_CShuffle<    Row,    Row, Row_Tuple,    Row,   F16,    F8,     F32,      F32, F32_Tuple,   F16, PassThrough, PassThrough,    MultiplyAdd, GemmMNKPadding,        1,   128,   128,    64,    32,   8,   8,   32,   32,    2,    2,     S<4, 32, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<4, 32, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              2,              8,         1,           1,           1,               S<1, 32, 1, 4>,               8>,
        DeviceGemmMultipleD_Xdl_CShuffle<    Row,    Row, Row_Tuple,    Row,   F16,    F8,     F32,      F32, F32_Tuple,   F16, PassThrough, PassThrough,    MultiplyAdd, GemmMNKPadding,        1,   128,    64,   128,    32,   8,   2,   32,   32,    2,    2,     S<4, 32, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<4, 32, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              4,              2,         0,           1,           1,               S<1, 16, 1, 8>,               8>,
        DeviceGemmMultipleD_Xdl_CShuffle<    Row,    Row, Row_Tuple,    Row,   F16,    F8,     F32,      F32, F32_Tuple,   F16, PassThrough, PassThrough,    MultiplyAdd, GemmMNKPadding,        1,   128,    64,   128,    32,   8,   8,   32,   32,    2,    2,     S<4, 32, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<4, 32, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              4,              8,         1,           1,           1,               S<1, 16, 1, 8>,               8>,
        DeviceGemmMultipleD_Xdl_CShuffle<    Row,    Row, Row_Tuple,    Row,   F16,    F8,     F32,      F32, F32_Tuple,   F16, PassThrough, PassThrough,    MultiplyAdd, GemmMNKPadding,        1,   256,   128,    64,    32,   8,   2,   32,   32,    2,    1,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<16,16, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              4,              2,         0,           1,           1,               S<1, 32, 1, 8>,               8>,
        DeviceGemmMultipleD_Xdl_CShuffle<    Row,    Row, Row_Tuple,    Row,   F16,    F8,     F32,      F32, F32_Tuple,   F16, PassThrough, PassThrough,    MultiplyAdd, GemmMNKPadding,        1,   256,   128,    64,    32,   8,   8,   32,   32,    2,    1,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<4, 64, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              1,              8,         1,           1,           1,               S<1, 32, 1, 8>,               8>,
        DeviceGemmMultipleD_Xdl_CShuffle<    Row,    Row, Row_Tuple,    Row,   F16,    F8,     F32,      F32, F32_Tuple,   F16, PassThrough, PassThrough,    MultiplyAdd, GemmMNKPadding,        1,   256,    64,   128,    32,   8,   2,   32,   32,    1,    2,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<8, 32, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              4,              2,         0,           1,           1,               S<1, 32, 1, 8>,               8>,
        DeviceGemmMultipleD_Xdl_CShuffle<    Row,    Row, Row_Tuple,    Row,   F16,    F8,     F32,      F32, F32_Tuple,   F16, PassThrough, PassThrough,    MultiplyAdd, GemmMNKPadding,        1,   256,    64,   128,    32,   8,   8,   32,   32,    1,    2,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<4, 64, 1>,     S<0, 2, 1>,     S<0, 2, 1>,             1,              2,              8,         1,           1,           1,               S<1, 32, 1, 8>,               8>
        // clang-format on
        >;

void add_device_gemm_multiply_add_xdl_c_shuffle_f16_f8_f32_f32_f16_mk_kn_mn_mn_mn_instances(
    std::vector<std::unique_ptr<DeviceGemmMultipleD<Row,
                                                    Row,
                                                    Row_Tuple,
                                                    Row,
                                                    F16,
                                                    F8,
                                                    F32_Tuple,
                                                    F16,
                                                    PassThrough,
                                                    PassThrough,
                                                    MultiplyAdd>>>& instances)
{
    add_device_operation_instances(
        instances,
        device_gemm_multiply_add_xdl_c_shuffle_f16_f8_f32_f32_f16_mk_kn_mn_mn_mn_instances{});
}

} // namespace instance
} // namespace device
} // namespace tensor_operation
} // namespace ck
