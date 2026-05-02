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
constexpr const char* kTransformOrigin = "transformOrigin";
constexpr const char* kAlphaScale = "alphaScale";
constexpr const char* kFillColor = "fillColor";
constexpr const char* kFillOpacity = "fillOpacity";

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
  g_parameter->paramDefine(params, kOfxParamTypeDouble2D, kTransformOrigin, &offset_props);
  g_property->propSetString(offset_props, kOfxPropLabel, 0, "Light Position");
  g_property->propSetDouble(offset_props, kOfxParamPropDefault, 0, 12.0);
  g_property->propSetDouble(offset_props, kOfxParamPropDefault, 1, 12.0);

  define_double_param(params, kAlphaScale, "Alpha Scale", 1.08, 1.0, 4.0);
  define_double_param(params, kFillOpacity, "Fill Opacity", 0.75, 0.0, 1.0);

  OfxPropertySetHandle fill_color_props = nullptr;
  g_parameter->paramDefine(params, kOfxParamTypeRGB, kFillColor, &fill_color_props);
  g_property->propSetString(fill_color_props, kOfxPropLabel, 0, "Fill Color");
  g_property->propSetDouble(fill_color_props, kOfxParamPropDefault, 0, 1.0);
  g_property->propSetDouble(fill_color_props, kOfxParamPropDefault, 1, 1.0);
  g_property->propSetDouble(fill_color_props, kOfxParamPropDefault, 2, 1.0);

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
  result.mode = rtk::core::LightMode::Point;
  OfxParamHandle handle = nullptr;
  double x = 0.0;
  double y = 0.0;
  double value = 0.0;
  double r = 0.0;
  double g = 0.0;
  double b = 0.0;

  g_parameter->paramGetHandle(params, kTransformOrigin, &handle, nullptr);
  g_parameter->paramGetValueAtTime(handle, time, &x, &y);
  result.transform_origin_x = static_cast<float>(x);
  result.transform_origin_y = static_cast<float>(y);

  g_parameter->paramGetHandle(params, kAlphaScale, &handle, nullptr);
  g_parameter->paramGetValueAtTime(handle, time, &value);
  result.alpha_scale = static_cast<float>(value);

  g_parameter->paramGetHandle(params, kFillOpacity, &handle, nullptr);
  g_parameter->paramGetValueAtTime(handle, time, &value);
  result.fill_opacity = static_cast<float>(value);

  g_parameter->paramGetHandle(params, kFillColor, &handle, nullptr);
  g_parameter->paramGetValueAtTime(handle, time, &r, &g, &b);
  result.fill_color.r = static_cast<float>(r);
  result.fill_color.g = static_cast<float>(g);
  result.fill_color.b = static_cast<float>(b);
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
