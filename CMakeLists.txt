# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(my_clang_tool VERSION 0.1.0)

find_package(Clang CONFIG REQUIRED)

add_executable(my_clang_tool main.cpp)
target_include_directories(my_clang_tool PRIVATE ${CLANG_INCLUDE_DIRS})
target_link_libraries(my_clang_tool PRIVATE libclang)
