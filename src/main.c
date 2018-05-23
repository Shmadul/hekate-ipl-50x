/*
* Copyright (c) 2018 naehrwert
*
* This program is free software; you can redistribute it and/or modify it
* under the terms and conditions of the GNU General Public License,
* version 2, as published by the Free Software Foundation.
*
* This program is distributed in the hope it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <string.h>
#include <stdlib.h>
#include <alloca.h>

#include "clock.h"
#include "uart.h"
#include "i2c.h"
#include "sdram.h"
#include "di.h"
#include "mc.h"
#include "t210.h"
#include "pmc.h"
#include "pinmux.h"
#include "fuse.h"
#include "util.h"
#include "gfx.h"
#include "max77620.h"
#include "gpio.h"
#include "sdmmc.h"
#include "ff.h"
#include "heap.h"
#include "se.h"
#include "se_t210.h"
#include "hos.h"
#include "splash.h"

void config_oscillators()
{
	CLOCK(CLK_RST_CONTROLLER_SPARE_REG0) = CLOCK(CLK_RST_CONTROLLER_SPARE_REG0) & 0xFFFFFFF3 | 4;
	SYSCTR0(SYSCTR0_CNTFID0) = 19200000;
	TMR(0x14) = 0x45F;
	CLOCK(CLK_RST_CONTROLLER_OSC_CTRL) = 0x50000071;
	PMC(APBDEV_PMC_OSC_EDPD_OVER) = PMC(APBDEV_PMC_OSC_EDPD_OVER) & 0xFFFFFF81 | 0xE;
	PMC(APBDEV_PMC_OSC_EDPD_OVER) = PMC(APBDEV_PMC_OSC_EDPD_OVER) & 0xFFBFFFFF | 0x400000;
	PMC(APBDEV_PMC_CNTRL2) = PMC(APBDEV_PMC_CNTRL2) & 0xFFFFEFFF | 0x1000;
	PMC(APBDEV_PMC_SCRATCH188) = PMC(APBDEV_PMC_SCRATCH188) & 0xFCFFFFFF | 0x2000000;
	CLOCK(CLK_RST_CONTROLLER_CLK_SYSTEM_RATE) = 0x10;
	CLOCK(CLK_RST_CONTROLLER_PLLMB_BASE) &= 0xBFFFFFFF;
	PMC(APBDEV_PMC_TSC_MULT) = PMC(APBDEV_PMC_TSC_MULT) & 0xFFFF0000 | 0x249F; //0x249F = 19200000 * (16 / 32.768 kHz)
	CLOCK(CLK_RST_CONTROLLER_SCLK_BURST_POLICY) = 0x20004444;
	CLOCK(CLK_RST_CONTROLLER_SUPER_SCLK_DIVIDER) = 0x80000000;
	CLOCK(CLK_RST_CONTROLLER_CLK_SYSTEM_RATE) = 2;
}

void config_gpios()
{
	PINMUX_AUX(PINMUX_AUX_UART2_TX) = 0;
	PINMUX_AUX(PINMUX_AUX_UART3_TX) = 0;

	PINMUX_AUX(PINMUX_AUX_GPIO_PE6) = 0x40;
	PINMUX_AUX(PINMUX_AUX_GPIO_PH6) = 0x40;

	gpio_config(GPIO_PORT_G, GPIO_PIN_0, GPIO_MODE_GPIO);
	gpio_config(GPIO_PORT_D, GPIO_PIN_1, GPIO_MODE_GPIO);
	gpio_config(GPIO_PORT_E, GPIO_PIN_6, GPIO_MODE_GPIO);
	gpio_config(GPIO_PORT_H, GPIO_PIN_6, GPIO_MODE_GPIO);
	gpio_output_enable(GPIO_PORT_G, GPIO_PIN_0, GPIO_OUTPUT_DISABLE);
	gpio_output_enable(GPIO_PORT_D, GPIO_PIN_1, GPIO_OUTPUT_DISABLE);
	gpio_output_enable(GPIO_PORT_E, GPIO_PIN_6, GPIO_OUTPUT_DISABLE);
	gpio_output_enable(GPIO_PORT_H, GPIO_PIN_6, GPIO_OUTPUT_DISABLE);

	pinmux_config_i2c(I2C_1);
	pinmux_config_i2c(I2C_5);
	pinmux_config_uart(UART_A);

	//Configure volume up/down as inputs.
	gpio_config(GPIO_PORT_X, GPIO_PIN_6, GPIO_MODE_GPIO);
	gpio_config(GPIO_PORT_X, GPIO_PIN_7, GPIO_MODE_GPIO);
	gpio_output_enable(GPIO_PORT_X, GPIO_PIN_6, GPIO_OUTPUT_DISABLE);
	gpio_output_enable(GPIO_PORT_X, GPIO_PIN_7, GPIO_OUTPUT_DISABLE);
}

void config_pmc_scratch()
{
	PMC(APBDEV_PMC_SCRATCH20) &= 0xFFF3FFFF;
	PMC(APBDEV_PMC_SCRATCH190) &= 0xFFFFFFFE;
	PMC(APBDEV_PMC_SECURE_SCRATCH21) |= 0x10;
}

void mbist_workaround()
{
	CLOCK(0x410) = (CLOCK(0x410) | 0x8000) & 0xFFFFBFFF;
	CLOCK(0xD0) |= 0x40800000u;
	CLOCK(0x2AC) = 0x40;
	CLOCK(0x294) = 0x40000;
	CLOCK(0x304) = 0x18000000;
	sleep(2);

	I2S(0x0A0) |= 0x400;
	I2S(0x088) &= 0xFFFFFFFE;
	I2S(0x1A0) |= 0x400;
	I2S(0x188) &= 0xFFFFFFFE;
	I2S(0x2A0) |= 0x400;
	I2S(0x288) &= 0xFFFFFFFE;
	I2S(0x3A0) |= 0x400;
	I2S(0x388) &= 0xFFFFFFFE;
	I2S(0x4A0) |= 0x400;
	I2S(0x488) &= 0xFFFFFFFE;
	DISPLAY_A(0xCF8) |= 4;
	VIC(0x8C) = 0xFFFFFFFF;
	sleep(2);

	CLOCK(0x2A8) = 0x40;
	CLOCK(0x300) = 0x18000000;
	CLOCK(0x290) = 0x40000;
	CLOCK(CLK_RST_CONTROLLER_CLK_OUT_ENB_H) = 0xC0;
	CLOCK(CLK_RST_CONTROLLER_CLK_OUT_ENB_L) = 0x80000130;
	CLOCK(CLK_RST_CONTROLLER_CLK_OUT_ENB_U) = 0x1F00200;
	CLOCK(CLK_RST_CONTROLLER_CLK_OUT_ENB_V) = 0x80400808;
	CLOCK(CLK_RST_CONTROLLER_CLK_OUT_ENB_W) = 0x402000FC;
	CLOCK(CLK_RST_CONTROLLER_CLK_OUT_ENB_X) = 0x23000780;
	CLOCK(CLK_RST_CONTROLLER_CLK_OUT_ENB_Y) = 0x300;
	CLOCK(0xF8) = 0;
	CLOCK(0xFC) = 0;
	CLOCK(0x3A0) = 0;
	CLOCK(0x3A4) = 0;
	CLOCK(0x554) = 0;
	CLOCK(0xD0) &= 0x1F7FFFFF;
	CLOCK(0x410) &= 0xFFFF3FFF;
	CLOCK(0x148) = CLOCK(0x148) & 0x1FFFFFFF | 0x80000000;
	CLOCK(0x180) = CLOCK(0x180) & 0x1FFFFFFF | 0x80000000;
	CLOCK(0x6A0) = CLOCK(0x6A0) & 0x1FFFFFFF | 0x80000000;
}

void config_se_brom()
{
	//Bootrom part we skipped.
	u32 sbk[4] = { FUSE(0x1A4), FUSE(0x1A8), FUSE(0x1AC), FUSE(0x1B0) };
	se_aes_key_set(14, sbk, 0x10);
	//Lock SBK from being read.
	SE(SE_KEY_TABLE_ACCESS_REG_OFFSET + 14 * 4) = 0x7E;
	//This memset needs to happen here, else TZRAM will behave weirdly later on.
	memset((void *)0x7C010000, 0, 0x10000);
	PMC(APBDEV_PMC_CRYPTO_OP) = 0;
	SE(SE_INT_STATUS_REG_OFFSET) = 0x1F;
	//Lock SSK (although it's not set and unused anyways).
	SE(SE_KEY_TABLE_ACCESS_REG_OFFSET + 15 * 4) = 0x7E;
}

void config_hw()
{
	//Bootrom stuff we skipped by going thru rcm.
	config_se_brom();
	//FUSE(FUSE_PRIVATEKEYDISABLE) = 0x11;
	SYSREG(0x110) &= 0xFFFFFF9F;
	PMC(0x244) = ((PMC(0x244) >> 1) << 1) & 0xFFFFFFFD;

	mbist_workaround();
	clock_enable_se();

	//Enable fuse clock.
	clock_enable_fuse(1);
	//Disable fuse programming.
	fuse_disable_program();

	mc_enable();

	config_oscillators();
	APB_MISC(0x40) = 0;
	config_gpios();

	//clock_enable_uart(UART_C);
	//uart_init(UART_C, 115200);

	clock_enable_cl_dvfs();

	clock_enable_i2c(I2C_1);
	clock_enable_i2c(I2C_5);

	static const clock_t clock_unk1 = { 0x358, 0x360, 0x42C, 0x1F, 0, 0 };
	static const clock_t clock_unk2 = { 0x358, 0x360, 0, 0x1E, 0, 0 };
	clock_enable(&clock_unk1);
	clock_enable(&clock_unk2);

	i2c_init(I2C_1);
	i2c_init(I2C_5);

	i2c_send_byte(I2C_5, 0x3C, MAX77620_REG_CNFGBBC, 0x40);
	i2c_send_byte(I2C_5, 0x3C, MAX77620_REG_ONOFFCNFG1, 0x78);

	i2c_send_byte(I2C_5, 0x3C, MAX77620_REG_FPS_CFG0, 0x38);
	i2c_send_byte(I2C_5, 0x3C, MAX77620_REG_FPS_CFG1, 0x3A);
	i2c_send_byte(I2C_5, 0x3C, MAX77620_REG_FPS_CFG2, 0x38);
	i2c_send_byte(I2C_5, 0x3C, MAX77620_REG_FPS_LDO4, 0xF);
	i2c_send_byte(I2C_5, 0x3C, MAX77620_REG_FPS_LDO8, 0xC7);
	i2c_send_byte(I2C_5, 0x3C, MAX77620_REG_FPS_SD0, 0x4F);
	i2c_send_byte(I2C_5, 0x3C, MAX77620_REG_FPS_SD1, 0x29);
	i2c_send_byte(I2C_5, 0x3C, MAX77620_REG_FPS_SD3, 0x1B);

	i2c_send_byte(I2C_5, 0x3C, MAX77620_REG_SD0, 42); //42 = (1125000 - 600000) / 12500 -> 1.125V

	config_pmc_scratch();

	CLOCK(CLK_RST_CONTROLLER_SCLK_BURST_POLICY) = CLOCK(CLK_RST_CONTROLLER_SCLK_BURST_POLICY) & 0xFFFF8888 | 0x3333;

	mc_config_carveout();

	sdram_init();
	//TODO: test this with LP0 wakeup.
	sdram_lp0_save_params(sdram_get_params());
}

//TODO: ugly.
sdmmc_t sd_sdmmc;
sdmmc_storage_t sd_storage;
FATFS sd_fs;
int sd_mounted;

int sd_mount()
{
	if (sd_mounted)
		return 1;

	if (sdmmc_storage_init_sd(&sd_storage, &sd_sdmmc, SDMMC_1, SDMMC_BUS_WIDTH_4, 11) &&
		f_mount(&sd_fs, "", 1) == FR_OK)
	{
		sd_mounted = 1;
		return 1;
	}

	return 0;
}

void launch_firmware(gfx_con_t * con)
{
	gfx_con_setcol(con, 0xFFFFFFFF, 1, 0xFF000000);
	gfx_con_setpos(con, 0, 0);

    if (sd_mount())
    {
        con->enabled = draw_splash(con);	

        if (!hos_launch(con))
             gfx_debug(con, error, "Failed to launch firmware.\n");
    }
    else
        gfx_debug(con, error, "Failed to mount SD card (make sure that it is inserted).\n");
}

extern void pivot_stack(u32 stack_top);

void ipl_main()
{
	gfx_ctxt_t gfx_ctxt;
	gfx_con_t gfx_con;
	
	config_hw();

	//Pivot the stack so we have enough space.
	pivot_stack(0x90010000);

	//Tegra/Horizon configuration goes to 0x80000000+, package2 goes to 0xA9800000, we place our heap in between.
	heap_init(0x90020000);
	
	display_init();

	u32 *fb = display_init_framebuffer();
	gfx_init_ctxt(&gfx_ctxt, fb, 720, 1280, 768);
	gfx_clear(&gfx_ctxt, 0xFF000000);
	gfx_con_init(&gfx_con, &gfx_ctxt);
	gfx_clear(&gfx_ctxt, 0xFF000000);

	launch_firmware(&gfx_con);
}