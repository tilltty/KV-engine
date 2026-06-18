# KV-Engine：分布式内存KV存储系统

KV-Engine 是一个基于 C++17 的高性能分布式内存键值存储系统。它采用一致性哈希实现水平扩展，利用 boost::asio 异步网络模型处理高并发连接，并支持 AOF（Append-Only File） 持久化，兼顾了高性能与数据可靠性。该项目专为小文件（≤1MB）的高速存取场景设计。

## 核心特性

高性能网络：基于 boost::asio 实现 Proactor 异步模型，单节点可处理数万并发连接。

分布式扩展：采用一致性哈希环 + 虚拟节点技术，节点动态扩缩容时仅迁移少量数据（1/N）。

智能哈希选型：经过多种哈希函数（MurmurHash3、CityHash 等）实验对比，最终选用 FNV-1a 哈希算法，确保虚拟节点在环上分布均匀，显著降低数据倾斜风险。

## 混合存储引擎：

内存存储：基于 std::unordered_map 实现 O(1) 快速存取。

缓存淘汰：内置 LRU（最近最少使用） 策略，有效控制内存水位。

数据持久化：支持 AOF（Append-Only File） 日志重放机制，服务崩溃重启后可快速恢复数据。

生产级可观测性：提供完整的 wrk 压测报告与性能分析。

## 架构设计

1. 一致性哈希与虚拟节点
   系统将物理节点映射到哈希环上，并为每个物理节点生成多个虚拟节点以解决数据分布不均问题。

哈希函数选择（FNV-1a）：在实验中，我对比了多种哈希算法在 150 个虚拟节点下的分布标准差。FNV-1a 不仅计算速度快，且分布随机性最佳，有效避免了特定文件名集中涌入同一物理节点。

2. 异步网络模型
   基于 boost::asio 的 单线程 Reactor + 线程池 模式：

主事件循环：负责监听 Socket 事件，非阻塞地接收请求。

Worker 线程池：处理具体的存取逻辑（如哈希计算、内存拷贝），防止耗时任务阻塞网络 I/O。

3. 持久化机制（AOF）
   采用 追加写日志 策略，每次 set/delete 操作均会先记录日志到磁盘，再更新内存。服务启动时通过重放 AOF 文件重建内存状态，兼顾了性能与数据安全性。

## 快速开始

环境要求
Linux / macOS

GCC 7.4+ / Clang 10+

CMake 3.12+

Boost 1.70+

使用示例：

bash
通过 nc 连接服务
nc localhost 6001
SET name KV-Engine
+OK
GET name
KV-Engine
DEL name
+OK
## 性能测试报告（Benchmark）
我们使用 wrk 工具对单节点进行了高并发压测，主要测试 GET 操作性能。

### 测试环境

CPU：4 核 8 线程

内存：8GB

OS：Ubuntu 22.04

连接数：200

线程数：4

### 测试结果
Running 15s test @ http://192.168.31.64:6001/
  4 threads and 200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    23.43ms   63.57ms   1.25s    98.24%
    Req/Sec     3.04k   682.34     8.31k    83.39%
  Latency Distribution
     50%   16.53ms
     75%   19.53ms
     90%   22.14ms
     99%  303.18ms
  180255 requests in 15.04s, 6.53MB read
Requests/sec:  11984.48
Transfer/sec:    444.74KB

### 性能分析

吞吐量（QPS）：达到 ~12,000，满足中小规模业务场景的高并发需求。

延迟表现：P50 延迟稳定在 16.5ms，P90 保持在 22ms 以内。

长尾优化：当前 P99 延迟约为 303ms（最大 1.25s）。经排查，主要瓶颈在于全局哈希表的锁竞争以及内存分配开销。后续计划引入 分片锁（Sharded Map） 和 tcmalloc 内存分配器来降低长尾延迟。

### 未来优化计划（Roadmap）

分片锁优化：将全局锁拆分为 16-256 个分片，预期将 P99 延迟降低至 50ms 以内。

内存分配器升级：集成 tcmalloc 或 jemalloc，减少多线程下的内存分配抖动。

Raft 共识算法：集成 Raft 协议，实现 Leader 选举与强一致性数据复制。

HTTP/RESTful API：支持通过 HTTP 协议访问，增强通用性。

## 技术栈

语言：C++17

网络库：Boost.Asio

构建工具：CMake

测试工具：wrk, Google Test (GTest)

性能分析：Valgrind, perf

许可证
本项目仅供学习交流使用。

关于作者
独立开发者，对分布式存储与高性能网络编程充满热情。
