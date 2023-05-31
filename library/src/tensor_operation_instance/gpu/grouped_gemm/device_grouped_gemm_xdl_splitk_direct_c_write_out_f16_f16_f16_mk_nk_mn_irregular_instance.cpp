// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2023, Advanced Micro Devices, Inc. All rights reserved.

#include <cstdlib>

#include "ck/ck.hpp"
#include "ck/tensor_operation/gpu/device/tensor_layout.hpp"
#include "ck/tensor_operation/gpu/device/gemm_specialization.hpp"
#include "ck/tensor_operation/gpu/device/impl/device_grouped_gemm_xdl_splitk_direct_c_write_out.hpp"

#include "ck/library/tensor_operation_instance/add_device_operation_instance.hpp"

namespace ck {
namespace tensor_operation {
namespace device {
namespace instance {

using F16 = ck::half_t;
using F32 = float;

using Row = ck::tensor_layout::gemm::RowMajor;
using Col = ck::tensor_layout::gemm::ColumnMajor;

template <ck::index_t... Is>
using S = ck::Sequence<Is...>;

using Empty_Tuple = ck::Tuple<>;

using PassThrough                    = ck::tensor_operation::element_wise::PassThrough;
static constexpr auto GemmMNKPadding = ck::tensor_operation::device::GemmSpecialization::MNKPadding;

using device_grouped_gemm_irregular_tile_instances = std::tuple<
    // clang-format off
        //#######################################|      A|      B|          Ds|      E| AData| BData| AccData|      DsData| EData|           A|           B|           C|           GEMM| NumGemmK| Block|  MPer|  NPer|  KPer| AK1| BK1| MPer| NPer| MXdl| NXdl|  ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockLds|  BBlockTransfer| BBlockTransfer| BBlockTransfer| BlockTransfer| BBlockTransfer| BBlockTransfer| BBlockLds|
        //#######################################| Layout| Layout|      Layout| Layout|  Type|  Type|    Type|        Type|  Type| Elementwise| Elementwise| Elementwise| Spacialization| Prefetch|  Size| Block| Block| Block|    |    |  XDL|  XDL|  Per|  Per|   ThreadCluster|  ThreadCluster| SrcAccessOrder|   SrcVectorDim|      SrcScalar|      DstScalar| AddExtraM|   ThreadCluster|  ThreadCluster| SrcAccessOrder|  SrcVectorDim|      SrcScalar|      DstScalar| AddExtraN|
        //#######################################|       |       |            |       |      |      |        |            |      |   Operation|   Operation|   Operation|               |    Stage|      |      |      |      |    |    |     |     | Wave| Wave| Lengths_K0_M_K1|   ArrangeOrder|               |               |      PerVector|   PerVector_K1|          | Lengths_K0_N_K1|   ArrangeOrder|               |              |      PerVector|   PerVector_K1|          |
        //#######################################|       |       |            |       |      |      |        |            |      |            |            |            |               |         |      |      |      |      |    |    |     |     |     |     |                |               |               |               |               |               |          |                |               |               |              |               |               |          |
        DeviceGroupedGemmXdlSplitKDirectCWriteOut<    Row,    Col, Empty_Tuple,    Row,   F16,   F16,     F32, Empty_Tuple,   F16, PassThrough, PassThrough, PassThrough, GemmMNKPadding,        1,   256,   128,   256,    32,   8,   8,   32,   32,    2,    4,  S<1, 4, 64, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,              3,              8,              8,         1,  S<1, 4, 64, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,             3,              8,              8,         1>,
        DeviceGroupedGemmXdlSplitKDirectCWriteOut<    Row,    Col, Empty_Tuple,    Row,   F16,   F16,     F32, Empty_Tuple,   F16, PassThrough, PassThrough, PassThrough, GemmMNKPadding,        1,   256,   192,    64,    32,   8,   8,   32,   32,    3,    1,  S<1, 4, 64, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,              3,              8,              8,         1,  S<1, 4, 32, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,             3,              8,              8,         1>,
        DeviceGroupedGemmXdlSplitKDirectCWriteOut<    Row,    Col, Empty_Tuple,    Row,   F16,   F16,     F32, Empty_Tuple,   F16, PassThrough, PassThrough, PassThrough, GemmMNKPadding,        1,   256,    64,   192,    32,   8,   8,   32,   32,    1,    3,  S<1, 4, 64, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,              3,              8,              8,         1,  S<1, 4, 48, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,             3,              8,              8,         1>,
        DeviceGroupedGemmXdlSplitKDirectCWriteOut<    Row,    Col, Empty_Tuple,    Row,   F16,   F16,     F32, Empty_Tuple,   F16, PassThrough, PassThrough, PassThrough, GemmMNKPadding,        1,   256,   128,   128,    32,   8,   8,   32,   32,    2,    2,  S<1, 4, 64, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,              3,              8,              8,         1,  S<1, 4, 64, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,             3,              8,              8,         1>,
        DeviceGroupedGemmXdlSplitKDirectCWriteOut<    Row,    Col, Empty_Tuple,    Row,   F16,   F16,     F32, Empty_Tuple,   F16, PassThrough, PassThrough, PassThrough, GemmMNKPadding,        1,   256,   128,    64,    32,   8,   8,   32,   32,    2,    1,  S<1, 4, 64, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,              3,              8,              8,         1,  S<1, 4, 64, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,             3,              8,              8,         1>,
        DeviceGroupedGemmXdlSplitKDirectCWriteOut<    Row,    Col, Empty_Tuple,    Row,   F16,   F16,     F32, Empty_Tuple,   F16, PassThrough, PassThrough, PassThrough, GemmMNKPadding,        1,   256,    64,   128,    32,   8,   8,   32,   32,    1,    2,  S<1, 4, 64, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,              3,              8,              8,         1,  S<1, 4, 64, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,             3,              8,              8,         1>,
        DeviceGroupedGemmXdlSplitKDirectCWriteOut<    Row,    Col, Empty_Tuple,    Row,   F16,   F16,     F32, Empty_Tuple,   F16, PassThrough, PassThrough, PassThrough, GemmMNKPadding,        1,   128,   128,   128,    32,   8,   8,   32,   32,    4,    2,  S<1, 4, 32, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,              3,              8,              8,         1,  S<1, 4, 32, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,             3,              8,              8,         1>,
        DeviceGroupedGemmXdlSplitKDirectCWriteOut<    Row,    Col, Empty_Tuple,    Row,   F16,   F16,     F32, Empty_Tuple,   F16, PassThrough, PassThrough, PassThrough, GemmMNKPadding,        1,   128,   128,    64,    32,   8,   8,   32,   32,    2,    2,  S<1, 4, 32, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,              3,              8,              8,         1,  S<1, 4, 32, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,             3,              8,              8,         1>,    
        DeviceGroupedGemmXdlSplitKDirectCWriteOut<    Row,    Col, Empty_Tuple,    Row,   F16,   F16,     F32, Empty_Tuple,   F16, PassThrough, PassThrough, PassThrough, GemmMNKPadding,        1,   128,    64,   128,    32,   8,   8,   32,   32,    2,    2,  S<1, 4, 32, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,              3,              8,              8,         1,  S<1, 4, 32, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,             3,              8,              8,         1>,
        DeviceGroupedGemmXdlSplitKDirectCWriteOut<    Row,    Col, Empty_Tuple,    Row,   F16,   F16,     F32, Empty_Tuple,   F16, PassThrough, PassThrough, PassThrough, GemmMNKPadding,        1,   128,   192,    32,    32,   8,   8,   32,   32,    3,    1,  S<1, 4, 32, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,              3,              8,              8,         1,  S<1, 4, 32, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,             3,              8,              8,         1>,
        DeviceGroupedGemmXdlSplitKDirectCWriteOut<    Row,    Col, Empty_Tuple,    Row,   F16,   F16,     F32, Empty_Tuple,   F16, PassThrough, PassThrough, PassThrough, GemmMNKPadding,        1,   128,    32,   192,    32,   8,   8,   32,   32,    1,    3,  S<1, 4, 32, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,              3,              8,              8,         1,  S<1, 4, 32, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,             3,              8,              8,         1>,
        DeviceGroupedGemmXdlSplitKDirectCWriteOut<    Row,    Col, Empty_Tuple,    Row,   F16,   F16,     F32, Empty_Tuple,   F16, PassThrough, PassThrough, PassThrough, GemmMNKPadding,        1,   128,   128,    32,    32,   8,   8,   32,   32,    2,    1,  S<1, 4, 32, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,              3,              8,              8,         1,  S<1, 4, 32, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,             3,              8,              8,         1>,
        DeviceGroupedGemmXdlSplitKDirectCWriteOut<    Row,    Col, Empty_Tuple,    Row,   F16,   F16,     F32, Empty_Tuple,   F16, PassThrough, PassThrough, PassThrough, GemmMNKPadding,        1,   128,    32,   128,    32,   8,   8,   32,   32,    1,    2,  S<1, 4, 32, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,              3,              8,              8,         1,  S<1, 4, 32, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,             3,              8,              8,         1>,
        DeviceGroupedGemmXdlSplitKDirectCWriteOut<    Row,    Col, Empty_Tuple,    Row,   F16,   F16,     F32, Empty_Tuple,   F16, PassThrough, PassThrough, PassThrough, GemmMNKPadding,        1,   128,    32,   256,    32,   8,   8,   32,   32,    1,    4,  S<1, 4, 32, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,              3,              8,              8,         1,  S<1, 4, 32, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,             3,              8,              8,         1>,
        DeviceGroupedGemmXdlSplitKDirectCWriteOut<    Row,    Col, Empty_Tuple,    Row,   F16,   F16,     F32, Empty_Tuple,   F16, PassThrough, PassThrough, PassThrough, GemmMNKPadding,        1,   128,    32,    64,    32,   8,   8,   32,   32,    1,    1,  S<1, 4, 32, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,              3,              8,              8,         1,  S<1, 4, 32, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,             3,              8,              8,         1>,
        DeviceGroupedGemmXdlSplitKDirectCWriteOut<    Row,    Col, Empty_Tuple,    Row,   F16,   F16,     F32, Empty_Tuple,   F16, PassThrough, PassThrough, PassThrough, GemmMNKPadding,        1,   128,    64,    32,    32,   8,   8,   32,   32,    1,    1,  S<1, 4, 32, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,              3,              8,              8,         1,  S<1, 4, 32, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,             3,              8,              8,         1>,
        DeviceGroupedGemmXdlSplitKDirectCWriteOut<    Row,    Col, Empty_Tuple,    Row,   F16,   F16,     F32, Empty_Tuple,   F16, PassThrough, PassThrough, PassThrough, GemmMNKPadding,        1,   128,    64,    64,    32,   8,   8,   32,   32,    2,    1,  S<1, 4, 32, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,              3,              8,              8,         1,  S<1, 4, 32, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,             3,              8,              8,         1>,
        DeviceGroupedGemmXdlSplitKDirectCWriteOut<    Row,    Col, Empty_Tuple,    Row,   F16,   F16,     F32, Empty_Tuple,   F16, PassThrough, PassThrough, PassThrough, GemmMNKPadding,        1,    64,    64,    64,    32,   8,   8,   32,   32,    2,    2,  S<1, 4, 16, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,              3,              8,              8,         1,  S<1, 4, 16, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,             3,              8,              8,         1>,
        DeviceGroupedGemmXdlSplitKDirectCWriteOut<    Row,    Col, Empty_Tuple,    Row,   F16,   F16,     F32, Empty_Tuple,   F16, PassThrough, PassThrough, PassThrough, GemmMNKPadding,        1,    64,    64,    32,    32,   8,   8,   32,   32,    2,    1,  S<1, 4, 16, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,              3,              8,              8,         1,  S<1, 4, 16, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,             3,              8,              8,         1>,
        DeviceGroupedGemmXdlSplitKDirectCWriteOut<    Row,    Col, Empty_Tuple,    Row,   F16,   F16,     F32, Empty_Tuple,   F16, PassThrough, PassThrough, PassThrough, GemmMNKPadding,        1,    64,    32,    64,    32,   8,   8,   32,   32,    1,    2,  S<1, 4, 16, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,              3,              8,              8,         1,  S<1, 4, 16, 1>,  S<0, 2, 1, 3>,  S<0, 2, 1, 3>,             3,              8,              8,         1>
    // clang-format on
    >;

void add_device_grouped_gemm_xdl_splitk_direct_c_write_out_f16_f16_f16_mk_nk_mn_irregular_instances(
    std::vector<std::unique_ptr<DeviceGroupedGemm<Row,
                                                  Col,
                                                  Empty_Tuple,
                                                  Row,
                                                  F16,
                                                  F16,
                                                  Empty_Tuple,
                                                  F16,
                                                  PassThrough,
                                                  PassThrough,
                                                  PassThrough>>>& instances)
{
    add_device_operation_instances(instances, device_grouped_gemm_irregular_tile_instances{});
}

} // namespace instance
} // namespace device
} // namespace tensor_operation
} // namespace ck