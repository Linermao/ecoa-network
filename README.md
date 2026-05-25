## 编译并运行 example

```bash
cmake -S example -B build/example \
  -G Ninja \
  -DAPR_INCLUDE_DIR="$APR_INCLUDE_DIR" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake --build build/example
./build/example/platform
```

## 单独编译 lib

如果只想确认库本身能否编译：

```bash
cmake -S lib -B build/lib \
  -G Ninja \
  -DAPR_INCLUDE_DIR="$APR_INCLUDE_DIR" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake --build build/lib
```

## VSCode 配置

当前推荐让 CMake Tools 直接打开 `example/`，这样既能得到 `compile_commands.json`，也能直接构建 platform 程序：

```json
{
  "cmake.sourceDirectory": "${workspaceFolder}/example",
  "cmake.buildDirectory": "${workspaceFolder}/build/example",
  "cmake.generator": "Ninja",
  "cmake.configureOnOpen": true,
  "cmake.configureArgs": [
    "-DAPR_INCLUDE_DIR=${env:APR_INCLUDE_DIR}",
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
  ],
  "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
  "C_Cpp.default.compileCommands": "${workspaceFolder}/build/example/compile_commands.json",
  "clangd.arguments": [
    "--compile-commands-dir=${workspaceFolder}/build/example"
  ]
}
```
