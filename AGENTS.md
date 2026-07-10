# 仓库开发指引

这个仓库当前有两条并行的开发主线：

- `lib/apex_apr/` 下的 APR 到 APEX 兼容层工作
- `lib/dds/` 下的 LDP 传输工作，包括 TCP/UDP 以及更新的 DDS 后端

除非某次修改明确需要同时触及两者，否则请保持这两条主线彼此独立。APR/APEX 兼容层的修改不应随意重构 DDS 行为；同样，DDS 传输相关修改也不应反向重塑 APR shim。

## APR/APEX 兼容模型

APR 兼容层当前是一个逐步演进的 shim，还不是完整的 APR 替代实现。

当前模型如下：

1. 在 `lib/apex_apr/include/` 下新增一个头文件，文件名与要拦截的 APR 头文件 basename 保持一致。
2. 在该头文件中使用 `#include_next <apr_xxx.h>`，这样原始 APR 的声明仍然可见。
3. 在 `#ifdef USE_APEX_API` 下声明名为 `apex_apr_<原始名称>` 的 wrapper 函数。
4. 使用宏把原始 APR 函数名重映射过去，例如：

```c
#define apr_sleep apex_apr_sleep
```

5. 在 `lib/apex_apr/src/` 下对应的源文件中实现该 wrapper。
6. 在实现文件里，在定义 wrapper 之前先 `#undef` 被重映射的 APR 函数名，然后再选择委托给系统 APR，或者调用未来的 APEX 实现。

当前的演示拦截的是 `apr_sleep`，它属于 `apr_time.h`：

- `lib/apex_apr/include/apr_time.h`
- `lib/apex_apr/src/apr_time.c`

在当前阶段，这个 wrapper 会打印一行 trace，然后委托给系统 APR。这是有意为之：在真正替换行为之前，先验证 include 顺序、宏重映射、源文件编译以及运行时拦截路径都已经成立。

## 添加更多 APR 替换项

请遵循 APR 原本的模块边界。如果某个函数定义在 `apr_thread_mutex.h` 中，就新增或修改：

- `lib/apex_apr/include/apr_thread_mutex.h`
- `lib/apex_apr/src/apr_thread_mutex.c`

如果某个函数定义在 `apr_pools.h` 中，就新增或修改：

- `lib/apex_apr/include/apr_pools.h`
- `lib/apex_apr/src/apr_pools.c`

当新增源文件时，也要把它加入 [lib/CMakeLists.txt](/home/c/workspace/ecoa-network/lib/CMakeLists.txt) 中 `USE_APEX_API` 的 `target_sources` 配置块。

建议一个 APR 模块对应一对 shim 头文件/源文件。避免把不相关的替换都堆进一个通用的 `apr.c`，否则后续 APEX 迁移会更难审计。

## 函数替换检查清单

对于每一个新的 APR 函数 wrapper：

- 确认它原本是在哪个 APR 头文件里声明的。
- 在 `lib/apex_apr/include/` 下创建或更新对应的 shim 头文件。
- 保持 `#include_next <apr_xxx.h>` 位于 shim 头文件顶部。
- 声明与原签名一致的 `apex_apr_<function>`。
- 在 `USE_APEX_API` 下添加 `#define apr_<function> apex_apr_<function>`。
- 在对应的源文件中实现该 wrapper。
- 在源文件中先包含 shim 后的 APR 头文件，再 `#undef apr_<function>`。
- 在过渡期，采用“少量 trace + 委托系统 APR”的方式。
- 后续在行为完全搞清楚后，再把委托替换成真正的 APEX 调用。

## 类型与结构替换

处理 APR 类型时要谨慎。

不透明的 APR 指针类型最容易替换，因为项目代码通常只是传递这些指针，而不会直接读取它们的字段。对于这类类型，应当定义一个兼容的 shim 类型，并把相关的 create/destroy/use 函数一起实现。

除非任务明确要求，否则不要修改公开的 LDP 结构体字段。APR 类型出现在一些公开头文件中，比如 [ldp_structures.h](/home/c/workspace/ecoa-network/lib/ldp_structures.h) 和 [ldp_network.h](/home/c/workspace/ecoa-network/lib/ldp_network.h)；修改这些地方可能影响生成的平台代码。

除非兼容策略已经非常明确，否则不要把 APR 类型像函数重映射那样简单地用宏改名。更推荐按模块进行有计划的替换。

## 构建配置

项目级默认配置位于 `cmake_config.cmake`。

当前默认值：

- `USE_APEX_API=ON`
- `LDP_LOCAL_TRANSPORT=TCP`

[lib/CMakeLists.txt](/home/c/workspace/ecoa-network/lib/CMakeLists.txt) 会读取这些设置。当启用 `USE_APEX_API` 时，它会把 `lib/apex_apr/include` 放到正常 APR include 路径之前，并编译 shim 源文件。

开发者仍然可以在 CMake 配置时覆盖这些值，例如：

```bash
cmake -S example -B build/example \
  -DAPR_INCLUDE_DIR="$APR_INCLUDE_DIR" \
  -DUSE_APEX_API=OFF \
  -DLDP_LOCAL_TRANSPORT=DDS
```

## DDS 传输相关工作

DDS 支持是一个独立的传输后端，其主要代码位于 `lib/dds/`，并通过 `LDP_LOCAL_TRANSPORT=DDS` 进行选择。

在处理 APR/APEX shim 时，不要改动 DDS 报文格式、CycloneDDS 配置或 DDS 路由，除非用户明确要求两边一起改。反过来，处理 DDS 工作时，也应尽量保持 APR shim 的 include 和 CMake 结构不变，除非那项工作明确需要调整构建配置。
