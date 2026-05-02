include(FetchContent)

function(rtk_add_lodepng)
  if(TARGET rtk_lodepng)
    return()
  endif()

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
endfunction()
