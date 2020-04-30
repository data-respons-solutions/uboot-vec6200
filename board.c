/*
 * Copyright (C) 2020 Data Respons Solutions AB
 *
 * Author: Mikko Salomäki <ms@datarespons.se>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <errno.h>
#include <version.h>
#include <asm/arch/clock.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/crm_regs.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/arch/sys_proto.h>
#include <dm.h>
#include <pwm.h>
#include <power/pmic.h>
#include <power/pfuze100_pmic.h>
#include <miiphy.h>
#include <netdev.h>
#include <power/regulator.h>
#include "vec6200_pins.h"
#include "vec6200_gpio.h"

DECLARE_GLOBAL_DATA_PTR;

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

struct reset_gpio {
	const char* gpio;
	const char* name;
	const ulong flags;
};

static const struct reset_gpio AR8033_RESET_GPIO =
	{.gpio = "GPIO1_25", .name = "RGMII--CPU_nRST", .flags = GPIOD_IS_OUT | GPIOD_ACTIVE_LOW};

static const struct reset_gpio RESET_GPIOS[] = {
	{.gpio = "GPIO2_23", .name = "RST--UARTA", .flags = GPIOD_IS_OUT | GPIOD_ACTIVE_LOW},
	{.gpio = "GPIO2_24", .name = "RST--UARTB", .flags = GPIOD_IS_OUT | GPIOD_ACTIVE_LOW},
	{.gpio = "GPIO3_23", .name = "CAN4_RESET--CPU", .flags = GPIOD_IS_OUT | GPIOD_ACTIVE_LOW},
	{.gpio = "GPIO4_20", .name = "CAN3_RESET--CPU", .flags = GPIOD_IS_OUT | GPIOD_ACTIVE_LOW},
	{.gpio = "GPIO4_8", .name = "GP-nRESET--TPM", .flags = GPIOD_IS_OUT | GPIOD_ACTIVE_LOW},
	{.gpio = "GPIO7_13", .name = "HUB1_RESET#", .flags = GPIOD_IS_OUT | GPIOD_ACTIVE_LOW},
	{.gpio = "GPIO5_30", .name = "GP-PCIE_PWR_nEN", .flags = GPIOD_IS_OUT | GPIOD_ACTIVE_LOW},
};

#if 0     //Available flags
#define GPIOD_REQUESTED		(1 << 0)	/* Requested/claimed */
#define GPIOD_IS_OUT		(1 << 1)	/* GPIO is an output */
#define GPIOD_IS_IN		(1 << 2)	/* GPIO is an input */
#define GPIOD_ACTIVE_LOW	(1 << 3)	/* value has active low */
#define GPIOD_IS_OUT_ACTIVE	(1 << 4)	/* set output active */
#endif
static int request_gpio(struct gpio_desc* desc, const struct reset_gpio* rst)
{
	int r = dm_gpio_lookup_name(rst->gpio, desc);
	if (r) {
		printf("%s: failed finding: %s [%d]: %s\n", __func__, rst->name, r, errno_str(r));
		return r;
	}

	r = dm_gpio_request(desc, rst->name);
	if (r) {
		printf("%s: failed requesting: %s [%d]: %s\n", __func__, rst->name, r, errno_str(r));
		return r;
	}

	r = dm_gpio_set_dir_flags(desc, rst->flags);
	if (r) {
		printf("%s: failed setting flags: %s [%d]: %s\n", __func__, rst->name, r, errno_str(r));
		return r;
	}

	return 0;
}

static int set_gpio(struct gpio_desc* desc, int value)
{
	int r = dm_gpio_set_value(desc, value);
	if (r) {
		printf("%s: failed setting value: %s->%d [%d]: %s\n", __func__, desc->dev->name, value, r, errno_str(r));
		return r;
	}

	return 0;
}

/*
 *
 * board_early_init_f
 *
 */

int board_early_init_f(void)
{
	return 0;
}

/*
 *
 * dram_init
 *
 */

int dram_init(void)
{
	gd->ram_size = imx_ddr_size();
	return 0;
}

/*
 *
 * board_init
 *
 */

static int ar8033_125M_clk(struct phy_device *phydev)
{
	unsigned short val = 0;

	printf("%s: Setting enet ref clock to 125 MHz\n", __func__);
	/* To enable AR8031 ouput a 125MHz clk from CLK_25M */
	phy_write(phydev, MDIO_DEVAD_NONE, 0xd, 0x7);
	phy_write(phydev, MDIO_DEVAD_NONE, 0xe, 0x8016);
	phy_write(phydev, MDIO_DEVAD_NONE, 0xd, 0x4007);

	val = phy_read(phydev, MDIO_DEVAD_NONE, 0xe);
	val &= 0xffe3;
	val |= 0x18;
	phy_write(phydev, MDIO_DEVAD_NONE, 0xe, val);

	/* introduce tx clock delay */
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x5);
	val = phy_read(phydev, MDIO_DEVAD_NONE, 0x1e);
	val |= 0x0100;
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, val);

	return 0;
}

int board_phy_config(struct phy_device *phydev)
{
	struct gpio_desc ar8033_nrst;
	if (!request_gpio(&ar8033_nrst, &AR8033_RESET_GPIO)) {
		set_gpio(&ar8033_nrst, 1);
		udelay(500);
		set_gpio(&ar8033_nrst, 0);
	}

	ar8033_125M_clk(phydev);

	if (phydev->drv->config) {
		phydev->drv->config(phydev);
	}

	return 0;
}

static void reset_all(void)
{
	struct gpio_desc gpios[ARRAY_SIZE(RESET_GPIOS)];
	for (int i = 0; i < ARRAY_SIZE(RESET_GPIOS); ++i) {
		if (!request_gpio(&gpios[i], &RESET_GPIOS[i])) {
			set_gpio(&gpios[i], 1);
		}
	}

	udelay(500);

	for (int i = 0; i < ARRAY_SIZE(RESET_GPIOS); ++i) {
		if (gpios[i].dev) {
			set_gpio(&gpios[i], 0);
		}
	}
}

int board_init(void)
{
	// address of boot parameters
	gd->bd->bi_boot_params = CONFIG_SYS_SDRAM_BASE + 0x100;

	reset_all();

	return 0;
}

/*
 *
 * power_init_board
 *
 */

int power_init_board(void)
{
	printf("%s: SKIP -- PMIC VDDIO WRONG VOLTAGE -- \n", __func__);
	return 0;

	struct udevice *dev = NULL;
	int r = 0;
	int dev_id = 0;
	int rev_id = 0;

	r = pmic_get("pfuze100", &dev);
	if (r) {
		printf("pmic: pfuze100: not found [%d]: %s\n", r, errno_str(r));
		return r;
	}

	dev_id = pmic_reg_read(dev, PFUZE100_DEVICEID);
	if (dev_id < 0) {
		printf("pmic: pfuze100: dev_id [%d]: %s\n", dev_id, errno_str(dev_id));
		return dev_id;
	}
	rev_id = pmic_reg_read(dev, PFUZE100_REVID);
	if (dev_id < 0) {
		printf("pmic: pfuze100: rev_id [%d]: %s\n", rev_id, errno_str(rev_id));
		return rev_id;
	}
	printf("pmic: pfuze100: dev_id: 0x%04x: rev_id: 0x%04x\n", dev_id, rev_id);

	/* VDDCORE to 1425 mV */
	r = pmic_reg_write(dev, PFUZE100_SW1ABVOL, SW1x_1_425V);
	if (!r) {
		printf("pmic: pfuze100: SW1ABVOL: 1425 mV\n");
	}
	else {
		printf("pmic: pfuze100: SW1ABVOL [%d]: %s\n", r, errno_str(r));
		return r;
	}

	/* VDDSOC to 1425 mV */
	r = pmic_reg_write(dev, PFUZE100_SW1CVOL, SW1x_1_425V);
	if (!r) {
		printf("pmic: pfuze100: SW1CVOL: 1425 mV\n");
	}
	else {
		printf("pmic: pfuze100: SW1CVOL [%d]: %s\n", r, errno_str(r));
		return r;
	}

	/* DDR to 1350 mV */
	r = pmic_reg_write(dev, PFUZE100_SW3AVOL, SW1x_1_350V);
	if (!r) {
		printf("pmic: pfuze100: SW3AVOL: 1350 mV\n");
	}
	else {
		printf("pmic: pfuze100: SW3AVOL [%d]: %s\n", r, errno_str(r));
		return r;
	}
	r = pmic_reg_write(dev, PFUZE100_SW3BVOL, SW1x_1_350V);
	if (!r) {
		printf("pmic: pfuze100: SW3BVOL: 1350 mV\n");

	}
	else {
		printf("pmic: pfuze100: SW3BVOL [%d]: %s\n", r, errno_str(r));
		return r;
	}

	/* delay needed? */
	mdelay(10);

	return 0;
}
