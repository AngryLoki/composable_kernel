#include <unistd.h>
#include "device.hpp"
#include "tensor.hpp"
#include "gridwise_convolution_kernel_wrapper.hpp"
#include "gridwise_convolution_implicit_gemm_v4r4_xdlops_fp16_nchw_kcyx_nkhw.hpp"

template <class T,
          class InDesc,
          class WeiDesc,
          class OutDesc,
          class ConvStrides,
          class ConvDilations,
          class InLeftPads,
          class InRightPads>
void device_convolution_implicit_gemm_v4r4_xdlops_fp16_nchw_kcyx_nkhw(InDesc,
                                                                      const Tensor<T>& in_nchw,
                                                                      WeiDesc,
                                                                      const Tensor<T>& wei_kcyx,
                                                                      OutDesc,
                                                                      Tensor<T>& out_nkhw,
                                                                      ConvStrides,
                                                                      ConvDilations,
                                                                      InLeftPads,
                                                                      InRightPads,
                                                                      ck::index_t nrepeat)
{
    using namespace ck;

    constexpr auto I0 = Number<0>{};
    constexpr auto I1 = Number<1>{};
    constexpr auto I2 = Number<2>{};
    constexpr auto I3 = Number<3>{};

    constexpr auto in_nchw_desc =
        make_native_tensor_descriptor(InDesc::GetLengths(), InDesc::GetStrides());
    constexpr auto wei_kcyx_desc =
        make_native_tensor_descriptor(WeiDesc::GetLengths(), WeiDesc::GetStrides());
    constexpr auto out_nkhw_desc =
        make_native_tensor_descriptor(OutDesc::GetLengths(), OutDesc::GetStrides());

    constexpr index_t N  = out_nkhw_desc.GetLength(I0);
    constexpr index_t K  = out_nkhw_desc.GetLength(I1);
    constexpr index_t Ho = out_nkhw_desc.GetLength(I2);
    constexpr index_t Wo = out_nkhw_desc.GetLength(I3);

    std::size_t data_sz = sizeof(T);
    DeviceMem in_nchw_device_buf(data_sz * in_nchw.mDesc.GetElementSpace());
    DeviceMem wei_kcyx_device_buf(data_sz * wei_kcyx.mDesc.GetElementSpace());
    DeviceMem out_nkhw_device_buf(data_sz * out_nkhw.mDesc.GetElementSpace());

    in_nchw_device_buf.ToDevice(in_nchw.mData.data());
    wei_kcyx_device_buf.ToDevice(wei_kcyx.mData.data());
    out_nkhw_device_buf.ToDevice(out_nkhw.mData.data());

    // cdata = 64, BlockSize = 256, 128x128x16
    constexpr index_t BlockSize = 128;

    constexpr index_t GemmMPerBlock = 128;
    constexpr index_t GemmNPerBlock = 64;
    constexpr index_t GemmKPerBlock = 4;

    constexpr index_t GemmKPACK = 4;

    constexpr index_t GemmMPerWave = 64;
    constexpr index_t GemmNPerWave = 64;

    constexpr index_t ThreadGemmDataPerReadM = 1;
    constexpr index_t ThreadGemmDataPerReadN = 1;

    using GemmABlockCopyThreadSliceLengths_GemmK_GemmM_GemmKPACK   = Sequence<1, 4, 4>;
    using GemmABlockCopyThreadClusterLengths_GemmK_GemmM_GemmKPACK = Sequence<4, 32, 1>;

    constexpr index_t GemmABlockCopySrcDataPerRead_GemmKPACK  = 1;
    constexpr index_t GemmABlockCopyDstDataPerWrite_GemmKPACK = 1;

    using GemmBBlockCopyThreadSliceLengths_GemmK_GemmN_GemmKPACK   = Sequence<1, 2, 4>;
    using GemmBBlockCopyThreadClusterLengths_GemmK_GemmN_GemmKPACK = Sequence<4, 32, 1>;

    constexpr index_t GemmBBlockCopySrcDataPerRead_GemmN      = 1;
    constexpr index_t GemmBBlockCopyDstDataPerWrite_GemmKPACK = 1;

    constexpr index_t GemmM = K;
    constexpr index_t GemmN = N * Ho * Wo;

    constexpr index_t GridSize = math::integer_divide_ceil(GemmM, GemmMPerBlock) *
                                 math::integer_divide_ceil(GemmN, GemmNPerBlock);

    printf("%s: BlockSize %u, GridSize %u \n", __func__, BlockSize, GridSize);

    constexpr auto gridwise_conv =
        GridwiseConvolutionImplicitGemm_v4r4_xdlops_fwd_fp16_nchw_kcyx_nkhw<
            GridSize,
            BlockSize,
            half,
            float,
            decltype(in_nchw_desc),
            decltype(wei_kcyx_desc),
            decltype(out_nkhw_desc),
            ConvStrides,
            ConvDilations,
            InLeftPads,
            InRightPads,
            GemmMPerBlock,
            GemmNPerBlock,
            GemmKPerBlock,
            GemmKPACK,
            GemmMPerWave,
            GemmNPerWave,
            ThreadGemmDataPerReadM,
            ThreadGemmDataPerReadN,
            GemmABlockCopyThreadSliceLengths_GemmK_GemmM_GemmKPACK,
            GemmABlockCopyThreadClusterLengths_GemmK_GemmM_GemmKPACK,
            GemmABlockCopySrcDataPerRead_GemmKPACK,
            GemmABlockCopyDstDataPerWrite_GemmKPACK,
            GemmBBlockCopyThreadSliceLengths_GemmK_GemmN_GemmKPACK,
            GemmBBlockCopyThreadClusterLengths_GemmK_GemmN_GemmKPACK,
            GemmBBlockCopySrcDataPerRead_GemmN,
            GemmBBlockCopyDstDataPerWrite_GemmKPACK>{};

    for(index_t i = 0; i < 10; ++i)
    {
        float time =
            launch_and_time_kernel(run_gridwise_convolution_kernel<decltype(gridwise_conv), T>,
                                   dim3(GridSize),
                                   dim3(BlockSize),
                                   0,
                                   0,
                                   static_cast<T*>(in_nchw_device_buf.GetDeviceBuffer()),
                                   static_cast<T*>(wei_kcyx_device_buf.GetDeviceBuffer()),
                                   static_cast<T*>(out_nkhw_device_buf.GetDeviceBuffer()));

        printf("Elapsed time : %f ms, %f TFlop/s\n",
               time,
               (float)calculate_convolution_flops(InDesc{}, WeiDesc{}, OutDesc{}) /
                   (std::size_t(1000) * 1000 * 1000) / time);
    }

    // warm up
    printf("Warn up running %d times...\n", nrepeat);

    for(index_t i = 0; i < nrepeat; ++i)
    {
        launch_kernel(run_gridwise_convolution_kernel<decltype(gridwise_conv), T>,
                      dim3(GridSize),
                      dim3(BlockSize),
                      0,
                      0,
                      static_cast<T*>(in_nchw_device_buf.GetDeviceBuffer()),
                      static_cast<T*>(wei_kcyx_device_buf.GetDeviceBuffer()),
                      static_cast<T*>(out_nkhw_device_buf.GetDeviceBuffer()));
    }

    printf("Start running %d times...\n", nrepeat);

    cudaDeviceSynchronize();
    auto start = std::chrono::steady_clock::now();

    for(index_t i = 0; i < nrepeat; ++i)
    {
        launch_kernel(run_gridwise_convolution_kernel<decltype(gridwise_conv), T>,
                      dim3(GridSize),
                      dim3(BlockSize),
                      0,
                      0,
                      static_cast<T*>(in_nchw_device_buf.GetDeviceBuffer()),
                      static_cast<T*>(wei_kcyx_device_buf.GetDeviceBuffer()),
                      static_cast<T*>(out_nkhw_device_buf.GetDeviceBuffer()));
    }

    cudaDeviceSynchronize();
    auto end = std::chrono::steady_clock::now();

    float ave_time = std::chrono::duration<float, std::milli>(end - start).count() / nrepeat;

    printf("Average elapsed time : %f ms, %f TFlop/s\n",
           ave_time,
           (float)calculate_convolution_flops(InDesc{}, WeiDesc{}, OutDesc{}) /
               (std::size_t(1000) * 1000 * 1000) / ave_time);

    out_nkhw_device_buf.FromDevice(out_nkhw.mData.data());
}
