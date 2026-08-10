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

当前兼容层已经按 APR 模块拆分接管了以下入口：

- `lib/apex_apr/include/apr_time.h`
- `lib/apex_apr/src/apr_time.c`
- `lib/apex_apr/include/apr_general.h`
- `lib/apex_apr/src/apr_general.c`
- `lib/apex_apr/include/apr_errno.h`
- `lib/apex_apr/src/apr_errno.c`
- `lib/apex_apr/include/apr_pools.h`
- `lib/apex_apr/src/apr_pools.c`
- `lib/apex_apr/include/apr_strings.h`
- `lib/apex_apr/src/apr_strings.c`
- `lib/apex_apr/include/apr_thread_mutex.h`
- `lib/apex_apr/src/apr_thread_mutex.c`
- `lib/apex_apr/include/apr_thread_cond.h`
- `lib/apex_apr/src/apr_thread_cond.c`

`USE_APEX_API=ON` 是 APR/APEX 兼容层唯一的总开关。启用后会同时接管
time、pool、status、string、mutex、condition 以及相关 APR 类型；不再需要额外的
extended 开关。CMake 不注入任何临时 APEX SDK 头文件路径：当前开发环境仅通过
本机 `.clangd` 提供编辑器诊断所需的过渡头文件，真实目标构建应由平台工具链提供
正式的 APEX 头文件。

例如，可以在仓库根目录创建仅供本机使用的 `.clangd`：

```yaml
CompileFlags:
  Add:
    # 当前过渡环境的公开 APEX 头文件
    - -I/absolute/path/to/apex-sdk/include
    # 如果当前 SDK 还有单独的生成头文件目录，可以临时追加这一项
    - -I/absolute/path/to/apex-sdk/generated/include

---

If:
  PathMatch: ^lib/apex_apr/include/.*\.h$

CompileFlags:
  # 编辑 shim 头文件时移除其自身 include 目录，使 #include_next 能找到真实 APR 头。
  Remove:
    - -I/absolute/path/to/ecoa-network/lib/apex_apr/include
```

这里建议使用绝对路径，因为 `.clangd` 添加的相对 include 路径会相对于
`compile_commands.json` 中记录的编译工作目录解释。仓库已忽略 `.clangd`，因此
每位开发者可以按本机 SDK 位置配置；切换到真实平台 SDK 时只需替换这里的路径。
该配置只影响 clangd，不会传递给 CMake 或真实编译器。

`lib/apex_port/` 仍是独立的 LDP 网络后端，由 `LDP_LOCAL_TRANSPORT=APEX`
选择。传输后端选择和 APR/APEX API 兼容层属于两个不同维度。

后续替换新函数时，优先按 APR 模块拆分成一组同名 shim 头和对应实现文件，例如 `apr_thread_mutex.h` / `apr_thread_mutex.c`。

更详细的维护规则见 `AGENTS.md`；`lib/apex_apr/` 下当前迁移边界和后续状态码替换方案见 `lib/apex_apr/README.md`。

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
