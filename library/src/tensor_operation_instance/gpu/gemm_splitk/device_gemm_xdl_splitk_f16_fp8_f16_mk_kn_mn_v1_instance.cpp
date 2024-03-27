// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2023, Advanced Micro Devices, Inc. All rights reserved.

#include <cstdlib>

#include "ck/ck.hpp"
#include "ck/tensor_operation/gpu/device/tensor_layout.hpp"
#include "ck/tensor_operation/gpu/device/gemm_specialization.hpp"
#include "ck/tensor_operation/gpu/device/impl/device_gemm_xdl_splitk_c_shuffle.hpp"

#include "device_gemm_xdl_splitk_f16_fp8_f16_mk_kn_mn_common.hpp"

#include "ck/library/tensor_operation_instance/add_device_operation_instance.hpp"

namespace ck {
namespace tensor_operation {
namespace device {
namespace instance {

void add_device_gemm_xdl_splitk_f16_f8_f16_mk_kn_mn_v1_instances(
    std::vector<std::unique_ptr<
        DeviceGemmSplitK<Row, Row, Row, F16, F8, F16, PassThrough, PassThrough, PassThrough>>>&
        instances)
{
    add_device_operation_instances(instances,
                                   device_gemm_xdl_splitk_f16_f8_f16_mk_kn_mn_generic_instances{});

    add_device_operation_instances(
        instances,
        device_gemm_xdl_splitk_f16_f8_f16_mk_kn_mn_instances<GemmDefault,
                                                             PipelineVersion::v1,
                                                             LoopScheduler::Default>{});

    add_device_operation_instances(
        instances,
        device_gemm_xdl_splitk_f16_f8_f16_mk_kn_mn_instances<GemmMNPadding,
                                                             PipelineVersion::v1,
                                                             LoopScheduler::Default>{});

    add_device_operation_instances(
        instances,
        device_gemm_xdl_splitk_f16_f8_f16_mk_kn_mn_instances<GemmMNKPadding,
                                                             PipelineVersion::v1,
                                                             LoopScheduler::Default>{});
}

} // namespace instance
} // namespace device
} // namespace tensor_operation
} // namespace ck
