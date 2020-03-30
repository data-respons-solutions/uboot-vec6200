#ifndef __VEC6200_PINS_H__
#define __VEC6200_PINS_H__

#include <asm/arch/mx6-pins.h>
#include <asm/arch/iomux.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/mach-imx/mxc_i2c.h>
#include "../common/include/mx6_common_defs.h"

static iomux_v3_cfg_t const console_pads[] = {
	IOMUX_PADS(PAD_CSI0_DAT10__UART1_TX_DATA | MUX_PAD_CTRL(UART_PAD_CTRL)),
	IOMUX_PADS(PAD_CSI0_DAT11__UART1_RX_DATA | MUX_PAD_CTRL(UART_PAD_CTRL)),
};

static iomux_v3_cfg_t const spi_nor_pads[] = {
	IOMUX_PADS(PAD_DISP0_DAT19__ECSPI2_SCLK | MUX_PAD_CTRL(SPI_PAD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT17__ECSPI2_MISO | MUX_PAD_CTRL(SPI_PAD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT16__ECSPI2_MOSI | MUX_PAD_CTRL(SPI_PAD_CTRL)),
	IOMUX_PADS(PAD_DISP0_DAT18__GPIO5_IO12 | MUX_PAD_CTRL(SPI_PAD_CTRL)), // CS
};

#endif /* __VEC6200_PINS_H__ */
