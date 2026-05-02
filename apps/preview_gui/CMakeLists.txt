include(${PROJECT_SOURCE_DIR}/cmake/LodePNG.cmake)
rtk_add_lodepng()

add_executable(rtk_preview_gui WIN32 main_win32.cpp)

target_link_libraries(rtk_preview_gui PRIVATE rtk::core rtk_lodepng user32 gdi32 shell32)

if(MSVC)
  target_compile_options(rtk_preview_gui PRIVATE /W4 /permissive-)
endif()
