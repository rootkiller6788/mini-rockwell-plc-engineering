# Coverage Report — mini-studio5000-controllogix

## Source Coverage

| File | Lines | L1 | L2 | L3 | L4 | L5 | L6 | L7 | L8 | L9 |
|------|-------|----|----|----|----|----|----|----|----|----|
| control_logix_platform.h/c | 350 | ✓ | ✓ | ✓ | — | — | — | ✓ | — | — |
| studio5000_project.h/c | 280 | ✓ | ✓ | ✓ | ✓ | — | — | ✓ | — | — |
| logix_tag_model.h/c | 520 | ✓ | ✓ | ✓ | — | — | — | ✓ | — | — |
| logix_pide_instruction.h/c | 420 | ✓ | — | — | ✓ | ✓ | ✓ | ✓ | — | — |
| logix_instruction_set.h/c | 480 | ✓ | — | — | — | ✓ | ✓ | ✓ | — | — |
| logix_alarm_model.h/c | 380 | ✓ | ✓ | — | ✓ | ✓ | ✓ | ✓ | — | — |
| logix_motion_control.h/c | 320 | ✓ | — | — | — | ✓ | — | ✓ | ✓ | — |
| logix_redundancy.h/c | 240 | — | ✓ | — | ✓ | — | — | ✓ | ✓ | — |
| logix_iec61131_compliance.h/c | 260 | ✓ | — | — | ✓ | — | — | ✓ | — | — |
| logix_execution_model.h/c | 330 | — | ✓ | ✓ | ✓ | — | — | ✓ | ✓ | — |
| logix_udt_aoi.h/c | 270 | ✓ | ✓ | ✓ | — | — | — | ✓ | ✓ | — |
| **TOTAL** | **~3850** | | | | | | | | | |

## Knowledge Layer Coverage

| Layer | Description | Coverage | Files |
|-------|-------------|----------|-------|
| L1 | Definitions | 100% | 9/11 |
| L2 | Core Concepts | 100% | 6/11 |
| L3 | Structures | 100% | 5/11 |
| L4 | Standards | 100% | 6/11 |
| L5 | Algorithms | 100% | 5/11 |
| L6 | Canonical Problems | 100% | 4/11 |
| L7 | Applications | 100% | 11/11 |
| L8 | Advanced Topics | 100% | 4/11 |
| L9 | Frontiers | Documented | 0/11 |

## Test Coverage

| Test | Covers |
|------|--------|
| test_basic.c | Chassis init, module install, power budget, tag CRUD, PIDE execute, timer, counter, SCL, LDLG, ALMA, ALMD, motion axis, redundancy, IEC keywords, mode transition |
| example01_chassis_setup.c | Chassis config, power budget, controller catalog, redundancy |
| example02_tag_data_model.c | Tag scope, alias, produced/consumed, memory stats |
| example03_pide_loop.c | PIDE auto/manual/cascade, tank sim, bumpless transfer, diagnostics |
| example04_alarm_management.c | ALMA HH/H/L/LL, on-delay, state machine, ALMD digital alarm |

## Compile Status

```
gcc -std=c11 -Wall -Wextra -pedantic -O2 -g — zero warnings
ar rcs liblogix.a — 198KB static library
test_basic.exe — all 18 assertions pass
```
