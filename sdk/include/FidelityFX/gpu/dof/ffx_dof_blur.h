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
#include "ffx_dof_common.h"

// Factor applied to a distance value before checking that it is in range of the blur kernel.
FFX_STATIC const FfxFloat32 FFX_DOF_RANGE_TOLERANCE_FACTOR = 0.98;

// Accumulators for one ring. Used for ring occlusion.
struct FfxDofBucket
{
	FfxFloat32x4 color; // rgb=color sum, a=weight sum
	FfxFloat32 ringCovg; // radius of the ring coverage (average of tileCoc/coc with some clamping)
	FfxFloat32 radius; // radius of the ring center
	FfxUInt32 sampleCount; // number of samples counted
};

// One sample of the input and related variables
struct FfxDofSample
{
	FfxFloat32 coc; // signed circle of confusion in pixels. negative values are far-field.
	FfxFloat32x3 color; // color value of the sample
};

// Helper struct to contain all input variables
struct FfxDofInputState
{
	FfxUInt32x2 imageSize; // input pixel size (half res)
	FfxFloat32x2 pxCoord; // pixel coordinates of the kernel center
	FfxFloat32 tileCoc; // coc value bilinearly interpolated from the tile map
	FfxFloat32 centerCoc; // signed coc value at the kernel center
	FfxFloat32 ringGap; // undersampling factor. ringGap * nRings = tileCoc.
	FfxUInt32 mipLevel; // mip level to use based on coc and MAX_RINGS
	FfxBoolean nearField; // whether the center pixel is in the near field
	FfxUInt32 nSamples, // number of actual samples taken
		nRings; // number of rings to sample (<= MAX_RINGS)
	FfxFloat32 covgFactor, covgBias; // coverage parameters
};

// Helper struct to contain accumulation variables
struct FfxDofAccumulators
{
	FfxDofBucket prevBucket, currBucket;
	FfxFloat32x4 ringColor;
	FfxFloat32 ringCovg;
	FfxFloat32x4 nearColor, fillColor;
};

// Merges currBucket into prevBucket. Opacity is ratio of hit/total samples in current ring.
void FfxDofMergeBuckets(inout FfxDofAccumulators acc, FfxFloat32 opacity)
{
	// averaging
	FfxFloat32 prevRC = ffxSaturate(acc.prevBucket.ringCovg / acc.prevBucket.sampleCount);
	FfxFloat32 currRC = ffxSaturate(acc.currBucket.ringCovg / acc.currBucket.sampleCount);

	// occlusion term is calculated as the ratio of the area of intersection of both buckets
	// (being viewed as rings with a radius (centered on the samples) and ring width (=avg coverage))
	// divided by the area of the previous bucket ring.
	FfxFloat32 prevOuter = ffxSaturate(acc.prevBucket.radius + prevRC);
	FfxFloat32 prevInner = (acc.prevBucket.radius - prevRC);
	FfxFloat32 currOuter = ffxSaturate(acc.currBucket.radius + currRC);
	FfxFloat32 currInner = (acc.currBucket.radius - currRC);
	// intersection is between min(outer) and max(inner)
	FfxFloat32 insOuter = min(prevOuter, currOuter);
	FfxFloat32 insInner = max(prevInner, currInner);
	// intersection area formula.
	// ffxSaturate here fixes edge case where prev area = 0 -> ffxSaturate(0/0)=ffxSaturate(nan) = 0
	// The value does not matter in that case, since the previous values will be all zero, but it must be finite.
	FfxFloat32 occlusion = insOuter < insInner ? 0 : ffxSaturate((insOuter * insOuter - sign(insInner) * insInner * insInner) / (prevOuter * prevOuter - sign(prevInner) * prevInner * prevInner));

	FfxFloat32 factor = 1 - opacity * occlusion;
	acc.prevBucket.color = acc.prevBucket.color * factor + acc.currBucket.color;
	// select new radius so that (roughly) covers both rings, so in the middle of the combined ring
	FfxFloat32 newRadius = 0.5 * (max(prevOuter, currOuter) + min(prevInner, currInner));
	// the new coverage should then be the difference between the radius and either bound
	FfxFloat32 newCovg = 0.5 * (max(prevOuter, currOuter) - min(prevInner, currInner));
	acc.prevBucket.sampleCount = FfxUInt32(acc.prevBucket.sampleCount * factor) + acc.currBucket.sampleCount;
	acc.prevBucket.ringCovg = acc.prevBucket.sampleCount * newCovg;
	acc.prevBucket.radius = newRadius;
}

FfxFloat32 FfxDofWeight(FfxDofInputState ins, FfxFloat32 coc)
{
	// Weight is inverse coc area (1 / pi*r^2). Use pi~=4 for perf reasons.
	// Saturate to avoid explosion of weight close to zero. If coc < 0.5, the coc is contained
	// within this pixel and the weight should be 1.
	FfxFloat32 invRad = 1.0 / coc;
	return ffxSaturate(invRad * invRad / 4);
}

FfxFloat32 FfxDofCoverage(FfxDofInputState ins, FfxFloat32 coc)
{
	// Coverage is essentially the radius of the sample's projection to the lens aperture.
	// The radius is normalized to the tile CoC and kernel diameter in samples.
	// Add a small bias to account for gaps between sample rings.
	// Clamped to avoid infinity near zero.
	return ffxSaturate(ins.covgFactor / coc + ins.covgBias);
}

#ifdef FFX_DOF_CUSTOM_SAMPLES

// declarations only

// *MUST DEFINE* own struct SampleStreamState ! See below #else for example.

/// Callback for custom blur kernels. Gets the sample offset and advances the iterator.
/// @param state Mutable state for the sample stream.
/// @return The sample location in normalized uv coordinates. The default implementation approximates a circle using a combination of rotation and translation in an affine transform.
/// @ingroup FfxGPUDof
FfxFloat32x2 FfxDofAdvanceSampleStream(inout SampleStreamState state);

/// Callback for custom blur kernels. Gets the number of samples in a ring and initalizes iteration state.
/// @param state Mutable state for the sample stream.
/// @param ins Input parameters for the blur kernel. Contains the full number of rings.
/// @param ri Index of the current ring. If rings are being merged, this is the center of the indices.
/// @param mergeRingsCount The number of rings being merged. 1 if the current ring is not merged with any other.
/// @return The number of samples in the ring, assuming no merging. This is divided by the mergeRingsCount to get the actual number of samples.
/// @ingroup FfxGPUDof
FfxUInt32 FfxDofInitSampleStream(out SampleStreamState state, FfxDofInputState ins, FfxFloat32 ri, FfxUInt32 mergeRingsCount);

#else

struct SampleStreamState {
	// represents an affine 2d transform to go from one sample position to the next.
	FfxFloat32 cosTheta; // top-left and bottom-right element of rotation matrix
	FfxFloat32 sinThetaXAspect; // bottom-left element of rotation matrix
	FfxFloat32 mSinThetaXrAspect; // top-right element of rotation matrix
	FfxFloat32x2 translation; // additive part of affine transform
	FfxFloat32x2 position; // next sample position
};

FfxFloat32x2 FfxDofAdvanceSampleStream(inout SampleStreamState state)
{
	FfxFloat32x2 pos = state.position;
	// affine transformation.
	FfxFloat32 x = state.cosTheta * pos.x + state.mSinThetaXrAspect * pos.y;
	FfxFloat32 y = state.sinThetaXAspect * pos.x + state.cosTheta * pos.y;
	state.position = FfxFloat32x2(x, y) + state.translation;
	return pos;
}

FfxUInt32 FfxDofInitSampleStream(out SampleStreamState state, FfxDofInputState ins, FfxFloat32 ri, FfxUInt32 merge)
{
#if FFX_DOF_MAX_RING_MERGE > 1
	FfxUInt32 n = FfxUInt32(6.25 * (ins.nRings - ri)); // approx. pi/asin(1/(2*(nR-ri)))
#else
	// using fixed-point version of above allows scalar alu to do the same operation. Equivalent if merge == 1 (=> ri is integer).
	FfxUInt32 n = (27 * FfxUInt32(ins.nRings - ri)) >> 2;
#endif
	FfxFloat32 r = ins.tileCoc * ffxReciprocal(ins.nRings) * FfxFloat32(ins.nRings - ri);
	state.position = InputSizeHalfRcp() * (ins.pxCoord + FfxFloat32x2(r, 0));
	FfxFloat32 theta = 6.2831853 * ffxReciprocal(n) * merge;
	FfxFloat32 s = sin(theta), c = cos(theta);
	FfxFloat32x2 aspect = InputSizeHalf() * InputSizeHalfRcp().yx;
	
	state.cosTheta = c;
	state.sinThetaXAspect = s * aspect.x;
	state.mSinThetaXrAspect = -s * aspect.y;
	state.translation = InputSizeHalfRcp() * (ins.pxCoord - ins.pxCoord.x * FfxFloat32x2(c, s) - ins.pxCoord.y * FfxFloat32x2(-s, c));
	return n;
}

#endif // #ifdef FFX_DOF_CUSTOM_SAMPLES

struct FfxDofRingParams {
	FfxFloat32 distance; // distance to center in pixels / radius of ring
	FfxFloat32 bucketBorder; // (far field) border between curr/prev bucket
	FfxFloat32 rangeThreshNear; // threshold for in-range determination in near field
	FfxFloat32 rangeThreshFar; // same for far field
	FfxFloat32 rangeThreshFill; // threshold for main or fallback fill selection
	FfxFloat32 fillQuality; // bg-fill contribution quality
};

FfxDofSample FfxDofFetchSample(FfxDofInputState ins, inout SampleStreamState streamState, FfxUInt32 mipBias)
{
	FfxFloat32x2 samplePos = FfxDofAdvanceSampleStream(streamState);
	FfxUInt32 mipLevel = ins.mipLevel + mipBias;
	FfxFloat32x4 texval = FfxDofSampleInput(samplePos, mipLevel);
	FfxDofSample result;
	result.coc = texval.a;
	result.color = texval.rgb;
	return result;
}

void FfxDofProcessNearSample(FfxDofInputState ins, FfxDofSample s, inout FfxDofAccumulators acc, FfxDofRingParams ring)
{
	// feather the range slightly (1px)
	FfxFloat32 inRangeWeight = ffxSaturate(s.coc - ring.rangeThreshNear);
	FfxFloat32 weight = FfxDofWeight(ins, abs(s.coc));

	// fill background behind center using farther away samples.
	if (ins.nearField)
	{
		// Try to reject same-surface samples using using a slope of 1px radius per px distance.
		// But use the rejected pixels if no others are available.
		FfxFloat32 rejectionWeight = (s.coc < ring.rangeThreshFill) ? 1.0 : (1.0/2048.0);
		// prefer nearest (in image space) samples
		// Contribution decreases quadratically with sample distance.
		acc.fillColor += FfxFloat32x4(s.color, 1) * weight * ring.fillQuality * rejectionWeight;
	}

	acc.nearColor += FfxFloat32x4(s.color, 1) * weight * inRangeWeight;
}

void FfxDofProcessFarSample(FfxDofInputState ins, FfxDofSample s, inout FfxDofAccumulators acc, FfxDofRingParams ring)
{
	FfxFloat32 clampedFarCoc = max(0, -s.coc);
	FfxFloat32 inRangeWeight = ffxSaturate(-s.coc - ring.rangeThreshFar);

	FfxFloat32 covg = FfxDofCoverage(ins, abs(s.coc));
	FfxFloat32x4 color = FfxFloat32x4(s.color, 1) * FfxDofWeight(ins, abs(s.coc)) * inRangeWeight;

	if (-s.coc >= ring.bucketBorder)
	{
		acc.prevBucket.ringCovg += covg;
		acc.prevBucket.color += color;
		acc.prevBucket.sampleCount++;
	}
	else
	{
		acc.currBucket.ringCovg += covg;
		acc.currBucket.color += color;
		acc.currBucket.sampleCount++;
	}
}

void FfxDofProcessNearFar(FfxDofInputState ins, inout FfxDofAccumulators acc)
{
	// base case: both near and far-field are processed.
	// scan outside-in for far-field occlusion
	for (FfxUInt32 ri = 0; ri < ins.nRings; ri++)
	{
		acc.currBucket.color = FFX_BROADCAST_FLOAT32X4(0);
		acc.currBucket.ringCovg = 0;
		acc.currBucket.radius = ffxReciprocal(ins.nRings) * FfxFloat32(ins.nRings - ri);
		acc.currBucket.sampleCount = 0;

		SampleStreamState streamState;
		FfxUInt32 ringSamples = FfxDofInitSampleStream(streamState, ins, ri, 1);
		FfxDofRingParams ring;
		ring.distance = ins.tileCoc * ffxReciprocal(ins.nRings) * FfxFloat32(ins.nRings - ri);
		ring.bucketBorder = (ins.nRings - 1 - ri + 2.5) * ins.tileCoc * ffxReciprocal(0.5 + ins.nRings);
		ring.rangeThreshNear = max(0, ring.distance - ins.ringGap - 0.5);
		ring.rangeThreshFar = max(0, ring.distance - ins.ringGap);
		ring.rangeThreshFill = ins.centerCoc - ring.distance;
		ring.fillQuality = (1 / ring.distance) * (1 / ring.distance);

		// partially unrolled loop
		const FfxUInt32 UNROLL_CNT = 2;
		FfxUInt32 iunr = 0;
		for (iunr = 0; iunr + UNROLL_CNT <= ringSamples; iunr += UNROLL_CNT)
		{
			FFX_DOF_UNROLL
			for (FfxUInt32 i = 0; i < UNROLL_CNT; i++)
			{
				FfxDofSample s = FfxDofFetchSample(ins, streamState, 0);
				FfxDofProcessFarSample(ins, s, acc, ring);
				FfxDofProcessNearSample(ins, s, acc, ring);
			}
		}
		for (FfxUInt32 i = iunr; i < ringSamples; i++)
		{
			FfxDofSample s = FfxDofFetchSample(ins, streamState, 0);
			FfxDofProcessFarSample(ins, s, acc, ring);
			FfxDofProcessNearSample(ins, s, acc, ring);
		}

		FfxFloat32 opacity = FfxFloat32(acc.currBucket.sampleCount) / FfxFloat32(ringSamples);
		FfxDofMergeBuckets(acc, opacity);
	}
}

void FfxDofProcessNearOnly(FfxDofInputState ins, inout FfxDofAccumulators acc)
{
	// variant with the assumption that all samples are near field
	for (FfxUInt32 ri = 0; ri < ins.nRings; ri++)
	{
		SampleStreamState streamState;
		FfxUInt32 ringSamples = FfxDofInitSampleStream(streamState, ins, ri, 1);
		FfxDofRingParams ring;
		ring.distance = ins.tileCoc * ffxReciprocal(ins.nRings) * FfxFloat32(ins.nRings - ri);
		ring.rangeThreshNear = max(0, ring.distance - ins.ringGap - 0.5);
		ring.rangeThreshFill = ins.centerCoc - ring.distance;
		ring.fillQuality = (1 / ring.distance) * (1 / ring.distance);

		// partially unrolled loop
		const FfxUInt32 UNROLL_CNT = 4;
		FfxUInt32 iunr = 0;
		for (iunr = 0; iunr + UNROLL_CNT <= ringSamples; iunr += UNROLL_CNT)
		{
			FFX_DOF_UNROLL
			for (FfxUInt32 i = 0; i < UNROLL_CNT; i++)
			{
				FfxDofSample s = FfxDofFetchSample(ins, streamState, 0);
				FfxDofProcessNearSample(ins, s, acc, ring);
			}
		}
		for (FfxUInt32 i = iunr; i < ringSamples; i++)
		{
			FfxDofSample s = FfxDofFetchSample(ins, streamState, 0);
			FfxDofProcessNearSample(ins, s, acc, ring);
		}
	}
}

void FfxDofProcessFarOnly(FfxDofInputState ins, inout FfxDofAccumulators acc)
{
	// variant with the assumption that all samples are far field
	// scan outside-in for far-field occlusion
	for (FfxUInt32 ri = 0; ri < ins.nRings; ri++)
	{
		acc.currBucket.color = FFX_BROADCAST_FLOAT32X4(0);
		acc.currBucket.ringCovg = 0;
		acc.currBucket.radius = ffxReciprocal(ins.nRings) * FfxFloat32(ins.nRings - ri);
		acc.currBucket.sampleCount = 0;

		SampleStreamState streamState;
		FfxUInt32 ringSamples = FfxDofInitSampleStream(streamState, ins, ri, 1);
		FfxDofRingParams ring;
		ring.distance = ins.tileCoc * ffxReciprocal(ins.nRings) * FfxFloat32(ins.nRings - ri);
		ring.bucketBorder = (ins.nRings - 1 - ri + 2.5) * ins.tileCoc * ffxReciprocal(0.5 + ins.nRings);
		ring.rangeThreshFar = max(0, ring.distance - ins.ringGap);

		// partially unrolled loop
		const FfxUInt32 UNROLL_CNT = 3;
		FfxUInt32 iunr = 0;
		for (iunr = 0; iunr + UNROLL_CNT <= ringSamples; iunr += UNROLL_CNT)
		{
			FFX_DOF_UNROLL
			for (FfxUInt32 i = 0; i < UNROLL_CNT; i++)
			{
				FfxDofSample s = FfxDofFetchSample(ins, streamState, 0);
				FfxDofProcessFarSample(ins, s, acc, ring);
			}
		}
		for (FfxUInt32 i = iunr; i < ringSamples; i++)
		{
			FfxDofSample s = FfxDofFetchSample(ins, streamState, 0);
			FfxDofProcessFarSample(ins, s, acc, ring);
		}

		FfxFloat32 opacity = FfxFloat32(acc.currBucket.sampleCount) / FfxFloat32(ringSamples);
		FfxDofMergeBuckets(acc, opacity);
	}
}

void FfxDofProcessNearColorOnly(FfxDofInputState ins, inout FfxDofAccumulators acc)
{
	// variant with the assumption that all samples are near and equally weighed
	for (FfxUInt32 ri = 0; ri < ins.nRings;)
	{
		// merge inner rings if possible
		FfxUInt32 merge = min(min(1 << ri, FFX_DOF_MAX_RING_MERGE), ins.nRings - ri);
		FfxFloat32 rif = ri + 0.5 * merge - 0.5;
		FfxUInt32 weight = merge * merge;
		SampleStreamState streamState;
		FfxUInt32 ringSamples = FfxDofInitSampleStream(streamState, ins, rif, merge) / merge;
		FfxUInt32 mipBias = 2 * FfxUInt32(log2(merge));
		FfxFloat32 sampleDist = ins.tileCoc * ffxReciprocal(ins.nRings) * (FfxFloat32(ins.nRings) - rif);
		FfxHalfOpt rangeThresh = FfxHalfOpt(max(0, sampleDist - ins.ringGap - 0.5));

		FfxHalfOpt3 nearColorAcc = FfxHalfOpt3(0, 0, 0);
		FfxHalfOpt weightSum = FfxHalfOpt(0);
		// We presume that all samples are in range
		// partially unrolled loop (x6)
		const FfxUInt32 UNROLL_CNT = 6;
		FfxUInt32 iunr = 0;
		for (iunr = 0; iunr + UNROLL_CNT <= ringSamples; iunr += UNROLL_CNT)
		{
			FFX_DOF_UNROLL
			for (FfxUInt32 i = 0; i < UNROLL_CNT; i++)
			{
				FfxDofSample s = FfxDofFetchSample(ins, streamState, mipBias);
				FfxHalfOpt rangeWeight = ffxSaturate(FfxHalfOpt(s.coc) - rangeThresh);
				nearColorAcc += FfxHalfOpt3(s.color) * rangeWeight;
				weightSum += rangeWeight;
			}
		}
		for (FfxUInt32 i = iunr; i < ringSamples; i++)
		{
			FfxDofSample s = FfxDofFetchSample(ins, streamState, mipBias);
			FfxHalfOpt rangeWeight = ffxSaturate(FfxHalfOpt(s.coc) - rangeThresh);
			nearColorAcc += FfxHalfOpt3(s.color) * rangeWeight;
			weightSum += rangeWeight;
		}
		acc.nearColor.rgb += nearColorAcc * FfxHalfOpt(weight);
		acc.nearColor.a += weightSum * FfxHalfOpt(weight);
		ri += merge;
	}
}

void FfxDofProcessFarColorOnly(FfxDofInputState ins, inout FfxDofAccumulators acc)
{
	FfxFloat32 nSamples = 0;
	FfxUInt32 ri = 0;
	// peel first iteration
	if (ri < ins.nRings)
	{
		// merge inner rings if possible
		FfxUInt32 merge = min(min(1 << ri, FFX_DOF_MAX_RING_MERGE), ins.nRings - ri);
		FfxFloat32 rif = ri + 0.5 * merge - 0.5;
		FfxUInt32 weight = merge * merge;
		SampleStreamState streamState;
		FfxUInt32 ringSamples = FfxDofInitSampleStream(streamState, ins, rif, merge) / merge;
		FfxUInt32 mipBias = 2 * FfxUInt32(log2(merge));
		FfxFloat32 sampleDist = ins.tileCoc * ffxReciprocal(ins.nRings) * (FfxFloat32(ins.nRings) - rif);
		FfxHalfOpt rangeThresh = FfxHalfOpt(max(0, sampleDist - ins.ringGap));

		FfxHalfOpt3 colorAcc = FfxHalfOpt3(0, 0, 0);
		FfxHalfOpt weightSum = FfxHalfOpt(0);
		// partially unrolled loop (x8, then x4)
		FfxUInt32 iunr = 0;
		for (; iunr + 8 <= ringSamples; iunr += 8)
		{
			FFX_DOF_UNROLL
			for (FfxUInt32 i = 0; i < 8; i++)
			{
				FfxDofSample s = FfxDofFetchSample(ins, streamState, mipBias);
				FfxHalfOpt rangeWeight = ffxSaturate(FfxHalfOpt(-s.coc) - rangeThresh);
				colorAcc += FfxHalfOpt3(s.color) * rangeWeight;
				weightSum += rangeWeight;
			}
		}
		for (; iunr + 4 <= ringSamples; iunr += 4)
		{
			FFX_DOF_UNROLL
			for (FfxUInt32 i = 0; i < 4; i++)
			{
				FfxDofSample s = FfxDofFetchSample(ins, streamState, mipBias);
				FfxHalfOpt rangeWeight = ffxSaturate(FfxHalfOpt(-s.coc) - rangeThresh);
				colorAcc += FfxHalfOpt3(s.color) * rangeWeight;
				weightSum += rangeWeight;
			}
		}
		for (FfxUInt32 i = iunr; i < ringSamples; i++)
		{
			FfxDofSample s = FfxDofFetchSample(ins, streamState, mipBias);
			FfxHalfOpt rangeWeight = ffxSaturate(FfxHalfOpt(-s.coc) - rangeThresh);
			colorAcc += FfxHalfOpt3(s.color) * rangeWeight;
			weightSum += rangeWeight;
		}
		acc.prevBucket.color.rgb += colorAcc * FfxHalfOpt(weight);
		nSamples += weightSum * FfxHalfOpt(weight);
		ri += merge;
	}
	while (ri < ins.nRings)
	{
		// merge inner rings if possible
		FfxUInt32 merge = min(min(1 << ri, FFX_DOF_MAX_RING_MERGE), ins.nRings - ri);
		FfxFloat32 rif = ri + 0.5 * merge - 0.5;
		FfxUInt32 weight = merge * merge;
		SampleStreamState streamState;
		FfxUInt32 ringSamples = FfxDofInitSampleStream(streamState, ins, rif, merge) / merge;
		FfxUInt32 mipBias = 2 * FfxUInt32(log2(merge));

		FfxHalfOpt3 colorAcc = FfxHalfOpt3(0, 0, 0);
		// partially unrolled loop (x12, then x4)
		FfxUInt32 iunr = 0;
		for (; iunr + 12 <= ringSamples; iunr += 12)
		{
			FFX_DOF_UNROLL
			for (FfxUInt32 i = 0; i < 12; i++)
			{
				FfxDofSample s = FfxDofFetchSample(ins, streamState, mipBias);
				// Max difference between sample coc and tile coc is 0.5px (see PrepareTile).
				// Max radius of any ring after first (ri>0) is 1 less than tile coc.
				// rangeWeight = 1
				colorAcc += FfxHalfOpt3(s.color);
			}
		}
		for (; iunr + 4 <= ringSamples; iunr += 4)
		{
			FFX_DOF_UNROLL
			for (FfxUInt32 i = 0; i < 4; i++)
			{
				FfxDofSample s = FfxDofFetchSample(ins, streamState, mipBias);
				colorAcc += FfxHalfOpt3(s.color);
			}
		}
		for (FfxUInt32 i = iunr; i < ringSamples; i++)
		{
			FfxDofSample s = FfxDofFetchSample(ins, streamState, mipBias);
			colorAcc += FfxHalfOpt3(s.color);
		}
		acc.prevBucket.color.rgb += colorAcc * FfxHalfOpt(weight);
		nSamples += ringSamples * weight;
		ri += merge;
	}
	acc.prevBucket.color.a = nSamples;
	acc.prevBucket.ringCovg = nSamples;
	acc.prevBucket.sampleCount = FfxUInt32(nSamples);
}

struct FfxDofTileClass
{
	FfxBoolean colorOnly, needsNear, needsFar;
};

// prepare values for the tile. Return classification.
FfxDofTileClass FfxDofPrepareTile(FfxUInt32x2 id, out FfxDofInputState ins)
{
	FfxDofTileClass tileClass;
	FfxFloat32x2 dilatedCocSigned = FfxDofSampleDilatedRadius(id);
	FfxFloat32x2 dilatedCoc = abs(dilatedCocSigned);
	FfxFloat32 tileRad = max(dilatedCoc.x, dilatedCoc.y);

	// check if copying the tile is good enough
#if FFX_HLSL
	if (WaveActiveAllTrue(tileRad < 0.5))
#elif FFX_GLSL
	if (subgroupAll(tileRad < 0.5))
#endif
	{
		tileClass.needsNear = false;
		tileClass.needsFar = false;
		tileClass.colorOnly = false;
		return tileClass;
	}

	FfxFloat32 idealRingCount = // kernel radius in pixels -> one sample per pixel
#if FFX_HLSL
		WaveActiveMax(ceil(tileRad)); 
#elif FFX_GLSL
		subgroupMax(ceil(tileRad));
#endif
	ins.nRings = FfxUInt32(idealRingCount);
	ins.mipLevel = 0;
	if (idealRingCount > MaxRings())
	{
		ins.nRings = MaxRings();
		// use a higher mip to cover the missing rings.
		// for every factor 2 decrease of the rings, increase mips by 1.
		ins.mipLevel = FfxUInt32(log2(idealRingCount * ffxReciprocal(MaxRings())));
	}
	// Gap = number of pixels between rings that are not sampled.
	ins.ringGap = idealRingCount / ins.nRings - 1.0;
	ins.tileCoc = tileRad;

	FfxFloat32x2 texcoord = FfxFloat32x2(id) + FfxFloat32x2(0.25, 0.25); // shift to center of top-left pixel in quad
	// Add noise to reduce banding (if too noisy, this could be disabled)
	{
		/* hash22 adapted from https://www.shadertoy.com/view/4djSRW
		Copyright (c)2014 David Hoskins.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.*/
		FfxFloat32x3 p3 = ffxFract(texcoord.xyx * FfxFloat32x3(0.1031, 0.1030, 0.0973));
		p3 += dot(p3, p3.yzx + 33.33);
		texcoord += ffxFract((p3.xx+p3.yz)*p3.zy) * 0.5 - 0.25;
	}
	ins.pxCoord = texcoord;

	FfxFloat32 centerCoc = FfxDofLoadInput(id).a;
	ins.nearField = centerCoc > 0;
	ins.centerCoc = abs(centerCoc);

	ins.nSamples = 0;
#ifdef FFX_DOF_CUSTOM_SAMPLES
	for (FfxUInt32 ri = 0; ri < ins.nRings; ri++)
	{
		SampleStreamState streamState;
		ins.nSamples += FfxDofInitSampleStream(streamState, ins, ri, 1);
	}
#else
	// due to rounding this will likely over-approximate, but that should be okay.
	ins.nSamples = FfxUInt32(6.25 * 0.5 * (ins.nRings * (ins.nRings + 1)));
#endif

	// read first lane to force using a scalar register.
	ins.covgFactor = ffxWaveReadLaneFirst(0.5 * ffxReciprocal(ins.nRings) * ins.tileCoc);
	ins.covgBias = ffxWaveReadLaneFirst(0.5 * ffxReciprocal(ins.nRings));

#if FFX_HLSL
	// simple check: is there even any near/far that we would sample?
	// See ProcessNearSample: No relevant code is executed if the center is not near and no sample is near
	// (use the CoC of the dilated tile minimum depth as the proxy for any sample)
	tileClass.needsNear = WaveActiveAnyTrue(dilatedCocSigned.x > -1);
	// See ProcessFarSample: All weights will be 0 if no sample is in the far field.
	// (use the CoC of dilated tile max depth as proxy)
	tileClass.needsFar = WaveActiveAnyTrue(dilatedCocSigned.y < 1);

	tileClass.colorOnly = WaveActiveAllTrue(dilatedCocSigned.x - dilatedCocSigned.y < 0.5);
#elif FFX_GLSL
	// as above
	tileClass.needsNear = subgroupAny(dilatedCocSigned.x > -1);
	tileClass.needsFar = subgroupAny(dilatedCocSigned.y < 1);
	tileClass.colorOnly = subgroupAll(dilatedCocSigned.x - dilatedCocSigned.y < 0.5);
#endif

	return tileClass;
}

/// Blur pass entry point. Runs in 8x8x1 thread groups and computes transient near and far outputs.
///
/// @param pixel Coordinate of the pixel (SV_DispatchThreadID)
/// @param halfImageSize Resolution of the source image (half resolution) in pixels
/// @ingroup FfxGPUDof
void FfxDofBlur(FfxUInt32x2 pixel, FfxUInt32x2 halfImageSize)
{
	FfxDofResetMaxTileRadius();

	FfxDofInputState ins;
	FfxDofTileClass tileClass = FfxDofPrepareTile(pixel, ins);
	ins.imageSize = halfImageSize;

	// initialize accumulators
	FfxDofAccumulators acc;
	acc.prevBucket.color = FfxFloat32x4(0, 0, 0, 0);
	acc.prevBucket.ringCovg = 0;
	acc.prevBucket.radius = 0;
	acc.prevBucket.sampleCount = 0;

	FfxFloat32 centerWeight = FfxDofWeight(ins, ins.centerCoc);
	FfxFloat32 centerCovg = FfxDofCoverage(ins, ins.centerCoc);

	FfxFloat32x4 centerColor = FfxFloat32x4(FfxDofLoadInput(pixel).rgb, 1);
	acc.nearColor = FfxFloat32x4(0, 0, 0, 0);
	acc.fillColor = FfxFloat32x4(0, 0, 0, 0);

	if (ins.nearField)
	{
		// for near field: adjust center weight to cover everything beyond innermost ring
		// radius of innermost ring:
		FfxFloat32 innerRingRad = max(1, ins.tileCoc * ffxReciprocal(ins.nRings) - pow(2, ins.mipLevel));
		FfxFloat32 nearCenterWeight = innerRingRad * innerRingRad;

		// if center radius < 1px, split the center color between near and fill
		FfxFloat32 nearPart = ffxSaturate(ins.centerCoc), fillPart = 1 - nearPart;
		acc.nearColor = centerColor * centerWeight * nearCenterWeight * nearPart;
		acc.fillColor = centerColor * fillPart;
	}

	if (tileClass.needsNear && tileClass.needsFar)
	{
		FfxDofProcessNearFar(ins, acc);
	}
	else if (tileClass.needsNear)
	{
		if (tileClass.colorOnly)
			FfxDofProcessNearColorOnly(ins, acc);
		else
			FfxDofProcessNearOnly(ins, acc);
	}
	else if (tileClass.needsFar)
	{
		if (tileClass.colorOnly)
			FfxDofProcessFarColorOnly(ins, acc);
		else
			FfxDofProcessFarOnly(ins, acc);
	}
	else // if (!tileClass.needsFar && !tileClass.needsNear)
	{
		FfxDofStoreFar(pixel, FfxHalfOpt4(FfxDofLoadInput(pixel).rgb, 1));
		FfxDofStoreNear(pixel, FfxHalfOpt4(0, 0, 0, 0));
		return;
	}

	// process center
	acc.currBucket.ringCovg = ins.nearField ? 0 : centerCovg;
	acc.currBucket.color = ins.nearField ? FfxFloat32x4(0, 0, 0, 0) : centerColor * centerWeight;
	acc.currBucket.radius = 0;
	acc.currBucket.sampleCount = 1;
	FfxDofMergeBuckets(acc, 1);

	if (ins.nearField)
	{
	acc.prevBucket.color += acc.fillColor;
	}
	FfxFloat32 fgOpacity = (!tileClass.needsFar && tileClass.colorOnly) ? 1.0 :
		ffxSaturate(acc.nearColor.a / (FfxDofWeight(ins, ins.tileCoc) * ins.nSamples));

	FfxFloat32x4 ffTarget = acc.prevBucket.color / acc.prevBucket.color.a;
	FfxFloat32x4 ffOutput = !any(isnan(ffTarget)) ? ffTarget : FfxFloat32x4(0, 0, 0, 0);
	FfxFloat32x3 nfTarget = acc.nearColor.rgb / acc.nearColor.a;
	FfxFloat32x4 nfOutput = FfxFloat32x4(!any(isnan(nfTarget)) ? nfTarget : FfxFloat32x3(0, 0, 0), fgOpacity);

	FfxDofStoreFar(pixel, FfxHalfOpt4(ffOutput));
	FfxDofStoreNear(pixel, FfxHalfOpt4(nfOutput));
}
