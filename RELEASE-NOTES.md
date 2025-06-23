<!-- Copyright © 2024-2025 Arm Limited.
SPDX-License-Identifier: MIT -->

## 25.06 Release

- Bug fixes:
  - FP16 was not being enabled correctly when targeting Android.
  - Fix a DX11 packaging error.
  - Correct inverted logic with FFXM_SPD_WAVE_OPERATIONS.
- Added documentation to allow external contributions.
- Code clean up and remove unused code.
- Added a new experimental Ultra Performance shader quality preset.
- Plugins for different versions of Unreal Engine are now on branches instead of in folders.

## Initial Public Release

**Arm® Accuracy Super Resolution™** is a mobile-optimized temporal upscaling technique derived from AMD's [FidelityFX™ Super Resolution 2 v2.2.2](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/main/docs/techniques/super-resolution-temporal.md). Arm ASR includes multiple optimizations on top of the original **FidelityFX™ Super Resolution 2** to make the technique suited for the more resource-constrained environment of mobile gaming. The team would like to express their gratitude to AMD and the engineers behind the original technique for their work and releasing it publicly to the world.

This Unreal® Engine plugin provides an implementation of the Arm ASR upscaler which can be integrated into your game.

Features include:
- Temporal upscaling using the `UE::Renderer::Private::ITemporalUpscaler` interface.
- Robust Contrast-Adaptive Sharpening (RCAS) - User specified option to enable image sharpening. This will improve the level of detail in the output.
- Reactive Mask generation shader to improve quality for fast moving, translucent objects.
- Transparency and composition mask generation shader to improve raytraced reflections or animated textures.
- Shader Quality Presets (Quality, Balanced or Performance) - User specified option to allow fine tuning of image quality, performance or a balance of both.
- Support for Vulkan® SM5 on Android™.
- Support for OpenGL® ES 3.2.
- Support for Unreal® Engine v5.3, v5.4, v5.5.

Known issues which will be addressed in a future release:
- Screen percentage over 100 causes incorrect behaviour of the camera.
- Shaders fail to compile when using Unreal 5.3, with GLES RHI and mobile renderer.

More information about the plugin can be found in the [README.md](./README.md).

More information on the technique, performance and the image quality of Arm ASR can be found [here](https://community.arm.com/arm-community-blogs/b/graphics-gaming-and-vr-blog/posts/introducing-arm-accuracy-super-resolution).
