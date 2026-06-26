# Course Tree — mini-compactlogix-pointio

## Prerequisite Knowledge Dependencies

`
                        ┌─────────────────────────┐
                        │ mini-compactlogix-pointio│
                        │  (L7: Rockwell PLC I/O)  │
                        └───────────┬─────────────┘
                                    │
              ┌─────────────────────┼─────────────────────┐
              │                     │                     │
    ┌─────────▼─────────┐  ┌────────▼────────┐  ┌────────▼──────────┐
    │ mini-rockwell-    │  │ mini-rockwell-   │  │ mini-studio5000-  │
    │ ethernet-ip-cip   │  │ plantpax-dcs     │  │ controllogix      │
    │ (L3: CIP proto)   │  │ (L7: DCS on AB)  │  │ (L7: CPU config)  │
    └─────────┬─────────┘  └──────────────────┘  └───────────────────┘
              │
    ┌─────────▼─────────┐
    │ mini-plc-          │
    │ iec61131-fundamentals│
    │ (L4: IEC 61131)    │
    └─────────┬─────────┘
              │
    ┌─────────▼─────────┐
    │ mini-industrial-   │
    │ measurement-actuator│
    │ (L1: I/O basics)   │
    └────────────────────┘
`

## Internal Module Dependencies

`
pointio_types.h          ← Base types (no dependencies)

pointio_module.h         ← depends on pointio_types.h
pointio_digital.h        ← depends on pointio_types.h, pointio_module.h
pointio_analog.h         ← depends on pointio_types.h, pointio_module.h
pointio_connection.h     ← depends on pointio_types.h, pointio_module.h
pointio_diagnostics.h    ← depends on pointio_types.h, pointio_module.h
pointio_scanner.h        ← depends on pointio_types.h, pointio_module.h,
                            pointio_connection.h

src/pointio_module.c     ← depends on pointio_module.h
src/pointio_digital_io.c ← depends on pointio_digital.h
src/pointio_analog_io.c  ← depends on pointio_analog.h
src/pointio_connection.c ← depends on pointio_connection.h
src/pointio_scanner.c    ← depends on pointio_scanner.h, all other headers
src/pointio_fault.c      ← depends on pointio_diagnostics.h
src/pointio_signal_proc.c← depends on pointio_types.h (standalone DSP)
`

## Knowledge Progression

1. **L1** (pointio_types.h): Hardware definitions, catalog numbers, status codes
2. **L2** (pointio_module.h + connection): Architecture concepts, configuration
3. **L3** (module/connection structs): Engineering data structures and state machines
4. **L4** (power budget, CIP spec): Standards compliance and engineering laws
5. **L5** (digital, analog, scanner, signal_proc): Algorithms and methods
6. **L6** (examples, fault troubleshooting): Canonical industrial problems
7. **L7** (CPU database, Studio 5000 mapping): Rockwell-specific applications
8. **L8** (signal_proc advanced): Statistical signal processing and anomaly detection
9. **L9** (knowledge-graph): Industry frontier documentation
