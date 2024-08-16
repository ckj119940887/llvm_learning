# buld steps (下面指的文件是llvm源码树中的文件)
```
在 include/llvm/LinkAllPasses.h 文件中添加 createFuncBlockCount Pass函数：
(void) llvm:: createFuncBlockCountPass ();
```

```
在 include/llvm/Transforms/Scalar.h 文件中添加声明：
Pass * createFuncBlockCountPass ();
```

```
在 lib/Transforms/Scalar/Scalar.cpp 文件中增加初始化 Pass 的条目：
initializeFuncBlockCountPass (Registry);
```

```
在 include/llvm/InitializePassed.h 文件中添加初始化声明：
void initializeFuncBlockCountPass (Registry);
```

```
在 lib/Transforms/Scalar/CMakeLists.text 文件中添加 FuncBlock-Count.cpp 文件名：
FuncBlockCount.cpp
```
