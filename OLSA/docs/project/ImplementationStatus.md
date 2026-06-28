# 项目实施文档

## 1. 当前目标

当前项目的主目标是：

- 基于真实 DLL 逆向结果，恢复 OLSA/FTML 归档读取链
- 对 `test.dat` 输出完整、正确、有业务意义的统一报告
- 在此基础上逐步把确认过的数据结构转入正式 reader
- 当前工程只做解析读取，不实现任何写文件/回写 archive 的功能

## 2. 已完成目标

### 2.1 类型系统与识别

已完成：

- 类表 / 模块表读取
- `SmartPointer::extract`
- recovered lineage 回退
- 兼容判定与单测

### 2.2 XMSE 定向 reader

已完成：

- `DCRecipe`
- `DPRecipe`
- `RawData`
- `ConfigurationSet`
- `Configuration`
- `RawDataSet` 顶层识别与证据 trace
- `RawDataSet` 最小正式 reader（已覆盖真实样本匿名布局）
- `Configuration` 匿名时间戳前缀的结构化摘要输出

### 2.3 数组物化

已完成：

- formatted `UInt32` payload 解包
- `Array2D<unsigned>` / `Array<unsigned>` 的最小真实物化
- `RawData.sig / enc1 / enc2 / clk / bm` 的样本级落地

### 2.4 样本验证

已完成：

- 用真实 `test.dat` 验证 `DCRecipe / DPRecipe / RawData / ConfigurationSet`
- 证明 `node[3]` 顶层存在 `RawDataSet`
- 让 `rawdataset_top_level` 在真实样本上从 `materialized=false` 变成 `materialized=true`
- 证明顶层 `RawDataSet` 内部确实包含：
  - `DCRecipe`
  - `DPRecipe`
  - `RawData x3`
  - `ConfigurationSet`

### 2.5 统一报告

已完成：

- 统一 capability boundary 输出
- 动态对象统一摘要
- `RawData` 业务角色统一映射
- 顶层 `RawDataSet` 证据 trace
- 已证实槽位摘要 `rawdataset_proven_slots`
- 正式 reader 输出 `rawdataset_filters / rawdataset_slots`

说明：

- 当前 capability boundary 中 `handled_counts.RawDataSet` 仍可能显示为 `0`
- 这是因为该计数口径只统计统一动态扫描结果，不包含 `read_first_section()` 预读后再单独回收的顶层 `RawDataSet`
- 当前应以 `rawdataset_top_level materialized=true` 与 `rawdataset_filters / rawdataset_slots` 为顶层 reader 成功的判据

## 3. 未完成目标

### 3.1 顶层 `RawDataSet` 深化

尚未完成：

- `SampleID / SysInfo` 的最终业务语义命名
- `SysInfo` 动态实际类型为何与期望基型存在兼容差异
- 匿名标量段的业务命名继续收敛，但不提前命名

### 3.2 `ConfigurationSet` 深化

尚未完成：

- `TextID::get` 最终业务语义恢复
- `NamedValueList` 等通用容器更深层物化
- `ConfigurationSet` 全路径 reader 收敛

当前已新增的可靠结果：

- `node[3]` 顶层 `RawDataSet` 的前部对象骨架现已正式进入 reader，不再只是证据槽位：
  - `SampleID` 已正式读出，当前真实样本值稳定为 `<empty>`
  - `SysInfo` 已正式读成 `NamedValueSet<=NamedValueTable` 兼容变体
  - `SysInfo` 内部当前已稳定输出 `structuredScalars{name,valueTag,encoding,rawValueType,raw value}`
- 当前真实 `test.dat` 中 `SysInfo` 已稳定命中两类只读编码归档：
  - `valueTag=0x4 -> encoding=TextPayload`
  - `valueTag=0x80 -> encoding=NumericScalar`
- `node[3].Tilt` 已完成最小语义闭合：
  - 真实落盘形态为匿名三元组 `[theta: float, phi: float, unitsRaw: uint32]`
  - 当前样本稳定读出 `theta=0.0, phi=0.0, unitsRaw=0x13148DA`
  - 结合 `FTML::Tilt::Tilt(float,float)`、`ResultType::Tilt` 初始化与 `UnitsConvert` 链，现已坐实
    - `0x13148DA -> radians`
    - `0x1314924 -> arcseconds`
- 因而对 `node[3]` 当前阶段的判断已可收紧为：
  - 不能宣称“node[3] 每一字节业务语义都已完全恢复”
  - 但按现阶段只读解析目标，`node[3]` 的前部骨架、`SysInfo`、`Tilt`、三条 `RawData`、`ConfigurationSet` 与后半段 section/字典关系都已稳定闭合，可进入本阶段收尾
- 若放宽标准为“结构自洽 + 样本稳定 + 静态结果不冲突”，则当前 `node[3]` 可按三栏口径收口：
  - `完全还原`：
    - `RawDataSet` 顶层骨架与 `node[3]` 物理拓扑
    - `SampleID`
    - `Tilt`（匿名三元组 `[theta: float, phi: float, unitsRaw: uint32]`，且 `0x13148DA -> radians`、`0x1314924 -> arcseconds`）
    - `SysInfo` 的读取结构与编码层（`TextPayload` / `NumericScalar`）
    - `SysInfo` 中大部分直观元字段：`LotID / RunID / WaferSlotNumber / WaferOrdinalNumber / SiteID / TestID / TestLabel / RecipeName / ToolSerialNumber / SoftwareVersion / FTML Version / WaferSize / ToolType / ToolTitle / SubSystemName / SubSystemIndex / SiteSerialNumber / WaferID / Data Acquisition Start Time / Data Acquisition End Time`
    - `FieldX / FieldY`
    - `Ref / Dark / Sample` 的角色、位置与主结构
    - `ConfigurationSet` 的主链结构（`legacyRoot / TF#1 / @Default / XMSE::Configuration`）
    - 上述字段当前已在代码中直接标为 `semanticStatus=FullyRestored`，并通过 `semanticMeaning` 对外输出
  - `闭合但未完全语义化`：
    - `AOI`
    - `ND Filter / UV 400 Filter / UV 320 Filter / UV 240 Filter / Yellow Filter`
    - `Alignment Mode`
    - `SE Pixel Binning Mode / SE Pixel Binning Size`
    - `ApplyIDN / ApplyDC_RPRC / FMA / 9K_Mode`
    - `RPRC Harmonics / RPRC Mueller Matrix`
    - `StaticRepeatCount`
    - `SubSystemTypeAcushape / UVSE / Crossover / IRSE_Module`
    - `Cell_X / Cell_Y`
    - `GroupID / Group Item Count / Group ItemID`
    - 上述字段当前已在代码中统一标为 `semanticStatus=ClosedWithCaveat`，并通过 `semanticNote` 明示缺陷边界
  - `仍待后续`：
    - `Acquisition Order` 的最终业务语义
    - 各 calibration object 内部全部子字段的最终业务命名
    - `RawData` 内所有辅助数组/缓存值的最终算法语义
    - 其中 `Acquisition Order` 当前已在代码中直接标为 `semanticStatus=Opaque`
- `Configuration` 在真实样本中可稳定输出 `System / Comment / TimeStamp(year/month/day/hour/minute/subMinuteRaw) / BaseName / ConfigInfo`
- 真实 `test.dat` 当前可稳定观测到 `@Default` 配置时间戳前缀为 `2018-04-12 17:17 raw=26743`
- `TF#1 -> FTML::NamedValueList` 当前可稳定输出最小原始摘要 `itemCount=2, preview=[value=int8(1), group[3]=[text=@Default, value=uint8(32), pointer=FTML::XMSE::Configuration]]`
- 当前 reader 已正式输出最小结构化结果：`entriesCountRaw=1`，`structuredEntries[0]={name=@Default, valueTag=0x20, pointerType=FTML::XMSE::Configuration}`
- 当前 reader 已进一步把 `StructuredEntry` 中命中的 `XMSE::Configuration` 指针下沉为正式 `ConfigurationSummary`
- `legacyRoot -> NamedValueSet` 内部 entry 已开始正式保留 `object_id / body_offset / expected / compatibility`
- `ConfigurationSummary::configInfo` 已开始保留 `object_id / body_offset`，当前真实样本命中 `body_offset` 且 `object_id=0`
- 当前 reader 已支持把 `configInfo` 命中的 `NamedValueTable` 条目下沉为 `configInfoSummary.entries[]`，并有单测覆盖；真实 `test.dat` 目前尚未出现该层样本输出
- 已通过 `FTMLUtil.dll` 逆向确认：`NamedValueTable::get()` 本体不读取 entry 列表，只有 `NamedValueSet::get()` 才继续消费 `Named / Unnamed / AddTables`
- 因此真实 `test.dat` 中 `ConfigInfo@0x2A5CE` 仅显示 `NamedValueTable` 基型、未出现 `configInfoEntries[...]`，目前判断为样本真实形态，而不是 reader 缺口
- 已通过 `FTMLSysXMSE.dll` 导出实现确认 `XMSE::Configuration` 在 `DC::Configuration` 之后还会继续读取 `SumsPerCycle / TimingMode / Saturation / TurnsPerCycle[2] / NumPixel`
- 当前工程已支持这组派生尾字段并有单测覆盖
- 真实 `test.dat` 回归已确认：当前 reader 在确认命中 `Extracted + BeginGroup` 固定边界后，10 个 `XMSE::Configuration` 实例均可稳定命中 `xmseTail`
- 已通过当前真实输出确认 `test.dat` 中至少有两种 `XMSE::Configuration` 覆盖形态：
  - `body=0x2A575 -> coverage=[system,comment,timestamp,configInfo,xmseTail]`
  - `body=0x86E76 -> coverage=[system,comment,timestamp,base,configInfo,xmseTail]`
- 已结合用户给出的 10 次 `XMSE::Configuration::get` 进入偏移，将当前可见实例进一步映射为：
  - `legacyRoot -> @Default (0x2A575)`
  - `TF#1 / CD#2 / TurboFilm#3 / FoG#4 / FDC#5 / TurboShape#6 / IDO#7 / TrueShape#8 / SPA#9 -> @Default`
  - 对应 body 分别为 `0x86E76 / 0x86F37 / 0x86FFF / 0x870AF / 0x8715F / 0x87216 / 0x872C6 / 0x8737C / 0x8742C`
- 这 9 个 `NamedValueList` 生成配置当前都稳定命中 `coverage=[system,comment,timestamp,base,configInfo,xmseTail]`
- 样本级已证实的尾字段分布现为：

  - `legacyRoot.@Default` 命中 `160, 3, 64000, [5,1], 1024`
  - `TF#1 / CD#2` 命中 `320, 3, 64000, [5,1], 1024`
  - `TurboFilm#3 / FoG#4 / FDC#5 / TurboShape#6 / IDO#7 / TrueShape#8 / SPA#9` 命中 `160, 3, 64000, [5,1], 1024`
- `Calibrations` group 的样本级差异现也已坐实：
  - `legacyRoot.@Default@0x2A5CE` 为非空 group，内部实物字节与 `Settings + Calibration*` 循环对齐
  - `0x86E76..0x8742C` 这 9 个生成配置为 `BeginGroup EndGroup` 的空 group
  - 当前 reader 已把这条差异最小化物化为：
    - `legacyRoot.@Default` -> `dc(extracted=322(XMSErprc65) calibrations=14)`
    - `0x86E76..0x8742C` -> `dc(extracted=322(XMSErprc65) calibrations=0)`
  - `legacyRoot` 的 14 条 calibration pointer 动态类型全集现已稳定显示
  - 这 14 条在当前样本中全部稳定携带 `settingsRaw=322(XMSErprc65)`
  - reader/CLI 已把 `Extracted` 与每条 calibration entry 的 `settingsRaw` 同步显示为标准配置名，不再停留在匿名整数
- 已逆向确认 `XMSE::Configuration::put` 无条件写出 6 个派生尾字段，且 `XMSE::Configuration` 拷贝构造会完整复制这些字段
- 已逆向确认 `SubSystem::setDefaultConfigSet(uint32_t, bool)` 会生成 `FTMLSysXMSE generated` 配置链，但在当前已坐实的代码路径中只做基类层初始化，没有看到 `setSumsPerCycle / setTimingMode / setSaturation / setTurnsPerCycle / setNumPixel`
- `XMSE::Configuration(uint32_t)` 默认构造本身会给这些派生字段赋非零默认值，因此 `test.dat` 中 `xmseTail` 缺失当前更像是保存/归档路径省略，而不是对象天然没有这些值
- 已继续把保存链收窄到容器层以下：
  - `XMSE::RawDataSet::put` 在 `ConfigSet` 槽位仍以 `expected_class_id = 0xDCCD2C00` 写入 `XMSE::ConfigurationSet`
  - `XMSE::ConfigurationSet` 当前未见 `put` override，集合体序列化沿用 `FTMLBase.dll` 中的 `DC::ConfigurationSet::put`
  - `DC::ConfigurationSet::set(...)` 仅把配置条目包成 `NVT::Value::setPtr(const Countable*)`
  - `FTMLCore.dll` 中 `SmartPointer::insert` 自身只转发到 `Archive::putPointer(...)`

### `node[3]` Final Closure

- 当前 `node[3]` 已按本阶段目标收口，不再把“继续追更深业务语义”视为收口前置条件。
- 收口判断采用两层标准：
  - 严格标准：不宣称“每一字节业务语义都已完全恢复”，但 `node[3]` 的前部骨架、`SysInfo`、`Tilt`、三条 `RawData`、`ConfigurationSet` 与后半段 section/字典关系都已稳定闭合。
  - 放宽标准：只要结构自洽、样本稳定、静态结果不冲突，就允许进入 `FullyRestored / ClosedWithCaveat / Opaque` 三栏口径。
- 当前代码、输出、测试与文档已统一采用这套三栏口径：
  - `FullyRestored`：已可当正式还原字段使用。
  - `ClosedWithCaveat`：字段方向与样本事实已稳定，但仍保留明确缺陷备注。
  - `Opaque`：仅保留类型、值与只读事实，不再继续强行解释业务语义。
- 为避免 `node[3]` 输出继续退化成超长拼接摘要，当前 CLI 已把 `rawdataset_front` 适配为结构化块输出：
  - `sampleID / tilt / sysInfo` 分行显示
  - `sysInfo` 内的结构化 scalar 再按 `FullyRestored / ClosedWithCaveat / Opaque / Unclassified` 分组展示
  - 每个字段同步显示 `meaning` 与必要的 `note`
- 对应实现也已整理成可扩展结构：
  - `node[3].SysInfo` 语义提升规则改为单独规则表驱动，而不是继续在 reader 中堆叠条件分支
  - 后续若新增可提升字段，只需补规则表和必要注释，不必再改 reader 主流程
- 因而 `node[3]` 现在没有“尚未处理的悬空尾巴”：
  - 能正式提升的字段，已落进 reader 和 CLI 输出。
  - 不能完全坐实但仍有稳定方向的字段，已落成 `ClosedWithCaveat`。
  - 当前无法进一步语义化的字段，已落成 `Opaque`。
- 从项目阶段管理角度，后续若继续逆向：
  - 不再计入“`node[3]` 收口”范围。
  - 应视为新增证据驱动下的语义增强，或转入其它 node / 其它模块。
  - `Archive::putPointer(...)` 会先用 `TaggedObjectList::findTag(...)` 判断当前对象是否已有 pointer tag
  - 只有首次出现的对象才会 `add(...)` 后继续进入公共对象写出 helper；已有 tag 的对象只复用 pointer/tag，不重复写对象体
  - 公共对象写出 helper 再分派到对象虚 `put` 或 `ClassInfo` 静态 `put` 回调
- 已继续把保存链收窄到 `ClassInfo` 分派层：
  - `FTMLCore.dll` 中 `ClassInfo::ClassInfo(ClassData const&)` 会把 `ClassData+0x1A` 复制到 `classInfo+0x52`
  - `Archive::getObject/putObject` 在 `classInfo+0x52 != 0` 时走对象虚 `get/put`
  - `XMSE::Configuration` 的静态 `ClassData@0x18008C1A0` 当前可直接解出 `usesVirtualIO=1, static_get=null, static_put=null`
  - `XMSE::ConfigurationSet` 的静态 `ClassData@0x18008C200` 也同样表现为 `usesVirtualIO=1, static_get=null, static_put=null`
- 已继续把默认配置生成链收窄到对象状态层：
  - `setDefaultConfigSet(uint32_t, bool)` 的完整后半段显示，`@Default` 先入集合，后续 `TF / CD / <Filter> / <Filter>.TF / <Filter>.CD` 全部来自 `SmartPointer::makeCopy()`
  - 每个派生配置在入集合前仅执行 `DC::Configuration::setBaseName` 与 `DC::Configuration::removeCals`
  - 因而当前已可直接解释真实样本里的 `Calibrations` group 分歧：
    - `legacyRoot.@Default` 保留创建时已有的 calibration 集合，因此稳定表现为 `calibrations=14`
    - 9 个生成配置在复制后入集合前显式执行 `removeCals()`，因此稳定表现为 `calibrations=0`
  - `XMSE::Configuration::setNumPixel / setSaturation / setSumsPerCycle / setTimingMode / setTurnsPerCycle` 当前均未发现直接代码 xref
- 已补上旧体兼容读证据：
  - `XMSE::Configuration::get` 会继续尝试读取 `xmseTail` 6 字段
  - 但函数体并不检查这些 `Archive::get(...)` 的返回值
  - 因而旧版本落盘、缺失 `xmseTail` 的 `Configuration` 仍可被当前 DLL 正常读出
- 已补上文件容器层的正向信号：
  - `test.dat` 文件头当前可直接解出 `node_count=40, valid_node_count=14`
  - 这至少证明该文件格式支持“预留/扩展 node 槽位后继续写入”
  - 但由于 `14` 之后的 node 表项当前为零，暂时只能把它视为“支持多轮写入”的格式证据，不能直接当作“旧版本 node 残留”的已证实事实
- 已补上当前字典层约束：
  - `test.dat` 当前统一解析出的类/模块字典视图为 `module_version=7.00.14`
  - 因而若文件里混有更老年代写入的对象体，更可能是“旧对象体保留，而当前索引/字典层已被较新版本改写”
- 已补上同 node 的 section 物理布局证据：
  - `BinaryArchive::read_first_section()` 与真实字节对照后，`node[3]` 头部的 `FTML::BinaryArchive` 后紧跟 `Int32(section_offset=0x874AA)`
  - 当前活动 class/module list 的入口位于同一 `node[3]` 的 `0x874AA`
  - `ConfigurationSet` 下 10 个 `XMSE::Configuration` 体全部位于该偏移之前
  - 最后一个 `SPA#9.@Default` body=`0x8742C` 距该 section 仅 `0x7E`
  - 其 `ConfigInfo` body=`0x8748E` 距该 section 仅 `0x1C`
- 已补上 `BinaryArchive` 写侧组织证据：
  - `BinaryArchive::doFinalize(...)` 会先保存当前写游标，再把游标清零回填 node 头的 `FTML::BinaryArchive` 对象与 `FinalIndex`
  - 之后恢复写状态，并从该 `FinalIndex` 位置继续执行 `putClassAndModuleLists(...)`
  - 因而 node 的后半段 section 当前可直接理解为“当前活动 class/module list 字典段”
- 已补上读侧对称证据：
  - `Archive::reread(...)` 会 finalize 后清空 `TaggedObjectList`，再进入二进制 reset helper
  - 该 helper 会先读取 node 头里的 `FinalIndex`
  - 临时跳到 `FinalIndex` 预读 class/module list
  - 再回到原游标继续读取 node 前部对象体
- 已补上 begin/end marker 的字节级证据：
  - `BinaryArchive::doPut(...)` 中 `0x23/0x25` 的第 2 字节是 nibble-packed header：低 4 bit 表示 class-id 字节数，高 4 bit 表示 pointer-tag 字节数
  - 读侧 `sub_180013730(...)` 已对称坐实这一点
  - 因而 `node[3]` 头部 `23 04 50 9F D6 04` 可直接解成 `BeginObject + 4-byte class id(0x04D69F50)`
  - 紧随其后的 `25 84 00 19 28 DE 30 C3 7E 49 00 00 00 00` 可直接解成 `BeginPointer + 4-byte class id + 8-byte tag`
  - 末段 `... 26 22 26 22 26 26 26 24 21 ...` 则与 `EndPointer / EndObject / 下一个 section BeginGroup` 的闭合顺序对上
- 已补上顶层前缀的绝对偏移链：
  - `0x6E3` = 顶层 `RawDataSet` pointer slot
  - `0x6F1` = `RawDataSet` body 起点
  - `0x6F3` = `SysInfo` pointer slot
  - `0x701` = `SysInfo` body 起点
  - `0xDD8` = `SysInfo` 的真实 `EndPointer`
  - `0xDD9` = `Version` 的原始 `Int8(2)`
  - `0xDDB` = `DCRecipe` pointer slot
- 已补上“假 marker”约束：
  - `SysInfo` body 内部按裸字节值搜索会出现 `0x729=0x26`、`0xA9E=0x25` 之类的假命中
  - 当前只能结合 top-level trace、`Version@0xDD9` 与结构化 skip 结果，把 `0xDD8` 稳定认作真实 `EndPointer`
- 已补上偏移口径与固定 trailer 约束：
  - 上述 `body_offset / section_offset` 当前都应理解为 `node[3]` 内偏移
  - 对应到文件绝对位置时，需要统一加上 `node[3].offset = 0x6D8`
  - `SPA#9.@Default.ConfigInfo@0x8748E` 与 section@`0x874AA` 之间存在一段稳定写死的 `0x1C` trailer
  - 该 trailer 在 `test.dat` 中不是漂移边界，而是固定存在的文件内结构
- 已补上 `DC::Configuration` 基类尾部的新逆向约束：
  - `FTMLBase.dll` 中 `DC::Configuration::get/put` 已坐实在 `ConfigInfo` 之后还会继续处理 `Extracted / Calibrations / Settings`
  - 因而当前 `ConfigInfo` 之后出现的固定 trailer，不应直接整体归类为 `XMSE::Configuration` 派生尾字段
  - 对 `SPA#9` 样本而言，真实文件 `0x87B81` 已命中 `EndObject`，`0x87B82` 才进入当前活动 section
- 因而当前新增结论是：
  - 当前主要矛盾已不再是“`xmseTail` 为什么没有在真实样本中读出来”
  - 而是“`ConfigInfo` 之后固定 trailer 的哪些字节属于 `DC::Configuration` 基类尾部，哪些字节属于 `XMSE` 派生尾字段”这一语义切分
  - 当前 reader 已经把这段边界最小化落地为：`Extracted + Calibrations group + compact xmseTail`
  - 并新增一条已落地的安全约束：如果只看到一个紧凑整型而后面没有 `BeginGroup`，则整段回退，不把它误认成 `DC::Configuration::Extracted`
  - “不同版本先后写入”仍保留为解释对象年代/section 覆写关系的候选，而不再作为解释 `xmseTail` 缺失的主证据
- 新增反编译坐实：
  - 顶层 `int8(1)` 与 `NamedValueList::get` 的 `Entries=1` 一致
  - `group[3]` 对应单个 `Entry`
  - `uint8(32)` 对应 `NVT::Value` 的内部类型标签 `0x20`
  - `0x20` 与 `Value::setPtr(const Countable*)` 对齐，后接 `XMSE::Configuration` 指针载荷
  - 调用点交叉验证显示：只读上下文包装复用 `0x20`，而创建可变 `NamedValueList/NamedValueSet` 等对象时走 `0x10`

### 3.3 更上层业务链

尚未完成：

- 0x88E 的 binned 派生态生成链
- `RawData::bin` 上游 helper 追踪
- `SubSystem::simulate` 相关 helper 逆向

## 4. 当前阶段判断

当前项目处于：

- **阶段 3：证据固化与顶层结构收敛阶段**

这一定义现在可以进一步明确成“四层系统视图 + 五阶段分析法”：

- 四层系统视图：
  - `FTMLCore.dll`：归档协议、字典、动态类型与压缩解包
  - `FTMLUtil.dll`：名字语义与通用容器
  - `FTMLBase.dll`：通用业务基类与公共对象前缀
  - `FTMLSysXMSE.dll`：XMSE 业务派生对象与保存链
- 五阶段分析法：
  - 阶段一看协议层：先排除 `TypeCode / offset / class-list / module-list / SmartPointer` 问题
  - 阶段二看容器层：先确认 `NameString / NamedValue* / ValueTag` 的承载语义
  - 阶段三看业务基类：先确认字节属于 `FTMLBase` 还是 `XMSE` 派生部分
  - 阶段四看业务派生：只在前三层稳定后再提升 XMSE 业务字段
  - 阶段五看保存链与年代解释：最后才解释“为什么文件会长成这样”，但不进入写入实现

这个阶段的特征是：

- 核心动态对象已经可识别
- 多数关键 reader 已能最小物化
- 顶层 `RawDataSet` 已完成最小正式物化
- 但前缀对象与部分匿名标量的业务语义仍在证据收敛中

当前不是“从零探索阶段”，也不是“全面收尾阶段”。

## 5. 当前最重要的阶段目标

当前最重要的阶段目标只有一个：

- **把 `ConfigInfo` 之后固定 trailer 的语义切分继续坐实到字段级，并同步收敛保存链对“同一文件被不同版本先后写入”的解释边界**

结合上面的四层系统视图，这个目标当前对应的是：

- 阶段三：
  - 继续确认 `FTMLBase.dll` 中 `DC::Configuration` 的 `Extracted / Calibrations / Settings` 边界
- 阶段四：
  - 继续确认 `FTMLSysXMSE.dll` 中 `xmseTail` 与默认配置链的业务语义
- 阶段五：
  - 继续确认 `put` 保存链、同 node 内对象体/section 叠加与“不同版本先后写入”的解释是否闭合，但这些结论只回收给读取边界与样本解释

## 6. 当前最重要的下一步动作

最重要的下一步动作是：

- **继续把 `DC::Configuration::Settings` 的真实读写边界补齐，确认 `legacyRoot.@Default` 非空 group 内每一项的最小字段语义**
- **继续围绕 `legacyRoot.@Default` 与 9 个生成配置的异构 `Calibrations` group 做逐项对照，确认是否还存在未提升的基类或派生语义**
- **继续围绕“同一文件被不同版本先后写入”与“同一 node 内对象体/section 代际叠加”的可能性收集直接证据，但不再把它当作 `xmseTail` 缺失解释**
- **继续沿 `setConfigSetXMSE -> RawDataSet::setConfigSet -> RawDataSet::put` 收集“新 `ConfigSet` 引用如何挂回旧 `RawDataSet`”的直接证据，收窄 node 内代际叠加模型**

之所以这是当前最重要动作，是因为：

- `ConfigurationSet / Configuration` 的 reader 最小闭环已经具备，当前主要矛盾不再是“读不到”，而是“非空 `Calibrations` group 的内部语义如何继续坐实”
- 当前“非空 `Calibrations` group`”已经从纯 trace 提升为正式只读摘要，下一步主要矛盾收敛成：
  - 这 14 条 calibration entry 的动态类型全集是什么
  - `settingsRaw=322` 在 `HWSetupDef` 语义上还能否再继续收窄
- 其中前一个问题本轮已完成，当前真正剩下的收敛目标已进一步缩小为：
  - `settingsRaw=322(XMSErprc65)` 的更深静态语义
  - 是否需要把 14 条 calibration 动态类型全集单独整理成稳定输出表
- `put` 本体、copy ctor、default ctor、`setDefaultConfigSet()`、`RawDataSet::put`、`ConfigurationSet::put`、`Archive::putObject`、`ClassInfo` 分派这几层证据已经连起来了
- `setConfigSetXMSE(...)` 与 `RawDataSet::setConfigSet(...)` 的新证据又把“挂接方式”进一步收窄为：
  - 基类签名的 `SubSystem::setConfigSet(FTML::P<DC::ConfigurationSet>)` 会先断言传入对象 `isA(XMSE::ConfigurationSet)`
  - 然后用 `canCast(..., 0xDCCD2C00)` 收窄回 `XMSE::ConfigurationSet`，再转调 `setConfigSetXMSE(...)`
  - 先按 `TextID` 找到目标 `RawDataSet`
  - 若新集合非空，还会先刷新 `SubSystem` 当前选中条目的内部指针对
  - 再直接替换 `this+0x138` 的 `ConfigurationSet` 指针
  - 并在替换前用当前 app 名称到新集合里 `find(...)`，失败时走 `failBadApp(...)`
  - 并清空 `this+0x148` 相关缓存后触发刷新
- 因而当前更符合“新 `ConfigSet` 引用挂回既有 `RawDataSet`”而非“回写重构旧 `Configuration` 对象体”的模型
- 与 `RawDataSet::put(...)` 的新反编译交叉后，这个模型还能再收紧一层：
  - 保存 `ConfigSet` 时直接执行 `SmartPointer::insert(*(this+0x138), 0xDCCD2C00, operator()(arg2, "ConfigSet"))`
  - 因而落盘来源就是当前 `0x138` 槽里的活动 `ConfigurationSet` 指针，而不是回头遍历旧 `Configuration` body
- 再与 `FTMLCore::Archive::putPointer(...)` 的 tag 复用逻辑交叉后，可继续补一条更强约束：
  - 同一 node 内完全可能保留较早写出的对象体
  - 后续新增/重写的只是指向该对象体或替代对象体的 pointer/tag/index 结构
- 再与最新坐实的顶层绝对偏移链交叉后，这条 overlay 模型还能再闭一层：
  - `0x6E3=RawDataSet slot -> 0x6F1=RawDataSet body -> 0x6F3=SysInfo slot -> 0x701=SysInfo body -> 0xDD8=EndPointer -> 0xDD9=Version -> 0xDDB=DCRecipe slot`
  - 这说明 `RawDataSet -> SysInfo -> Version -> DCRecipe` 这一串当前确实稳定存在于 node[3] 的前部对象体区域
  - 而同一 node 的当前活动 class/module section 则位于 `0x874AA(node 内) / 0x87B82(file abs)` 的后半段
  - 因而当前更具体的解释已经收窄为：
    - 前部对象体链保留旧的持久内容
    - 后续运行时变化主要体现在 `ConfigSet` 引用、派生缓存失效重建，以及 node 头/后部 section 的更新
- 已继续把 `setDCRecipe/setDPRecipe` 并入同一模型：
  - `RawDataSet::change(P<Countable>)` 就是此前匿名的 `0x180015650`
  - 它会按动态类型分派到 `setConfigSet/setDCRecipe/setDPRecipe`，并在任一路成功后统一清空 `this+0x158`
  - `setDCRecipe(...)` 直接替换 `this+0x28`，清空 `this+0x140`
  - `setDPRecipe(...)` 直接替换 `this+0x30`；若 `configApp` 变化，还会额外清空 `this+0x148`，并同样清空 `this+0x140`
  - `RawDataSet::put(...)` 又直接从 `this+0x28/0x30` 以 `expected_class_id = 0xDCF62C00 / 0xDCF65100` 写出 `DCRecipe/DPRecipe`
  - 因而它们和 `ConfigSet` 的共同点仍是“替换活动指针 -> 清理下游缓存 -> 保存时直接从当前槽位取值”
  - 但差异也已明确：`DCRecipe/DPRecipe` 的槽位在前部持久对象区 `0x28/0x30`，而不是后置 `0x138..0x160` 缓存段
- 已继续把 `Ref/Dark/Sample` 的写出路径并入主线：
  - `Ref` 直接从 `this+0x38` 写出
  - `Sample` 直接从 `this+0x48` 写出
  - `Dark` 则存在保存期派生分支：
    - 若当前 `DCRecipe::averageDark` 未启用，或 `Dark` 底层数组体缺失，则直接写 `this+0x40`
    - 否则会先构造临时 `BinaryArchive`，对当前 `Dark` 重新 `finalize/reread/extract`，再按 `DC::bin(...)` 现算一份新的 dark body 写出
  - 因而前部基础指针槽位并不都等于“无条件原样落盘”：
    - `Ref / Sample` 当前符合直接写活动槽位的模型
    - `Dark` 当前则已出现“活动槽位 + 保存期重算物化”的特例
- 同时，`RawDataSet::{configSet,baseConfigSet,config,setConfigApp}` 的静态证据已把缓存层继续收窄为：
  - `0x138` = 当前 `ConfigurationSet`
  - `0x148` = 由当前 `ConfigSet` + config-app 名称 `build(...)` 出来的“当前配置缓存”
  - `setConfigApp()` 与 `setConfigSet()` 都会显式清空 `0x148`
- 这进一步支持：
  - node 内后来变化的重点是“配置引用与缓存被替换/失效”
  - 而不是旧 `Configuration` 对象体被原地重写
- 新增的 `getFixedNoiseData()/measData()` 证据又把缓存链再推进了一层：
  - `0x140` = fixed-noise 的 `MeasData` 缓存
  - `measData` 两个重载都共享 `sub_18001c890 / sub_180021970`
  - 这两个共享 helper 都会反向调用 `getFixedNoiseData()`
- 因而当前后置缓存段至少可保守分层为：
  - `0x138` = `ConfigSet`
  - `0x140` = fixed-noise measData 缓存
  - `0x148` = 当前 config 缓存
  - `0x150/0x158/0x160` = 尚未命名、但同属后置派生缓存段
- 新增的 vtable/未命名虚函数证据又把 `0x150/0x158` 的依赖层级再收紧为：
  - 当当前 config (`0x148`) 被直接替换时，会清空 `0x150`
  - 当 `ConfigSet / DCRecipe / DPRecipe` 更新成功时，会继续清空 `0x158`
  - setter 本身统一走 `Countable::notify(...)`，因此缓存更像“通知驱动 + 懒重建”
- 这意味着当前最保守且最稳的理解是：
  - `0x150` = 依赖当前 config 的下一层派生缓存
  - `0x158` = 依赖 configset/recipe 组合状态的再下一层派生缓存
  - `0x160` = 仍待继续坐实
- 新增的 vtable 尾槽对齐又把接口边界进一步澄清为：
  - `systemModel()`、文本导入/导出、SNR 检查都在 `0x138..0x160` 这段后置缓存/派生层之上工作
  - `0x180018900` 中出现的 `config()+0x160` 命中属于 `XMSE::Configuration` 内部字段，而非 `RawDataSet+0x160`
- 因而当前关于 `RawDataSet+0x160` 的结论仍需保持严格证据边界：
  - 还不能因为看到了某个 `+0x160` 就把它误认成 `RawDataSet` 后置缓存槽
- 新增的 ctor/dtor/copy ctor 对照又把后置缓存的“代际短命性”收紧为：
  - `0x138..0x160` 会在构造时统一清零、析构时统一释放
  - 但 copy ctor 目前只显式复制到 `0x150`
  - `0x158/0x160` 没有被复制进新对象
- 这进一步支持：
  - `0x158/0x160` 比 `0x138..0x150` 更偏向运行期瞬态缓存
  - 目前不应把它们当作稳定逻辑状态字段
- 新增的 helper caller/xref 证据又把“共享前置链”拆清为两类：
  - `sub_180015a80` = sample/dark/ref/config 的存在性与一致性前置检查
  - `sub_18002e270` = `ConfigurationSet/@Default` 相关的 calibration/HWSetup 有效性校验
- 因而当前 `get/put/systemModel` 共享 `sub_18002e270`，并不是因为它们共享同一后置缓存，而是因为它们共享同一“配置有效性门槛”
- 另一个新增结果是：
  - `acquire()` 中再次出现的 `+0x160` 仍属于 `XMSE::Configuration`
  - 它当前可保守理解为 signal saturation 阈值字段的 fallback 来源
  - 这再次排除了把它误记成 `RawDataSet+0x160` 的风险
- 新增的 `idnFactor()/idnIntensity()` 证据又进一步说明：
  - 它们共用 `sub_180015a80` 作为前置检查器
  - 但主路径本身是 `binStep/raw matrix -> IDNCal/OpticalFilterCal` 的算法计算
  - 当前未见它们直接读写 `RawDataSet+0x158/+0x160`
- 因而这两条线目前更适合作为“又一次排除缓存误判”的证据，而不是 `0x158/0x160` 的命名来源
- 新增的 `binStep()` 证据则首次把两个后置缓存槽位坐实为“有用途的缓存”：
  - `0x150` = `PSFCal::psfFFT(...)` 结果缓存，供 `RawData::psfCorrect(...)` 使用
  - `0x158` = `idnFactor()` 结果数组缓存，供 `Apply IDN` 校正使用
- 新增的 `sub_180021970` / SIMD 核心证据又把最后一个匿名槽位坐实为：
  - `0x160` = `measData/getFixedNoiseData` 复用的 `Array<double>` 工作缓冲区缓存
  - 该缓存直接传给 AVX/SSE2 `MeasData calculations` 内核复用
- 因而 `0x138..0x160` 这一段当前已经全部有了最小语义：
  - `0x138` = `ConfigSet`
  - `0x140` = fixed-noise `MeasData`
  - `0x148` = 当前 config
  - `0x150` = PSF FFT kernel
  - `0x158` = IDN factor array
  - `0x160` = SIMD measData scratch array<double>
- 真实 `test.dat` 已经直接给出 10 条 `xmseTail` 命中，因此后续收益最高的是继续收窄 `legacyRoot` 非空 group 的内部语义，而不是重复证明“有没有命中”
- 即便继续逆向 `put/save`，目标也仍然是服务解析，不是补写入功能
- 如果不先按四层/五阶段切分问题，就很容易把：
  - 协议层误差
  - 容器层语义
  - 基类公共尾部
  - XMSE 派生字段
  - 保存链年代解释
  混在一起，导致“每一步都像在猜字段”

有了这条锚点链，后续匿名标量段才能安全地逐段对照。

## 7. 下一步具体处理方案

### 7.1 先做什么

先做：

- 以当前已坐实的 `RawDataSet::put -> SmartPointer::insert -> Archive::putPointer -> Archive::putObject` 为入口
- 把 `SubSystem::setConfigSet(DC)` -> `setConfigSetXMSE(XMSE)` -> `RawDataSet::setConfigSet` 这一层无降级转发固定下来
- 将 `ClassInfo` 分派结论固定下来，不再在“静态回调 vs 虚 put”上反复空转
- 继续检查 `DC::Configuration::Settings` 是否在当前样本里被版本门控、默认值省略或完全为空
- 继续检查 `test.dat` 是否更可能由多个版本先后写入，而不是一次性由当前 DLL 分支生成
- 保留默认配置生成时序为次级分支，仅在出现新证据时再继续深挖
- 在保存链闭环前，不再对未命中的 trailer 语义做超出字节证据的 reader 侧假设

### 7.2 如何处理

处理方法：

- 保留 `SampleID / SysInfo` 等前缀槽位为证据项，不草率扩写
- 继续利用真实 `test.dat`、现有 unified report、DLL 反编译、Core/Base 保存链四方交叉验证
- 优先记录“哪一层已经排除对象切片/类型降级”，避免在同一假设上反复空转
- 每拿到一条新的保存链结论，就同步回写逆向/设计/实施文档

### 7.3 做完后的直接收益

一旦这一步完成，统一输出将可以直接表达：

- 顶层 `RawDataSet`
- `Ref / Dark / Sample`
- `DCRecipe / DPRecipe`
- `ConfigSet`
- `Configuration` 中 `xmseTail` 缺失的真实原因边界

## 8. 当前项目的风险点

当前最主要风险不是“识别不到类型”，而是：

- 把匿名标量段过早命名
- 把容器兼容关系误当作严格继承关系
- 让局部样本成立的假设误入通用 reader

因此当前实施策略必须继续保持：

- 先证据
- 后结构
- 再物化
