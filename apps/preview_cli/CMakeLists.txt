include(FetchContent)

FetchContent_Declare(
  lodepng
  GIT_REPOSITORY https://github.com/lvandeve/lodepng.git
  GIT_TAG 22561883dd63fd1850f18e1f6adac321e4f609b0)

FetchContent_GetProperties(lodepng)
if(NOT lodepng_POPULATED)
  FetchContent_Populate(lodepng)
endif()

add_library(rtk_lodepng STATIC ${lodepng_SOURCE_DIR}/lodepng.cpp)
target_include_directories(rtk_lodepng PUBLIC ${lodepng_SOURCE_DIR})

add_executable(rtk_preview_cli main.cpp)

target_link_libraries(rtk_preview_cli PRIVATE rtk::core rtk_lodepng)

if(MSVC)
  target_compile_options(rtk_preview_cli PRIVATE /W4 /permissive-)
else()
  target_compile_options(rtk_preview_cli PRIVATE -Wall -Wextra -Wpedantic)
endif()
