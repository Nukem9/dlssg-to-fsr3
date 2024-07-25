


//*********************************************************
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License (MIT).
//
//*********************************************************

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
    #define NOMINMAX 1
#endif
#ifdef __MINGW32__
    #include <unknwn.h>
#endif
#ifndef _WIN32
    #include <wsl/winadapter.h>
#endif
#include "d3dx12_property_format_table.h"
#include <assert.h>
#include <algorithm>
#include "D3D12TokenizedProgramFormat.hpp"
#if defined(D3D12_SDK_VERSION) && (D3D12_SDK_VERSION >= 606)
#ifndef ASSUME
  #define ASSUME(x) assert(x)
#endif

#define R D3DFCN_R
#define G D3DFCN_G
#define B D3DFCN_B
#define A D3DFCN_A
#define D D3DFCN_D
#define S D3DFCN_S
#define X D3DFCN_X

#define _TYPELESS   D3DFCI_TYPELESS
#define _FLOAT      D3DFCI_FLOAT
#define _SNORM      D3DFCI_SNORM
#define _UNORM      D3DFCI_UNORM
#define _SINT       D3DFCI_SINT
#define _UINT       D3DFCI_UINT
#define _UNORM_SRGB D3DFCI_UNORM_SRGB
#define _FIXED_2_8  D3DFCI_BIASED_FIXED_2_8

#ifndef INTSAFE_E_ARITHMETIC_OVERFLOW
    #define INTSAFE_E_ARITHMETIC_OVERFLOW   ((HRESULT)0x80070216L)
#endif

//
// UINT addition
//
inline HRESULT Safe_UIntAdd(UINT uAugend, UINT uAddend, UINT* puResult)
{
    if ((uAugend + uAddend) >= uAugend)
    {
        *puResult = (uAugend + uAddend);
        return S_OK;
    }
    *puResult = UINT_MAX;
    return E_FAIL;
}

//
// UINT multiplication
//
inline HRESULT Safe_UIntMult(UINT uMultiplicand, UINT uMultiplier, UINT* puResult)
{
    ULONGLONG ull64Result = (ULONGLONG)uMultiplicand *  (ULONGLONG)uMultiplier;
    
    if (ull64Result <= UINT_MAX)
    {
        *puResult = (UINT)ull64Result;
        return S_OK;
    }
    *puResult = UINT_MAX;
    return E_FAIL;
}

const LPCSTR D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::s_FormatNames[] =  // separate from above structure so it can be compiled out of the runtime.
{
//   Name
    "UNKNOWN",                      
    "R32G32B32A32_TYPELESS",        
        "R32G32B32A32_FLOAT",       
        "R32G32B32A32_UINT",        
        "R32G32B32A32_SINT",        
    "R32G32B32_TYPELESS",           
        "R32G32B32_FLOAT",          
        "R32G32B32_UINT",           
        "R32G32B32_SINT",           
    "R16G16B16A16_TYPELESS",        
        "R16G16B16A16_FLOAT",       
        "R16G16B16A16_UNORM",       
        "R16G16B16A16_UINT",        
        "R16G16B16A16_SNORM",       
        "R16G16B16A16_SINT",        
    "R32G32_TYPELESS",              
        "R32G32_FLOAT",             
        "R32G32_UINT",              
        "R32G32_SINT",              
    "R32G8X24_TYPELESS",            
        "D32_FLOAT_S8X24_UINT",     
        "R32_FLOAT_X8X24_TYPELESS", 
        "X32_TYPELESS_G8X24_UINT",  
    "R10G10B10A2_TYPELESS",         
        "R10G10B10A2_UNORM",        
        "R10G10B10A2_UINT",         
    "R11G11B10_FLOAT",              
    "R8G8B8A8_TYPELESS",            
        "R8G8B8A8_UNORM",           
        "R8G8B8A8_UNORM_SRGB",      
        "R8G8B8A8_UINT",            
        "R8G8B8A8_SNORM",           
        "R8G8B8A8_SINT",            
    "R16G16_TYPELESS",              
        "R16G16_FLOAT",             
        "R16G16_UNORM",             
        "R16G16_UINT",              
        "R16G16_SNORM",             
        "R16G16_SINT",              
    "R32_TYPELESS",                 
        "D32_FLOAT",                
        "R32_FLOAT",                
        "R32_UINT",                 
        "R32_SINT",                 
    "R24G8_TYPELESS",               
        "D24_UNORM_S8_UINT",        
        "R24_UNORM_X8_TYPELESS",    
        "X24_TYPELESS_G8_UINT",     
    "R8G8_TYPELESS",                
        "R8G8_UNORM",               
        "R8G8_UINT",                
        "R8G8_SNORM",               
        "R8G8_SINT",                
    "R16_TYPELESS",                 
        "R16_FLOAT",                
        "D16_UNORM",                
        "R16_UNORM",                
        "R16_UINT",                 
        "R16_SNORM",                
        "R16_SINT",                 
    "R8_TYPELESS",                  
        "R8_UNORM",                 
        "R8_UINT",                  
        "R8_SNORM",                 
        "R8_SINT",                  
    "A8_UNORM",                     
    "R1_UNORM",                     
    "R9G9B9E5_SHAREDEXP",           
    "R8G8_B8G8_UNORM",              
    "G8R8_G8B8_UNORM",              
    "BC1_TYPELESS",                 
        "BC1_UNORM",                
        "BC1_UNORM_SRGB",           
    "BC2_TYPELESS",                 
        "BC2_UNORM",                
        "BC2_UNORM_SRGB",           
    "BC3_TYPELESS",                 
        "BC3_UNORM",                
        "BC3_UNORM_SRGB",           
    "BC4_TYPELESS",                 
        "BC4_UNORM",                
        "BC4_SNORM",                
    "BC5_TYPELESS",                 
        "BC5_UNORM",                
        "BC5_SNORM",                
    "B5G6R5_UNORM",                 
    "B5G5R5A1_UNORM",               
    "B8G8R8A8_UNORM",               
    "B8G8R8X8_UNORM",               
    "R10G10B10_XR_BIAS_A2_UNORM",   
    "B8G8R8A8_TYPELESS",            
       "B8G8R8A8_UNORM_SRGB",       
    "B8G8R8X8_TYPELESS",            
       "B8G8R8X8_UNORM_SRGB",       
    "BC6H_TYPELESS",                
       "BC6H_UF16",                 
       "BC6H_SF16",                 
    "BC7_TYPELESS",                 
       "BC7_UNORM",                 
       "BC7_UNORM_SRGB",            
     "AYUV",                        
     "Y410",                        
     "Y416",                        
     "NV12",                        
     "P010",                        
     "P016",                        
     "420_OPAQUE",                  
     "YUY2",                        
     "Y210",                        
     "Y216",                        
     "NV11",                        
     "AI44",                        
     "IA44",                        
     "P8",                          
     "A8P8",
};

// --------------------------------------------------------------------------------------------------------------------------------
// Format Cast Sets
// --------------------------------------------------------------------------------------------------------------------------------
constexpr DXGI_FORMAT D3DFCS_UNKNOWN[] =
{
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_R32G32B32A32[] =
{
    DXGI_FORMAT_R32G32B32A32_TYPELESS,
    DXGI_FORMAT_R32G32B32A32_FLOAT,
    DXGI_FORMAT_R32G32B32A32_UINT,
    DXGI_FORMAT_R32G32B32A32_SINT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_R32G32B32[] =
{
    DXGI_FORMAT_R32G32B32_TYPELESS,
    DXGI_FORMAT_R32G32B32_FLOAT,
    DXGI_FORMAT_R32G32B32_UINT,
    DXGI_FORMAT_R32G32B32_SINT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_R16G16B16A16[] =
{
    DXGI_FORMAT_R16G16B16A16_TYPELESS,
    DXGI_FORMAT_R16G16B16A16_FLOAT,
    DXGI_FORMAT_R16G16B16A16_UNORM,
    DXGI_FORMAT_R16G16B16A16_UINT,
    DXGI_FORMAT_R16G16B16A16_SNORM,
    DXGI_FORMAT_R16G16B16A16_SINT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_R32G32[] =
{
    DXGI_FORMAT_R32G32_TYPELESS,
    DXGI_FORMAT_R32G32_FLOAT,
    DXGI_FORMAT_R32G32_UINT,
    DXGI_FORMAT_R32G32_SINT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_R32G8X24[] =
{
    DXGI_FORMAT_R32G8X24_TYPELESS,
    DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
    DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,
    DXGI_FORMAT_X32_TYPELESS_G8X24_UINT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_R11G11B10[] =
{
    DXGI_FORMAT_R11G11B10_FLOAT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_R8G8B8A8[] =
{
    DXGI_FORMAT_R8G8B8A8_TYPELESS,
    DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
    DXGI_FORMAT_R8G8B8A8_UINT,
    DXGI_FORMAT_R8G8B8A8_SNORM,
    DXGI_FORMAT_R8G8B8A8_SINT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_R16G16[] =
{
    DXGI_FORMAT_R16G16_TYPELESS,
    DXGI_FORMAT_R16G16_FLOAT,
    DXGI_FORMAT_R16G16_UNORM,
    DXGI_FORMAT_R16G16_UINT,
    DXGI_FORMAT_R16G16_SNORM,
    DXGI_FORMAT_R16G16_SINT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_R32[] =
{
    DXGI_FORMAT_R32_TYPELESS,
    DXGI_FORMAT_D32_FLOAT,
    DXGI_FORMAT_R32_FLOAT,
    DXGI_FORMAT_R32_UINT,
    DXGI_FORMAT_R32_SINT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_R24G8[] =
{
    DXGI_FORMAT_R24G8_TYPELESS,
    DXGI_FORMAT_D24_UNORM_S8_UINT,
    DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
    DXGI_FORMAT_X24_TYPELESS_G8_UINT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_R8G8[] =
{
    DXGI_FORMAT_R8G8_TYPELESS,
    DXGI_FORMAT_R8G8_UNORM,
    DXGI_FORMAT_R8G8_UINT,
    DXGI_FORMAT_R8G8_SNORM,
    DXGI_FORMAT_R8G8_SINT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_R16[] =
{
    DXGI_FORMAT_R16_TYPELESS,
    DXGI_FORMAT_R16_FLOAT,
    DXGI_FORMAT_D16_UNORM,
    DXGI_FORMAT_R16_UNORM,
    DXGI_FORMAT_R16_UINT,
    DXGI_FORMAT_R16_SNORM,
    DXGI_FORMAT_R16_SINT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_R8[] =
{
    DXGI_FORMAT_R8_TYPELESS,
    DXGI_FORMAT_R8_UNORM,
    DXGI_FORMAT_R8_UINT,
    DXGI_FORMAT_R8_SNORM,
    DXGI_FORMAT_R8_SINT,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_A8[] =
{
    DXGI_FORMAT_A8_UNORM,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_R1[] =
{
    DXGI_FORMAT_R1_UNORM,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_R9G9B9E5[] =
{
    DXGI_FORMAT_R9G9B9E5_SHAREDEXP,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_R8G8_B8G8[] =
{
    DXGI_FORMAT_R8G8_B8G8_UNORM,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_G8R8_G8B8[] =
{
    DXGI_FORMAT_G8R8_G8B8_UNORM,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_BC1[] =
{
    DXGI_FORMAT_BC1_TYPELESS,
    DXGI_FORMAT_BC1_UNORM,
    DXGI_FORMAT_BC1_UNORM_SRGB,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_BC2[] =
{
    DXGI_FORMAT_BC2_TYPELESS,
    DXGI_FORMAT_BC2_UNORM,
    DXGI_FORMAT_BC2_UNORM_SRGB,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_BC3[] =
{
    DXGI_FORMAT_BC3_TYPELESS,
    DXGI_FORMAT_BC3_UNORM,
    DXGI_FORMAT_BC3_UNORM_SRGB,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_BC4[] =
{
    DXGI_FORMAT_BC4_TYPELESS,
    DXGI_FORMAT_BC4_UNORM,
    DXGI_FORMAT_BC4_SNORM,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_BC5[] =
{
    DXGI_FORMAT_BC5_TYPELESS,
    DXGI_FORMAT_BC5_UNORM,
    DXGI_FORMAT_BC5_SNORM,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_B5G6R5[] =
{
    DXGI_FORMAT_B5G6R5_UNORM,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_B5G5R5A1[] =
{
    DXGI_FORMAT_B5G5R5A1_UNORM,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_B8G8R8A8[] =
{
    DXGI_FORMAT_B8G8R8A8_TYPELESS,
    DXGI_FORMAT_B8G8R8A8_UNORM,
    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_B8G8R8X8[] =
{
    DXGI_FORMAT_B8G8R8X8_TYPELESS,
    DXGI_FORMAT_B8G8R8X8_UNORM,
    DXGI_FORMAT_B8G8R8X8_UNORM_SRGB,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_R10G10B10A2[] =
{
    DXGI_FORMAT_R10G10B10A2_TYPELESS,
    DXGI_FORMAT_R10G10B10A2_UNORM,
    DXGI_FORMAT_R10G10B10A2_UINT,
    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_BC6H[] =
{
    DXGI_FORMAT_BC6H_TYPELESS,
    DXGI_FORMAT_BC6H_UF16,
    DXGI_FORMAT_BC6H_SF16,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_BC7[] =
{
    DXGI_FORMAT_BC7_TYPELESS,
    DXGI_FORMAT_BC7_UNORM,
    DXGI_FORMAT_BC7_UNORM_SRGB,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_AYUV[] =
{
    DXGI_FORMAT_AYUV,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_NV12[] =
{
    DXGI_FORMAT_NV12,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_YUY2[] =
{
    DXGI_FORMAT_YUY2,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_P010[] =
{
    DXGI_FORMAT_P010,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_P016[] =
{
    DXGI_FORMAT_P016,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_NV11[] =
{
    DXGI_FORMAT_NV11,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_420_OPAQUE[] =
{
    DXGI_FORMAT_420_OPAQUE,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_Y410[] =
{
    DXGI_FORMAT_Y410,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_Y416[] =
{
    DXGI_FORMAT_Y416,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_Y210[] =
{
    DXGI_FORMAT_Y210,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_Y216[] =
{
    DXGI_FORMAT_Y216,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_AI44[] =
{
    DXGI_FORMAT_AI44,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_IA44[] =
{
    DXGI_FORMAT_IA44,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_P8[] =
{
    DXGI_FORMAT_P8,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_A8P8[] =
{
    DXGI_FORMAT_A8P8,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_B4G4R4A4[] =
{
    DXGI_FORMAT_B4G4R4A4_UNORM,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_P208[] =
{
    DXGI_FORMAT_P208,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_V208[] =
{
    DXGI_FORMAT_V208,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr DXGI_FORMAT D3DFCS_V408[] =
{
    DXGI_FORMAT_V408,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};
     
constexpr DXGI_FORMAT D3DFCS_A4B4G4R4[] =
{
    DXGI_FORMAT_A4B4G4R4_UNORM,
    DXGI_FORMAT_UNKNOWN // not part of cast set, just the "null terminator"
};

constexpr D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::FORMAT_DETAIL D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::s_FormatDetail[] =
{
    //   DXGI_FORMAT                                           ParentFormat                              pDefaultFormatCastSet BitsPerComponent[4], BitsPerUnit,    SRGB,  WidthAlignment, HeightAlignment, DepthAlignment,   Layout,           TypeLevel,            ComponentName[4],ComponentInterpretation[4],                          bDX9VertexOrIndexFormat bDX9TextureFormat,   bFloatNormFormat, bPlanar, bYUV    bDependantFormatCastSet bInternal
        {DXGI_FORMAT_UNKNOWN                                  ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          TRUE,                   FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_R32G32B32A32_TYPELESS                    ,DXGI_FORMAT_R32G32B32A32_TYPELESS,        D3DFCS_R32G32B32A32,  {32,32,32,32},       128,            FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_PARTIAL_TYPE,  R,G,B,A,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R32G32B32A32_FLOAT                   ,DXGI_FORMAT_R32G32B32A32_TYPELESS,        D3DFCS_R32G32B32A32,  {32,32,32,32},       128,            FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,B,A,         _FLOAT, _FLOAT, _FLOAT, _FLOAT,                      TRUE,                   FALSE,               TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R32G32B32A32_UINT                    ,DXGI_FORMAT_R32G32B32A32_TYPELESS,        D3DFCS_R32G32B32A32,  {32,32,32,32},       128,            FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,B,A,         _UINT, _UINT, _UINT, _UINT,                          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R32G32B32A32_SINT                    ,DXGI_FORMAT_R32G32B32A32_TYPELESS,        D3DFCS_R32G32B32A32,  {32,32,32,32},       128,            FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,B,A,         _SINT, _SINT, _SINT, _SINT,                          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_R32G32B32_TYPELESS                       ,DXGI_FORMAT_R32G32B32_TYPELESS,           D3DFCS_R32G32B32,     {32,32,32,0},        96,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_PARTIAL_TYPE,  R,G,B,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R32G32B32_FLOAT                      ,DXGI_FORMAT_R32G32B32_TYPELESS,           D3DFCS_R32G32B32,     {32,32,32,0},        96,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,B,X,         _FLOAT, _FLOAT, _FLOAT, _TYPELESS,                   TRUE,                   FALSE,               TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R32G32B32_UINT                       ,DXGI_FORMAT_R32G32B32_TYPELESS,           D3DFCS_R32G32B32,     {32,32,32,0},        96,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,B,X,         _UINT, _UINT, _UINT, _TYPELESS,                      FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R32G32B32_SINT                       ,DXGI_FORMAT_R32G32B32_TYPELESS,           D3DFCS_R32G32B32,     {32,32,32,0},        96,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,B,X,         _SINT, _SINT, _SINT, _TYPELESS,                      FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_R16G16B16A16_TYPELESS                    ,DXGI_FORMAT_R16G16B16A16_TYPELESS,        D3DFCS_R16G16B16A16,  {16,16,16,16},       64,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_PARTIAL_TYPE,  R,G,B,A,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {   DXGI_FORMAT_R16G16B16A16_FLOAT                    ,DXGI_FORMAT_R16G16B16A16_TYPELESS,        D3DFCS_R16G16B16A16,  {16,16,16,16},       64,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,B,A,         _FLOAT, _FLOAT, _FLOAT, _FLOAT,                      TRUE,                   FALSE,               TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R16G16B16A16_UNORM                   ,DXGI_FORMAT_R16G16B16A16_TYPELESS,        D3DFCS_R16G16B16A16,  {16,16,16,16},       64,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,B,A,         _UNORM, _UNORM, _UNORM, _UNORM,                      TRUE,                   TRUE,                TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R16G16B16A16_UINT                    ,DXGI_FORMAT_R16G16B16A16_TYPELESS,        D3DFCS_R16G16B16A16,  {16,16,16,16},       64,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,B,A,         _UINT, _UINT, _UINT, _UINT,                          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R16G16B16A16_SNORM                   ,DXGI_FORMAT_R16G16B16A16_TYPELESS,        D3DFCS_R16G16B16A16,  {16,16,16,16},       64,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,B,A,         _SNORM, _SNORM, _SNORM, _SNORM,                      TRUE,                   FALSE,               TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R16G16B16A16_SINT                    ,DXGI_FORMAT_R16G16B16A16_TYPELESS,        D3DFCS_R16G16B16A16,  {16,16,16,16},       64,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,B,A,         _SINT, _SINT, _SINT, _SINT,                          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_R32G32_TYPELESS                          ,DXGI_FORMAT_R32G32_TYPELESS,              D3DFCS_R32G32,        {32,32,0,0},         64,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_PARTIAL_TYPE,  R,G,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R32G32_FLOAT                         ,DXGI_FORMAT_R32G32_TYPELESS,              D3DFCS_R32G32,        {32,32,0,0},         64,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,X,X,         _FLOAT, _FLOAT, _TYPELESS, _TYPELESS,                TRUE,                   FALSE,               TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R32G32_UINT                          ,DXGI_FORMAT_R32G32_TYPELESS,              D3DFCS_R32G32,        {32,32,0,0},         64,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,X,X,         _UINT, _UINT, _TYPELESS, _TYPELESS,                  FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R32G32_SINT                          ,DXGI_FORMAT_R32G32_TYPELESS,              D3DFCS_R32G32,        {32,32,0,0},         64,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,X,X,         _SINT, _SINT, _TYPELESS, _TYPELESS,                  FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_R32G8X24_TYPELESS                        ,DXGI_FORMAT_R32G8X24_TYPELESS,            D3DFCS_R32G8X24,      {32,8,24,0},         64,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_PARTIAL_TYPE,  R,G,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            TRUE,    FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_D32_FLOAT_S8X24_UINT                 ,DXGI_FORMAT_R32G8X24_TYPELESS,            D3DFCS_R32G8X24,      {32,8,24,0},         64,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     D,S,X,X,         _FLOAT,_UINT,_TYPELESS,_TYPELESS,                    FALSE,                  FALSE,               FALSE,            TRUE,    FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS             ,DXGI_FORMAT_R32G8X24_TYPELESS,            D3DFCS_R32G8X24,      {32,8,24,0},         64,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,X,X,X,         _FLOAT,_TYPELESS,_TYPELESS,_TYPELESS,                FALSE,                  FALSE,               TRUE,             TRUE,    FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_X32_TYPELESS_G8X24_UINT              ,DXGI_FORMAT_R32G8X24_TYPELESS,            D3DFCS_R32G8X24,      {32,8,24,0},         64,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     X,G,X,X,         _TYPELESS,_UINT,_TYPELESS,_TYPELESS,                 FALSE,                  FALSE,               FALSE,            TRUE,    FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_R10G10B10A2_TYPELESS                     ,DXGI_FORMAT_R10G10B10A2_TYPELESS,         D3DFCS_R10G10B10A2,   {10,10,10,2},        32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_PARTIAL_TYPE,  R,G,B,A,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  TRUE,                   FALSE,    },
        {    DXGI_FORMAT_R10G10B10A2_UNORM                    ,DXGI_FORMAT_R10G10B10A2_TYPELESS,         D3DFCS_R10G10B10A2,   {10,10,10,2},        32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,B,A,         _UNORM, _UNORM, _UNORM, _UNORM,                      FALSE,                  TRUE,                TRUE,             FALSE,   FALSE,  TRUE,                   FALSE,    },
        {    DXGI_FORMAT_R10G10B10A2_UINT                     ,DXGI_FORMAT_R10G10B10A2_TYPELESS,         D3DFCS_R10G10B10A2,   {10,10,10,2},        32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,B,A,         _UINT, _UINT, _UINT, _UINT,                          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  TRUE,                   FALSE,    },
        {DXGI_FORMAT_R11G11B10_FLOAT                          ,DXGI_FORMAT_R11G11B10_FLOAT,              D3DFCS_R11G11B10,     {11,11,10,0},        32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,B,X,         _FLOAT, _FLOAT, _FLOAT, _TYPELESS,                   FALSE,                  FALSE,               TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_R8G8B8A8_TYPELESS                        ,DXGI_FORMAT_R8G8B8A8_TYPELESS,            D3DFCS_R8G8B8A8,      {8,8,8,8},           32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_PARTIAL_TYPE,  R,G,B,A,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R8G8B8A8_UNORM                       ,DXGI_FORMAT_R8G8B8A8_TYPELESS,            D3DFCS_R8G8B8A8,      {8,8,8,8},           32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,B,A,         _UNORM, _UNORM, _UNORM, _UNORM,                      TRUE,                   TRUE,                TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB                  ,DXGI_FORMAT_R8G8B8A8_TYPELESS,            D3DFCS_R8G8B8A8,      {8,8,8,8},           32,             TRUE,  1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,B,A,         _UNORM_SRGB, _UNORM_SRGB, _UNORM_SRGB, _UNORM_SRGB,  FALSE,                  TRUE,                TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R8G8B8A8_UINT                        ,DXGI_FORMAT_R8G8B8A8_TYPELESS,            D3DFCS_R8G8B8A8,      {8,8,8,8},           32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,B,A,         _UINT, _UINT, _UINT, _UINT,                          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R8G8B8A8_SNORM                       ,DXGI_FORMAT_R8G8B8A8_TYPELESS,            D3DFCS_R8G8B8A8,      {8,8,8,8},           32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,B,A,         _SNORM, _SNORM, _SNORM, _SNORM,                      FALSE,                  FALSE,               TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R8G8B8A8_SINT                        ,DXGI_FORMAT_R8G8B8A8_TYPELESS,            D3DFCS_R8G8B8A8,      {8,8,8,8},           32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,B,A,         _SINT, _SINT, _SINT, _SINT,                          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_R16G16_TYPELESS                          ,DXGI_FORMAT_R16G16_TYPELESS,              D3DFCS_R16G16,        {16,16,0,0},         32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_PARTIAL_TYPE,  R,G,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R16G16_FLOAT                         ,DXGI_FORMAT_R16G16_TYPELESS,              D3DFCS_R16G16,        {16,16,0,0},         32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,X,X,         _FLOAT, _FLOAT, _TYPELESS, _TYPELESS,                TRUE,                   TRUE,                TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R16G16_UNORM                         ,DXGI_FORMAT_R16G16_TYPELESS,              D3DFCS_R16G16,        {16,16,0,0},         32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,X,X,         _UNORM, _UNORM, _TYPELESS, _TYPELESS,                TRUE,                   TRUE,                TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R16G16_UINT                          ,DXGI_FORMAT_R16G16_TYPELESS,              D3DFCS_R16G16,        {16,16,0,0},         32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,X,X,         _UINT, _UINT, _TYPELESS, _TYPELESS,                  FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R16G16_SNORM                         ,DXGI_FORMAT_R16G16_TYPELESS,              D3DFCS_R16G16,        {16,16,0,0},         32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,X,X,         _SNORM, _SNORM, _TYPELESS, _TYPELESS,                TRUE,                   TRUE,                TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R16G16_SINT                          ,DXGI_FORMAT_R16G16_TYPELESS,              D3DFCS_R16G16,        {16,16,0,0},         32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,X,X,         _SINT, _SINT, _TYPELESS, _TYPELESS,                  FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_R32_TYPELESS                             ,DXGI_FORMAT_R32_TYPELESS,                 D3DFCS_R32,           {32,0,0,0},          32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_PARTIAL_TYPE,  R,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_D32_FLOAT                            ,DXGI_FORMAT_R32_TYPELESS,                 D3DFCS_R32,           {32,0,0,0},          32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     D,X,X,X,         _FLOAT, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,                  TRUE,                TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R32_FLOAT                            ,DXGI_FORMAT_R32_TYPELESS,                 D3DFCS_R32,           {32,0,0,0},          32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,X,X,X,         _FLOAT, _TYPELESS, _TYPELESS, _TYPELESS,             TRUE,                   TRUE,                TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R32_UINT                             ,DXGI_FORMAT_R32_TYPELESS,                 D3DFCS_R32,           {32,0,0,0},          32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,X,X,X,         _UINT, _TYPELESS, _TYPELESS, _TYPELESS,              FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R32_SINT                             ,DXGI_FORMAT_R32_TYPELESS,                 D3DFCS_R32,           {32,0,0,0},          32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,X,X,X,         _SINT, _TYPELESS, _TYPELESS, _TYPELESS,              FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_R24G8_TYPELESS                           ,DXGI_FORMAT_R24G8_TYPELESS,               D3DFCS_R24G8,         {24,8,0,0},          32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_PARTIAL_TYPE,  R,G,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            TRUE,    FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_D24_UNORM_S8_UINT                    ,DXGI_FORMAT_R24G8_TYPELESS,               D3DFCS_R24G8,         {24,8,0,0},          32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     D,S,X,X,         _UNORM,_UINT,_TYPELESS,_TYPELESS,                    FALSE,                  TRUE,                FALSE,            TRUE,    FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R24_UNORM_X8_TYPELESS                ,DXGI_FORMAT_R24G8_TYPELESS,               D3DFCS_R24G8,         {24,8,0,0},          32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,X,X,X,         _UNORM,_TYPELESS,_TYPELESS,_TYPELESS,                FALSE,                  FALSE,               TRUE,             TRUE,    FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_X24_TYPELESS_G8_UINT                 ,DXGI_FORMAT_R24G8_TYPELESS,               D3DFCS_R24G8,         {24,8,0,0},          32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     X,G,X,X,         _TYPELESS,_UINT,_TYPELESS,_TYPELESS,                 FALSE,                  FALSE,               FALSE,            TRUE,    FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_R8G8_TYPELESS                            ,DXGI_FORMAT_R8G8_TYPELESS,                D3DFCS_R8G8,          {8,8,0,0},           16,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_PARTIAL_TYPE,  R,G,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R8G8_UNORM                           ,DXGI_FORMAT_R8G8_TYPELESS,                D3DFCS_R8G8,          {8,8,0,0},           16,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,X,X,         _UNORM, _UNORM, _TYPELESS, _TYPELESS,                FALSE,                  FALSE,               TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R8G8_UINT                            ,DXGI_FORMAT_R8G8_TYPELESS,                D3DFCS_R8G8,          {8,8,0,0},           16,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,X,X,         _UINT, _UINT, _TYPELESS, _TYPELESS,                  FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R8G8_SNORM                           ,DXGI_FORMAT_R8G8_TYPELESS,                D3DFCS_R8G8,          {8,8,0,0},           16,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,X,X,         _SNORM, _SNORM, _TYPELESS, _TYPELESS,                FALSE,                  TRUE,                TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R8G8_SINT                            ,DXGI_FORMAT_R8G8_TYPELESS,                D3DFCS_R8G8,          {8,8,0,0},           16,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,X,X,         _SINT, _SINT, _TYPELESS, _TYPELESS,                  FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_R16_TYPELESS                             ,DXGI_FORMAT_R16_TYPELESS,                 D3DFCS_R16,           {16,0,0,0},          16,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_PARTIAL_TYPE,  R,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R16_FLOAT                            ,DXGI_FORMAT_R16_TYPELESS,                 D3DFCS_R16,           {16,0,0,0},          16,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,X,X,X,         _FLOAT, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,                  FALSE,               TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_D16_UNORM                            ,DXGI_FORMAT_R16_TYPELESS,                 D3DFCS_R16,           {16,0,0,0},          16,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     D,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,                  TRUE,                TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R16_UNORM                            ,DXGI_FORMAT_R16_TYPELESS,                 D3DFCS_R16,           {16,0,0,0},          16,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,                  FALSE,               TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R16_UINT                             ,DXGI_FORMAT_R16_TYPELESS,                 D3DFCS_R16,           {16,0,0,0},          16,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,X,X,X,         _UINT, _TYPELESS, _TYPELESS, _TYPELESS,              FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R16_SNORM                            ,DXGI_FORMAT_R16_TYPELESS,                 D3DFCS_R16,           {16,0,0,0},          16,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,X,X,X,         _SNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,                  FALSE,               TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R16_SINT                             ,DXGI_FORMAT_R16_TYPELESS,                 D3DFCS_R16,           {16,0,0,0},          16,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,X,X,X,         _SINT, _TYPELESS, _TYPELESS, _TYPELESS,              FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_R8_TYPELESS                              ,DXGI_FORMAT_R8_TYPELESS,                  D3DFCS_R8,            {8,0,0,0},           8,              FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_PARTIAL_TYPE,  R,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R8_UNORM                             ,DXGI_FORMAT_R8_TYPELESS,                  D3DFCS_R8,            {8,0,0,0},           8,              FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,                  FALSE,               TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R8_UINT                              ,DXGI_FORMAT_R8_TYPELESS,                  D3DFCS_R8,            {8,0,0,0},           8,              FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,X,X,X,         _UINT, _TYPELESS, _TYPELESS, _TYPELESS,              FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R8_SNORM                             ,DXGI_FORMAT_R8_TYPELESS,                  D3DFCS_R8,            {8,0,0,0},           8,              FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,X,X,X,         _SNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,                  FALSE,               TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_R8_SINT                              ,DXGI_FORMAT_R8_TYPELESS,                  D3DFCS_R8,            {8,0,0,0},           8,              FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,X,X,X,         _SINT, _TYPELESS, _TYPELESS, _TYPELESS,              FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_A8_UNORM                                 ,DXGI_FORMAT_A8_UNORM,                     D3DFCS_A8,            {0,0,0,8},           8,              FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     X,X,X,A,         _TYPELESS, _TYPELESS, _TYPELESS, _UNORM,             FALSE,                  TRUE,                TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_R1_UNORM                                 ,DXGI_FORMAT_R1_UNORM,                     D3DFCS_R1,            {1,0,0,0},           1,              FALSE, 8,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,                  FALSE,               TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_R9G9B9E5_SHAREDEXP                       ,DXGI_FORMAT_R9G9B9E5_SHAREDEXP,           D3DFCS_R9G9B9E5,      {0,0,0,0},           32,             FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,G,B,X,         _FLOAT, _FLOAT, _FLOAT, _FLOAT,                      FALSE,                  FALSE,               TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_R8G8_B8G8_UNORM                          ,DXGI_FORMAT_R8G8_B8G8_UNORM,              D3DFCS_R8G8_B8G8,     {0,0,0,0},           16,             FALSE, 2,              1,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,G,B,X,         _UNORM, _UNORM, _UNORM, _TYPELESS,                   FALSE,                  TRUE,                TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_G8R8_G8B8_UNORM                          ,DXGI_FORMAT_G8R8_G8B8_UNORM,              D3DFCS_G8R8_G8B8,     {0,0,0,0},           16,             FALSE, 2,              1,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,G,B,X,         _UNORM, _UNORM, _UNORM, _TYPELESS,                   FALSE,                  TRUE,                TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_BC1_TYPELESS                             ,DXGI_FORMAT_BC1_TYPELESS,                 D3DFCS_BC1,           {0,0,0,0},           64,             FALSE, 4,              4,               1,                D3DFL_CUSTOM,     D3DFTL_PARTIAL_TYPE,  R,G,B,A,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  TRUE,                FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_BC1_UNORM                            ,DXGI_FORMAT_BC1_TYPELESS,                 D3DFCS_BC1,           {0,0,0,0},           64,             FALSE, 4,              4,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,G,B,A,         _UNORM, _UNORM, _UNORM, _UNORM,                      FALSE,                  TRUE,                TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_BC1_UNORM_SRGB                       ,DXGI_FORMAT_BC1_TYPELESS,                 D3DFCS_BC1,           {0,0,0,0},           64,             TRUE,  4,              4,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,G,B,A,         _UNORM_SRGB, _UNORM_SRGB, _UNORM_SRGB, _UNORM,       FALSE,                  TRUE,                TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_BC2_TYPELESS                             ,DXGI_FORMAT_BC2_TYPELESS,                 D3DFCS_BC2,           {0,0,0,0},           128,            FALSE, 4,              4,               1,                D3DFL_CUSTOM,     D3DFTL_PARTIAL_TYPE,  R,G,B,A,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  TRUE,                FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_BC2_UNORM                            ,DXGI_FORMAT_BC2_TYPELESS,                 D3DFCS_BC2,           {0,0,0,0},           128,            FALSE, 4,              4,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,G,B,A,         _UNORM, _UNORM, _UNORM, _UNORM,                      FALSE,                  TRUE,                TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_BC2_UNORM_SRGB                       ,DXGI_FORMAT_BC2_TYPELESS,                 D3DFCS_BC2,           {0,0,0,0},           128,            TRUE,  4,              4,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,G,B,A,         _UNORM_SRGB, _UNORM_SRGB, _UNORM_SRGB, _UNORM,       FALSE,                  TRUE,                TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_BC3_TYPELESS                             ,DXGI_FORMAT_BC3_TYPELESS,                 D3DFCS_BC3,           {0,0,0,0},           128,            FALSE, 4,              4,               1,                D3DFL_CUSTOM,     D3DFTL_PARTIAL_TYPE,  R,G,B,A,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_BC3_UNORM                            ,DXGI_FORMAT_BC3_TYPELESS,                 D3DFCS_BC3,           {0,0,0,0},           128,            FALSE, 4,              4,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,G,B,A,         _UNORM, _UNORM, _UNORM, _UNORM,                      FALSE,                  TRUE,                TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_BC3_UNORM_SRGB                       ,DXGI_FORMAT_BC3_TYPELESS,                 D3DFCS_BC3,           {0,0,0,0},           128,            TRUE,  4,              4,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,G,B,A,         _UNORM_SRGB, _UNORM_SRGB, _UNORM_SRGB, _UNORM,       FALSE,                  TRUE,                TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_BC4_TYPELESS                             ,DXGI_FORMAT_BC4_TYPELESS,                 D3DFCS_BC4,           {0,0,0,0},           64,             FALSE, 4,              4,               1,                D3DFL_CUSTOM,     D3DFTL_PARTIAL_TYPE,  R,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_BC4_UNORM                            ,DXGI_FORMAT_BC4_TYPELESS,                 D3DFCS_BC4,           {0,0,0,0},           64,             FALSE, 4,              4,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,                  FALSE,               TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_BC4_SNORM                            ,DXGI_FORMAT_BC4_TYPELESS,                 D3DFCS_BC4,           {0,0,0,0},           64,             FALSE, 4,              4,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,X,X,X,         _SNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,                  FALSE,               TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_BC5_TYPELESS                             ,DXGI_FORMAT_BC5_TYPELESS,                 D3DFCS_BC5,           {0,0,0,0},           128,            FALSE, 4,              4,               1,                D3DFL_CUSTOM,     D3DFTL_PARTIAL_TYPE,  R,G,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_BC5_UNORM                            ,DXGI_FORMAT_BC5_TYPELESS,                 D3DFCS_BC5,           {0,0,0,0},           128,            FALSE, 4,              4,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,G,X,X,         _UNORM, _UNORM, _TYPELESS, _TYPELESS,                FALSE,                  FALSE,               TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_BC5_SNORM                            ,DXGI_FORMAT_BC5_TYPELESS,                 D3DFCS_BC5,           {0,0,0,0},           128,            FALSE, 4,              4,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,G,X,X,         _SNORM, _SNORM, _TYPELESS, _TYPELESS,                FALSE,                  FALSE,               TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_B5G6R5_UNORM                             ,DXGI_FORMAT_B5G6R5_UNORM,                 D3DFCS_B5G6R5,        {5,6,5,0},           16,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     B,G,R,X,         _UNORM, _UNORM, _UNORM, _TYPELESS,                   FALSE,                  TRUE,                TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_B5G5R5A1_UNORM                           ,DXGI_FORMAT_B5G5R5A1_UNORM,               D3DFCS_B5G5R5A1,      {5,5,5,1},           16,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     B,G,R,A,         _UNORM, _UNORM, _UNORM, _UNORM,                      FALSE,                  TRUE,                TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_B8G8R8A8_UNORM                           ,DXGI_FORMAT_B8G8R8A8_TYPELESS,            D3DFCS_B8G8R8A8,      {8,8,8,8},           32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     B,G,R,A,         _UNORM, _UNORM, _UNORM, _UNORM,                      FALSE,                  TRUE,                TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_B8G8R8X8_UNORM                           ,DXGI_FORMAT_B8G8R8X8_TYPELESS,            D3DFCS_B8G8R8X8,      {8,8,8,8},           32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     B,G,R,X,         _UNORM, _UNORM, _UNORM, _TYPELESS,                   FALSE,                  TRUE,                TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM               ,DXGI_FORMAT_R10G10B10A2_TYPELESS,         D3DFCS_R10G10B10A2,   {10,10,10,2},        32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     R,G,B,A,         _FIXED_2_8, _FIXED_2_8, _FIXED_2_8, _UNORM,          FALSE,                  TRUE,                FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_B8G8R8A8_TYPELESS                        ,DXGI_FORMAT_B8G8R8A8_TYPELESS,            D3DFCS_B8G8R8A8,      {8,8,8,8},           32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_PARTIAL_TYPE,  B,G,R,A,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  TRUE,                FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB                  ,DXGI_FORMAT_B8G8R8A8_TYPELESS,            D3DFCS_B8G8R8A8,      {8,8,8,8},           32,             TRUE,  1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     B,G,R,A,         _UNORM_SRGB, _UNORM_SRGB, _UNORM_SRGB, _UNORM_SRGB,  FALSE,                  TRUE,                TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_B8G8R8X8_TYPELESS                        ,DXGI_FORMAT_B8G8R8X8_TYPELESS,            D3DFCS_B8G8R8X8,      {8,8,8,8},           32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_PARTIAL_TYPE,  B,G,R,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  TRUE,                FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_B8G8R8X8_UNORM_SRGB                  ,DXGI_FORMAT_B8G8R8X8_TYPELESS,            D3DFCS_B8G8R8X8,      {8,8,8,8},           32,             TRUE,  1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     B,G,R,X,         _UNORM_SRGB, _UNORM_SRGB, _UNORM_SRGB, _TYPELESS,    FALSE,                  TRUE,                TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_BC6H_TYPELESS                            ,DXGI_FORMAT_BC6H_TYPELESS,                D3DFCS_BC6H,          {0,0,0,0},           128,            FALSE, 4,              4,               1,                D3DFL_CUSTOM,     D3DFTL_PARTIAL_TYPE,  R,G,B,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_BC6H_UF16                            ,DXGI_FORMAT_BC6H_TYPELESS,                D3DFCS_BC6H,          {0,0,0,0},           128,            FALSE, 4,              4,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,G,B,X,         _FLOAT, _FLOAT, _FLOAT, _TYPELESS,                   FALSE,                  FALSE,               TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_BC6H_SF16                            ,DXGI_FORMAT_BC6H_TYPELESS,                D3DFCS_BC6H,          {0,0,0,0},           128,            FALSE, 4,              4,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,G,B,X,         _FLOAT, _FLOAT, _FLOAT, _TYPELESS,                   FALSE,                  FALSE,               TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {DXGI_FORMAT_BC7_TYPELESS                             ,DXGI_FORMAT_BC7_TYPELESS,                 D3DFCS_BC7,           {0,0,0,0},           128,            FALSE, 4,              4,               1,                D3DFL_CUSTOM,     D3DFTL_PARTIAL_TYPE,  R,G,B,A,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_BC7_UNORM                            ,DXGI_FORMAT_BC7_TYPELESS,                 D3DFCS_BC7,           {0,0,0,0},           128,            FALSE, 4,              4,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,G,B,A,         _UNORM, _UNORM, _UNORM, _UNORM,                      FALSE,                  FALSE,               TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        {    DXGI_FORMAT_BC7_UNORM_SRGB                       ,DXGI_FORMAT_BC7_TYPELESS,                 D3DFCS_BC7,           {0,0,0,0},           128,            TRUE,  4,              4,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,G,B,A,         _UNORM_SRGB, _UNORM_SRGB, _UNORM_SRGB, _UNORM,       FALSE,                  FALSE,               TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        // YUV 4:4:4 formats                                                                                                                                                                                                       
        { DXGI_FORMAT_AYUV                                    ,DXGI_FORMAT_AYUV,                         D3DFCS_AYUV,          {8,8,8,8},           32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     B,G,R,A,         _UNORM, _UNORM, _UNORM, _UNORM,                      FALSE,                  TRUE,                FALSE,            FALSE,   TRUE,   FALSE,                  FALSE,    },
        { DXGI_FORMAT_Y410                                    ,DXGI_FORMAT_Y410,                         D3DFCS_Y410,          {10,10,10,2},        32,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     B,G,R,A,         _UNORM, _UNORM, _UNORM, _UNORM,                      FALSE,                  FALSE,               FALSE,            FALSE,   TRUE,   FALSE,                  FALSE,    },
        { DXGI_FORMAT_Y416                                    ,DXGI_FORMAT_Y416,                         D3DFCS_Y416,          {16,16,16,16},       64,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     B,G,R,A,         _UNORM, _UNORM, _UNORM, _UNORM,                      FALSE,                  FALSE,               FALSE,            FALSE,   TRUE,   FALSE,                  FALSE,    },
        // YUV 4:2:0 formats                                                                                                                                                                                                                            
        { DXGI_FORMAT_NV12                                    ,DXGI_FORMAT_NV12,                         D3DFCS_NV12,          {0,0,0,0},           8,              FALSE, 2,              2,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,                  TRUE,                FALSE,            TRUE,    TRUE,   FALSE,                  FALSE,    },
        { DXGI_FORMAT_P010                                    ,DXGI_FORMAT_P010,                         D3DFCS_P010,          {0,0,0,0},           16,             FALSE, 2,              2,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,                  FALSE,               FALSE,            TRUE,    TRUE,   FALSE,                  FALSE,    },
        { DXGI_FORMAT_P016                                    ,DXGI_FORMAT_P016,                         D3DFCS_P016,          {0,0,0,0},           16,             FALSE, 2,              2,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,                  FALSE,               FALSE,            TRUE,    TRUE,   FALSE,                  FALSE,    },
        { DXGI_FORMAT_420_OPAQUE                              ,DXGI_FORMAT_420_OPAQUE,                   D3DFCS_420_OPAQUE,    {0,0,0,0},           8,              FALSE, 2,              2,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,                  TRUE,                FALSE,            TRUE,    TRUE,   FALSE,                  FALSE,    },
        // YUV 4:2:2 formats                                                                                                                                                                                                                            
        { DXGI_FORMAT_YUY2                                    ,DXGI_FORMAT_YUY2,                         D3DFCS_YUY2,          {0,0,0,0},           16,             FALSE, 2,              1,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,G,B,X,         _UNORM, _UNORM, _UNORM, _TYPELESS,                   FALSE,                  TRUE,                FALSE,            FALSE,   TRUE,   FALSE,                  FALSE,    },
        { DXGI_FORMAT_Y210                                    ,DXGI_FORMAT_Y210,                         D3DFCS_Y210,          {0,0,0,0},           32,             FALSE, 2,              1,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,G,B,X,         _UNORM, _UNORM, _UNORM, _TYPELESS,                   FALSE,                  FALSE,               FALSE,            FALSE,   TRUE,   FALSE,                  FALSE,    },
        { DXGI_FORMAT_Y216                                    ,DXGI_FORMAT_Y216,                         D3DFCS_Y216,          {0,0,0,0},           32,             FALSE, 2,              1,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,G,B,X,         _UNORM, _UNORM, _UNORM, _TYPELESS,                   FALSE,                  FALSE,               FALSE,            FALSE,   TRUE,   FALSE,                  FALSE,    },
        // YUV 4:1:1 formats                                                                                                                                                                                                                            
        { DXGI_FORMAT_NV11                                    ,DXGI_FORMAT_NV11,                         D3DFCS_NV11,          {0,0,0,0},           8,              FALSE, 4,              1,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,                  TRUE,                FALSE,            TRUE,    TRUE,   FALSE,                  FALSE,    },
        // Legacy substream formats                                                                                                                                                                                                                     
        { DXGI_FORMAT_AI44                                    ,DXGI_FORMAT_AI44,                         D3DFCS_AI44,          {0,0,0,0},           8,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,                  TRUE,                FALSE,            FALSE,   TRUE,   FALSE,                  FALSE,    },
        { DXGI_FORMAT_IA44                                    ,DXGI_FORMAT_IA44,                         D3DFCS_IA44,          {0,0,0,0},           8,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,                  TRUE,                FALSE,            FALSE,   TRUE,   FALSE,                  FALSE,    },
        { DXGI_FORMAT_P8                                      ,DXGI_FORMAT_P8,                           D3DFCS_P8,            {0,0,0,0},           8,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,                  TRUE,                FALSE,            FALSE,   TRUE,   FALSE,                  FALSE,    },
        { DXGI_FORMAT_A8P8                                    ,DXGI_FORMAT_A8P8,                         D3DFCS_A8P8,          {0,0,0,0},           16,             FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,                  TRUE,                FALSE,            FALSE,   TRUE,   FALSE,                  FALSE,    },
        { DXGI_FORMAT_B4G4R4A4_UNORM                          ,DXGI_FORMAT_B4G4R4A4_UNORM,               D3DFCS_B4G4R4A4,      {4,4,4,4},           16,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     B,G,R,A,         _UNORM, _UNORM, _UNORM, _UNORM,                      FALSE,                  TRUE,                TRUE,             FALSE,   FALSE,  FALSE,                  FALSE,    },
        { DXGI_FORMAT(116)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(117)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(118)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(119)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(120)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(121)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(122)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },

        { DXGI_FORMAT(123)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },

        { DXGI_FORMAT(124)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(125)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(126)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(127)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(128)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(129)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(DXGI_FORMAT_P208)                       ,DXGI_FORMAT_P208,                         D3DFCS_P208,          {0,0,0,0},           8,              FALSE, 2,              1,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,                  TRUE,                FALSE,            TRUE,    TRUE,   FALSE,                  FALSE,    },
        { DXGI_FORMAT(DXGI_FORMAT_V208)                       ,DXGI_FORMAT_V208,                         D3DFCS_V208,          {0,0,0,0},           8,              FALSE, 1,              2,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,                  TRUE,                FALSE,            TRUE,    TRUE,   FALSE,                  FALSE,    },
        { DXGI_FORMAT(DXGI_FORMAT_V408)                       ,DXGI_FORMAT_V408,                         D3DFCS_V408,          {0,0,0,0},           8,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,                  TRUE,                FALSE,            TRUE,    TRUE,   FALSE,                  FALSE,    },

        { DXGI_FORMAT(133)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(134)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(135)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(136)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(137)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(138)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(139)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(140)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(141)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(142)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(143)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(144)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(145)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(146)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(147)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(148)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(149)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(150)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(151)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(152)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(153)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(154)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(155)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(156)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(157)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(158)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(159)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(160)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(161)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(162)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(163)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(164)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(165)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(166)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(167)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(168)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(169)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(170)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(171)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(172)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(173)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(174)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(175)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(176)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(177)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(178)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(179)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(180)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(181)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(182)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(183)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(184)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(185)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(186)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(187)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },
        { DXGI_FORMAT(188)                                    ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           0,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_NO_TYPE,       X,X,X,X,         _TYPELESS, _TYPELESS, _TYPELESS, _TYPELESS,          FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  TRUE,     },

        { DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE         ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           8,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        { DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE ,DXGI_FORMAT_UNKNOWN,                      D3DFCS_UNKNOWN,       {0,0,0,0},           8,              FALSE, 1,              1,               1,                D3DFL_CUSTOM,     D3DFTL_FULL_TYPE,     R,X,X,X,         _UNORM, _TYPELESS, _TYPELESS, _TYPELESS,             FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        { DXGI_FORMAT_A4B4G4R4_UNORM                          ,DXGI_FORMAT_A4B4G4R4_UNORM,               D3DFCS_A4B4G4R4,      {4,4,4,4},           16,             FALSE, 1,              1,               1,                D3DFL_STANDARD,   D3DFTL_FULL_TYPE,     A,B,G,R,         _UNORM, _UNORM, _UNORM, _UNORM,                      FALSE,                  FALSE,               FALSE,            FALSE,   FALSE,  FALSE,                  FALSE,    },
        //DXGI_FORMAT                                          ParentFormat                              pDefaultFormatCastSet BitsPerComponent[4], BitsPerUnit,    SRGB,  WidthAlignment, HeightAlignment, DepthAlignment,   Layout,           TypeLevel,            ComponentName[4],ComponentInterpretation[4],                          bDX9VertexOrIndexFormat bDX9TextureFormat,   bFloatNormFormat, bPlanar, bYUV    bDependantFormatCastSet bInternal

};

const UINT D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::s_NumFormats = (sizeof(D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::s_FormatDetail) / sizeof(D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::FORMAT_DETAIL));

//---------------------------------------------------------------------------------------------------------------------------------
// GetHighestDefinedFeatureLevel
D3D_FEATURE_LEVEL D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetHighestDefinedFeatureLevel() 
{
    return D3D_FEATURE_LEVEL_12_2; 
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetNumFormats
UINT D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetNumFormats()
{
    return s_NumFormats;
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetFormatTable
const D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::FORMAT_DETAIL* D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetFormatTable()
{
    return &s_FormatDetail[0];
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetFormatTable
BOOL D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::Opaque(DXGI_FORMAT Format)
{
     return Format == DXGI_FORMAT_420_OPAQUE;
}

//---------------------------------------------------------------------------------------------------------------------------------
// FormatExists
bool D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::FormatExists(DXGI_FORMAT Format)
{
    return GetFormat( Format) != (DXGI_FORMAT) -1 ? true : false;
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetDetailTableIndex
UINT D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetDetailTableIndex(DXGI_FORMAT  Format )
{
    if( (UINT)Format < _countof( s_FormatDetail ) )
    {
        assert( s_FormatDetail[(UINT)Format].DXGIFormat == Format );
        return static_cast<UINT>(Format);
    }

    return (UINT)-1;
}

//---------------------------------------------------------------------------------------------------------------------------------
// IsBlockCompressFormat - returns true if format is block compressed. This function is a helper function for GetBitsPerUnit and
// if this function returns true then GetBitsPerUnit returns block size. 
bool D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::IsBlockCompressFormat(DXGI_FORMAT Format)
{
    // Returns true if BC1, BC2, BC3, BC4, BC5, BC6, BC7, or ASTC
    return (Format >= DXGI_FORMAT_BC1_TYPELESS && Format <= DXGI_FORMAT_BC5_SNORM) || 
           (Format >= DXGI_FORMAT_BC6H_TYPELESS && Format <= DXGI_FORMAT_BC7_UNORM_SRGB);
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetByteAlignment 
UINT D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetByteAlignment(DXGI_FORMAT Format)
{
    UINT bits = GetBitsPerUnit(Format);
    if (!IsBlockCompressFormat(Format))
    {
        bits *= GetWidthAlignment(Format)*GetHeightAlignment(Format)*GetDepthAlignment(Format);
    }

    assert((bits & 0x7) == 0); // Unit must be byte-aligned
    return bits >> 3;
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetBitsPerUnitThrow
UINT D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetBitsPerUnitThrow(DXGI_FORMAT Format)
{
    return s_FormatDetail[GetDetailTableIndexThrow( Format )].BitsPerUnit;
}

//---------------------------------------------------------------------------------------------------------------------------------
// FormatExistsInHeader
bool D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::FormatExistsInHeader(DXGI_FORMAT Format, bool bExternalHeader)
{
    const UINT Index = GetDetailTableIndex( Format );
    if (UINT( -1 ) == Index || (bExternalHeader && GetFormatDetail( Format )->bInternal))
    {
        return false;
    }
    else
    {
        return true;
    }
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetName
LPCSTR D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetName(DXGI_FORMAT Format, bool bHideInternalFormats)
{
    const UINT Index = GetDetailTableIndex( Format );
    if (UINT( -1 ) == Index || (bHideInternalFormats && GetFormatDetail( Format )->bInternal))
    {
        return "Unrecognized";
    }
    else
    {
        return s_FormatNames[ Index ];
    }
}

//---------------------------------------------------------------------------------------------------------------------------------
// IsSRGBFormat
bool D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::IsSRGBFormat(DXGI_FORMAT Format)
{
    const UINT Index = GetDetailTableIndex(Format);
    if(UINT( -1 ) == Index )
    {
        return false;
    }

    return s_FormatDetail[Index].SRGBFormat ? true : false;
}

//----------------------------------------------------------------------------
// DivideAndRoundUp
inline HRESULT DivideAndRoundUp(UINT dividend, UINT divisor, _Out_ UINT& result)
{
    HRESULT hr = S_OK;

    UINT adjustedDividend;
    hr = Safe_UIntAdd(dividend, (divisor - 1), &adjustedDividend);

    result = SUCCEEDED(hr) ? (adjustedDividend / divisor) : 0;

    return hr;
}

//----------------------------------------------------------------------------
// CalculateExtraPlanarRows
HRESULT D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::CalculateExtraPlanarRows(
    DXGI_FORMAT format,
    UINT plane0Height,
    _Out_ UINT& totalHeight
    )
{
    if (!D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::Planar(format))
    {
        totalHeight = plane0Height;
        return S_OK;
    }

    // blockWidth, blockHeight, and blockSize only reflect the size of plane 0.  Each planar format has additonal planes that must
    // be counted.  Each format increases size by another 0.5x, 1x, or 2x.  Grab the number of "half allocation" increments so integer
    // math can be used to calculate the extra size.
    UINT extraHalfHeight;
    UINT round;

    switch (D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetParentFormat(format))
    {
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
    case DXGI_FORMAT_420_OPAQUE:
        extraHalfHeight = 1;
        round = 1;
        break;

    case DXGI_FORMAT_NV11:
    case DXGI_FORMAT_P208:
        extraHalfHeight = 2;
        round = 0;
        break;
    case DXGI_FORMAT_V208:
        extraHalfHeight = 2;
        round = 1;
        break;

    case DXGI_FORMAT_V408:
        extraHalfHeight = 4;
        round = 0;
        break;

    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_R32G8X24_TYPELESS:
        totalHeight = plane0Height;
        return S_OK;

    default:
        assert(false);
        return S_OK;
    }

    UINT extraPlaneHeight;
    if (FAILED(Safe_UIntMult(plane0Height, extraHalfHeight, &extraPlaneHeight))
        || FAILED(Safe_UIntAdd(extraPlaneHeight, round, &extraPlaneHeight))
        || FAILED(Safe_UIntAdd(plane0Height, (extraPlaneHeight >> 1), &totalHeight)))
    {
        return INTSAFE_E_ARITHMETIC_OVERFLOW;
    }

    return S_OK;
}

//----------------------------------------------------------------------------
// CalculateResourceSize
HRESULT D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::CalculateResourceSize(
    UINT width, 
    UINT height, 
    UINT depth,
    DXGI_FORMAT format, 
    UINT mipLevels, 
    UINT subresources, 
    _Out_ SIZE_T& totalByteSize, 
    _Out_writes_opt_(subresources) D3D12_MEMCPY_DEST *pDst)
{
    UINT tableIndex = GetDetailTableIndexNoThrow( format );
    const FORMAT_DETAIL& formatDetail = s_FormatDetail[tableIndex];

    bool fIsBlockCompressedFormat = IsBlockCompressFormat(format );

    // No format currently requires depth alignment.
    assert(formatDetail.DepthAlignment == 1);

    UINT subWidth = width;
    UINT subHeight = height;
    UINT subDepth = depth;
    for (UINT s = 0, iM = 0; s < subresources; ++s)
    {
        UINT blockWidth;
        if (FAILED(DivideAndRoundUp(subWidth, formatDetail.WidthAlignment, /*_Out_*/ blockWidth)))
        {
            return INTSAFE_E_ARITHMETIC_OVERFLOW;
        }

        UINT blockSize, blockHeight;
        if (fIsBlockCompressedFormat)
        {
            if (FAILED(DivideAndRoundUp(subHeight, formatDetail.HeightAlignment, /*_Out_*/ blockHeight)))
            {
                return INTSAFE_E_ARITHMETIC_OVERFLOW;
            }

            // Block Compressed formats use BitsPerUnit as block size.
            blockSize = formatDetail.BitsPerUnit;
        }
        else
        {
            // The height must *not* be aligned to HeightAlign.  As there is no plane pitch/stride, the expectation is that the 2nd plane
            // begins immediately after the first.  The only formats with HeightAlignment other than 1 are planar or block compressed, and
            // block compressed is handled above.
            assert(formatDetail.bPlanar || formatDetail.HeightAlignment == 1);
            blockHeight = subHeight;

            // Combined with the division os subWidth by the width alignment above, this helps achieve rounding the stride up to an even multiple of
            // block width.  This is especially important for formats like NV12 and P208 whose chroma plane is wider than the luma.
            blockSize = formatDetail.BitsPerUnit * formatDetail.WidthAlignment;
        }

        if (DXGI_FORMAT_UNKNOWN == formatDetail.DXGIFormat)
        {
            blockSize = 8;
        }

        // Convert block width size to bytes.
        assert((blockSize & 0x7) == 0);
        blockSize = blockSize >> 3;

        if (formatDetail.bPlanar)
        {
            if (FAILED(CalculateExtraPlanarRows(format, blockHeight, /*_Out_*/ blockHeight)))
            {
                return INTSAFE_E_ARITHMETIC_OVERFLOW;
            }
        }

        // Calculate rowPitch, depthPitch, and total subresource size.
        UINT rowPitch, depthPitch;
   
        if (   FAILED(Safe_UIntMult(blockWidth, blockSize, &rowPitch))
            || FAILED(Safe_UIntMult(blockHeight, rowPitch, &depthPitch)))
        {
            return INTSAFE_E_ARITHMETIC_OVERFLOW;
        } 
        SIZE_T subresourceByteSize = subDepth * depthPitch;

        if (pDst)
        {
            D3D12_MEMCPY_DEST& dst = pDst[s];

            // This data will be returned straight from the API to satisfy Map. So, strides/ alignment must be API-correct.
            dst.pData = reinterpret_cast<void*>(totalByteSize);
            assert(s != 0 || dst.pData == nullptr);

            dst.RowPitch = rowPitch; 
            dst.SlicePitch = depthPitch;
        }

        // Align the subresource size.
        static_assert((MAP_ALIGN_REQUIREMENT & (MAP_ALIGN_REQUIREMENT - 1)) == 0, "This code expects MAP_ALIGN_REQUIREMENT to be a power of 2.");
        
        SIZE_T subresourceByteSizeAligned = subresourceByteSize + MAP_ALIGN_REQUIREMENT - 1;
        subresourceByteSizeAligned = subresourceByteSizeAligned & ~(MAP_ALIGN_REQUIREMENT - 1);
        totalByteSize = totalByteSize + subresourceByteSizeAligned;


        // Iterate over mip levels and array elements
        if (++iM >= mipLevels)
        {
            iM = 0;

            subWidth = width;
            subHeight = height;
            subDepth = depth;
        }
        else
        {
            subWidth /= (1 == subWidth ? 1 : 2);
            subHeight /= (1 == subHeight ? 1 : 2);
            subDepth /= (1 == subDepth ? 1 : 2);
        }
    }

    return S_OK;
}

inline bool IsPow2( UINT Val )
{
    return 0 == (Val & (Val - 1));
}

// This helper function calculates the Row Pitch for a given format. For Planar formats this function returns
// the row major RowPitch of the resource. The RowPitch is the same for all the planes. For Planar 
// also use the CalculateExtraPlanarRows function to calculate the corresonding height or use the CalculateMinimumRowMajorSlicePitch
// function. For Block Compressed Formats, this function returns the RowPitch of a row of blocks. For packed subsampled formats and other formats, 
// this function returns the row pitch of one single row of pixels.
HRESULT D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::CalculateMinimumRowMajorRowPitch(DXGI_FORMAT Format, UINT Width, _Out_ UINT &RowPitch)
{
    // Early out for DXGI_FORMAT_UNKNOWN special case.
    if (Format == DXGI_FORMAT_UNKNOWN)
    {
        RowPitch = Width;
        return S_OK;
    }

    UINT WidthAlignment = GetWidthAlignment(Format);

    UINT NumUnits;
    if (IsBlockCompressFormat(Format))
    {
        // This function calculates the minimum stride needed for a block row when the format 
        // is block compressed.The GetBitsPerUnit value stored in the format table indicates 
        // the size of a compressed block for block compressed formats.
        assert(WidthAlignment != 0);
        if (FAILED(DivideAndRoundUp(Width, WidthAlignment, NumUnits)))
        {
            return INTSAFE_E_ARITHMETIC_OVERFLOW;
        }
    }
    else
    {
        // All other formats must have strides aligned to their width alignment requirements.
        // The Width may not be aligned to the WidthAlignment.  This is not an error for this 
        // function as we expect to allow formats like NV12 to have odd dimensions in the future.

        // The following alignement code expects only pow2 alignment requirements.  Only block 
        // compressed formats currently have non-pow2 alignment requriements.
        assert(IsPow2(WidthAlignment));

        UINT Mask = WidthAlignment - 1;
        if (FAILED(Safe_UIntAdd(Width, Mask, &NumUnits)))
        {
            return INTSAFE_E_ARITHMETIC_OVERFLOW;
        }

        NumUnits &= ~Mask;
    }

    if (FAILED(Safe_UIntMult(NumUnits, GetBitsPerUnit(Format), &RowPitch)))
    {
        return INTSAFE_E_ARITHMETIC_OVERFLOW;
    }

    // This must to always be Byte aligned.
    assert((RowPitch & 7) == 0);
    RowPitch >>= 3;

    return S_OK;
}

// This helper function calculates the SlicePitch for a given format. For Planar formats the slice pitch includes the extra 
// planes.
HRESULT D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::CalculateMinimumRowMajorSlicePitch(DXGI_FORMAT Format, UINT TightRowPitch, UINT Height, _Out_ UINT &SlicePitch)
{
    if (Planar(Format))
    {
        UINT PlanarHeight;
        if (FAILED(CalculateExtraPlanarRows(Format, Height, PlanarHeight)))
        {
            return INTSAFE_E_ARITHMETIC_OVERFLOW;
        }

        return Safe_UIntMult(TightRowPitch, PlanarHeight, &SlicePitch);
    }
    else if (Format == DXGI_FORMAT_UNKNOWN)
    {
        return Safe_UIntMult(TightRowPitch, Height, &SlicePitch);
    }

    UINT HeightAlignment = GetHeightAlignment(Format);

    // Caution assert to make sure that no new format breaks this assumption that all HeightAlignment formats are BC or Planar.
    // This is to make sure that Height handled correctly for this calculation.
    assert(HeightAlignment == 1 || IsBlockCompressFormat(Format));

    UINT HeightOfPacked;
    if (FAILED(DivideAndRoundUp(Height, HeightAlignment, HeightOfPacked)))
    {
        return INTSAFE_E_ARITHMETIC_OVERFLOW;
    }

    if (FAILED(Safe_UIntMult(HeightOfPacked, TightRowPitch, &SlicePitch)))
    {
        return INTSAFE_E_ARITHMETIC_OVERFLOW;
    }

    return S_OK;
}



//---------------------------------------------------------------------------------------------------------------------------------
// GetBitsPerUnit - returns bits per pixel unless format is a block compress format then it returns bits per block. 
// use IsBlockCompressFormat() to determine if block size is returned.
UINT D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetBitsPerUnit(DXGI_FORMAT Format)
{
    return s_FormatDetail[GetDetailTableIndexNoThrow( Format )].BitsPerUnit;
}

//---------------------------------------------------------------------------------------------------------------------------------
UINT D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetWidthAlignment(DXGI_FORMAT Format)
{
    return s_FormatDetail[GetDetailTableIndexNoThrow( Format )].WidthAlignment;
}

UINT D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetHeightAlignment(DXGI_FORMAT Format)
{
    return s_FormatDetail[GetDetailTableIndexNoThrow( Format )].HeightAlignment;
}

UINT D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetDepthAlignment(DXGI_FORMAT Format)
{
    return s_FormatDetail[GetDetailTableIndexNoThrow( Format )].DepthAlignment;
}
//---------------------------------------------------------------------------------------------------------------------------------
// GetFormat
DXGI_FORMAT D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetFormat(SIZE_T Index)
{
    if( Index < GetNumFormats() )
    {
        return s_FormatDetail[Index].DXGIFormat;
    }
    return (DXGI_FORMAT)-1;
}

//---------------------------------------------------------------------------------------------------------------------------------
// CanBeCastEvenFullyTyped
bool D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::CanBeCastEvenFullyTyped(DXGI_FORMAT Format, D3D_FEATURE_LEVEL fl)
{
    //SRGB can be cast away/back, and XR_BIAS can be cast to/from UNORM
    switch(fl)
    {
    case D3D_FEATURE_LEVEL_1_0_GENERIC:
    case D3D_FEATURE_LEVEL_1_0_CORE:
        return false;
    default:
        break;
    }
    switch( Format )
    {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        return true;
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
        return fl >= D3D_FEATURE_LEVEL_10_0;
    default:
        return false;
    }
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetFormatDetail
const D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::FORMAT_DETAIL* D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetFormatDetail( DXGI_FORMAT  Format )
{
    const UINT Index = GetDetailTableIndex(Format);
    if(UINT( -1 ) == Index )
    {
        return nullptr;
    }
  
  return &s_FormatDetail[ Index ];
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetBitsPerStencil
UINT D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetBitsPerStencil(DXGI_FORMAT  Format)
{
    const UINT Index = GetDetailTableIndexThrow( Format );
    if( (s_FormatDetail[Index].TypeLevel != D3DFTL_PARTIAL_TYPE) &&
        (s_FormatDetail[Index].TypeLevel != D3DFTL_FULL_TYPE) )
    {
        return 0;
    }
    for( UINT comp = 0; comp < 4; comp++ )
    {
        D3D_FORMAT_COMPONENT_NAME name = D3DFCN_D;
        switch(comp)
        {
        case 0: name = s_FormatDetail[Index].ComponentName0; break;
        case 1: name = s_FormatDetail[Index].ComponentName1; break;
        case 2: name = s_FormatDetail[Index].ComponentName2; break;
        case 3: name = s_FormatDetail[Index].ComponentName3; break;
        }
        if( name == D3DFCN_S )
        {
            return s_FormatDetail[Index].BitsPerComponent[comp];
        }
    }
    return 0;
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetFormatReturnTypes
void    D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetFormatReturnTypes(
        DXGI_FORMAT                            Format,
        D3D_FORMAT_COMPONENT_INTERPRETATION* pInterpretations  ) // return array with 4 entries
{
    const UINT Index = GetDetailTableIndexThrow(Format);
    pInterpretations[D3D10_SB_4_COMPONENT_R] = s_FormatDetail[Index].ComponentInterpretation0;
    pInterpretations[D3D10_SB_4_COMPONENT_G] = s_FormatDetail[Index].ComponentInterpretation1;
    pInterpretations[D3D10_SB_4_COMPONENT_B] = s_FormatDetail[Index].ComponentInterpretation2;
    pInterpretations[D3D10_SB_4_COMPONENT_A] = s_FormatDetail[Index].ComponentInterpretation3;
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetAddressingBitsPerAlignedSize
UINT8 D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetAddressingBitsPerAlignedSize(DXGI_FORMAT Format)
{
    UINT byteAlignment = GetByteAlignment(Format);
    UINT8 addressBitsPerElement = 0;
    
    switch(byteAlignment)
    {
        case 1:  addressBitsPerElement  = 0; break;
        case 2:  addressBitsPerElement  = 1; break;
        case 4:  addressBitsPerElement  = 2; break;
        case 8:  addressBitsPerElement  = 3; break;
        case 16: addressBitsPerElement  = 4; break;

        // The format is not supported
        default: return UINT8( -1 );
    }
  
    return addressBitsPerElement;
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetNumComponentsInFormat
UINT D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetNumComponentsInFormat( DXGI_FORMAT  Format )
{
    UINT n = 0;
    const UINT Index = GetDetailTableIndexThrow(Format);
    for( UINT comp = 0; comp < 4; comp++ )
    {
        D3D_FORMAT_COMPONENT_NAME name = D3DFCN_D;
        switch(comp)
        {
        case 0: name = s_FormatDetail[Index].ComponentName0; break;
        case 1: name = s_FormatDetail[Index].ComponentName1; break;
        case 2: name = s_FormatDetail[Index].ComponentName2; break;
        case 3: name = s_FormatDetail[Index].ComponentName3; break;
        }
        if( name != D3DFCN_X )
        {
            n++;
        }
    }
    return n;
}

//---------------------------------------------------------------------------------------------------------------------------------
// Sequential2AbsoluteComponentIndex
UINT D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::Sequential2AbsoluteComponentIndex( DXGI_FORMAT  Format, UINT SequentialComponentIndex)
{
    UINT n = 0;
    const UINT Index = GetDetailTableIndexThrow(Format);
    for( UINT comp = 0; comp < 4; comp++ )
    {
        D3D_FORMAT_COMPONENT_NAME name = static_cast<D3D_FORMAT_COMPONENT_NAME>(0);
        switch(comp)
        {
        case 0: name = s_FormatDetail[Index].ComponentName0; break;
        case 1: name = s_FormatDetail[Index].ComponentName1; break;
        case 2: name = s_FormatDetail[Index].ComponentName2; break;
        case 3: name = s_FormatDetail[Index].ComponentName3; break;
        }
        if( name != D3DFCN_X )
        {
            if (SequentialComponentIndex == n)
            {
                return comp;
            }
            n++;
        }
    }
    return UINT(-1);
}

//---------------------------------------------------------------------------------------------------------------------------------
// Depth Only Format
bool D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::DepthOnlyFormat(DXGI_FORMAT Format)
{
    switch( Format )
    {
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_D16_UNORM:
        return true;
    default:
        return false;
    }
}

//---------------------------------------------------------------------------------------------------------------------------------
bool D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::SupportsSamplerFeedback(DXGI_FORMAT Format)
{
    switch (Format)
    {
    case DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE:
    case DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE:
        return true;
    default:
        return false;
    }
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetParentFormat
DXGI_FORMAT D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetParentFormat(DXGI_FORMAT Format)
{
    return s_FormatDetail[Format].ParentFormat;
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetFormatCastSet
const DXGI_FORMAT* D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetFormatCastSet(DXGI_FORMAT Format)
{
    return s_FormatDetail[Format].pDefaultFormatCastSet;
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetTypeLevel
D3D_FORMAT_TYPE_LEVEL D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetTypeLevel(DXGI_FORMAT Format)
{
    return s_FormatDetail[GetDetailTableIndexNoThrow( Format )].TypeLevel;
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetLayout
D3D_FORMAT_LAYOUT D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetLayout(DXGI_FORMAT Format)
{
    return s_FormatDetail[GetDetailTableIndexNoThrow( Format )].Layout;
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetComponentName
D3D_FORMAT_COMPONENT_NAME D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetComponentName(DXGI_FORMAT Format, UINT AbsoluteComponentIndex)
{
    D3D_FORMAT_COMPONENT_NAME name;
    switch( AbsoluteComponentIndex )
    {
    case 0: name = s_FormatDetail[GetDetailTableIndexNoThrow( Format )].ComponentName0; break;
    case 1: name = s_FormatDetail[GetDetailTableIndexNoThrow( Format )].ComponentName1; break;
    case 2: name = s_FormatDetail[GetDetailTableIndexNoThrow( Format )].ComponentName2; break;
    case 3: name = s_FormatDetail[GetDetailTableIndexNoThrow( Format )].ComponentName3; break;
    default: throw E_FAIL;
    }
    return name;
}
//---------------------------------------------------------------------------------------------------------------------------------
// GetBitsPerComponent
UINT D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetBitsPerComponent(DXGI_FORMAT Format, UINT AbsoluteComponentIndex)
{
    if( AbsoluteComponentIndex > 3 )
    {
        throw E_FAIL;
    }
    return s_FormatDetail[GetDetailTableIndexNoThrow( Format )].BitsPerComponent[AbsoluteComponentIndex];
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetFormatComponentInterpretation
D3D_FORMAT_COMPONENT_INTERPRETATION D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetFormatComponentInterpretation(DXGI_FORMAT Format, UINT AbsoluteComponentIndex)
{
    D3D_FORMAT_COMPONENT_INTERPRETATION interp {};

    switch( AbsoluteComponentIndex )
    {
    case 0: interp = s_FormatDetail[GetDetailTableIndexNoThrow( Format )].ComponentInterpretation0; break;
    case 1: interp = s_FormatDetail[GetDetailTableIndexNoThrow( Format )].ComponentInterpretation1; break;
    case 2: interp = s_FormatDetail[GetDetailTableIndexNoThrow( Format )].ComponentInterpretation2; break;
    case 3: interp = s_FormatDetail[GetDetailTableIndexNoThrow( Format )].ComponentInterpretation3; break;
//    default: throw E_FAIL;
    }
    return interp;
}

//---------------------------------------------------------------------------------------------------------------------------------
// Planar
BOOL D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::Planar(DXGI_FORMAT Format)
{
    return s_FormatDetail[GetDetailTableIndexNoThrow( Format )].bPlanar;
}

//---------------------------------------------------------------------------------------------------------------------------------
// Non-opaque Planar
BOOL D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::NonOpaquePlanar(DXGI_FORMAT Format)
{
    return Planar(Format) && !Opaque(Format);
}

//---------------------------------------------------------------------------------------------------------------------------------
// YUV
BOOL D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::YUV(DXGI_FORMAT Format)
{
    return s_FormatDetail[GetDetailTableIndexNoThrow( Format )].bYUV;
}

//---------------------------------------------------------------------------------------------------------------------------------
// Format family supports stencil
bool D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::FamilySupportsStencil(DXGI_FORMAT Format)
{
    switch( GetParentFormat(Format) )
    {
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_R24G8_TYPELESS:
        return true;
    default:
        return false;
    }
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetDetailTableIndexThrow
UINT D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetDetailTableIndexThrow(DXGI_FORMAT  Format)
{
    UINT Index = GetDetailTableIndex( Format );
    if(UINT( -1 ) == Index )
    {
        throw E_FAIL;
    }
    return Index;
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetDetailTableIndexNoThrow
UINT D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetDetailTableIndexNoThrow(DXGI_FORMAT  Format)
{
    UINT Index = GetDetailTableIndex( Format );
    assert(UINT( -1 ) != Index ); // Needs to be validated externally.
    return Index;
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetYCbCrChromaSubsampling
void D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetYCbCrChromaSubsampling(
    DXGI_FORMAT Format, 
    _Out_ UINT& HorizontalSubsampling, 
    _Out_ UINT& VerticalSubsampling
    )
{
    switch( Format)
    {
        // YCbCr 4:2:0 
        case DXGI_FORMAT_NV12:
        case DXGI_FORMAT_P010:
        case DXGI_FORMAT_P016:
        case DXGI_FORMAT_420_OPAQUE:
            HorizontalSubsampling =  2;
            VerticalSubsampling = 2;
            break;

        // YCbCr 4:2:2
        case DXGI_FORMAT_P208:
        case DXGI_FORMAT_YUY2:
        case DXGI_FORMAT_Y210:
            HorizontalSubsampling =  2;
            VerticalSubsampling = 1;
            break;

        // YCbCr 4:4:0
        case DXGI_FORMAT_V208:
            HorizontalSubsampling =  1;
            VerticalSubsampling = 2;
            break;

        // YCbCr 4:4:4
        case DXGI_FORMAT_AYUV:
        case DXGI_FORMAT_V408:
        case DXGI_FORMAT_Y410:
        case DXGI_FORMAT_Y416:
            // Fallthrough

        // YCbCr palletized  4:4:4:
        case DXGI_FORMAT_AI44:
        case DXGI_FORMAT_IA44:
        case DXGI_FORMAT_P8:
        case DXGI_FORMAT_A8P8:
            HorizontalSubsampling =  1;
            VerticalSubsampling = 1;
            break;

        // YCbCr 4:1:1
        case DXGI_FORMAT_NV11:
            HorizontalSubsampling =  4;
            VerticalSubsampling = 1;
            break;

        default:
            // All YCbCr formats should be in this list.
            assert( !YUV(Format) );
            HorizontalSubsampling =  1;
            VerticalSubsampling = 1;
            break;
    };
}

//---------------------------------------------------------------------------------------------------------------------------------
// Plane count for non-opaque planar formats
UINT D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::NonOpaquePlaneCount(DXGI_FORMAT Format)
{
    if (!D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::NonOpaquePlanar(Format))
    {
        return 1;
    }

    // V208 and V408 are the only 3-plane formats. 
    return (Format == DXGI_FORMAT_V208 || Format == DXGI_FORMAT_V408) ? 3 : 2;
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetTileShape
//
// Retrieve 64K Tiled Resource tile shape
void D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetTileShape(
    D3D12_TILE_SHAPE* pTileShape, 
    DXGI_FORMAT Format, 
    D3D12_RESOURCE_DIMENSION Dimension, 
    UINT SampleCount
    )
{
    UINT BPU = GetBitsPerUnit(Format);

    switch(Dimension)
    {
    case D3D12_RESOURCE_DIMENSION_UNKNOWN:
    case D3D12_RESOURCE_DIMENSION_BUFFER:
    case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
        {
            assert(!IsBlockCompressFormat(Format));
            pTileShape->WidthInTexels = (BPU == 0) ? D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES : D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES*8 / BPU;
            pTileShape->HeightInTexels = 1;
            pTileShape->DepthInTexels = 1;
        }
        break;
    case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
        {
            if (IsBlockCompressFormat(Format))
            {
                // Currently only supported block sizes are 64 and 128.
                // These equations calculate the size in texels for a tile. It relies on the fact that 64 * 64 blocks fit in a tile if the block size is 128 bits.
                assert(BPU == 64 || BPU == 128);
                pTileShape->WidthInTexels = 64 * GetWidthAlignment(Format);
                pTileShape->HeightInTexels = 64 * GetHeightAlignment(Format);
                pTileShape->DepthInTexels = 1;
                if (BPU == 64)
                {
                    // If bits per block are 64 we double width so it takes up the full tile size.
                    // This is only true for BC1 and BC4
                    assert((Format >= DXGI_FORMAT_BC1_TYPELESS && Format <= DXGI_FORMAT_BC1_UNORM_SRGB) ||
                           (Format >= DXGI_FORMAT_BC4_TYPELESS && Format <= DXGI_FORMAT_BC4_SNORM));
                    pTileShape->WidthInTexels *= 2;
                }
            }
            else
            {
                pTileShape->DepthInTexels = 1;
                if (BPU <= 8)
                {
                    pTileShape->WidthInTexels = 256;
                    pTileShape->HeightInTexels = 256;
                }
                else if (BPU <= 16)
                {
                    pTileShape->WidthInTexels = 256;
                    pTileShape->HeightInTexels = 128;
                }
                else if (BPU <= 32)
                {
                    pTileShape->WidthInTexels = 128;
                    pTileShape->HeightInTexels = 128;
                }
                else if (BPU <= 64)
                {
                    pTileShape->WidthInTexels = 128;
                    pTileShape->HeightInTexels = 64;
                }
                else if (BPU <= 128)
                {
                    pTileShape->WidthInTexels = 64;
                    pTileShape->HeightInTexels = 64;
                }
                else
                {
                    ASSUME( FALSE );
                }
                
                if (SampleCount <= 1)
                { /* Do nothing */ }
                else if (SampleCount <= 2)
                {
                    pTileShape->WidthInTexels /= 2;
                    pTileShape->HeightInTexels /= 1;
                }
                else if (SampleCount <= 4)
                {
                    pTileShape->WidthInTexels /= 2;
                    pTileShape->HeightInTexels /= 2;
                }
                else if (SampleCount <= 8)
                {
                    pTileShape->WidthInTexels /= 4;
                    pTileShape->HeightInTexels /= 2;
                }
                else if (SampleCount <= 16)
                {
                    pTileShape->WidthInTexels /= 4;
                    pTileShape->HeightInTexels /= 4;
                }
                else
                {
                    ASSUME( FALSE );
                }
            }
        }
        break;
    case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
        {
            if (IsBlockCompressFormat(Format))
            {
                // Currently only supported block sizes are 64 and 128.
                // These equations calculate the size in texels for a tile. It relies on the fact that 16*16*16 blocks fit in a tile if the block size is 128 bits.
                assert(BPU == 64 || BPU == 128);
                pTileShape->WidthInTexels = 16 * GetWidthAlignment(Format);
                pTileShape->HeightInTexels = 16 * GetHeightAlignment(Format);
                pTileShape->DepthInTexels = 16 * GetDepthAlignment(Format);
                if (BPU == 64)
                {
                    // If bits per block are 64 we double width so it takes up the full tile size.
                    // This is only true for BC1 and BC4
                    assert((Format >= DXGI_FORMAT_BC1_TYPELESS && Format <= DXGI_FORMAT_BC1_UNORM_SRGB) ||
                           (Format >= DXGI_FORMAT_BC4_TYPELESS && Format <= DXGI_FORMAT_BC4_SNORM));
                    pTileShape->WidthInTexels *= 2;
                }
            }
            else if (Format == DXGI_FORMAT_R8G8_B8G8_UNORM || Format == DXGI_FORMAT_G8R8_G8B8_UNORM)
            {
                //RGBG and GRGB are treated as 2x1 block format
                pTileShape->WidthInTexels = 64;
                pTileShape->HeightInTexels = 32;
                pTileShape->DepthInTexels = 16;
            }
            else
            {
                // Not a block format so BPU is bits per pixel.
                assert(GetWidthAlignment(Format) == 1 && GetHeightAlignment(Format) == 1 && GetDepthAlignment(Format));
                switch(BPU)
                {
                case 8:
                    pTileShape->WidthInTexels = 64;
                    pTileShape->HeightInTexels = 32;
                    pTileShape->DepthInTexels = 32;
                    break;
                case 16:
                    pTileShape->WidthInTexels = 32;
                    pTileShape->HeightInTexels = 32;
                    pTileShape->DepthInTexels = 32;
                    break;
                case 32:
                    pTileShape->WidthInTexels = 32;
                    pTileShape->HeightInTexels = 32;
                    pTileShape->DepthInTexels = 16;
                    break;
                case 64:
                    pTileShape->WidthInTexels = 32;
                    pTileShape->HeightInTexels = 16;
                    pTileShape->DepthInTexels = 16;
                    break;
                case 128:
                    pTileShape->WidthInTexels = 16;
                    pTileShape->HeightInTexels = 16;
                    pTileShape->DepthInTexels = 16;
                    break;
                }
            }
            break;
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------------------
// Get4KTileShape
//
// Retrieve 4K Tiled Resource tile shape
void D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::Get4KTileShape(
    D3D12_TILE_SHAPE* pTileShape, 
    DXGI_FORMAT Format, 
    D3D12_RESOURCE_DIMENSION Dimension, 
    UINT SampleCount
    )
{
    UINT BPU = GetBitsPerUnit(Format);

    switch(Dimension)
    {
    case D3D12_RESOURCE_DIMENSION_UNKNOWN:
    case D3D12_RESOURCE_DIMENSION_BUFFER:
    case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
        {
            assert(!IsBlockCompressFormat(Format));
            pTileShape->WidthInTexels = (BPU == 0) ? 4096 : 4096*8 / BPU;
            pTileShape->HeightInTexels = 1;
            pTileShape->DepthInTexels = 1;
        }
        break;
    case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
        {
            pTileShape->DepthInTexels = 1;
            if (IsBlockCompressFormat(Format))
            {
                // Currently only supported block sizes are 64 and 128.
                // These equations calculate the size in texels for a tile. It relies on the fact that 16*16*16 blocks fit in a tile if the block size is 128 bits.
                assert(BPU == 64 || BPU == 128);
                pTileShape->WidthInTexels = 16 * GetWidthAlignment(Format);
                pTileShape->HeightInTexels = 16 * GetHeightAlignment(Format);
                if (BPU == 64)
                {
                    // If bits per block are 64 we double width so it takes up the full tile size.
                    // This is only true for BC1 and BC4
                    assert((Format >= DXGI_FORMAT_BC1_TYPELESS && Format <= DXGI_FORMAT_BC1_UNORM_SRGB) ||
                           (Format >= DXGI_FORMAT_BC4_TYPELESS && Format <= DXGI_FORMAT_BC4_SNORM));
                    pTileShape->WidthInTexels *= 2;
                }
            }
            else
            {
                if (BPU <= 8)
                {
                    pTileShape->WidthInTexels = 64;
                    pTileShape->HeightInTexels = 64;
                }
                else if (BPU <= 16)
                {
                    pTileShape->WidthInTexels = 64;
                    pTileShape->HeightInTexels = 32;
                }
                else if (BPU <= 32)
                {
                    pTileShape->WidthInTexels = 32;
                    pTileShape->HeightInTexels = 32;
                }
                else if (BPU <= 64)
                {
                    pTileShape->WidthInTexels = 32;
                    pTileShape->HeightInTexels = 16;
                }
                else if (BPU <= 128)
                {
                    pTileShape->WidthInTexels = 16;
                    pTileShape->HeightInTexels = 16;
                }
                else
                {
                    ASSUME( FALSE );
                }

                if (SampleCount <= 1)
                { /* Do nothing */ }
                else if (SampleCount <= 2)
                {
                    pTileShape->WidthInTexels /= 2;
                    pTileShape->HeightInTexels /= 1;
                }
                else if (SampleCount <= 4)
                {
                    pTileShape->WidthInTexels /= 2;
                    pTileShape->HeightInTexels /= 2;
                }
                else if (SampleCount <= 8)
                {
                    pTileShape->WidthInTexels /= 4;
                    pTileShape->HeightInTexels /= 2;
                }
                else if (SampleCount <= 16)
                {
                    pTileShape->WidthInTexels /= 4;
                    pTileShape->HeightInTexels /= 4;
                }
                else
                {
                    ASSUME( FALSE );
                }
                    
                assert(GetWidthAlignment(Format) == 1);
                assert(GetHeightAlignment(Format) == 1);
                assert(GetDepthAlignment(Format) == 1);
            }

            break;
        }
    case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
        {
            if (IsBlockCompressFormat(Format))
            {
                // Currently only supported block sizes are 64 and 128.
                // These equations calculate the size in texels for a tile. It relies on the fact that 16*16*16 blocks fit in a tile if the block size is 128 bits.
                assert(BPU == 64 || BPU == 128);
                pTileShape->WidthInTexels = 8 * GetWidthAlignment(Format);
                pTileShape->HeightInTexels = 8 * GetHeightAlignment(Format);
                pTileShape->DepthInTexels = 4;
                if (BPU == 64)
                {
                    // If bits per block are 64 we double width so it takes up the full tile size.
                    // This is only true for BC1 and BC4
                    assert((Format >= DXGI_FORMAT_BC1_TYPELESS && Format <= DXGI_FORMAT_BC1_UNORM_SRGB) ||
                           (Format >= DXGI_FORMAT_BC4_TYPELESS && Format <= DXGI_FORMAT_BC4_SNORM));
                    pTileShape->DepthInTexels *= 2;
                }
            }
            else
            {
                if (BPU <= 8)
                {
                    pTileShape->WidthInTexels = 16;
                    pTileShape->HeightInTexels = 16;
                    pTileShape->DepthInTexels = 16;
                }
                else if (BPU <= 16)
                {
                    pTileShape->WidthInTexels = 16;
                    pTileShape->HeightInTexels = 16;
                    pTileShape->DepthInTexels = 8;
                }
                else if (BPU <= 32)
                {
                    pTileShape->WidthInTexels = 16;
                    pTileShape->HeightInTexels = 8;
                    pTileShape->DepthInTexels = 8;
                }
                else if (BPU <= 64)
                {
                    pTileShape->WidthInTexels = 8;
                    pTileShape->HeightInTexels = 8;
                    pTileShape->DepthInTexels = 8;
                }
                else if (BPU <= 128)
                {
                    pTileShape->WidthInTexels = 8;
                    pTileShape->HeightInTexels = 8;
                    pTileShape->DepthInTexels = 4;
                }
                else
                {
                    ASSUME( FALSE );
                }

                assert(GetWidthAlignment(Format) == 1);
                assert(GetHeightAlignment(Format) == 1);
                assert(GetDepthAlignment(Format) == 1);
            }
        }
        break;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
// GetPlaneSliceFromViewFormat
// Maps resource format + view format to a plane index for resource formats where the plane index can be inferred from this information.
// For planar formats where the plane index is ambiguous given this information (examples: V208, V408), this function returns 0.
// This function returns 0 for non-planar formats.
UINT8 D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetPlaneSliceFromViewFormat(
    DXGI_FORMAT ResourceFormat,
    DXGI_FORMAT ViewFormat
    )
{
    switch(GetParentFormat(ResourceFormat))
    {
    case DXGI_FORMAT_R24G8_TYPELESS:
        switch(ViewFormat)
        {
        case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
            return 0;
        case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
            return 1;
        default:
            ASSUME( false );
        }
        break;
    case DXGI_FORMAT_R32G8X24_TYPELESS:
        switch(ViewFormat)
        {
        case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
            return 0;
        case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
            return 1;
        default:
            ASSUME( false );
        }
        break;
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_NV11:
    case DXGI_FORMAT_P208:
        switch(ViewFormat)
        {
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_R8_UINT:
            return 0;
        case DXGI_FORMAT_R8G8_UNORM:
        case DXGI_FORMAT_R8G8_UINT:
            return 1;
        default:
            ASSUME( false );
        }
        break;
    case DXGI_FORMAT_P016:
    case DXGI_FORMAT_P010:
        switch(ViewFormat)
        {
        case DXGI_FORMAT_R16_UNORM:
        case DXGI_FORMAT_R16_UINT:
            return 0;
        case DXGI_FORMAT_R16G16_UNORM:
        case DXGI_FORMAT_R16G16_UINT:
        case DXGI_FORMAT_R32_UINT:
            return 1;
        default:
            ASSUME( false );
        }
        break;
    default:
        break;
    }
    return 0;
}

//----------------------------------------------------------------------------------------------------------------------------------
void D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetPlaneSubsampledSizeAndFormatForCopyableLayout(
    UINT PlaneSlice,
    DXGI_FORMAT Format, 
    UINT Width, 
    UINT Height, 
    _Out_ DXGI_FORMAT& PlaneFormat, 
    _Out_ UINT& MinPlanePitchWidth, 
    _Out_ UINT& PlaneWidth, 
    _Out_ UINT& PlaneHeight)
{
    DXGI_FORMAT ParentFormat = GetParentFormat(Format);

    if (Planar(ParentFormat))
    {
        switch (ParentFormat)
        {
        // YCbCr 4:2:0 
        case DXGI_FORMAT_NV12:
            switch(PlaneSlice)
            {
            case 0:
                PlaneFormat = DXGI_FORMAT_R8_TYPELESS;
                PlaneWidth = Width;
                PlaneHeight = Height;
                break;
            case 1:
                PlaneFormat = DXGI_FORMAT_R8G8_TYPELESS;
                PlaneWidth = (Width + 1) >> 1;
                PlaneHeight = (Height + 1) >> 1;
                break;
            default:
                ASSUME(FALSE);
            };

            MinPlanePitchWidth = PlaneWidth;
            break;

        case DXGI_FORMAT_P010:
        case DXGI_FORMAT_P016:
            switch(PlaneSlice)
            {
            case 0:
                PlaneFormat = DXGI_FORMAT_R16_TYPELESS;
                PlaneWidth = Width;
                PlaneHeight = Height;
                break;
            case 1:
                PlaneFormat = DXGI_FORMAT_R16G16_TYPELESS;
                PlaneWidth = (Width + 1) >> 1;
                PlaneHeight = (Height + 1) >> 1;
                break;
            default:
                ASSUME(FALSE);
            };

            MinPlanePitchWidth = PlaneWidth;
            break;

            // YCbCr 4:2:2
        case DXGI_FORMAT_P208:
            switch(PlaneSlice)
            {
            case 0:
                PlaneFormat = DXGI_FORMAT_R8_TYPELESS;
                PlaneWidth = Width;
                PlaneHeight = Height;
                break;
            case 1:
                PlaneFormat = DXGI_FORMAT_R8G8_TYPELESS;
                PlaneWidth = (Width + 1) >> 1;
                PlaneHeight = Height;
                break;
            default:
                ASSUME(FALSE);
            };

            MinPlanePitchWidth = PlaneWidth;
            break;

            // YCbCr 4:4:0
        case DXGI_FORMAT_V208:
            PlaneFormat = DXGI_FORMAT_R8_TYPELESS;
            switch(PlaneSlice)
            {
            case 0:
                PlaneWidth = Width;
                PlaneHeight = Height;
                break;
            case 1:
            case 2:
                PlaneWidth = Width;
                PlaneHeight = (Height + 1) >> 1;
                break;
            default:
                ASSUME(FALSE);
            };

            MinPlanePitchWidth = PlaneWidth;
            break;

            // YCbCr 4:4:4
        case DXGI_FORMAT_V408:
            
            switch(PlaneSlice)
            {
            case 0:
            case 1:
            case 2:
                PlaneFormat = DXGI_FORMAT_R8_TYPELESS;
                PlaneWidth = Width;
                PlaneHeight = Height;
                MinPlanePitchWidth = PlaneWidth;
                break;
            default:
                ASSUME(FALSE);
            };
            break;

            // YCbCr 4:1:1
        case DXGI_FORMAT_NV11:
            switch(PlaneSlice)
            {
            case 0:
                PlaneFormat = DXGI_FORMAT_R8_TYPELESS;
                PlaneWidth = Width;
                PlaneHeight = Height;                
                MinPlanePitchWidth = Width;
                break;
            case 1:
                PlaneFormat = DXGI_FORMAT_R8G8_TYPELESS;
                PlaneWidth = (Width + 3) >> 2;
                PlaneHeight = Height;
                
                // NV11 has unused padding to the right of the chroma plane in the RowMajor (linear) copyable layout.
                MinPlanePitchWidth = (Width + 1) >> 1;
                break;
            default:
                ASSUME(FALSE);
            };
            
            break;

        case DXGI_FORMAT_R32G8X24_TYPELESS:
        case DXGI_FORMAT_R24G8_TYPELESS:
            switch(PlaneSlice)
            {
            case 0:
                PlaneFormat = DXGI_FORMAT_R32_TYPELESS;
                PlaneWidth = Width;
                PlaneHeight = Height;
                MinPlanePitchWidth = Width;
                break;
            case 1:
                PlaneFormat = DXGI_FORMAT_R8_TYPELESS;
                PlaneWidth = Width;
                PlaneHeight = Height;
                MinPlanePitchWidth = Width;
                break;
            default:
                ASSUME(FALSE);
            };
            break;

        default:
            ASSUME( FALSE );
        };
    }
    else
    {
        assert(PlaneSlice == 0);
        PlaneFormat = Format;
        PlaneWidth = Width;
        PlaneHeight = Height;
        MinPlanePitchWidth = PlaneWidth;
    }
}

//---------------------------------------------------------------------------------------------------------------------------------
// GetPlaneCount
UINT8 D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetPlaneCount(DXGI_FORMAT Format)
{
    switch( GetParentFormat(Format) )
    {
        case DXGI_FORMAT_NV12:
        case DXGI_FORMAT_NV11:
        case DXGI_FORMAT_P208:
        case DXGI_FORMAT_P016:
        case DXGI_FORMAT_P010:
        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_R32G8X24_TYPELESS:
            return 2;
        case DXGI_FORMAT_V208:
        case DXGI_FORMAT_V408:
            return 3;
        default:
            return 1;
    }
}

//---------------------------------------------------------------------------------------------------------------------------------
void D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetMipDimensions(UINT8 mipSlice, _Inout_ UINT64 *pWidth, _Inout_opt_ UINT64 *pHeight, _Inout_opt_ UINT64 *pDepth)
{
    UINT denominator = (1 << mipSlice); // 2 ^ subresource
    UINT64 mipWidth = *pWidth / denominator;
    UINT64 mipHeight = pHeight ? *pHeight / denominator : 1;
    UINT64 mipDepth = pDepth ? *pDepth / denominator : 1;

    // Adjust dimensions for degenerate mips
    if(mipHeight == 0 )
        mipHeight = 1;
    if(mipWidth == 0 )
        mipWidth = 1;
    if(mipDepth == 0)
        mipDepth = 1;

    *pWidth = mipWidth;
    if(pHeight) *pHeight = mipHeight;
    if(pDepth) *pDepth = mipDepth;
}

//---------------------------------------------------------------------------------------------------------------------------------
// DX9VertexOrIndexFormat
BOOL D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::DX9VertexOrIndexFormat(DXGI_FORMAT Format)
{
    return s_FormatDetail[D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetDetailTableIndexThrow( Format )].bDX9VertexOrIndexFormat;
}

//---------------------------------------------------------------------------------------------------------------------------------
// DX9TextureFormat
BOOL D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::DX9TextureFormat(DXGI_FORMAT Format)
{
    return s_FormatDetail[D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetDetailTableIndexThrow( Format )].bDX9TextureFormat;
}
//---------------------------------------------------------------------------------------------------------------------------------
// FloatNormTextureFormat
BOOL D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::FloatNormTextureFormat(DXGI_FORMAT Format)
{
    return s_FormatDetail[D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::GetDetailTableIndexThrow( Format )].bFloatNormFormat;
}

//---------------------------------------------------------------------------------------------------------------------------------
// ValidCastToR32UAV
//
// D3D11 has a limitation on typed UAVs (e.g. Texture1D/2D/3D) whereby the only format that can be read is R32_*.  Lots of formats
// can be written though, with type conversion (e.g. R8G8B8A8_*).  If an API user wants to do image processing in-place, in either
// the Compute Shader or the Pixel Shader, the only format available is R32_* (since it can be read and written at the same time).
//
// We were able to allow resources (Texture1D/2D/3D), created with a format from a small set of families that have 32 bits per element
// (such as R8G8B8A8_TYPELESS), to be cast to R32_* when creating a UAV.  This means the Compute Shader or Pixel Shader can
// do simultaneous read+write on the resource when bound as an R32_* UAV, with the caveat that the shader code has to do manual
// type conversion manually, but later on the resource can be used as an SRV or RT as the desired type (e.g. R8G8B8A8_UNORM), and 
// thus have access to filtering/blending where the hardware knows what the format is.  
//
// If we didn't have this ability to cast some formats to R32_* UAVs, applications would have to keep an extra allocation around 
// and do a rendering pass that copies from the R32_* UAV to whatever typed resource they really wanted.  For formats not included 
// in this list, such as any format that doesn't have 32-bits per component, as well as some 32-bit per component formats like 
// R24G8 or R11G11B10_FLOAT there is no alternative for an application but to do the extra copy as mentioned, or avoid in-place 
// image editing in favor of ping-ponging between buffers with multiple passes.
// 
bool D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::ValidCastToR32UAV(DXGI_FORMAT from, DXGI_FORMAT to)
{
    // Allow casting of 32 bit formats to R32_*
    if(
        ((to == DXGI_FORMAT_R32_UINT)||(to == DXGI_FORMAT_R32_SINT)||(to == DXGI_FORMAT_R32_FLOAT))
        && 
        (
            (from == DXGI_FORMAT_R10G10B10A2_TYPELESS) ||
            (from == DXGI_FORMAT_R8G8B8A8_TYPELESS) ||
            (from == DXGI_FORMAT_B8G8R8A8_TYPELESS) ||
            (from == DXGI_FORMAT_B8G8R8X8_TYPELESS) ||
            (from == DXGI_FORMAT_R16G16_TYPELESS) ||
            (from == DXGI_FORMAT_R32_TYPELESS) 
        )
    )
    {
        return true;
    }
    return false;
}

//---------------------------------------------------------------------------------------------------------------------------------
// IsSupportedTextureDisplayableFormat
//
// List of formats associated with Feature_D3D1XDisplayable
bool D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::IsSupportedTextureDisplayableFormat
    ( DXGI_FORMAT Format
    , bool bMediaFormatOnly
    )
{
    if (bMediaFormatOnly)
    {
        return (false
        || ( Format == DXGI_FORMAT_NV12
            || Format == DXGI_FORMAT_YUY2
            )
        );
    }
    else
    {
        return (false // eases evolution
            || ( Format == DXGI_FORMAT_B8G8R8A8_UNORM
                || Format == DXGI_FORMAT_R8G8B8A8_UNORM
                || Format == DXGI_FORMAT_R16G16B16A16_FLOAT
                || Format == DXGI_FORMAT_R10G10B10A2_UNORM
                || Format == DXGI_FORMAT_NV12
                || Format == DXGI_FORMAT_YUY2
                )
            );
    }
}

//---------------------------------------------------------------------------------------------------------------------------------
bool  D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::FloatAndNotFloatFormats(DXGI_FORMAT FormatA, DXGI_FORMAT FormatB)
{
    UINT NumComponents = (std::min)(GetNumComponentsInFormat(FormatA), GetNumComponentsInFormat(FormatB));
    for (UINT c = 0; c < NumComponents; c++)
    {
        D3D_FORMAT_COMPONENT_INTERPRETATION fciA = GetFormatComponentInterpretation(FormatA, c);
        D3D_FORMAT_COMPONENT_INTERPRETATION fciB = GetFormatComponentInterpretation(FormatB, c);
        if ((fciA != fciB) && ((fciA == D3DFCI_FLOAT) || (fciB == D3DFCI_FLOAT)))
        {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------------------------------------------------------------------------------
bool  D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::SNORMAndUNORMFormats(DXGI_FORMAT FormatA, DXGI_FORMAT FormatB)
{
    UINT NumComponents = (std::min)(GetNumComponentsInFormat(FormatA), GetNumComponentsInFormat(FormatB));
    for (UINT c = 0; c < NumComponents; c++)
    {
        D3D_FORMAT_COMPONENT_INTERPRETATION fciA = GetFormatComponentInterpretation(FormatA, c);
        D3D_FORMAT_COMPONENT_INTERPRETATION fciB = GetFormatComponentInterpretation(FormatB, c);
        if (((fciA == D3DFCI_SNORM) && (fciB == D3DFCI_UNORM)) ||
            ((fciB == D3DFCI_SNORM) && (fciA == D3DFCI_UNORM)))
        {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------------------------------------------------------------------------------
// Formats allowed by runtime for decode histogram.
 bool D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::DecodeHistogramAllowedForOutputFormatSupport(DXGI_FORMAT Format)
 {
     return (
         /* YUV 4:2:0 */
            Format == DXGI_FORMAT_NV12
         || Format == DXGI_FORMAT_P010
         || Format == DXGI_FORMAT_P016
         /* YUV 4:2:2 */
         || Format == DXGI_FORMAT_YUY2
         || Format == DXGI_FORMAT_Y210
         || Format == DXGI_FORMAT_Y216
         /* YUV 4:4:4 */
         || Format == DXGI_FORMAT_AYUV
         || Format == DXGI_FORMAT_Y410
         || Format == DXGI_FORMAT_Y416
     );
 }

//---------------------------------------------------------------------------------------------------------------------------------
// Formats allowed by runtime for decode histogram.  Scopes to tested formats.
 bool D3D12_PROPERTY_LAYOUT_FORMAT_TABLE::MotionEstimatorAllowedInputFormat(DXGI_FORMAT Format)
 {
     return Format == DXGI_FORMAT_NV12;
 }

#undef R 
#undef G 
#undef B 
#undef A 
#undef D 
#undef S 
#undef X 

#undef _TYPELESS   
#undef _FLOAT      
#undef _SNORM      
#undef _UNORM      
#undef _SINT       
#undef _UINT       
#undef _UNORM_SRGB 
#undef _FIXED_2_8  

#endif // D3D12_SDK_VERSION >= 606

