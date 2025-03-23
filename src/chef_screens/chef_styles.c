#include "chef_styles.h"
#include "lvgl.h"

lv_style_t screen_background;
lv_style_t icon_default;
lv_style_t label_style;
lv_style_t arc_style;

static void _init_screen_background(void)
{
    lv_style_init(&screen_background);
    lv_style_set_bg_color(&screen_background, lv_color_black());//lv_color_make(0x22,0x22,0x22));
    lv_style_set_border_opa(&screen_background, LV_OPA_TRANSP);
    lv_style_set_text_color(&screen_background, lv_color_white());
} 

static void _init_icon_default(void)
{
    lv_style_init(&icon_default);
    lv_style_set_pad_all(&icon_default, 3);
    //lv_style_set_bg_color(&icon_default,lv_color_white()); //[BJ] just for debugging, will only h
}

static void _init_arc_default(void)
{
    lv_style_init(&arc_style);
    lv_style_set_arc_width(&arc_style, 15);
    lv_style_set_arc_color(&arc_style, lv_color_hex(0x0088FF));
}

static void _init_label_default(void)
{
    lv_style_init(&label_style);
    lv_style_set_text_font(&label_style, &lv_font_montserrat_14);
    lv_style_set_text_color(&label_style, lv_color_hex(0x333333));
}

void chef_init_styles(void)
{
    _init_screen_background();
    _init_icon_default();
    _init_arc_default();
    _init_label_default();
}