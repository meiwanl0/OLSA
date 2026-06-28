# FTMLCore.dll 逆向分析

## 1. 范围

本文只记录当前已经坐实的 `FTMLCore.dll` 逆向结论，重点覆盖：

- `FTML::Archive / BinaryArchive / TextArchive`
- `FTML::TextID`
- `FTML::Compress`
- 类表 / 模块表加载流程

凡是本文没有写入“已证实数据结构”的内容，都不应在代码中作为确定结构直接消费。

## 2. 已确认结论

### 2.1 归档架构

已确认 `FTMLCore` 的归档体系是：

- `FTML::Archive`：统一协议门面
- `FTML::BinaryArchive`：二进制介质实现
- `FTML::TextArchive`：文本介质实现

这不是“一个大类 + 若干工具函数”，而是清晰的抽象基类 + 两个派生实现。

### 2.2 `Archive::get(...)` 的职责

`FTML::Archive::get(...)` 是统一读取入口，负责：

- 写入“期望类型 / 期望数量”等上下文
- 通过虚调用进入具体介质实现
- 在调用完成后清理期望状态

这说明上层业务对象应依赖 `Archive` 协议，而不是直接绑定 `BinaryArchive`。

### 2.3 `BinaryArchive::next()` 的职责

`FTML::BinaryArchive::next()` 的职责是把当前字节流推进为统一的归档条目事件，核心包括：

- 解析 type code
- 区分普通 payload 与 formatted payload
- 解析 meta/name
- 维护当前 `ItemHeader`

当前项目里的 `BinaryArchive::next()` 正是按这个结论正向实现的。

### 2.4 `TextID::get()` 的语义

`FTML::TextID::get()` 并不是一套独立复杂容器协议。它本质上是：

- 通过 archive 的 string-like 读取接口装载字符数据
- 更新内部 string/backing 长度

这意味着在当前项目里把 `TextID` 先作为“char payload 驱动的文本载体”处理，是有直接逆向证据支撑的。

### 2.5 `Compress::unpack()` 的入口关系

`FTML::Compress::unpack(void const*, uint32_t)` 的职责已经坐实：

- 先初始化输入 buffer 指针和长度
- 再调用无参 `unpack()` 完成真实解包

这与当前项目里“先解析 formatted header，再按解包结果落地 `payloadBytes`”的实现方向一致。

### 2.6 类表 / 模块表的加载

`FTML::Archive::getClassAndModuleLists()` 已确认负责：

- 进入 `ClassList`
- 读取 `NumClass`
- 批量装载 `ClassItem`
- 进入 `ModuleList`
- 读取 `NumModule`
- 批量装载 `ModuleItem`

这也是当前工程中运行时类型识别、`SmartPointer::extract()`、继承链恢复的前置条件。

### 2.7 保存链的最小分派语义

继续下探 `SmartPointer::insert / Archive::putPointer / Archive::putObject` 后，当前可把 `FTMLCore` 保存链最小化收敛为：

- `SmartPointer::insert(const Countable*, expected_class_id, Archive&)` 自身只做一次 retain/release 包装
- 真正的指针写入由 `Archive::putPointer(...)` 负责
- `Archive::putPointer(...)` 会先在 `TaggedObjectList` 中按对象查 tag
- 若 tag 已存在，则只写一对 pointer 边界并复用既有 tag，不再次写出对象体
- 若 tag 不存在，则先把对象加入 `TaggedObjectList`，再继续进入公共对象写出 helper
- `Archive::putObject(...)` 与 `Archive::putPointer(...)` 最终都汇到同一个对象写出 helper
- 该 helper 会先写 begin marker，再按 `ClassInfo` 分派到：
  - 对象虚 `put`
  - 或 `ClassInfo` 上登记的静态 `put` 回调
- 写完后再补 end marker

这说明 `FTMLCore` 层当前更像是在维护“对象体是否首次出现、当前 pointer 是否仅是复用旧 tag”的归档语义，而不是强制重写上层业务对象体。

### 2.8 `BinaryArchive` 的 node 头 / section 组织

继续下探 `BinaryArchive::doFinalize(...)`、`Archive::reread(...)` 以及读侧 reset helper 后，当前可以把 node 内部布局进一步收敛为：

- 写侧并不是“先写完整对象体，再额外追加一个独立的 `BinaryArchive` 对象”
- 更接近的真实语义是：
  - 先顺序写前部对象体
  - finalize 时保存当前写游标作为 `FinalIndex`
  - 回到 node 起始处回填 `FTML::BinaryArchive` 头和 `FinalIndex`
  - 再跳回 `FinalIndex` 位置，把 `ClassList / ModuleList` 作为后半段 section 写出
- 读侧 `Archive::reread(...)` 会清空 `TaggedObjectList` 后调用二进制介质 reset helper
- 该 helper 先消费 node 头的 `FTML::BinaryArchive` 与 `FinalIndex`
- 然后临时跳到 `FinalIndex` 指向的位置预读 class/module list
- 再回到原位置继续读取 node 前部对象体

这与当前 `test.dat` 实物观测到的“node 头先给出 section 偏移、对象体位于其前、class/module list 位于其后”完全一致。

## 3. 关键证据函数

### 3.1 `FTML::Archive::get`

函数：

- `?get@Archive@FTML@@QEBA_NW4ID@TypeCode@2@HPEAX@Z`

已确认行为：

- 写入期望 type/count
- 通过虚表调用具体实现
- 清理状态

### 3.2 `FTML::BinaryArchive::next`

函数：

- `?next@BinaryArchive@FTML@@UEBAAEBUItemHeader@Archive@2@XZ`

已确认行为：

- 读取原始 type byte
- 分离 sign/payload/meta/format 标志
- 形成统一 `ItemHeader`

### 3.3 `FTML::Archive::getClassAndModuleLists`

函数：

- `?getClassAndModuleLists@Archive@FTML@@IEAAXXZ`

已确认行为：

- 读取 `ClassList / NumClass`
- 读取 `ModuleList / NumModule`
- 批量落地 class/module 字典

### 3.4 `FTML::TextID::get`

函数：

- `?get@TextID@FTML@@QEAA_NAEBVArchive@2@@Z`

反编译证据显示：

- 通过 archive 的字符串读取路径把文本装入内部 string
- 用字符串长度更新 `TextID` 自身的长度/视图字段

### 3.5 `FTML::Compress::unpack`

函数：

- `?unpack@Compress@FTML@@QEAA_NPEBXI@Z`

反编译证据显示：

- 设置内部 buffer 指针、总长度、剩余长度
- 调用无参 `unpack()`

### 3.6 `FTML::SmartPointer::insert`

函数：

- `?insert@SmartPointer@FTML@@KA_NPEBVCountable@2@IAEAVArchive@2@@Z`

已确认行为：

- 若传入对象非空，先做一次引用计数保护
- 之后直接通过 `Archive` 虚表调用 `putPointer(...)`
- 自身不决定写 pointer tag、对象头还是对象体

### 3.7 `FTML::Archive::putPointer`

函数：

- `?putPointer@Archive@FTML@@UEAA_NIV?$P@$$CBVCountable@FTML@@@2@@Z`

已确认行为：

- 先在 `TaggedObjectList` 中查找当前对象是否已有 tag
- 若已有 tag：
  - 回填当前 expected class id / tag
  - 写一对 pointer 边界 marker
  - 不再重复写对象体
- 若没有 tag：
  - 将对象加入 `TaggedObjectList`
  - 进入公共对象写出 helper

其中 `arg4=1` 的公共 helper 路径会写 `0x25/0x26` 这对 pointer marker，对应“pointer 包裹下的对象体写出”。

### 3.8 `FTML::Archive::putObject`

函数：

- `?putObject@Archive@FTML@@UEAA_NAEBVCountable@2@@Z`

已确认行为：

- 读取对象动态 `class_id`
- 进入与 `putPointer(...)` 共用的对象写出 helper
- `arg4=0` 时会写 `0x23/0x24` 这对 object marker

### 3.9 公共对象写出 helper `sub_180006e10`

已确认行为：

- 先取对象 `classInfo`
- 若 `classInfo+0x52 == 0` 且 `classInfo+0x68 == 0`，直接报错 `no put for %s`
- 否则先写 begin marker，再按 `classInfo` 分派：
  - `classInfo+0x52 != 0` 时走对象虚 `put`
  - 否则走 `classInfo+0x68` 上登记的静态 `put`
- 最后恢复现场并写 end marker

### 3.10 `FTML::ClassInfo::ClassInfo(ClassData const&)`

函数：

- `??0ClassInfo@FTML@@QEAA@AEBUClassData@1@@Z`

已确认行为：

- `classInfo+0x52` 直接来自 `ClassData+0x1A`
- `classInfo+0x60` 直接来自 `ClassData+0x28`
- `classInfo+0x68` 直接来自 `ClassData+0x30`

因此，当前项目里把 `classInfo+0x52` 理解为 `usesVirtualIO`，把 `classInfo+0x68` 理解为静态 `put` 回调槽位，已有直接构造证据。

### 3.11 `FTML::BinaryArchive::doFinalize`

函数：

- `?doFinalize@BinaryArchive@FTML@@EEAA_NXZ`

已确认行为：

- 保存当前写游标 `this+0x128` 到局部变量
- 将写游标临时清零
- 回填 `BeginObject(class_id=0x04D69F50 = FTML::BinaryArchive)`
- 以 `FinalIndex` 名义写入刚才保存的旧游标值
- 恢复写状态后调用 `Archive::putClassAndModuleLists(this)`

因此，`FinalIndex` 当前应理解为“node 内后半段 class/module section 的入口偏移”。

### 3.12 `FTML::Archive::reread` 与 `sub_180012850`

函数：

- `?reread@Archive@FTML@@QEAA_NXZ`
- `sub_180012850`

已确认行为：

- `Archive::reread(...)` finalize 当前 archive 后，会清空 `TaggedObjectList` 并重新插入空 tag 基准项
- 随后进入二进制介质 reset helper
- `sub_180012850` 会先读取 node 头的 `FTML::BinaryArchive` 头
- 再读取 `FinalIndex`
- 把当前读游标临时改到 `FinalIndex`
- 预读 `ClassList / ModuleList`
- 最后恢复到原先游标，继续消费 node 前部对象体

这正是当前工程里 `read_first_section()` 所依赖的 DLL 侧真实行为。

### 3.13 `FTML::Archive::putClassAndModuleLists`

函数：

- `?putClassAndModuleLists@Archive@FTML@@IEAAXXZ`

已确认行为：

- 顺序写 `ClassList -> BeginGroup -> NumClass -> ClassItem* -> EndGroup`
- 再顺序写 `ModuleList -> BeginGroup -> NumModule -> ModuleItem* -> EndGroup`

因此，`FinalIndex` 之后的 section 不是模糊“索引块”，而是当前活动 class/module list 的完整字典段。

### 3.14 `BinaryArchive::doPut` 中 begin marker 的实际字节布局

继续下探 `?doPut@BinaryArchive@FTML@@EEAA_NAEBUItemHeader@Archive@2@PEBX@Z` 与读侧对称 helper `sub_180013730(...)` 后，当前可把 `0x23/0x25` begin marker 的物理编码收紧为：

- 第 1 字节：
  - `0x23` = BeginObject
  - `0x25` = BeginPointer
- 第 2 字节是一个 nibble-packed header：
  - 低 4 bit = class id 的字节数
  - 高 4 bit = pointer tag 的字节数
- 随后先写 class id
- 若是 `BeginPointer`，再额外写 pointer tag

读侧 `sub_180013730(...)` 的对称实现已经直接坐实：

- `count = header & 0x0F`，复制到 class-id 缓冲
- `count_1 = header >> 4`，复制到 tag 缓冲

因此：

- `0x04` = `class_id` 占 4 字节、无 tag
- `0x84` = `class_id` 占 4 字节、tag 占 8 字节

对 `test.dat` 中 `node[3]` 起始处的真实字节：

```text
0x000006D8: 23 04 50 9F D6 04
0x000006DE: 06 AA 74 08 00
0x000006E3: 25 84 00 19 28 DE 30 C3 7E 49 00 00 00 00
```

当前可以逐项对齐为：

- `23` = BeginObject
- `04` = 4-byte class id, no tag
- `50 9F D6 04` = `FTML::BinaryArchive` 的 class id `0x04D69F50`
- `06 AA 74 08 00` = 紧随其后的 `FinalIndex = 0x874AA`
- `25` = BeginPointer
- `84` = 4-byte class id + 8-byte tag
- `00 19 28 DE` = pointer 目标动态 class id
- `30 C3 7E 49 00 00 00 00` = 该 pointer 的 8-byte tag

这说明 node 前部 pointer/object 的物理编码当前已经不只是“语义上有 BeginPointer”，而是连 class-id/tag 的字节排列都能与 `test.dat` 实物逐项对上。

### 3.15 end marker 与样本末段字节

由 `sub_180006e10(...)`、`Archive::putPointer(...)` 与 `doPut(...)` 的组合可进一步确认：

- `0x24` = EndObject
- `0x26` = EndPointer
- 这两个 end marker 当前不携带额外 payload，在样本中表现为单字节闭合符

因此，`SPA#9.@Default.ConfigInfo` 后、section 之前的末段字节：

```text
0x00087B66: 26
0x00087B67: 0A 42 01
0x00087B6A: 21 22
0x00087B6C: 09 A0 09 03 0A 00 FA 09 05 09 01 05 00 04
0x00087B7A: 26 22 26 22 26 26 26 24
0x00087B82: 21
```

当前可最保守地对齐为：

- `26` = `ConfigInfo` 指针闭合
- `0A 42 01` = `UInt16(0x0142)`
- `21 22` = 空 group
- `09 A0 09 03 0A 00 FA 09 05 09 01 05 00 04` = 紧凑整型编码的 `xmseTail`
- `26 22 26 22 26 26 26 24` = 外层容器/指针/对象的连续闭合序列
- `21` = 下一段 section（class/module list）的 `BeginGroup`

这条末段字节链现在已经能和 begin/end marker 机制共同成立：

- node 前部是对象体与 pointer body
- 对象闭合后才进入 node 后半段的 section

同时需要补一个协议层约束：

- `0x25/0x26` 不能按“裸字节值”直接认作 `BeginPointer/EndPointer`
- 真实样本 `RawDataSet -> SysInfo` body 内部已经出现：
  - `0x729` 处的 `0x26`，当前落在 `82 01 26 "{1AF...}"` 这一段里，更应解释为字符串长度
  - `0xA9E` 处的 `0x25`，当前也只是普通 payload 中的字节值
- 因而 marker 判断必须依赖 `ItemHeader` 语义、调用侧 skip/read 顺序，以及相邻可信锚点（如 `Version@0xDD9`）共同成立

## 4. 已证实数据结构

### 4.1 `Archive` 协议层

已证实字段语义级别：

- 当前条目状态
- 期望 type / count
- 类表 / 模块表
- 通过虚函数派发 `get / next / doGet`

注意：

- 这里是“职责结构”已确认
- 不是说所有成员偏移都已完全恢复

### 4.2 `ClassItem`

当前项目中已经稳定使用并被样本验证的字段：

- `class_id`
- `name`
- `version`
- `module_id`

### 4.3 `ModuleItem`

当前项目中已经稳定使用并被样本验证的字段：

- `module_id`
- `name`
- `version`

### 4.4 `TextID`

已证实结构语义：

- 内部持有字符串 backing
- `get()` 最终依赖 archive 的字符读取路径
- 当前样本中的 `XMSErprc65` 应视为 archive 中的原始文本结果，而不是误读的直接证据

### 4.5 `Compress`

已证实结构语义：

- 持有输入 buffer 指针
- 持有输入长度 / 剩余长度
- 对外提供多种 `unpack(...)` 入口，但最终汇总到统一解包过程

### 4.6 `TaggedObjectList`

当前已证实的最小结构语义：

- 用于在保存链中跟踪“这个对象是否已被分配过 pointer tag”
- `Archive::putPointer(...)` 会先对它执行 `findTag(...)`
- 首次出现的对象会先 `add(...)`，之后再真正写对象体
- 再次出现的对象只复用既有 tag，不重复写对象体

### 4.7 `BinaryArchive` node 头

当前已证实的最小结构语义：

- node 起始处存在一个 `FTML::BinaryArchive` 头对象
- 头内紧跟 `FinalIndex`
- `FinalIndex` 指向同一 node 后半段的 class/module list section
- node 前部保存实际业务对象体与 pointer/tag 结构
- node 后部保存当前活动字典视图

## 5. 当前工程中的落地影响

当前代码里与 `FTMLCore` 结论直接对应的实现包括：

- `src/BinaryArchive.cpp`
- `src/Archive.cpp`
- `src/Compress.cpp`
- `src/SmartPointer.cpp`
- `src/TypeCompatibility.cpp`

这些实现使用 `FTMLCore` 结论完成了：

- 统一条目模型
- formatted payload 解包
- 字典驱动的运行时类型识别
- 部分缺字典情况下的 lineage 回退识别
- 对保存链的当前解释边界：
  - pointer 复用与首次对象写出是分开的
  - `FTMLCore` 负责 tag/object 分派，不直接决定 XMSE 业务对象是否“降级”
  - `BinaryArchive` 负责把 node 组织成“前部对象体 + node 头回填 + 后部字典 section”的物理布局

## 6. 样本实证

真实样本 `test.dat` 已经用来验证以下结论：

- class/module 字典可正常装载
- `SmartPointer::extract()` 可基于字典或 recovered lineage 识别 XMSE 动态类型
- `TextID` 读取结果可稳定进入 `ConfigurationSet.systemID` 等字段
- formatted `UInt32` payload 可被真实解包并供 `RawData` 数组读取使用
- 保存链逆向当前又进一步支持：
  - 同一 node 内完全可能出现“前部旧对象体 + 后部较新 pointer/tag/index”这类代际叠加

## 7. 未确认边界

以下内容当前不应写成确定结构：

- `Archive`、`BinaryArchive`、`TextArchive` 的完整对象成员布局
- `Compress` 内部全部工作缓冲区的精确偏移
- `TextID` 的全部辅助 API 与所有写路径
- `Array2D<T>` / `Array<T>` 通用模板的完整成员布局

当前项目只对“被样本和调用链双重证明”的那一部分做了实现。
