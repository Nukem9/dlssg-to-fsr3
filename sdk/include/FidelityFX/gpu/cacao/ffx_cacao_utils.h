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

// packing/unpacking for edges; 2 bits per edge mean 4 gradient values (0, 0.33, 0.66, 1) for smoother transitions!
FfxFloat32 FFX_CACAO_PackEdges(FfxFloat32x4 edgesLRTB)
{
	edgesLRTB = round(ffxSaturate(edgesLRTB) * 3.05);
	return dot(edgesLRTB, FfxFloat32x4(64.0 / 255.0, 16.0 / 255.0, 4.0 / 255.0, 1.0 / 255.0));
}

FfxFloat32x4 FFX_CACAO_UnpackEdges(FfxFloat32 _packedVal)
{
	FfxUInt32 packedVal = FfxUInt32(_packedVal * 255.5);
	FfxFloat32x4 edgesLRTB;
	edgesLRTB.x = FfxFloat32((packedVal >> 6) & 0x03) / 3.0;          // there's really no need for mask (as it's an 8 bit input) but I'll leave it in so it doesn't cause any trouble in the future
	edgesLRTB.y = FfxFloat32((packedVal >> 4) & 0x03) / 3.0;
	edgesLRTB.z = FfxFloat32((packedVal >> 2) & 0x03) / 3.0;
	edgesLRTB.w = FfxFloat32((packedVal >> 0) & 0x03) / 3.0;

	return ffxSaturate(edgesLRTB + InvSharpness());
}

FfxFloat32 FFX_CACAO_ScreenSpaceToViewSpaceDepth(FfxFloat32 screenDepth)
{
	FfxFloat32 depthLinearizeMul = DepthUnpackConsts().x;
	FfxFloat32 depthLinearizeAdd = DepthUnpackConsts().y;

	return depthLinearizeMul * ffxInvertSafe(depthLinearizeAdd - screenDepth);
}

FfxFloat32x4 FFX_CACAO_ScreenSpaceToViewSpaceDepth(FfxFloat32x4 screenDepth)
{
	FfxFloat32 depthLinearizeMul = DepthUnpackConsts().x;
	FfxFloat32 depthLinearizeAdd = DepthUnpackConsts().y;

	return depthLinearizeMul * ffxInvertSafe(depthLinearizeAdd - screenDepth);
}

FfxFloat32x4 FFX_CACAO_CalculateEdges(const FfxFloat32 centerZ, const FfxFloat32 leftZ, const FfxFloat32 rightZ, const FfxFloat32 topZ, const FfxFloat32 bottomZ)
{
	// slope-sensitive depth-based edge detection
	FfxFloat32x4 edgesLRTB = FfxFloat32x4(leftZ, rightZ, topZ, bottomZ) - centerZ;
	FfxFloat32x4 edgesLRTBSlopeAdjusted = edgesLRTB + edgesLRTB.yxwz;
	edgesLRTB = min(abs(edgesLRTB), abs(edgesLRTBSlopeAdjusted));
	return ffxSaturate((1.3 - edgesLRTB / (centerZ * 0.040)));
}

FfxFloat32x3 FFX_CACAO_NDCToViewSpace(FfxFloat32x2 pos, FfxFloat32 viewspaceDepth)
{
	FfxFloat32x3 ret;

	ret.xy = (NDCToViewMul() * pos.xy + NDCToViewAdd()) * viewspaceDepth;

	ret.z = viewspaceDepth;

	return ret;
}

FfxFloat32x3 FFX_CACAO_DepthBufferUVToViewSpace(FfxFloat32x2 pos, FfxFloat32 viewspaceDepth)
{
	FfxFloat32x3 ret;
	ret.xy = (DepthBufferUVToViewMul() * pos.xy + DepthBufferUVToViewAdd()) * viewspaceDepth;
	ret.z = viewspaceDepth;
	return ret;
}

FfxFloat32x3 FFX_CACAO_CalculateNormal(const FfxFloat32x4 edgesLRTB, FfxFloat32x3 pixCenterPos, FfxFloat32x3 pixLPos, FfxFloat32x3 pixRPos, FfxFloat32x3 pixTPos, FfxFloat32x3 pixBPos)
{
	// Get this pixel's viewspace normal
	FfxFloat32x4 acceptedNormals = FfxFloat32x4(edgesLRTB.x*edgesLRTB.z, edgesLRTB.z*edgesLRTB.y, edgesLRTB.y*edgesLRTB.w, edgesLRTB.w*edgesLRTB.x);

	pixLPos = normalize(pixLPos - pixCenterPos);
	pixRPos = normalize(pixRPos - pixCenterPos);
	pixTPos = normalize(pixTPos - pixCenterPos);
	pixBPos = normalize(pixBPos - pixCenterPos);

	FfxFloat32x3 pixelNormal = FfxFloat32x3(0, 0, -0.0005);
	pixelNormal += (acceptedNormals.x) * cross(pixLPos, pixTPos);
	pixelNormal += (acceptedNormals.y) * cross(pixTPos, pixRPos);
	pixelNormal += (acceptedNormals.z) * cross(pixRPos, pixBPos);
	pixelNormal += (acceptedNormals.w) * cross(pixBPos, pixLPos);
	pixelNormal = normalize(pixelNormal);

	return pixelNormal;
}