# compiler command
```
mkdir build
cd build
cmake ../
cmake --build ./
```

```
生成的libfuncBlockCountlib.so位于build目录下
```

# compile sample.c
```
clang-9 -O0 -S -emit-llvm sample.c -o sample.ll
opt-9 -load build/libfuncBlockCountlib.so --func-block-count sample.ll

其中参数--func-block-count是由自己的pass注册时声明的
```

```
-disable-output 
-debug-pass=Structure
LLVM 的 Pass 管理器提供了 Pass 调试选项，因此我们能够看到我们的 Pass 使用了哪些分析和优化
```