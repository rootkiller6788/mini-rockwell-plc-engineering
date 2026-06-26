#ifndef ETHERNET_IP_CIP_H
#define ETHERNET_IP_CIP_H
#include <stdint.h>
#include <stddef.h>
#define CIP_PROTOCOL_VERSION_MAJOR  1u
#define CIP_PROTOCOL_VERSION_MINOR  33u
#define CIP_PROTOCOL_VERSION        ((CIP_PROTOCOL_VERSION_MAJOR << 8) | CIP_PROTOCOL_VERSION_MINOR)
#define EIP_ENCAP_VERSION           1u
#define EIP_TCP_PORT_EXPLICIT       44818u
#define EIP_UDP_PORT_IMPLICIT       2222u
#define EIP_MULTICAST_BASE_HI       239u
#define EIP_MULTICAST_BASE_LO       192u
#define CIP_CLASS_IDENTITY                  0x01u
#define CIP_CLASS_MESSAGE_ROUTER            0x02u
#define CIP_CLASS_ASSEMBLY                  0x04u
#define CIP_CLASS_CONNECTION_MANAGER        0x06u
#define CIP_CLASS_PARAMETER                 0x0Fu
#define CIP_CLASS_DISCRETE_INPUT_POINT      0x08u
#define CIP_CLASS_DISCRETE_OUTPUT_POINT     0x09u
#define CIP_CLASS_ANALOG_INPUT_POINT        0x0Au
#define CIP_CLASS_ANALOG_OUTPUT_POINT       0x0Bu
#define CIP_CLASS_TCPIP_INTERFACE           0xF5u
#define CIP_CLASS_ETHERNET_LINK             0xF6u
#define CIP_CLASS_POSITION_CONTROLLER       0x25u
#define CIP_CONN_TYPE_EXCLUSIVE_OWNER       0x00u
#define CIP_CONN_TYPE_INPUT_ONLY            0x01u
#define CIP_CONN_TYPE_LISTEN_ONLY           0x02u
#define CIP_CONN_TYPE_REDUNDANT_OWNER       0x03u
#define CIP_MAX_CONNECTED_MSG_SIZE          2000u
#define CIP_MAX_UCMM_MSG_SIZE               502u
#define EIP_MAX_FRAME_SIZE                  1436u
#define CIP_MAX_CONNECTION_SIZE             65511u
#define CIP_TRANSPORT_CLASS_0               0x00u
#define CIP_TRANSPORT_CLASS_1               0x01u
#define CIP_TRANSPORT_CLASS_2               0x02u
#define CIP_TRANSPORT_CLASS_3               0x03u
#define CIP_TRIGGER_CYCLIC                  0x00u
#define CIP_TRIGGER_CHANGE_OF_STATE         0x01u
#define CIP_TRIGGER_APPLICATION             0x02u
#endif
