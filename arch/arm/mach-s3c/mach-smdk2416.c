// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2009 Yauhen Kharuzhy <jekhor@gmail.com>,
//	as part of OpenInkpot project
// Copyright (c) 2009 Promwad Innovation Company
//	Yauhen Kharuzhy <yauhen.kharuzhy@promwad.com>

#include "linux/input-event-codes.h"
#include "linux/printk.h"
#include "sound/s3c24xx_uda134x.h"
#include "sound/uda134x.h"
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/serial_s3c.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/mtd/partitions.h>
#include <linux/gpio.h>
#include <linux/fb.h>
#include <linux/delay.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <video/samsung_fimd.h>
#include <asm/irq.h>
#include <asm/mach-types.h>

#include "hardware-s3c24xx.h"
#include "regs-gpio.h"
#include "regs-s3c2443-clock.h"
#include "gpio-samsung.h"

#include <linux/platform_data/leds-s3c24xx.h>
#include <linux/platform_data/i2c-s3c2410.h>

#include "gpio-cfg.h"
#include "devs.h"
#include "cpu.h"
#include <linux/platform_data/mtd-nand-s3c2410.h>
#include "sdhci.h"
#include <linux/platform_data/usb-s3c2410_udc.h>
#include <linux/platform_data/s3c-hsudc.h>

#include <linux/input.h>
#include <linux/input/matrix_keypad.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>

#include "fb.h"

#include "s3c24xx.h"
#include "common-smdk-s3c24xx.h"

static struct map_desc smdk2416_iodesc[] __initdata = {
	/* ISA IO Space map (memory space selected by A24) */

	{
		.virtual = (u32)S3C24XX_VA_ISA_BYTE,
		.pfn = __phys_to_pfn(S3C2410_CS2),
		.length = 0x10000,
		.type = MT_DEVICE,
	},
	{
		.virtual = (u32)S3C24XX_VA_ISA_BYTE + 0x10000,
		.pfn = __phys_to_pfn(S3C2410_CS2 + (1 << 24)),
		.length = SZ_4M,
		.type = MT_DEVICE,
	}
};

#define UCON \
	(S3C2410_UCON_DEFAULT | S3C2440_UCON_PCLK | S3C2443_UCON_RXERR_IRQEN)

#define ULCON (S3C2410_LCON_CS8 | S3C2410_LCON_PNONE)

#define UFCON                                             \
	(S3C2410_UFCON_RXTRIG8 | S3C2410_UFCON_FIFOMODE | \
	 S3C2440_UFCON_TXTRIG16)

static struct s3c2410_uartcfg smdk2416_uartcfgs[] __initdata = {
	[0] = {
		.hwport	     = 0,
		.flags	     = 0,
		.ucon	     = UCON,
		.ulcon	     = ULCON,
		.ufcon	     = UFCON,
	},
	[1] = {
		.hwport	     = 1,
		.flags	     = 0,
		.ucon	     = UCON,
		.ulcon	     = ULCON,
		.ufcon	     = UFCON,
	},
	/* IR port */
	// [2] = {
	// 	.hwport	     = 2,
	// 	.flags	     = 0,
	// 	.ucon	     = UCON,
	// 	.ulcon	     = ULCON | 0x50,
	// 	.ufcon	     = UFCON,
	// },
	// [3] = {
	// 	.hwport	     = 3,
	// 	.flags	     = 0,
	// 	.ucon	     = UCON,
	// 	.ulcon	     = ULCON,
	// 	.ufcon	     = UFCON,
	// }
};

static void smdk2416_hsudc_gpio_init(void)
{
	s3c_gpio_setpull(S3C2410_GPH(14), S3C_GPIO_PULL_UP);
	s3c_gpio_setpull(S3C2410_GPF(2), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C2410_GPH(14), S3C_GPIO_SFN(1));
	s3c2410_modify_misccr(S3C2416_MISCCR_SEL_SUSPND, 0);
}

static void smdk2416_hsudc_gpio_uninit(void)
{
	s3c2410_modify_misccr(S3C2416_MISCCR_SEL_SUSPND, 1);
	s3c_gpio_setpull(S3C2410_GPH(14), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(S3C2410_GPH(14), S3C_GPIO_SFN(0));
}

static struct s3c24xx_hsudc_platdata smdk2416_hsudc_platdata = {
	.epnum = 9,
	.gpio_init = smdk2416_hsudc_gpio_init,
	.gpio_uninit = smdk2416_hsudc_gpio_uninit,
};

static void s3c2416_fb_gpio_setup_24bpp(void)
{
	unsigned int gpio;

	for (gpio = S3C2410_GPC(0); gpio <= S3C2410_GPC(4); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
	}

	for (gpio = S3C2410_GPC(8); gpio <= S3C2410_GPC(15); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
	}

	for (gpio = S3C2410_GPD(0); gpio <= S3C2410_GPD(15); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
	}
}

static struct s3c_fb_pd_win smdk2416_fb_win[] = {
	[0] = {
		.default_bpp	= 24,
		.max_bpp		= 32,
		.xres           = 800,
		.yres           = 480
	},
};

static struct fb_videomode smdk2416_lcd_timing = {
	//.pixclock = 4,
	.left_margin = 46, //VIDTCON1_HBPD
	.right_margin = 100, //VIDTCON1_HFPD
	.upper_margin = 23, //VIDTCON0_VBPD
	.lower_margin = 22, //VIDTCON0_VFPD

	.hsync_len = 20, //VIDTCON1_HSPW
	.vsync_len = 23, //VIDTCON0_VSPW
	.xres = 800,
	.yres = 480,
};

static struct s3c_fb_platdata smdk2416_fb_platdata = {
	.win[0] = &smdk2416_fb_win[0],
	.vtiming = &smdk2416_lcd_timing,
	.setup_gpio = s3c2416_fb_gpio_setup_24bpp,
	.vidcon0 = VIDCON0_VIDOUT_RGB | VIDCON0_PNRMODE_RGB,
	.vidcon1 = VIDCON1_INV_HSYNC | VIDCON1_INV_VSYNC,
	
};

static struct s3c_sdhci_platdata smdk2416_hsmmc0_pdata __initdata = {
	.max_width = 4,
	.cd_type = S3C_SDHCI_CD_GPIO,
	.ext_cd_gpio = S3C2410_GPF(1),
	.ext_cd_gpio_invert = 1,
};

static struct s3c_sdhci_platdata smdk2416_hsmmc1_pdata __initdata = {
	.max_width = 4,
	.cd_type = S3C_SDHCI_CD_NONE,
};

uint32_t keys_define[] = {
	KEY(3, 1, KEY_ESC),	   KEY(6, 15, KEY_F1),
	KEY(6, 13, KEY_F2),	   KEY(1, 3, KEY_F3),
	KEY(3, 3, KEY_F4),	   KEY(7, 0, KEY_F5),
	KEY(3, 6, KEY_F6),	   KEY(1, 7, KEY_F7),
	KEY(6, 7, KEY_F8),	   KEY(6, 9, KEY_F9),
	KEY(7, 12, KEY_F12),	   KEY(4, 8, KEY_NUMLOCK),
	KEY(7, 10, KEY_PRINT),	   KEY(6, 8, KEY_DELETE),
	KEY(6, 1, KEY_GRAVE),	   KEY(7, 1, KEY_1),
	KEY(7, 2, KEY_2),	   KEY(7, 3, KEY_3),
	KEY(7, 4, KEY_4),	   KEY(6, 4, KEY_5),
	KEY(6, 5, KEY_6),	   KEY(7, 5, KEY_7),
	KEY(7, 6, KEY_8),	   KEY(7, 7, KEY_9),
	KEY(7, 11, KEY_0),	   KEY(6, 11, KEY_MINUS),
	KEY(1, 9, KEY_BACKSPACE),  KEY(1, 1, KEY_TAB),
	KEY(0, 1, KEY_Q),	   KEY(0, 2, KEY_W),
	KEY(0, 3, KEY_E),	   KEY(0, 4, KEY_R),
	KEY(1, 4, KEY_T),	   KEY(1, 5, KEY_Y),
	KEY(0, 5, KEY_U),	   KEY(0, 6, KEY_I),
	KEY(0, 7, KEY_O),	   KEY(0, 11, KEY_P),
	KEY(6, 6, KEY_EQUAL),	   KEY(4, 11, KEY_BACKSLASH),
	KEY(1, 2, KEY_CAPSLOCK),   KEY(2, 1, KEY_A),
	KEY(2, 2, KEY_S),	   KEY(2, 3, KEY_D),
	KEY(2, 4, KEY_F),	   KEY(3, 4, KEY_G),
	KEY(3, 5, KEY_H),	   KEY(2, 5, KEY_J),
	KEY(2, 6, KEY_K),	   KEY(2, 7, KEY_L),
	KEY(2, 11, KEY_SEMICOLON), KEY(4, 9, KEY_ENTER),
	KEY(1, 15, KEY_LEFTSHIFT), KEY(3, 2, KEY_BACKSLASH),
	KEY(4, 1, KEY_Z),	   KEY(4, 2, KEY_X),
	KEY(4, 3, KEY_C),	   KEY(4, 4, KEY_V),
	KEY(5, 4, KEY_B),	   KEY(5, 5, KEY_N),
	KEY(4, 5, KEY_M),	   KEY(4, 6, KEY_COMMA),
	KEY(4, 7, KEY_DOT),	   KEY(5, 11, KEY_SLASH),
	KEY(2, 14, KEY_UP),	   KEY(2, 15, KEY_RIGHTSHIFT),
	KEY(7, 15, KEY_FN),	   KEY(6, 0, KEY_LEFTCTRL),
	KEY(1, 12, KEY_SLEEP),	   KEY(3, 10, KEY_LEFTALT),
	KEY(0, 0, KEY_PAUSE),	   KEY(3, 8, KEY_SPACE),
	KEY(5, 7, KEY_RIGHTALT),   KEY(1, 11, KEY_LEFTBRACE),
	KEY(1, 6, KEY_RIGHTBRACE), KEY(3, 11, KEY_APOSTROPHE),
	KEY(4, 14, KEY_LEFT),	   KEY(3, 14, KEY_DOWN),
	KEY(5, 14, KEY_RIGHT),

};

struct matrix_keymap_data matrix_keyboard_data = {
	.keymap = keys_define,
	.keymap_size = ARRAY_SIZE(keys_define),
};

uint32_t rows[] = {
	S3C2410_GPG(0), S3C2410_GPG(1), S3C2410_GPG(2), S3C2410_GPG(3),
	S3C2410_GPG(4), S3C2410_GPG(5), S3C2410_GPG(6), S3C2410_GPG(7),
};
uint32_t cols[] = {
	S3C2410_GPK(0),	 S3C2410_GPK(1),  S3C2410_GPK(2),  S3C2410_GPK(3),
	S3C2410_GPK(4),	 S3C2410_GPK(5),  S3C2410_GPK(6),  S3C2410_GPK(7),
	S3C2410_GPK(8),	 S3C2410_GPK(9),  S3C2410_GPK(10), S3C2410_GPK(11),
	S3C2410_GPK(12), S3C2410_GPK(13), S3C2410_GPK(14), S3C2410_GPK(15),
};

struct matrix_keypad_platform_data matrix_keypad_platform_data = {
	.keymap_data = &matrix_keyboard_data,
	.row_gpios = rows,
	.col_gpios = cols,

	.num_row_gpios = ARRAY_SIZE(rows),
	.num_col_gpios = ARRAY_SIZE(cols),

	.col_scan_delay_us = 100,
	.debounce_ms = 10,

	.active_low = 1,
	.no_autorepeat = 0,
};

struct platform_device ubi_keyboard = {
    .name = "matrix-keypad",
    .id = -1,
    .dev = {
        .platform_data = &matrix_keypad_platform_data,
    },
};

static struct i2c_board_info touchscreen_i2c_devs[] __initdata = {
	{ I2C_BOARD_INFO("ubi-tp-i2c", 0x04) },
};

struct gpio_keys_button btns[] = {
	{ BTN_LEFT, S3C2410_GPF(6), 1, "mouse-btn-left", EV_KEY, 0, 100 },
	{ BTN_RIGHT, S3C2410_GPF(5), 1, "mouse-btn-left", EV_KEY, 0, 100 },
	{ KEY_POWER, S3C2410_GPF(7), 0, "mouse-btn-right", EV_KEY, 0, 100 },
};

struct gpio_keys_platform_data pdata = {
	.buttons = btns,
	.nbuttons = ARRAY_SIZE(btns),
	.rep = 0,
	.name = "mygpio-keys",
};

struct platform_device gpio_button = {
    .name = "gpio-keys",
    .id = -1,
    .dev = {
        .platform_data = &pdata,
    },
};

static struct s3c24xx_uda134x_platform_data mini2440_audio_pins = {
	.l3_clk = S3C2410_GPE(1),
	.l3_mode = S3C2410_GPE(0),
	.l3_data = S3C2410_GPE(4),
	.model = UDA134X_UDA1341
};



static struct platform_device mini2440_audio = {
	.name		= "s3c24xx_uda134x",
	.id		= 0,
	.dev		= {
		.platform_data	= &mini2440_audio_pins,
	},
};


static struct uda134x_platform_data s3c24xx_uda134x = {
	.l3 = {
		.gpio_clk = S3C2410_GPE(1),
		.gpio_data = S3C2410_GPE(0),
		.gpio_mode = S3C2410_GPE(4),
		.use_gpios = 1,
		.data_hold = 1,
		.data_setup = 1,
		.clock_high = 1,
		.mode_hold = 1,
		.mode = 1,
		.mode_setup = 1,
	},
	.model = UDA134X_UDA1341,
};

static struct platform_device uda1340_codec = {
		.name = "uda134x-codec",
		.id = -1,
		.dev = {
			.platform_data	= &s3c24xx_uda134x,
		},
};


static struct platform_device *smdk2416_devices[] __initdata = {
	&s3c_device_fb,	       &s3c_device_wdt,
	&s3c_device_ohci,      &s3c_device_i2c0,
	&s3c_device_hsmmc0,    &s3c_device_hsmmc1,
	&s3c_device_usb_hsudc, &s3c2443_device_dma,
	&ubi_keyboard,	       &gpio_button,
	&uda1340_codec,
	&mini2440_audio
};

static void __init smdk2416_init_time(void)
{
	s3c2416_init_clocks(12000000);
	s3c24xx_timer_init();
}

static void __init smdk2416_map_io(void)
{
	s3c24xx_init_io(smdk2416_iodesc, ARRAY_SIZE(smdk2416_iodesc));
	s3c24xx_init_uarts(smdk2416_uartcfgs, ARRAY_SIZE(smdk2416_uartcfgs));
	s3c24xx_set_timer_source(S3C24XX_PWM3, S3C24XX_PWM4);
}

static void powercut(void)
{
	gpio_direction_output(S3C2410_GPB(3), 0);
	
	s3c_gpio_setpull(S3C2410_GPH(4), S3C_GPIO_PULL_DOWN);
	//s3c_gpio_cfgpin(S3C2410_GPH(4), S3C_GPIO_SFN(1));
	gpio_direction_output(S3C2410_GPH(4), 0);
	
	printk("--- Power Cutting ---\r\n");
	mdelay(1000);
	while (1)
		;
}

static void __init smdk2416_machine_init(void)
{
	s3c_i2c0_set_platdata(NULL);
	s3c_fb_set_platdata(&smdk2416_fb_platdata);

	s3c_sdhci0_set_platdata(&smdk2416_hsmmc0_pdata);
	s3c_sdhci1_set_platdata(&smdk2416_hsmmc1_pdata);

	s3c24xx_hsudc_set_platdata(&smdk2416_hsudc_platdata);

	i2c_register_board_info(0, touchscreen_i2c_devs,
				+ARRAY_SIZE(touchscreen_i2c_devs));

	gpio_request(S3C2410_GPH(4), "Power Control");
	gpio_request(S3C2410_GPB(3), "Display Backlight");
	gpio_direction_output(S3C2410_GPB(3), 1);

	//gpio_request(S3C2410_GPB(4), "USBHost Power");
	//gpio_direction_output(S3C2410_GPB(4), 1);

	//gpio_request(S3C2410_GPB(3), "Display Power");
	//gpio_direction_output(S3C2410_GPB(3), 1);

	//gpio_request(S3C2410_GPB(1), "Display Reset");
	//gpio_direction_output(S3C2410_GPB(1), 1);

	pm_power_off = powercut;

	platform_add_devices(smdk2416_devices, ARRAY_SIZE(smdk2416_devices));
	smdk_machine_init();
}

MACHINE_START(SMDK2416, "SMDK2416")
	/* Maintainer: Yauhen Kharuzhy <jekhor@gmail.com> */
	.atag_offset = 0x100,
	.nr_irqs = NR_IRQS_S3C2416,

	.init_irq = s3c2416_init_irq, .map_io = smdk2416_map_io,
	.init_machine = smdk2416_machine_init,
	.init_time = smdk2416_init_time, 
MACHINE_END
