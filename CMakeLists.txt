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
add_test(NAME vector_capacity COMMAND vector_lifetime --experiment-b)
set_tests_properties(vector_capacity PROPERTIES TIMEOUT 10 LABELS cpp)
add_observation(vector_iterators labs/03_vector/iterators.cpp cpp)
add_executable(mini_vector labs/03_vector/mini_vector.cpp)
target_link_libraries(mini_vector PRIVATE lab_options Threads::Threads)
add_observation(container_observe labs/04_containers/observe.cpp cpp)
# Learner implementations are TODOs; run their checks manually until completed.
add_executable(mini_hash_map_test labs/04_containers/mini_hash_map_test.cpp)
target_link_libraries(mini_hash_map_test PRIVATE lab_options Threads::Threads)
add_executable(mini_bst_map_test labs/04_containers/mini_bst_map_test.cpp)
target_link_libraries(mini_bst_map_test PRIVATE lab_options Threads::Threads)
foreach(exercise IN ITEMS mini_list mini_heap ring_buffer mini_array mini_deque mini_rb_tree_map)
  add_executable(${exercise}_test labs/04_containers/${exercise}_test.cpp)
  target_link_libraries(${exercise}_test PRIVATE lab_options Threads::Threads)
endforeach()
add_custom_target(container_exercises DEPENDS
  mini_hash_map_test mini_bst_map_test mini_list_test mini_heap_test
  ring_buffer_test mini_array_test mini_deque_test mini_rb_tree_map_test)
add_observation(cv_handshake labs/05_mutex_cv/observe.cpp concurrency)
# L05 learner checks contain TODOs and run manually until the implementation is complete.
add_executable(sync_exercises_test labs/05_mutex_cv/sync_exercises_test.cpp)
target_link_libraries(sync_exercises_test PRIVATE lab_options Threads::Threads)
foreach(experiment IN ITEMS locks ordering notifications consumers timeout)
  add_test(NAME cv_${experiment} COMMAND cv_handshake --${experiment})
  set_tests_properties(cv_${experiment} PROPERTIES TIMEOUT 10 LABELS concurrency)
endforeach()
add_observation(stream_io labs/11_stream_framing/observe.cpp network)

# L06-L12: completed observations and separate learner implementations.
add_observation(atomic_observe labs/07_atomic_publish/observe.cpp concurrency)
add_observation(memory_observe labs/08_os_memory/observe.cpp memory)
add_observation(locality_observe labs/09_locality/observe.cpp performance)
add_observation(false_sharing_observe labs/10_false_sharing/observe.cpp performance)
add_observation(poll_readiness labs/12_event_loop/observe.cpp network)
function(add_learner_exercise target source)
  add_executable(${target} ${source})
  target_link_libraries(${target} PRIVATE lab_options Threads::Threads)
endfunction()
add_learner_exercise(bounded_queue_test labs/06_bounded_queue/bounded_queue_test.cpp)
add_learner_exercise(atomic_publication_test labs/07_atomic_publish/atomic_publication_test.cpp)
add_learner_exercise(memory_access_test labs/08_os_memory/page_exercises_test.cpp)
add_learner_exercise(locality_test labs/09_locality/locality_exercises_test.cpp)
add_learner_exercise(false_sharing_test labs/10_false_sharing/sharing_exercises_test.cpp)
add_learner_exercise(framing_test labs/11_stream_framing/frame_decoder_test.cpp)
add_learner_exercise(stream_io_test labs/11_stream_framing/stream_io_test.cpp)
add_learner_exercise(event_loop_test labs/12_event_loop/event_loop_test.cpp)
add_custom_target(followup_exercises DEPENDS
  bounded_queue_test atomic_publication_test memory_access_test locality_test
  false_sharing_test framing_test stream_io_test event_loop_test)
# These runners call learner TODOs; they are not completed CTest observations.
add_learner_exercise(memory_benchmark labs/08_os_memory/benchmark.cpp)
add_learner_exercise(locality_benchmark labs/09_locality/benchmark.cpp)
add_learner_exercise(false_sharing_benchmark labs/10_false_sharing/benchmark.cpp)
add_custom_target(followup_benchmarks DEPENDS
  memory_benchmark locality_benchmark false_sharing_benchmark)

# The ownership test is an executable; the learner implements UniquePtr in
# observer.cpp and uses this test to check ownership and lifetime behavior.
add_observation(unique_ownership_test labs/02_ownership/test.cpp cpp)

# Write new experiments here; scratch has no automatic correctness specification.
add_executable(scratch scratch/main.cpp)
target_link_libraries(scratch PRIVATE lab_options Threads::Threads)
