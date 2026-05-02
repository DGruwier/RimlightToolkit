#pragma once

#include <string>

#include "rtk/core/Image.hpp"
#include "rtk/core/Parameters.hpp"

namespace rtk::core {

enum class RenderStatus {
  Ok,
  InvalidInput,
  UnsupportedFormat,
};

struct RenderResult {
  RenderStatus status = RenderStatus::Ok;
  std::string message;
};

RenderResult render(const ImageView& source,
                    const MutableImageView& destination,
                    const RenderParams& params);

const char* to_string(RenderStatus status) noexcept;

}  // namespace rtk::core
