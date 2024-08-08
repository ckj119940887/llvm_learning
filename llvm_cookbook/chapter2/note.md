# vscode 中找不到头文件的问题
```
在包含如下都文件的时候，会出现找不到的情况
#include "llvm/IR/LLVMContext.h"

检测llvm头文件目录的方法：llvm-config-9 --includedir

解决方法是更新vscode的includePath，具体做法详见 
myNote/tools_installation/vscode/vscode_C++_Plugin.md
```