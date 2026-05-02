include(${PROJECT_SOURCE_DIR}/cmake/LodePNG.cmake)
rtk_add_lodepng()

add_executable(rtk_preview_cli main.cpp)

target_link_libraries(rtk_preview_cli PRIVATE rtk::core rtk_lodepng)

if(MSVC)
  target_compile_options(rtk_preview_cli PRIVATE /W4 /permissive-)
else()
  target_compile_options(rtk_preview_cli PRIVATE -Wall -Wextra -Wpedantic)
endif()
