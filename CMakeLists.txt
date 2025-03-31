cmake_minimum_required(VERSION 3.30)
project(OS_project)

set(CMAKE_CXX_STANDARD 20)

add_executable(OS_project main.cpp)

add_executable(VDIFileTest step-1/VDIFileTest.cpp
        step-1/VDIFile.cpp
        step-1/VDIFile.h)

add_executable(MBRPartitionTest step-2/MBRPartitionTest.cpp
        step-2/MBRPartition.cpp
        step-2/MBRPartition.h
        step-1/VDIFile.cpp
        step-1/VDIFile.h)