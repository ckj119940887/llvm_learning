# command

## 优化
```
opt-9 -S -instcombine testfile.ll -o output1.ll

无用参数消除（ dead-argument-elimination）优化：
opt-9 -S -deadargelim testfile.ll -o output1.ll
```

### 重要参数
```
adce：入侵式无用代码消除。
bb-vectorize： 基本块向量化。
constprop： 简单常量传播。
dce： 无用代码消除。
deadargelim： 无用参数消除。
globaldce： 无用全局变量消除。
globalopt： 全局变量优化。
gvn： 全局变量编号。
inline： 函数内联。
instcombine：冗余指令合并。
licm： 循环常量代码外提。
loop-unswitch： 循环外提。
loweratomic： 原子内建函数 lowering。
lowerinvoke： invode 指令 lowering，以支持不稳定的代码生成器。
lowerswitch： switch 指令 lowering。
mem2reg： 内存访问优化。
memcpyopt： MemCpy 优化。
simplifycfg： 简化 CFG。
sink： 代码提升。
tailcallelim： 尾调用消除。
```

## 生成IR代码
```
clang-9 -emit-llvm -S multiply.c -o multiply.ll
```

## 生成bitcode
```
llvm-as-9 test_bitcode.ll -o test_bitcode.bc
```

## 将bitcode转换为目标平台汇编码
```
llc-9 test_bitcode.bc -o test_bitcode.s
llc 命令把 LLVM 输入编译为特定架构的汇编语言，如果我们在之前的命令中没有为其指定任何架构，那么默认生成本机的汇编码， 即调用 llc 命令的主机。
加入-march=architechture 参数， 可以生成特定目标架构的汇编码。 使用-mcpu=cpu 参数则可以指定其 CPU， 而-reg alloc =basic/greedy/fast/pbqp 则可以指定寄存器分配类型。

通过clang从bitcode生成汇编码(clang默认不消除桟指针，llc默认消除)
clang-9 -S test_bitcode.bc -o test_bitcode.s -fomit-frame-pointer
```

## 链接bitcode
```
clang-9 -emit-llvm -S test_link_1.c -o test_link_1.ll
clang-9 -emit-llvm -S test_link_2.c -o test_link_2.ll
llvm-as-9 test_link_1.ll -o test_link_1.bc
llvm-as-9 test_link_2.ll -o test_link_2.bc
llvm-link-9 test_link_1.bc test_link_2.bc -o output.bc
```

## 执行bitcode
```
lli-9 output.bc
```

## 将bitcode转回为IR
```
llvm-dis-9 test_bitcode.bc -o test_bitcode_reverse.ll
```

## 打印抽象语法树
```
clang-9 -cc1 test.c -asm-dump
-cc1 参数保证了只运行编译器前端，而不是编译器驱动
```