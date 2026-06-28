# 当前开发设计文档

## 1. 设计目标

当前项目的设计目标不是“尽快把样本读完”，而是：

- 以真实 DLL 逆向证据为唯一事实源
- 在当前工程中建立可验证、可扩展、可回归的 FTML/XMSE 读取框架
- 将“类型识别”和“按类型物化”分离
- 对未证实结构保持 trace/skip，而不是猜读
- 当前工程代码只做解析读取，不实现写文件/回写 archive 能力

## 2. 当前设计原则

### 2.1 证据优先

任何字段要进入正式 reader，至少满足以下之一：

- 有直接导出函数反编译证据
- 有样本 trace 与调用顺序双重对照
- 有单元测试和真实样本输出共同验证

否则只能进入：

- trace
- summary
- candidate

### 2.2 类型识别与字段读取解耦

当前工程显式分成两层：

- 运行时类型识别：
  - 类表 / 模块表
  - `SmartPointer::extract`
  - lineage / compatibility
- 定向 reader：
  - `DCRecipe`
  - `DPRecipe`
  - `RawData`
  - `ConfigurationSet`
  - `RawDataSet`

这样做的目的，是避免“识别失败”和“字段错位”互相污染。

### 2.3 顶层对象与内部对象分离建模

当前项目已经确认：

- `node[3]` 里存在顶层 `RawDataSet`
- 顶层对象在 `read_first_section()` 后会被预读

因此当前设计中：

- 顶层对象使用专门的 summary/tracing 通路
- 内部动态对象仍沿用统一 `read_dynamic()` 分派

### 2.4 数组与容器单独收敛

当前工程不会把所有 FTML 容器做成通用 materializer，而是：

- 只为当前样本需要的 `Array2D<unsigned>` / `Array<unsigned>` 实现真实落地
- 其它容器保持“识别到类型，但不泛化物化”

### 2.5 输出统一

统一输出是当前设计的一部分，不是调试附属品：

- 动态对象摘要
- 顶层 `RawDataSet` 证据摘要
- 当前能力边界
- 已支持 / 未支持 / 未物化原因

### 2.6 四个 DLL 的职责分层

当前项目现在已经可以把 `FTMLCore.dll / FTMLUtil.dll / FTMLBase.dll / FTMLSysXMSE.dll` 明确理解成四层，而不是一组并列的大杂烩：

- `FTMLCore.dll`
  - 归档协议层
  - 负责 `Archive / BinaryArchive / TextArchive`
  - 负责 type code、`ItemHeader`、formatted payload、类表/模块表、`Compress`、`SmartPointer` 所依赖的归档上下文
  - 这一层回答的是“字节怎样变成统一 archive item，动态类型怎样被识别”
- `FTMLUtil.dll`
  - 名字语义与通用容器层
  - 负责 `NVT::NameString`、`NamedValueTable`、`NamedValueSet`、`NamedValueList`、`NVT::Value`
  - 这一层回答的是“容器怎样表达 key/value、分层名字、对象指针值”
- `FTMLBase.dll`
  - 跨业务基础对象层
  - 负责 `DC::RawDataSet`、`DC::Configuration`、`Tilt`、`HWSetupDef` 等被多个业务模块复用的对象
  - 这一层回答的是“业务基类公共前缀是什么、哪些字段属于通用仪器/配置层”
- `FTMLSysXMSE.dll`
  - XMSE 业务对象层
  - 负责 `XMSE::RawDataSet / RawData / DCRecipe / DPRecipe / Configuration / ConfigurationSet`
  - 这一层回答的是“XMSE 自己在基类之后又扩展了什么字段、什么容器、什么保存链”

当前最重要的边界意识是：

- `FTMLCore` 解决协议与动态类型
- `FTMLUtil` 解决名字与容器
- `FTMLBase` 解决通用业务基类
- `FTMLSysXMSE` 解决 XMSE 具体业务语义

因此，任何一个“读错了”的问题，都不应该一上来就在 `FTMLSysXMSE` 里猜字段，而必须先判断它究竟属于哪一层的问题。

### 2.8 保存链的使用边界

当前项目虽然会持续逆向 `put/save` 链，但必须明确：

- 逆向 `put/save` 的目的不是恢复写入功能
- 当前工程不实现任何写文件、重打包、回写 `archive` 的能力
- `put/save` 链在这里只承担三种作用：
  - 证明读取顺序或字段边界
  - 解释对象体为何会呈现当前样本形态
  - 判断“同一文件被不同版本先后写入”之类的年代/形成过程假设是否成立

因此，对 `put/save` 的所有分析都必须回收到“解析如何更准确”这个主目标上，而不能扩展成写入侧开发范围。

### 2.7 四个 DLL 的串联关系

当前已经坐实的串联链，可以统一写成：

```text
文件字节 / node 数据
  -> FTMLCore::BinaryArchive::next/doGet/read_first_section
  -> FTMLCore::Archive::get / getClassAndModuleLists / SmartPointer::extract
  -> FTMLUtil::NameString / NamedValueTable / NamedValueSet / NamedValueList
  -> FTMLBase::DC::* / HWSetupDef / Tilt / Calibration
  -> FTMLSysXMSE::XMSE::* 业务对象
```

对读取链来说，这个串联的含义是：

- `FTMLCore` 先把字节提升成“可消费的 archive item”
- `FTMLCore` 再通过 class/module 字典和 `SmartPointer::extract` 给出动态类型与兼容关系
- 如果对象或字段命中的是通用容器/名字语义，则转到 `FTMLUtil`
- 如果对象先命中的是通用业务基类，则转到 `FTMLBase`
- 只有在基类前缀和容器边界都站稳之后，才继续进入 `FTMLSysXMSE` 派生字段

对保存链来说，这个串联的含义是：

- `FTMLSysXMSE` / `FTMLBase` 的业务对象实现 `get/put`
- `FTMLCore::Archive::putPointer / putObject` 负责统一归档分派
- `ClassInfo / ClassData / usesVirtualIO` 决定对象到底走虚 `put` 还是静态回调
- `FTMLUtil` 容器只负责把对象值包装成 `NVT::Value`、`NamedValueList/Set` 等通用承载，不直接决定业务对象被“降级”

所以，当前项目里“串联关系”的最稳妥理解不是“上层调用下层几个工具函数”，而是：

- `FTMLCore` 提供协议底座
- `FTMLUtil` 提供容器语义
- `FTMLBase` 提供业务基类
- `FTMLSysXMSE` 在前三层之上叠加 XMSE 自身语义

## 3. 当前代码结构

### 3.1 协议层

- `src/Archive.cpp`
- `src/BinaryArchive.cpp`
- `src/TextArchive.cpp`

职责：

- 统一归档协议
- 条目推进
- formatted payload 消费
- 类表 / 模块表装载

### 3.2 类型系统层

- `src/SmartPointer.cpp`
- `src/TypeCompatibility.cpp`

职责：

- pointer/object 动态类型提取
- 继承兼容判断
- recovered lineage 回退

### 3.3 业务读取层

- `src/XMSERawDataSet.cpp`

职责：

- XMSE 定向 reader
- trace 收集
- 数组物化
- 配置链最小读取

### 3.4 报告与样本分析层

- `src/main.cpp`

职责：

- 统一报告
- 顶层 `RawDataSet` 证据 trace
- 当前能力边界输出

### 3.5 回归层

- `tests/UnitTest.cpp`

职责：

- 单元测试
- 回归稳定
- 样本形状的最小协议覆盖

## 4. 分阶段分析方法

当前项目后续继续推进时，应该显式按阶段切换目的，而不是所有问题都混在一起逆向。

### 4.1 阶段一：协议识别阶段

目的：

- 先确认当前问题是不是协议层问题
- 判断是 `TypeCode / payload / meta / formatted payload / class-list / module-list` 哪一环出了偏差

主要看：

- `FTMLCore.dll`
- `src/Archive.cpp`
- `src/BinaryArchive.cpp`
- `src/SmartPointer.cpp`

典型问题：

- 当前 item 为什么读歪
- 这个 pointer 为什么识别成别的动态类型
- 这是 node 内偏移还是文件绝对偏移
- 这里能不能发生数值类型转换

证据标准：

- 反编译调用顺序
- 样本字节与 `ItemHeader` 对照
- 本地实现与真实 DLL 的行为对齐

### 4.2 阶段二：容器语义阶段

目的：

- 在对象字段还没完全命名前，先搞清楚容器怎样承载名字和值

主要看：

- `FTMLUtil.dll`
- `NameString / NamedValueTable / NamedValueSet / NamedValueList / NVT::Value`

典型问题：

- 这是普通字符串还是 `NameString`
- `@Default` 是业务字段还是容器 entry 名称
- `uint8(32)` 是随机值还是 `ValueTag`
- `ConfigInfo` 为什么有时只有 `NamedValueTable` 基型、没有条目

证据标准：

- 容器 `get/addValue/setPtr` 反编译
- 样本里的 group / pointer / tag 对照
- reader 最小结构化结果是否与反编译一致

### 4.3 阶段三：业务基类阶段

目的：

- 先收敛“这段字节属于通用基类还是派生类”
- 避免把 `FTMLBase` 的公共尾部误命名成 `XMSE` 业务字段

主要看：

- `FTMLBase.dll`
- `DC::RawDataSet`
- `DC::Configuration`
- `HWSetupDef`
- `Tilt`

典型问题：

- `ConfigInfo` 后面是不是已经进入 `XMSE` 派生尾字段
- `Extracted / Calibrations / Settings` 哪些字节是基类固定边界
- `HWSetupDef` 这里是文本表达还是 bitmask

证据标准：

- 基类 `get/put` 真实顺序
- 样本固定 trailer 字节
- 非空 / 空 group 的实物对照

### 4.4 阶段四：业务派生阶段

目的：

- 只在前三层站稳后，再提升 `XMSE` 业务字段

主要看：

- `FTMLSysXMSE.dll`
- `XMSE::RawDataSet`
- `XMSE::RawData`
- `XMSE::DCRecipe / DPRecipe`
- `XMSE::Configuration / ConfigurationSet`

典型问题：

- 这些字段是否真的是 XMSE 专属
- `xmseTail` 是未落盘、未读到，还是被基类段遮住
- 默认配置生成链为什么出现样本差异

证据标准：

- 派生 `get/put` 导出实现
- 样本 trace 与体内实际偏移
- 单测与真实 `test.dat` 双回归

### 4.5 阶段五：保存链与年代解释阶段

目的：

- 当字段边界已经坐实时，再解释“为什么文件会长成这样”
- 明确这一步只服务于解析解释，不服务于写入实现

主要看：

- `ClassInfo / ClassData / usesVirtualIO`
- `Archive::putPointer / putObject`
- `XMSE::Configuration::put`
- `setDefaultConfigSet()`
- node / section 的物理布局

典型问题：

- 对象是否真的被切片或降级
- 同一文件是否可能被不同版本先后写入
- 为什么 `legacyRoot` 与 9 个生成配置在 `Calibrations` group 上分化

证据标准：

- 保存链反编译
- section / object body 的物理布局
- 旧体兼容读与当前字典版本约束共同成立

## 4. 当前确定的数据结构建模策略

### 4.1 已正式建模

当前已经进入正式 reader/summary 的结构：

- `DCRecipe`
- `DPRecipe`
- `RawData`
- `ConfigurationSet`
- `Configuration`
- `RawDataSet` 的最小正式物化

其中 `Configuration` 当前已进入 summary 的可靠字段包括：

- `System`
- `Comment`
- `TimeStamp(year/month/day/hour/minute/subMinuteRaw)`
- `BaseName`
- `ConfigInfo`

### 4.2 仅作为候选或证据摘要

当前仍然只作为 trace/candidate 的内容：

- `RawDataSet.SampleID` 的最终业务语义
- `SysInfo` 的精确继承关系

当前已经从 candidate 升格为已证实槽位的内容：

- `RawDataSet.SampleID`
  - 当前已正式进入 `RawDataSet::get()`
  - 真实 `node[3]` 当前稳定读出为 `<empty>`
- `RawDataSet.SysInfo`
  - 当前已正式进入 `RawDataSet::get()`
  - 真实 `node[3]` 当前稳定读成 `NamedValueSet<=NamedValueTable` 兼容变体
  - 当前已稳定下沉最小键值摘要：`structuredScalars{name,valueTag,encoding,rawValueType,raw value}`
- `RawDataSet.Version`
  - 样本位置：`0x701`
  - archive 原始 item type：`Int8`
  - 样本值：`2`
  - 调用侧期望读取类型：`Int16`
- `RawDataSet` 中 `Rotation / AnalyzerRef / AnalyzerSample / Tilt flag`
  - 已由样本 item 序列与 `XMSE::RawDataSet::get` 调用顺序双重对齐
- `RawDataSet` 中 `PixShiftRef / PixShiftSample / FilterRef / FilterSample`
  - 已由 `Tilt` 后匿名块与后续 `RawData` 指针入口双重对齐
- `RawDataSet` 中 `Ref / Dark / Sample / ConfigSet`
  - 已正式进入 `RawDataSet::get()` 的最小 reader，并继续通过 `slot_offset -> body_offset(+0xE)`、`object_id`、现有动态对象摘要三重挂接进入统一报告

## 5. 当前设计的优势

- 不会因为一个前缀字段猜错导致后续全体错位
- 逆向结论可以逐块进入代码，而不需要一次性恢复整个对象
- 样本、单测、DLL 反编译三条证据线可互相校验

## 6. 当前设计的限制

- 顶层 `RawDataSet` 虽已恢复 `SampleID / SysInfo` 的正式 reader，但仍未恢复其最终业务语义命名
- `ConfigurationSet` 仍以最小 reader 为主
- `NamedValueList` 当前已在“原始预览 + 最小结构化承载”两层并存
  - 原始层仍保留顶层与 `group` 第一层摘要，避免过早业务命名
  - 结构化层已下沉已坐实的容器协议字段：`entriesCountRaw` 与 `structuredEntries{name,valueTag,pointerType}`
  - 对于 `StructuredEntry` 中已确认是 `Configuration` 的 pointer，允许进一步下沉 `ConfigurationSummary`
- `NamedValueSet` 的嵌套 pointer entry 也开始保留正式 reader 元信息：
  - `object_id`
  - `body_offset`
  - `expected / compatibility`
- 对 `node[3].SysInfo` 而言，当前还额外保留：
  - `structuredScalars{name,valueTag,encoding,rawValueType,raw value}`
  - 其中 `encoding` 只做样本级只读归档：`0x4 -> TextPayload`、`0x80 -> NumericScalar`
- `ConfigurationSummary::configInfo` 同步保留 `object_id / body_offset`
  - 当前真实样本命中 `body_offset`
  - `object_id` 允许为 `0`，不能据此草率判空
- 若 `configInfo` 的实际动态类型落到 `NamedValueTable / NamedValueSet`，当前允许再下沉一层最小结构化 entry 列表
  - 仅保留容器 entry 元信息，不提升其内部业务字段
  - 当前真实 `test.dat` 尚未命中该层输出
  - 根据 `FTMLUtil.dll` 逆向，若动态类型仅为 `NamedValueTable` 本体，则没有 entry 列表是预期行为
- `XMSE::Configuration` 的派生尾字段 `SumsPerCycle / TimingMode / Saturation / TurnsPerCycle[2] / NumPixel` 已允许进入正式 reader 结构
  - 当前有导出实现、真实样本与单测三重证据
  - 当前 reader 只有在确认命中 `Extracted + BeginGroup` 这一对固定边界后，才会把该段认作 `DC::Configuration` trailer
  - 之后再接受 `UInt8 / UInt16 / Int16 / UInt32` 的紧凑整型编码来物化 `xmseTail`
- 当前真实 `test.dat` 中，`XMSE::Configuration` 至少表现出两种覆盖形态：
  - `coverage=[system,comment,timestamp,configInfo,xmseTail]`
  - `coverage=[system,comment,timestamp,base,configInfo,xmseTail]`
- 真实 `test.dat` 当前已经不再是“`xmseTail` 缺失”问题，而是“`ConfigInfo` 之后固定 trailer 的语义切分已经完成到哪一步”问题
- 当前样本级已证实的 `xmseTail` 实测值为：
  - `legacyRoot.@Default` 命中 `160, 3, 64000, [5,1], 1024`
  - `TF#1 / CD#2` 命中 `320, 3, 64000, [5,1], 1024`
  - `TurboFilm#3 / FoG#4 / FDC#5 / TurboShape#6 / IDO#7 / TrueShape#8 / SPA#9` 命中 `160, 3, 64000, [5,1], 1024`
- 当前样本级还已证实一个更细的边界差异：
  - `legacyRoot.@Default` 的 `Calibrations` group 非空，内部形态与 `Settings + Calibration*` 循环对齐
  - 9 个生成配置的 `Calibrations` group 为空，随后直接进入 compact `xmseTail`
- 当前 reader 已把这条差异下沉成只读摘要而非业务过命名：
  - `dcTrailer.extracted`
  - `dcTrailer.calibrationEntries.size()`
  - 每条 entry 的 `settingsRaw + calibration pointer 元信息`
- 这一步的设计意图是：
  - 先把真实样本形态稳定读出来
  - 再决定后续是否有足够证据提升成更细的业务字段
  - 不在证据不足时直接把 calibration body 深层结构命名成固定业务语义
- 当前真实样本又进一步证明：
  - `legacyRoot.@Default` 这 14 条 calibration entry 的动态类型全集已经稳定可见
  - 这 14 条 entry 的 `settingsRaw` 当前全部等于 `322(XMSErprc65)`
  - `dcTrailer.extracted` 在当前样本中也同样稳定显示为 `322(XMSErprc65)`
  - 因而下一步若继续推进，应优先去解释 `settingsRaw=322(XMSErprc65)` 的更深静态语义，而不是继续猜 calibration body 内部字段
- 新增保存链约束：
  - `XMSE::RawDataSet::put` 在 `ConfigSet` 槽位写入时仍显式使用 `XMSE::ConfigurationSet` 的 expected class id
  - `XMSE::ConfigurationSet` 当前未见独立 `put` override，而是沿用 `DC::ConfigurationSet::put`
  - `DC::ConfigurationSet::set(...)` 仅把配置条目作为 `const Countable*` 指针封装进 `NVT::Value`
  - `FTMLCore::SmartPointer::insert` 自身只转发到 `Archive::putPointer(...)`
  - `Archive::putPointer(...)` 会先查 `TaggedObjectList`；已有 tag 的对象只复用 pointer/tag，不重复写对象体
  - 只有首次出现的对象才会继续进入公共对象写出 helper
  - 因而容器层并未直接证实发生对象切片或强制降级，反而更像是在复用既有对象体并更新外层引用
- 新增 `ClassInfo` 分派证据：
  - `ClassInfo::ClassInfo(ClassData const&)` 会把 `ClassData+0x1A` 复制到 `classInfo+0x52`
  - `Archive::getObject/putObject` 在 `classInfo+0x52 != 0` 时走对象虚 `get/put`
  - `XMSE::Configuration` 与 `XMSE::ConfigurationSet` 的静态 `ClassData` 当前都表现为 `usesVirtualIO=1`，且 `static_get/static_put` 槽位为空
- 新增默认配置生成链证据：
  - `setDefaultConfigSet()` 的完整后半段只使用 `DC::Configuration::setSystemID / setBaseName / removeCals` 与 `DC::ConfigurationSet::set`
  - 所有派生配置条目都由 `SmartPointer::makeCopy()` 生成
  - `@Default` 先创建并入集合，后续 `TF / CD / <Filter> / <Filter>.TF / <Filter>.CD` 才在 copy 后执行 `removeCals()`
  - 因而 `legacyRoot.@Default` 非空 `Calibrations` group` 与 9 个生成配置空 group 的分歧，当前已可由静态调用顺序直接解释，而不必再假设随机样本漂移
  - `XMSE::Configuration` 的 5 个专属 setter 当前都未发现代码 xref
- 新增旧体兼容读证据：
  - `XMSE::Configuration::get` 对 `xmseTail` 6 字段会顺序尝试读取
  - 但不会检查这些 `Archive::get(...)` 的返回值
  - 因而旧配置体即使缺失 `xmseTail`，当前 DLL 仍可把它当作合法 `XMSE::Configuration` 读出
- 新增字典层约束：
  - `test.dat` 当前可见的类/模块字典统一解析为 `module_version=7.00.14`
  - 因而“同一文件被不同版本先后写入”若成立，更像是旧对象体保留、而当前索引/字典层已被较新版本覆盖
- 新增同 node section 布局证据：
  - `node[3]` 起始处的 `FTML::BinaryArchive` 头后紧跟 `Int32(section_offset=0x874AA)`
  - 当前 `ConfigurationSet` 的 10 个 `XMSE::Configuration` 体全部位于 `0x874AA` 之前
  - 其中最后一个 `SPA#9.@Default` body=`0x8742C` 距 section 仅 `0x7E`，其 `ConfigInfo` body=`0x8748E` 距 section 仅 `0x1C`
  - 这说明当前 class/module list 入口确实位于同一 node 的后半段，而目标对象体位于其前部
  - `BinaryArchive::doFinalize(...)` 的写侧反编译又表明：这个 section 不是偶然“附在后面”的块，而是 finalize 时通过回填 node 头 `FinalIndex` 后，再从该偏移继续写出的 class/module list 字典段
  - `Archive::reread(...)` 与二进制 reset helper 的读侧反编译也表明：DLL 会先读 node 头 `FinalIndex`，跳到该偏移预读字典，再回到原位置继续读前部对象体
  - 新增字节级约束：`0x23/0x25` begin marker 的第 2 字节是 `class-id-bytes | tag-bytes` 的 nibble-packed 头
  - 因而 `23 04 50 9F D6 04` 与 `25 84 <class-id:4> <tag:8>` 现在都能直接按物理编码解释，而不是仅靠高层语义命名
- 新增固定边界约束：
  - 这些 `body_offset / section_offset` 在当前实现里是 node 内偏移，不是全文件绝对偏移
  - `SPA#9.@Default.ConfigInfo@0x8748E` 与 section@`0x874AA` 之间那 `0x1C` 字节，在 `test.dat` 中是稳定写死的一段 trailer，不是可随意漂移的模糊区
  - 当前真正未解出的只是这段 trailer 的语义，而不是边界是否存在
- 新增 `DC::Configuration` 基类尾部约束：
  - `FTMLBase.dll` 已坐实 `DC::Configuration::get/put` 在 `ConfigInfo` 之后还会继续处理 `Extracted / Calibrations / Settings`
  - 因而当前 `test.dat` 中 `ConfigInfo` 之后出现的固定 trailer，至少有一部分应先归入 `DC` 基类尾部，而不能直接拿来对齐 `XMSE` 派生尾字段
  - 对 `SPA#9` 样本而言，真实文件 `0x87B81` 已命中 `EndObject`，后一个字节 `0x87B82` 才进入当前活动 section，说明该对象体在 section 前已闭合
- 因此当前 reader/设计边界进一步收窄为：
  - 不再把“真实样本没有 `xmseTail`”作为当前主矛盾
  - 当前更强的直接结论是：`ConfigInfo` 后固定 trailer 的前半段已经与 `DC::Configuration` 基类尾部对齐，后半段也已在真实样本上物化为 `XMSE` 派生尾字段
  - “同一文件可能被不同版本先后写入”仍是可兼容假设，但它现在解释的是对象年代/索引重写关系，而不再用于解释 `xmseTail` 为什么没读出来
  - 新增的 `node[3]` 内部布局仍表明：即便不跨 node，文件也完全可能在同一 node 内形成“前部对象体 + 固定 trailer + 后部新索引/字典”的代际叠加
- 新增 `ConfigSet` 挂接证据：
  - `XMSE::SubSystem::setConfigSet(FTML::P<DC::ConfigurationSet>)` 基类入口会先断言实参 `isA(XMSE::ConfigurationSet)`，再 `canCast(..., 0xDCCD2C00)` 收窄回 XMSE 派生类型
  - 之后才继续转调 `XMSE::SubSystem::setConfigSetXMSE(...)`，当前未见“先降级成 DC，再以基类形态保存”的路径
  - `XMSE::SubSystem::setConfigSetXMSE(...)` 会先按 `TextID` 定位目标 `RawDataSet`
  - 若传入的新集合非空，还会先刷新 `SubSystem` 当前选中条目的内部指针对
  - `XMSE::RawDataSet::setConfigSet(...)` 在一致性检查通过后直接替换 `this+0x138` 的 `ConfigurationSet` 智能指针
  - 该一致性检查本身会先用当前 app 名称到新集合执行一次 `find(...)`，失败时走 `failBadApp(...)`
  - 同时清空 `this+0x148` 相关缓存并触发一次刷新
  - `XMSE::RawDataSet::put(...)` 在保存 `ConfigSet` 时又直接执行 `SmartPointer::insert(*(this+0x138), 0xDCCD2C00, operator()(arg2, "ConfigSet"))`
  - 因而“同一 node 内旧对象体仍在、但较新的 `ConfigSet` 引用/索引已被挂回”的解释现在比“原地回写旧对象体”更符合当前静态证据
  - 再把前部绝对偏移链并入后，可把这个模型再具体化成：
    - `0x6E3 -> 0x6F1 -> 0x6F3 -> 0x701 -> 0xDD8 -> 0xDD9 -> 0xDDB` 这条 `RawDataSet -> SysInfo -> Version -> DCRecipe` 链稳定留在 node 前部对象体区
    - `0x874AA(node 内) / 0x87B82(file abs)` 的后半段则承载当前活动 class/module section
    - 因而当前更稳妥的设计理解是“前部持久对象体 + 后部更新引用/字典”，而不是假定 `RawDataSet` 前缀对象体会被保存链整体重写
  - 再把 `setDCRecipe/setDPRecipe` 并入后，这个设计理解需要再补一个限定：
    - 两条 recipe 更新路径与 `ConfigSet` 一样，都是“替换当前活动指针 -> 清理下游缓存 -> 保存时直接从当前槽位取值”
    - 但 `DCRecipe/DPRecipe` 的活动槽位位于前部持久对象区 `0x28/0x30`
    - `ConfigSet` 则位于后置 smart-pointer/cache 段 `0x138`
    - 因而当前更精确的设计口径应是：
      - 前部对象区中的 recipe/config/rawdata 等基础指针槽位构成持久对象骨架
      - 后置 `0x140..0x160` 段承担由这些基础槽位派生出来的缓存与工作缓冲
      - 保存链更新不等于“整段对象体重建”，而更像“基础活动指针替换 + 派生缓存失效 + 后半段 section/字典更新”
  - 再把 `Ref/Dark/Sample` 的写出路径并入后，还要补一个保存期例外：
    - `Ref(0x38)` 与 `Sample(0x48)` 当前都表现为直接从活动槽位写出
    - `Dark(0x40)` 则可能在保存期按当前 `DCRecipe` 临时重算后再落盘
    - 因而“前部基础指针槽位”内部还需继续区分：
      - 一类是直接写出当前活动对象
      - 另一类是以当前活动对象为输入、但允许保存期再物化一份派生态对象
- 新增 `RawDataSet` 配置缓存证据：
  - `configSet()/baseConfigSet()` 都直接返回 `this+0x138`
  - `config()` 把由当前 `ConfigSet` 和 config-app 名称推导出的“当前配置”缓存到 `this+0x148`
  - `setConfigApp()` 与 `setConfigSet()` 都会显式清空 `this+0x148`
  - 因而 `0x148` 更应理解为派生缓存，而不是另一份独立持久 `Configuration` 体
- 新增 `measData/fixed-noise` 缓存证据：
  - `getFixedNoiseData()` 成功时会把 fixed-noise `MeasData` 缓存到 `this+0x140`
  - `measData()` 的两个重载都共享 `sub_18001c890 / sub_180021970`
  - 这两个共享 helper 又都会调用 `getFixedNoiseData()`
  - 因而 `0x140` 也更适合作为“派生测量数据缓存”理解，而不是基础对象槽位
  - `0x150/0x158/0x160` 当前虽尚未直接命名，但从构造/析构与相邻缓存链看，仍更像同一后置派生缓存段
- 新增 vtable/通知链证据：
  - setter (`setTilt/setRotation/setPixelShift/...`) 统一触发的 `(*this + 0x48)(..., 0x1D4746, 0)`，在 vtable 中对应 `FTML::Countable::notify(...)`
  - 这说明后置缓存失效目前更像“通知驱动 + 懒重建”，而不是 setter 内部直接逐项清空
  - 同时，一个未命名虚函数会按 `Countable` 动态类型把输入分派给 `setConfigSet/setDCRecipe/setDPRecipe`，或直接替换当前 config (`0x148`)
  - 当 `0x148` 被替换时，它会清空 `0x150`
  - 当 `ConfigSet / DCRecipe / DPRecipe` 更新成功时，它又会继续清空 `0x158`
  - 因而 `0x150` 与 `0x158` 现在至少可理解为比 `0x148` 更下游的两级派生缓存
- 新增 vtable 尾槽地图：
  - 尾部已可识别出 `systemModel()`、未命名配置分派入口、未命名采集/派生入口、step 时间戳访问、文本导入/导出、SNR 检查器等槽位
  - 这说明 `RawDataSet` 的后半段 virtual 更偏向“派生结果、外部 I/O 与质量检查”，而不是基础 archive 字段访问
  - 同时需要明确区分：`0x180018900` 中命中的 `config()+0x160` 属于 `XMSE::Configuration` 内部字段，不是 `RawDataSet+0x160`
  - 因此 `RawDataSet+0x160` 目前依然未被直接命中，不能超证据命名
- 新增 ctor/dtor/copy ctor 交叉约束：
  - `RawDataSet` 的默认构造会清零 `0x138..0x160`
  - 析构会按相同段统一释放
  - 但 copy ctor 目前只复制到 `0x150`
  - `0x158/0x160` 未被带入复制体，说明它们更像更短命的运行期缓存，而非稳定逻辑状态
- 新增共享 helper 分工：
  - `sub_180015a80` 负责 sample/dark/ref/config 的存在性与一致性前置检查
  - `sub_18002e270` 负责 `ConfigurationSet/@Default` 相关 calibration/HWSetup 的有效性校验
  - 因而 `get/put/systemModel` 共用 `sub_18002e270` 反映的是“配置有效性门槛共用”，不是“后置缓存共用”
- 新增 `Configuration+0x160` 的用途收敛：
  - `acquire()` 中若 recipe 未给出 signal saturation 阈值，则退回使用 `config()+0x160`
  - 该值随后直接进入 `FTML::DC::saturationCheck(..., "signal", threshold, ...)`
  - 所以当前能保守确认：这是 `XMSE::Configuration` 上与 signal saturation 检查相关的阈值字段，而非 `RawDataSet+0x160`
- 新增 `idnFactor()/idnIntensity()` 路径约束：
  - 两者都复用 `sub_180015a80` 做 sample/dark/ref/config 前置检查
  - 之后走的是 `binStep/raw matrix -> IDNCal/OpticalFilterCal` 的纯算法修正链
  - 当前未见它们直接触达 `RawDataSet+0x158/+0x160`
  - 因而这两条路径目前更适合作为“排除后置缓存误判”的证据
- 新增 `binStep()` 对后置缓存段的直接命中：
  - `0x150` 会在 `PSFCal` 路径下按长度检查并懒生成 `PSFCal::psfFFT(...)` 结果
  - 该缓存随后直接送入 `RawData::psfCorrect(...)`
  - `0x158` 会在 `DPRecipe::getOptApplyIDNCal(...)` 允许时缓存 `idnFactor()` 结果数组
  - 该缓存随后直接参与 `Apply IDN` 校正
  - 因而当前 `0x150/0x158` 都已经不是匿名槽位，而是具备明确算法用途的派生缓存
- 新增 `sub_180021970` 对 `0x160` 的直接命中：
  - `0x160` 被作为第 11 个参数传给 AVX/SSE2 `MeasData calculations` 内核
  - 这些内核会把它当作可复用的 `FTML::Array<double>` 工作缓冲区
  - 若为空或长度不足，则就地重建/扩容，再复用于后续 SIMD 计算
  - 因而 `0x160` 是比 `0x150/0x158` 更底层的 measData scratch cache
- 当前 reader 继续保持“只在样本真实出现时展示 `xmseTail`，不根据构造默认值补写”；并且新增约束：
  - 若只有一个紧凑整型而后面没有 `BeginGroup`，则不能提前把它认作 `DC::Configuration::Extracted`
  - 这条回退规则已经由新增单测覆盖，避免误吞“纯 `xmseTail` 无 `DC` trailer”的旧路径
- 其中 `int8(1)` 与 `uint8(32)` 已有协议级解释：
  - 前者与 `NamedValueList::get` 的 `Entries` 计数一致
  - 后者与 `NVT::Value` 的内部类型标签 `0x20` 一致
  - `0x20` 对齐 `Value::setPtr(const Countable*)`
  - 与之对应，构造可变容器对象的路径走 `0x10 = Value::setPtr(Countable*)`
- 尚未建立通用容器物化框架
- 对跨 node / 跨 block 的对象实体回填仍有限制

## 7. 下一阶段的设计要求

下一阶段继续坚持两个硬约束：

- 不让候选字段越级进入正式 reader
- 每推进一个结构，就必须同步更新样本输出与测试边界
- `xmseTail` 的解释继续坚持保存链证据优先，不允许因为当前构造默认值或导出实现存在就把缺失字段补写回 reader 结果
