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

#include "ffx_core.h"
#include "ffx_cacao_defines.h"
#include "ffx_cacao_utils.h"

FFX_STATIC const FfxFloat32x4 g_FFX_CACAO_samplePatternMain[] =
{
	{ 0.78488064,  0.56661671,  1.500000, -0.126083},    { 0.26022232, -0.29575172,  1.500000, -1.064030},    { 0.10459357,  0.08372527,  1.110000, -2.730563},    {-0.68286800,  0.04963045,  1.090000, -0.498827},
	{-0.13570161, -0.64190155,  1.250000, -0.532765},    {-0.26193795, -0.08205118,  0.670000, -1.783245},    {-0.61177456,  0.66664219,  0.710000, -0.044234},    { 0.43675563,  0.25119025,  0.610000, -1.167283},
	{ 0.07884444,  0.86618668,  0.640000, -0.459002},    {-0.12790935, -0.29869005,  0.600000, -1.729424},    {-0.04031125,  0.02413622,  0.600000, -4.792042},    { 0.16201244, -0.52851415,  0.790000, -1.067055},
	{-0.70991218,  0.47301072,  0.640000, -0.335236},    { 0.03277707, -0.22349690,  0.600000, -1.982384},    { 0.68921727,  0.36800742,  0.630000, -0.266718},    { 0.29251814,  0.37775412,  0.610000, -1.422520},
	{-0.12224089,  0.96582592,  0.600000, -0.426142},    { 0.11071457, -0.16131058,  0.600000, -2.165947},    { 0.46562141, -0.59747696,  0.600000, -0.189760},    {-0.51548797,  0.11804193,  0.600000, -1.246800},
	{ 0.89141309, -0.42090443,  0.600000,  0.028192},    {-0.32402530, -0.01591529,  0.600000, -1.543018},    { 0.60771245,  0.41635221,  0.600000, -0.605411},    { 0.02379565, -0.08239821,  0.600000, -3.809046},
	{ 0.48951152, -0.23657045,  0.600000, -1.189011},    {-0.17611565, -0.81696892,  0.600000, -0.513724},    {-0.33930185, -0.20732205,  0.600000, -1.698047},    {-0.91974425,  0.05403209,  0.600000,  0.062246},
	{-0.15064627, -0.14949332,  0.600000, -1.896062},    { 0.53180975, -0.35210401,  0.600000, -0.758838},    { 0.41487166,  0.81442589,  0.600000, -0.505648},    {-0.24106961, -0.32721516,  0.600000, -1.665244}
};

#define FFX_CACAO_MAX_TAPS (32)
#define FFX_CACAO_ADAPTIVE_TAP_BASE_COUNT (5)
#define FFX_CACAO_ADAPTIVE_TAP_FLEXIBLE_COUNT (FFX_CACAO_MAX_TAPS - FFX_CACAO_ADAPTIVE_TAP_BASE_COUNT)

// these values can be changed (up to FFX_CACAO_MAX_TAPS) with no changes required elsewhere; values for 4th and 5th preset are ignored but array needed to avoid compilation errors
// the actual number of texture samples is two times this value (each "tap" has two symmetrical depth texture samples)
FFX_STATIC const FfxUInt32 g_FFX_CACAO_numTaps[5] = {3, 5, 12, 0, 0};


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Optional parts that can be enabled for a required quality preset level and above (0 == Low, 1 == Medium, 2 == High, 3 == Highest/Adaptive, 4 == reference/unused )
// Each has its own cost. To disable just set to 5 or above.
//
// (experimental) tilts the disk (although only half of the samples!) towards surface normal; this helps with effect uniformity between objects but reduces effect distance and has other side-effects
#define FFX_CACAO_TILT_SAMPLES_ENABLE_AT_QUALITY_PRESET                      (99)        // to disable simply set to 99 or similar
#define FFX_CACAO_TILT_SAMPLES_AMOUNT                                        (0.4)
//
#define FFX_CACAO_HALOING_REDUCTION_ENABLE_AT_QUALITY_PRESET                 (1)         // to disable simply set to 99 or similar
#define FFX_CACAO_HALOING_REDUCTION_AMOUNT                                   (0.6)       // values from 0.0 - 1.0, 1.0 means max weighting (will cause artifacts, 0.8 is more reasonable)
//
#define FFX_CACAO_NORMAL_BASED_EDGES_ENABLE_AT_QUALITY_PRESET                (2) //2        // to disable simply set to 99 or similar
#define FFX_CACAO_NORMAL_BASED_EDGES_DOT_THRESHOLD                           (0.5)       // use 0-0.1 for super-sharp normal-based edges
//
#define FFX_CACAO_DETAIL_AO_ENABLE_AT_QUALITY_PRESET                         (1) //1         // whether to use DetailAOStrength; to disable simply set to 99 or similar
//
#define FFX_CACAO_DEPTH_MIPS_ENABLE_AT_QUALITY_PRESET                        (2)         // !!warning!! the MIP generation on the C++ side will be enabled on quality preset 2 regardless of this value, so if changing here, change the C++ side too
#define FFX_CACAO_DEPTH_MIPS_GLOBAL_OFFSET                                   (-4.3)      // best noise/quality/performance tradeoff, found empirically
//
// !!warning!! the edge handling is hard-coded to 'disabled' on quality level 0, and enabled above, on the C++ side; while toggling it here will work for
// testing purposes, it will not yield performance gains (or correct results)
#define FFX_CACAO_DEPTH_BASED_EDGES_ENABLE_AT_QUALITY_PRESET                 (1)
//
#define FFX_CACAO_REDUCE_RADIUS_NEAR_SCREEN_BORDER_ENABLE_AT_QUALITY_PRESET  (99)        // 99 means disabled; only helpful if artifacts at the edges caused by lack of out of screen depth data are not acceptable with the depth sampler in either clamp or mirror modes

// =======================================================================================================
// SSAO Generation

// calculate effect radius and fit our screen sampling pattern inside it
void FFX_CACAO_CalculateRadiusParameters(const FfxFloat32 pixCenterLength, const FfxFloat32x2 pixelDirRBViewspaceSizeAtCenterZ, out FfxFloat32 pixLookupRadiusMod, out FfxFloat32 effectRadius, out FfxFloat32 falloffCalcMulSq)
{
	effectRadius = EffectRadius();

	// leaving this out for performance reasons: use something similar if radius needs to scale based on distance
	//effectRadius *= pow( pixCenterLength, RadiusDistanceScalingFunctionPow());

	// when too close, on-screen sampling disk will grow beyond screen size; limit this to avoid closeup temporal artifacts
	const FfxFloat32 tooCloseLimitMod = ffxSaturate(pixCenterLength * EffectSamplingRadiusNearLimitRec()) * 0.8 + 0.2;

	effectRadius *= tooCloseLimitMod;

	// 0.85 is to reduce the radius to allow for more samples on a slope to still stay within influence
	pixLookupRadiusMod = (0.85 * effectRadius) / pixelDirRBViewspaceSizeAtCenterZ.x;

	// used to calculate falloff (both for AO samples and per-sample weights)
	falloffCalcMulSq = -1.0f / (effectRadius*effectRadius);
}

// all vectors in viewspace
FfxFloat32 FFX_CACAO_CalculatePixelObscurance(FfxFloat32x3 pixelNormal, FfxFloat32x3 hitDelta, FfxFloat32 falloffCalcMulSq)
{
	FfxFloat32 lengthSq = dot(hitDelta, hitDelta);
	FfxFloat32 NdotD = dot(pixelNormal, hitDelta) / sqrt(lengthSq);

	FfxFloat32 falloffMult = max(0.0, lengthSq * falloffCalcMulSq + 1.0);

	return max(0, NdotD - EffectHorizonAngleThreshold()) * falloffMult;
}

void FFX_CACAO_SSAOTapInner(const FfxInt32     qualityLevel,
                            inout FfxFloat32   obscuranceSum,
                            inout FfxFloat32   weightSum,
                            const FfxFloat32x2 samplingUV,
                            const FfxFloat32   mipLevel,
                            const FfxFloat32x3 pixCenterPos,
                            const FfxFloat32x3 negViewspaceDir,
                            FfxFloat32x3       pixelNormal,
                            const FfxFloat32   falloffCalcMulSq,
                            const FfxFloat32   weightMod,
                            const FfxInt32     dbgTapIndex,
                            FfxUInt32           layerId)
{
	// get depth at sample
	FfxFloat32 viewspaceSampleZ = FFX_CACAO_SSAOGeneration_SampleViewspaceDepthMip(samplingUV, mipLevel, layerId);

	// convert to viewspace
	FfxFloat32x3 hitPos = FFX_CACAO_DepthBufferUVToViewSpace(samplingUV.xy, viewspaceSampleZ).xyz;
	FfxFloat32x3 hitDelta = hitPos - pixCenterPos;

	FfxFloat32 obscurance = FFX_CACAO_CalculatePixelObscurance(pixelNormal, hitDelta, falloffCalcMulSq);
	FfxFloat32 weight = 1.0;

	if (qualityLevel >= FFX_CACAO_HALOING_REDUCTION_ENABLE_AT_QUALITY_PRESET)
	{
		//FfxFloat32 reduct = max( 0, dot( hitDelta, negViewspaceDir ) );
		FfxFloat32 reduct = max(0, -hitDelta.z); // cheaper, less correct version
		reduct = ffxSaturate(reduct * NegRecEffectRadius() + 2.0); // ffxSaturate( 2.0 - reduct / EffectRadius() );
		weight = FFX_CACAO_HALOING_REDUCTION_AMOUNT * reduct + (1.0 - FFX_CACAO_HALOING_REDUCTION_AMOUNT);
	}
	weight *= weightMod;
	obscuranceSum += obscurance * weight;
	weightSum += weight;
}

void FFX_CACAO_SSAOTap(const FfxInt32       qualityLevel,
                       inout FfxFloat32     obscuranceSum,
                       inout FfxFloat32     weightSum,
                       const FfxInt32       tapIndex,
                       const FfxFloat32x2x2 rotScale,
                       const FfxFloat32x3   pixCenterPos,
                       const FfxFloat32x3   negViewspaceDir,
                       FfxFloat32x3         pixelNormal,
                       const FfxFloat32x2   normalizedScreenPos,
                       const FfxFloat32x2   depthBufferUV,
                       const FfxFloat32     mipOffset,
                       const FfxFloat32     falloffCalcMulSq,
                       FfxFloat32           weightMod,
                       FfxFloat32x2         normXY,
                       FfxFloat32           normXYLength,
                       const FfxUInt32      layerId)
{
	FfxFloat32x2  sampleOffset;
	FfxFloat32   samplePow2Len;

	// patterns
	{
		FfxFloat32x4 newSample = g_FFX_CACAO_samplePatternMain[tapIndex];
		sampleOffset = FFX_TRANSFORM_VECTOR(rotScale, newSample.xy);
		samplePow2Len = newSample.w;                      // precalculated, same as: samplePow2Len = log2( length( newSample.xy ) );
		weightMod *= newSample.z;
	}

	// snap to pixel center (more correct obscurance math, avoids artifacts)
	sampleOffset = round(sampleOffset);

	// calculate MIP based on the sample distance from the centre, similar to as described
	// in http://graphics.cs.williams.edu/papers/SAOHPG12/.
	FfxFloat32 mipLevel = (qualityLevel < FFX_CACAO_DEPTH_MIPS_ENABLE_AT_QUALITY_PRESET) ? (0) : (samplePow2Len + mipOffset);

	FfxFloat32x2 samplingUV = sampleOffset * DeinterleavedDepthBufferInverseDimensions() + depthBufferUV;

	FFX_CACAO_SSAOTapInner(qualityLevel, obscuranceSum, weightSum, samplingUV, mipLevel, pixCenterPos, negViewspaceDir, pixelNormal, falloffCalcMulSq, weightMod, tapIndex * 2, layerId);

	// for the second tap, just use the mirrored offset
	FfxFloat32x2 sampleOffsetMirroredUV = -sampleOffset;

	// tilt the second set of samples so that the disk is effectively rotated by the normal
	// effective at removing one set of artifacts, but too expensive for lower quality settings
	if (qualityLevel >= FFX_CACAO_TILT_SAMPLES_ENABLE_AT_QUALITY_PRESET)
	{
		FfxFloat32 dotNorm = dot(sampleOffsetMirroredUV, normXY);
		sampleOffsetMirroredUV -= dotNorm * normXYLength * normXY;
		sampleOffsetMirroredUV = round(sampleOffsetMirroredUV);
	}

	// snap to pixel center (more correct obscurance math, avoids artifacts)
	FfxFloat32x2 samplingMirroredUV = sampleOffsetMirroredUV * DeinterleavedDepthBufferInverseDimensions() + depthBufferUV;

	FFX_CACAO_SSAOTapInner(qualityLevel, obscuranceSum, weightSum, samplingMirroredUV, mipLevel, pixCenterPos, negViewspaceDir, pixelNormal, falloffCalcMulSq, weightMod, tapIndex * 2 + 1, layerId);
}

struct FFX_CACAO_SSAOHits
{
	FfxFloat32x3 hits[2];
	FfxFloat32 weightMod;
};

struct FFX_CACAO_SSAOSampleData
{
	FfxFloat32x2 uvOffset;
	FfxFloat32 mipLevel;
	FfxFloat32 weightMod;
};

FFX_CACAO_SSAOSampleData FFX_CACAO_SSAOGetSampleData(const FfxInt32       qualityLevel,
                                                     const FfxFloat32x2x2 rotScale,
                                                     const FfxFloat32x4   newSample,
                                                     const FfxFloat32     mipOffset)
{
	FfxFloat32x2  sampleOffset = FFX_TRANSFORM_VECTOR(rotScale, newSample.xy);
	sampleOffset = round(sampleOffset) * DeinterleavedDepthBufferInverseDimensions();

	FfxFloat32 samplePow2Len = newSample.w;
	FfxFloat32 mipLevel = (qualityLevel < FFX_CACAO_DEPTH_MIPS_ENABLE_AT_QUALITY_PRESET) ? (0) : (samplePow2Len + mipOffset);

	FFX_CACAO_SSAOSampleData result;

	result.uvOffset = sampleOffset;
	result.mipLevel = mipLevel;
	result.weightMod = newSample.z;

	return result;
}

FFX_CACAO_SSAOHits FFX_CACAO_SSAOGetHits2(FFX_CACAO_SSAOSampleData data, const FfxFloat32x2 depthBufferUV, const FfxUInt32 layerId)
{
	FFX_CACAO_SSAOHits result;
	result.weightMod = data.weightMod;
	FfxFloat32x2 sampleUV = depthBufferUV + data.uvOffset;
	result.hits[0] = FfxFloat32x3(sampleUV, FFX_CACAO_SSAOGeneration_SampleViewspaceDepthMip(sampleUV, data.mipLevel, layerId));
	sampleUV = depthBufferUV - data.uvOffset;
	result.hits[1] = FfxFloat32x3(sampleUV, FFX_CACAO_SSAOGeneration_SampleViewspaceDepthMip(sampleUV, data.mipLevel, layerId));
	return result;
}

void FFX_CACAO_SSAOAddHits(const FfxInt32 qualityLevel, const FfxFloat32x3 pixCenterPos, const FfxFloat32x3 pixelNormal, const FfxFloat32 falloffCalcMulSq, inout FfxFloat32 weightSum, inout FfxFloat32 obscuranceSum, FFX_CACAO_SSAOHits hits)
{
	FfxFloat32 weight = hits.weightMod;
	FFX_UNROLL
	for (FfxInt32 hitIndex = 0; hitIndex < 2; ++hitIndex)
	{
		FfxFloat32x3 hit = hits.hits[hitIndex];
		FfxFloat32x3 hitPos = FFX_CACAO_DepthBufferUVToViewSpace(hit.xy, hit.z);
		FfxFloat32x3 hitDelta = hitPos - pixCenterPos;

		FfxFloat32 obscurance = FFX_CACAO_CalculatePixelObscurance(pixelNormal, hitDelta, falloffCalcMulSq);

		if (qualityLevel >= FFX_CACAO_HALOING_REDUCTION_ENABLE_AT_QUALITY_PRESET)
		{
			//FfxFloat32 reduct = max( 0, dot( hitDelta, negViewspaceDir ) );
			FfxFloat32 reduct = max(0, -hitDelta.z); // cheaper, less correct version
			reduct = ffxSaturate(reduct * NegRecEffectRadius() + 2.0); // ffxSaturate( 2.0 - reduct / EffectRadius() );
			weight = FFX_CACAO_HALOING_REDUCTION_AMOUNT * reduct + (1.0 - FFX_CACAO_HALOING_REDUCTION_AMOUNT);
		}
		obscuranceSum += obscurance * weight;
		weightSum += weight;
	}
}

void FFX_CACAO_GenerateSSAOShadowsInternal(out FfxFloat32                 outShadowTerm,
                                           out FfxFloat32x4               outEdges,
                                           out FfxFloat32                 outWeight,
                                           const FfxFloat32x2             SVPos /*, const FfxFloat32x2 normalizedScreenPos*/,
                                           FFX_PARAMETER_UNIFORM FfxInt32 qualityLevel,
                                           bool                           adaptiveBase,
                                           const FfxUInt32                layerId)
{
	FfxFloat32x2 SVPosRounded = trunc(SVPos);
	FfxUInt32x2 SVPosui = FfxUInt32x2(SVPosRounded); //same as FfxUInt32x2( SVPos )

	const FfxInt32 numberOfTaps = (adaptiveBase) ? (FFX_CACAO_ADAPTIVE_TAP_BASE_COUNT) : FfxInt32(g_FFX_CACAO_numTaps[qualityLevel]);
	FfxFloat32 pixZ, pixLZ, pixTZ, pixRZ, pixBZ;

	FfxFloat32x2 depthBufferUVCorner = (SVPos + 1.0f) * DeinterleavedDepthBufferInverseDimensions() + DeinterleavedDepthBufferNormalisedOffset(); //Need corner to avoid gather fixed point error.
	FfxFloat32x2 depthBufferUV = (SVPos + 0.5f) * DeinterleavedDepthBufferInverseDimensions() + DeinterleavedDepthBufferNormalisedOffset(); //Need center coord
	FfxFloat32x4 valuesUL = FFX_CACAO_SSAOGeneration_GatherViewspaceDepthOffset(depthBufferUVCorner, FfxInt32x2(-1, -1), layerId);
	FfxFloat32x4 valuesBR = FFX_CACAO_SSAOGeneration_GatherViewspaceDepthOffset(depthBufferUVCorner, FfxInt32x2(0, 0), layerId);


	// get this pixel's viewspace depth
	pixZ = valuesUL.y;

	// get left right top bottom neighbouring pixels for edge detection (gets compiled out on qualityLevel == 0)
	pixLZ = valuesUL.x;
	pixTZ = valuesUL.z;
	pixRZ = valuesBR.z;
	pixBZ = valuesBR.x;

	// FfxFloat32x2 normalizedScreenPos = SVPosRounded * Viewport2xPixelSize() + Viewport2xPixelSize_x_025();
	FfxFloat32x2 normalizedScreenPos = (SVPosRounded + 0.5f) * SSAOBufferInverseDimensions();
	FfxFloat32x3 pixCenterPos = FFX_CACAO_NDCToViewSpace(normalizedScreenPos, pixZ); // g

	// Load this pixel's viewspace normal
	// FfxUInt32x2 fullResCoord = 2 * (SVPosui * 2 + PerPassFullResCoordOffset().xy);
    FfxFloat32x3 pixelNormal = FFX_CACAO_SSAOGeneration_GetNormalPass(SVPosui, layerId);

	// optimized approximation of:  FfxFloat32x2 pixelDirRBViewspaceSizeAtCenterZ = FFX_CACAO_NDCToViewSpace( normalizedScreenPos.xy + _ViewportPixelSize().xy, pixCenterPos.z ).xy - pixCenterPos.xy;
	// const FfxFloat32x2 pixelDirRBViewspaceSizeAtCenterZ = pixCenterPos.z * NDCToViewMul() * Viewport2xPixelSize();
	const FfxFloat32x2 pixelDirRBViewspaceSizeAtCenterZ = pixCenterPos.z * NDCToViewMul() * SSAOBufferInverseDimensions();

	FfxFloat32 pixLookupRadiusMod;
	FfxFloat32 falloffCalcMulSq;

	// calculate effect radius and fit our screen sampling pattern inside it
	FfxFloat32 effectViewspaceRadius;
	FFX_CACAO_CalculateRadiusParameters(length(pixCenterPos), pixelDirRBViewspaceSizeAtCenterZ, pixLookupRadiusMod, effectViewspaceRadius, falloffCalcMulSq);

	// calculate samples rotation/scaling
	FfxFloat32x2x2 rotScale;
	{
		// reduce effect radius near the screen edges slightly; ideally, one would render a larger depth buffer (5% on each side) instead
		if (!adaptiveBase && (qualityLevel >= FFX_CACAO_REDUCE_RADIUS_NEAR_SCREEN_BORDER_ENABLE_AT_QUALITY_PRESET))
		{
			FfxFloat32 nearScreenBorder = min(min(depthBufferUV.x, 1.0 - depthBufferUV.x), min(depthBufferUV.y, 1.0 - depthBufferUV.y));
			nearScreenBorder = ffxSaturate(10.0 * nearScreenBorder + 0.6);
			pixLookupRadiusMod *= nearScreenBorder;
		}

		// load & update pseudo-random rotation matrix
		FfxUInt32 pseudoRandomIndex = FfxUInt32(SVPosRounded.y * 2 + SVPosRounded.x) % 5;
		FfxFloat32x4 rs = PatternRotScaleMatrices(layerId, pseudoRandomIndex);
		rotScale = FfxFloat32x2x2(rs.x * pixLookupRadiusMod, rs.y * pixLookupRadiusMod, rs.z * pixLookupRadiusMod, rs.w * pixLookupRadiusMod);
	}

	// the main obscurance & sample weight storage
	FfxFloat32 obscuranceSum = 0.0;
	FfxFloat32 weightSum = 0.0;

	// edge mask for between this and left/right/top/bottom neighbour pixels - not used in quality level 0 so initialize to "no edge" (1 is no edge, 0 is edge)
	FfxFloat32x4 edgesLRTB = FfxFloat32x4(1.0, 1.0, 1.0, 1.0);

	// Move center pixel slightly towards camera to avoid imprecision artifacts due to using of 16bit depth buffer; a lot smaller offsets needed when using 32bit floats
	pixCenterPos *= DepthPrecisionOffsetMod();

	if (!adaptiveBase && (qualityLevel >= FFX_CACAO_DEPTH_BASED_EDGES_ENABLE_AT_QUALITY_PRESET))
	{
		edgesLRTB = FFX_CACAO_CalculateEdges(pixZ, pixLZ, pixRZ, pixTZ, pixBZ);
	}

	// adds a more high definition sharp effect, which gets blurred out (reuses left/right/top/bottom samples that we used for edge detection)
	if (!adaptiveBase && (qualityLevel >= FFX_CACAO_DETAIL_AO_ENABLE_AT_QUALITY_PRESET))
	{
		// disable in case of quality level 4 (reference)
		if (qualityLevel != 4)
		{
			//approximate neighbouring pixels positions (actually just deltas or "positions - pixCenterPos" )
			FfxFloat32x3 viewspaceDirZNormalized = FfxFloat32x3(pixCenterPos.xy / pixCenterPos.zz, 1.0);

			// very close approximation of: FfxFloat32x3 pixLPos  = FFX_CACAO_NDCToViewSpace( normalizedScreenPos + FfxFloat32x2( -HalfViewportPixelSize().x, 0.0 ), pixLZ ).xyz - pixCenterPos.xyz;
			FfxFloat32x3 pixLDelta = FfxFloat32x3(-pixelDirRBViewspaceSizeAtCenterZ.x, 0.0, 0.0) + viewspaceDirZNormalized * (pixLZ - pixCenterPos.z);
			// very close approximation of: FfxFloat32x3 pixRPos  = FFX_CACAO_NDCToViewSpace( normalizedScreenPos + FfxFloat32x2( +HalfViewportPixelSize().x, 0.0 ), pixRZ ).xyz - pixCenterPos.xyz;
			FfxFloat32x3 pixRDelta = FfxFloat32x3(+pixelDirRBViewspaceSizeAtCenterZ.x, 0.0, 0.0) + viewspaceDirZNormalized * (pixRZ - pixCenterPos.z);
			// very close approximation of: FfxFloat32x3 pixTPos  = FFX_CACAO_NDCToViewSpace( normalizedScreenPos + FfxFloat32x2( 0.0, -HalfViewportPixelSize().y ), pixTZ ).xyz - pixCenterPos.xyz;
			FfxFloat32x3 pixTDelta = FfxFloat32x3(0.0, -pixelDirRBViewspaceSizeAtCenterZ.y, 0.0) + viewspaceDirZNormalized * (pixTZ - pixCenterPos.z);
			// very close approximation of: FfxFloat32x3 pixBPos  = FFX_CACAO_NDCToViewSpace( normalizedScreenPos + FfxFloat32x2( 0.0, +HalfViewportPixelSize().y ), pixBZ ).xyz - pixCenterPos.xyz;
			FfxFloat32x3 pixBDelta = FfxFloat32x3(0.0, +pixelDirRBViewspaceSizeAtCenterZ.y, 0.0) + viewspaceDirZNormalized * (pixBZ - pixCenterPos.z);

			const FfxFloat32 rangeReductionConst = 4.0f;                         // this is to avoid various artifacts
			const FfxFloat32 modifiedFalloffCalcMulSq = rangeReductionConst * falloffCalcMulSq;

			FfxFloat32x4 additionalObscurance;
			additionalObscurance.x = FFX_CACAO_CalculatePixelObscurance(pixelNormal, pixLDelta, modifiedFalloffCalcMulSq);
			additionalObscurance.y = FFX_CACAO_CalculatePixelObscurance(pixelNormal, pixRDelta, modifiedFalloffCalcMulSq);
			additionalObscurance.z = FFX_CACAO_CalculatePixelObscurance(pixelNormal, pixTDelta, modifiedFalloffCalcMulSq);
			additionalObscurance.w = FFX_CACAO_CalculatePixelObscurance(pixelNormal, pixBDelta, modifiedFalloffCalcMulSq);

			obscuranceSum += DetailAOStrength() * dot(additionalObscurance, edgesLRTB);
		}
	}

	// Sharp normals also create edges - but this adds to the cost as well
	if (!adaptiveBase && (qualityLevel >= FFX_CACAO_NORMAL_BASED_EDGES_ENABLE_AT_QUALITY_PRESET))
	{
		FfxFloat32x3 neighbourNormalL = FFX_CACAO_SSAOGeneration_GetNormalPass(SVPosui + FfxInt32x2(-1, +0), layerId);
		FfxFloat32x3 neighbourNormalR = FFX_CACAO_SSAOGeneration_GetNormalPass(SVPosui + FfxInt32x2(+1, +0), layerId);
		FfxFloat32x3 neighbourNormalT = FFX_CACAO_SSAOGeneration_GetNormalPass(SVPosui + FfxInt32x2(+0, -1), layerId);
		FfxFloat32x3 neighbourNormalB = FFX_CACAO_SSAOGeneration_GetNormalPass(SVPosui + FfxInt32x2(+0, +1), layerId);

		const FfxFloat32 dotThreshold = FFX_CACAO_NORMAL_BASED_EDGES_DOT_THRESHOLD;

		FfxFloat32x4 normalEdgesLRTB;
		normalEdgesLRTB.x = ffxSaturate((dot(pixelNormal, neighbourNormalL) + dotThreshold));
		normalEdgesLRTB.y = ffxSaturate((dot(pixelNormal, neighbourNormalR) + dotThreshold));
		normalEdgesLRTB.z = ffxSaturate((dot(pixelNormal, neighbourNormalT) + dotThreshold));
		normalEdgesLRTB.w = ffxSaturate((dot(pixelNormal, neighbourNormalB) + dotThreshold));

		//#define FFX_CACAO_SMOOTHEN_NORMALS // fixes some aliasing artifacts but kills a lot of high detail and adds to the cost - not worth it probably but feel free to play with it
#ifdef FFX_CACAO_SMOOTHEN_NORMALS
		//neighbourNormalL  = LoadNormal( fullResCoord, FfxInt32x2( -1,  0 ) );
		//neighbourNormalR  = LoadNormal( fullResCoord, FfxInt32x2(  1,  0 ) );
		//neighbourNormalT  = LoadNormal( fullResCoord, FfxInt32x2(  0, -1 ) );
		//neighbourNormalB  = LoadNormal( fullResCoord, FfxInt32x2(  0,  1 ) );
		pixelNormal += neighbourNormalL * edgesLRTB.x + neighbourNormalR * edgesLRTB.y + neighbourNormalT * edgesLRTB.z + neighbourNormalB * edgesLRTB.w;
		pixelNormal = normalize(pixelNormal);
#endif

		edgesLRTB *= normalEdgesLRTB;
	}



	const FfxFloat32 globalMipOffset = FFX_CACAO_DEPTH_MIPS_GLOBAL_OFFSET;
	FfxFloat32 mipOffset = (qualityLevel < FFX_CACAO_DEPTH_MIPS_ENABLE_AT_QUALITY_PRESET) ? (0) : (log2(pixLookupRadiusMod) + globalMipOffset);

	// Used to tilt the second set of samples so that the disk is effectively rotated by the normal
	// effective at removing one set of artifacts, but too expensive for lower quality settings
	FfxFloat32x2 normXY = FfxFloat32x2(pixelNormal.x, pixelNormal.y);
	FfxFloat32 normXYLength = length(normXY);
	normXY /= FfxFloat32x2(normXYLength, -normXYLength);
	normXYLength *= FFX_CACAO_TILT_SAMPLES_AMOUNT;

	const FfxFloat32x3 negViewspaceDir = -normalize(pixCenterPos);

	// standard, non-adaptive approach
	if ((qualityLevel != 3) || adaptiveBase)
	{
		FFX_UNROLL
		for (FfxInt32 i = 0; i < numberOfTaps; i++)
		{
			FFX_CACAO_SSAOTap(qualityLevel, obscuranceSum, weightSum, i, rotScale, pixCenterPos, negViewspaceDir, pixelNormal, normalizedScreenPos, depthBufferUV, mipOffset, falloffCalcMulSq, 1.0, normXY, normXYLength, layerId);
		}
	}
	else // if( qualityLevel == 3 ) adaptive approach
	{
		// add new ones if needed
		FfxFloat32x2 fullResUV = normalizedScreenPos + PerPassFullResUVOffset(layerId).xy;
		FfxFloat32 importance = FFX_CACAO_SSAOGeneration_SampleImportance(fullResUV);

		// this is to normalize FFX_CACAO_DETAIL_AO_AMOUNT across all pixel regardless of importance
		obscuranceSum *= (FFX_CACAO_ADAPTIVE_TAP_BASE_COUNT / FfxFloat32(FFX_CACAO_MAX_TAPS)) + (importance * FFX_CACAO_ADAPTIVE_TAP_FLEXIBLE_COUNT / FfxFloat32(FFX_CACAO_MAX_TAPS));

		// load existing base values
		FfxFloat32x2 baseValues = FFX_CACAO_SSAOGeneration_LoadBasePassSSAOPass(SVPosui, layerId); //PassIndex());
		weightSum += baseValues.y * FfxFloat32(FFX_CACAO_ADAPTIVE_TAP_BASE_COUNT * 4.0);
		obscuranceSum += (baseValues.x) * weightSum;

		// increase importance around edges
		FfxFloat32 edgeCount = dot(1.0 - edgesLRTB, FfxFloat32x4(1.0, 1.0, 1.0, 1.0));

		FfxFloat32 avgTotalImportance = FfxFloat32(FFX_CACAO_SSAOGeneration_GetLoadCounter()) * LoadCounterAvgDiv();

		FfxFloat32 importanceLimiter = ffxSaturate(AdaptiveSampleCountLimit() / avgTotalImportance);
		importance *= importanceLimiter;

		FfxFloat32 additionalSampleCountFlt = FFX_CACAO_ADAPTIVE_TAP_FLEXIBLE_COUNT * importance;

		additionalSampleCountFlt += 1.5;
		FfxUInt32 additionalSamples = FfxUInt32(additionalSampleCountFlt);
		FfxUInt32 additionalSamplesTo = min(FFX_CACAO_MAX_TAPS - 1, additionalSamples + FFX_CACAO_ADAPTIVE_TAP_BASE_COUNT);

		// sample loop
		{
			FfxFloat32x4 newSample = g_FFX_CACAO_samplePatternMain[FFX_CACAO_ADAPTIVE_TAP_BASE_COUNT];
			FFX_CACAO_SSAOSampleData data = FFX_CACAO_SSAOGetSampleData(qualityLevel, rotScale, newSample, mipOffset);
			FFX_CACAO_SSAOHits hits = FFX_CACAO_SSAOGetHits2(data, depthBufferUV, layerId);
			newSample = g_FFX_CACAO_samplePatternMain[FFX_CACAO_ADAPTIVE_TAP_BASE_COUNT + 1];

			for (FfxUInt32 i = FFX_CACAO_ADAPTIVE_TAP_BASE_COUNT; i < additionalSamplesTo - 1; i++)
			{
				data = FFX_CACAO_SSAOGetSampleData(qualityLevel, rotScale, newSample, mipOffset);
				newSample = g_FFX_CACAO_samplePatternMain[i + 2];
				FFX_CACAO_SSAOHits nextHits = FFX_CACAO_SSAOGetHits2(data, depthBufferUV, layerId);

				FFX_CACAO_SSAOAddHits(qualityLevel, pixCenterPos, pixelNormal, falloffCalcMulSq, weightSum, obscuranceSum, hits);
				hits = nextHits;
			}

			// last loop iteration
			{
				FFX_CACAO_SSAOAddHits(qualityLevel, pixCenterPos, pixelNormal, falloffCalcMulSq, weightSum, obscuranceSum, hits);
			}
		}
	}

	// early out for adaptive base - just output weight (used for the next pass)
	if (adaptiveBase)
	{
		FfxFloat32 obscurance = obscuranceSum / weightSum;

		outShadowTerm = obscurance;
		outEdges = FfxFloat32x4(0, 0, 0, 0);
		outWeight = weightSum;
		return;
	}

	// calculate weighted average
	FfxFloat32 obscurance = obscuranceSum / weightSum;

	// calculate fadeout (1 close, gradient, 0 far)
	FfxFloat32 fadeOut = ffxSaturate(pixCenterPos.z * EffectFadeOutMul() + EffectFadeOutAdd());

	// Reduce the SSAO shadowing if we're on the edge to remove artifacts on edges (we don't care for the lower quality one)
	if (!adaptiveBase && (qualityLevel >= FFX_CACAO_DEPTH_BASED_EDGES_ENABLE_AT_QUALITY_PRESET))
	{
		// FfxFloat32 edgeCount = dot( 1.0-edgesLRTB, FfxFloat32x4( 1.0, 1.0, 1.0, 1.0 ) );

		// when there's more than 2 opposite edges, start fading out the occlusion to reduce aliasing artifacts
		FfxFloat32 edgeFadeoutFactor = ffxSaturate((1.0 - edgesLRTB.x - edgesLRTB.y) * 0.35) + ffxSaturate((1.0 - edgesLRTB.z - edgesLRTB.w) * 0.35);

		// (experimental) if you want to reduce the effect next to any edge
		// edgeFadeoutFactor += 0.1 * ffxSaturate( dot( 1 - edgesLRTB, FfxFloat32x4( 1, 1, 1, 1 ) ) );

		fadeOut *= ffxSaturate(1.0 - edgeFadeoutFactor);
	}

	// same as a bove, but a lot more conservative version
	// fadeOut *= ffxSaturate( dot( edgesLRTB, FfxFloat32x4( 0.9, 0.9, 0.9, 0.9 ) ) - 2.6 );

	// strength
	obscurance = EffectShadowStrength() * obscurance;

	// clamp
	obscurance = min(obscurance, EffectShadowClamp());

	// fadeout
	obscurance *= fadeOut;

	// conceptually switch to occlusion with the meaning being visibility (grows with visibility, occlusion == 1 implies full visibility),
	// to be in line with what is more commonly used.
	FfxFloat32 occlusion = 1.0 - obscurance;

	// modify the gradient
	// note: this cannot be moved to a later pass because of loss of precision after storing in the render target
	occlusion = pow(ffxSaturate(occlusion), EffectShadowPow());

	// outputs!
	outShadowTerm = occlusion;    // Our final 'occlusion' term (0 means fully occluded, 1 means fully lit)
	outEdges = edgesLRTB;    // These are used to prevent blurring across edges, 1 means no edge, 0 means edge, 0.5 means half way there, etc.
	outWeight = weightSum;
}

void FFX_CACAO_GenerateQ0(FfxUInt32x3 tid)
{
    FfxUInt32 tidCorrectedZ = tid.z % 5;
    FfxUInt32 layerId       = tid.z / 5;
    layerId            = (layerId == 1 && BlurNumPasses() == 2) ? 3 : layerId; // Choose correct layer in 2 layer case
	FfxUInt32 xOffset = (tid.y * 3 + tidCorrectedZ) % 5;
	FfxUInt32x2 coord = FfxUInt32x2(5 * tid.x + xOffset, tid.y);
	FfxFloat32x2 inPos = FfxFloat32x2(coord);
	FfxFloat32   outShadowTerm;
	FfxFloat32   outWeight;
	FfxFloat32x4  outEdges;
	FFX_CACAO_GenerateSSAOShadowsInternal(outShadowTerm, outEdges, outWeight, inPos.xy, 0, false, layerId);
	FfxFloat32x2 out0;
	out0.x = outShadowTerm;
	out0.y = FFX_CACAO_PackEdges(FfxFloat32x4(1, 1, 1, 1)); // no edges in low quality
	FFX_CACAO_SSAOGeneration_StoreOutput(coord, out0, layerId);
}

void FFX_CACAO_GenerateQ1(FfxUInt32x3 tid)
{
    FfxUInt32 tidCorrectedZ = tid.z % 5;
    FfxUInt32 layerId       = tid.z / 5;
    layerId            = (layerId == 1 && BlurNumPasses() == 2) ? 3 : layerId;  // Choose correct layer in 2 layer case
	FfxUInt32 xOffset = (tid.y * 3 + tidCorrectedZ) % 5;
	FfxUInt32x2 coord = FfxUInt32x2(5 * tid.x + xOffset, tid.y);
	FfxFloat32x2 inPos = FfxFloat32x2(coord);
	FfxFloat32   outShadowTerm;
	FfxFloat32   outWeight;
	FfxFloat32x4  outEdges;
	FFX_CACAO_GenerateSSAOShadowsInternal(outShadowTerm, outEdges, outWeight, inPos.xy, 1, false, layerId);
	FfxFloat32x2 out0;
	out0.x = outShadowTerm;
	out0.y = FFX_CACAO_PackEdges(outEdges);
	FFX_CACAO_SSAOGeneration_StoreOutput(coord, out0, layerId);
}

void FFX_CACAO_GenerateQ2(FfxUInt32x3 coord)
{
	FfxFloat32x2 inPos = FfxFloat32x2(coord.xy);
    FfxUInt32    layerId = coord.z;
	FfxFloat32   outShadowTerm;
	FfxFloat32   outWeight;
	FfxFloat32x4  outEdges;
	FFX_CACAO_GenerateSSAOShadowsInternal(outShadowTerm, outEdges, outWeight, inPos.xy, 2, false, layerId);
	FfxFloat32x2 out0;
	out0.x = outShadowTerm;
	out0.y = FFX_CACAO_PackEdges(outEdges);
	FFX_CACAO_SSAOGeneration_StoreOutput(coord.xy, out0, layerId);
}

void FFX_CACAO_GenerateQ3Base(FfxUInt32x3 coord)
{
	FfxFloat32x2 inPos = FfxFloat32x2(coord.xy);
    FfxUInt32    layerId = coord.z;
	FfxFloat32   outShadowTerm;
	FfxFloat32   outWeight;
	FfxFloat32x4  outEdges;
	FFX_CACAO_GenerateSSAOShadowsInternal(outShadowTerm, outEdges, outWeight, inPos.xy, 3, true, layerId);
	FfxFloat32x2 out0;
	out0.x = outShadowTerm;
	out0.y = outWeight / (FfxFloat32(FFX_CACAO_ADAPTIVE_TAP_BASE_COUNT) * 4.0); //0.0; //frac(outWeight / 6.0);// / (FfxFloat32)(FFX_CACAO_MAX_TAPS * 4.0);
	FFX_CACAO_SSAOGeneration_StoreOutput(coord.xy, out0, layerId);
}

void FFX_CACAO_GenerateQ3(FfxUInt32x3 coord)
{
	FfxFloat32x2 inPos = FfxFloat32x2(coord.xy);
    FfxUInt32    layerId = coord.z;
	FfxFloat32   outShadowTerm;
	FfxFloat32   outWeight;
	FfxFloat32x4  outEdges;
	FFX_CACAO_GenerateSSAOShadowsInternal(outShadowTerm, outEdges, outWeight, inPos.xy, 3, false, layerId);
	FfxFloat32x2 out0;
	out0.x = outShadowTerm;
	out0.y = FFX_CACAO_PackEdges(outEdges);
	FFX_CACAO_SSAOGeneration_StoreOutput(coord.xy, out0, layerId);
}
