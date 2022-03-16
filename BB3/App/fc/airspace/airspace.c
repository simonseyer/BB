/*
 * airspace.cpp
 *
 *  Created on: Mar 14, 2022
 *      Author: horinek
 */

#include "airspace.h"
#include "fc/fc.h"


bool airspace_point(char * line, int32_t lon, int32_t lat)
{
    uint8_t lat_deg, lat_min, lat_sec, lat_dsec;
    uint8_t lon_deg, lon_min, lon_sec, lon_dsec;
    char lat_c, lon_c;

    sscanf(line, "%02u:%02u:%02u.%02 %c %03u:%02u:%02u.%02 %c",
            &lat_deg, &lat_min, &lat_sec, &lat_dsec, &lat_c,
            &lon_deg, &lon_min, &lon_sec, &lon_dsec, &lon_c);

    lat = lat_deg * GNSS_MUL
            + (lat_min * 100) / 60;
}

uint16_t airspace_alt(char * line, bool * gnd)
{
    bool fl = false;

    if (strstr(line, "GND") != NULL)
        *gnd = true;

    if (strncmp("FL", line, 2) == 0)
    {
        line += 2;
        fl = true;
    }

    uint16_t value = atoi_c(line);

    if (fl)
    {
        value = (value * 100) / FC_METER_TO_FEET;
        *gnd = false;
    }

    if (strstr(line, "ft") != NULL)
        value /= FC_METER_TO_FEET;

    if (strstr(line, "AMSL") != NULL)
        *gnd = false;




    return value;
}

void airspace_load(char * path)
{
    INFO("airspace_load %s", path);

    FIL * fp = malloc(sizeof(FIL));

    FRESULT res = f_open(fp, path, FA_READ);

    if (res == FR_OK)
    {
        char mem_line[128];
        char * line = mem_line;

        airspace_record_t first;

        while (true)
        {
            airspace_record_t * actual = NULL;

            if (f_gets(line, sizeof(line), fp) == NULL)
                break;

            if (line[0] == '*' || strlen(line) == 0)
                continue;

            //airspace class
            if (strncmp("AC ", line, 3) == 0)
            {
                if (actual == NULL)
                {
                    actual = &first;
                }
                else
                {
                    airspace_record_t * new_airspace = malloc(sizeof(airspace_record_t));
                    actual->next = new_airspace;
                    actual = new_airspace;
                }

                actual->name = NULL;
                actual->points = NULL;

                line += 3;
                if (*line == 'R')
                    actual->airspace_class = ac_restricted;
                else if (*line == 'Q')
                    actual->airspace_class = ac_danger;
                else if (*line == 'P')
                    actual->airspace_class = ac_prohibited;
                else if (*line == 'A')
                    actual->airspace_class = ac_class_A;
                else if (*line == 'B')
                    actual->airspace_class = ac_class_B;
                else if (*line == 'C')
                    actual->airspace_class = ac_class_C;
                else if (*line == 'D')
                    actual->airspace_class = ac_class_D;
                else if (*line == 'W')
                    actual->airspace_class = ac_wave_window;
                else if (strncmp(line, "PG", 2) == 0)
                    actual->airspace_class = ac_glider_prohibited;
                else if (strncmp(line, "CTR", 3) == 0)
                    actual->airspace_class = ac_ctr;
                else if (strncmp(line, "TMZ", 3) == 0)
                    actual->airspace_class = ac_tmz;
                else if (strncmp(line, "RMZ", 3) == 0)
                    actual->airspace_class = ac_rmz;
                else
                    actual->airspace_class = ac_undefined;

                continue;
            }
            else if (strncmp("AN ", line, 3) == 0)
            {
                line += 3;

                if (actual->name != NULL)
                {
                    ASSERT(0);
                    free(actual->name);
                }

                uint16_t len = min(AIRSPACE_NAME_LEN, strlen(line));
                actual->name = malloc(len);
                strncpy(actual->name, line, len - 1);

                continue;
            }
            else if (strncmp("AH ", line, 3) == 0)
            {
                line += 3;

                actual->ceil = airspace_alt(line, &actual->ceil_gnd);
            }
            else if (strncmp("AL ", line, 3) == 0)
            {
                line += 3;

                actual->floor = airspace_alt(line, &actual->floor_gnd);
            }



        }
    }
    else
    {
        ERR("Unable to open %s, res = %u", path, res);
    }

    free(fp);
}
