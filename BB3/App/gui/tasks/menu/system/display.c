#include "display.h"
#include "system.h"

#include "gui/gui_list.h"

#include "drivers/power/led.h"

#include "fc/fc.h"
#include "etc/format.h"
#include "drivers/power/pwr_mng.h"

REGISTER_TASK_I(display);

static gui_list_slider_options_t back_param =
{
	.disp_multi	= 1,
	.step = 10,
	.format = format_percent
};

lv_obj_t * display_init(lv_obj_t * par)
{
	lv_obj_t * list = gui_list_create(par, "Display Settings", &gui_system, NULL);

	gui_list_auto_entry(list, "Brightness", &config.display.backlight, &back_param);
	gui_list_auto_entry(list, "Page animation", &config.display.page_anim, NULL);

	if (pwr.fuel_gauge.status == fc_dev_ready)
		gui_list_auto_entry(list, "Battery percent", &config.display.bat_per, NULL);

	return list;
}

