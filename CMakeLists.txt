cmake_minimum_required(VERSION 3.24)

project(DOR2 VERSION 2.0.0 LANGUAGES CXX)

include(FetchContent)

option(DOR2_BUILD_TESTS "Build DOR2 tests" ON)
option(DOR2_BUILD_BENCHMARK "Build DOR2 benchmark" ON)
option(DOR2_BUILD_GUI "Build the wxWidgets graphical interface" ON)
option(DOR2_ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)

FetchContent_Declare(
    treesetlab
    GIT_REPOSITORY https://github.com/VenEraBle842/AD3.git
    GIT_TAG 406eff221ae7db82aef6d9ccfec0360203600aaa
    GIT_SHALLOW FALSE
    SOURCE_SUBDIR dor2-no-upstream-cmake
)
FetchContent_MakeAvailable(treesetlab)

add_library(treesetlab_core INTERFACE)
add_library(TreeSetLab::Core ALIAS treesetlab_core)
target_include_directories(treesetlab_core INTERFACE "${treesetlab_SOURCE_DIR}")

add_library(dor2_core
    src/benchmark_runner.cpp
    src/student_index_catalog.cpp
    src/student_file_storage.cpp
    src/student_application.cpp
)
target_compile_features(dor2_core PUBLIC cxx_std_23)
target_include_directories(dor2_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(dor2_core PUBLIC TreeSetLab::Core)

function(dor2_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
endfunction()

dor2_set_warnings(dor2_core)

if(DOR2_ENABLE_SANITIZERS AND NOT MSVC)
    target_compile_options(dor2_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(dor2_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
endif()

add_executable(dor2 src/main.cpp)
target_link_libraries(dor2 PRIVATE dor2_core)
dor2_set_warnings(dor2)

if(DOR2_BUILD_GUI)
    find_package(wxWidgets 3.2 REQUIRED COMPONENTS core base)
    find_package(Threads REQUIRED)
    add_library(dor2_gui_components src/gui/main_frame.cpp)
    target_link_libraries(dor2_gui_components PUBLIC dor2_core Threads::Threads)
    if(TARGET wxWidgets::wxWidgets)
        target_link_libraries(dor2_gui_components PUBLIC wxWidgets::wxWidgets)
    else()
        target_link_libraries(dor2_gui_components PUBLIC ${wxWidgets_LIBRARIES})
        target_link_directories(dor2_gui_components PUBLIC ${wxWidgets_LIBRARY_DIRS})
        target_include_directories(dor2_gui_components SYSTEM PUBLIC ${wxWidgets_INCLUDE_DIRS})
        target_compile_definitions(dor2_gui_components PUBLIC ${wxWidgets_DEFINITIONS})
        separate_arguments(dor2_wx_flags NATIVE_COMMAND "${wxWidgets_CXX_FLAGS}")
        target_compile_options(dor2_gui_components PUBLIC ${dor2_wx_flags})
    endif()
    dor2_set_warnings(dor2_gui_components)
    add_executable(dor2_gui src/gui/app.cpp)
    target_link_libraries(dor2_gui PRIVATE dor2_gui_components)
    dor2_set_warnings(dor2_gui)
endif()

if(DOR2_BUILD_BENCHMARK)
    add_executable(dor2_benchmark benchmark/benchmark_runner.cpp)
    target_link_libraries(dor2_benchmark PRIVATE dor2_core)
    dor2_set_warnings(dor2_benchmark)
endif()

if(DOR2_BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
    add_executable(dor2_tests
        tests/test_tree_dictionary.cpp
        tests/test_sorted_sequence.cpp
        tests/test_binary_search.cpp
        tests/test_student_indexes.cpp
        tests/test_file_storage.cpp
        tests/test_search_service.cpp
        tests/test_application.cpp
    )
    target_link_libraries(dor2_tests PRIVATE dor2_core GTest::gtest GTest::gtest_main)
    dor2_set_warnings(dor2_tests)
    include(GoogleTest)
    gtest_discover_tests(dor2_tests)
endif()
