cmake_minimum_required(VERSION 3.20)
project("260827-DHA_BT_Telecoil_PD5" C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(ENABLE_SANITIZERS "Enable ASan and UBSan on host tests" OFF)
option(ENABLE_COVERAGE "Enable GCC/Clang coverage instrumentation" OFF)

file(GLOB APP_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Application/Src/*.c")
file(GLOB UNIT_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Tests/test_*.c")

add_library(connectivity_app STATIC ${APP_SOURCES})
target_include_directories(connectivity_app PUBLIC Application/Inc)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  set(PROJECT_WARNINGS -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes)
  target_compile_options(connectivity_app PRIVATE ${PROJECT_WARNINGS})
endif()

add_executable(unity_tests
  Tests/Unity/unity.c
  ${UNIT_TEST_SOURCES}
  Host/host_test_main.c
)
target_link_libraries(unity_tests PRIVATE connectivity_app)
target_include_directories(unity_tests PRIVATE Tests Tests/Unity Application/Inc)
target_compile_definitions(unity_tests PRIVATE UNITY_INCLUDE_CONFIG_H)

add_executable(framework_free_selftest
  IndependentTests/Src/self_test_runner.c
  Host/host_selftest_main.c
)
target_link_libraries(framework_free_selftest PRIVATE connectivity_app)
target_include_directories(framework_free_selftest PRIVATE IndependentTests/Inc Application/Inc)

add_executable(connectivity_fuzz Host/fuzz_connectivity.c)
target_link_libraries(connectivity_fuzz PRIVATE connectivity_app)
target_include_directories(connectivity_fuzz PRIVATE Application/Inc)

foreach(target unity_tests framework_free_selftest connectivity_fuzz)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE ${PROJECT_WARNINGS})
  endif()
endforeach()

if(ENABLE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  foreach(target connectivity_app unity_tests framework_free_selftest connectivity_fuzz)
    target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  endforeach()
  foreach(target unity_tests framework_free_selftest connectivity_fuzz)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  endforeach()
endif()

if(ENABLE_COVERAGE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(connectivity_app PRIVATE --coverage -O0)
  foreach(target unity_tests framework_free_selftest connectivity_fuzz)
    target_compile_options(${target} PRIVATE --coverage -O0)
    target_link_options(${target} PRIVATE --coverage)
  endforeach()
endif()

enable_testing()
add_test(NAME unit_tests_256 COMMAND unity_tests)
add_test(NAME independent_selftest_52 COMMAND framework_free_selftest)
add_test(NAME deterministic_fuzz_100k COMMAND connectivity_fuzz)
