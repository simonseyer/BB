/*
 * airspace.h
 *
 *  Created on: Mar 14, 2022
 *      Author: horinek
 */

#ifndef FC_AIRSPACE_AIRSPACE_H_
#define FC_AIRSPACE_AIRSPACE_H_

#include "common.h"
#include "lvgl/lvgl.h"

typedef enum
{
    ac_restricted,
    ac_danger,
    ac_prohibited,
    ac_class_A,
    ac_class_B,
    ac_class_C,
    ac_class_D,
    ac_glider_prohibited,
    ac_ctr,
    ac_tmz,
    ac_rmz,
    ac_wave_window,
    ac_undefined,
} airspace_class_t;

typedef struct
{
    int32_t latitude;
    int32_t longitude;
} gnss_pos_t;

typedef struct __gnss_pos_list_t
{
    int32_t latitude;
    int32_t longitude;

    struct __gnss_pos_list_t * next;
} gnss_pos_list_t;

#define AIRSPACE_NAME_LEN  64

typedef struct __airspace_record_t
{
    char * name;
    uint16_t floor;
    uint16_t ceil;

    bool floor_gnd;
    bool ceil_gnd;
    airspace_class_t airspace_class;
    uint8_t _pad[1];

    lv_color_t pen;
    lv_color_t brush;

    gnss_pos_t * points;
    uint32_t number_of_points;

    struct __airspace_record_t * next;
} airspace_record_t;

#endif /* FC_AIRSPACE_AIRSPACE_H_ */
