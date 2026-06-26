# Mini Rockwell PLC Engineering（迷你 Rockwell PLC 工程）

**从零开始、零依赖的 C 语言实现**，涵盖 Rockwell Automation PLC 工程基础，覆盖 Studio 5000 / ControlLogix 平台、EtherNet/IP（CIP）、安全 PLC、运动控制（Kinetix）、PlantPAx DCS 以及 FactoryTalk HMI。每个模块对应工业标准（IEC 61131-3、IEC 61508、ISA-88/95/101）及大学级控制系统课程，将自动化概念转化为可运行的 C 代码，实现工业理论与实践的桥接。

## 模块总览

| 模块 | 主题 | 参考课程 |
|------|------|----------|
| [mini-add-on-instruction-aoi](mini-add-on-instruction-aoi/) | 自定义指令（AOI）开发、IEC 61131-3 功能块、CIP 标签模型、源代码保护、版本追踪 | IEC 61131-3, RWTH Aachen PLC/SCADA |
| [mini-compactlogix-pointio](mini-compactlogix-pointio/) | CompactLogix 平台、Point I/O 分布式模块、模拟量/数字量信号处理、CIP I/O 连接生命周期、诊断 | ODVA CIP, Purdue ME 575 |
| [mini-factorytalk-view-hmi](mini-factorytalk-view-hmi/) | FactoryTalk View SE/ME、ISA-101 高性能 HMI 层级、ISA-18.2 报警状态机、数据记录、通信 | ISA-101, ISA-18.2, EEMUA 191 |
| [mini-guardlogix-safety-plc](mini-guardlogix-safety-plc/) | GuardLogix 安全 PLC、SIL 3/PL e、1oo2D 双通道架构、安全任务调度、安全 I/O 与网络 | IEC 61508, IEC 61511, CMU 24-654 |
| [mini-rockwell-ethernet-ip-cip](mini-rockwell-ethernet-ip-cip/) | EtherNet/IP 协议、CIP 对象模型、显式/隐式报文、连接管理、安全层、EPATH 路由 | ODVA CIP Vol 1–2, RWTH Aachen |
| [mini-rockwell-motion-kinetix](mini-rockwell-motion-kinetix/) | Kinetix 集成运动控制、CIP Motion 协议、S 曲线/梯形轨迹规划、伺服整定、电子凸轮/齿轮 | RWTH Aachen Motion Control, Purdue ECE 602 |
| [mini-rockwell-plantpax-dcs](mini-rockwell-plantpax-dcs/) | PlantPAx 分布式控制系统、ISA-88 批处理控制、ISA-95 设备层级、过程对象库（P_PIDE、P_ValveMO）、报警管理、历史站 | ISA-88, ISA-95, CMU 24-677 |
| [mini-studio5000-controllogix](mini-studio5000-controllogix/) | Studio 5000 / ControlLogix 1756 平台架构、Logix 执行模型、IEC 61131-3 合规、指令集、ALMA/ALMD 报警模型 | IEC 61131-3, Purdue ECE 602, RWTH Aachen |

## 设计理念

- **零外部依赖** — 纯 C（C99/C11），仅使用 `libc` 和 `libm`
- **模块自包含** — 每个目录自带 `Makefile`、`include/`、`src/`、`examples/`、`demos/`、`tests/`
- **标准到代码的映射** — 每个模块包含 `docs/` 目录，内有 IEC/ISA/ODVA 工业标准对齐说明
- **实用演示程序** — 循环扫描、CIP 报文路由、轨迹规划器、报警状态机、PID 控制回路等

## 构建方式

每个模块相互独立。进入模块目录后运行：

```bash
cd mini-add-on-instruction-aoi
make all    # 构建全部
make test   # 运行测试
```

需要 **GCC** 和 **GNU Make**。

## 项目结构

```
mini-rockwell-plc-engineering/
├── mini-add-on-instruction-aoi/   # 自定义指令（AOI）、IEC 61131-3 功能块
├── mini-compactlogix-pointio/     # CompactLogix 控制器与 Point I/O 分布式模块
├── mini-factorytalk-view-hmi/     # FactoryTalk View HMI、ISA-101、ISA-18.2 报警
├── mini-guardlogix-safety-plc/    # GuardLogix 安全 PLC、SIL 3 / PL e、IEC 61508
├── mini-rockwell-ethernet-ip-cip/ # EtherNet/IP 与通用工业协议（CIP）
├── mini-rockwell-motion-kinetix/  # Kinetix 伺服运动、轨迹规划、凸轮控制
├── mini-rockwell-plantpax-dcs/    # PlantPAx DCS、ISA-88/95、过程对象库
└── mini-studio5000-controllogix/  # Studio 5000、ControlLogix 1756、Logix 执行模型
```

## 许可证

MIT
