cmake_minimum_required(VERSION 3.25)
project(quant_dev_interview_lab LANGUAGES CXX)

option(ENABLE_ASAN "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
if(ENABLE_ASAN AND ENABLE_TSAN)
  message(FATAL_ERROR "Enable only one sanitizer at a time")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# An in-source build otherwise tries to write an executable over scratch/.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/build/in-source")
endif()
find_package(Threads REQUIRED)

add_library(lab_options INTERFACE)
target_include_directories(lab_options INTERFACE include)
target_compile_options(lab_options INTERFACE -Wall -Wextra -Wpedantic -Wconversion)
if(ENABLE_ASAN)
  target_compile_options(lab_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(lab_options INTERFACE -fsanitize=address,undefined)
elseif(ENABLE_TSAN)
  target_compile_options(lab_options INTERFACE -fsanitize=thread -fno-omit-frame-pointer)
  target_link_options(lab_options INTERFACE -fsanitize=thread)
endif()

enable_testing()
function(add_observation target source label)
  add_executable(${target} ${source})
  target_link_libraries(${target} PRIVATE lab_options Threads::Threads)
  add_test(NAME ${target} COMMAND ${target})
  set_tests_properties(${target} PROPERTIES TIMEOUT 10 LABELS ${label})
endfunction()

add_observation(object_model labs/01_object_model/observe.cpp cpp)
add_observation(vector_lifetime labs/03_vector/observe.cpp cpp)
add_observation(cv_handshake labs/05_mutex_cv/observe.cpp concurrency)
add_observation(stream_io labs/11_stream_framing/observe.cpp network)

# The ownership test is an executable; the learner implements UniquePtr in
# observer.cpp and uses this test to check ownership and lifetime behavior.
add_observation(unique_ownership_test labs/02_ownership/test.cpp cpp)

# Write new experiments here; scratch has no automatic correctness specification.
add_executable(scratch scratch/main.cpp)
target_link_libraries(scratch PRIVATE lab_options Threads::Threads)
