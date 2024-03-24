/*
 * Allwinner SoCs display driver.
 *
 * Copyright (C) 2016 Allwinner.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include "ep28060.h"

//PC2
#define spi_scl_1 sunxi_lcd_gpio_set_value(0, 0, 1)
#define spi_scl_0 sunxi_lcd_gpio_set_value(0, 0, 0)
//PC4
#define spi_sdi_1 sunxi_lcd_gpio_set_value(0, 2, 1)
#define spi_sdi_0 sunxi_lcd_gpio_set_value(0, 2, 0)
//PC3
#define spi_cs_1 sunxi_lcd_gpio_set_value(0, 1, 1)
#define spi_cs_0 sunxi_lcd_gpio_set_value(0, 1, 0)
//PC5
#define spi_rst_1 sunxi_lcd_gpio_set_value(0, 3, 1)
#define spi_rst_0 sunxi_lcd_gpio_set_value(0, 3, 0)

static void LCD_power_on(u32 sel);
static void LCD_power_off(u32 sel);
static void LCD_bl_open(u32 sel);
static void LCD_bl_close(u32 sel);

static void LCD_panel_init(u32 sel);
static void LCD_panel_exit(u32 sel);

static void soft_spi_write_cmd(u8 value)
{
	int i;

	spi_cs_0;
	spi_scl_0;

	spi_sdi_0;
	sunxi_lcd_delay_us(10);
	spi_scl_1;
	spi_scl_0;
	for (i = 0; i < 8; i++) {
		if (value & 0x80)
			spi_sdi_1;
		else
			spi_sdi_0;
		sunxi_lcd_delay_us(10);
		spi_scl_1;
		spi_scl_0;

		value <<= 1;
	}
	sunxi_lcd_delay_us(10);
	spi_cs_1;
}

static void soft_spi_write_data(u8 value)
{
	int i;

	spi_cs_0;
	spi_scl_0;

	spi_sdi_1;
	sunxi_lcd_delay_us(10);
	spi_scl_1;
	spi_scl_0;

	for (i = 0; i < 8; i++) {
		if (value & 0x80)
			spi_sdi_1;
		else
			spi_sdi_0;

		sunxi_lcd_delay_us(10);

		spi_scl_1;
		spi_scl_0;
		
		value <<= 1;
	}
	sunxi_lcd_delay_us(10);
	spi_cs_1;
}


static void LCD_cfg_panel_info(panel_extend_para *info)
{
	u32 i = 0, j = 0;
	u32 items;
	u8 lcd_gamma_tbl[][2] = {
		/* {input value, corrected value} */
		{0, 0},
		{15, 15},
		{30, 30},
		{45, 45},
		{60, 60},
		{75, 75},
		{90, 90},
		{105, 105},
		{120, 120},
		{135, 135},
		{150, 150},
		{165, 165},
		{180, 180},
		{195, 195},
		{210, 210},
		{225, 225},
		{240, 240},
		{255, 255},
	};

	u32 lcd_cmap_tbl[2][3][4] = {
		{
		 {LCD_CMAP_G0, LCD_CMAP_B1, LCD_CMAP_G2, LCD_CMAP_B3},
		 {LCD_CMAP_B0, LCD_CMAP_R1, LCD_CMAP_B2, LCD_CMAP_R3},
		 {LCD_CMAP_R0, LCD_CMAP_G1, LCD_CMAP_R2, LCD_CMAP_G3},
		 },
		{
		 {LCD_CMAP_B3, LCD_CMAP_G2, LCD_CMAP_B1, LCD_CMAP_G0},
		 {LCD_CMAP_R3, LCD_CMAP_B2, LCD_CMAP_R1, LCD_CMAP_B0},
		 {LCD_CMAP_G3, LCD_CMAP_R2, LCD_CMAP_G1, LCD_CMAP_R0},
		 },
	};

	items = sizeof(lcd_gamma_tbl) / 2;
	for (i = 0; i < items - 1; i++) {
		u32 num = lcd_gamma_tbl[i + 1][0] - lcd_gamma_tbl[i][0];

		for (j = 0; j < num; j++) {
			u32 value = 0;

			value =
			    lcd_gamma_tbl[i][1] +
			    ((lcd_gamma_tbl[i + 1][1] -
			      lcd_gamma_tbl[i][1]) * j) / num;
			info->lcd_gamma_tbl[lcd_gamma_tbl[i][0] + j] =
			    (value << 16) + (value << 8) + value;
		}
	}
	info->lcd_gamma_tbl[255] =
	    (lcd_gamma_tbl[items - 1][1] << 16) +
	    (lcd_gamma_tbl[items - 1][1] << 8) + lcd_gamma_tbl[items - 1][1];

	memcpy(info->lcd_cmap_tbl, lcd_cmap_tbl, sizeof(lcd_cmap_tbl));

}

static s32 LCD_open_flow(u32 sel)
{
	/* open lcd power, and delay 50ms */
	LCD_OPEN_FUNC(sel, LCD_power_on, 30);
	/* open lcd power, than delay 200ms */
	LCD_OPEN_FUNC(sel, LCD_panel_init, 50);
	/* open lcd controller, and delay 100ms */
	LCD_OPEN_FUNC(sel, sunxi_lcd_tcon_enable, 100);
	/* open lcd backlight, and delay 0ms */
	LCD_OPEN_FUNC(sel, LCD_bl_open, 0);

	return 0;
}

static s32 LCD_close_flow(u32 sel)
{
	/* close lcd backlight, and delay 0ms */
	LCD_CLOSE_FUNC(sel, LCD_bl_close, 0);
	/* close lcd controller, and delay 0ms */
	LCD_CLOSE_FUNC(sel, sunxi_lcd_tcon_disable, 0);
	/* open lcd power, than delay 200ms */
	LCD_CLOSE_FUNC(sel, LCD_panel_exit, 200);
	/* close lcd power, and delay 500ms */
	LCD_CLOSE_FUNC(sel, LCD_power_off, 500);

	return 0;
}

static void LCD_power_on(u32 sel)
{
	sunxi_lcd_power_enable(sel, 0);//config lcd_power pin to open lcd power0
	sunxi_lcd_pin_cfg(sel, 1);

	sunxi_lcd_gpio_set_value(sel, 3, 1);

	spi_cs_0;

	spi_rst_1;
	sunxi_lcd_delay_ms(100);
	
	spi_rst_0;
	sunxi_lcd_delay_ms(800);

	spi_rst_1;
	sunxi_lcd_delay_ms(800);
}

static void LCD_power_off(u32 sel)
{
	sunxi_lcd_pin_cfg(sel, 0);
	/* config lcd_power pin to close lcd power0 */
	sunxi_lcd_power_disable(sel, 0);
}

static void LCD_bl_open(u32 sel)
{
	sunxi_lcd_pwm_enable(sel);
	sunxi_lcd_backlight_enable(sel);
}

static void LCD_bl_close(u32 sel)
{
	/* config lcd_bl_en pin to close lcd backlight */
	sunxi_lcd_backlight_disable(sel);
	sunxi_lcd_pwm_disable(sel);
}

static void LCD_panel_init(u32 sel)
{
	spi_cs_0;
	sunxi_lcd_gpio_set_value(0, 0, 1);
	sunxi_lcd_delay_ms(50);
	sunxi_lcd_gpio_set_value(0, 0, 0);
	/* Delay 10ms, This delay time is necessary */
	sunxi_lcd_delay_ms(100);
	sunxi_lcd_gpio_set_value(0, 0, 1);
	/* Delay 120 ms */
	sunxi_lcd_delay_ms(150);

	soft_spi_write_cmd(0xFF);
	soft_spi_write_data(0x77);
	soft_spi_write_data(0x01);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x13);

	soft_spi_write_cmd(0xEF);
	soft_spi_write_data(0x08);

	soft_spi_write_cmd(0xFF);
	soft_spi_write_data(0x77);
	soft_spi_write_data(0x01);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x10);

	soft_spi_write_cmd(0xC0);
	soft_spi_write_data(0x4F);
	soft_spi_write_data(0x00);

	soft_spi_write_cmd(0xC1);
	soft_spi_write_data(0x10);
	soft_spi_write_data(0x02);

	soft_spi_write_cmd(0xC2);
	soft_spi_write_data(0x07);
	soft_spi_write_data(0x02);

	soft_spi_write_cmd(0xCC);
	soft_spi_write_data(0x10);

	soft_spi_write_cmd(0xB0);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x10);
	soft_spi_write_data(0x17);
	soft_spi_write_data(0x0D);
	soft_spi_write_data(0x11);
	soft_spi_write_data(0x06);
	soft_spi_write_data(0x05);
	soft_spi_write_data(0x08);
	soft_spi_write_data(0x07);
	soft_spi_write_data(0x1F);
	soft_spi_write_data(0x04);
	soft_spi_write_data(0x11);
	soft_spi_write_data(0x0E);
	soft_spi_write_data(0x29);
	soft_spi_write_data(0x30);
	soft_spi_write_data(0x1F);

	soft_spi_write_cmd(0xB1);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x0D);
	soft_spi_write_data(0x14);
	soft_spi_write_data(0x0E);
	soft_spi_write_data(0x11);
	soft_spi_write_data(0x06);
	soft_spi_write_data(0x04);
	soft_spi_write_data(0x08);
	soft_spi_write_data(0x08);
	soft_spi_write_data(0x20);
	soft_spi_write_data(0x05);
	soft_spi_write_data(0x13);
	soft_spi_write_data(0x13);
	soft_spi_write_data(0x26);
	soft_spi_write_data(0x30);
	soft_spi_write_data(0x1F);

	soft_spi_write_cmd(0xFF);
	soft_spi_write_data(0x77);
	soft_spi_write_data(0x01);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x11);
	soft_spi_write_cmd(0xB0);
	soft_spi_write_data(0x65);

	soft_spi_write_cmd(0xB1);
	soft_spi_write_data(0x71);

	soft_spi_write_cmd(0xB2);
	soft_spi_write_data(0x87);

	soft_spi_write_cmd(0xB3);
	soft_spi_write_data(0x80);

	soft_spi_write_cmd(0xB5);
	soft_spi_write_data(0x4D);

	soft_spi_write_cmd(0xB7);
	soft_spi_write_data(0x85);

	soft_spi_write_cmd(0xB8);
	soft_spi_write_data(0x20);

	soft_spi_write_cmd(0xC1);
	soft_spi_write_data(0x78);

	soft_spi_write_cmd(0xC2);
	soft_spi_write_data(0x78);

	soft_spi_write_cmd(0xD0);
	soft_spi_write_data(0x88);

	soft_spi_write_cmd(0xEE);
	soft_spi_write_data(0x42);

	soft_spi_write_cmd(0xE0);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x02);

	soft_spi_write_cmd(0xE1);
	soft_spi_write_data(0x04);
	soft_spi_write_data(0xA0);
	soft_spi_write_data(0x06);
	soft_spi_write_data(0xA0);
	soft_spi_write_data(0x05);
	soft_spi_write_data(0xA0);
	soft_spi_write_data(0x07);
	soft_spi_write_data(0xA0);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x44);
	soft_spi_write_data(0x44);

	soft_spi_write_cmd(0xE2);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);

	soft_spi_write_cmd(0xE3);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x22);
	soft_spi_write_data(0x22);

	soft_spi_write_cmd(0xE4);
	soft_spi_write_data(0x44);
	soft_spi_write_data(0x44);

	soft_spi_write_cmd(0xE5);
	soft_spi_write_data(0x0C);
	soft_spi_write_data(0x90);
	soft_spi_write_data(0xA0);
	soft_spi_write_data(0xA0);
	soft_spi_write_data(0x0E);
	soft_spi_write_data(0x92);
	soft_spi_write_data(0xA0);
	soft_spi_write_data(0xA0);
	soft_spi_write_data(0x08);
	soft_spi_write_data(0x8C);
	soft_spi_write_data(0xA0);
	soft_spi_write_data(0xA0);
	soft_spi_write_data(0x0A);
	soft_spi_write_data(0x8E);
	soft_spi_write_data(0xA0);
	soft_spi_write_data(0xA0);

	soft_spi_write_cmd(0xE6);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x22);
	soft_spi_write_data(0x22);

	soft_spi_write_cmd(0xE7);
	soft_spi_write_data(0x44);
	soft_spi_write_data(0x44);

	soft_spi_write_cmd(0xE8);
	soft_spi_write_data(0x0D);
	soft_spi_write_data(0x91);
	soft_spi_write_data(0xA0);
	soft_spi_write_data(0xA0);
	soft_spi_write_data(0x0F);
	soft_spi_write_data(0x93);
	soft_spi_write_data(0xA0);
	soft_spi_write_data(0xA0);
	soft_spi_write_data(0x09);
	soft_spi_write_data(0x8D);
	soft_spi_write_data(0xA0);
	soft_spi_write_data(0xA0);
	soft_spi_write_data(0x0B);
	soft_spi_write_data(0x8F);
	soft_spi_write_data(0xA0);
	soft_spi_write_data(0xA0);

	soft_spi_write_cmd(0xEB);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0xE4);
	soft_spi_write_data(0xE4);
	soft_spi_write_data(0x44);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x40);

	soft_spi_write_cmd(0xED);
	soft_spi_write_data(0xFF);
	soft_spi_write_data(0xF5);
	soft_spi_write_data(0x47);
	soft_spi_write_data(0x6F);
	soft_spi_write_data(0x0B);
	soft_spi_write_data(0xA1);
	soft_spi_write_data(0xAB);
	soft_spi_write_data(0xFF);
	soft_spi_write_data(0xFF);
	soft_spi_write_data(0xBA);
	soft_spi_write_data(0x1A);
	soft_spi_write_data(0xB0);
	soft_spi_write_data(0xF6);
	soft_spi_write_data(0x74);
	soft_spi_write_data(0x5F);
	soft_spi_write_data(0xFF);

	soft_spi_write_cmd(0xEF);
	soft_spi_write_data(0x08);
	soft_spi_write_data(0x08);
	soft_spi_write_data(0x08);
	soft_spi_write_data(0x45);
	soft_spi_write_data(0x3F);
	soft_spi_write_data(0x54);

	soft_spi_write_cmd(0xFF);
	soft_spi_write_data(0x77);
	soft_spi_write_data(0x01);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);

	soft_spi_write_cmd(0xFF);
	soft_spi_write_data(0x77);
	soft_spi_write_data(0x01);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x13);

	soft_spi_write_cmd(0xE6);
	soft_spi_write_data(0x16);

	soft_spi_write_cmd(0xE8);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x0E);

	soft_spi_write_cmd(0xFF);
	soft_spi_write_data(0x77);
	soft_spi_write_data(0x01);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);

	soft_spi_write_cmd(0x11);
	sunxi_lcd_delay_ms(120);
	soft_spi_write_cmd(0xFF);
	soft_spi_write_data(0x77);
	soft_spi_write_data(0x01);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x13);

	soft_spi_write_cmd(0xE8);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x0C);
	sunxi_lcd_delay_ms(10);
	soft_spi_write_cmd(0xE8);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);

	soft_spi_write_cmd(0xFF);
	soft_spi_write_data(0x77);
	soft_spi_write_data(0x01);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);
	soft_spi_write_data(0x00);

	soft_spi_write_cmd(0x29);

	soft_spi_write_cmd(0x3A);
	soft_spi_write_data(0x77);
	soft_spi_write_cmd(0x29);

    soft_spi_write_cmd(0x36);
	soft_spi_write_data(0x08);

	return;
}

static void LCD_panel_exit(u32 sel)
{
}

/* sel: 0:lcd0; 1:lcd1 */
static s32 LCD_user_defined_func(u32 sel, u32 para1, u32 para2, u32 para3)
{
	return 0;
}

__lcd_panel_t ep28060_panel = {
	/* panel driver name, must mach the lcd_drv_name in sys_config.fex */
	.name = "ep28060",
	.func = {
		 .cfg_panel_info = LCD_cfg_panel_info,
		 .cfg_open_flow = LCD_open_flow,
		 .cfg_close_flow = LCD_close_flow,
		 .lcd_user_defined_func = LCD_user_defined_func,
		 }
	,
};
