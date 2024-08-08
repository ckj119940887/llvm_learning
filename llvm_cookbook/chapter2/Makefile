CUR_DIR = $(shell pwd)
LLVM_SRC := ${CUR_DIR}/../llvm-5.0.0.src/llvm
#cmd:
#clang++ -g toy.cpp `../../llvm/build/bin/llvm-config --cxxflags --ldflags --system-libs --libs core mcjit native` -O0 -o toy

CC = g++
SOURCE = toy.cpp
TARGET = toy

$(TARGET) : $(SOURCE)
	$(CC) -g $(SOURCE) `llvm-config-9 --cxxflags --ldflags --system-libs --libs core mcjit native` -O0 -o $(TARGET)

clean :
	rm $(TARGET)