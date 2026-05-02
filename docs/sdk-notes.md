# SDK Notes

## After Effects

References checked during scaffold setup:

- Adobe's After Effects developer page describes plug-ins as supporting built-in controls, parameter data types, and image buffers from 8-bit through 32-bit floating point.
- The AE SDK guide documents the effect entry point as a single host-called function receiving command selector, input data, output data, params, output layer, and extra data.
- AE SmartFX is required for 32-bit-per-channel AE rendering and uses a pre-render/render split where the plug-in declares needed input regions before rendering.
- `PF_EffectWorld` / `PF_LayerDef` row bytes must be honored. AE buffers are not guaranteed to be 16-byte aligned.
- GPU support is advertised through output flags such as `PF_OutFlag2_SUPPORTS_GPU_RENDER_F32` and handled by GPU selectors.

Implication for this repo: the AE target is optional and adapter-only. It will map AE worlds and controls into `rtk_core`, with SmartFX/GPU paths added behind the same core parameter contract.

## OpenFX

References checked during scaffold setup:

- OpenFX is a C API standard for image-processing plug-ins used by commercial hosts including Nuke and Resolve.
- Image effects identify themselves with `kOfxImageEffectPluginApi`.
- OFX effects expose clips and parameters during describe actions, then fetch clip images and parameter instances during render actions.
- Image instances are fetched through clip APIs and must be released before returning from the action.
- GPU/OpenCL rendering is optional and negotiated through host/plugin properties and GPU suites.

Implication for this repo: the OFX target is optional and adapter-only. It will define source/output clips and parameters, then translate OFX image planes into `rtk_core` descriptors.
