include(${PROJECT_SOURCE_DIR}/cmake/AfterEffectsSDK.cmake)
rtk_find_aftereffects_sdk()

add_library(RimlightToolkit_AE MODULE
  RimlightAE.cpp)

target_link_libraries(RimlightToolkit_AE PRIVATE rtk::core)
target_include_directories(RimlightToolkit_AE PRIVATE ${AE_SDK_INCLUDE_DIR})
target_compile_definitions(RimlightToolkit_AE PRIVATE RTK_AE_ADAPTER=1)

set_target_properties(RimlightToolkit_AE PROPERTIES
  PREFIX ""
  OUTPUT_NAME "RimlightToolkit")

if(WIN32)
  set_target_properties(RimlightToolkit_AE PROPERTIES SUFFIX ".aex")
elseif(APPLE)
  set_target_properties(RimlightToolkit_AE PROPERTIES BUNDLE TRUE SUFFIX ".plugin")
endif()
