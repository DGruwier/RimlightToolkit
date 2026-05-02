cmake_minimum_required(VERSION 3.24)

project(RimlightToolkit
  VERSION 0.1.0
  DESCRIPTION "Host-independent shadow and rimlight renderer with After Effects and OFX adapters"
  LANGUAGES CXX)

option(RTK_BUILD_PREVIEW_CLI "Build the preview/test CLI harness" ON)
option(RTK_BUILD_PREVIEW_GUI "Build the native interactive preview app where supported" ON)
option(RTK_BUILD_TESTS "Build core renderer tests" ON)
option(RTK_BUILD_AE_PLUGIN "Build the After Effects adapter. Requires AE_SDK_ROOT." OFF)
option(RTK_BUILD_OFX_PLUGIN "Build the OpenFX adapter. Requires OPENFX_SDK_ROOT or fetched headers." OFF)
option(RTK_FETCH_OPENFX "Fetch OpenFX headers when OPENFX_SDK_ROOT is not set" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(src/core)

if(RTK_BUILD_PREVIEW_CLI)
  add_subdirectory(apps/preview_cli)
endif()

if(RTK_BUILD_PREVIEW_GUI AND WIN32)
  add_subdirectory(apps/preview_gui)
endif()

if(RTK_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(RTK_BUILD_AE_PLUGIN)
  add_subdirectory(adapters/aftereffects)
endif()

if(RTK_BUILD_OFX_PLUGIN)
  add_subdirectory(adapters/ofx)
endif()
