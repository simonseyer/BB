/*
 * agl.cc
 *
 *  Created on: Jul 30, 2020
 *      Author: horinek
 */


#include "agl.h"
#include "fatfs.h"

#include "fc.h"

// The SRMT HGT file format is described in detail in
//     http://dds.cr.usgs.gov/srtm/version2_1/Documentation/SRTM_Topo.pdf

// Some HGT files contain 1201 x 1201 points (3 arc/90m resolution)
#define HGT_DATA_WIDTH_3		1201ul

// Some HGT files contain 3601 x 3601 points (1 arc/30m resolution)
#define HGT_DATA_WIDTH_1		3601ul

// Some HGT files contain 3601 x 1801 points (1 arc/30m resolution)
#define HGT_DATA_WIDTH_1_HALF	1801ul



hagl_pos_t agl_get_fpos(int32_t lon, int32_t lat)
{
	hagl_pos_t tmp;

	tmp.flags = 0x00;
    tmp.lat = lat / GNSS_MUL;
    if (lat < 0) tmp.lat--;

    tmp.lon = lon / GNSS_MUL;
    if (lon < 0) tmp.lon--;

    return tmp;
}

bool agl_pos_cmp(hagl_pos_t * a, hagl_pos_t * b)
{
    return a->lat == b->lat && a->lon == b->lon;
}

void agl_get_filename(char * fn, hagl_pos_t pos)
{
    char lat_c, lon_c;

    if (pos.lat >= 0)
    {
        lat_c = 'N';
    }
    else
    {
        lat_c = 'S';
    }

    if (pos.lon == -181)
    	pos.lon = 179;
    if (pos.lon == 180)
    	pos.lon = -180;

    if (pos.lon >= 0)
    {
        lon_c = 'E';
    }
    else
    {
        lon_c = 'W';
    }

    sprintf(fn, "%c%02u%c%03u", lat_c, abs(pos.lat), lon_c, abs(pos.lon));
}


void agl_get_file_min_max(char * filename, int16_t * vmin, int16_t  * vmax)
{
	uint8_t buff[2048];
	FIL file;

	f_open(&file, filename, FA_READ);

	*vmin = INT16_MAX;
	*vmax = INT16_MIN;

	while(!f_eof(&file))
	{
		UINT br;

		f_read(&file, buff, sizeof(buff), &br);

		for (uint16_t i = 0; i < br / 2; i++)
		{
			byte2 alt;

		    alt.uint8[0] = buff[i*2 + 1];
		    alt.uint8[1] = buff[i*2 + 0];

		    if (alt.int16 > *vmax) *vmax = alt.int16;
		    if (alt.int16 < *vmin) *vmin = alt.int16;
		}

	}

	f_close(&file);

}

int16_t agl_get_alt(int32_t lat, int32_t lon, bool use_bilinear)
{
    uint16_t num_points_x;
    uint16_t num_points_y;
    int16_t alt;

    #define CACHE_SIZE	4

    static FIL files_cache[CACHE_SIZE];
    static FIL files_cache2[CACHE_SIZE];
    static hagl_pos_t files_fpos[CACHE_SIZE] = {POS_INVALID, POS_INVALID, POS_INVALID, POS_INVALID};
    static uint8_t file_index = 0;

    hagl_pos_t fpos = agl_get_fpos(lon, lat);

    uint8_t i = 0;
    while (1)
    {
    	if(files_fpos[file_index].lat == fpos.lat && files_fpos[file_index].lon == fpos.lon)
    	{
    		if (files_fpos[file_index].flags & POS_FLAG_NOT_FOUND)
    			return AGL_INVALID;

    		break;
    	}
    	else
    	{
			i++;
			if (i == CACHE_SIZE)
			{
				char filename[9];
				char path[32];

				files_fpos[file_index] = fpos;

				agl_get_filename(filename, fpos);
				snprintf(path, sizeof(path), PATH_TOPO_DIR "/%s.hgt", filename);

				f_close(&files_cache[file_index]);
				f_close(&files_cache2[file_index]);

				INFO("opening file '%s' cache[%u]", path, file_index);
				uint8_t ret = f_open(&files_cache[file_index], path, FA_READ);
				if (ret != FR_OK)
				{
					files_fpos[file_index].flags |= POS_FLAG_NOT_FOUND;
					WARN("not found!");
					db_insert(PATH_TOPO_INDEX, filename, "W"); //set want flag

					return AGL_INVALID;
				}

//				int16_t vmin, vmax;
//
//				uint32_t t_start = HAL_GetTick();
//				agl_get_file_min_max(path, &vmin, &vmax);
//				DBG("time %lu ms", HAL_GetTick() - t_start);

				if (use_bilinear)
					f_open(&files_cache2[file_index], path, FA_READ);

				break;
			}
			file_index = (file_index + 1) % CACHE_SIZE;
    	}

    };


    if (lon < 0)
    {
        // we do not care above degree, only minutes are important
        // reverse the value, because file goes in opposite direction.
        lon = (GNSS_MUL - 1) + (lon % GNSS_MUL);   // lon is negative!
    }

    if (lat < 0)
    {
        // we do not care above degree, only minutes are important
        // reverse the value, because file goes in opposite direction.
        lat = (GNSS_MUL - 1) + (lat % GNSS_MUL);   // lat is negative!
    }

    // Check, if we have a 1201x1201 or 3601x3601 tile:
    switch (f_size(&files_cache[file_index]))
    {
        case HGT_DATA_WIDTH_3 * HGT_DATA_WIDTH_3 * 2:
            num_points_x = num_points_y = HGT_DATA_WIDTH_3;
        break;
        case HGT_DATA_WIDTH_1 * HGT_DATA_WIDTH_1 * 2:
            num_points_x = num_points_y = HGT_DATA_WIDTH_1;
        break;
        case HGT_DATA_WIDTH_1 * HGT_DATA_WIDTH_1_HALF * 2:
            num_points_x = HGT_DATA_WIDTH_1_HALF;
            num_points_y = HGT_DATA_WIDTH_1;
        break;
        default:
            return AGL_INVALID;
    }

    // "-2" is, because a file has a overlap of 1 point to the next file.
    uint32_t coord_div_x = GNSS_MUL / (num_points_x - 2);
    uint32_t coord_div_y = GNSS_MUL / (num_points_y - 2);
    uint16_t y = (lat % GNSS_MUL) / coord_div_y;
    uint16_t x = (lon % GNSS_MUL) / coord_div_x;

    UINT rd;
    uint8_t tmp[4];
    byte2 alt11, alt12, alt21, alt22;

    //seek to position
    uint32_t pos = ((uint32_t) x + num_points_x * (uint32_t) ((num_points_y - y) - 1)) * 2;
//    DBG("agl_get_alt: lat=%ld, lon=%ld; x=%d, y=%d; pos=%ld", lat, lon, x, y, pos);

    ASSERT(f_lseek(&files_cache[file_index], pos) == FR_OK);
    ASSERT(f_read(&files_cache[file_index], tmp, 4, &rd) == FR_OK);
    ASSERT(rd == 4);

    //switch big endian to little
    alt11.uint8[0] = tmp[1];
    alt11.uint8[1] = tmp[0];


	if (!use_bilinear)
		return alt11.int16;


    alt21.uint8[0] = tmp[3];
    alt21.uint8[1] = tmp[2];

    //seek to opposite position
    pos -= num_points_x * 2;
    ASSERT(f_lseek(&files_cache2[file_index], pos) == FR_OK);
    ASSERT(f_read(&files_cache2[file_index], tmp, 4, &rd) == FR_OK);
    ASSERT(rd == 4);

    //switch big endian to little
    alt12.uint8[0] = tmp[1];
    alt12.uint8[1] = tmp[0];

    alt22.uint8[0] = tmp[3];
    alt22.uint8[1] = tmp[2];

    //get point displacement
    float lat_dr = ((lat % GNSS_MUL) % coord_div_y) / (float)(coord_div_y);
    float lon_dr = ((lon % GNSS_MUL) % coord_div_x) / (float)(coord_div_x);

    //compute height by using bilinear interpolation
    float alt1 = alt11.int16 + (float)(alt12.int16 - alt11.int16) * lat_dr;
    float alt2 = alt21.int16 + (float)(alt22.int16 - alt21.int16) * lat_dr;

    alt = alt1 + (float)(alt2 - alt1) * lon_dr;
    //DEB("alt11=%d, alt21=%d, alt12=%d, alt22=%d, alt=%d\n", alt11.int16, alt21.int16, alt12.int16, alt22.int16, alt);

    return alt;
}

void agl_step()
{
	if (fc.gnss.fix == 3)
		fc.agl.ground_height = agl_get_alt(fc.gnss.latitude, fc.gnss.longtitude, true);
	else
		fc.agl.ground_height = AGL_INVALID;

	if (fc.agl.ground_height == AGL_INVALID)
		fc.agl.agl = AGL_INVALID;
	else
		fc.agl.agl = fc.gnss.altitude_above_msl - fc.agl.ground_height;

}

