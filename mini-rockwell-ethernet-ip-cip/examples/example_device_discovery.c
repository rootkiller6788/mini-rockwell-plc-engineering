#include <stdio.h>
#include <string.h>
#include "../include/ethernet_ip_encap.h"
#include "../include/cip_object.h"

int main(void) {
    printf("=== Example: EtherNet/IP Device Discovery ===
");
    printf("Simulating ListIdentity scan on 192.168.1.0/24...

");

    /* Simulate discovering 3 devices */
    cip_identity_object_t dev1, dev2, dev3;

    /* 1756-L83E ControlLogix at 192.168.1.10 */
    cip_obj_identity_init(&dev1, 1, 0x0E, 0x100, 32, 11, 0x12345678, "1756-L83E/B");
    printf("Device 1: %s (Vendor=%d, Rev=%d.%d, Serial=%u)
",
           dev1.product_name, dev1.vendor_id,
           dev1.major_revision, dev1.minor_revision, dev1.serial_number);
    printf("  Class: %s, Type: 0x%04X
",
           cip_obj_class_name(CIP_CLASS_IDENTITY), dev1.device_type);

    /* 1734-AENTR Point I/O at 192.168.1.11 */
    cip_obj_identity_init(&dev2, 1, 0x0C, 0x200, 5, 1, 0x87654321, "1734-AENTR/C");
    printf("Device 2: %s (Vendor=%d, Rev=%d.%d, Serial=%u)
",
           dev2.product_name, dev2.vendor_id,
           dev2.major_revision, dev2.minor_revision, dev2.serial_number);

    /* 525 VFD at 192.168.1.12 */
    cip_obj_identity_init(&dev3, 1, 0x02, 0x300, 7, 0, 0xAABBCCDD, "PowerFlex 525");
    printf("Device 3: %s (Vendor=%d, Rev=%d.%d, Serial=%u)
",
           dev3.product_name, dev3.vendor_id,
           dev3.major_revision, dev3.minor_revision, dev3.serial_number);

    printf("
Device discovery complete. 3 Rockwell devices found.
");
    return 0;
}
