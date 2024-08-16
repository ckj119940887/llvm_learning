# 生成.bc文件，作为分析pass的输入
```
clang-9 -c -emit-llvm testcode.c -o testcode.bc
opt-9 -load build/libopcodeCounterlib.so -opcodeCounter -disable-output testcode.bc
```