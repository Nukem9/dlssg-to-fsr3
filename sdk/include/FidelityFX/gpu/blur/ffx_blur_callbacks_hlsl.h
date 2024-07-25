// This file is part of the FidelityFX SDK.
//
// Copyright (C) 2024 Advanced Micro Devices, Inc.
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "ffx_blur_resources.h"

#if defined(FFX_GPU)

#ifdef __hlsl_dx_compiler
#pragma dxc diagnostic push
#pragma dxc diagnostic ignored "-Wambig-lit-shift"
#endif //__hlsl_dx_compiler

#include "ffx_core.h"

#ifdef __hlsl_dx_compiler
#pragma dxc diagnostic pop
#endif //__hlsl_dx_compiler

#ifndef FFX_PREFER_WAVE64
#define FFX_PREFER_WAVE64
#endif // #ifndef FFX_PREFER_WAVE64

#if defined(FFX_GPU)
#pragma warning(disable: 3205)  // conversion from larger type to smaller
#endif // #if defined(FFX_GPU)

#define DECLARE_SRV_REGISTER(regIndex)  t##regIndex
#define DECLARE_UAV_REGISTER(regIndex)  u##regIndex
#define DECLARE_CB_REGISTER(regIndex)   b##regIndex
#define FFX_BLUR_DECLARE_SRV(regIndex)  register(DECLARE_SRV_REGISTER(regIndex))
#define FFX_BLUR_DECLARE_UAV(regIndex)  register(DECLARE_UAV_REGISTER(regIndex))
#define FFX_BLUR_DECLARE_CB(regIndex)   register(DECLARE_CB_REGISTER(regIndex))

#if defined(FFX_BLUR_BIND_CB_BLUR)
    cbuffer cbBLUR : FFX_BLUR_DECLARE_CB(FFX_BLUR_BIND_CB_BLUR)
    {
        FfxInt32x2 imageSize;
        #define FFX_BLUR_CONSTANT_BUFFER_1_SIZE 2  // Number of 32-bit values. This must be kept in sync with the cbBLUR size.
    };
#else
    #define imageSize 0
#endif

#define FFX_BLUR_ROOTSIG_STRINGIFY(p) FFX_BLUR_ROOTSIG_STR(p)
#define FFX_BLUR_ROOTSIG_STR(p) #p
#define FFX_BLUR_ROOTSIG [RootSignature("DescriptorTable(UAV(u0, numDescriptors = " FFX_BLUR_ROOTSIG_STRINGIFY(FFX_BLUR_RESOURCE_IDENTIFIER_COUNT) ")), " \
                                        "DescriptorTable(SRV(t0, numDescriptors = " FFX_BLUR_ROOTSIG_STRINGIFY(FFX_BLUR_RESOURCE_IDENTIFIER_COUNT) ")), " \
                                        "CBV(b0)" )]

#if defined(FFX_BLUR_EMBED_ROOTSIG)
#define FFX_BLUR_EMBED_ROOTSIG_CONTENT FFX_BLUR_ROOTSIG
#else
#define FFX_BLUR_EMBED_ROOTSIG_CONTENT
#endif // #if FFX_BLUR_EMBED_ROOTSIG

FfxInt32x2 ImageSize()
{
    return imageSize;
}

// SRVs
#if defined FFX_BLUR_BIND_SRV_INPUT_SRC
    #if FFX_HALF
        Texture2D<FfxFloat16x4> r_input_src : FFX_BLUR_DECLARE_SRV(FFX_BLUR_BIND_SRV_INPUT_SRC);
    #else
        Texture2D<FfxFloat32x4> r_input_src : FFX_BLUR_DECLARE_SRV(FFX_BLUR_BIND_SRV_INPUT_SRC);
    #endif // FFX_HALF
#endif

    // UAV declarations
#if defined FFX_BLUR_BIND_UAV_OUTPUT
    #if FFX_HALF
        RWTexture2D<FfxFloat16x4> rw_output : FFX_BLUR_DECLARE_UAV(FFX_BLUR_BIND_UAV_OUTPUT);
    #else
        RWTexture2D<FfxFloat32x4> rw_output : FFX_BLUR_DECLARE_UAV(FFX_BLUR_BIND_UAV_OUTPUT);
    #endif // FFX_HALF
#endif

// FFX_BLUR_OPTION_KERNEL_DIMENSION to be defined by the client application
// App should define e.g the following for 5x5 blur:
// #define FFX_BLUR_OPTION_KERNEL_DIMENSION 5
#ifndef FFX_BLUR_OPTION_KERNEL_DIMENSION
#error Please define FFX_BLUR_OPTION_KERNEL_DIMENSION
#endif

// FFX_BLUR_KERNEL_RANGE is center + half width of the kernel
//
// consider a blur kernel 5x5 - '*' indicates the center of the kernel
// FFX_BLUR_OPTION_KERNEL_DIMENSION=5
// |---------------|
// x   x   x   x   x
// x   x   x   x   x
// x   x   x*  x   x
// x   x   x   x   x
// x   x   x   x   x
//
//
// as separate 1D kernels
//
// x   x   x*  x   x
//         |-------|
//        FFX_BLUR_KERNEL_RANGE
//
#define FFX_BLUR_KERNEL_RANGE        (((FFX_BLUR_OPTION_KERNEL_DIMENSION - 1) / 2) + 1)
#define FFX_BLUR_KERNEL_RANGE_MINUS1 (FFX_BLUR_KERNEL_RANGE - 1)

//
// FFX-Blur Callback definitions
//
#if FFX_HALF
    #define FFX_BLUR_KERNEL_TYPE FfxFloat16
#else
    #define FFX_BLUR_KERNEL_TYPE FfxFloat32
#endif

inline FFX_BLUR_KERNEL_TYPE FfxBlurLoadKernelWeight(FfxInt32 iKernelIndex)
{
// GAUSSIAN BLUR 1D KERNELS
//
//----------------------------------------------------------------------------------------------------------------------------------
// Kernel Size: [3, 21]: odd numbers
// Kernels are pregenerated using three different sigma values.
// Larger sigmas are better for larger kernels.
    const FFX_BLUR_KERNEL_TYPE kernel_weights[FFX_BLUR_KERNEL_RANGE] =
#if FFX_BLUR_OPTION_KERNEL_PERMUTATION == 0
// Sigma: 1.6
#if FFX_BLUR_KERNEL_RANGE == 2
    { 0.3765770884, 0.3117114558 };
#elif FFX_BLUR_KERNEL_RANGE == 3
    { 0.2782163289, 0.230293397, 0.1305984385 };
#elif FFX_BLUR_KERNEL_RANGE == 4
    { 0.2525903052, 0.2090814714, 0.1185692428, 0.0460541333 };
#elif FFX_BLUR_KERNEL_RANGE == 5
    { 0.2465514351, 0.2040828004, 0.115734517, 0.0449530818, 0.0119538834 };
#elif FFX_BLUR_KERNEL_RANGE == 6
    { 0.245483563, 0.2031988699, 0.1152332436, 0.0447583794, 0.0119021083, 0.0021656173 };
#elif FFX_BLUR_KERNEL_RANGE == 7
    { 0.2453513488, 0.2030894296, 0.1151711805, 0.0447342732, 0.011895698, 0.0021644509, 0.0002692935 };
#elif FFX_BLUR_KERNEL_RANGE == 8
    { 0.2453401155, 0.2030801313, 0.1151659074, 0.044732225, 0.0118951533, 0.0021643518, 0.0002692811, 2.28922E-05 };
#elif FFX_BLUR_KERNEL_RANGE == 9
    { 0.2453394635, 0.2030795916, 0.1151656014, 0.0447321061, 0.0118951217, 0.0021643461, 0.0002692804, 2.28922E-05, 1.3287E-06 };
#elif FFX_BLUR_KERNEL_RANGE == 10
    { 0.2453394377, 0.2030795703, 0.1151655892, 0.0447321014, 0.0118951205, 0.0021643458, 0.0002692804, 2.28922E-05, 1.3287E-06, 5.26E-08 };
#elif FFX_BLUR_KERNEL_RANGE == 11
    { 0.2453394371, 0.2030795697, 0.1151655889, 0.0447321013, 0.0118951204, 0.0021643458, 0.0002692804, 2.28922E-05, 1.3287E-06, 5.26E-08, 1.4E-09 };
#endif
#elif FFX_BLUR_OPTION_KERNEL_PERMUTATION == 1
// Sigma: 2.8
#if FFX_BLUR_KERNEL_RANGE == 2
    { 0.3474999743, 0.3262500129 };
#elif FFX_BLUR_KERNEL_RANGE == 3
    { 0.2256541468, 0.2118551763, 0.1753177504 };
#elif FFX_BLUR_KERNEL_RANGE == 4
    { 0.1796953063, 0.1687067636, 0.1396108926, 0.1018346906 };
#elif FFX_BLUR_KERNEL_RANGE == 5
    { 0.1588894947, 0.1491732476, 0.1234462081, 0.0900438796, 0.0578919173 };
#elif FFX_BLUR_KERNEL_RANGE == 6
    { 0.1491060676, 0.1399880866, 0.1158451582, 0.0844995374, 0.054327293, 0.0307868909 };
#elif FFX_BLUR_KERNEL_RANGE == 7
    { 0.1446570603, 0.1358111404, 0.1123885856, 0.0819782513, 0.0527062824, 0.0298682757, 0.0149189344 };
#elif FFX_BLUR_KERNEL_RANGE == 8
    { 0.1427814521, 0.1340502275, 0.110931367, 0.0809153299, 0.0520228983, 0.0294810068, 0.0147254971, 0.0064829474 };
#elif FFX_BLUR_KERNEL_RANGE == 9
    { 0.1420666821, 0.1333791663, 0.1103760399, 0.0805102644, 0.0517624694, 0.0293334236, 0.0146517806, 0.0064504935, 0.0025030212 };
#elif FFX_BLUR_KERNEL_RANGE == 10
    { 0.1418238658, 0.1331511984, 0.1101873883, 0.0803726585, 0.0516739985, 0.0292832877, 0.0146267382, 0.0064394685, 0.0024987432, 0.0008545858 };
#elif FFX_BLUR_KERNEL_RANGE == 11
    { 0.1417508359, 0.1330826344, 0.1101306491, 0.0803312719, 0.0516473898, 0.0292682088, 0.0146192064, 0.0064361526, 0.0024974565, 0.0008541457, 0.0002574667 };
#endif
#elif FFX_BLUR_OPTION_KERNEL_PERMUTATION == 2
// Sigma: 4
#if FFX_BLUR_KERNEL_RANGE == 2
    { 0.3402771036, 0.3298614482 };
#elif FFX_BLUR_KERNEL_RANGE == 3
    { 0.2125433723, 0.2060375614, 0.1876907525 };
#elif FFX_BLUR_KERNEL_RANGE == 4
    { 0.1608542243, 0.1559305837, 0.1420455978, 0.1215967064 };
#elif FFX_BLUR_KERNEL_RANGE == 5
    { 0.1345347233, 0.1304167051, 0.1188036266, 0.1017006505, 0.0818116562 };
#elif FFX_BLUR_KERNEL_RANGE == 6
    { 0.1197258568, 0.1160611281, 0.1057263555, 0.090505984, 0.0728062644, 0.0550373395 };
#elif FFX_BLUR_KERNEL_RANGE == 7
    { 0.1110429695, 0.1076440182, 0.0980587551, 0.0839422118, 0.0675261302, 0.0510458624, 0.0362615375 };
#elif FFX_BLUR_KERNEL_RANGE == 8
    { 0.1059153311, 0.1026733334, 0.0935306896, 0.0800660068, 0.0644079717, 0.0486887143, 0.0345870861, 0.0230885324 };
#elif FFX_BLUR_KERNEL_RANGE == 9
    { 0.1029336421, 0.0997829119, 0.0908976484, 0.0778120183, 0.0625947824, 0.0473180477, 0.0336134033, 0.0224385526, 0.0140758142 };
#elif FFX_BLUR_KERNEL_RANGE == 10
    { 0.1012533395, 0.0981540422, 0.089413823, 0.0765418045, 0.0615729768, 0.0465456216, 0.0330646936, 0.0220722627, 0.0138460388, 0.0081620671 };
#elif FFX_BLUR_KERNEL_RANGE == 11
    { 0.1003459368, 0.0972744146, 0.0886125226, 0.0758558594, 0.0610211779, 0.0461284934, 0.0327683775, 0.0218744576, 0.0137219546, 0.008088921, 0.0044808529 };
#endif
#else
#error FFX_BLUR_OPTION_KERNEL_PERMUTATION is not a valid value.
#endif // FFX_BLUR_OPTION_KERNEL_PERMUTATIONs

    return kernel_weights[iKernelIndex];
}

#if FFX_HALF

    #if defined (FFX_BLUR_BIND_UAV_OUTPUT)
    void FfxBlurStoreOutput(FfxInt32x2 outPxCoord, FfxFloat16x3 color)
    {
        rw_output[outPxCoord] = FfxFloat16x4(color, 1);
    }
    #endif // #if defined (FFX_BLUR_BIND_UAV_OUTPUT)

    #if defined (FFX_BLUR_BIND_SRV_INPUT_SRC)
    FfxFloat16x3 FfxBlurLoadInput(FfxInt16x2 inPxCoord)
    {
        return r_input_src[inPxCoord].rgb;
    }
    #endif // #if defined (FFX_BLUR_BIND_SRV_INPUT_SRC)

#else // FFX_HALF

    #if defined (FFX_BLUR_BIND_UAV_OUTPUT)
    void FfxBlurStoreOutput(FfxInt32x2 outPxCoord, FfxFloat32x3 color)
    {
        rw_output[outPxCoord] = FfxFloat32x4(color, 1);
    }
    #endif // #if defined (FFX_BLUR_BIND_UAV_OUTPUT)

    #if defined (FFX_BLUR_BIND_SRV_INPUT_SRC)
    FfxFloat32x3 FfxBlurLoadInput(FfxInt32x2 inPxCoord)
    {
        return r_input_src[inPxCoord].rgb;
    }
    #endif // #if defined (FFX_BLUR_BIND_SRV_INPUT_SRC)

#endif // !FFX_HALF

#endif // #if defined(FFX_GPU)
