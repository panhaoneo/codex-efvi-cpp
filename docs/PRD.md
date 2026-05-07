# efvi-cpp 项目需求文档（PRD）v1.2

**AMD Solarflare ef_vi C++ 封装库**
兼容网卡：X2522（EF10）/ X3522（EfCT）
目标平台：Linux · C++11 · OpenOnload 8.1.26
文档日期：2025-04

---

## 变更记录

| 版本 | 日期 | 变更内容 |
|------|------|---------|
| v1.0 | 2025-04 | 初始版本，5 个待确认问题 |
| v1.1 | 2025-04 | 确认 Q-01~Q-05；新增多队列需求；确定日志回调接口；文档格式转 Markdown；明确文档输出要求 |
| v1.2 | 2025-04 | 语言标准从 C++17 降至 C++11；CMake 最低版本降至 3.10；接口适配 C++11（string_view→const string&，span→裸指针+count）；设计决策更新 |

---

## 目录

1. [项目背景与目标](#1-项目背景与目标)
2. [约束条件与技术环境](#2-约束条件与技术环境)
3. [功能需求](#3-功能需求)
4. [非功能需求](#4-非功能需求)
5. [模块划分与接口设计](#5-模块划分与接口设计)
6. [测试需求](#6-测试需求)
7. [开发里程碑](#7-开发里程碑)
8. [关键设计决策](#8-关键设计决策)
9. [已确认问题清单](#9-已确认问题清单)
10. [文档输出要求](#10-文档输出要求)

---

## 1. 项目背景与目标

efvi-cpp 是对 AMD Solarflare ef_vi 内核旁路网络 API 的 C++11 封装库，面向超低延迟量化交易基础设施场景（A 股市场数据接收、行情订阅、订单通路）。

ef_vi 是一个 L2 原生 API，提供直接访问网卡数据路径的能力，完全绕过内核 TCP/IP 栈，实现亚微秒级收发包延迟。原始 C API 使用复杂、资源管理繁琐，本项目以 C++11 对其进行简洁封装，同时兼容 X2522（EF10 架构）和 X3522（EfCT 架构）两款网卡。

### 1.1 核心价值主张

- **极简初始化**：少量代码完成 VI 创建、内存注册、过滤器配置全流程
- **架构透明**：X2522/X3522 差异由库内部自动适配，用户代码无感知
- **零开销抽象**：热路径（收发包）不使用虚函数，不动态分配内存
- **可观测性**：内置原子统计计数器，支持快照导出；日志通过用户回调输出
- **多队列支持**：支持 VISet 多 VI 多队列分发（RSS），可按核绑定
- **测试友好**：Mock 模式可在无网卡环境下完整运行单元测试（CI 友好）

---

## 2. 约束条件与技术环境

| 维度 | 要求 |
|------|------|
| 操作系统 | Linux（OpenEuler 22.03 SP3，Kernel ≥ 4.15） |
| 编译器 | GCC 7+，标准 C++11（GCC 10+ 推荐） |
| 构建系统 | CMake 3.10+ |
| Onload 版本 | OpenOnload 8.1.26，`ONLOAD_SRC_DIR` 指向源码树 |
| 目标网卡 | X2522（EF10 架构）、X3522（EfCT 架构） |
| VI 设计 | 单 VI 基础设计；多队列通过 VISet 扩展 |
| 内存模型 | 热路径无动态分配；X3522 必须使用 hugepage |
| 依赖限制 | 仅依赖 OpenOnload 头文件 + 标准库，禁止引入第三方框架 |
| Mock 模式 | 无网卡环境可编译运行，用于 CI / 单元测试 |
| 测试框架 | Google Test（gtest） |
| TX 接口 | 仅支持连续 buffer，不支持 scatter-gather |
| 日志接口 | 用户提供回调函数，库不直接输出到 stderr |

---

## 3. 功能需求

### 3.1 初始化与资源管理（FR-INIT）

初始化封装以下 C API 调用序列：

```
ef_driver_open()  →  ef_pd_alloc()  →  ef_vi_alloc_from_pd()  →  ef_memreg_alloc()
```

- **FR-INIT-01**：提供 `ViConfig` 配置结构体，含网卡名、RXQ/TXQ 大小、PD 标志、过滤器列表、日志回调、性能参数
- **FR-INIT-02**：`Vi` 类构造时完成所有初始化，析构时自动释放（RAII，强异常安全保证）
- **FR-INIT-03**：支持查询 NIC 架构类型（`EF10` / `EfCT`）并暴露能力标志（`EF_VI_CAP_*`）
- **FR-INIT-04**：初始化失败抛出 `ViException`（含错误码 + 描述字符串），不使用返回码
- **FR-INIT-05**：提供 `NicInfo` 结构体，记录网卡型号、驱动版本、MAC 地址、MTU、端口速率
- **FR-INIT-06**：支持多 `Vi` 实例并发创建（每实例独立状态，无全局锁）

### 3.2 发包（TX，FR-TX）

X2522 与 X3522 发包路径差异：

| 特性 | X2522 (EF10) | X3522 (EfCT) |
|------|-------------|-------------|
| 发包方式 | DMA / PIO / CTPIO | 仅 CTPIO（自动 fallback store-and-forward） |
| TX Checksum Offload | 支持（IP/TCP/UDP） | 不支持，必须软件计算 |
| TX Alternatives | 支持（硬件特性，本库不封装，见 §9） | 不支持 |
| 多包排队发送 | 支持 | 不支持（CTPIO 限制） |

- **FR-TX-01**：提供 `send(const void* buf, size_t len)` 统一接口，内部根据架构选择最优路径
- **FR-TX-02**：X3522 模式下自动计算并填充 IP/TCP/UDP checksum（软件 offload，`checksum.cpp`）
- **FR-TX-03**：支持 CTPIO 发包（`ef_vi_transmit_ctpio`），提供 `ctpio_threshold` 参数配置
- **FR-TX-04**：X2522 支持 PIO 发包路径（`ef_pio_alloc` + `ef_vi_transmit_pio`）
- **FR-TX-05**：TX 完成事件自动处理（`EF_EVENT_TYPE_TX`），自动回收 descriptor
- **FR-TX-06**：提供 `send_batch()` 批量接口（仅 X2522），减少 doorbell 次数

### 3.3 收包（RX，FR-RX）

X2522 与 X3522 收包路径差异：

| 特性 | X2522 (EF10) | X3522 (EfCT) |
|------|-------------|-------------|
| RX buffer 管理 | 用户自管理，需预先 post descriptor | 驱动管理，superbuf 共享 |
| 接收事件类型 | `EF_EVENT_TYPE_RX` | `EF_EVENT_TYPE_RX_REF` |
| Packet 访问方式 | 直接内存指针 | `efct_vi_rxpkt_get()` / `rxpkt_release()` |
| Ring refill | 手动（批量 8/16/32/64） | 自动（superbuf 机制） |
| Filter 类型数 | TCP/UDP/MAC/VLAN 等多种 | IPv4 local + MAC+VLAN + mcast mismatch |

- **FR-RX-01**：提供统一 `poll(RxBatch& batch)` 接口，内部调用 `ef_eventq_poll()`
- **FR-RX-02**：EF10 模式下自动管理 RX ring refill（至少保持 16 个 buffer，默认批量 32）
- **FR-RX-03**：EfCT 模式下封装 `rxpkt_get/release` 生命周期，提供 `PacketRef` RAII 对象
- **FR-RX-04**：每次 poll 至少处理 `EF_VI_EVENT_POLL_MIN_EVS` 个事件
- **FR-RX-05**：支持 RX hardware timestamp 提取（`ef_vi_receive_get_timestamp_sync`）
- **FR-RX-06**：暴露 RX discard 事件（`rx_no_desc_trunc` / `rx_discard`）用于丢包统计

### 3.4 多队列支持（FR-MULTIQUEUE）

- **FR-MQ-01**：提供 `ViSet` 类，封装 `ef_vi_set` 多队列分发，支持 RSS（Receive Side Scaling）
- **FR-MQ-02**：`ViSet` 支持配置队列数量（1~N，受网卡 port 最大 VI 数限制）
- **FR-MQ-03**：每个队列对应一个独立的 `Vi` 实例，可绑定到不同 CPU 核心
- **FR-MQ-04**：`ViSet` 提供聚合统计接口，汇总所有子 VI 的 `Stats::Snapshot`
- **FR-MQ-05**：X3522 EfCT 多接收队列（shared RX queue）机制与 EF10 VISet 的差异由库内部屏蔽

### 3.5 过滤器管理（FR-FILT）

- **FR-FILT-01**：支持 `FilterSpec` 配置 UDP/TCP IPv4 精确匹配（本地 IP:Port）
- **FR-FILT-02**：支持组播过滤（`multicast_all` / 精确组播 IP）
- **FR-FILT-03**：支持 MAC + VLAN 过滤
- **FR-FILT-04**：过滤器使用 `FilterCookie` 管理，支持运行时 `add` / `remove`
- **FR-FILT-05**：X3522 过滤器数量限制（256/Port）及类型限制在配置时检查并抛 `ViException`

### 3.6 性能参数调节（FR-PERF）

- **FR-PERF-01**：RXQ/TXQ 深度可配（默认 512，范围 64~4096，必须为 2 的幂）
- **FR-PERF-02**：RX refill batch size 可配（默认 32，选项 8/16/32/64）
- **FR-PERF-03**：CTPIO threshold 可配（默认 64 字节，范围 0~1500）
- **FR-PERF-04**：PD flags 可配：`EF_PD_DEFAULT` / `EF_PD_VF` / `EF_PD_PHYS_MODE`（仅 X2522）
- **FR-PERF-05**：RX buffer 内存对齐可配（默认 4MB 对齐以提升 DMA 效率）
- **FR-PERF-06**：Hugepage 开关（X3522 强制开启，X2522 可选）

### 3.7 统计与监控（FR-STAT）

- **FR-STAT-01**：维护原子计数器：`rx_packets` / `tx_packets` / `rx_drops` / `tx_drops` / `rx_errors` / `tx_errors`
- **FR-STAT-02**：提供 `Stats::snapshot()` 返回当前快照（无锁，不阻塞热路径）
- **FR-STAT-03**：封装 `ef_vi_stats_query()` 硬件统计为 `NicStats` 结构
- **FR-STAT-04**：提供 `reset()` 接口清空所有计数器
- **FR-STAT-05**：支持 `latency_ns` 字段（TX 完成时间戳 − 发包时间戳，CTPIO 路径）

### 3.8 日志接口（FR-LOG）

- **FR-LOG-01**：`ViConfig` 中包含 `LogCallback` 字段，类型为 `std::function<void(LogLevel, const std::string&)>`
- **FR-LOG-02**：`LogLevel` 枚举：`DEBUG` / `INFO` / `WARN` / `ERROR`
- **FR-LOG-03**：未设置回调时，库完全静默（不输出任何内容）
- **FR-LOG-04**：热路径（send/poll）不产生日志调用（即使设置了回调）
- **FR-LOG-05**：初始化路径、过滤器操作、错误恢复路径可产生 `INFO` / `WARN` / `ERROR` 级别日志

### 3.9 不封装特性备忘（IN-SCOPE EXCLUSIONS）

以下 ef_vi 特性在当前版本中**有意不封装**，但保留于文档供后续版本参考：

| 特性 | 原因 | 后续版本计划 |
|------|------|------------|
| TX Alternatives（X2522） | 超出当前使用场景，API 复杂度高 | v2.0 可选 |
| `efct_vi_rx_future_peek`（X3522） | 使用场景有限，增加复杂度 | 按需评估 |

---

## 4. 非功能需求

| 编号 | 类别 | 要求 |
|------|------|------|
| NFR-01 | 延迟 | 热路径 `send`/`poll` 不调用 `malloc`/`free`，不使用虚函数，不产生系统调用 |
| NFR-02 | 吞吐 | 目标：单核 ≥ 5M pps 收包（64B UDP），≥ 2M pps 发包 |
| NFR-03 | 内存安全 | 析构时完整释放所有 `ef_vi` / `ef_pd` / `ef_memreg` / `ef_pio` 资源 |
| NFR-04 | 异常安全 | 初始化异常不产生资源泄漏（强异常保证） |
| NFR-05 | 可移植性 | 头文件路径通过 CMake `ONLOAD_SRC_DIR` 配置，不硬编码 |
| NFR-06 | 编译隔离 | Mock 模式（`EFVI_MOCK_MODE`）下不 `include` 任何 ef_vi 头文件 |
| NFR-07 | 测试覆盖 | 核心模块单元测试覆盖率 ≥ 85%，CI 无网卡环境全通过 |
| NFR-08 | 文档 | 所有 public API 提供 Doxygen 注释；`README.md` 包含快速上手示例 |
| NFR-09 | 文档随代码 | 每次 git commit 必须同步更新 `docs/` 下的需求文档和使用文档（见 §10） |

---

## 5. 模块划分与接口设计

### 5.1 目录结构

```
efvi-cpp/
├── include/efvi/
│   ├── vi.hpp            # Vi 主类（RAII 封装）
│   ├── vi_set.hpp        # ViSet 多队列封装
│   ├── config.hpp        # ViConfig / FilterSpec / PerformanceParams / LogCallback
│   ├── packet.hpp        # PacketRef / RxBatch（零拷贝接收抽象）
│   ├── stats.hpp         # Stats / NicStats
│   ├── nic_info.hpp      # NicInfo（网卡元信息）
│   └── exception.hpp     # ViException
├── src/
│   ├── vi_impl.cpp       # Vi 实现（条件编译 EF10/EfCT）
│   ├── vi_set_impl.cpp   # ViSet 实现
│   ├── checksum.cpp      # 软件 checksum（X3522 TX 使用）
│   └── mock/             # Mock 实现（EFVI_MOCK_MODE）
├── tests/
│   ├── test_init.cpp
│   ├── test_tx.cpp
│   ├── test_rx.cpp
│   ├── test_filter.cpp
│   ├── test_stats.cpp
│   ├── test_multiqueue.cpp
│   └── test_log.cpp
├── examples/
│   ├── udp_sink.cpp      # 单队列收包示例
│   ├── udp_send.cpp      # 发包示例
│   └── multiqueue_sink.cpp  # 多队列收包示例
├── docs/
│   ├── PRD.md            # 本文件（需求文档）
│   ├── USAGE.md          # 使用文档（快速上手 + API 参考）
│   └── ARCHITECTURE.md   # 架构说明（可选，Phase 4-H 生成）
└── CMakeLists.txt
```

### 5.2 核心接口草案

#### Vi 主类

```cpp
namespace efvi {

// 日志回调
enum class LogLevel { DEBUG, INFO, WARN, ERROR };
using LogCallback = std::function<void(LogLevel, const std::string&)>;

// NIC 架构类型
enum class NicArch { EF10, EFCT };

class Vi {
public:
  explicit Vi(const ViConfig& cfg);    // RAII 初始化，失败抛 ViException
  ~Vi();
  Vi(const Vi&) = delete;
  Vi& operator=(const Vi&) = delete;
  Vi(Vi&&) noexcept;
  Vi& operator=(Vi&&) noexcept;

  // TX（仅连续 buffer，不支持 scatter-gather）
  void send(const void* buf, size_t len);
  void send_batch(const void* const* bufs, const size_t* lens, int count); // X2522 only

  // RX
  int poll(RxBatch& batch);            // 返回收到包数

  // Filter
  FilterCookie add_filter(const FilterSpec& spec);
  void          remove_filter(FilterCookie cookie);

  // Meta
  NicArch        arch() const;
  const NicInfo& nic_info() const;
  Stats::Snapshot stats_snapshot() const;
  void            reset_stats();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace efvi
```

#### ViSet 多队列类

```cpp
namespace efvi {

class ViSet {
public:
  explicit ViSet(const ViSetConfig& cfg); // 包含 queue_count、每队列 ViConfig
  ~ViSet();

  // 获取指定队列的 Vi（用于绑核收发）
  Vi& vi(int queue_idx);
  int queue_count() const;

  // 聚合统计
  Stats::Snapshot aggregate_stats() const;
  void            reset_all_stats();
};

} // namespace efvi
```

#### PacketRef（X3522 EfCT 专用）

```cpp
namespace efvi {

class PacketRef {
public:
  // 仅由 RxBatch 内部构造，不可直接创建
  ~PacketRef();                         // 自动调用 efct_vi_rxpkt_release()
  PacketRef(PacketRef&&) noexcept;
  PacketRef(const PacketRef&) = delete;

  const void* data() const;
  size_t      len() const;
  uint64_t    timestamp_ns() const;     // 硬件 RX timestamp
};

} // namespace efvi
```

### 5.3 X2522 / X3522 差异处理矩阵

| 处理点 | EF10（X2522） | EfCT（X3522） |
|--------|-------------|-------------|
| TX 发包 | 优先 CTPIO，fallback DMA | 仅 CTPIO + store-and-forward |
| TX Checksum | 硬件 offload | 软件计算（`checksum.cpp`） |
| RX 事件类型 | `EF_EVENT_TYPE_RX` | `EF_EVENT_TYPE_RX_REF` |
| RX buffer 管理 | 用户 post，手动 refill | superbuf，驱动管理 |
| Packet 访问 | 直接指针 | `PacketRef`（get/release RAII） |
| PD `PHYS_MODE` | 支持 | 不支持（初始化时抛异常） |
| Hugepage | 可选 | 强制（库内自动开启） |
| TX Alternatives | 硬件支持（库不封装） | 不支持 |
| 多队列机制 | `ef_vi_set` VISet | EfCT shared RX queue |

---

## 6. 测试需求

### 6.1 单元测试矩阵

| 测试文件 | 覆盖功能 | Mock 环境 | 目标用例数 |
|---------|---------|---------|---------|
| `test_init.cpp` | ViConfig 校验、初始化成功/失败、NicInfo、ViException 消息格式 | 是 | ≥ 15 |
| `test_tx.cpp` | `send()` 路径、X3522 checksum 计算、CTPIO/DMA 路径选择、错误处理 | 是 | ≥ 15 |
| `test_rx.cpp` | `poll()` 收包、RxBatch 迭代、PacketRef RAII、discard 处理、timestamp 提取 | 是 | ≥ 15 |
| `test_filter.cpp` | FilterSpec 构造、add/remove、X3522 限制检查、重复过滤器检测 | 是 | ≥ 10 |
| `test_stats.cpp` | 计数器递增、`snapshot()` 原子性、NicStats 解析、`reset()` | 是 | ≥ 10 |
| `test_multiqueue.cpp` | ViSet 初始化、`vi(idx)` 获取、聚合统计、多线程并发收包 | 是 | ≥ 10 |
| `test_log.cpp` | 回调触发时机、LogLevel 过滤、热路径不触发回调、无回调时静默 | 是 | ≥ 8 |

**总计目标：≥ 83 个单元测试，覆盖率 ≥ 85%（核心模块）**

### 6.2 集成测试（需真实网卡）

- **IT-01**：X2522 环回测试：`send` 1000 个 UDP 包，验证全部收到
- **IT-02**：X3522 收包压测：`efsend` 工具发包，验证 ≥ 1M pps 无丢包
- **IT-03**：过滤器精确性：非目标端口的包不出现在 `poll` 结果中
- **IT-04**：多 Vi 并发：2 线程各持一个 `Vi` 实例同时收发，无互相干扰
- **IT-05**：ViSet 多队列：4 队列配置，每队列绑定独立线程，验证 RSS 分流正确

### 6.3 验收标准

- 所有单元测试在 Mock 模式下通过（CI 环境无网卡，使用 `EFVI_MOCK_MODE`）
- X2522 实机：稳定收发 10 分钟无崩溃，低负载下 `rx_drops = 0`
- X3522 实机：稳定收发 10 分钟无崩溃，`PacketRef` 无泄漏（valgrind 验证）
- ASAN + TSAN 运行单元测试无报告
- `docs/PRD.md` 和 `docs/USAGE.md` 在最终提交分支中存在且与实现一致

---

## 7. 开发里程碑

| 阶段 | 内容 | 完成标志 | 文档要求 |
|------|------|---------|---------|
| Phase 4-A（已完成） | 项目骨架、CMake、Mock 模式、基础测试框架 | 可编译 + 5 Passing Tests | 无 |
| Phase 4-B | Vi RAII 初始化、ViConfig、NicInfo、ViException、NicArch 检测、LogCallback | `test_init.cpp` + `test_log.cpp` 全通过 | `docs/PRD.md` v1.2 提交 |
| Phase 4-C | EF10 TX 路径（DMA/PIO/CTPIO）+ TX 统计 | `test_tx.cpp` EF10 通过 | — |
| Phase 4-D | EF10 RX 路径 + ring refill + 统计 + timestamp | `test_rx.cpp` EF10 通过 | — |
| Phase 4-E | EfCT TX（CTPIO only）+ 软件 checksum | `test_tx.cpp` EfCT 通过 | — |
| Phase 4-F | EfCT RX（superbuf / PacketRef RAII） | `test_rx.cpp` EfCT 通过 | — |
| Phase 4-G | Filter 管理（add/remove/X3522 限制） | `test_filter.cpp` 全通过 | — |
| Phase 4-H | ViSet 多队列（EF10 + EfCT） | `test_multiqueue.cpp` 全通过 | — |
| Phase 4-I | Stats 完善 + examples + README + Doxygen | `test_stats.cpp` 全通过 + 示例可运行 | `docs/USAGE.md` 提交 |
| Phase 5-A | 实机集成测试（X2522 / X3522） | IT-01 ~ IT-05 全通过 | `docs/` 最终版本随 release 分支提交 |

---

## 8. 关键设计决策

| 决策 | 决定 | 理由 |
|------|------|------|
| 热路径多态 | 编译期 `if/switch on arch()`，不用虚函数 | 避免 vptr 间接调用，<100ns 场景显著 |
| 内存分配 | 预分配 + 热路径零动态分配 | `malloc` 引入 jitter，对 HFT 不可接受 |
| 统计计数器 | `std::atomic`（`memory_order_relaxed`） | stats 无需跨变量一致性，relaxed 最低开销 |
| X3522 checksum | 软件实现（`checksum.cpp`） | X3522 EfCT 不支持 TX checksum offload |
| 错误处理 | 初始化路径用 exception，热路径用返回值 | 初始化非热路径；热路径需要 zero-cost |
| 接口隔离（pimpl） | `Vi::Impl` 隐藏所有 ef_vi C 头文件 | 隔离 C API 污染，公共头文件保持整洁 |
| Mock 模式 | 编译宏 `EFVI_MOCK_MODE` 完全隔离 | CI 环境无法安装 OpenOnload |
| TX 接口 | 仅支持连续 buffer，不支持 scatter-gather | 简化接口，当前使用场景不需要 |
| 日志接口 | `LogCallback` 用户回调，库静默 | 避免日志库依赖，适配不同部署环境 |
| TX Alternatives | 不封装（EF10 硬件特性保留记录） | 超出当前场景，API 复杂度不值得 |

---

## 9. 已确认问题清单

以下问题已在 v1.2 中全部确认，无待决事项：

| 编号 | 问题 | 决定 | 影响 |
|------|------|------|------|
| Q-01 | TX Alternatives 是否封装？ | **不封装**，需求文档保留记录 | Phase 4-C 工作量不变 |
| Q-02 | RSS 多队列是否需要？ | **需要**，新增 `ViSet` 类 | 新增 Phase 4-H，`test_multiqueue.cpp` |
| Q-03 | X3522 `rx_future_peek` 是否封装？ | **不封装** | Phase 4-F 工作量不变 |
| Q-04 | TX 是否支持 scatter-gather？ | **不支持** | TX 接口简化，仅连续 buffer |
| Q-05 | 诊断日志输出方式？ | **用户回调**（`LogCallback`） | 新增 FR-LOG 章节，`test_log.cpp` |

---

## 10. 文档输出要求

### 10.1 文档结构

最终提交分支（`release` 或 `main`）必须包含以下文档：

```
docs/
├── PRD.md        # 本文件：项目需求文档（随需求变更同步更新）
├── USAGE.md      # 使用文档（Phase 4-I 完成时生成）
└── ARCHITECTURE.md  # 架构说明（可选，建议 Phase 4-H 生成）
```

### 10.2 USAGE.md 内容要求

`USAGE.md` 必须包含以下章节：

1. **环境要求**（OS、编译器、Onload 版本、CMake 版本）
2. **编译与安装**（CMake 命令，含 Mock 模式和真实网卡模式两套命令）
3. **快速开始**（30 行以内完整可运行示例：初始化 → 设置过滤器 → 收包循环 → 统计输出）
4. **配置参数参考**（`ViConfig` 所有字段说明，含默认值）
5. **多队列使用**（`ViSet` 快速示例 + 绑核方法）
6. **性能调优指南**（CTPIO threshold、RXQ depth、hugepage 配置、CPU 亲和性）
7. **API 速查表**（所有 public 方法一句话描述）
8. **X2522 vs X3522 差异说明**（用户可能遇到的行为差异）
9. **常见错误与解决**（`ViException` 常见错误码含义 + 解决方法）

### 10.3 提交规范

- Phase 4-B 提交时：`docs/PRD.md`（v1.2，即本文件）必须包含在 commit 中
- Phase 4-I 提交时：`docs/USAGE.md` 初稿必须包含在 commit 中
- Phase 5-A（release 分支）提交时：`docs/` 下所有文档必须与实现对齐，作为 release 的必要条件
- 禁止在无文档更新的情况下修改 public API（`include/efvi/` 下任何文件）

---

*efvi-cpp PRD v1.2 | 所有设计决策已确认，无待决问题 | 下一步：Phase 4-B 执行 Session*
