# Removed.

While in theory [ffxFrameInterpolationDispatch can consume depth/MV inputs from other sources](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/release-FSR3-3.0.3/docs/techniques/frame-interpolation.md#estimate-interpolated-frame-depth), or [can use a custom pass](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/release-FSR3-3.0.3/docs/techniques/super-resolution-temporal.md#reconstruct-and-dilate), it's not designed to do so and won't work in its current state. FSR3 FG code relies on internal logic and upscaling structures. Can they be decoupled? Maybe, but I don't have the time right now.

So why did this seem to generate frames? Because [these functions always return FFX_OK](source/maindll/FFXInterpolator.h#L240), which is a flat out lie. Optical flow analysis succeeds. The debug overlay looks fine. Yet for some reason there are no interpolated frames, only copies of the previous. Seems familiar.

Repository left intact since it's an experiment.
