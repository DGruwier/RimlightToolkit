# Architecture

## Boundary Rule

`rtk_core` must not include After Effects SDK or OpenFX headers. It receives only plain C++ descriptors:

- `ImageView`
- `MutableImageView`
- `RenderParams`
- `RenderResult`

Host-specific concepts such as `PF_EffectWorld`, `PF_ParamDef`, `OfxImageEffectHandle`, clip handles, suite pointers, bundle layouts, PiPL resources, and installer conventions belong in adapter directories.

The core parameter header also owns the small control schema for the current renderer. Preview controls and host adapter parameters should read labels, keys, defaults, and ranges from that schema before adding host-specific behavior.

## Current Render Model

The current processing stack is intentionally minimal:

1. Read source RGBA pixels.
2. Multiply RGB by `RenderParams::color_multiplier.rgb`.
3. Multiply alpha by `RenderParams::color_multiplier.a * source_opacity`.
4. Write the result in the destination pixel format.

This clean baseline keeps the host-independent core easy to verify before more advanced image-processing logic is reintroduced.

## Host Adapters

After Effects and OFX adapters are thin translation layers. They define host controls and map host image buffers into `rtk_core` descriptors. Algorithm logic belongs in `rtk_core`, not in host adapters.

The AE scaffold currently advertises only the CPU render path it actually implements. SmartFX, AE 32-bpc world mapping, and GPU selectors should be added as adapter backends that still call through the same core parameter contract.

## Preview Harnesses

The CLI preview harness is dependency-light and writes PNG output. The Windows GUI preview supports PNG drag/drop, basic multiplier sliders, saving, and a benchmark mode. Both call the same core renderer. When no source is supplied, both previewers load `assets/test_images/test_case_john_01.png` and fall back to a synthetic gradient only if the asset is unavailable.
