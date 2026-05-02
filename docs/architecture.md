# Architecture

## Boundary Rule

`rtk_core` must never include After Effects SDK or OpenFX headers. It receives only plain C++ descriptors:

- `ImageView`
- `MutableImageView`
- `RenderParams`
- `RenderResult`

All host-specific concepts such as `PF_EffectWorld`, `PF_ParamDef`, `OfxImageEffectHandle`, clip handles, suite pointers, GPU device handles, bundle layouts, PiPL resources, and installer conventions belong in adapter directories.

## Render Model

The initial CPU path is generic matte transformation over RGBA pixels:

1. Sample the source alpha.
2. Scale the alpha field around a transform origin controlled by the host or preview canvas.
3. Invert the transformed alpha.
4. Matte that inverse alpha with the original alpha.
5. Use the resulting mask to stencil a user color and opacity.
6. Alpha-over the fill on top of the original source image.
7. Write the result in the destination pixel format.

The renderer clips reads/writes to the supplied descriptors and honors row stride, pixel stride, and channel depth.

## Host Adapters

### After Effects

The AE adapter owns:

- Effect Controls parameter definition.
- PiPL/resource metadata.
- 8/16/32-bpc `PF_EffectWorld` mapping.
- SmartFX pre-render/render expansion for blur and offset.
- GPU selector plumbing once backend kernels exist.
- Commercial packaging concerns such as plug-in naming, versioning, and install paths.

The AE adapter calls the core renderer with normalized descriptors. It should not own algorithm state beyond parameter conversion and host lifecycle bookkeeping.

### OpenFX

The OFX adapter owns:

- OFX image-effect description actions.
- Source/output clip definition.
- Parameter definition/fetching.
- Render window mapping.
- Host capability negotiation.
- Optional OpenCL/CUDA/Metal dispatch once backend kernels exist.

The OFX adapter should be portable across hosts, while allowing host quirks to be isolated in adapter helpers.

## Preview/Test Harness

The CLI preview harness is deliberately dependency-light. It provides repeatable output for quick visual checks and CI. A later GUI harness can wrap the same API with slider controls without touching AE or OFX code.
