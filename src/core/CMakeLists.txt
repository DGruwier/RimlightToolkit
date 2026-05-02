add_library(rtk_core STATIC
  Renderer.cpp)

add_library(rtk::core ALIAS rtk_core)

set_target_properties(rtk_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(rtk_core
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include)

target_compile_features(rtk_core PUBLIC cxx_std_17)

if(MSVC)
  target_compile_options(rtk_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(rtk_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
