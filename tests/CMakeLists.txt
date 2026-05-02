add_executable(rtk_core_tests core_renderer_tests.cpp)
target_link_libraries(rtk_core_tests PRIVATE rtk::core)
add_test(NAME rtk_core_tests COMMAND rtk_core_tests)

if(MSVC)
  target_compile_options(rtk_core_tests PRIVATE /W4 /permissive-)
else()
  target_compile_options(rtk_core_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
