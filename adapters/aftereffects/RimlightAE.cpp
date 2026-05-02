#include "rtk/core/Renderer.hpp"

#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "Param_Utils.h"

namespace {

enum ParamId {
  kInput = 0,
  kMultiplierColor,
  kAlphaMultiplier,
  kParamCount
};

rtk::core::PixelFormat pixel_format_for_world(const PF_EffectWorld& world) {
  if (PF_WORLD_IS_DEEP(&world)) {
    return rtk::core::PixelFormat::RgbaU16;
  }
  return rtk::core::PixelFormat::RgbaU8;
}

rtk::core::RenderParams read_params(PF_ParamDef* params[]) {
  rtk::core::RenderParams result;

  const auto color = params[kMultiplierColor]->u.cd.value;
  result.color_multiplier = {
      color.red / 255.0f,
      color.green / 255.0f,
      color.blue / 255.0f,
      1.0f,
  };
  result.color_multiplier.a = static_cast<float>(params[kAlphaMultiplier]->u.fs_d.value) / 65536.0f;

  return result;
}

PF_Err global_setup(PF_OutData* out_data) {
  out_data->my_version = PF_VERSION(0, 1, 0, 0, 1);
  out_data->out_flags = PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_USE_OUTPUT_EXTENT;
  out_data->out_flags2 = PF_OutFlag2_FLOAT_COLOR_AWARE |
                         PF_OutFlag2_SUPPORTS_SMART_RENDER |
                         PF_OutFlag2_SUPPORTS_THREADED_RENDERING;
  return PF_Err_NONE;
}

PF_Err params_setup(PF_InData* in_data, PF_OutData* out_data) {
  PF_Err err = PF_Err_NONE;
  PF_ParamDef def;

  AEFX_CLR_STRUCT(def);
  PF_ADD_COLOR("Color Multiplier", 255, 255, 255, kMultiplierColor);

  AEFX_CLR_STRUCT(def);
  PF_ADD_FLOAT_SLIDERX("Alpha Multiplier", 0, 1, 0, 1, 1, PF_Precision_HUNDREDTHS, 0, 0, kAlphaMultiplier);

  out_data->num_params = kParamCount;
  return err;
}

PF_Err render_cpu(PF_ParamDef* params[], PF_LayerDef* output) {
  auto* input_world = &params[kInput]->u.ld;
  const rtk::core::ImageView source{
      input_world->data,
      static_cast<int>(input_world->width),
      static_cast<int>(input_world->height),
      input_world->rowbytes,
      pixel_format_for_world(*input_world),
  };
  const rtk::core::MutableImageView destination{
      output->data,
      static_cast<int>(output->width),
      static_cast<int>(output->height),
      output->rowbytes,
      pixel_format_for_world(*output),
  };

  const auto result = rtk::core::render(source, destination, read_params(params));
  return result.status == rtk::core::RenderStatus::Ok ? PF_Err_NONE : PF_Err_BAD_CALLBACK_PARAM;
}

}  // namespace

extern "C" DllExport PF_Err EffectMain(PF_Cmd cmd,
                                       PF_InData* in_data,
                                       PF_OutData* out_data,
                                       PF_ParamDef* params[],
                                       PF_LayerDef* output,
                                       void* extra) {
  switch (cmd) {
    case PF_Cmd_GLOBAL_SETUP:
      return global_setup(out_data);
    case PF_Cmd_PARAM_SETUP:
      return params_setup(in_data, out_data);
    case PF_Cmd_RENDER:
      return render_cpu(params, output);
    case PF_Cmd_SMART_PRE_RENDER:
      return PF_Err_NONE;
    case PF_Cmd_SMART_RENDER:
      return PF_Err_NONE;
    case PF_Cmd_GPU_DEVICE_SETUP:
    case PF_Cmd_SMART_RENDER_GPU:
      return PF_Err_UNRECOGNIZED_PARAM_TYPE;
    default:
      return PF_Err_NONE;
  }
}
