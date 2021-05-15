cmake_minimum_required(VERSION 3.14.0)

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)

project(ha-fx-collection)

add_subdirectory(external)

add_library(fx-collection STATIC
    include/ha/fx_collection/types.h
    include/ha/fx_collection/trance_gate.h
    source/trance_gate.cpp
    source/detail/shuffle_note.cpp
    source/detail/shuffle_note.h
)

target_link_libraries(fx-collection 
    PUBLIC
        dsp-tool-box
)

target_include_directories(fx-collection
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/source
)

target_compile_features(fx-collection
    PUBLIC
        cxx_std_17
)

enable_testing()

add_executable(fx-collection_test
    test/trance_gate_test.cpp
    test/array_alignment_test.cpp
)

target_include_directories(fx-collection_test
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/source
)

target_link_libraries(fx-collection_test
    PRIVATE
        fx-collection
        gtest
        gtest_main
)