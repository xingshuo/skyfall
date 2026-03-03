# skyfall

skyfall 是一个基于 C++17 实现的轻量级多线程服务调度框架，设计和实现主要参考自 [skynet](https://github.com/cloudwu/skynet)。以 **M 个 Worker 线程 + N 个 Service（Context）** 的两级调度为核心，通过动态库插件机制实现业务逻辑的热插拔。

---

## 目录

- [架构概览](#架构概览)
- [核心组件](#核心组件)
- [消息系统](#消息系统)
- [快速开始](#快速开始)
- [配置文件](#配置文件)
- [开发指南：编写一个 Service](#开发指南编写一个-service)
- [项目结构](#项目结构)
- [依赖与编译环境](#依赖与编译环境)

---

## 架构概览

```
┌─────────────────────────────────────────────────────┐
│                    skyfall 进程                      │
│                                                     │
│  ┌──────────┐  ┌──────────┐  ┌───────────────────┐  │
│  │ Worker 0 │  │ Worker 1 │  │   Timer Thread    │  │
│  │  Thread  │  │  Thread  │  │  (1ms tick 驱动)  │  │
│  └────┬─────┘  └────┬─────┘  └────────┬──────────┘  │
│       │             │                 │             │
│       └─────────────┴─────────────────┘             │
│                     │  Pop                          │
│             ┌───────▼────────┐                      │
│             │   GlobalMQ     │  ← 全局消息队列链表   │
│             └───────┬────────┘                      │
│                     │                               │
│         ┌───────────┼───────────┐                   │
│         ▼           ▼           ▼                   │
│   ┌──────────┐ ┌──────────┐ ┌──────────┐            │
│   │ MsgQueue │ │ MsgQueue │ │ MsgQueue │  ...       │
│   │(Context A)│ │(Context B)│ │(Context C)│           │
│   └──────────┘ └──────────┘ └──────────┘            │
│         │           │           │                   │
│   ┌──────────┐ ┌──────────┐ ┌──────────┐            │
│   │ Context  │ │ Context  │ │ Context  │  ...       │
│   │  (Svc A) │ │  (Svc B) │ │  (Svc C) │           │
│   └──────────┘ └──────────┘ └──────────┘            │
│                                                     │
│  ┌──────────────────────────────────────────────┐   │
│  │            Monitor Thread                    │   │
│  │          (5s 周期死锁检测)                    │   │
│  └──────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
```

**两级调度模型：**

- **Level 1（线程级）**：`Scheduler` 管理固定数量的 Worker 线程，所有线程共享 `GlobalMQ`。
- **Level 2（服务级）**：每个 `Context`（Service）拥有独立的 `MsgQueue`，消息到达时 `MsgQueue` 被推入 `GlobalMQ`，由空闲 Worker 取出并驱动对应 Context 的回调执行。每个 Context 的消息保证串行处理，业务代码无需加锁。

---

## 核心组件

### Context（服务）

`Context` 是调度的最小单元，对应一个业务 Service 实例：

- 通过 `ContextNew(name, param)` 创建，`name` 对应动态库模块名；
- 持有唯一 `uint32_t handle`，可通过 `SetHandleName` 注册可读名字；
- 拥有独立 `MsgQueue`，通过 `Send` / `SendName` 向其他 Context 投递消息；
- 注册 `ContextCallback` 回调，消息到达时由 Worker 线程驱动执行。

```cpp
// 回调函数签名
using ContextCallback = std::function<
    int(ContextSPtr ctx, void *ud, int type, int session,
        uint32_t source, const void *msg, size_t sz)>;
```

### Scheduler（调度器）

启动 N 个 Worker 线程持续消费 `GlobalMQ`，同时维护：
- **Timer 线程**：每 1ms tick 一次，驱动定时器超时检测；
- **Monitor 线程**：每 5s 检查所有 Worker 是否在同一消息上阻塞（死锁检测）；
- 所有线程均在 `ContextTotal() == 0`（所有 Context 退出）后自动停止。

### TimerManager（定时器）

毫秒精度定时器，基于 `std::multimap` 管理超时事件。调用 `Timeout(handle, interval_ms, session)` 注册，超时后以 `PTYPE_RESPONSE` 类型消息投递到目标 Context。`interval_ms <= 0` 时立即投递（下一次 dispatch 触发）。

### Logger（日志）

异步日志，后台单线程写入，双缓冲队列（`write_q_` / `read_q_` swap）减少锁竞争。退出时保证最后一批日志不丢失。

| 宏 | 级别 |
|---|---|
| `SKYFALL_DEBUG(ctx, fmt, ...)` | DEBUG |
| `SKYFALL_INFO(ctx, fmt, ...)` | INFO |
| `SKYFALL_WARN(ctx, fmt, ...)` | WARN |
| `SKYFALL_ERROR(ctx, fmt, ...)` | ERROR |
| `SKYFALL_FATAL(ctx, fmt, ...)` | FATAL（写日志后强制退出） |

日志格式：
```
DD/MM/YY HH:MM:SS.mmm [:%08x] [LEVEL] message (file:line)
```

### ModuleManager（模块管理）

通过 `dlopen` / `dlsym` 按需加载 `.so` 模块，同一模块只加载一次（内部缓存）。`module_path` 支持 `;` 分隔的多路径，`?` 占位符会被替换为模块名。

### HandleStorage（句柄表）

维护 `handle → Context` 和 `name → handle` 两张映射，使用 `std::shared_mutex` 实现并发安全的读写分离。

---

## 消息系统

消息类型编码在 `Message.sz` 的高 8 位中：

| 常量 | 值 | 含义 |
|------|----|------|
| `PTYPE_RESPONSE` | 0 | 响应消息 / 定时器超时 |
| `PTYPE_TEXT` | 1 | 文本请求消息 |
| `PTYPE_SYSTEM` | 2 | 系统消息 |
| `PTYPE_ERROR` | 3 | 错误通知 |
| `PTYPE_TAG_DONTCOPY` | 0x10000 | 标志位：不复制消息数据（由接收方负责 free） |

默认情况下发送消息会 `malloc` + `memcpy` 复制数据，接收方 `dispatch` 后自动 `free`。设置 `PTYPE_TAG_DONTCOPY` 可避免拷贝，此时发送方不再持有数据所有权。

---

## 快速开始

### 1. 编译

```bash
make
```

产物：
- `skyfall`：主进程可执行文件
- `examples/chat/testchat.so`：聊天示例动态库

清理：
```bash
make clean
```

### 2. 运行 Chat 示例

```bash
./skyfall examples/chat/config.ini
```

示例输出（Bob 发起聊天，Alice 回声，依次发送 5 条消息后双方自动退出）：

```
DD/MM/YY HH:MM:SS.mmm [:%08x] [INFO ] bootstrap service start: 1 (...)
DD/MM/YY HH:MM:SS.mmm [:%08x] [INFO ] receiver service Alice start: 2 (...)
DD/MM/YY HH:MM:SS.mmm [:%08x] [INFO ] sender service Bob start: 3 (...)
DD/MM/YY HH:MM:SS.mmm [:%08x] [INFO ] Bob new chat text 'Hello world' session: 2 (...)
DD/MM/YY HH:MM:SS.mmm [:%08x] [INFO ] Alice recv ping msg 'Hello world' session:2 from peer 3 (...)
DD/MM/YY HH:MM:SS.mmm [:%08x] [INFO ] Bob recv pong msg 'Hello world' session:2 from peer 2 (...)
...
DD/MM/YY HH:MM:SS.mmm [:%08x] [INFO ] Bob exit peer: Alice (...)
DD/MM/YY HH:MM:SS.mmm [:%08x] [INFO ] Bob exit self (...)
```

### 3. 信号处理

| 信号 | 行为 |
|------|------|
| `SIGTERM` / `SIGINT` | 优雅退出（退出所有 Context） |
| `SIGHUP` / `SIGQUIT` / `SIGPIPE` | 忽略 |

---

## 配置文件

INI 格式，示例见 `examples/chat/config.ini`：

```ini
[app]
; Worker 线程数量
thread = 4
; 动态库搜索路径，? 会被替换为模块名，多路径用 ; 分隔
module_path = examples/chat/?.so
; 启动 Service：格式为 "<模块名> <参数>"，参数部分传递给 create 函数
bootstrap = testchat bootstrap

[log]
; 日志文件路径，留空则输出到标准输出
logfile =
; 日志等级：DEBUG / INFO / WARN / ERROR
loglevel = DEBUG
```

---

## 开发指南：编写一个 Service

以 `examples/chat/` 为参考，一个 Service 动态库需要导出三个 C 接口（`_signal` 可选）：

### 模块接口规范

| C 函数签名 | 必须 | 说明 |
|------------|:----:|------|
| `void* <name>_create(std::string_view param)` | ✅ | 创建服务实例，返回 `void*`；若不需要 create 逻辑可不导出，框架会使用默认占位实例 |
| `int <name>_init(void*, ContextSPtr, std::string_view)` | ✅ | 初始化服务，注册 callback，返回 0 成功 |
| `void <name>_release(void*)` | ✅ | 销毁服务实例 |
| `void <name>_signal(void*, int)` | — | 处理信号（可选） |

### 推荐模式：基类 + 多态

```cpp
// my_service.h
#include "kernel/Server.h"
#include "kernel/Skyfall.h"
#include "kernel/Log.h"
#include <functional>
#include <unordered_map>

class MyService {
public:
    explicit MyService(std::string_view name) : m_name(name) {}
    virtual ~MyService() = default;
    virtual int Init(skyfall::ContextSPtr ctx) = 0;

protected:
    // 封装定时器，将 session 与回调函数绑定
    int Timeout(skyfall::ContextSPtr ctx, int64_t ms, std::function<void()> cb);
    virtual void OnRequest(skyfall::ContextSPtr ctx, int session,
                           uint32_t source, const void *msg, size_t sz);
    virtual void OnResponse(skyfall::ContextSPtr ctx, int session,
                            uint32_t source, const void *msg, size_t sz);

    int Callback(skyfall::ContextSPtr ctx, void *, int type, int session,
                 uint32_t source, const void *msg, size_t sz);

protected:
    int m_seq{0};
    std::string m_name;

private:
    std::unordered_map<int, std::function<void()>> m_timeouts;
};
```

```cpp
// my_service.cc
int MyService::Callback(...) {
    switch (type) {
    case skyfall::PTYPE_TEXT:     OnRequest(...); break;
    case skyfall::PTYPE_RESPONSE: OnResponse(...); break;
    default: SKYFALL_WARN(ctx, "unknown type %d", type); break;
    }
    return 0;
}

int MyService::Timeout(skyfall::ContextSPtr ctx, int64_t ms,
                        std::function<void()> cb) {
    m_timeouts[++m_seq] = std::move(cb);
    return skyfall::Timeout(ctx->GetHandle(), ms, m_seq);
}

void MyService::OnResponse(skyfall::ContextSPtr, int session, ...) {
    if (auto it = m_timeouts.find(session); it != m_timeouts.end()) {
        auto cb = std::move(it->second);
        m_timeouts.erase(it);
        cb();
    }
}
```

```cpp
// 导出 C 接口
extern "C" MyService* myservice_create(std::string_view param) {
    return new MyService(param);
}

extern "C" int myservice_init(MyService *svc, skyfall::ContextSPtr ctx,
                               std::string_view param) {
    return svc->Init(ctx);
}

extern "C" void myservice_release(MyService *svc) {
    delete svc;
}
```

```cpp
// Init 实现：注册 callback，使用 weak_ptr 避免循环引用
int MyService::Init(skyfall::ContextSPtr ctx) {
    ctx->SetCallback([this](auto&&... args) {
        return Callback(std::forward<decltype(args)>(args)...);
    });
    // 使用 weak_ptr 防止 lambda 延长 Context 生命周期
    skyfall::ContextWPtr weak_ctx = ctx;
    Timeout(ctx, 1000, [this, weak_ctx] {
        auto shared_ctx = weak_ctx.lock();
        if (!shared_ctx) return;
        // 定时器触发后的业务逻辑 ...
    });
    return 0;
}
```

### 编译为动态库

```bash
g++ -g -Wall -Wextra -std=c++17 -fPIC --shared \
    -I path/to/skyfall/src \
    my_service.cc -o myservice.so
```

### 配置并启动

```ini
[app]
module_path = path/to/?.so
bootstrap = myservice <init_param>
```

---

## Chat 示例架构

`examples/chat/` 演示了一个完整的 Service 继承体系：

```
ChatService（抽象基类）
├── Guider   ── bootstrap：创建 Sender 和 Receiver，随即退出
├── Sender   ── Bob：每隔 1s 发起一条聊天，收到 Pong 后继续或退出
└── Receiver ── Alice：收到 Text 消息后原样回送 Response
```

**param 格式**（由 `testchat_create` 解析）：

| param | 创建的实例 |
|-------|-----------|
| `"bootstrap"` 或 `""` | `Guider` |
| `"Sender Bob Alice"` | `Sender`，名字 Bob，对端 Alice |
| `"Receiver Alice Bob"` | `Receiver`，名字 Alice，对端 Bob |

**消息流程：**

```
Guider ──[init]──> 创建 Receiver(Alice) + Sender(Bob) ──> 退出

Sender(Bob) ──[1s timer]──> SendName("Alice", PTYPE_TEXT, msg)
                                       │
Receiver(Alice) ──[OnRequest]──> SendName("Bob", PTYPE_RESPONSE, msg)
                                       │
Sender(Bob) ──[OnResponse]──> 收到 Pong，继续下一条或双方退出
```

---

## 项目结构

```
skyfall/
├── Makefile
├── src/
│   ├── kernel/
│   │   ├── Main.cc          # 程序入口：配置解析、组件初始化、启动调度
│   │   ├── Skyfall.h        # 对外公开的核心 API
│   │   ├── Config.h         # Config 结构体、ThreadType 枚举
│   │   ├── Server.h/cc      # Context（服务单元）、Node（全局节点）
│   │   ├── Handle.h/cc      # HandleStorage：handle ↔ Context 映射
│   │   ├── Module.h/cc      # Module（动态库封装）、ModuleManager
│   │   ├── MsgQueue.h/cc    # MsgQueue（per-Context）、GlobalMQ（全局链表）
│   │   ├── Scheduler.h/cc   # Worker 线程调度、Timer 线程、Monitor 线程
│   │   ├── Timer.h/cc       # TimerManager：毫秒定时器
│   │   ├── Monitor.h/cc     # 死锁检测（版本号机制）
│   │   └── Log.h/cc         # 异步日志（双缓冲 + 后台线程）
│   └── util/
│       ├── INIReader.h/cc   # INI 配置文件解析
│       ├── StringUtil.h/cc  # Split / Strip / Format
│       └── TimeUtil.h/cc    # 毫秒级时间戳（steady_clock + system_clock 混合）
└── examples/
    └── chat/
        ├── config.ini              # 示例配置
        ├── service_testchat.h      # ChatService / Guider / Sender / Receiver 声明
        ├── service_testchat.cc     # 实现 + C 导出接口
        └── testchat.so             # 编译产物
```

---

## 依赖与编译环境

| 项目 | 要求 |
|------|------|
| 编译器 | g++ 支持 C++17（推荐 GCC 7+） |
| 标准库 | C++17（`std::string_view`、`std::shared_mutex`、`std::atomic` 等） |
| 系统库 | `libpthread`、`libdl` |
| 操作系统 | Linux |

编译选项：`-g -Wall -Wextra -Werror -std=c++17`
