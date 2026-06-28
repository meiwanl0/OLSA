# Archive / BinaryArchive / TextArchive 架构分析与正向设计

## 1. 目标

本文基于 `FTMLCore.dll` 的 Binary Ninja MCP 逆向结果，分析 `Archive`、`BinaryArchive`、`TextArchive` 三个类之间的关系，并给出适用于当前 OLSA 项目的正向开发架构建议。

本文刻意不讨论具体解析算法、压缩细节、字节布局微观分支，只关注以下问题：

- 三个类的职责边界是什么
- 运行时协作关系是什么
- 当前项目在正向开发时应该如何组织抽象层与实现层
- 如何在不破坏架构稳定性的前提下继续扩展

## 2. 逆向结论摘要

通过 Binary Ninja MCP 检索到的符号可以确认：

- `FTML::Archive` 是统一的归档抽象层，不是单纯的二进制读取器
- `FTML::BinaryArchive` 与 `FTML::TextArchive` 是两个并列的具体归档实现
- 两个具体类都实现了相同的核心接口：`next()`、`doGet()`、`doPut()`、`isText()`、`blob()`、`readCopy()`
- 两个具体类都提供静态工厂：`make(...)`、`makeBase()`，说明系统原始设计中已经存在“统一抽象 + 具体介质实现 + 工厂创建”的架构思想
- `Archive::get(...)` 是统一入口，它先写入“期望读取的类型/长度”等上下文，再通过虚调用分派给具体实现
- `Archive::getClassAndModuleLists()` 等基类流程函数构成上层协议编排，说明 `Archive` 承担的是“模板方法/协议门面”职责，而不是底层介质细节

换句话说，原始产品的真实关系不是“`BinaryArchive` 比 `Archive` 多一点功能”，而是：

- `Archive` 定义协议
- `BinaryArchive` 实现二进制协议载体
- `TextArchive` 实现文本协议载体
- 上层业务对象统一依赖 `Archive`

## 3. Binary Ninja 证据

### 3.1 已确认的符号

Binary Ninja MCP 中可直接检索到以下函数族：

- `FTML::Archive::get(...)`
- `FTML::Archive::getClassAndModuleLists()`
- `FTML::Archive::checkVersions()`
- `FTML::Archive::readCopy()`
- `FTML::Archive::isText()`
- `FTML::BinaryArchive::next()`
- `FTML::BinaryArchive::doGet()`
- `FTML::BinaryArchive::doPut()`
- `FTML::BinaryArchive::make(...)`
- `FTML::BinaryArchive::makeBase()`
- `FTML::TextArchive::next()`
- `FTML::TextArchive::doGet()`
- `FTML::TextArchive::doPut()`
- `FTML::TextArchive::make(...)`
- `FTML::TextArchive::makeBase()`

这说明三者关系明确是“一个抽象基类 + 两个具体派生类”的体系，而不是三个互不相关的工具类。

### 3.2 从 `Archive::get(...)` 看基类职责

逆向结果表明 `Archive::get(...)` 的行为大致是：

1. 记录期望类型
2. 记录期望数量
3. 通过虚表调用具体实现的读取函数
4. 调用后清理期望状态

这说明 `Archive::get(...)` 是稳定的统一入口，负责定义“读取协议”，而不是关心底层数据来自文本还是二进制。

### 3.3 从 `BinaryArchive::next()` / `TextArchive::next()` 看派生类职责

逆向结果表明：

- `BinaryArchive::next()` 负责从二进制流推进到下一个逻辑条目，并填充当前 `ItemHeader`
- `TextArchive::next()` 负责从文本 token 流推进到下一个逻辑条目，并填充当前 `ItemHeader`
- 两者都在“生成下一个可消费条目”
- 两者都把结果统一映射到同一种归档条目语义上

这说明 `next()` 的本质不是“读字节”或“扫 token”，而是将不同介质统一提升为同一种归档协议事件。

### 3.4 从 `doGet()` 看协议分层

逆向结果表明：

- `BinaryArchive::doGet()` 会校验当前条目与期望类型是否匹配，必要时做类型转换，然后消费当前条目
- `TextArchive::doGet()` 也做相同级别的协议校验，只是其底层数据来源于文本扫描结果

因此，`doGet()` 的职责不是业务字段赋值，而是：

- 校验协议一致性
- 执行必要转换
- 从“当前条目”拷贝出目标数据
- 标记条目已消费

### 3.5 从 `make()` / `makeBase()` 看对象创建方式

逆向结果表明，`BinaryArchive` 与 `TextArchive` 都提供静态工厂，并返回统一引用计数基类对象体系中的实例。

这说明原始设计至少具备以下架构意图：

- 对象创建不直接暴露给调用方
- 调用方通过抽象类型持有实例
- 具体是文本归档还是二进制归档，由工厂决定

这属于典型的“工厂 + 多态实现”设计。

## 4. 三个类的关系

可以将逆向出的关系抽象为：

```text
                +-----------------------+
                |        Archive        |
                |-----------------------|
                | 统一归档协议入口       |
                | 条目状态/期望状态管理   |
                | begin/end/get 编排     |
                | 业务对象统一依赖点      |
                +-----------+-----------+
                            |
          +-----------------+-----------------+
          |                                   |
          v                                   v
+-----------------------+         +-----------------------+
|     BinaryArchive     |         |      TextArchive      |
|-----------------------|         |-----------------------|
| 二进制介质解析         |         | 文本介质解析           |
| next(): 产出条目       |         | next(): 产出条目       |
| doGet(): 消费条目      |         | doGet(): 消费条目      |
| blob()/binary traits   |         | text()/scan traits     |
+-----------------------+         +-----------------------+
```

这个关系在正向开发中应理解为：

- `Archive` 是协议层
- `BinaryArchive` / `TextArchive` 是传输层或介质适配层
- 业务对象只应该依赖协议层，不应该依赖具体载体

## 5. 正向开发时应采用的架构

结合逆向结果与当前项目现状，建议将正向开发架构定义为四层。

### 5.1 协议门面层

核心类型：`Archive`

职责：

- 对外暴露统一读取接口，例如 `get(...)`、`getObject(...)`、`getPointer(...)`
- 维护当前条目状态、期望类型、对象/类/模块上下文
- 封装统一的协议流程，例如 `begin()`、`end()`、`checkVersions()`、`getClassAndModuleLists()`
- 屏蔽不同底层介质的实现差异

设计原则：

- 这里放“流程编排”
- 不放具体二进制解析算法
- 不放文本扫描实现
- 不放业务模型字段映射

### 5.2 介质实现层

核心类型：`BinaryArchive`、`TextArchive`

职责：

- 将底层原始输入转换成统一的 `ItemHeader + payload/meta` 语义
- 实现 `next()`，负责产生下一个可消费条目
- 实现 `doGet()`，负责校验并消费条目
- 必要时实现 `doPut()`，支持反向序列化
- 暴露介质特征，例如 `isText()`

设计原则：

- 二进制与文本实现应并列，不互相依赖
- 派生类只解决“如何从当前介质得到统一条目”
- 一旦条目语义建立完成，后续上层流程应尽量共享

### 5.3 状态模型层

核心类型建议：

- `ArchiveState`
- `ItemHeader`
- `ClassItem`
- `ModuleItem`
- `TaggedObjectIndex`

职责：

- 承载归档运行时状态
- 保存跨条目上下文
- 记录类表、模块表、对象表、偏移、计数器等

设计原则：

- 状态结构与算法解耦
- 状态字段按职责域拆分，而不是堆在类成员平铺
- `Archive` 持有状态，具体实现通过受控方式访问状态

### 5.4 业务反序列化层

核心类型：各业务对象的 `get(const Archive&)` 或独立 parser

职责：

- 基于 `Archive` 读取字段
- 只处理业务字段语义
- 不处理介质细节

设计原则：

- 业务代码只看到 `Archive`
- 不允许业务对象直接依赖 `BinaryArchive`
- 不允许业务对象直接处理偏移、压缩、token 扫描等底层细节

## 6. 推荐的协作流程

正向开发时，建议统一为如下调用链：

```text
业务对象 / 业务解析器
        |
        v
   Archive::get(...)
        |
        v
 Archive 维护期望状态
        |
        v
 具体实现 doGet(...)
        |
        +--> 如当前无可消费条目，则 next()
        |
        +--> BinaryArchive::next() 或 TextArchive::next()
        |
        +--> 统一生成 ItemHeader
        |
        +--> doGet() 校验/转换/消费
        |
        v
 业务对象拿到目标值
```

关键思想是：

- `next()` 负责“产出条目”
- `doGet()` 负责“消费条目”
- `Archive::get()` 负责“定义契约并触发流程”

这三者不要混写，否则代码会迅速退化成状态错乱的解析器。

## 7. 对当前 OLSA 项目的正向设计建议

结合当前仓库与历史重构结论，推荐采用“组合优先”的现代化版本，而不是继续强化继承层级。

### 7.1 推荐架构

建议的最终结构不是：

- `BinaryArchive` 继承 `Archive` 并把所有逻辑都塞进子类

而是：

- `Archive` 作为稳定门面
- `IArchiveEngine` 作为底层实现策略接口
- `BinaryArchiveEngine` 作为二进制实现
- `TextArchiveEngine` 作为未来文本实现
- `ArchiveState` 独立承载运行时状态
- `BinaryArchive` 可以只是“预配置为二进制引擎的便捷入口”，甚至最终弱化为别名/包装器

也就是：

```text
业务对象
   |
   v
 Archive
   |
   +--> ArchiveState
   |
   +--> IArchiveEngine
            |
            +--> BinaryArchiveEngine
            +--> TextArchiveEngine
```

这样做的好处：

- 比“基类 + 大量虚函数 + 大量成员继承”更容易维护
- 状态与行为分离更清晰
- 更容易写单元测试
- 更容易支持未来新格式
- 更符合当前项目已经记录下来的重构方向

### 7.2 为什么当前项目更适合组合而非继续继承

逆向分析告诉我们“原始产品是多态归档体系”，但正向开发不必机械复制实现形式。

当前项目更适合把原始设计中的“抽象协议”保留下来，把“实现形式”现代化：

- 保留 `Archive` 作为统一协议入口
- 保留 `BinaryArchive` / `TextArchive` 作为用户可理解的概念
- 将真正复杂的实现下沉到 engine
- 将可变状态下沉到 `ArchiveState`

这样既忠实于逆向出来的系统意图，又不会被历史类层级绑死。

## 8. TextArchive 在当前项目中的定位

当前仓库中尚未看到 `TextArchive` 的正向实现文件，但从 FTMLCore 的符号来看，它不是边角料，而是与 `BinaryArchive` 对称的一等公民。

因此在当前项目中应当这样定位：

- 即使暂时不实现 `TextArchiveEngine`，架构上也要为其预留插槽
- 不要把 `Archive` 设计成只服务二进制格式
- 所有基类命名、接口语义、状态字段都应避免“仅二进制化”
- 未来如果补齐文本格式支持，应只新增 engine/adapter，而不是重写业务层

## 9. 建议的目录与模块拆分

建议后续按职责拆分为：

```text
include/
  archive.hpp               // Archive 门面接口
  ArchiveState.hpp          // 运行时状态
  ItemHeader.hpp            // 条目协议模型
  IArchiveEngine.hpp        // 引擎抽象
  BinaryArchive.hpp         // 二进制便捷入口 / 适配器
  TextArchive.hpp           // 文本便捷入口 / 适配器

src/
  archive.cpp               // Archive 协议编排
  BinaryArchive.cpp         // 二进制入口薄封装
  TextArchive.cpp           // 文本入口薄封装
  BinaryArchiveEngine.cpp   // 二进制核心实现
  TextArchiveEngine.cpp     // 文本核心实现
```

如果当前阶段不准备引入完整 `TextArchive`，也建议至少保留：

- `IArchiveEngine`
- `BinaryArchiveEngine`
- `ArchiveState`
- `TextArchive` 的接口占位

## 10. 开发约束建议

为了让后续演进稳定，建议遵守以下约束：

- 业务模型只依赖 `Archive`，不依赖 `BinaryArchiveEngine`
- `Archive::get()` 只负责协议编排，不嵌入介质分支
- `next()` 与 `doGet()` 必须职责分离
- `ItemHeader` 只表达协议语义，不掺杂业务字段
- 类表、模块表、对象表应视为归档状态，而不是具体实现私有临时变量
- 新增格式支持时，只新增 engine，不改业务读取接口

## 11. 一句话结论

从 Binary Ninja 逆向结果看，`Archive`、`BinaryArchive`、`TextArchive` 的本质关系是：

- `Archive` 是统一归档协议门面
- `BinaryArchive` 与 `TextArchive` 是两种并列的介质实现
- 业务层应面向 `Archive` 编程

在当前 OLSA 项目的正向开发中，最佳落地方式不是继续强化传统继承，而是保留这个抽象关系，并用 `Archive + ArchiveState + IArchiveEngine + Binary/Text Engine` 的组合式架构去实现它。
