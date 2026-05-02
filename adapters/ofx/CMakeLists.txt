include(${PROJECT_SOURCE_DIR}/cmake/OpenFXSDK.cmake)
rtk_find_openfx_sdk()

add_library(RimlightToolkit_OFX MODULE
  RimlightOFX.cpp)

target_link_libraries(RimlightToolkit_OFX PRIVATE rtk::core)
target_include_directories(RimlightToolkit_OFX PRIVATE ${OPENFX_INCLUDE_DIR})
target_compile_definitions(RimlightToolkit_OFX PRIVATE RTK_OFX_ADAPTER=1)

set_target_properties(RimlightToolkit_OFX PROPERTIES
  PREFIX ""
  OUTPUT_NAME "RimlightToolkit")

if(WIN32)
  set_target_properties(RimlightToolkit_OFX PROPERTIES SUFFIX ".ofx")
endif()
