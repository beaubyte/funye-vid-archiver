cmake_minimum_required(VERSION 4.1)
project(funye_vid_archiver)

set(CMAKE_CXX_STANDARD 20)
add_executable(funye_vid_archiver main.cpp)
find_package(DPP REQUIRED)
target_link_libraries(funye_vid_archiver PRIVATE dpp::dpp)