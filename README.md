## Requirements

需要 CMake、Ninja、APR 和 CycloneDDS。进入开发环境后，确认 `APR_INCLUDE_DIR` 指向 APR 头文件目录。

## Example

`example/` 是一个接近真实生成平台结构的 DDS demo。它会构建：

- `platform`
- `PD_sender_PD`
- `PD_receiver_PD`

首次配置：

```bash
cmake -S example -B build/example \
  -G Ninja \
  -DAPR_INCLUDE_DIR="$APR_INCLUDE_DIR" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

编译：

```bash
cmake --build build/example
```

运行：

```bash
CYCLONEDDS_URI=file://$PWD/example/cyclonedds-loopback.xml \
  ./build/example/platform
```

打开 DDS trace：

```bash
CYCLONEDDS_URI=file://$PWD/example/cyclonedds-loopback.xml \
LDP_DDS_TRACE=1 \
  ./build/example/platform
```

## Marx Brothers

`marx_brothers/6-Output` 是真实 generated platform，可以用来验证 LDP-DDS 与生成代码的贯通性。

配置：

```bash
cmake -S marx_brothers/6-Output -B build/marx-brothers-dds \
  -G Ninja \
  -DAPR_INCLUDE_DIR="$APR_INCLUDE_DIR" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

编译：

```bash
cmake --build build/marx-brothers-dds
```

运行：

```bash
ROOT=$PWD
cd build/marx-brothers-dds/bin

CYCLONEDDS_URI=file://$ROOT/example/cyclonedds-loopback.xml \
LDP_DDS_TRACE=1 \
  ./platform
```

## Lib

只编译 LDP library：

```bash
cmake -S lib -B build/lib \
  -G Ninja \
  -DAPR_INCLUDE_DIR="$APR_INCLUDE_DIR" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake --build build/lib
```

## VSCode 配置

推荐让 CMake Tools 直接打开 `example/`，这样可以生成 `compile_commands.json` 并构建 platform 程序：

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
