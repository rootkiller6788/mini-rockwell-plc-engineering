/**
 * cip_device_discovery.c - EtherNet/IP Device Discovery
 * Knowledge Domain: L5 Algorithms, L6 Canonical Problems, L7 Industrial Applications
 * Reference: ODVA Vol 2, Sec 2-4.3.2: ListIdentity
 *
 * Each function implements one independent knowledge point:
 *   1. cip_dd_init_scan()             - Initialize device discovery scan
 *   2. cip_dd_parse_identity()        - Parse ListIdentity response
 *   3. cip_dd_extract_vendor()        - Extract Vendor ID & vendor name
 *   4. cip_dd_extract_product_name()  - Extract product name string
 *   5. cip_dd_extract_revision()      - Extract firmware revision
 *   6. cip_dd_extract_ip()            - Extract IP address from sockaddr
 *   7. cip_dd_filter_by_vendor()      - Filter devices by Vendor ID
 *   8. cip_dd_sort_by_ip()            - Sort devices by IP address
 *   9. cip_dd_count_devices()         - Count discovered devices
 *
 * School: RWTH Aachen (device commissioning), Purdue ME 575 (network mgmt),
 *         Rockwell Studio 5000 (RSLinx device browsing)
 */
#include <string.h>
#include <stdio.h>
#include "../include/ethernet_ip_encap.h"

#define CIP_DD_MAX_DEVICES 256

/* Discovered device entry */
typedef struct {
    uint32_t ip_address;
    uint16_t vendor_id;
    uint16_t device_type;
    uint16_t product_code;
    uint8_t  major_rev;
    uint8_t  minor_rev;
    uint32_t serial_number;
    char     product_name[32];
    uint8_t  state;
    int      found;
} cip_dd_device_t;

/* Discovery scan state */
typedef struct {
    cip_dd_device_t devices[CIP_DD_MAX_DEVICES];
    uint16_t device_count;
    uint32_t scan_timeout_ms;
} cip_dd_scan_t;

/* Well-known Rockwell Automation Vendor IDs */
#define CIP_VENDOR_ROCKWELL    1u
#define CIP_VENDOR_SIEMENS     42u
#define CIP_VENDOR_SCHNEIDER   43u
#define CIP_VENDOR_OMRON       47u

/* -------------------------------------------------------------------------
 * cip_dd_init_scan - Initialize device discovery scan
 *
 * Knowledge: EtherNet/IP device discovery via ListIdentity broadcast.
 * A ListIdentity request is sent to UDP broadcast (255.255.255.255) or
 * directed to a specific device. All EIP devices respond with their
 * identity information.
 * Complexity: O(C) where C = CIP_DD_MAX_DEVICES (memset).
 * ------------------------------------------------------------------------- */
void cip_dd_init_scan(cip_dd_scan_t *scan, uint32_t timeout_ms)
{
    if (!scan) return;
    memset(scan, 0, sizeof(cip_dd_scan_t));
    scan->scan_timeout_ms = timeout_ms;
}

/* -------------------------------------------------------------------------
 * cip_dd_parse_identity - Parse a ListIdentity response into device record
 *
 * Knowledge: ListIdentity response decoding (ODVA Vol 2 Sec 2-4.3.2).
 * The response contains the device's Identity Object attributes:
 * vendor, device type, product code, revision, serial, product name.
 *
 * Returns: 0 = success, -1 = parse error.
 * Complexity: O(n) where n = product_name_len.
 * ------------------------------------------------------------------------- */
int cip_dd_parse_identity(cip_dd_device_t *dev, const uint8_t *data, uint16_t data_len)
{
    if (!dev || !data || data_len < 24) return -1;

    /* Skip CPF item header (4 bytes) */
    uint16_t offset = 4;

    /* Protocol version (2 bytes) */
    offset += 2;

    /* Option flags (2 bytes) */
    offset += 2;

    /* Socket address: sin_family (2), sin_port (2), sin_addr (4), sin_zero (8) */
    dev->ip_address  = (uint32_t)data[offset+4]
                     | ((uint32_t)data[offset+5] << 8)
                     | ((uint32_t)data[offset+6] << 16)
                     | ((uint32_t)data[offset+7] << 24);
    offset += 16;

    if (offset + 15 > data_len) return -2;

    dev->vendor_id     = (uint16_t)data[offset] | ((uint16_t)data[offset+1] << 8);
    dev->device_type   = (uint16_t)data[offset+2] | ((uint16_t)data[offset+3] << 8);
    dev->product_code  = (uint16_t)data[offset+4] | ((uint16_t)data[offset+5] << 8);
    dev->major_rev     = data[offset+6];
    dev->minor_rev     = data[offset+7];
    offset += 8;

    /* Status (2 bytes) */
    offset += 2;

    dev->serial_number = (uint32_t)data[offset]
                      | ((uint32_t)data[offset+1] << 8)
                      | ((uint32_t)data[offset+2] << 16)
                      | ((uint32_t)data[offset+3] << 24);
    offset += 4;

    /* Product name */
    uint8_t name_len = data[offset++];
    if (name_len > 31) name_len = 31;
    if (offset + name_len <= data_len) {
        memcpy(dev->product_name, &data[offset], name_len);
        dev->product_name[name_len] = '\0';
    }
    offset += name_len;

    dev->state = (offset < data_len) ? data[offset] : 0;
    dev->found = 1;

    return 0;
}

/* -------------------------------------------------------------------------
 * cip_dd_extract_vendor - Get vendor name from Vendor ID
 *
 * Knowledge: ODVA-assigned Vendor IDs.
 * Each CIP vendor has a unique 16-bit Vendor ID assigned by ODVA.
 * Rockwell Automation = 1 (the creator of CIP).
 *
 * Returns: vendor name string (static). Complexity: O(1).
 * ------------------------------------------------------------------------- */
const char *cip_dd_extract_vendor(uint16_t vendor_id)
{
    switch (vendor_id) {
    case CIP_VENDOR_ROCKWELL:  return "Rockwell Automation / Allen-Bradley";
    case CIP_VENDOR_SIEMENS:   return "Siemens AG";
    case CIP_VENDOR_SCHNEIDER: return "Schneider Electric";
    case CIP_VENDOR_OMRON:     return "Omron Corporation";
    case 0:                    return "ODVA (Open DeviceNet Vendor Assoc.)";
    default:                   return "Unknown Vendor";
    }
}

/* -------------------------------------------------------------------------
 * cip_dd_extract_product_name - Get product name
 *
 * Knowledge: CIP device identification from Identity Object.
 * Returns the product name string stored in the device record.
 * Complexity: O(1).
 * ------------------------------------------------------------------------- */
const char *cip_dd_extract_product_name(const cip_dd_device_t *dev)
{
    if (!dev || !dev->found) return "Unknown";
    return dev->product_name;
}

/* -------------------------------------------------------------------------
 * cip_dd_extract_revision - Get firmware revision as Major.Minor
 *
 * Knowledge: CIP firmware revision formatting.
 * The Identity Object stores revision as separate Major and Minor bytes.
 * Common format: "Major.Minor" (e.g., "32.11" for CompactLogix v32.011).
 *
 * Complexity: O(1).
 * ------------------------------------------------------------------------- */
void cip_dd_extract_revision(const cip_dd_device_t *dev,
                             uint8_t *major, uint8_t *minor)
{
    if (!dev || !major || !minor) return;
    *major = dev->major_rev;
    *minor = dev->minor_rev;
}

/* -------------------------------------------------------------------------
 * cip_dd_extract_ip - Get IPv4 address from device record
 *
 * Knowledge: CIP device IP extraction.
 * The ListIdentity response includes the device's IP in the sockaddr_in
 * structure. Returns IP in host byte order (big-endian from wire).
 *
 * Complexity: O(1).
 * ------------------------------------------------------------------------- */
uint32_t cip_dd_extract_ip(const cip_dd_device_t *dev)
{
    if (!dev) return 0;
    return dev->ip_address;
}

/* -------------------------------------------------------------------------
 * cip_dd_filter_by_vendor - Count devices matching a Vendor ID
 *
 * Knowledge: Device filtering for commissioning tools.
 * RSLinx and similar tools filter discovered devices by vendor to show
 * only relevant hardware (e.g., only Rockwell devices).
 *
 * Returns: count of matching devices. Complexity: O(N).
 * ------------------------------------------------------------------------- */
int cip_dd_filter_by_vendor(const cip_dd_scan_t *scan,
                            uint16_t vendor_id,
                            cip_dd_device_t *results, int max_results)
{
    if (!scan || !results) return 0;

    int count = 0;
    for (uint16_t i = 0; i < scan->device_count && count < max_results; i++) {
        if (scan->devices[i].found && scan->devices[i].vendor_id == vendor_id) {
            results[count] = scan->devices[i];
            count++;
        }
    }
    return count;
}

/* -------------------------------------------------------------------------
 * cip_dd_sort_by_ip - Sort devices by ascending IP address
 *
 * Knowledge: Device listing for HMI configuration tools.
 * Sorts discovered devices by IP for orderly display in device browsers
 * like RSLinx Classic or FactoryTalk Linx.
 *
 * Algorithm: Insertion sort (O(N^2)) ¡ª efficient for small N typical in
 * industrial networks (typically < 50 devices per subnet).
 * Complexity: O(N^2) where N = scan->device_count.
 * ------------------------------------------------------------------------- */
void cip_dd_sort_by_ip(cip_dd_scan_t *scan)
{
    if (!scan || scan->device_count <= 1) return;

    for (uint16_t i = 1; i < scan->device_count; i++) {
        cip_dd_device_t key = scan->devices[i];
        int j = (int)i - 1;
        while (j >= 0 && scan->devices[j].ip_address > key.ip_address) {
            scan->devices[j + 1] = scan->devices[j];
            j--;
        }
        scan->devices[j + 1] = key;
    }
}

/* -------------------------------------------------------------------------
 * cip_dd_count_devices - Count discovered devices
 *
 * Knowledge: Scan result summarization.
 * Returns the number of devices that responded to the ListIdentity scan.
 * Complexity: O(1).
 * ------------------------------------------------------------------------- */
uint16_t cip_dd_count_devices(const cip_dd_scan_t *scan)
{
    if (!scan) return 0;
    return scan->device_count;
}

/* -------------------------------------------------------------------------
 * cip_dd_add_device - Add a discovered device to the scan results
 *
 * Knowledge: Scan result accumulation.
 * Adds a parsed device to the scan table if space is available.
 *
 * Returns: 0 = added, -1 = scan full.
 * Complexity: O(1).
 * ------------------------------------------------------------------------- */
int cip_dd_add_device(cip_dd_scan_t *scan, const cip_dd_device_t *dev)
{
    if (!scan || !dev || !dev->found) return -1;
    if (scan->device_count >= CIP_DD_MAX_DEVICES) return -2;

    scan->devices[scan->device_count] = *dev;
    scan->device_count++;
    return 0;
}
