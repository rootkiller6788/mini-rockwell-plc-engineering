# Mini Rockwell PLC Engineering

A collection of **from-scratch, zero-dependency C implementations** of Rockwell Automation PLC engineering fundamentals, covering Studio 5000 / ControlLogix platform, EtherNet/IP (CIP), safety PLC, motion control (Kinetix), PlantPAx DCS, and FactoryTalk HMI. Each module maps to industrial standards (IEC 61131-3, IEC 61508, ISA-88/95/101) and university-level control systems courses, bridging theory and industrial practice by translating automation concepts into runnable C code.

## Sub-Modules

| Sub-Module | Topics | Key Courses |
|------------|--------|-------------|
| [mini-add-on-instruction-aoi](mini-add-on-instruction-aoi/) | Add-On Instruction (AOI) development, IEC 61131-3 function blocks, CIP tag model, source protection, revision tracking | IEC 61131-3, RWTH Aachen PLC/SCADA |
| [mini-compactlogix-pointio](mini-compactlogix-pointio/) | CompactLogix platform, Point I/O distributed modules, analog/digital signal processing, CIP I/O connection lifecycle, diagnostics | ODVA CIP, Purdue ME 575 |
| [mini-factorytalk-view-hmi](mini-factorytalk-view-hmi/) | FactoryTalk View SE/ME, ISA-101 high-performance HMI hierarchy, ISA-18.2 alarm state machine, data logging, communications | ISA-101, ISA-18.2, EEMUA 191 |
| [mini-guardlogix-safety-plc](mini-guardlogix-safety-plc/) | GuardLogix safety PLC, SIL 3/PL e, 1oo2D dual-channel architecture, safety task scheduling, safety I/O and networking | IEC 61508, IEC 61511, CMU 24-654 |
| [mini-rockwell-ethernet-ip-cip](mini-rockwell-ethernet-ip-cip/) | EtherNet/IP protocol, CIP object model, explicit/implicit messaging, connection manager, safety layer, EPATH routing | ODVA CIP Vol 1–2, RWTH Aachen |
| [mini-rockwell-motion-kinetix](mini-rockwell-motion-kinetix/) | Kinetix integrated motion, CIP Motion protocol, S-curve/trapezoidal trajectory planning, servo tuning, electronic camming/gearing | RWTH Aachen Motion Control, Purdue ECE 602 |
| [mini-rockwell-plantpax-dcs](mini-rockwell-plantpax-dcs/) | PlantPAx distributed control system, ISA-88 batch control, ISA-95 equipment hierarchy, process object library (P_PIDE, P_ValveMO), alarm management, historian | ISA-88, ISA-95, CMU 24-677 |
| [mini-studio5000-controllogix](mini-studio5000-controllogix/) | Studio 5000 / ControlLogix 1756 platform architecture, Logix execution model, IEC 61131-3 compliance, instruction set, ALMA/ALMD alarm model | IEC 61131-3, Purdue ECE 602, RWTH Aachen |

## Design Philosophy

- **Zero external dependencies** — pure C (C99/C11), only `libc` and `libm`
- **Self-contained modules** — each directory has its own `Makefile`, `include/`, `src/`, `examples/`, `demos/`, `tests/`
- **Standard-to-code mapping** — every module includes `docs/` with alignment notes to IEC/ISA/ODVA industrial standards
- **Practical demos** — cycle scans, CIP message routers, trajectory planners, alarm state machines, PID control loops, and more

## Building

Each module is standalone. Navigate to a module directory and run:

```bash
cd mini-add-on-instruction-aoi
make all    # build everything
make test   # run tests
```

Requires **GCC** and **GNU Make**.

## Project Structure

```
mini-rockwell-plc-engineering/
├── mini-add-on-instruction-aoi/   # Add-On Instructions (AOI), IEC 61131-3 function blocks
├── mini-compactlogix-pointio/     # CompactLogix controllers & Point I/O distributed modules
├── mini-factorytalk-view-hmi/     # FactoryTalk View HMI, ISA-101, ISA-18.2 alarms
├── mini-guardlogix-safety-plc/    # GuardLogix safety PLC, SIL 3 / PL e, IEC 61508
├── mini-rockwell-ethernet-ip-cip/ # EtherNet/IP and Common Industrial Protocol (CIP)
├── mini-rockwell-motion-kinetix/  # Kinetix servo motion, trajectory planning, camming
├── mini-rockwell-plantpax-dcs/    # PlantPAx DCS, ISA-88/95, process object library
└── mini-studio5000-controllogix/  # Studio 5000, ControlLogix 1756, Logix execution model
```

## License

MIT
