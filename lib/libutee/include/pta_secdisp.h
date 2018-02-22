/*
 * Copyright (C) Microsoft. All rights reserved
 */

#ifndef __PTA_SECDISP_H
#define __PTA_SECDISP_H

/*
 * Interface to the secure display pseudo-TA.
 */

                           
// {5FB6F3D5-5592-41DB-B24D-2605B1603E56}
#define PTA_SECDISP_UUID { 0x5fb6f3d5, 0x5592, 0x41db, { \
                           0xb2, 0x4d, 0x26, 0x5, 0xb1, 0x60, 0x3e, 0x56 } }

typedef enum _SECDISP_DRIVERS {
    SECDIP_DRIVER_INVALID = 0,
    SECDIP_DRIVER_ILI9340,
} SECDISP_DRIVERS;

typedef enum _SECDISP_COLORS {
    SECDISP_BLACK = 0,
    SECDISP_BLUE,
    SECDISP_RED,
    SECDISP_GREEN,
    SECDISP_CYAN,
    SECDISP_MAGENTA,
    SECDISP_YELLOW,
    SECDISP_WHITE,

    SECDISP_CURRENT = 0xFFFF,
} SECDISP_COLORS;

typedef enum _SECDISP_ROTATION {
    SECDISP_0 = 0,
    SECDISP_90,
    SECDISP_180,
    SECDISP_270,
} SECDISP_ROTATION;

typedef struct _SECDISP_INFORMATION {
    uint16_t height;
    uint16_t width;
    uint16_t bits_per_pixel;
} SECDISP_INFORMATION;

enum SECDISP_PTA_COMMANDS {
 
    /*
     * Description: Secure display initialization
     * 
     * [in]  params[0].value.a: display driver (SECDISP_DRIVERS)
     * [out] params[1].memref: display information (SECDISP_INFORMATION)
     */
    PTA_SECDISP_INIT  = 0,

    /*
    * Description: Draw a single pixel
    *
    * [in] params[0].value.a: color (SECDISP_COLORS)
    */
    PTA_SECDISP_CLEAR = 1,

    /*
     * Description: Draw a single pixel
     * 
     * [in] params[0].value.a: x (16 bit)
     * [in] params[0].value.b: y (16 bit)
     * [in] params[1].value.a: color (SECDISP_COLORS)
     */
    PTA_SECDISP_DRAW_PIXEL = 2,

    /*
     * Description: Draw a vertical line
     * 
     * [in] params[0].value.a: x (16 bit)
     * [in] params[0].value.b: y (16 bit)
     * [in] params[1].value.a: height (16 bit)
     * [in] params[2].value.a: color (SECDISP_COLORS)
     */
    PTA_SECDISP_DRAW_VLINE = 3,

    /*
     * Description: Draw a horizontal line
     *
     * [in] params[0].value.a: x (16 bit)
     * [in] params[0].value.b: y (16 bit)
     * [in] params[1].value.a: width (16 bit)
     * [in] params[2].value.a: color (SECDISP_COLORS)
     */
    PTA_SECDISP_DRAW_HLINE = 4,

    /*
     * Description: Draw a rectangle
     *
     * [in] params[0].value.a: x (16 bit)
     * [in] params[0].value.b: y (16 bit)
     * [in] params[1].value.a: width (16 bit)
     * [in] params[1].value.b: height (16 bit)
     * [in] params[2].value.a: color (SECDISP_COLORS)
     */
    PTA_SECDISP_FILL_RECT = 5,

    /*
     * Description: Set display rotation 
     *
     * [in] params[0].value.a: Rotation (SECDISP_ROTATION)
     */
    PTA_SECDISP_SET_ROTATION = 6,

    /*
     * Description: Invert display
     *
     * [in] params[0].value.a: Invert (1), otherwise 0
     */
    PTA_SECDISP_INVERT_DISPLAY = 7,

    /*
     * Description: Set text position
     *
     * [in] params[0].value.a: x (16 bit)
     * [in] params[0].value.b: y (16 bit)
     */
    PTA_SECDISP_SET_TEXT_POS = 8,

    /*
     * Description: Draw text
     *
     * [in] params[0].value.a: x (16 bit), or 0xFFFF for keeping current position
     * [in] params[0].value.b: y (16 bit), or 0xFFFF for keeping current position
     * [in] params[1].value.a: color (SECDISP_COLORS)
     * [in] params[2].memref: String chars
     */
    PTA_SECDISP_DRAW_TEXT = 9,
};
#endif /* __PTA_SECDISP_H */
