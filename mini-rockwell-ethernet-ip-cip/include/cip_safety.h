/**
 * cip_safety.h - CIP Safety Protocol Definitions
 * Knowledge Domain: L2 Core Concepts, L4 Engineering Laws
 * Reference: ODVA CIP Safety Specification, IEC 61508 SIL 3
 *
 * Concepts: Safety Network Number (SNN), Safety Validator, Safety CRC,
 *           Safety state machine, Configuration signature,
 *           Safety watchdog timeout, GuardLogix safety connections
 * School: RWTH Aachen Safety Systems, ISA/IEC 61508/61511
 */
#ifndef CIP_SAFETY_H
#define CIP_SAFETY_H
#include <stdint.h>
#include "ethernet_ip_cip.h"

#define CIP_SAFETY_SNN_SIZE                 6u
#define CIP_SAFETY_MAX_CRC_LENGTH           8u
#define CIP_SAFETY_CONFIG_SIGNATURE_SIZE    4u
#define CIP_SAFETY_DEFAULT_TIMEOUT_MS       50u
#define CIP_SAFETY_MAX_WATCHDOG_MS          500u

#define CIP_SAFETY_CLASS_VALIDATOR          0x2Bu
#define CIP_SAFETY_CLASS_SAFETY_SUPERVISOR  0x2Cu
#define CIP_SAFETY_CLASS_SAFETY_VALIDATOR_CONNECTION 0x2Du

typedef enum {
    CIP_SAFETY_STATE_IDLE         = 0,
    CIP_SAFETY_STATE_VERIFYING    = 1,
    CIP_SAFETY_STATE_OPERATIONAL  = 2,
    CIP_SAFETY_STATE_FAULTED      = 3,
    CIP_SAFETY_STATE_CONFIGURING  = 4,
    CIP_SAFETY_STATE_SAFE_STATE   = 5
} cip_safety_state_t;

typedef enum {
    CIP_SAFETY_MODE_NORMAL        = 0,
    CIP_SAFETY_MODE_SAFE_OP       = 1,
    CIP_SAFETY_MODE_CONFIG_UNLOCK = 2
} cip_safety_mode_t;

typedef struct {
    uint8_t  snn[CIP_SAFETY_SNN_SIZE];
    uint8_t  snn_size;
    uint16_t safety_network_number;
    uint32_t node_id;
    uint32_t configuration_signature;
    uint32_t safety_crc;
    uint8_t  safety_state;
    uint8_t  safety_mode;
    uint32_t watchdog_timeout_ms;
    uint32_t elapsed_since_last_valid_ms;
} cip_safety_config_t;

void cip_safety_init(cip_safety_config_t *cfg);
int cip_safety_set_snn(cip_safety_config_t *cfg, const uint8_t snn_data[6]);
int cip_safety_validate_snn(const cip_safety_config_t *cfg);
int cip_safety_calculate_crc(const uint8_t *data, uint16_t data_len, uint32_t *crc_out);
int cip_safety_transition_state(cip_safety_config_t *cfg, cip_safety_state_t new_state);
int cip_safety_check_timeout(const cip_safety_config_t *cfg);
int cip_safety_verify_config_signature(const cip_safety_config_t *cfg, uint32_t expected_signature);
const char *cip_safety_state_string(cip_safety_state_t state);
const char *cip_safety_mode_string(cip_safety_mode_t mode);
#endif /* CIP_SAFETY_H */
