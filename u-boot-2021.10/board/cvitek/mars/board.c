// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2013
 * David Feng <fenghua@phytium.com.cn>
 * Sharma Bhupesh <bhupesh.sharma@freescale.com>
 */

#include <common.h>
#include <dm.h>
#include <dm/uclass.h>
#include <env.h>
#include <i2c.h>
#include <malloc.h>
#include <errno.h>
#include <net.h>
#include <u-boot/md5.h>
#include <asm/io.h>
#include <linux/compiler.h>
#if defined(__aarch64__)
#include <asm/armv8/mmu.h>
#endif
#include <usb/dwc2_udc.h>
#include <usb.h>
#include "mars_reg.h"
#include "mmio.h"
#include "mars_reg_fmux_gpio.h"
#include "mars_pinlist_swconfig.h"
#include <linux/delay.h>
#include <bootstage.h>
#include <cvi_efuse.h>

#if defined(__riscv)
#include <asm/csr.h>
#endif

DECLARE_GLOBAL_DATA_PTR;
#define SD1_SDIO_PAD

#if defined(__aarch64__)
static struct mm_region cv181x_mem_map[] = {
	{
		.virt = 0x0UL,
		.phys = 0x0UL,
		.size = 0x80000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		.virt = PHYS_SDRAM_1,
		.phys = PHYS_SDRAM_1,
		.size = PHYS_SDRAM_1_SIZE,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		/* List terminator */
		0,
	}
};

struct mm_region *mem_map = cv181x_mem_map;
#endif

// #define PINMUX_CONFIG(PIN_NAME, FUNC_NAME) printf ("%s\n", PIN_NAME ##_ ##FUNC_NAME);
#define PINMUX_CONFIG(PIN_NAME, FUNC_NAME) \
		mmio_clrsetbits_32(PINMUX_BASE + FMUX_GPIO_FUNCSEL_##PIN_NAME, \
			FMUX_GPIO_FUNCSEL_##PIN_NAME##_MASK << FMUX_GPIO_FUNCSEL_##PIN_NAME##_OFFSET, \
			PIN_NAME##__##FUNC_NAME)

void pinmux_config(int io_type)
{
		switch (io_type) {
		case PINMUX_UART0:
			PINMUX_CONFIG(UART0_RX, UART0_RX);
			PINMUX_CONFIG(UART0_TX, UART0_TX);
		break;
		case PINMUX_SDIO0:
			PINMUX_CONFIG(SD0_CD, SDIO0_CD);
			// on licheervnano, this pin use for led
			//PINMUX_CONFIG(SD0_PWR_EN, SDIO0_PWR_EN);
			PINMUX_CONFIG(SD0_CMD, SDIO0_CMD);
			PINMUX_CONFIG(SD0_CLK, SDIO0_CLK);
			PINMUX_CONFIG(SD0_D0, SDIO0_D_0);
			PINMUX_CONFIG(SD0_D1, SDIO0_D_1);
			PINMUX_CONFIG(SD0_D2, SDIO0_D_2);
			PINMUX_CONFIG(SD0_D3, SDIO0_D_3);
			break;
		case PINMUX_SDIO1:
#if defined(SD1_SDIO_PAD)
			/*
			 * Name            Address            SD1  MIPI
			 * reg_sd1_phy_sel REG_0x300_0294[10] 0x0  0x1
			 */
			mmio_write_32(TOP_BASE + 0x294,
				      (mmio_read_32(TOP_BASE + 0x294) & 0xFFFFFBFF));
			PINMUX_CONFIG(SD1_CMD, PWR_SD1_CMD_VO36);
			PINMUX_CONFIG(SD1_CLK, PWR_SD1_CLK_VO37);
			PINMUX_CONFIG(SD1_D0, PWR_SD1_D0_VO35);
			PINMUX_CONFIG(SD1_D1, PWR_SD1_D1_VO34);
			PINMUX_CONFIG(SD1_D2, PWR_SD1_D2_VO33);
			PINMUX_CONFIG(SD1_D3, PWR_SD1_D3_VO32);
#elif defined(SD1_MIPI_PAD)
			/*
			 * Name            Address            SD1  MIPI
			 * reg_sd1_phy_sel REG_0x300_0294[10] 0x0  0x1
			 */
			mmio_write_32(TOP_BASE + 0x294,
				      (mmio_read_32(TOP_BASE + 0x294) & 0xFFFFFBFF) | BIT(10));
			PINMUX_CONFIG(PAD_MIPI_TXM4, SD1_CLK);
			PINMUX_CONFIG(PAD_MIPI_TXP4, SD1_CMD);
			PINMUX_CONFIG(PAD_MIPI_TXM3, SD1_D0);
			PINMUX_CONFIG(PAD_MIPI_TXP3, SD1_D1);
			PINMUX_CONFIG(PAD_MIPI_TXM2, SD1_D2);
			PINMUX_CONFIG(PAD_MIPI_TXP2, SD1_D3);
#endif
			break;
		case PINMUX_EMMC:
			PINMUX_CONFIG(EMMC_CLK, EMMC_CLK);
			PINMUX_CONFIG(EMMC_RSTN, EMMC_RSTN);
			PINMUX_CONFIG(EMMC_CMD, EMMC_CMD);
			PINMUX_CONFIG(EMMC_DAT1, EMMC_DAT_1);
			PINMUX_CONFIG(EMMC_DAT0, EMMC_DAT_0);
			PINMUX_CONFIG(EMMC_DAT2, EMMC_DAT_2);
			PINMUX_CONFIG(EMMC_DAT3, EMMC_DAT_3);
			break;
		case PINMUX_SPI_NAND:
			PINMUX_CONFIG(EMMC_DAT2, SPINAND_HOLD);
			PINMUX_CONFIG(EMMC_CLK, SPINAND_CLK);
			PINMUX_CONFIG(EMMC_DAT0, SPINAND_MOSI);
			PINMUX_CONFIG(EMMC_DAT3, SPINAND_WP);
			PINMUX_CONFIG(EMMC_CMD, SPINAND_MISO);
			PINMUX_CONFIG(EMMC_DAT1, SPINAND_CS);
		break;
		case PINMUX_DSI:
			PINMUX_CONFIG(PAD_MIPI_TXM0, XGPIOC_12);
			PINMUX_CONFIG(PAD_MIPI_TXP0, XGPIOC_13);
			PINMUX_CONFIG(PAD_MIPI_TXM1, XGPIOC_14);
			PINMUX_CONFIG(PAD_MIPI_TXP1, XGPIOC_15);
			PINMUX_CONFIG(PAD_MIPI_TXM2, XGPIOC_16);
			PINMUX_CONFIG(PAD_MIPI_TXP2, XGPIOC_17);
			PINMUX_CONFIG(PAD_MIPI_TXM3, XGPIOC_20);
			PINMUX_CONFIG(PAD_MIPI_TXP3, XGPIOC_21);
			PINMUX_CONFIG(PAD_MIPI_TXM4, XGPIOC_18);
			PINMUX_CONFIG(PAD_MIPI_TXP4, XGPIOC_19);
		break;
		case PINMUX_LVDS:
			PINMUX_CONFIG(PAD_MIPI_TXM0, XGPIOC_12);
			PINMUX_CONFIG(PAD_MIPI_TXP0, XGPIOC_13);
			PINMUX_CONFIG(PAD_MIPI_TXM1, XGPIOC_14);
			PINMUX_CONFIG(PAD_MIPI_TXP1, XGPIOC_15);
			PINMUX_CONFIG(PAD_MIPI_TXM2, XGPIOC_16);
			PINMUX_CONFIG(PAD_MIPI_TXP2, XGPIOC_17);
			PINMUX_CONFIG(PAD_MIPI_TXM3, XGPIOC_20);
			PINMUX_CONFIG(PAD_MIPI_TXP3, XGPIOC_21);
			PINMUX_CONFIG(PAD_MIPI_TXM4, XGPIOC_18);
			PINMUX_CONFIG(PAD_MIPI_TXP4, XGPIOC_19);
		break;
		default:
			break;
	}
}

#include "../cvi_board_init.c"

#if defined(CONFIG_PHY_CVITEK) /* config cvitek cv181x eth internal phy on ASIC board */
static void cv181x_ephy_id_init(void)
{
	// set rg_ephy_apb_rw_sel 0x0804@[0]=1/APB by using APB interface
	mmio_write_32(0x03009804, 0x0001);

	// Release 0x0800[0]=0/shutdown
	mmio_write_32(0x03009800, 0x0900);

	// Release 0x0800[2]=1/dig_rst_n, Let mii_reg can be accessabile
	mmio_write_32(0x03009800, 0x0904);

	// ANA INIT (PD/EN), switch to MII-page5
	mmio_write_32(0x0300907c, 0x0500);
	// Release ANA_PD p5.0x10@[13:8] = 6'b001100
	mmio_write_32(0x03009040, 0x0c00);
	// Release ANA_EN p5.0x10@[7:0] = 8'b01111110
	mmio_write_32(0x03009040, 0x0c7e);

	// Wait PLL_Lock, Lock_Status p5.0x12@[15] = 1
	//mdelay(1);

	// Release 0x0800[1] = 1/ana_rst_n
	mmio_write_32(0x03009800, 0x0906);

	// ANA INIT
	// @Switch to MII-page5
	mmio_write_32(0x0300907c, 0x0500);

	// PHY_ID
	mmio_write_32(0x03009008, 0x0043);
	mmio_write_32(0x0300900c, 0x5649);

	// switch to MDIO control by ETH_MAC
	mmio_write_32(0x03009804, 0x0000);
}

static void cv181x_ephy_led_pinmux(void)
{
	// LED PAD MUX
	mmio_write_32(0x030010e0, 0x05);
	mmio_write_32(0x030010e4, 0x05);
	//(SD1_CLK selphy)
	mmio_write_32(0x050270b0, 0x11111111);
	//(SD1_CMD selphy)
	mmio_write_32(0x050270b4, 0x11111111);
}
#endif

void cpu_pwr_ctrl(void)
{
#if defined(CONFIG_RISCV)
	mmio_write_32(0x01901008, 0x30001);// cortexa53_pwr_iso_en
#elif defined(CONFIG_ARM)
	mmio_write_32(0x01901004, 0x30001);// c906_top_pwr_iso_en
#endif
}

int board_init(void)
{
#ifndef CONFIG_TARGET_CVITEK_CV181X_FPGA
	extern volatile u32 BOOT0_START_TIME;
	u16 start_time = DIV_ROUND_UP(BOOT0_START_TIME, SYS_COUNTER_FREQ_IN_SECOND / 1000);

	// Save uboot start time. time is from boot0.h
	mmio_write_16(TIME_RECORDS_FIELD_UBOOT_START, start_time);
#endif

	cpu_pwr_ctrl();

#if defined(CONFIG_PHY_CVITEK) /* config cvitek cv181x eth internal phy on ASIC board */
	cv181x_ephy_id_init();
	cv181x_ephy_led_pinmux();
#endif

#if defined(CONFIG_NAND_SUPPORT)
	pinmux_config(PINMUX_SPI_NAND);
#elif defined(CONFIG_SPI_FLASH)
	pinmux_config(PINMUX_SPI_NOR);
#elif defined(CONFIG_EMMC_SUPPORT)
	pinmux_config(PINMUX_EMMC);
#endif
#ifdef CONFIG_DISPLAY_CVITEK_MIPI
	pinmux_config(PINMUX_DSI);
#elif defined(CONFIG_DISPLAY_CVITEK_LVDS)
	pinmux_config(PINMUX_LVDS);
#endif
	pinmux_config(PINMUX_SDIO1);
	cvi_board_init();
	return 0;
}

#if defined(__riscv)
#define CVITEK_UID_WORD3_OFFSET	0x0c
#define CVITEK_UID_WORD4_OFFSET	0x10
#define CVITEK_UID_STRING_LEN	17

#define ONEKVM_PCIE_OLED_ADDR		0x3c
#define ONEKVM_PCIE_OLED_WIDTH		64
#define ONEKVM_PCIE_OLED_PAGES		4
#define ONEKVM_PCIE_OLED_COLUMN_OFFSET	32
#define ONEKVM_PCIE_OLED_PAGE_OFFSET	8
#define ONEKVM_CUBE_OLED_ADDR		0x3d
#define ONEKVM_CUBE_OLED_WIDTH		128
#define ONEKVM_CUBE_OLED_PAGES		8
#define ONEKVM_CUBE_OLED_COLUMN_OFFSET	0
#define ONEKVM_CUBE_OLED_PAGE_OFFSET	0
#define ONEKVM_OLED_MAX_WIDTH		ONEKVM_CUBE_OLED_WIDTH
#define ONEKVM_OLED_ENABLE_GPIO		22
#define ONEKVM_OLED_PROBE_ATTEMPTS	4
#define ONEKVM_OLED_PROBE_DELAY_MS	5
#define CVITEK_GPIOA_DR			0x03020000
#define CVITEK_GPIOA_DDR		0x03020004
#define CVITEK_GPIOA_EXT		0x03020050

/*
 * Captured from assets/canvases/uboot-pcie.json in the NanoKVM OLED
 * extension: "BOOTING" / "PLEASE WAIT", 64x32, rotation 0 degrees.
 */
static const u8 onekvm_pcie_oled_boot_frame[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3e, 0x2a, 0x2a, 0x2a, 0x14,
	0x00, 0x1c, 0x22, 0x22, 0x22, 0x1c, 0x00, 0x1c,
	0x22, 0x22, 0x22, 0x1c, 0x00, 0x02, 0x02, 0x3e,
	0x02, 0x02, 0x00, 0x22, 0x22, 0x3e, 0x22, 0x22,
	0x00, 0x3e, 0x04, 0x08, 0x10, 0x3e, 0x00, 0x1c,
	0x22, 0x2a, 0x2a, 0x3a, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x3e, 0x0a, 0x0a, 0x0a, 0x04, 0x00, 0x3e, 0x20,
	0x20, 0x20, 0x20, 0x00, 0x3e, 0x2a, 0x2a, 0x2a,
	0x22, 0x00, 0x3c, 0x0a, 0x0a, 0x0a, 0x3c, 0x00,
	0x24, 0x2a, 0x2a, 0x2a, 0x12, 0x00, 0x3e, 0x2a,
	0x2a, 0x2a, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x3e, 0x10, 0x08, 0x10, 0x3e, 0x00, 0x3c, 0x0a,
	0x0a, 0x0a, 0x3c, 0x00, 0x22, 0x22, 0x3e, 0x22,
	0x22, 0x00, 0x02, 0x02, 0x3e, 0x02, 0x02, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/*
 * Rendered from assets/canvases/uboot-cube.json in the NanoKVM OLED
 * extension.  The source is the Cube's active two-layer canvas:
 * "PLEASE WAIT" / "BOOTING", 128x64, rotation 0 degrees.  The byte pairs
 * are run length followed by value and expand to 1024 controller bytes.
 * Source canvas SHA-256:
 * d880d157a64a8cce20b889a23ddc4727ac18131186a2776f5a063199a8827699
 */
static const u8 onekvm_cube_oled_boot_frame_rle[] = {
	0xff, 0x00, 0x02, 0x00, 0x02, 0xfe, 0x06, 0x66,
	0x02, 0x18, 0x02, 0x00, 0x02, 0xfe, 0x0a, 0x00,
	0x02, 0xfe, 0x06, 0x66, 0x02, 0x06, 0x02, 0x00,
	0x02, 0xf8, 0x06, 0x66, 0x02, 0xf8, 0x02, 0x00,
	0x02, 0x18, 0x06, 0x66, 0x02, 0x86, 0x02, 0x00,
	0x02, 0xfe, 0x06, 0x66, 0x02, 0x06, 0x0a, 0x00,
	0x02, 0xfe, 0x02, 0x80, 0x02, 0x60, 0x02, 0x80,
	0x02, 0xfe, 0x02, 0x00, 0x02, 0xf8, 0x06, 0x66,
	0x02, 0xf8, 0x02, 0x00, 0x04, 0x06, 0x02, 0xfe,
	0x04, 0x06, 0x02, 0x00, 0x04, 0x06, 0x02, 0xfe,
	0x04, 0x06, 0x02, 0x00, 0x02, 0x07, 0x0a, 0x00,
	0x02, 0x07, 0x08, 0x06, 0x02, 0x00, 0x02, 0x07,
	0x08, 0x06, 0x02, 0x00, 0x02, 0x07, 0x06, 0x00,
	0x02, 0x07, 0x02, 0x00, 0x08, 0x06, 0x02, 0x01,
	0x02, 0x00, 0x02, 0x07, 0x08, 0x06, 0x0a, 0x00,
	0x02, 0x07, 0x02, 0x01, 0x02, 0x00, 0x02, 0x01,
	0x02, 0x07, 0x02, 0x00, 0x02, 0x07, 0x06, 0x00,
	0x02, 0x07, 0x02, 0x00, 0x04, 0x06, 0x02, 0x07,
	0x04, 0x06, 0x06, 0x00, 0x02, 0x07, 0x1c, 0x00,
	0x02, 0xe0, 0x06, 0x60, 0x02, 0x80, 0x02, 0x00,
	0x02, 0x80, 0x06, 0x60, 0x02, 0x80, 0x02, 0x00,
	0x02, 0x80, 0x06, 0x60, 0x02, 0x80, 0x02, 0x00,
	0x04, 0x60, 0x02, 0xe0, 0x04, 0x60, 0x02, 0x00,
	0x04, 0x60, 0x02, 0xe0, 0x04, 0x60, 0x02, 0x00,
	0x02, 0xe0, 0x02, 0x80, 0x04, 0x00, 0x02, 0xe0,
	0x02, 0x00, 0x02, 0x80, 0x08, 0x60, 0x2e, 0x00,
	0x02, 0x7f, 0x06, 0x66, 0x02, 0x19, 0x02, 0x00,
	0x02, 0x1f, 0x06, 0x60, 0x02, 0x1f, 0x02, 0x00,
	0x02, 0x1f, 0x06, 0x60, 0x02, 0x1f, 0x06, 0x00,
	0x02, 0x7f, 0x06, 0x00, 0x04, 0x60, 0x02, 0x7f,
	0x04, 0x60, 0x02, 0x00, 0x02, 0x7f, 0x02, 0x01,
	0x02, 0x06, 0x02, 0x18, 0x02, 0x7f, 0x02, 0x00,
	0x02, 0x1f, 0x02, 0x60, 0x04, 0x66, 0x02, 0x7e,
	0xff, 0x00, 0x18, 0x00,
};

struct onekvm_oled_profile {
	const char *name;
	u8 address;
	u8 width;
	u8 pages;
	u8 column_offset;
	u8 page_offset;
	u8 segment_remap;
	u8 com_scan;
	const u8 *frame;
	size_t frame_size;
	bool frame_is_rle;
};

static const struct onekvm_oled_profile onekvm_pcie_oled = {
	.name = "PCIe",
	.address = ONEKVM_PCIE_OLED_ADDR,
	.width = ONEKVM_PCIE_OLED_WIDTH,
	.pages = ONEKVM_PCIE_OLED_PAGES,
	.column_offset = ONEKVM_PCIE_OLED_COLUMN_OFFSET,
	.page_offset = ONEKVM_PCIE_OLED_PAGE_OFFSET,
	.segment_remap = 0xa0,
	.com_scan = 0xc0,
	.frame = onekvm_pcie_oled_boot_frame,
	.frame_size = ARRAY_SIZE(onekvm_pcie_oled_boot_frame),
	.frame_is_rle = false,
};

static const struct onekvm_oled_profile onekvm_cube_oled = {
	.name = "Cube",
	.address = ONEKVM_CUBE_OLED_ADDR,
	.width = ONEKVM_CUBE_OLED_WIDTH,
	.pages = ONEKVM_CUBE_OLED_PAGES,
	.column_offset = ONEKVM_CUBE_OLED_COLUMN_OFFSET,
	.page_offset = ONEKVM_CUBE_OLED_PAGE_OFFSET,
	.segment_remap = 0xa1,
	.com_scan = 0xc8,
	.frame = onekvm_cube_oled_boot_frame_rle,
	.frame_size = ARRAY_SIZE(onekvm_cube_oled_boot_frame_rle),
	.frame_is_rle = true,
};

static void onekvm_prepare_oled(void)
{
	/* GPIO-I2C: SCL=A15, SDA=A27; OLED enable/reset: A22. */
	PINMUX_CONFIG(SPK_EN, XGPIOA_15);
	PINMUX_CONFIG(EMMC_DAT3, XGPIOA_27);
	PINMUX_CONFIG(EMMC_CLK, XGPIOA_22);

	/* Give the controller a defined reset/power cycle before probing it. */
	mmio_clrbits_32(CVITEK_GPIOA_DR, BIT(ONEKVM_OLED_ENABLE_GPIO));
	mmio_setbits_32(CVITEK_GPIOA_DDR, BIT(ONEKVM_OLED_ENABLE_GPIO));
	mdelay(1);
	mmio_setbits_32(CVITEK_GPIOA_DR, BIT(ONEKVM_OLED_ENABLE_GPIO));
	mdelay(ONEKVM_OLED_PROBE_DELAY_MS);
}

static int onekvm_oled_probe(struct udevice *bus, u8 address)
{
	struct dm_i2c_ops *ops = i2c_get_ops(bus);
	struct i2c_msg message = {
		.addr = address,
		.flags = 0,
		.len = 0,
		.buf = NULL,
	};
	int ret;

	if (ops->probe_chip) {
		ret = ops->probe_chip(bus, address, 0);
		if (ret != -ENOSYS)
			return ret;
	}
	if (!ops->xfer)
		return -ENOSYS;
	return ops->xfer(bus, &message, 1);
}

static int onekvm_oled_write(struct udevice *bus, u8 address, u8 control,
			     const u8 *payload, size_t length)
{
	struct dm_i2c_ops *ops = i2c_get_ops(bus);
	u8 transfer[ONEKVM_OLED_MAX_WIDTH + 1];
	struct i2c_msg message = {
		.addr = address,
		.flags = 0,
		.len = length + 1,
		.buf = transfer,
	};

	if (length > ONEKVM_OLED_MAX_WIDTH)
		return -EINVAL;
	if (!ops->xfer)
		return -ENOSYS;
	transfer[0] = control;
	memcpy(transfer + 1, payload, length);
	return ops->xfer(bus, &message, 1);
}

static int onekvm_oled_probe_retries(struct udevice *bus, u8 address)
{
	int attempt;
	int ret = -ENODEV;

	for (attempt = 1; attempt <= ONEKVM_OLED_PROBE_ATTEMPTS; ++attempt) {
		ret = onekvm_oled_probe(bus, address);
		if (!ret)
			break;
		if (attempt < ONEKVM_OLED_PROBE_ATTEMPTS)
			mdelay(ONEKVM_OLED_PROBE_DELAY_MS);
	}
	return ret;
}

static int onekvm_show_oled(const char **detected_name)
{
	static const u8 init_sequence[] = {
		0xae, 0x00, 0x10, 0x40, 0x81, 0xcf, 0xa1, 0xc8,
		0xa6, 0xa8, 0x3f, 0xd3, 0x00, 0xd5, 0x80, 0xd9,
		0xf1, 0xda, 0x12, 0xdb, 0x40, 0x20, 0x02, 0x8d,
		0x14, 0xa4, 0xa6, 0xaf,
	};
	static const struct onekvm_oled_profile * const profiles[] = {
		&onekvm_cube_oled,
		&onekvm_pcie_oled,
	};
	const struct onekvm_oled_profile *profile = NULL;
	struct udevice *bus;
	u8 page_frame[ONEKVM_OLED_MAX_WIDTH];
	size_t rle_index = 0;
	unsigned int run_remaining = 0;
	u8 run_value = 0;
	ofnode node;
	size_t candidate;
	int page;
	int ret;

	onekvm_prepare_oled();
	node = ofnode_path("/i2c5@gpio");
	if (!ofnode_valid(node))
		return -ENOENT;
	ret = uclass_get_device_by_ofnode(UCLASS_I2C, node, &bus);
	if (ret)
		return ret;
	for (candidate = 0; candidate < ARRAY_SIZE(profiles); ++candidate) {
		ret = onekvm_oled_probe_retries(bus,
						profiles[candidate]->address);
		if (!ret) {
			profile = profiles[candidate];
			break;
		}
	}
	if (!profile) {
		printf("OneKVM: OLED probes at 0x%02x/0x%02x failed "
		       "(last=%d), GPIOA DR=%08x DDR=%08x EXT=%08x\n",
		       ONEKVM_CUBE_OLED_ADDR, ONEKVM_PCIE_OLED_ADDR, ret,
		       mmio_read_32(CVITEK_GPIOA_DR),
		       mmio_read_32(CVITEK_GPIOA_DDR),
		       mmio_read_32(CVITEK_GPIOA_EXT));
		return -ENODEV;
	}
	if (detected_name)
		*detected_name = profile->name;

	ret = onekvm_oled_write(bus, profile->address, 0x00, init_sequence,
				 ARRAY_SIZE(init_sequence));
	if (ret)
		return ret;
	{
		const u8 direction[] = { profile->segment_remap,
					 profile->com_scan };

		ret = onekvm_oled_write(bus, profile->address, 0x00,
					 direction, ARRAY_SIZE(direction));
		if (ret)
			return ret;
	}

	if (!profile->frame_is_rle &&
	    profile->frame_size != profile->width * profile->pages)
		return -EINVAL;
	for (page = 0; page < profile->pages; ++page) {
		const u8 position[] = {
			0xb0 + profile->page_offset + page,
			0x10 | (profile->column_offset >> 4),
			profile->column_offset & 0x0f,
		};
		int column = 0;

		if (profile->frame_is_rle) {
			while (column < profile->width) {
				unsigned int count;

				if (!run_remaining) {
					if (rle_index + 2 > profile->frame_size)
						return -EINVAL;
					run_remaining = profile->frame[rle_index++];
					run_value = profile->frame[rle_index++];
					if (!run_remaining)
						return -EINVAL;
				}
				count = profile->width - column;
				if (count > run_remaining)
					count = run_remaining;
				memset(page_frame + column, run_value, count);
				column += count;
				run_remaining -= count;
			}
		} else {
			memcpy(page_frame, profile->frame + page * profile->width,
			       profile->width);
		}

		ret = onekvm_oled_write(bus, profile->address, 0x00, position,
					 ARRAY_SIZE(position));
		if (ret)
			return ret;
		ret = onekvm_oled_write(bus, profile->address, 0x40,
					 page_frame, profile->width);
		if (ret)
			return ret;
	}
	if (profile->frame_is_rle &&
	    (run_remaining || rle_index != profile->frame_size))
		return -EINVAL;

	return 0;
}

static int cvitek_set_ethaddr_from_uid(u8 mac[ARP_HLEN])
{
	s64 uid_3;
	s64 uid_4;
	char uid[CVITEK_UID_STRING_LEN + 1];
	u8 digest[MD5_SUM_LEN];

	uid_3 = cvi_efuse_read_from_shadow(CVITEK_UID_WORD3_OFFSET);
	uid_4 = cvi_efuse_read_from_shadow(CVITEK_UID_WORD4_OFFSET);
	if (uid_3 < 0 || uid_4 < 0)
		return -EIO;

	snprintf(uid, sizeof(uid), "%08x_%08x", (u32)uid_3, (u32)uid_4);
	md5((unsigned char *)uid, strlen(uid), digest);
	memcpy(mac, digest, ARP_HLEN);
	mac[0] = (mac[0] & 0xfe) | 0x02;

	if (!is_valid_ethaddr(mac))
		return -EINVAL;

	return eth_env_set_enetaddr("ethaddr", mac);
}

int misc_init_r(void)
{
	u8 mac[ARP_HLEN];
	int ret;

	ret = cvitek_set_ethaddr_from_uid(mac);
	if (ret) {
		printf("OneKVM: failed to derive ethaddr from chip UID: %d\n",
		       ret);
	} else {
		printf("OneKVM: ethaddr=%02x:%02x:%02x:%02x:%02x:%02x "
		       "(chip UID)\n",
		       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	}

	return 0;
}
#endif

#if defined(__aarch64__)
int dram_init(void)
{
	gd->ram_size = PHYS_SDRAM_1_SIZE;
	return 0;
}

int dram_init_banksize(void)
{
	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;

	return 0;
}
#endif

#ifdef CV_SYS_OFF
static void cv_system_off(void)
{
	mmio_write_32(REG_RTC_BASE + RTC_EN_SHDN_REQ, 0x01);
	while (mmio_read_32(REG_RTC_BASE + RTC_EN_SHDN_REQ) != 0x01)
		;
	mmio_write_32(REG_RTC_CTRL_BASE + RTC_CTRL0_UNLOCKKEY, 0xAB18);
	mmio_setbits_32(REG_RTC_CTRL_BASE + RTC_CTRL0, 0xFFFF0800 | (0x1 << 0));

	while (1)
		;
}
#endif

void cv_system_reset(void)
{
	mmio_write_32(REG_RTC_BASE + RTC_EN_WARM_RST_REQ, 0x01);
	while (mmio_read_32(REG_RTC_BASE + RTC_EN_WARM_RST_REQ) != 0x01)
		;
	mmio_write_32(REG_RTC_CTRL_BASE + RTC_CTRL0_UNLOCKKEY, 0xAB18);
	mmio_setbits_32(REG_RTC_CTRL_BASE + RTC_CTRL0, 0xFFFF0800 | (0x1 << 4));

	while (1)
		;
}

/*
 * Board specific reset that is system reset.
 */
void reset_cpu(void)
{
	cv_system_reset();
}

#ifdef CONFIG_USB_GADGET_DWC2_OTG
struct dwc2_plat_otg_data cv182x_otg_data = {
	.regs_otg = USB_BASE,
	.usb_gusbcfg    = 0x40081400,
	.rx_fifo_sz     = 512,
	.np_tx_fifo_sz  = 512,
	.tx_fifo_sz     = 512,
};

int board_usb_init(int index, enum usb_init_type init)
{
	u32 value;

	value = mmio_read_32(TOP_BASE + REG_TOP_SOFT_RST) & (~BIT_TOP_SOFT_RST_USB);
	mmio_write_32(TOP_BASE + REG_TOP_SOFT_RST, value);
	udelay(50);
	value = mmio_read_32(TOP_BASE + REG_TOP_SOFT_RST) | BIT_TOP_SOFT_RST_USB;
	mmio_write_32(TOP_BASE + REG_TOP_SOFT_RST, value);

	/* Set USB phy configuration */
	value = mmio_read_32(REG_TOP_USB_PHY_CTRL);
	mmio_write_32(REG_TOP_USB_PHY_CTRL, value | BIT_TOP_USB_PHY_CTRL_EXTVBUS
					| USB_PHY_ID_OVERRIDE_ENABLE
					| USB_PHY_ID_VALUE);

	/* Enable ECO RXF */
	mmio_write_32(REG_TOP_USB_ECO, mmio_read_32(REG_TOP_USB_ECO) | BIT_TOP_USB_ECO_RX_FLUSH);

	printf("cvi_usb_hw_init done\n");

	return dwc2_udc_probe(&cv182x_otg_data);
}
#endif

void board_save_time_record(uintptr_t saveaddr)
{
	u64 boot_us = 0;
#if defined(__aarch64__)
	boot_us = timer_get_boot_us();
#elif defined(__riscv)
	// Read from CSR_TIME directly. RISC-V timers is initialized later.
	boot_us = csr_read(CSR_TIME) / (SYS_COUNTER_FREQ_IN_SECOND / 1000000);
#else
#error "Unknown ARCH"
#endif

	mmio_write_16(saveaddr, DIV_ROUND_UP(boot_us, 1000));
}
