# mini-rockwell-ethernet-ip-cip

**EtherNet/IP & Common Industrial Protocol (CIP) Implementation**

Reference: ODVA Specification Volumes 1 & 2
Coverage: L1-L6 Complete, L7 Complete, L8 Partial, L9 Partial

---

## Module Status: COMPLETE

- L1-L6: Complete
- L7: Complete (3 applications: ControlLogix tags, GuardLogix safety, RSLinx discovery)
- L8: Partial (3/5 advanced topics: CIP Safety SIL3, deterministic scheduling, multicast I/O)
- L9: Partial (documented: IT/OT convergence, Industrial 5G, L4 autonomous)

Line count (include/ + src/): 3634 lines
Compilation: make all passes

---

## Nine-Layer Knowledge Coverage Summary

| Level | Name | Status | Key Items |
|-------|------|--------|-----------|
| L1 | Definitions | Complete | 12 class codes, 17 services, 34 status codes, 4 conn types |
| L2 | Core Concepts | Complete | State machine, RPI/API, ownership, transport, safety watchdog |
| L3 | Engineering Structures | Complete | EPATH, MR PDU, encap header, CPF, Forward Open, 6 object types |
| L4 | Engineering Laws | Complete | ODVA CIP Vol 1/2, IEC 61508 SIL 3, IEEE 802.3 CRC |
| L5 | Algorithms | Complete | EPATH parser, CRC-32, API scheduling, serialization, pack/unpack |
| L6 | Canonical Problems | Complete | Device discovery, I/O lifecycle, tag read/write |
| L7 | Industrial Applications | Complete | Rockwell ControlLogix, GuardLogix, RSLinx |
| L8 | Advanced Topics | Partial | SIL 3 safety, deterministic scheduling, multicast |
| L9 | Research Frontiers | Partial | IT/OT, 5G, L4 autonomous (documented) |

---

## Core Definitions (L1)

### CIP Object Class Codes
- Identity (0x01), Message Router (0x02), Assembly (0x04), Connection Manager (0x06)
- TCP/IP Interface (0xF5), Ethernet Link (0xF6), Position Controller (0x25)
- Discrete/Analog I/O Point objects

### CIP Service Codes
- Get_Attribute_Single (0x0E), Set_Attribute_Single (0x10), Reset (0x05)
- Forward Open (0x54), Forward Close (0x4E)

### Connection Types
- Exclusive Owner, Input Only, Listen Only, Redundant Owner

---

## Core Theorems

### Connection Timeout Theorem (ODVA Vol 1, Sec 3-3.8.3)

Allows up to 3 consecutive lost packets before declaring timeout.

### API Ceil-Division Scheduling

Aligns connection production to common time base for deterministic scheduling.

### CRC-32 Safety Integrity


---

## Core Algorithms (L5)

| Algorithm | Complexity | Description |
|-----------|------------|-------------|
| EPATH Encode/Decode | O(n) | Byte-scan parser for CIP logical segments |
| CRC-32 | O(n) | Table-driven Ethernet CRC computation |
| Forward Open Serialization | O(1) | 50+ byte fixed-format serialization |
| Assembly Pack/Unpack | O(n*m) | Multi-member data marshaling |
| Tag Value Decode | O(1) | Type-dispatched value extraction |
| IP Insertion Sort | O(N^2) | Device sort for small (N<50) networks |

---

## Ninth-School Curriculum Mapping

| School | Course | Topic |
|--------|--------|-------|
| RWTH Aachen | Industrial Control Systems | EtherNet/IP protocol stack |
| Purdue | ME 575 Industrial Control | Deterministic network scheduling |
| CMU | 24-677 Adv Ctrl Systems | Industrial protocol stacks |
| ISA/IEC | 61131-3, 61508, 61511, 62439-3 | International standards |
| MIT | 6.302 Feedback Systems | Real-time control over networks |
| Berkeley | ME233 Advanced Control | Networked control systems |
| Georgia Tech | ECE 6550 | Safety-critical networked control |

---

## File Structure



---

## Building and Testing

gcc -Wall -Wextra -std=c11 -I./include -O2 -c src/cip_connection.c -o src/cip_connection.o
gcc -Wall -Wextra -std=c11 -I./include -O2 -c src/cip_data_assembly.c -o src/cip_data_assembly.o
gcc -Wall -Wextra -std=c11 -I./include -O2 -c src/cip_device_discovery.c -o src/cip_device_discovery.o
gcc -Wall -Wextra -std=c11 -I./include -O2 -c src/cip_forward_open.c -o src/cip_forward_open.o
gcc -Wall -Wextra -std=c11 -I./include -O2 -c src/cip_message.c -o src/cip_message.o
gcc -Wall -Wextra -std=c11 -I./include -O2 -c src/cip_object.c -o src/cip_object.o
gcc -Wall -Wextra -std=c11 -I./include -O2 -c src/cip_safety.c -o src/cip_safety.o
gcc -Wall -Wextra -std=c11 -I./include -O2 -c src/cip_services.c -o src/cip_services.o
make: Nothing to be done for 'test'.
gcc -Wall -Wextra -std=c11 -I./include -O2 -c src/cip_services.c -o src/cip_services.o
rm -f src/*.o
rm -f tests/*.test
rm -f examples/*.exe
