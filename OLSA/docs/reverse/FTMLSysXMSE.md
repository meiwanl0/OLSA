# FTMLSysXMSE.dll 逆向分析

## 1. 范围

本文记录当前已经坐实的 `FTMLSysXMSE.dll` 逆向结论，重点覆盖：

- `FTML::XMSE::DCRecipe`
- `FTML::XMSE::DPRecipe`
- `FTML::XMSE::RawData`
- `FTML::XMSE::Configuration`
- `FTML::XMSE::RawDataSet`
- `FTML::XMSE::ConfigurationSet` 的当前样本证据链

## 2. 已确认结论

### 2.1 `XMSE::DCRecipe::get`

`DCRecipe::get` 已确认在 `DC::DCRecipe::get` 之后继续读取：

- `HWSRef`
- `HWSDark`
- `HWSSample`
- `NumCyclesRef`
- `NumCyclesDark`
- `NumCyclesSample`
- `SumCycles`
- `WRange`
- `AnalyzerRef`
- `AnalyzerSample`
- `Rotation`
- `SymThresh`
- `SumsPerCycle`
- `TimingMode`
- `Saturation`
- `FilterRef`
- `FilterSample`

并且在高版本下还会继续处理：

- `AverageDark`
- `ValidityFlag`
- `NominalPolarizerMotorSpeed`
- `NominalCompensatorMotorSpeed`

### 2.2 `XMSE::DPRecipe::get`

`DPRecipe::get` 已确认在 `DC::DPRecipe::get` 之后读取：

- `Binning`
- `ApplyMultiScanErr`
- `ApplyPSF`
- `ApplyLinearity`
- `ApplyDCOffset`
- `ApplyA0P0Offset`
- `ApplyWShift`
- `ApplyTilt`
- `ApplyIDN`
- `ModelTilt`
- `FixedNoiseStack`

并根据版本继续读取：

- `ModelA0Offset / ModelP0Offset / ModelC0Offset`
- `ApplyGOFAdjustCal`
- `UseNormalizedAB`

当前项目里已经额外坐实：真实样本在匿名前缀中还有 `ConfigApp` 与 `AutoResults`，这是通过样本 + `FTMLUtil::NameString` 证据补齐的工程结论。

### 2.3 `XMSE::RawData::get`

`RawData::get` 已确认顺序为：

- `Countable::get`
- `HWSetup`
- `NumSums`
- `SumsPerCycle`
- `TimingMode`
- `NumPixel`
- `TurnsPerCycle[2]`
- `NumBM`
- `FirstSum`
- `FirstAcqSum`
- `TimeStamp`
- `PixelRange`
- `ClkPeriod`
- `Enc1Lines`
- `Enc2Lines`
- `Sig`
- `Enc1`
- `Enc2`
- `Clk`
- `BM`，仅 `NumBM != 0` 时

其中数组指针期望类型已确认：

- `Sig` -> `FTML::Array2D<unsigned>`，`class_id=0x001D42B7`
- `Enc1 / Enc2 / Clk / BM` -> `FTML::Array<unsigned>`，`class_id=0x001D401E`

### 2.4 `XMSE::Configuration::get`

`XMSE::Configuration::get` 已确认在 `DC::Configuration::get` 之后继续读取：

- `System`
- `Comment`
- 时间戳前缀：`year / month / day / hour / minute / subMinuteRaw`
- `BaseName`
- `ConfigInfo`
- `SumsPerCycle`
- `TimingMode`
- `Saturation`
- `TurnsPerCycle[2]`
- `NumPixel`

但这里现在需要补一个关键纠偏：

- `FTMLBase.dll` 中 `DC::Configuration::get / put` 已直接坐实：
  - 在 `System / Comment / TimeStamp / BaseName / ConfigInfo` 之后
  - 基类层本身还会继续处理：
    - `Extracted`
    - `Calibrations`
    - 以及 `Settings` 相关项
- 因而当前样本里 `ConfigInfo` 之后出现的固定 trailer，不能先验全部归因于 `XMSE::Configuration` 的派生尾字段
- 更严格的证据口径应为：
  - 其中至少有一部分确定属于 `DC::Configuration` 基类尾部
  - 只有在把 `Extracted / Calibrations` 这段固定边界完全消费完之后，才有资格继续判断是否还存在 `XMSE` 派生尾字段

并且新增两个关键结论：

- `XMSE::Configuration::get` 虽然会顺序调用 `Archive::get(operator()(arg2, "..."), ...)` 读取上述 6 个尾字段
- 但函数体并不会检查这些 `Archive::get(...)` 的返回值
- 也就是说，尾字段缺失不会让整个 `Configuration::get` 失败
- 这说明当前 DLL 的 reader 语义本身就允许较老落盘体缺失 `xmseTail`，而仍然把对象作为合法 `XMSE::Configuration` 读出
- 与真实 `test.dat` 的当前字典视图交叉后，可进一步补充：
  - 当前文件内可见的类/模块字典统一解析为 `module_version=7.00.14`
  - 这意味着如果文件里确实混有较老年代写入的 `Configuration` 体，更可能的形态不是“多套字典并存”
  - 而是旧对象体被保留，而较新的索引/字典层已经覆盖到当前版本视角
- 同时，当前工程在匿名 `Configuration` reader 中只在确认命中 `Extracted + BeginGroup` 这一对固定边界后，才消费 `DC::Configuration` 的固定尾部：
  - `Extracted`
  - `Calibrations` group
- 然后再以 `BinaryArchive::doGet()` 的数值转换能力接受紧凑整型编码的 6 个派生尾字段
- 用真实 `test.dat` 回归后，10 个 `XMSE::Configuration` 实例现在全部稳定命中 `xmseTail`
- 具体模式为：
  - `legacyRoot.@Default` 命中 `sumsPerCycle=160, timingMode=3, saturation=64000, turnsPerCycle=[5,1], numPixel=1024`
  - `TF#1 / CD#2` 命中 `sumsPerCycle=320, timingMode=3, saturation=64000, turnsPerCycle=[5,1], numPixel=1024`
  - `TurboFilm#3 / FoG#4 / FDC#5 / TurboShape#6 / IDO#7 / TrueShape#8 / SPA#9` 命中 `sumsPerCycle=160, timingMode=3, saturation=64000, turnsPerCycle=[5,1], numPixel=1024`
  - 但 `legacyRoot.@Default` 与后面 9 个生成配置在 `Calibrations` group 的内部形态并不相同：
    - `legacyRoot.@Default@0x2A5CE` 的 group 非空，实物字节直接出现重复的 `UInt16(0x0142) + BeginPointer(0x49087FA9, ...)` 片段
    - 这与 `DC::Configuration::get/put` 中 `Settings + Calibration*` 的循环体对上
    - `0x86E76` 至 `0x8742C` 这一簇生成配置则稳定表现为 `BeginGroup EndGroup` 的空 group
  - 当前 reader 已把这段最小化物化为只读摘要：
    - `Extracted`
    - `Calibrations` 是否为空
    - 每个非空条目的 `settingsRaw`
    - 以及 calibration pointer 的动态类型 / 兼容关系 / `object_id` / `body_offset`

### 2.5 `test.dat` 中 `node[3]` 的 `FinalIndex` / section 实物布局

通过本地 `BinaryArchive::read_first_section()` 实现与 `test.dat` 实际字节对照，可进一步坐实：

- 当前工程里的 `body_offset / section_offset` 口径需要严格理解为：
  - `BinaryFileReader::read_data_block(node_index)` 只会把单个 node 的数据块读入 `state.input.buffer`
  - `BinaryArchive::reset()` 只是把这块 node buffer 放入 `ArchiveState::Input::buffer`
  - `state.input.offset` 始终是在当前 node buffer 内移动
  - 因而 `0x8742C / 0x8748E / 0x874AA` 这类偏移是 `node[3]` 内偏移，不是整个文件的绝对偏移
- 对应到真实文件时，需额外加上 `node[3].offset = 0x6D8`：
  - `SPA#9.@Default` body(abs) = `0x87B04`
  - `SPA#9.@Default.ConfigInfo` body(abs) = `0x87B66`
  - 当前活动 section(abs) = `0x87B82`

- `read_first_section()` 的逻辑是：
  - 先读取 `BeginObject(0x04D69F50 = FTML::BinaryArchive)`
  - 再读取紧随其后的 `Int32` 作为 `section_offset`
  - 临时跳转到该 `section_offset` 处读取当前 class/module list
  - 然后回到保存位置继续读取顶层对象指针
- `test.dat` 的 `node[3]` 起始字节（文件偏移 `0x6D8`）可直接解出：
  - `23 04 50 9F D6 04` -> `BeginObject(class_id=0x04D69F50)`
  - `06 AA 74 08 00` -> `Int32(section_offset=0x874AA)`
- 因而 `node[3]` 当前活动 section 的入口就是同一 node 内的 `0x874AA`

再把已确认的 `XMSE::Configuration` 体映射到该 `section_offset`，可得到：

- `legacyRoot.@Default` body=`0x2A575`
- `TF#1.@Default` body=`0x86E76`
- `TurboFilm#3.@Default` body=`0x86FFF`
- `FoG#4.@Default` body=`0x870AF`
- `FDC#5.@Default` body=`0x8715F`
- `TurboShape#6.@Default` body=`0x87216`
- `IDO#7.@Default` body=`0x872C6`
- `TrueShape#8.@Default` body=`0x8737C`
- `SPA#9.@Default` body=`0x8742C`

其中最接近 `section_offset=0x874AA` 的对象体关系为：

- `SPA#9.@Default` body=`0x8742C`，距离当前 section 仅 `0x7E`
- `SPA#9.@Default.ConfigInfo` body=`0x8748E`，距离当前 section 仅 `0x1C`

把 `SPA#9` 末尾这一小段按真实字节展开后，还能拿到一个比“对象体在前、section 在后”更硬的边界结论：

- `SPA#9.@Default.ConfigInfo` 的 node 内偏移 `0x8748E` 对应真实文件 `0x87B66`
- 从 `0x87B66` 到 section 入口 `0x87B82` 之间，不是空白区，而是稳定存在的一段固定 trailer：
  - `26` -> `EndPointer`
  - `0A 42 01`
  - `21 22`
  - `09 A0`
  - `09 03`
  - `0A 00 FA`
  - `09 05`
  - `09 01`
  - `05 00 04`
  - `26 22 26 22 26 26 26 24`
- 然后才在 `0x87B82` 命中当前活动 section 的首字节：
  - `21` -> `BeginGroup`
  - `04 33`
  - `21`
  - `0B C1 3A 1D 00`
  - `82 01 0D "FTML::Archive"...`
- 这说明在 `test.dat` 里：
  - 边界不是漂移的，也不是“读到哪算哪”
  - 而是已经被固定写入
  - 当前还未完全解释的，只是 `ConfigInfo` 之后这段固定 trailer 的业务语义，而不是它的存在性或边界本身

再结合 `FTMLBase.dll` 中 `DC::Configuration::put` 的真实顺序：

- `Countable`
- `System`
- `Comment`
- `TimeStamp`
- `BaseName`
- `ConfigInfo`
- `Extracted`
- `Calibrations`
- `EndObject`

可把当前样本边界进一步收紧为：

- `SPA#9.@Default.ConfigInfo@0x8748E` 之后的固定 trailer，前半段可直接归入 `DC::Configuration` 基类尾部
- 并且在真实文件 `0x87B81` 已直接命中 `EndObject(0x24)`，后一个字节 `0x87B82` 才进入当前活动 section
- 因而对这一个真实样本对象来说：
  - `ConfigInfo` 后确实还有固定尾部
  - 其中 `EndPointer + UInt16(0x0142) + BeginGroup/EndGroup` 已与 `Extracted + empty Calibrations` 对齐
  - 其后的 `09 A0 / 09 03 / 0A 00 FA / 09 05 / 09 01 / 05 00 04` 已由当前 reader 在真实 `test.dat` 上稳定物化为 `xmseTail`
  - 因而当前对象体是在 `DC` 基类尾部之后继续写出 6 个紧凑整型编码的 `XMSE` 派生尾字段，然后才在 section 前闭合

再与 `legacyRoot.@Default.ConfigInfo@0x2A5CE` 的真实字节交叉，可把边界进一步坐实为：

- `0x2A5CE` 开始同样先出现 `EndPointer + UInt16(0x0142) + BeginGroup`
- 但该 group 不是空的，而是立即进入重复的：
  - `UInt16(0x0142)`
  - `BeginPointer(class_id=0x49087FA9, ...)`
  - pointer body / `EndPointer`
- 这与 `DC::Configuration::get` 中：
  - `HWSetupDef::get(operator()("Settings"), &var_78)`
  - `SmartPointer::extract(..., 0x49087FA9, ...)`
  的循环体逐项对齐
- 因而 `legacyRoot.@Default` 当前样本可进一步确认为：
  - `ConfigInfo`
  - `Extracted`
  - 非空 `Calibrations` group，其中条目形态是 `Settings + Calibration*`
  - group 结束后才继续进入 `XMSE` 的 6 个派生尾字段
- 用最新真实 `test.dat` 报告进一步回归后，可把这条样本级结论收紧为：
  - `legacyRoot.@Default body=0x2A575`
  - `configInfo body=0x2A5CE`
  - `dc(extracted=322(XMSErprc65) calibrations=14)`
  - 14 条 calibration 动态类型当前已完整物化为：
    - `FTMLBase::FTML::AOICal`
    - `FTMLBase::FTML::NACal`
    - `FTMLBase::FTML::WavelengthCal`
    - `FTMLBase::FTML::NoiseCal`
    - `FTMLBase::FTML::P0Cal`
    - `FTMLBase::FTML::ApertureCal`
    - `FTMLBase::FTML::PSFCal`
    - `FTMLBase::FTML::C0Cal`
    - `FTMLSysXMSE::FTML::XMSE::WavePlateCal`
    - `FTMLBase::FTML::ExclusionZoneCal`
    - `FTMLBase::FTML::DCOffsetZoneCal`
    - `FTMLBase::FTML::A0OffsetCal`
    - `FTMLBase::FTML::P0OffsetCal`
    - `FTMLBase::FTML::LinearityCal`
  - 并且这 14 条在当前样本中全部稳定携带 `settingsRaw=322(XMSErprc65)`
  - 结合 `FTMLBase.dll` 中 `HWSetupDef::{get,scan,count,print,remove}` 与 `FTMLSysXMSE.dll` 中 `stdConfigName(uint32_t)` / `getHWSetup(const String&)` 的静态证据，当前已可把 `0x142 == 322` 提升为 XMSE 标准 `HWSetup` 值 `XMSErprc65`
  - 当前 reader/CLI 也已把这个映射最小化接入 `dcTrailer.extracted` 与 calibration entry 的 `settingsRaw`
  - 现阶段仍保持保守的一点是：
    - `XMSErprc65` 作为标准配置名已经坐实
    - 但其内部位集合为何正好覆盖这 14 条 calibration 类型，仍需继续用保存链与默认配置生成链证据闭环

这条布局证据说明：

- 当前 class/module list 的入口位于同一 `node[3]` 的后半段
- 而 `ConfigurationSet` 及其 10 个 `XMSE::Configuration` 体都位于该入口之前
- 因而“对象体在前、当前索引/字典 section 在后”的同 node 布局已经被样本直接支持
- 这并不能单独证明前面的对象体一定来自旧版本
- 但它与“旧对象体被保留，而较新的索引/字典层后来在同一 node 内被重写/追加”的模型高度相容

### 2.5 `XMSE::RawDataSet::get`

`XMSE::RawDataSet::get` 已确认在 `DC::RawDataSet::get` 之后继续读取：

- `Version`
- `DCRecipe`
- `DPRecipe`
- `Rotation`
- `AnalyzerRef`
- `AnalyzerSample`
- `Tilt` flag + `FTML::Tilt`
- `PixShiftRef`
- `PixShiftSample`
- `FilterRef`
- `FilterSample`
- `Ref`
- `Dark`
- `Sample`
- `ConfigSet`

这条调用链已经与真实样本 `node[3]` 顶层 `RawDataSet` body 对上了如下锚点：

- `0x19`：`SampleID` 前缀候选
- `0x1B`：`SysInfo` 槽位，动态实际类型为 `FTML::NamedValueSet`
- `0x701`：`Version`
- `0x703`：`DCRecipe`
- `0x76E`：`DPRecipe`
- `0x7DD`：`FTML::Tilt`
- `0x800 / 0x880 / 0xCC0`：三条 `RawData`
- `0x2A52E`：`ConfigurationSet`

若把这些 node 内偏移统一换算成文件绝对偏移（`+ node[3].offset = 0x6D8`），则顶层前缀还可进一步拆成：

- `0x6E3`：顶层 `RawDataSet` pointer slot，形状为 `25 84 <0xDE281900> <tag>`
- `0x6F1`：`RawDataSet` body 起点
- `0x6F1..0x6F2`：`SampleID` 前缀候选，当前样本形状为 `82 00`
- `0x6F3`：`SysInfo` pointer slot，形状为 `25 84 <0x499602DE> <tag>`
- `0x701`：`SysInfo` pointer body 起点
- `0xDD8`：这条 `SysInfo` pointer 的真实 `EndPointer`
- `0xDD9`：`Version`，真实字节为 `04 02`
- `0xDDB`：`DCRecipe` pointer slot

这说明当前 `RawDataSet` 顶层前缀的真实层级已经可以稳定写成：

```text
RawDataSet slot @0x6E3
  -> RawDataSet body @0x6F1
     -> SampleID candidate @0x6F1
     -> SysInfo slot @0x6F3
        -> SysInfo body @0x701
        -> EndPointer @0xDD8
     -> Version @0xDD9
     -> DCRecipe slot @0xDDB
```

其中 `0x701` 的 `Version` 现在已具备双重证据：

- 反编译显示 `XMSE::RawDataSet::get` 先执行 `Archive::get(operator()(arg2, "Version"), Int16, 1, ...)`
- 真实样本 `node[3]` 在 `0x701` 处出现原始字节 `04 02`，即 `Int8(2)`，并且后续无缝进入 `0x703` 的 `DCRecipe` pointer

这说明：

- 顶层匿名 body 在该位置确实承载 `Version`
- 归档层对 `Version` 读取依赖数值类型转换，不要求 archive 中必须物理存成 `Int16`

同时，新增一条很重要的字节级约束：

- 在 `SysInfo` body 内部，单纯按裸字节值搜索 `0x25/0x26` 会出现假命中
- 例如：
  - `0x729` 落在 `82 01 26 "{1AF...}"` 这一段里，当前更应解释为字符串长度 `0x26`
  - `0xA9E` 也只是普通 payload 中的字节值 `0x25`
- 当前只有结合既有 top-level trace、`Version@0xDD9` 锚点以及 `skip_item_quiet(...)` 的结构化推进，才能把 `0xDD8` 稳定认作 `SysInfo` 的真实 `EndPointer`

因此，现阶段对 begin/end marker 的证据口径应保持为：

- `0x25/0x26` 只有在 `ItemHeader` 语义成立时才可视为 pointer marker
- 不能把 payload 中同值字节直接当作结构边界

`DPRecipe` 与 `Tilt` 之间的匿名标量块也已经被样本级坐实为：

- `0x7CB / 0x7CD`：`Rotation = 1.570796`，形状为 `bool(true) + float`
- `0x7D2`：`AnalyzerRef` 缺失，形状为 `bool(false)`
- `0x7D4 / 0x7D6`：`AnalyzerSample = -0.436332`，形状为 `bool(true) + float`
- `0x7DB / 0x7DD`：`Tilt flag = true`，随后进入 `FTML::Tilt`

`Tilt` 之后的匿名块也已经被样本级坐实为：

- `0x7F3`：`PixShiftRef` 缺失，形状为 `bool(false)`
- `0x7F5 / 0x7F7`：`PixShiftSample = -0.001`，形状为 `bool(true) + float`
- `0x7FC`：`FilterRef`，当前样本为空 `NameString`
- `0x7FE`：`FilterSample`，当前样本为空 `NameString`
- `0x800 / 0x880 / 0xCC0`：依次进入三条 `RawData` pointer

顶层 `RawDataSet` 的业务回收现在也已经坐实：

- `Ref@0x800 -> body@0x80E`
- `Dark@0x880 -> body@0x88E`
- `Sample@0xCC0 -> body@0xCCE`
- `ConfigSet@0x2A52E -> body@0x2A53C`

这四条挂接同时满足：

- 顶层 trace 的 pointer slot 偏移证据
- `body_offset = slot_offset + 0xE` 的样本级稳定关系
- 现有 `DynamicObjectReadResult` 中的 `object_id` / `body_offset` 命中

### 2.6 `XMSE::ConfigurationSet`

当前尚未拿到 `XMSE::ConfigurationSet::get` 的直接导出函数体，但已经通过样本与当前 reader 坐实了以下事实：

- 真实样本 `test.dat` 走 `legacyRoot` 分支
- `legacyRoot` 指向 `FTML::NamedValueSet`
- 其中至少存在 `@Default` -> `FTML::XMSE::Configuration`
- `Configuration` 中已稳定读取：
  - `System`
  - `Comment`
  - `TimeStamp(year/month/day/hour/minute/subMinuteRaw)`
  - `BaseName`
  - `ConfigInfo`
  - `xmseTail`
- `TF#1 -> FTML::NamedValueList` 当前已稳定给出最小原始摘要：
  - `itemCount = 2`
  - 顶层预览 = `value=int8(1), group[3]=[text=@Default, value=uint8(32), pointer=FTML::XMSE::Configuration]`
- 当前工程已把该层最小容器结构正式落到 reader 结果：
  - `entriesCountRaw = 1`
  - `structuredEntries[0] = { name=@Default, valueTag=0x20, pointerType=FTML::XMSE::Configuration }`
  - 若该 pointer 为 `XMSE::Configuration`，则进一步落到 `StructuredEntry::configuration`
- `legacyRoot -> NamedValueSet` 的每个嵌套 pointer entry 现在也正式保留：
  - `object_id`
  - `body_offset`
  - `expected / compatibility`
- 当前真实样本 `TF#1 -> @Default` 已直接给出嵌套配置摘要：
  - `system=XMSErprc65`
  - `comment=FTMLSysXMSE generated (XMSErprc65, 2016/05/19 01:24:45.950)`
  - `object_id=213549488`
  - `body_offset=0x86E76`
- 当前真实样本 `legacyRoot -> @Default` 也已直接给出嵌套配置摘要：
  - `body_offset=0x2A575`
  - `xmseTail={160, 3, 64000, [5,1], 1024}`
  - `Calibrations` group 为非空
- 当前真实样本 `TF#1 -> @Default` 已直接给出：
  - `xmseTail={320, 3, 64000, [5,1], 1024}`
  - `Calibrations` group 为空
- `ConfigurationSummary::configInfo` 也开始保留 pointer 元信息：
  - 当前真实样本里 `body_offset` 稳定可见
  - `object_id = 0`，说明该样本中的 `ConfigInfo` 指针可兼容无对象号壳/内联体路径
- 当前工程还支持在 `ConfigInfo` 实际落到 `NamedValueTable` 时，将其内部 entry 列表下沉为 `configInfoSummary.entries[]`
  - 这一步已由单元测试验证
  - 当前真实 `test.dat` 报告里尚未出现 `configInfoEntries[...]`，因此暂不把其内部条目提升为样本级已证实结果
- 结合 `FTMLUtil.dll` 的 `NamedValueList::get / addValue / NVT::Value::get` 反编译，当前可进一步解释为：
  - 顶层 `int8(1)` 对应 `Entries = 1`
  - 唯一 `Entry` 的名字是 `@Default`
  - `uint8(32)` 是 `NVT::Value` 的内部类型标签 `0x20`
  - `0x20` 与 `Value::setPtr(const Countable*)` 对齐
  - 其后指针载荷为 `FTML::XMSE::Configuration`
  - `0x20` 不是样本特有值：在 `FTMLUtil.dll` 中它与只读上下文包装 helper 复用，而构造可变 `NamedValueList/NamedValueSet` 时走的是 `0x10`

因此，`ConfigurationSet -> legacyRoot -> NamedValueSet -> XMSE::Configuration` 这条链已经是样本级坐实结论。

## 3. 关键证据函数

### 3.1 `XMSE::DCRecipe::get`

函数：

- `?get@DCRecipe@XMSE@FTML@@UEAA_NAEBVArchive@3@@Z`

### 3.2 `XMSE::DPRecipe::get`

函数：

- `?get@DPRecipe@XMSE@FTML@@UEAA_NAEBVArchive@3@@Z`

### 3.3 `XMSE::RawData::get`

函数：

- `?get@RawData@XMSE@FTML@@UEAA_NAEBVArchive@3@@Z`

### 3.4 `XMSE::Configuration::get`

函数：

- `?get@Configuration@XMSE@FTML@@UEAA_NAEBVArchive@3@@Z`

### 3.5 `XMSE::RawDataSet::get`

函数：

- `?get@RawDataSet@XMSE@FTML@@UEAA_NAEBVArchive@3@@Z`

## 4. 已证实数据结构

### 4.1 `FTML::XMSE::DCRecipe`

当前已证实字段：

- `numCyclesRef`
- `numCyclesDark`
- `numCyclesSample`
- `sumCycles`
- `wRange`
- `analyzerRef`
- `analyzerSample`
- `rotation`
- `symThresh`
- `sumsPerCycle`
- `timingMode`
- `saturation`
- `filterRef`
- `filterSample`

### 4.2 `FTML::XMSE::DPRecipe`

当前已证实字段：

- `binning`
- `applyMultiScanErr`
- `applyPSF`
- `applyLinearity`
- `applyDCOffset`
- `applyA0P0Offset`
- `applyWShift`
- `applyTilt`
- `applyIDN`
- `modelTilt`

样本级额外已证实字段：

- `configApp`
- `autoResults` 存在性

### 4.3 `FTML::XMSE::RawData`

当前已证实字段：

- `numSums`
- `sumsPerCycle`
- `timingMode`
- `numPixel`
- `turnsPerCycle[2]`
- `numBM`
- `firstSum`
- `firstAcqSum`
- `pixelRange`
- `clkPeriod`
- `enc1Lines`
- `enc2Lines`
- `sig`
- `enc1`
- `enc2`
- `clk`
- `bm`

### 4.4 `FTML::XMSE::Configuration`

当前已证实字段：

- 继承 `DC::Configuration`
- `System`
- `Comment`
- `TimeStamp(year/month/day/hour/minute/subMinuteRaw)`
- `BaseName`
- `ConfigInfo`
- 派生尾部读取顺序还包括：
  - `SumsPerCycle`
  - `TimingMode`
  - `Saturation`
  - `TurnsPerCycle[2]`
  - `NumPixel`

说明：

- 上述字段是通过 `ConfigurationSet -> legacyRoot -> NamedValueSet -> XMSE::Configuration` 样本链稳定命中的字段
- 在真实 `test.dat` 中，当前可稳定观测到：`2018-04-12 17:17 raw=26743`
- `ConfigInfo` 当前已能稳定识别为 `FTML::NamedValueTable` 兼容链，但其更深层业务容器仍在后续收敛
- 上述 6 个派生字段已由导出实现直接坐实，当前工程也已支持读取并有单测覆盖
- 真实 `test.dat` 回归已经直接命中这组数值，因此现在可提升为样本级已证实结果
- 进一步对当前真实输出做覆盖面归纳后，可确认 `test.dat` 中至少存在两种 `XMSE::Configuration` 覆盖形态：
  - `body=0x2A575`：命中 `coverage=[system,comment,timestamp,configInfo,xmseTail]`
  - `body=0x86E76`：命中 `coverage=[system,comment,timestamp,base,configInfo,xmseTail]`
- 其中 `0x2A575` 这一条 `legacyRoot.@Default` 当前稳定命中：
  - `sumsPerCycle=160`
  - `timingMode=3`
  - `saturation=64000`
  - `turnsPerCycle=[5,1]`
  - `numPixel=1024`
- `0x86E76 / 0x86F37` 这两条 `FTMLSysXMSE generated` 配置当前稳定命中：
  - `sumsPerCycle=320`
  - `timingMode=3`
  - `saturation=64000`
  - `turnsPerCycle=[5,1]`
  - `numPixel=1024`
- `0x86FFF / 0x870AF / 0x8715F / 0x87216 / 0x872C6 / 0x8737C / 0x8742C` 这一簇当前稳定命中：
  - `sumsPerCycle=160`
  - `timingMode=3`
  - `saturation=64000`
  - `turnsPerCycle=[5,1]`
  - `numPixel=1024`
- `0x2A575` 与 `0x86E76..0x8742C` 当前共享同一组 `xmseTail` 读取结果，但不共享同一个 `Calibrations` group 内部布局
- `0x2A575` 与 `0x86E76..0x8742C` 当前不仅在“空 / 非空 group”上分化，而且在 reader 输出层已出现更细差异：
  - `0x2A575`：`dc(extracted=322(XMSErprc65) calibrations=14)`，并附带 calibration pointer 元信息
  - `0x86E76..0x8742C`：`dc(extracted=322(XMSErprc65) calibrations=0)`
- 结合用户提供的 `XMSE::Configuration::get` 进入偏移（文件偏移 = `body_offset + 0x6D8`）与当前统一输出，现已能将 10 次调用映射为：
  - `0x2A575`：`legacyRoot -> @Default`
  - `0x86E76`：`TF#1 -> @Default`
  - `0x86F37`：`CD#2 -> @Default`
  - `0x86FFF`：`TurboFilm#3 -> @Default`
  - `0x870AF`：`FoG#4 -> @Default`
  - `0x8715F`：`FDC#5 -> @Default`
  - `0x87216`：`TurboShape#6 -> @Default`
  - `0x872C6`：`IDO#7 -> @Default`
  - `0x8737C`：`TrueShape#8 -> @Default`
  - `0x8742C`：`SPA#9 -> @Default`
- 其中 `0x86E76` 至 `0x8742C` 这一簇均来自 `NamedValueSet` 中的 `NamedValueList` 条目，当前都稳定命中 `coverage=[system,comment,timestamp,base,configInfo,xmseTail]`
- 当前统一输出中的 `observed` 顺序也已经固定为：
  - `Configuration.System`
  - `Configuration.Comment`
  - `Configuration.TimeStamp.*`
  - `Configuration.BaseName`
  - `Configuration.ConfigInfo`
  - `Configuration.Extracted`
  - `Configuration.Calibrations`
  - `Configuration.SumsPerCycle`
  - `Configuration.TimingMode`
  - `Configuration.Saturation`
  - `Configuration.TurnsPerCycle[0]`
  - `Configuration.TurnsPerCycle[1]`
  - `Configuration.NumPixel`
- `XMSE::Configuration::put`（`0x1800088D0`）已反编译确认会在 `DC::Configuration::put` 之后继续无条件写出：
  - `SumsPerCycle`
  - `TimingMode`
  - `Saturation`
  - `TurnsPerCycle[2]`
  - `NumPixel`
- `XMSE::Configuration` 拷贝构造（`0x1800047D0`）同样会逐项复制上述 6 个派生字段，因此“生成配置在 copy 时丢失 xmseTail”这一假设已被排除
- `SubSystem::setDefaultConfigSet(uint32_t, bool)`（命中字符串 `FTMLSysXMSE generated (%s, %s)\n`）会：
  - `XMSE::Configuration::make()` 创建新的 `XMSE::Configuration`
  - 生成 `TF / CD / <Filter> / <Filter>.TF / <Filter>.CD` 等配置名
  - 对这些对象执行 `setSystemID / setBaseName / removeCals` 等基类层初始化
  - 通过 `SmartPointer::makeCopy()` 派生更多配置条目
- 在该函数当前已坐实的调用序列中，没有看到 `setSumsPerCycle / setTimingMode / setSaturation / setTurnsPerCycle / setNumPixel`
- 继续完整展开 `setDefaultConfigSet(uint32_t, bool)` 的后半段后，可进一步确认：
  - 初始 `@Default` 配置先创建后立刻经 `DC::ConfigurationSet::set(...)` 放入集合
  - 后续 `TF / CD / <Filter> / <Filter>.TF / <Filter>.CD` 全部来自 `SmartPointer::makeCopy()`
  - 每个派生配置在入集合前只执行：
    - `DC::Configuration::setBaseName(...)`
    - `DC::Configuration::removeCals()`
    - `DC::ConfigurationSet::set(...)`
  - 这也直接解释了当前真实样本的 `Calibrations` group 分歧：
    - `legacyRoot.@Default` 没有经过复制后 `removeCals()` 这一步，因此保留 14 条 calibration entry
    - 9 个生成配置都在 copy 后显式调用 `removeCals()`，因此统一收敛为 `calibrations=0`
  - 当前函数体里依然没有任何一次命中：
    - `XMSE::Configuration::setNumPixel`
    - `XMSE::Configuration::setSaturation`
    - `XMSE::Configuration::setSumsPerCycle`
    - `XMSE::Configuration::setTimingMode`
    - `XMSE::Configuration::setTurnsPerCycle`
- 进一步对这 5 个 `XMSE::Configuration` setter 的 xref 检查显示：
  - 当前 DLL 中均未发现直接代码引用
  - 因而在当前可见代码范围内，默认配置生成链不会在构造后再二次填充 `xmseTail`
- 同时，`XMSE::Configuration(uint32_t)` 默认构造会把派生尾字段初始化为非零默认值：
  - `SumsPerCycle = 0x40`
  - `TimingMode = 2`
  - `Saturation = 0xFA00`
  - `TurnsPerCycle[0] = 1`
  - `NumPixel = 0x400`
- 因此当前最强结论是：
  - 9 个 `FTMLSysXMSE generated` 配置并不是因为 copy 或默认构造而“天然没有 xmseTail”
  - 真实 `test.dat` 中 `xmseTail` 缺失更可能来自保存/归档路径对这些派生字段的省略，而不是对象内存里没有这些值
- 新增对保存链容器层的交叉验证：
  - `XMSE::ConfigurationSet` 当前未见 `put` override，集合序列化沿用 `FTMLBase.dll` 中的 `DC::ConfigurationSet::put`
  - `XMSE::RawDataSet::put` 在写 `ConfigSet` 时调用 `SmartPointer::insert(*(this + 0x138), 0xDCCD2C00, operator()(arg2, "ConfigSet"))`
  - 这说明顶层 `ConfigSet` 在落盘时保留的是 `XMSE::ConfigurationSet` 期望类型，而不是先降级成 `DC::ConfigurationSet`
- 继续下探 `FTMLSysXMSE.dll` 中 `SubSystem::setConfigSetXMSE(...)` 与 `RawDataSet::setConfigSet(...)` 可进一步确认：
  - `XMSE::SubSystem::setConfigSet(FTML::P<DC::ConfigurationSet>)` 这个基类签名入口会先断言传入对象 `isA(0xDCCD2C00)`
  - 之后它会对实参执行 `canCast(..., 0xDCCD2C00)`，把 `DC::ConfigurationSet` 基类指针收窄回 `XMSE::ConfigurationSet`
  - 成功收窄后再转调 `setConfigSetXMSE(...)`，因此当前并未看到“先降级到 DC，再以基类形态保存”的分支
  - `setConfigSetXMSE(...)` 先按 `TextID` 定位目标 `RawDataSet`
  - 当传入的新 `ConfigurationSet` 非空时，它还会先刷新 `SubSystem` 当前选中条目的那一对内部指针槽，再继续调用 `RawDataSet::setConfigSet(...)`
  - 然后把传入的 `ConfigurationSet` 交给 `RawDataSet::setConfigSet(...)`
  - `RawDataSet::setConfigSet(...)` 会先用当前 app 名称在新集合上 `find(...)`，并确认结果可 `canCast` 到配置对象；失败时走 `DC::RawDataSet::failBadApp(...)`
  - 只有在一致性检查通过后，它才直接替换 `this + 0x138` 的 `ConfigurationSet` 智能指针
  - 同时它会清空 `this + 0x148` 的相关缓存，并触发一次对象刷新/通知
  - 因而当前保存链更像是“把新的 `ConfigSet` 引用挂回现有 `RawDataSet`”，而不是“回头重写旧 `Configuration` 对象体”
  - 再与 `RawDataSet::put(...)` 交叉后，可把这条链收紧为：保存时实际写出的 `ConfigSet` 永远来自当前 `this + 0x138`，而不是回溯扫描旧 `Configuration` body
- 再继续下探 `RawDataSet::{configSet,baseConfigSet,config,setConfigApp}` 后，可把 `0x138 / 0x148` 的最小语义进一步坐实为：
  - `configSet()` 与 `baseConfigSet()` 都直接返回 `this + 0x138` 上的 `ConfigurationSet` 指针
  - `config()` 直接把当前配置缓存保存在 `this + 0x148`
  - 当 `this + 0x148` 为空时，`config()` 会根据当前 `ConfigSet` 与当前 config-app 名称重新 `build(...)` 出一个 `XMSE::Configuration`
  - `setConfigApp(...)` 在切换 app 后会显式清空 `this + 0x148`，并打印 `Clear config cache`
  - `setConfigSet(...)` 替换 `this + 0x138` 后同样清空 `this + 0x148`
  - 因而 `0x148` 不是独立持久对象体，而是由当前 `ConfigSet` 派生出来的“当前配置缓存”
- 再把前面已经坐实的顶层绝对偏移链一起并入后，可把 overlay/save-chain 的物理分层进一步收紧为：
  - `0x6E3 -> 0x6F1 -> 0x6F3 -> 0x701 -> 0xDD8 -> 0xDD9 -> 0xDDB` 已把 `RawDataSet -> SysInfo -> Version -> DCRecipe` 固定在 node[3] 的前部对象体区域
  - 同一 node 的当前活动 section 入口则位于 `0x874AA(node 内) / 0x87B82(file abs)`
  - 因而当前样本里：
    - `SampleID / SysInfo / Version / DCRecipe` 这条前缀链不是靠后部 section 临时拼出来的视图
    - 它们已经作为前部对象体的稳定内容真实落盘，并明显早于后部字典段
  - 与 `setConfigSetXMSE -> RawDataSet::setConfigSet -> RawDataSet::put` 交叉后，当前更符合的模型是：
    - 旧的前部对象体链继续保留
    - 后续运行时变化主要通过替换 `this+0x138(ConfigSet)`、清空 `0x148/0x150/0x158` 这类派生缓存，以及 finalize 时回填 node 头与后半段 section 来体现
    - 也就是说，当前更像“前部持久对象体 + 后部更新引用/字典”的叠加，而不是“整段 RawDataSet 前缀对象体被原地重写”
- 再继续把 `setDCRecipe/setDPRecipe` 并回这条主线后，还能补一个更细的限定：
  - `RawDataSet::change(P<Countable>)` 现在已直接坐实就是此前匿名的 `0x180015650`
  - 它按动态类型把输入分派到：
    - `setConfigSet(...)`
    - `setDCRecipe(...)`
    - `setDPRecipe(...)`
    - 或直接替换当前 `this + 0x148(config)`
  - `setDCRecipe(...)`：
    - 直接替换 `this + 0x28` 上的活动 `DCRecipe` 指针
    - 立即清空 `this + 0x140`
    - 完成后通过 `notify(..., 0x54368980, 0)` 通知下游
  - `setDPRecipe(...)`：
    - 直接替换 `this + 0x30` 上的活动 `DPRecipe` 指针
    - 若 `configApp` 相比旧 `DPRecipe` 发生变化，会先清空 `this + 0x148`
    - 同时也会清空 `this + 0x140`
    - 完成后通过 `notify(..., 0x4927A4D1, 0)` 通知下游
  - `change(...)` 在 `setConfigSet/setDCRecipe/setDPRecipe` 任一路更新成功后，还会统一清空 `this + 0x158`
  - 再与 `RawDataSet::put(...)` 交叉后，可把保存语义补齐为：
    - `DCRecipe` 直接从 `*(this + 0x28)` 以 `expected_class_id = 0xDCF62C00` 写出
    - `DPRecipe` 直接从 `*(this + 0x30)` 以 `expected_class_id = 0xDCF65100` 写出
  - 因而这两条 recipe 更新路径与 `ConfigSet` 的共同点是：
    - 都是“替换当前活动指针 -> 清理下游缓存 -> 保存时直接从当前槽位落盘”
  - 但也要保留一个边界差异：
    - `ConfigSet` 位于后置 smart-pointer/cache 段 `0x138`
    - `DCRecipe/DPRecipe` 位于前部持久对象槽位 `0x28/0x30`
    - 因而它们并不是“纯后置缓存更新”，而是“前部 recipe 指针更新 + 后置缓存失效”的同类模型
- 再继续下探 `RawDataSet::put(...)` 中 `Ref/Dark/Sample` 的写出路径后，还能补一个更细的分歧：
  - `Ref` 直接从 `*(this + 0x38)` 以 `expected_class_id = 0xDE284800` 写出
  - `Sample` 直接从 `*(this + 0x48)` 以 `expected_class_id = 0xDE284800` 写出
  - `Dark` 则不是始终“原样直写”：
    - 若当前 `DCRecipe::averageDark(...) == 0`，或当前 `Dark` 槽位里底层数组体不存在，则直接把 `*(this + 0x40)` 写出
    - 否则会先构造一个临时 `BinaryArchive`
    - 再把当前 `Dark` 对象插入其中、`finalize+reread` 后重新 `extract(...)`
    - 随后按 `DC::bin(...)` 现算一份新的 dark body，并把这份临时对象写回 `Archive::operator()(arg2, "Dark")`
  - 因而对前部持久对象骨架来说，当前更精确的口径应为：
    - `Ref / Sample` 遵循“活动槽位直接写出”的同类模型
    - `Dark` 既可能直接写当前槽位，也可能在保存时按当前 `DCRecipe` 派生出一份临时对象再写出
    - 这说明前部基础指针槽位并不都是“无条件原样落盘”，其中 `Dark` 已经表现出保存期物化/重算分支
- 继续沿 `getFixedNoiseData()` 与 `measData()` 下探后，又可把 `0x140` 的语义坐实为：
  - `getFixedNoiseData()` 在成功生成 fixed-noise `MeasData` 后，会把结果缓存到 `this + 0x140`
  - 若当前路径无法重新生成，它会直接回用已有的 `this + 0x140`
  - `sub_18001c890` 与 `sub_180021970` 这两个 `measData` 共享 helper 都会反向调用 `getFixedNoiseData()`
  - 因而 `0x140` 更像是 `measData` 生成链中的 fixed-noise 中间结果缓存，而不是独立基础字段
- 目前仍未直接命名但已能确认的一点是：
  - `0x150 / 0x158 / 0x160` 与 `0x140 / 0x148 / 0x138` 同属一段连续的后置 smart-pointer 槽位
  - 构造函数会将 `0x138..0x160` 整段清零，析构函数也按相同顺序整段释放
  - 它们因此大概率都属于围绕 `ConfigSet / config / measData` 派生出来的缓存层，而不是前部 `RawData / Recipe / Tilt` 这类基础对象槽位
- 继续沿 vtable 与未命名虚函数 `0x180015650` 下探后，可再把后两级缓存依赖关系收紧为：
  - 该虚函数会接收一个 `Countable*` 并按动态类型分派到：
    - `setConfigSet(...)`
    - `setDCRecipe(...)`
    - `setDPRecipe(...)`
    - 或直接把 `DC::Configuration`/`XMSE::Configuration` 塞入 `this + 0x148`
  - 当它直接更新 `this + 0x148` 时，会立刻清空 `this + 0x150`
  - 当 `ConfigSet / DCRecipe / DPRecipe` 更新成功后，又会继续清空 `this + 0x158`
  - 因而当前至少可保守判定：
    - `0x150` 依赖于“当前 config (`0x148`)”
    - `0x158` 依赖于更高一层的 `ConfigSet / Recipe` 组合状态
  - 这使 `0x150/0x158` 更像分层派生缓存，而不是额外持久对象体
- 同时，setter (`setTilt/setRotation/setPixelShift/...`) 内部统一调用的 `(*this + 0x48)(..., 0x1D4746, 0)` 槽位，在 vtable 中对应的只是 `FTML::Countable::notify(this, arg2, arg3)`：
  - 因而后置缓存的失效机制目前更像“通知驱动 + 懒重建”
  - 而不是每个 setter 都直接手工清 `0x140..0x160`
- 继续把 `RawDataSet` vtable 尾部几个槽位与函数体逐个对齐后，当前可稳定识别为：
  - `0x18006B970 -> 0x18002DC80`：`systemModel()`，按当前 config/校准即时组装 `SystemModel`
  - `0x18006B978 -> 0x180015650`：未命名配置分派入口，负责把传入 `Countable` 分派到 `setConfigSet/setDCRecipe/setDPRecipe` 或直接更新 `0x148`
  - `0x18006B980 -> 0x180018900`：未命名采集/派生入口，会调用 `SubSystem::acquire(...)`，并在需要时复制 `ConfigSet`、补入 `OpticalFilterCal`
  - `0x18006B988 -> 0x18002E220`：按 step 返回 `RawData + 0x40` 上的时间戳区域
  - `0x18006B990 -> 0x180015620`：能力检查，要求 sample 与 dark 的底层数据都存在
  - `0x18006B998 -> 0x1800155F0`：能力检查，要求 sample 与 noise 的底层数据都存在
  - `0x18006B9A0 -> 0x18001B760`：文本文件导入入口，会把导入矩阵写回 sample/dark 的二维数据
  - `0x18006B9A8 -> 0x1800174E0`：文本文件导出入口，会把 sample/noise 的二维数据导出
  - `0x18006B9B0 -> 0x180015D30`：SNR 阈值检查器，检查 sample/noise 是否满足阈值
- 一个容易混淆但现已排除的点：
  - 在 `0x180018900` 中出现的 `*(*config(this)) + 0x160`，访问的是 `XMSE::Configuration` 对象内部字段
  - 它不是 `RawDataSet + 0x160`
  - 因此当前尚未发现 `RawDataSet + 0x160` 的直接读写路径，不能把这两者混为一谈
- 继续对照 ctor / dtor / copy ctor 后，后置缓存段还能再加一条很强的约束：
  - 默认构造会清零 `0x138..0x160`
  - 析构会释放 `0x138..0x160`
  - 但 copy ctor 当前只复制到 `0x150`，没有把 `0x158/0x160` 一并带过去
  - 因而 `0x158/0x160` 至少比 `0x138..0x150` 更“瞬态”，更像运行期懒生成缓存，而不是对象逻辑状态的一部分
- 继续沿“前置检查 / 有效性门槛”这条线可把两个共享 helper 的职责拆开：
  - `sub_180015a80`：
    - 按 bitmask 检查 sample (`0x48`) / background (`0x40`) / chip (`0x38`) 是否存在
    - 检查 sample/dark/ref 之间的 `sumsPerCycle` 一致性与 spectrometer 一致性
    - 在需要时再检查 `ConfigSet (0x138)` 与 `config()` 是否存在
    - 其当前 callers 为：
      - `binStep(...)`
      - `idnFactor()`
      - `idnIntensity()`
      - `sub_18001c890`
      - `sub_180021970`
  - `sub_18002e270`：
    - 不触碰后置缓存
    - 它共享于 `get()/put()/systemModel()`
    - 作用是围绕 `ConfigurationSet` 的 `@Default` 与各配置条目做 calibration/HWSetup 一致性校验，并在发现 wildcard 时补写标准 `HWSetup`
    - 因而它更像“配置有效性门槛”，不是缓存重建入口
- 新增一个容易误判但很有价值的结果：
  - 在 `acquire()` 中再次出现的 `*(*config(this)) + 0x160`，仍然属于 `XMSE::Configuration`
  - 其使用方式是：
    - 若 recipe 自身未给出 `0x74/0x78` 对应的 signal saturation 阈值，则退回使用 `*(*config(this) + 0x160)`
    - 然后将该阈值传给 `FTML::DC::saturationCheck(..., "signal", threshold, ...)`
  - 因而 `Configuration + 0x160` 当前至少可保守收敛为：
    - 与 signal saturation 检查直接相关的阈值字段
    - 不是 `RawDataSet + 0x160`
- 继续沿 `idnFactor()/idnIntensity()` 下探后，还能再排除一条误判路径：
  - 这两条函数都先经 `sub_180015a80(..., 0xD, ...)` 做 sample/dark/ref/config 前置检查
  - `idnIntensity()` 的主路径是：
    - `binStep(this, step=1, ...)`
    - 按 RawData 的二维矩阵计算强度数组
    - 如存在 chip filter，则再用 `OpticalFilterCal::evalFilter(...)` 做逐点修正
  - `idnFactor()` 的主路径是：
    - 先取 `IDNCal`
    - 再调用 `idnIntensity()`
    - 用 `IDNCal::idnRef(...)` 逐点归一化，并在 sample filter 存在时继续做 `OpticalFilterCal` 修正
  - 在当前已拆开的函数体中，未出现对 `RawDataSet + 0x158` 或 `RawDataSet + 0x160` 的直接读写
  - 因而 `idnFactor/idnIntensity` 当前更像“纯算法路径”，不是 `0x158/0x160` 的缓存入口
- 继续沿 `binStep()` 下探后，`0x150/0x158` 终于可以开始带用途命名：
  - `0x150`：
    - `binStep()` 在命中 `PSFCal` 路径时会检查 `*(this + 0x150)`
    - 若为空或长度不匹配，则调用 `FTML::PSFCal::psfFFT(...)` 重建，并写回 `this + 0x150`
    - 随后把 `*(this + 0x150)` 作为频域核输入传给 `FTML::XMSE::RawData::psfCorrect(...)`
    - 因而 `0x150` 当前可保守收敛为：
      - `PSFCal::psfFFT(...)` 生成的 FFT/卷积核缓存
      - 直接服务于 `psfCorrect(...)`
  - `0x158`：
    - 在 `DPRecipe::getOptApplyIDNCal(...)` 允许时，`binStep()` 会调用 `RawDataSet::idnFactor(this)`
    - 然后通过 `sub_180012790(this + 0x158, rax_35)` 将结果写回 `this + 0x158`
    - 后续 `binStep()` 若发现 `*(this + 0x158) != 0`，会打印 `Apply IDN`，并把 `*(this + 0x158)` 的数组数据作为后续校正输入
    - 因而 `0x158` 当前可保守收敛为：
      - `idnFactor()` 结果数组缓存
      - 直接服务于 `Apply IDN` 校正步骤
  - 当前 `binStep()` 中仍未发现对 `RawDataSet + 0x160` 的直接读写
- 继续沿 `sub_180021970` 下探后，`0x160` 也已经能最小命名：
  - `sub_180021970` 被 `getFixedNoiseData()` 与两个 `measData()` 重载共用
  - 它内部会把 `arg_8 + 0x160` 直接作为第 11 个参数传给：
    - `0x180002B00` (AVX MeasData calculations)
    - `0x1800032B0` (SSE2 MeasData calculations)
  - 这两个 SIMD 核心都会：
    - 把 `arg11` 当作 `Countable* / Array<double>` 智能指针槽位
    - 若为空或长度不足，则分配/扩容 `FTML::Array<double>`
    - 将当前输入列先转成 `double` 工作数组，再参与后续 SIMD 计算
  - 因而 `0x160` 当前可保守收敛为：
    - `measData/getFixedNoiseData` 路径复用的 `Array<double>` 工作缓冲区缓存
    - 专供 AVX/SSE2 MeasData 计算核心复用
  - 这也解释了：
    - 为什么 `0x160` 没有出现在 `binStep()/idnFactor()/idnIntensity()` 这类较高层算法路径里
    - 为什么它没有被 copy ctor 带入复制体，它比 `0x150/0x158` 更偏“底层瞬态 scratch cache”
- 继续下探 `FTMLBase.dll` 可确认：
  - `DC::ConfigurationSet::put` 本体只写 `Countable` 基类、`SystemID` 以及内部 `NamedValueSet` 智能指针
  - `DC::ConfigurationSet::set(...)` 将条目值包装为 `NVT::Value::setPtr(const Countable*)`
  - 当前未看到把 `XMSE::Configuration` 条目实体复制/切片成独立 `DC::Configuration` 实例的代码
- 继续下探 `FTMLCore.dll` 可确认：
  - `SmartPointer::insert(const Countable*, expected_class_id, Archive&)` 自身不决定对象体写法，只转发到 `Archive` 的指针写入口
  - `Archive::putPointer(...)` 会先通过 `TaggedObjectList::findTag(...)` 判断当前对象是否已有 pointer tag
  - 若 tag 已存在，则只写 pointer 边界并复用既有 tag，不重复写对象体
  - 只有首次出现的对象才会先 `TaggedObjectList::add(...)`，再继续进入公共对象写出 helper
  - `Archive::putObject(...)` 只有两条落盘路径：
    - 走对象自身虚函数 `put`
    - 或走 `ClassInfo` 上登记的静态 `put` 回调
  - `Archive::putPointer(...)` / `putObject(...)` 最终共用 `sub_180006e10`，分别写 `0x25/0x26` 与 `0x23/0x24` 这两对 begin/end marker
- 继续下探 `FTMLCore.dll` 中 `ClassInfo::ClassInfo(ClassData const&)` 可确认：
  - `classInfo + 0x52` 直接来自 `ClassData + 0x1A`
  - `classInfo + 0x60` / `classInfo + 0x68` 分别来自 `ClassData + 0x28` / `ClassData + 0x30`
  - `Archive::getObject/putObject` 的分派规则是：
    - `classInfo->usesVirtualIO != 0` 时走对象虚 `get/put`
    - `classInfo->usesVirtualIO == 0` 时才走 `ClassInfo` 上登记的静态 `get/put` 回调
- `XMSE::Configuration` 的静态 `ClassData@0x18008C1A0` 当前已可直接解出：
  - `class_id = 0xDCCE0200`
  - `parent_class_id = 0xDCCC9F00`
  - `usesVirtualIO = 1`
  - `static_get = nullptr`
  - `static_put = nullptr`
- `XMSE::ConfigurationSet` 的静态 `ClassData@0x18008C200` 也同样表现为：
  - `class_id = 0xDCCD2C00`
  - `parent_class_id = 0xDCCC9F00`
  - `usesVirtualIO = 1`
  - `static_get = nullptr`
  - `static_put = nullptr`
- 因而当前新增的约束结论是：
  - `xmseTail` 缺失暂时不能再归因于“`RawDataSet::put` / `ConfigurationSet::put` 把 XMSE 配置在容器层降级或切片”
  - 同时也不能再归因于“`Archive::putObject` 对 `XMSE::Configuration` 命中了 `ClassInfo` 静态写路径”
  - 在当前 DLL 语义下，`XMSE::Configuration` / `XMSE::ConfigurationSet` 的对象体 IO 都应走虚 `get/put`
  - 若样本里存在“旧对象体仍在、但后续索引/引用已更新”的形态，那么它现在更像是 pointer/tag 复用带来的对象体保留，而不是对象实体被重新按基类降级重写
  - 因此剩余更可疑的分支进一步收敛为：
    - `test.dat` 的生成/保存链并不完全对应当前分析到的 DLL 版本与保存分支
    - 或保存时对象尚未处于当前读侧看到的完整 `XMSE::Configuration` 状态
  - 并且在“当前 DLL 直接参与生成这些默认配置并落盘”的假设下，样本里理论上至少应出现构造默认值对应的 `xmseTail` 覆盖面；真实 `test.dat` 未出现该现象，因而版本/分支差异的解释权重进一步上升
  - 同时，`XMSE::Configuration::get` 对尾字段缺失采取“尝试读取但不因失败而报错”的兼容策略，这与“同一文件里可能混有不同版本先后写入的配置体”是相容的
- 新增与 `FTMLUtil.dll` 的交叉验证：
  - `FTML::NamedValueTable::get()` 本体只读取基类 `Countable` 部分
  - 真正消费 `Named / Unnamed / AddTables` 的是 `FTML::NamedValueSet::get()`
  - 因此真实样本 `ConfigInfo@0x2A5CE` 当前只显示 `FTML::NamedValueTable<=FTML::NamedValueTable:Exact` 且没有 entry 列表，是符合逆向证据的

### 4.5 `FTML::XMSE::RawDataSet`

当前已证实的槽位级结构：

- 基类前缀：
  - `SampleID` 候选
  - `SysInfo`
- 派生层：
  - `Version = 2`，样本原始 item type 为 `Int8`，调用侧期望读取类型为 `Int16`
  - `DCRecipe`
  - `DPRecipe`
  - `Rotation = 1.570796`
  - `AnalyzerRef` 缺失
  - `AnalyzerSample = -0.436332`
  - `Tilt flag = true`
  - `Tilt`
  - `PixShiftRef` 缺失
  - `PixShiftSample = -0.001`
  - `FilterRef = <empty>`
  - `FilterSample = <empty>`
  - `Ref -> RawData body@0x80E`
  - `Dark -> RawData body@0x88E`
  - `Sample -> RawData body@0xCCE`
  - `ConfigSet -> ConfigurationSet body@0x2A53C`

注意：

- 上述中后缀槽位现已下沉到正式 `RawDataSet::get()` 最小 reader，并已在真实 `test.dat` 上使 `rawdataset_top_level` 变为 `materialized=true`
- `SampleID / SysInfo` 当前已不再只是证据槽位：
  - `SampleID` 已正式读出，真实 `node[3]` 当前稳定为 `<empty>`
  - `SysInfo` 已正式读成 `NamedValueSet<=NamedValueTable` 兼容变体
  - `SysInfo` 当前已稳定输出最小键值摘要：`structuredScalars{name,valueTag,encoding,rawValueType,raw value}`
  - 当前代码已按放宽后的 `node[3]` 收口标准，为 `structuredScalars` 直接补充 `semanticStatus / semanticMeaning / semanticNote`
  - 其中 `FieldX / FieldY` 与一批稳定元字段会直接提升为 `FullyRestored`
  - `AOI / ND Filter / ApplyIDN / Alignment Mode / Cell_X / GroupID ...` 会标为 `ClosedWithCaveat`
  - `Acquisition Order` 会直接标为 `Opaque`
  - 当前 CLI 对 `rawdataset_front` 的 `SysInfo` 输出也已改为结构化块，而非继续堆叠在单行摘要中
  - `SysInfo` 字段提升规则已整理为单独规则表，后续新增字段只需扩表，不必继续改 reader 主流程
  - `FieldX / FieldY` 已有 `FTMLBase.dll` 直接静态消费者：`FTML::ResultType::DFFSiteInfo::setFrom(const NamedValueTable&)` 会显式读取 `XPosition`、`YPosition`、`FieldX`、`FieldY`
  - 因而 `FieldX / FieldY` 可以提升为正式 site/field 坐标元信息，而不是仅凭样本键名猜测
  - `DFFSiteInfo::matches(...)` 的标准匹配逻辑围绕 `XPosition / YPosition / WaferAngle / WaferID / LotID` 展开，进一步说明 `FTMLBase` 的标准 site 模型并未显式出现 `Cell` 层坐标
  - 相比之下，`Cell_X / Cell_Y` 与 `Acquisition Order` 当前尚未在 `FTMLBase.dll` 的标准 `NamedValueTable` 归一化路径中看到直接消费点
  - 对 `Acquisition Order = -35718272` 的立即数做了静态复核：其 32-bit 原始字节形态为 `80 FB DE FD`，但在 IDA 的 immediate 搜索中没有命中，说明它不像 DLL 中的固定常量或枚举值
  - 同时在 `FTMLBase.dll` 的字符串面未见 `Acquisition` 明文键名，仅能看到与结果参数排序/滤波相关的通用 `Order` 字样；因此当前更合理的定性是：`Acquisition Order` 在 `node[3]` 中属于样本透传的 opaque `int32` 元数据，而非已坐实业务语义的标准 FTML 字段
  - 对 `GroupID / Group Item Count / Group ItemID` 做了 `FTMLBase.dll / FTMLCore.dll / FTMLSysXMSE.dll / FTMLUtil.dll` 的字符串面复核，当前均未见明文键名或直接静态消费者
  - 因而这组三元字段目前只能保守视为样本中的分组元数据键，尚不能提升成 FTML/XMSE 已坐实的标准业务字段
- 当前仍保持保守边界的主要是：
  - `SampleID / SysInfo` 的最终业务语义命名
  - `SysInfo` 动态实际类型为何与期望基型存在兼容差异
  - `Tilt` 内部业务语义

### 4.6 `FTML::XMSE::ConfigurationSet`

当前已证实的样本结构链：

- `systemID`
- `legacyRoot`
- `legacyRoot -> FTML::NamedValueSet`
- `NamedValueSet` 中出现 `XMSE::Configuration`

## 5. 样本实证

真实样本 `test.dat` 当前已经稳定验证：

- `DCRecipe x1`
- `DPRecipe x1`
- `RawData x3`
- `ConfigurationSet x1`
- 顶层 `RawDataSet x1`

并且已经能在统一报告里稳定看到：

- `DPRecipe configApp=CD@Default`
- `ConfigurationSet systemID=XMSErprc65`
- `RawDataSet` 顶层 trace

## 6. 对当前代码的影响

当前工程中与 `FTMLSysXMSE` 结论直接对应的实现包括：

- `src/XMSERawDataSet.cpp`
- `include/domain/XMSERawDataSet.h`
- `src/main.cpp`
- `tests/UnitTest.cpp`

## 7. 未确认边界

以下内容当前仍不应写成确定结构：

- `RawDataSet.SampleID / SysInfo` 的正式业务语义
- `Tilt` 内部字段的业务命名
- `ConfigurationSet::get` 的完整导出实现
- `SysInfo` 动态实际类型为何与期望基型存在兼容差异

当前阶段的原则是：只把“导出函数 + 样本 trace + 当前 reader 验证”三方一致的内容提升为正式结构。
