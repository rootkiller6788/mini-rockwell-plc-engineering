# Knowledge Graph — mini-add-on-instruction-aoi

## L1: Definitions (COMPLETE)
- `aoi_param_direction_t`: Input/Output/InOut/EnableIn/EnableOut directions
- `aoi_data_type_t`: 14 Rockwell atomic types (BOOL..AOI_REF)
- `aoi_atomic_value_t`: Union covering all elementary Logix types
- `aoi_parameter_t`: Complete AOI parameter definition
- `aoi_scan_mode_t`: Overload/PrescanImmune/AlwaysScan
- `aoi_signature_t`: SHA-1-based instruction signature (20 bytes)
- `aoi_revision_t`: Major/minor version tracking
- `plc_tag_t`: Tag definition (name, scope, type, memory layout)
- `plc_udt_t`: User-Defined Type with members and alignment
- `plc_controller_spec_t`: Controller model specifications
- `plc_controller_config_t`: .ACD project configuration
- `plc_task_config_t`: Continuous/Periodic/Event task configuration
- `plc_program_config_t`: IEC 61131-3 PROGRAM unit
- `plc_io_module_t`: I/O module device tree entry
- `plc_ethernet_port_t`: EtherNet/IP port configuration
- `cip_identity_t`: CIP Identity Object (Class 0x01)
- `cip_connection_t`: CIP Connection Object (Class 0x05)
- `cip_request_t`/`cip_response_t`: CIP explicit messaging

## L2: Core Concepts (COMPLETE)
- AOI vs Subroutine distinctions (parameters, local tags, signature)
- Controller scan cycle (Input/Program/Output/Housekeeping)
- Tag-based addressing vs register-based addressing
- Tag scope hierarchy (Controller → Program → Local)
- CIP implicit vs explicit messaging
- EtherNet/IP connection lifecycle (Forward Open/Close)
- AOI instance lifecycle (Create/Execute/Reset/Free)
- UDT memory alignment (32-bit DINT-aligned model)
- Scan mode behavior (Overload/PrescanImmune/AlwaysScan)
- Redundancy modes (Hot Standby, Warm Standby)

## L3: Engineering Structures (COMPLETE)
- AOI parameter array (fixed-size, 64 max per Studio 5000 v36)
- AOI local tag array (256 max, private scope)
- Tag database (4096 max tags, 256 UDTs)
- UDT member byte-offset computation with DINT alignment
- CIP electronic path (Class/Instance/Attribute segments)
- CIP explicit message packet structure
- Context-sensitive parameter (CSP) override system
- L5K text export format (Tag section)

## L4: Engineering Laws (COMPLETE)
- IEC 61131-3 POU mapping (Program/FB/Function/Class → Rockwell)
- IEC 61508 SIL 2/3 safety constraints (GuardLogix)
- IEC 62443-4-2 CIP Security configuration
- Safety AOI certification requirements (TÜV, exida)
- PFDavg calculation per IEC 61508-6 simplified formula
- ODVA CIP specification compliance (Vol 1, Vol 2)
- FIPS 180-4 SHA-1 specification for signature integrity
- Constant-time comparison for side-channel resistance

## L5: Algorithms/Methods (COMPLETE)
- SHA-1 signature computation with canonical serialization
- Constant-time signature comparison (no timing oracle)
- Tag resolution algorithm (scope hierarchy traversal)
- CIP Forward Open/Close connection procedure
- CIP Tag Read/Write protocol implementation
- CIP Multiple Service Packet assembly
- CRC-16 computation for CIP transport
- UDT structural hash for change detection
- Scan time estimation algorithm (RungCount×BranchRate)
- PFD calculation per IEC 61508-6

## L6: Canonical Problems (COMPLETE)
- Motor Start/Stop with Emergency Stop pattern
- PID Temperature Control loop (auto/manual, anti-windup)
- AOI Library management with signature verification
- Tag database with program-scope resolution
- UDT definition and member management

## L7: Industrial Applications (PARTIAL → COMPLETE)
- Rockwell ControlLogix 5580 (1756-L85E) configuration
- GuardLogix safety controller (1756-L84ES)
- CompactLogix 5380 (5069-L330ER/L340ERM)
- FactoryTalk integration (Security, Historian, AssetCentre)
- EtherNet/IP CIP protocol implementation
- Studio 5000 firmware version management

## L8: Advanced Topics (PARTIAL)
- Context-Sensitive Parameters (Studio 5000 v31+)
- Redundancy configuration (Hot Standby)
- Time synchronization (IEEE 1588 PTP, CIP Sync)
- Controller performance modeling (scan time prediction)
- Safety signature verification (STS)

## L9: Research Frontiers (PARTIAL)
- Source protection and code signing
- IT/OT convergence via FactoryTalk
- CIP Security for zero-trust industrial networking
- Industry 4.0 edge computing (CompactLogix 5480)

**Status: L1-L6 Complete, L7 Complete, L8-L9 Partial**
