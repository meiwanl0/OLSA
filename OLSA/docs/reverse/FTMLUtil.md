# FTMLUtil.dll 逆向分析

## 1. 范围

本文只记录当前已经坐实的 `FTMLUtil.dll` 逆向结论，重点覆盖：

- `FTML::NVT::NameString`
- `FTML::NamedValueTable`
- `FTML::NamedValueSet`

## 2. 已确认结论

### 2.1 `NameString` 不是普通 string

`FTML::NVT::NameString` 与普通字符串不是同一个语义层。它内部使用单一 backing string，但多段名字通过内嵌 `NUL` 分隔。

这意味着：

- 不能把它简单等同于 `std::string`
- 展示时必须保留分段语义
- 当前项目中的 `summarize_name_string()` 把 `\0` 显式显示成 `|`，这个决定有真实逆向依据

### 2.2 `NameString::get()` 的行为

`NameString::get()` 不自己逐 token 解析 archive，而是：

- 通过 archive 的 string-like 读取逻辑把 backing string 读入对象
- 然后同步自身的视图/长度字段

因此当前项目为 `NameString` 单独保留 reader 入口是合理的，但底层消费仍遵循字符串读取路径。

### 2.3 `append / set` 证明了 NUL 分段语义

`NameString::append()` 和 `NameString::set()` 都直接支撑了以下结论：

- 多段名字不是多个独立字段
- 而是同一 backing string 中被 `NUL` 分开的 segment

### 2.4 `NamedValueSet` 的版本 1 读取结构

`FTML::NamedValueSet::get()` 已确认：

- 非版本 1 时走 `NamedValueList::get`
- 版本 1 时按 `Named` / `Unnamed` / `AddTables` 三段读取
- 多处使用 `SmartPointer::extract(..., 0x499602dd, ...)`

这说明 `NamedValueSet` / `NamedValueTable` 在读取上是强相关的容器簇，而不是互不相关的两个类型。

## 3. 关键证据函数

### 3.1 `NameString::get`

函数：

- `?get@NameString@NVT@FTML@@QEAA_NAEBVArchive@3@@Z`

已确认行为：

- 通过 archive 把 backing string 读到 `this + 0x20`
- 然后同步 `data / length / view`

### 3.2 `NameString::append`

函数：

- `?append@NameString@NVT@FTML@@QEAAAEAV123@V?$CconstArray@D@3@@Z`

已确认行为：

- 先打印一个单字节分隔符
- 再追加新片段

这与“segment 之间以 `NUL` 分开”一致。

### 3.3 `NameString::set`

函数：

- `?set@NameString@NVT@FTML@@MEAAXV?$CconstArray@D@3@H@Z`

已确认行为：

- 先清空 backing string
- 再打印首段
- 之后以单字节分隔符连接后续 segment
- 最终同步视图字段

### 3.4 `NamedValueSet::get`

函数：

- `?get@NamedValueSet@FTML@@UEAA_NAEBVArchive@2@@Z`

已确认行为：

- `version != 1` 时退回 `NamedValueList::get`
- `version == 1` 时依次处理：
  - `Named`
  - `Unnamed`
  - `AddTables`
- `Unnamed` / `AddTables` 路径都可出现 `SmartPointer::extract(..., 0x499602dd, ...)`

## 4. 已证实数据结构

### 4.1 `FTML::NVT::NameString`

已证实语义字段：

- backing string
- 当前 data/view 指针
- 当前长度
- 多 segment 由 `NUL` 分隔

### 4.2 `FTML::NamedValueTable`

当前已证实的结构级语义：

- 是 `NamedValueSet` 相关读取链的预期容器基型
- `NamedValueSet::get()` 会反复以 `0x499602dd` 作为期望类型进行 `SmartPointer::extract`
- `NamedValueTable::get()` 本体只调用 `Countable::get()`，不直接读取 `Named / Unnamed / AddTables`

注意：

- 当前只证实了“作为预期容器基型出现”
- 尚未完成完整成员布局恢复
- 因此若真实样本中的动态类型只落到 `NamedValueTable` 本体，而非 `NamedValueSet` 派生体，那么“不出现 entry 列表”是符合逆向证据的

### 4.3 `FTML::NamedValueSet`

当前已证实的结构级语义：

- 版本 1 下包含 `Named` / `Unnamed` / `AddTables`
- `Named` 路径里有：
  - `Count`
  - `Name`
  - `Value`
- `Unnamed` 路径里是按条目序列读取对象指针
- `NamedValueSet::get()` 开头会先调用 `NamedValueTable::get()` 读取基类部分，再继续消费 `Named / Unnamed / AddTables`

## 5. 样本实证

真实样本 `test.dat` 中，当前已经坐实：

- `RawDataSet` 顶层 `SysInfo` 槽位的实际动态类型是 `FTMLUtil::FTML::NamedValueSet`
- `ConfigurationSet -> legacyRoot` 会落到 `FTML::NamedValueSet`
- `ConfigurationSet` 的预览输出中可稳定看到：
  - `@Default`
  - `FTML::NamedValueTable`
  - `FTML::NamedValueList`
- 当前 `TF#1 -> FTML::NamedValueList` 已可稳定给出最小原始摘要：
  - `itemCount = 2`
  - 顶层预览 = `value=int8(1), group[3]=[text=@Default, value=uint8(32), pointer=FTML::XMSE::Configuration]`
- 当前 reader 已把这层证据下沉为正式结构化结果：
  - `entriesCountRaw = 1`
  - `structuredEntries[0] = { name=@Default, valueTag=0x20, pointerType=FTML::XMSE::Configuration }`
  - 若 `Entry` 的 pointer 命中 `Configuration`，当前 reader 会继续复用正式 `Configuration` reader，下沉为 `StructuredEntry::configuration`
- 因此 `TF#1` 当前不再只保留 `@Default -> XMSE::Configuration` 的类型名，而是同步保留：
  - `expected / compatibility`
  - `object_id / body_offset / tag`
  - `ConfigurationSummary(system/comment/timestamp/baseName/configInfo...)`
- 与之并行，`NamedValueSet` 的 entry 指针元信息也已正式保留在 reader 结果中：
  - `object_id`
  - `body_offset`
  - `expected / compatibility`
- `ConfigurationSummary::configInfo` 现在也支持最小结构化下沉：
  - 若 `ConfigInfo` 指针实际落到 `FTML::NamedValueTable / FTML::NamedValueSet`
  - 则会把其内部 entry 列表收成 `configInfoSummary.entries[]`
  - 当前仅保留 `key / ordinal / type / expected / compatibility / object_id / body_offset / tag`
- 对真实 `test.dat` 中的 `configInfo@0x2A5CE`，当前动态类型为 `FTML::NamedValueTable<=FTML::NamedValueTable:Exact`
  - 结合 `NamedValueTable::get()` 的逆向实现，这一实例当前不出现 `configInfoEntries[...]` 是预期现象，不再视为 reader 缺口
- 结合 `FTML::NamedValueList::get / addValue / FTML::NVT::Value::get` 的反编译，可进一步坐实：
  - 顶层 `int8(1)` 与 `NamedValueList::get` 读取的 `Entries` 计数字段一致
  - `group[3]` 对应单个 `Entry`
  - `text=@Default` 对应 entry 的 `String` 名称
  - `uint8(32)` 对应 `FTML::NVT::Value` 的内部类型标签 `0x20`
  - 随后的 `pointer=FTML::XMSE::Configuration` 对应该 `Value` 携带的 `Countable` 指针载荷

### 5.1 `FTML::NamedValueList` 结构语义

通过 `FTMLUtil.dll` 反编译当前已经坐实：

- `NamedValueList` 本体是：
  - `Array<String>`
  - `Array<NVT::Value>`
- `NamedValueList::get` 顺序为：
  - `Entries`
  - `begin()`
  - 每个 `Entry` 先读 `String`
  - 再读 `NVT::Value`
- `NamedValueList::addValue` 会把分层名称拆成 `head + tail`
  - 如果还有 `tail`，则递归构造新的 `NamedValueList`
  - 再把 `head -> ptr(nested NamedValueList)` 追加到当前表
  - 如果没有 `tail`，则直接追加 `head -> value`
- `NVT::Value::get` 在版本 1 路径下先读 `Type`
  - 其中样本命中的 `uint8(32)` 与内部标签 `0x20` 对齐
  - 该标签后续走 `SmartPointer::extract(...)`，与样本中的 `XMSE::Configuration` 指针完全一致
  - `Value::setPtr(Countable*)` 写入的是 `0x10`
  - `Value::setPtr(const Countable*)` 写入的是 `0x20`
  - 因此，当前样本中的 `ValueTag=0x20` 可进一步解释为“const Countable pointer”分支，而不是泛化的匿名指针类型
  - 交叉引用进一步显示：
    - `0x20` 的 const-overload 调用点主要出现在 `Value::get` 和只读上下文包装 helper（如 `sub_180022a70`）
    - `0x10` 的 mutable-overload 广泛出现在 `NamedValueList::make`、`NamedValueSet::make`、表达式构造等可变对象创建路径
  - 因此 `0x20 / 0x10` 的差别已经可以稳定解释为“const pointer value / mutable pointer value”的容器协议分流

## 6. 对当前代码的影响

当前工程里与 `FTMLUtil` 结论直接对应的实现包括：

- `src/XMSERawDataSet.cpp` 中 `read_name_string_traced()`
- `src/XMSERawDataSet.cpp` 中 `NamedValueList` 的顶层原始 item 摘要扫描
- `src/XMSERawDataSet.cpp` 中 `NamedValueListSummary::entriesCountRaw / structuredEntries` 的结构化收敛
- `src/XMSERawDataSet.cpp` 中 `NamedValueList` 结构化 entry 对 `Configuration` 指针的递归 reader 复用
- `src/XMSERawDataSet.cpp` 中 `ConfigurationSummary::configInfoSummary` 对 `NamedValueTable` entry 的最小结构化映射
- `src/main.cpp` 中 `summarize_name_string()`
- `ConfigurationSet` / `Configuration` 的 `BaseName`、`ConfigApp` 等字段走 `NameString` 路径，而不是普通 string 路径

## 7. 未确认边界

以下内容当前仍不应作为确定结构：

- `NamedValueTable` 的完整对象成员偏移
- `NamedValueSet` 中每个内部数组/容器的完整布局
- `NamedValueList` 的全量读取协议
- `NamedValueList` 中 `int8(1)` 在更一般样本中的窄存储规则
- `uint8(32)` 在其他样本里是否始终保持“const Countable pointer”语义
- `@Default -> XMSE::Configuration` 在上层业务上是否应提升为正式键值语义

当前只将“版本分支 + Named/Unnamed/AddTables 语义 + 动态对象期望类型”作为坐实结论使用。
