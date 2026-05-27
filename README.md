## 编译并运行 example

`example/` 现在是一个接近真实生成平台结构的 demo，而不是单文件 smoke test。

它会构建三个可执行文件：

- `platform`：main platform，负责启动 PD 进程并进入 `ldp_start_father_server()`
- `PD_sender_PD`：发送端 protection domain，启动自己的 `ldp_start_comp_server()`
- `PD_RECEIVER_PD`：接收端 protection domain，启动自己的 `ldp_start_comp_server()`

运行 `platform` 后，它会用 `apr_proc_create()` 拉起两个 PD 进程。随后 sender PD 通过 LDP-DDS 发送一条业务消息给 receiver PD，receiver PD 通过 router 打印 payload，最后 sender PD 发送 kill 控制消息让 receiver 和 main platform 退出。

```bash
cmake --build build/example

CYCLONEDDS_URI=file://$PWD/example/cyclonedds-loopback.xml ./build/example/platform
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
