cmake_minimum_required(VERSION 3.10)
project(MyLLVMPass)

find_package(LLVM REQUIRED CONFIG)

add_definitions(${LLVM_DEFINITIONS})
include_directories("/usr/lib/llvm-9/include")

add_library(opcodeCounterlib MODULE OpcodeCounter.cpp)