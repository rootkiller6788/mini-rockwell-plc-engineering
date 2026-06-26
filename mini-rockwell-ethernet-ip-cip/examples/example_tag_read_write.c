#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../include/cip_message.h"
#include "../include/cip_object.h"

/* Forward declarations from cip_tag_operations.c */
int cip_tag_build_read_request(const char *tag_name, uint16_t element_count,
                               uint8_t *req_buffer, uint16_t buffer_size);
int cip_tag_build_write_request(const char *tag_name, uint16_t type_code,
                                uint16_t element_count,
                                const uint8_t *write_data, uint16_t data_len,
                                uint8_t *req_buffer, uint16_t buffer_size);
int cip_tag_validate_name(const char *tag_name);
int cip_tag_encode_symbol(const char *tag_name, uint8_t *epath_buf, uint16_t buf_size);
int cip_tag_decode_value(const uint8_t *data, uint16_t data_len,
                         uint16_t type_code, void *value_out, uint16_t *value_size);
uint16_t cip_tag_get_type_code(const char *type_name);

int main(void) {
    printf("=== Example: ControlLogix Tag Read/Write ===

");

    /* Tag name validation */
    const char *tags[] = {"MyCounter", "_privateVar", "123bad", "Program:Main.Routine"};
    for (int i = 0; i < 4; i++) {
        printf("Tag '%s': %s
", tags[i],
               cip_tag_validate_name(tags[i]) ? "VALID" : "INVALID");
    }

    printf("
--- Build Tag Read Request ---
");
    uint8_t req_buf[256];
    int n = cip_tag_build_read_request("MyDINT", 1, req_buf, sizeof(req_buf));
    printf("Read request for 'MyDINT': %d bytes
", n);
    printf("  Service: 0x%02X
", req_buf[0]);

    printf("
--- Build Tag Write Request ---
");
    int32_t write_value = 42;
    uint16_t type_code = cip_tag_get_type_code("DINT");
    printf("DINT type code: 0x%04X
", type_code);
    n = cip_tag_build_write_request("MyDINT", type_code, 1,
                                    (uint8_t*)&write_value, 4,
                                    req_buf, sizeof(req_buf));
    printf("Write request for 'MyDINT' = 42: %d bytes
", n);

    printf("
--- Tag Value Decoding ---
");
    uint8_t raw_data[4] = {0x2A, 0x00, 0x00, 0x00}; /* DINT = 42 */
    int32_t decoded = 0;
    uint16_t val_size;
    cip_tag_decode_value(raw_data, 4, 0x00C4, &decoded, &val_size);
    printf("Decoded DINT: %d (%u bytes)
", decoded, val_size);

    printf("
--- EPATH Symbolic Encoding ---
");
    uint8_t epath[64];
    n = cip_tag_encode_symbol("MyTag", epath, 64);
    printf("Encoded 'MyTag' as ANCI Extended Symbolic: %d bytes
", n);
    printf("  Segment byte: 0x%02X
", epath[0]);
    if (n > 1) {
        printf("  Name bytes: ");
        for (int i = 1; i < n && i < 10; i++) printf("%c", epath[i]);
        printf("
");
    }

    printf("
Tag operations example complete.
");
    return 0;
}
