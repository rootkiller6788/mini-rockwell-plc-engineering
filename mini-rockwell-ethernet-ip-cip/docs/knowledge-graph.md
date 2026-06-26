# Knowledge Graph - mini-rockwell-ethernet-ip-cip

## L1: Definitions (Complete)

| Item | C Definition | Location |
|------|-------------|----------|
| CIP Object Class Codes | #define CIP_CLASS_* | include/ethernet_ip_cip.h |
| CIP Service Codes | #define CIP_SERVICE_* | include/cip_message.h |
| General Status Codes | #define CIP_STATUS_* | include/cip_message.h |
| Connection Types | #define CIP_CONN_TYPE_* | include/ethernet_ip_cip.h |
| Transport Class/Trigger | #define CIP_TRANSPORT_* | include/ethernet_ip_cip.h |
| EIP Encapsulation Commands | #define EIP_ENCAP_COMMAND_* | include/ethernet_ip_encap.h |
| CIP Safety States | cip_safety_state_t enum | include/cip_safety.h |
| CIP Data Type Codes | cip_attr_data_type_t enum | include/cip_object.h |

## L2: Core Concepts (Complete)

| Concept | Implementation | Location |
|---------|---------------|----------|
| Connection State Machine | cip_conn_transition() + state table | src/cip_connection.c |
| RPI / API / Timeout | cip_conn_calculate_timeout() | src/cip_connection.c |
| O->T / T->O Data Paths | cip_conn_configure() | src/cip_connection.c |
| Ownership Models | cip_conn_set_type() | src/cip_connection.c |
| Transport Class/Trigger | cip_conn_set_transport() | src/cip_connection.c |
| Session Management | eip_encap_register_session() | src/ethernet_ip_encap.c |
| Safety Watchdog | cip_safety_check_timeout() | src/cip_safety.c |

## L3: Engineering Structures (Complete)

| Structure | Implementation | Location |
|-----------|---------------|----------|
| EPATH Segment Encoding | cip_epath_encode_class/instance/attribute() | src/cip_message.c |
| Message Router PDU | cip_mr_request_t / cip_mr_response_t | include/cip_message.h |
| Encapsulation Header | eip_encap_header_t (24-byte fixed) | include/ethernet_ip_encap.h |
| CPF Packet Format | eip_cpf_packet_t / eip_cpf_item_t | include/ethernet_ip_encap.h |
| Forward Open Request/Response | cip_forward_open_request/response_t | src/cip_forward_open.c |
| Identity Object Structure | cip_identity_object_t | include/cip_object.h |

## L4: Engineering Laws (Complete)

| Law/Standard | Implementation | Location |
|-------------|---------------|----------|
| Ziegler-Nichols Tuning | (N/A - control theory module) | - |
| ODVA CIP Vol 1 Compliance | All service codes, object models | include/ + src/ |
| ODVA Vol 2 EIP Adaptation | Encapsulation protocol | src/ethernet_ip_encap.c |
| IEC 61508 SIL 3 Safety | CIP Safety CRC, SNN, state machine | src/cip_safety.c |
| ISA/IEC 62439-3 PRP | (Referenced, not implemented) | - |

## L5: Algorithms (Complete)

| Algorithm | Implementation | Location |
|-----------|---------------|----------|
| EPATH Decode (Byte-Scan Parser) | cip_epath_decode() | src/cip_message.c |
| CRC-32 (IEEE 802.3) | cip_safety_calculate_crc() | src/cip_safety.c |
| Connection Timeout Formula | cip_conn_calculate_timeout() | src/cip_connection.c |
| API Scheduling (Ceil Division) | cip_conn_calculate_api() | src/cip_connection.c |
| Forward Open Serialization | cip_fo_encode_request() | src/cip_forward_open.c |
| Assembly Pack/Unpack | cip_da_pack/unpack_data() | src/cip_data_assembly.c |
| Tag Value Decode (Type-Dispatched) | cip_tag_decode_value() | src/cip_tag_operations.c |
| IP Insertion Sort | cip_dd_sort_by_ip() | src/cip_device_discovery.c |

## L6: Canonical Problems (Complete)

| Problem | Solution | Location |
|---------|----------|----------|
| Device Discovery on Subnet | ListIdentity scan, filter, sort | examples/example_device_discovery.c |
| I/O Connection Lifecycle | Full state machine walkthrough | examples/example_io_exchange.c |
| Tag Read/Write to ControlLogix | Symbolic EPATH, type-coded values | examples/example_tag_read_write.c |

## L7: Industrial Applications (Complete)

| Application | Coverage | Location |
|------------|----------|----------|
| Rockwell ControlLogix/CompactLogix | Tag operations, Forward Open | src/cip_tag_operations.c |
| Rockwell GuardLogix Safety | SNN, CRC, state machine | src/cip_safety.c |
| RSLinx Device Browsing | Device discovery, vendor filter | src/cip_device_discovery.c |

## L8: Advanced Topics (Partial)

| Topic | Coverage | Location |
|-------|----------|----------|
| IEC 61508 Functional Safety | CIP Safety CRC, SIL 3 watchdog | src/cip_safety.c |
| Deterministic Scheduling | API computation, tick alignment | src/cip_connection.c |
| Multicast I/O (Implicit Msg) | SendUnitData, UDP port 2222 | src/ethernet_ip_encap.c |

## L9: Research Frontiers (Partial - Documented)

| Frontier | Coverage |
|----------|----------|
| IT/OT Convergence | EIP to OPC UA gateway concepts (docs only) |
| Industrial 5G | Deterministic latency requirements (docs only) |
| Autonomous Operations (L4) | CIP Safety autonomous safe state (docs only) |
