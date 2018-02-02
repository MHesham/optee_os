/*
 * Copyright (C) Microsoft. All rights reserved
 */

#ifndef __PTA_GT511C3_H
#define __PTA_GT511C3_H

/*
 * Interface to the GT511C3 pseudo-TA.
 */


// {BD7B756D-CAC7-4E8F-8189-C69A1A21DE85}
#define PTA_GT511C3_UUID { 0xbd7b756d, 0xcac7, 0x4e8f, { \
                           0x81, 0x89, 0xc6, 0x9a, 0x1a, 0x21, 0xde, 0x85 } }

/*
 * GT511C3 device configuration
 */
struct _GT511C3_DeviceConfig {
    paddr_t uart_base_pa;
    uint32_t uart_clock_hz;
    uint32_t baud_rate;
} __packed;
typedef struct _GT511C3_DeviceConfig GT511C3_DeviceConfig;

/*
 * GT511C3 device information
 */
#define GT511C3_SERIAL_NUMBER_SIZE 16

struct _GT511C3_DeviceInfo {
    uint32_t firmware_version;
    uint32_t iso_area_max_size;
    union {
        uint8_t  b[GT511C3_SERIAL_NUMBER_SIZE/sizeof(uint8_t)];
        uint16_t w[GT511C3_SERIAL_NUMBER_SIZE/sizeof(uint16_t)];
        uint32_t d[GT511C3_SERIAL_NUMBER_SIZE/sizeof(uint32_t)];
    } device_serial_number;
} __packed;
typedef struct _GT511C3_DeviceInfo GT511C3_DeviceInfo;

/*
 * Description: Device initialization
 * 
 * [in]  params[0].memref: Device configuration (GT511C3_DeviceConfig)
 * [out] params[1].memref: Device information (GT511C3_DeviceInfo)
 */ 
#define PTA_GT511C3_INIT    0

/*
 * Description: Execute a command
 * 
 * [in]  params[0].value.a: GT511C3 command 
 * [in]  params[0].value.b: Command parameter
 * [out] params[0].value.b: Command status (GT511C3 status code)
 * [in  optional] params[1].memref: Command input
 * [out optional] params[2].memref: Command output
 */ 
#define PTA_GT511C3_EXEC    1

#endif /* __PTA_GT511C3_H */
