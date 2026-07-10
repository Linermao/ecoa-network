#任务清单1
1、阅读现在这个项目中有关apex_apr兼容层的实现
2、在这个项目基础上进行兼容层的开发，保证与这个项目已有的架构保持一致
3、在apex_apr中复写以下函数或常量或结构体，不需要具体功能的实现，先确定函数的传入参数，返回值，具体功能实现可以给出实现的方案：
apr_initialize
apr_terminate
apr_status_t
APR_SUCCESS
APR_EGENERAL
apr_strerror

apr_sleep
apr_time_now
apr_time_from_sec
apr_time_usec

apr_pool_create
apr_pool_destroy
apr_palloc
apr_pcalloc
apr_psprintf
apr_cpystrn

#任务清单2
1、帮我完成我的ecoa项目中出现的所有APR类型，必须也符合之前开发的apex_apr架构
要求如下：
apr类型：
| APR 类型 | 在项目中的作用 | 653 侧建议 |
| --- | --- | --- |
| `apr_status_t` | 全局状态码类型 | 定义为项目自有整数状态码类型 |
| `apr_pool_t` | 内存池句柄 | 自定义 pool 结构 |
| `apr_size_t` | 缓冲区长度 | 直接兼容到 `size_t` |
| `apr_int32_t` | 轮询返回数量等 | 兼容到 32 位整数 |
| `apr_int64_t` | 时间等待辅助 | 兼容到 64 位整数 |
| `apr_time_t` | APR 时间类型 | 建议统一为微秒语义的 64 位时间 |
| `apr_thread_t` | 模块线程/触发线程句柄 | 包装 653 process 句柄 |
| `apr_threadattr_t` | 线程属性 | 包装 653 process 创建参数 |
| `apr_thread_mutex_t` | 互斥锁句柄 | 包装 semaphore/mutex 风格对象 |
| `apr_thread_cond_t` | 条件变量句柄 | 包装 event/semaphore 组合对象 |
| `apr_proc_t` | 进程句柄 | 如保留，则包装“预定义 process 描述符” |
| `apr_procattr_t` | 进程属性 | 如保留，则包装静态配置参数 |
| `apr_socket_t` | socket 句柄 | 若继续兼容，则包装端口/通道对象 |
| `apr_sockaddr_t` | 地址对象 | 若改端口模型，建议逐步移除 |
| `apr_pollfd_t` | poll 描述项 | 若改端口模型，建议逐步移除 |
| `apr_pollset_t` | poll 集合 | 若改端口模型，建议逐步移除 |
| `apr_exit_why_e` | 进程退出原因 | 若保留 `proc` 接口，则需自定义兼容枚举 |
2、该项目中现在存在apr的源码，可适量参考apr源码的类型定义，但定义一定要符合apex规范，可省略类型定义中的细节（给出未来计划），先确定总体架构