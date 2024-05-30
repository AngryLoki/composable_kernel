// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2024, Advanced Micro Devices, Inc. All rights reserved.

#include "ck/library/tensor_operation_instance/add_device_operation_instance.hpp"
#include "ck/library/tensor_operation_instance/gpu/grouped_conv_bwd_data/device_grouped_conv_bwd_data_xdl_mem_instance.hpp"

namespace ck {
namespace tensor_operation {
namespace device {
namespace instance {
// Compilation parameters for out[n, di, hi, wi, g, c] * wei[g, k, z, y, x, c] = in[n, do, ho, wo,
// g, k]
void add_device_grouped_conv3d_bwd_data_xdl_ndhwgk_gkzyxc_ndhwgc_bf16_mem_intra_instances(
    std::vector<std::unique_ptr<DeviceGroupedConvBwdDataMultipleD<3,
                                                                  NDHWGK,
                                                                  GKZYXC,
                                                                  Empty_Tuple,
                                                                  NDHWGC,
                                                                  BF16,
                                                                  BF16,
                                                                  Empty_Tuple,
                                                                  BF16,
                                                                  PassThrough,
                                                                  PassThrough,
                                                                  PassThrough>>>& instances)
{
    // 1. Default
    add_device_operation_instances(
        instances,
        device_grouped_conv_bwd_data_xdl_bf16_mem_instances<3,
                                                            NDHWGK,
                                                            GKZYXC,
                                                            Empty_Tuple,
                                                            NDHWGC,
                                                            ConvBwdDataDefault,
                                                            Intrawave>{});
    // 2. Filter1x1Stride1Pad0
    add_device_operation_instances(
        instances,
        device_grouped_conv_bwd_data_xdl_bf16_mem_instances<3,
                                                            NDHWGK,
                                                            GKZYXC,
                                                            Empty_Tuple,
                                                            NDHWGC,
                                                            ConvBwdDataFilter1x1Stride1Pad0,
                                                            Intrawave>{});
}

} // namespace instance
} // namespace device
} // namespace tensor_operation
} // namespace ck
