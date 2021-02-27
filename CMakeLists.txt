cmake_minimum_required(VERSION 3.14.0)

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)

project(ha-effect-collection)

add_subdirectory(external)

add_library(effect-collection STATIC
    include/ha/effect_collection/types.h
    include/ha/effect_collection/trance_gate.h
    source/trance_gate.cpp
)

target_link_libraries(effect-collection 
    PRIVATE
        dsp-tool-box
)

target_include_directories(effect-collection
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/source
)

enable_testing()

add_executable(effect-collection_test
    test/trance_gate_test.cpp
)

target_link_libraries(effect-collection_test
    PRIVATE
        effect-collection
        gtest
        gtest_main
)