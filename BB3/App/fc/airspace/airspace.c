/*
 * airspace.cpp
 *
 *  Created on: Mar 14, 2022
 *      Author: horinek
 */

#include "airspace.h"
#include "fc/fc.h"


uint16_t airspace_alt(char * line, bool * amsl)
{
    bool fl = false;



    if (strncmp("FL", line, 2) == 0)
    {
        line += 2;
        fl = true;
    }

    uint16_t value = atoi_c(line);

    if (fl)
        value = (value * 100) / FC_METER_TO_FEET;

    if (strstr(line, "ft") != NULL)
        value /= FC_METER_TO_FEET;



        return 0;
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
                else if (strncmp(line, "PG", 2) == 0)
                    actual->airspace_class = ac_glider_prohibited;
                else if (strncmp(line, "CTR", 3) == 0)
                    actual->airspace_class = ac_ctr;
                else if (*line == 'W')
                    actual->airspace_class = ac_wave_window;
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
            }



        }
    }
    else
    {
        ERR("Unable to open %s, res = %u", path, res);
    }

    free(fp);
}
