# FTMLBase.dll 逆向分析

## 1. 范围

本文记录当前已经坐实的 `FTMLBase.dll` 逆向结论，重点覆盖：

- `FTML::DC::RawDataSet`
- `FTML::Tilt`
- `FTML::HWSetupDef`

## 2. 已确认结论

### 2.1 `FTML::DC::RawDataSet::get`

`FTML::DC::RawDataSet::get` 已确认是 `XMSE::RawDataSet` 的基类读取前缀，核心顺序为：

- `Countable::get`
- `SampleID`，版本 `> 2`
- `SysInfo`，版本 `> 3`

其中：

- `SampleID` 通过 `FTML::TextID::get`
- `SysInfo` 通过 `SmartPointer::extract(..., 0x499602dd, ...)`

这条证据已经与 `node[3]` 顶层 `RawDataSet` 匿名序列完成对照。

### 2.2 `FTML::Tilt::get`

`FTML::Tilt::get` 已确认是一个具名 object 读取过程，字段为：

- `Theta`
- `Phi`
- `Units`

当前样本里 `RawDataSet` 顶层 body 在 `0x7DD` 位置出现的 `BeginObject class_id=0x038084F9` 已确认就是 `FTML::Tilt`。

进一步结合 `FTML::Tilt::Tilt(float, float)`、`FTML::ResultType::Tilt` 初始化链与真实 `node[3]` object dump，可将当前样本相关的 `UnitsInfo` id 收紧为：

- `0x13148DA -> FTML::Units::radians()`
- `0x1314924 -> FTML::Units::arcseconds()`

其中：

- `Tilt(float, float)` 先把 `this+8` 固定写成 `0x13148DA`
- 随后执行 `UnitsConvert(0x1314924, 0x13148DA)`
- `ResultType::Tilt` 默认单位与内部 `InfoRef` 都固定落在 `0x1314924`
- 同一初始化函数还把默认量程设为 `400.0`，与角秒显示口径一致，不支持“度数”解释

### 2.3 `FTML::HWSetupDef::get`

`HWSetupDef::get` 已确认支持两种读取路径：

- 如果当前 archive item 是 char/string，则先取文本，再调用 `HWSetupDef::scan(...)`
- 否则直接按 `uint32` 读取结果

这说明 `HWSetupDef` 既支持字符串表达，也支持位标志表达。

### 2.4 `HWSetupDef::scan`

`HWSetupDef::scan` 已确认具备如下语义能力：

- 跳过空白
- 识别 `Any`
- 识别括号
- 识别 `|` 组合
- 将文本 token 映射为 bitmask

这也是当前项目将 `HWSetupDef` 视为“文本或位标志二选一”的直接证据来源。

## 3. 关键证据函数

### 3.1 `FTML::DC::RawDataSet::get`

函数：

- `?get@RawDataSet@DC@FTML@@UEAA_NAEBVArchive@3@@Z`

反编译已确认：

- `version(0x01A2C46A)`
- `Countable::get`
- `TextID::get(..., "SampleID")`
- `SmartPointer::extract(..., 0x499602dd, "SysInfo")`

### 3.2 `FTML::DC::RawDataSet::put`

函数：

- `?put@RawDataSet@DC@FTML@@UEBA_NAEAVArchive@3@@Z`

反编译已确认：

- `Countable::put`
- `TextID::put(..., "SampleID")`
- `SmartPointer::insert(*(this + 0x20), 0x499602dd, "SysInfo")`

这条基类写出路径说明：

- `DC::RawDataSet` 基类前缀在保存时也直接从当前活动槽位取值
- 至少对 `SampleID / SysInfo` 这两个前缀字段来说，当前未见“重新组装另一份前缀对象体再落盘”的证据

### 3.3 `FTML::Tilt::get`

函数：

- `?get@Tilt@FTML@@QEAA_NAEBVArchive@2@@Z`

反编译已确认：

- `begin(..., 0x038084F9)`
- `Theta`
- `Phi`
- `Units`
- `end(..., 0x038084F9)`

### 3.4 `FTML::HWSetupDef::get`

函数：

- `?get@HWSetupDef@FTML@@QEBAXAEBVArchive@2@AEAI@Z`

反编译已确认：

- 若当前条目是 char/string，则走 `scan`
- 否则走 `Archive::get(..., UInt32, ...)`

### 3.5 `FTML::HWSetupDef::scan`

函数：

- `?scan@HWSetupDef@FTML@@QEBAIV?$CconstArray@D@2@PEAH@Z`

反编译已确认：

- 解析 token 串
- 识别空白、括号、`|`、`Any`
- 将多个命中的 setup 名字合并为位掩码

## 4. 已证实数据结构

### 4.1 `FTML::DC::RawDataSet`

当前已证实字段：

- 基类前缀来自 `Countable`
- `SampleID`
- `SysInfo`

当前已证实的保存语义：

- `SampleID` 直接从 `this + 0x18` 写出
- `SysInfo` 直接从 `this + 0x20` 以 `expected_class_id = 0x499602dd` 写出

注意：

- 当前只证实了这两个业务字段
- 并未恢复完整对象成员布局

### 4.2 `FTML::Tilt`

当前已证实字段：

- `Theta`
- `Phi`
- `Units`

当前已证实最小语义：

- `node[3]` 当前样本中的 `Tilt` 物理落盘为匿名三元组 `[float, float, uint32]`
- 其第三项 `unitsRaw=0x13148DA` 对应 `radians`
- 与 `Tilt` 相关的外部结果/校正口径 `0x1314924` 对应 `arcseconds`

### 4.3 `FTML::HWSetupDef`

当前已证实语义：

- 最终可落为 `uint32` bitmask
- 文本表达可通过 `scan()` 转换
- 二进制表达可直接读 `uint32`

## 5. 样本实证

真实样本 `test.dat` 已经把以下证据坐实：

- `RawDataSet` 顶层 body 的 `0x1B` 槽位是 `SysInfo` 位置上的对象
- 该对象当前样本中实际动态类型为 `FTML::NamedValueSet`
- `RawDataSet` 顶层 body 的 `0x701` 是 `Version` 槽位，样本原始存储为 `Int8(2)`，但调用侧按 `Int16` 读取
- `RawDataSet` 顶层 body 的 `0x7DD` 内嵌 object 是 `FTML::Tilt`
- `RawDataSet` 顶层 body 的 `Tilt.Units` 当前样本值稳定为 `0x13148DA(radians)`
- `RawData` 前缀中的 `HWSetup` 已经可以按 `HWSetupDef` 的双路径规则被当前工程消费

## 6. 对当前代码的影响

当前工程中与 `FTMLBase` 结论直接对应的实现包括：

- `src/XMSERawDataSet.cpp` 中 `read_hwsetup_ref_traced()`
- `src/XMSERawDataSet.cpp` 中 `read_has_min_max_traced()`
- `src/main.cpp` 中 `rawdataset_proven_slots`

## 7. 未确认边界

以下内容当前仍不应作为确定结构：

- `DC::RawDataSet` 的完整成员布局
- `SysInfo` 预期类型为什么在样本里落成 `NamedValueSet` 而不是严格的 `NamedValueTable` 继承链
- `HWSetupDef` 内部注册表的完整静态数据结构

当前只能确认“调用链 + 样本命中 + 动态实际类型”，不能把所有兼容关系写死。
