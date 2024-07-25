// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2023, Advanced Micro Devices, Inc. All rights reserved.

#include "device_gemm_xdl_universal_bf16_bf16_bf16_mk_nk_mn.hpp"

namespace ck {
namespace tensor_operation {
namespace device {
namespace instance {

void add_device_gemm_xdl_universal_bf16_bf16_bf16_mk_nk_mn_comp_mnpadding_instances(
    std::vector<std::unique_ptr<DeviceGemmV2R1<Row,
                                               Col,
                                               DsLayout,
                                               Row,
                                               BF16,
                                               BF16,
                                               DsDataType,
                                               BF16,
                                               PassThrough,
                                               PassThrough,
                                               PassThrough>>>& instances)
{
    add_device_operation_instances(
        instances,
        device_gemm_xdl_universal_bf16_bf16_bf16_mk_nk_mn_comp_instances<GemmMNPadding>{});
}

} // namespace instance
} // namespace device
} // namespace tensor_operation
} // namespace ck
