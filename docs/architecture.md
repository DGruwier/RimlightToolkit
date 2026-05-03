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

The active renderer is an AE-operation-parity mask stack:

1. Extract source alpha.
2. Offset the mask using directional pixels or point-source scaling.
3. Run max-blur occlusion along the light direction with an explicit slice count.
4. Apply iterative box blur.
5. Invert the mask.
6. Matte by original alpha.
7. Convert the mask into a solid-color layer.
8. Composite the color layer over the source.

Every named stage can be selected through `DebugView`; single-channel debug stages render as opaque grayscale.

## Host Adapters

After Effects and OFX adapters are future-ready scaffolds only during this phase. Algorithm logic belongs in `rtk_core`, not in host adapters. Default CI does not build host adapters while the core stack is being iterated.

## Preview Harnesses

The CLI preview harness is dependency-light and writes PNG output. The Windows GUI preview supports PNG drag/drop, debug-stage selection, stack controls, stage toggles, canvas drag positioning, saving, and a benchmark mode. Both call the same core renderer. When no source is supplied, both previewers load `assets/test_images/test_case_john_01.png` and fall back to a synthetic gradient only if the asset is unavailable.
