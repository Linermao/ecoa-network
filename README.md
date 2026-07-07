## Requirements

需要 CMake、Ninja、APR 和 CycloneDDS。进入开发环境后，确认 `APR_INCLUDE_DIR` 指向 APR 头文件目录。

## Build Defaults

项目级默认配置在 `cmake_config.cmake`：

- `USE_APEX_API=ON`：默认启用 `lib/apex_apr/` 下的 APR 兼容层。
- `LDP_LOCAL_TRANSPORT=TCP`：默认使用 TCP 后端，方便本地开发和 VSCode/clangd 代码提示。

可以在配置时覆盖：

```bash
cmake -S example -B build/example \
  -G Ninja \
  -DAPR_INCLUDE_DIR="$APR_INCLUDE_DIR" \
  -DUSE_APEX_API=OFF \
  -DLDP_LOCAL_TRANSPORT=DDS \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

APR/APEX 兼容层和 DDS transport 是两条并行工作线。APR 兼容层位于 `lib/apex_apr/`，DDS 后端位于 `lib/dds/`；两者原则上不互相阻塞。

## APR/APEX Shim

当前兼容层采用逐步接管模型：在 `lib/apex_apr/include/` 下创建与 APR 同名的头文件，用 `#include_next` 透传真实 APR 头，再用 `USE_APEX_API` 下的宏把单个 APR 函数重定向到 `apex_apr_*` wrapper。

当前示例接管的是 `apr_time.h` 里的 `apr_sleep`：

- `lib/apex_apr/include/apr_time.h`
- `lib/apex_apr/src/apr_time.c`

现阶段 wrapper 只打印一行 trace，然后委托真实 APR。后续替换新函数时，优先按 APR 模块拆分成一组同名 shim 头和对应实现文件，例如 `apr_thread_mutex.h` / `apr_thread_mutex.c`。

更详细的维护规则见 `AGENTS.md`。

## Example

`example/` 是一个接近真实生成平台结构的 demo。默认配置使用 TCP 后端；如需 DDS，可在 CMake 配置时传入 `-DLDP_LOCAL_TRANSPORT=DDS`。它会构建：

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
./build/example/platform
```

使用 DDS 后端时运行：

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
