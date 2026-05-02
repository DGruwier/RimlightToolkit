#include "rtk/core/Renderer.hpp"

#include "ofxCore.h"
#include "ofxImageEffect.h"
#include "ofxMemory.h"
#include "ofxParam.h"
#include "ofxPixels.h"
#include "ofxProperty.h"

#include <cstring>

namespace {

OfxHost* g_host = nullptr;
OfxImageEffectSuiteV1* g_image_effect = nullptr;
OfxPropertySuiteV1* g_property = nullptr;
OfxParameterSuiteV1* g_parameter = nullptr;

constexpr const char* kPluginIdentifier = "com.dgruwier.rimlighttoolkit";
constexpr const char* kSourceClip = "Source";
constexpr const char* kOutputClip = "Output";
constexpr const char* kShadowOffset = "shadowOffset";
constexpr const char* kShadowBlur = "shadowBlur";
constexpr const char* kShadowColor = "shadowColor";
constexpr const char* kRimWidth = "rimWidth";
constexpr const char* kRimIntensity = "rimIntensity";
constexpr const char* kRimColor = "rimColor";

bool fetch_suites() {
  if (!g_host || !g_host->fetchSuite) {
    return false;
  }
  g_image_effect = static_cast<OfxImageEffectSuiteV1*>(
      g_host->fetchSuite(g_host->host, kOfxImageEffectSuite, 1));
  g_property = static_cast<OfxPropertySuiteV1*>(
      g_host->fetchSuite(g_host->host, kOfxPropertySuite, 1));
  g_parameter = static_cast<OfxParameterSuiteV1*>(
      g_host->fetchSuite(g_host->host, kOfxParameterSuite, 1));
  return g_image_effect && g_property && g_parameter;
}

void define_double_param(OfxParamSetHandle params,
                         const char* name,
                         const char* label,
                         double default_value,
                         double min,
                         double max) {
  OfxPropertySetHandle props = nullptr;
  g_parameter->paramDefine(params, kOfxParamTypeDouble, name, &props);
  g_property->propSetString(props, kOfxPropLabel, 0, label);
  g_property->propSetDouble(props, kOfxParamPropDefault, 0, default_value);
  g_property->propSetDouble(props, kOfxParamPropMin, 0, min);
  g_property->propSetDouble(props, kOfxParamPropMax, 0, max);
  g_property->propSetDouble(props, kOfxParamPropDisplayMin, 0, min);
  g_property->propSetDouble(props, kOfxParamPropDisplayMax, 0, max);
}

OfxStatus describe(OfxImageEffectHandle effect) {
  OfxPropertySetHandle props = nullptr;
  g_image_effect->getPropertySet(effect, &props);
  g_property->propSetString(props, kOfxPropLabel, 0, "Rimlight Toolkit");
  g_property->propSetString(props, kOfxImageEffectPluginPropGrouping, 0, "Rimlight Toolkit");
  g_property->propSetString(props, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthByte);
  g_property->propSetString(props, kOfxImageEffectPropSupportedPixelDepths, 1, kOfxBitDepthShort);
  g_property->propSetString(props, kOfxImageEffectPropSupportedPixelDepths, 2, kOfxBitDepthFloat);
  g_property->propSetString(props, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
  return kOfxStatOK;
}

OfxStatus describe_in_context(OfxImageEffectHandle effect) {
  OfxImageClipHandle source_clip = nullptr;
  OfxImageClipHandle output_clip = nullptr;
  g_image_effect->clipDefine(effect, kSourceClip, &source_clip);
  g_image_effect->clipDefine(effect, kOutputClip, &output_clip);

  OfxPropertySetHandle source_props = nullptr;
  OfxPropertySetHandle output_props = nullptr;
  g_image_effect->clipGetPropertySet(source_clip, &source_props);
  g_image_effect->clipGetPropertySet(output_clip, &output_props);
  g_property->propSetString(source_props, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
  g_property->propSetString(output_props, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);

  OfxParamSetHandle params = nullptr;
  g_image_effect->getParamSet(effect, &params);

  OfxPropertySetHandle offset_props = nullptr;
  g_parameter->paramDefine(params, kOfxParamTypeDouble2D, kShadowOffset, &offset_props);
  g_property->propSetString(offset_props, kOfxPropLabel, 0, "Shadow Offset");
  g_property->propSetDouble(offset_props, kOfxParamPropDefault, 0, 12.0);
  g_property->propSetDouble(offset_props, kOfxParamPropDefault, 1, 12.0);

  define_double_param(params, kShadowBlur, "Shadow Blur", 10.0, 0.0, 128.0);
  define_double_param(params, kRimWidth, "Rim Width", 2.0, 0.0, 64.0);
  define_double_param(params, kRimIntensity, "Rim Intensity", 0.35, 0.0, 4.0);

  OfxPropertySetHandle shadow_color_props = nullptr;
  g_parameter->paramDefine(params, kOfxParamTypeRGB, kShadowColor, &shadow_color_props);
  g_property->propSetString(shadow_color_props, kOfxPropLabel, 0, "Shadow Color");
  g_property->propSetDouble(shadow_color_props, kOfxParamPropDefault, 0, 0.0);
  g_property->propSetDouble(shadow_color_props, kOfxParamPropDefault, 1, 0.0);
  g_property->propSetDouble(shadow_color_props, kOfxParamPropDefault, 2, 0.0);

  OfxPropertySetHandle rim_color_props = nullptr;
  g_parameter->paramDefine(params, kOfxParamTypeRGB, kRimColor, &rim_color_props);
  g_property->propSetString(rim_color_props, kOfxPropLabel, 0, "Rim Color");
  g_property->propSetDouble(rim_color_props, kOfxParamPropDefault, 0, 1.0);
  g_property->propSetDouble(rim_color_props, kOfxParamPropDefault, 1, 1.0);
  g_property->propSetDouble(rim_color_props, kOfxParamPropDefault, 2, 1.0);

  return kOfxStatOK;
}

rtk::core::PixelFormat format_from_depth(const char* bit_depth) {
  if (std::strcmp(bit_depth, kOfxBitDepthShort) == 0) {
    return rtk::core::PixelFormat::RgbaU16;
  }
  if (std::strcmp(bit_depth, kOfxBitDepthFloat) == 0) {
    return rtk::core::PixelFormat::RgbaF32;
  }
  return rtk::core::PixelFormat::RgbaU8;
}

rtk::core::RenderParams read_params(OfxImageEffectHandle effect, double time) {
  OfxParamSetHandle params = nullptr;
  g_image_effect->getParamSet(effect, &params);

  rtk::core::RenderParams result;
  OfxParamHandle handle = nullptr;
  double x = 0.0;
  double y = 0.0;
  double value = 0.0;
  double r = 0.0;
  double g = 0.0;
  double b = 0.0;

  g_parameter->paramGetHandle(params, kShadowOffset, &handle, nullptr);
  g_parameter->paramGetValueAtTime(handle, time, &x, &y);
  result.shadow_offset_x = static_cast<float>(x);
  result.shadow_offset_y = static_cast<float>(y);

  g_parameter->paramGetHandle(params, kShadowBlur, &handle, nullptr);
  g_parameter->paramGetValueAtTime(handle, time, &value);
  result.shadow_blur_radius = static_cast<float>(value);

  g_parameter->paramGetHandle(params, kRimWidth, &handle, nullptr);
  g_parameter->paramGetValueAtTime(handle, time, &value);
  result.rim_width = static_cast<float>(value);

  g_parameter->paramGetHandle(params, kRimIntensity, &handle, nullptr);
  g_parameter->paramGetValueAtTime(handle, time, &value);
  result.rim_intensity = static_cast<float>(value);

  g_parameter->paramGetHandle(params, kShadowColor, &handle, nullptr);
  g_parameter->paramGetValueAtTime(handle, time, &r, &g, &b);
  result.shadow_color.r = static_cast<float>(r);
  result.shadow_color.g = static_cast<float>(g);
  result.shadow_color.b = static_cast<float>(b);

  g_parameter->paramGetHandle(params, kRimColor, &handle, nullptr);
  g_parameter->paramGetValueAtTime(handle, time, &r, &g, &b);
  result.rim_color.r = static_cast<float>(r);
  result.rim_color.g = static_cast<float>(g);
  result.rim_color.b = static_cast<float>(b);
  return result;
}

OfxStatus render(OfxImageEffectHandle effect, OfxPropertySetHandle in_args) {
  double time = 0.0;
  g_property->propGetDouble(in_args, kOfxPropTime, 0, &time);

  OfxImageClipHandle source_clip = nullptr;
  OfxImageClipHandle output_clip = nullptr;
  g_image_effect->clipGetHandle(effect, kSourceClip, &source_clip, nullptr);
  g_image_effect->clipGetHandle(effect, kOutputClip, &output_clip, nullptr);

  OfxPropertySetHandle source_image = nullptr;
  OfxPropertySetHandle output_image = nullptr;
  if (g_image_effect->clipGetImage(source_clip, time, nullptr, &source_image) != kOfxStatOK ||
      g_image_effect->clipGetImage(output_clip, time, nullptr, &output_image) != kOfxStatOK) {
    return kOfxStatFailed;
  }

  void* source_data = nullptr;
  void* output_data = nullptr;
  int source_row_bytes = 0;
  int output_row_bytes = 0;
  OfxRectI bounds{};
  char* bit_depth = nullptr;

  g_property->propGetPointer(source_image, kOfxImagePropData, 0, &source_data);
  g_property->propGetPointer(output_image, kOfxImagePropData, 0, &output_data);
  g_property->propGetInt(source_image, kOfxImagePropRowBytes, 0, &source_row_bytes);
  g_property->propGetInt(output_image, kOfxImagePropRowBytes, 0, &output_row_bytes);
  g_property->propGetIntN(output_image, kOfxImagePropBounds, 4, &bounds.x1);
  g_property->propGetString(output_image, kOfxImageEffectPropPixelDepth, 0, &bit_depth);

  const int width = bounds.x2 - bounds.x1;
  const int height = bounds.y2 - bounds.y1;
  const auto format = format_from_depth(bit_depth);

  const rtk::core::ImageView source{
      source_data, width, height, source_row_bytes, format};
  const rtk::core::MutableImageView destination{
      output_data, width, height, output_row_bytes, format};

  const auto result = rtk::core::render(source, destination, read_params(effect, time));

  g_image_effect->clipReleaseImage(source_image);
  g_image_effect->clipReleaseImage(output_image);
  return result.status == rtk::core::RenderStatus::Ok ? kOfxStatOK : kOfxStatFailed;
}

OfxStatus main_entry(const char* action,
                     const void* handle,
                     OfxPropertySetHandle in_args,
                     OfxPropertySetHandle /*out_args*/) {
  if (std::strcmp(action, kOfxActionLoad) == 0) {
    return fetch_suites() ? kOfxStatOK : kOfxStatFailed;
  }
  if (std::strcmp(action, kOfxActionDescribe) == 0) {
    return describe(static_cast<OfxImageEffectHandle>(const_cast<void*>(handle)));
  }
  if (std::strcmp(action, kOfxImageEffectActionDescribeInContext) == 0) {
    return describe_in_context(static_cast<OfxImageEffectHandle>(const_cast<void*>(handle)));
  }
  if (std::strcmp(action, kOfxImageEffectActionRender) == 0) {
    return render(static_cast<OfxImageEffectHandle>(const_cast<void*>(handle)), in_args);
  }
  return kOfxStatReplyDefault;
}

OfxPlugin g_plugin = {
    kOfxImageEffectPluginApi,
    1,
    kPluginIdentifier,
    0,
    1,
    nullptr,
    main_entry,
};

}  // namespace

extern "C" OfxExport int OfxGetNumberOfPlugins() {
  return 1;
}

extern "C" OfxExport OfxPlugin* OfxGetPlugin(int index) {
  return index == 0 ? &g_plugin : nullptr;
}

extern "C" OfxExport void OfxSetHost(OfxHost* host) {
  g_host = host;
}
