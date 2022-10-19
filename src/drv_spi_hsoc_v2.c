/***************************************************************************
 *   Copyright (C) 2010 by Hilscher GmbH                                   *
 *   cthelen@hilscher.com                                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/


#include <string.h>

#include "drv_spi_hsoc_v2.h"

#include "netx_io_areas.h"


#if ASIC_TYP==ASIC_TYP_NETX50 || ASIC_TYP==ASIC_TYP_NETX10 || ASIC_TYP==ASIC_TYP_NETX56
#include "mmio.h"
static const HOSTMMIODEF aatMmioValues[3][4] =
{
	/*
	 * Chip select 0
	 */
	{
		HOSTMMIO(spi1_cs0n),		/* chip select */
		HOSTMMIO(spi1_clk),		/* clock */
		HOSTMMIO(spi1_miso),		/* MISO */
		HOSTMMIO(spi1_mosi)		/* MOSI */
	},

	/*
	 * Chip select 1
	 */
	{
		HOSTMMIO(spi1_cs1n),		/* chip select */
		HOSTMMIO(spi1_clk),		/* clock */
		HOSTMMIO(spi1_miso),		/* MISO */
		HOSTMMIO(spi1_mosi)		/* MOSI */
	},

	/*
	 * Chip select 2
	 */
	{
		HOSTMMIO(spi1_cs2n),		/* chip select */
		HOSTMMIO(spi1_clk),		/* clock */
		HOSTMMIO(spi1_miso),		/* MISO */
		HOSTMMIO(spi1_mosi)		/* MOSI */
	}
};
#elif ASIC_TYP==ASIC_TYP_NETX4000
#include "mmio.h"

static const HOSTMMIODEF aatMmioValues_SPI0[3][4] =
{
	/*
	 * For SPI0 
	 * Chip select 0
	 */
	{
		HOSTMMIO(SPI0_CS0N),		/* chip select */
		HOSTMMIO(SPI0_CLK),		/* clock */
		HOSTMMIO(SPI0_MISO),		/* MISO */
		HOSTMMIO(SPI0_MOSI)		/* MOSI */
	},
	/*
	 * Chip select 1
	 */
	{
		HOSTMMIO(SPI0_CS1N),		/* chip select */
		HOSTMMIO(SPI0_CLK),		/* clock */
		HOSTMMIO(SPI0_MISO),		/* MISO */
		HOSTMMIO(SPI0_MOSI)		/* MOSI */
	},
	/*
	 * Chip select 2
	 */
	{
		HOSTMMIO(SPI0_CS2N),		/* chip select */
		HOSTMMIO(SPI0_CLK),		/* clock */
		HOSTMMIO(SPI0_MISO),		/* MISO */
		HOSTMMIO(SPI0_MOSI)		/* MOSI */
	},
};

static const HOSTMMIODEF aatMmioValues_SPI1[3][4] =
{
	/*
	 * For SPI1/XPIC3 SPI
	 * Chip select 0
	 */
	{
		HOSTMMIO(SPI1_CS0N),		/* chip select */
		HOSTMMIO(SPI1_CLK),		/* clock */
		HOSTMMIO(SPI1_MISO),		/* MISO */
		HOSTMMIO(SPI1_MOSI)		/* MOSI */
	},
	/*
	 * Chip select 1
	 */
	{
		HOSTMMIO(SPI1_CS1N),		/* chip select */
		HOSTMMIO(SPI1_CLK),		/* clock */
		HOSTMMIO(SPI1_MISO),		/* MISO */
		HOSTMMIO(SPI1_MOSI)		/* MOSI */
	},
	/*
	 * Chip select 2
	 */
	{
		HOSTMMIO(SPI1_CS2N),		/* chip select */
		HOSTMMIO(SPI1_CLK),		/* clock */
		HOSTMMIO(SPI1_MISO),		/* MISO */
		HOSTMMIO(SPI1_MOSI)		/* MOSI */
	},
};

#endif

static unsigned char spi_exchange_byte(const FLASHER_SPI_CFG_T *ptCfg, unsigned char ucByte)
{
	HOSTADEF(SPI) *ptSpiUnit;
	unsigned long ulValue;


	/* Get the pointer to the registers. */
	ptSpiUnit = ptCfg->pvUnit;

	/* Send the byte. */
	ptSpiUnit->ulSpi_dr = ucByte;

	/* Wait for one byte in the FIFO. */
	do
	{
		ulValue  = ptSpiUnit->ulSpi_sr;
		ulValue &= HOSTMSK(spi_sr_RNE);
	} while( ulValue==0 );

	/* Grab the byte. */
	ucByte = (unsigned char)(ptSpiUnit->ulSpi_dr);
	return ucByte;
}

/*-----------------------------------*/


static unsigned long spi_get_device_speed_representation(const FLASHER_SPI_CFG_T *ptCfg, unsigned int uiSpeed)
{
	unsigned long ulDevSpeed;
	unsigned long ulInputFilter;
	unsigned long ulMaximumSpeedKhz;


	/* ulSpeed is in kHz */

	/* Limit the speed. */
	ulMaximumSpeedKhz = ptCfg->ulMaximumSpeedKhz;
	if( uiSpeed>ulMaximumSpeedKhz )
	{
		uiSpeed = ulMaximumSpeedKhz;
	}

	/* Limit the speed to the maximum possible value of the hardware. */
	if( uiSpeed>50000 )
	{
		uiSpeed = 50000;
	}

	/* convert speed to "multiply add value" */
	ulDevSpeed  = uiSpeed * 4096;

	/* NOTE: do not round up here */
	ulDevSpeed /= 100000;

	/* use input filtering? */
	ulInputFilter = 0;
	if( ulDevSpeed<=0x0200 )
	{
		ulInputFilter = HOSTMSK(spi_cr0_filter_in);
	}

	/* shift to register position */
	ulDevSpeed <<= HOSTSRT(spi_cr0_sck_muladd);

	/* add filter bit */
	ulDevSpeed |= ulInputFilter;

	return ulDevSpeed;
}

/*** Convert a Spi Frequency in device speed representation into a kHz value.
 * @param ptCfg The configuration struct of the Spi Interface
 * @param ulDeviceSpeed The device speed representation of the Spi Frequency
 * 
 * @return The Spi Frequency in kHz
*/
static unsigned long spi_get_khz_speed_representation(const FLASHER_SPI_CFG_T* ptCfg, const unsigned long ulDeviceSpeed)
{
	unsigned long ulKHzSpeed;
	unsigned long ulInputFilter;

	/* Remove input filter bit */
	ulInputFilter = HOSTMSK(spi_cr0_filter_in);
	ulKHzSpeed = ulDeviceSpeed & (~ulInputFilter);

	// shift to value position
	ulKHzSpeed >>= HOSTSRT(spi_cr0_sck_muladd);

	ulKHzSpeed *= 100000;

	/* reconvert speed from "multiply add value" */
	ulKHzSpeed /= 4096;

	return ulKHzSpeed;
}

static int spi_slave_select(const FLASHER_SPI_CFG_T *ptCfg, int fIsSelected)
{
	HOSTADEF(SPI) *ptSpiUnit;
	unsigned long uiChipSelect;
	unsigned long ulValue;


	/* Get the pointer to the registers. */
	ptSpiUnit = ptCfg->pvUnit;

	/* get the chip select value */
	uiChipSelect = 0;
	if( fIsSelected!=0 )
	{
		uiChipSelect  = ptCfg->uiChipSelect << HOSTSRT(spi_cr1_fss);
		uiChipSelect &= HOSTMSK(spi_cr1_fss);
	}

	/* get control register contents */
	ulValue = ptSpiUnit->aulSpi_cr[1];

	/* mask out the slave select bits */
	ulValue &= ~HOSTMSK(spi_cr1_fss);

	/* mask in the new slave id */
	ulValue |= uiChipSelect;

	/* write back new value */
	ptSpiUnit->aulSpi_cr[1] = ulValue;

	return 0;
}


static int spi_send_idle(const FLASHER_SPI_CFG_T *ptCfg, size_t sizBytes)
{
	unsigned char ucIdleChar;


	/* Get the idle byte. */
	ucIdleChar = ptCfg->ucIdleChar;

	while( sizBytes>0 )
	{
		spi_exchange_byte(ptCfg, ucIdleChar);
		--sizBytes;
	}

	return 0;
}


static int spi_send_data(const FLASHER_SPI_CFG_T *ptCfg, const unsigned char *pucData, size_t sizData)
{
	const unsigned char *pucDataEnd;


	/* send data */
	pucDataEnd = pucData + sizData;
	while( pucData<pucDataEnd )
	{
		spi_exchange_byte(ptCfg, *(pucData++));
	}

	return 0;
}


static int spi_receive_data(const FLASHER_SPI_CFG_T *ptCfg, unsigned char *pucData, size_t sizData)
{
	unsigned char ucIdleChar;
	unsigned char *pucDataEnd;


	/* get the idle byte */
	ucIdleChar = ptCfg->ucIdleChar;

	/* send data */
	pucDataEnd = pucData + sizData;
	while( pucData<pucDataEnd )
	{
		*pucData = spi_exchange_byte(ptCfg, ucIdleChar);
		++pucData;
	}

	return 0;
}


static int spi_exchange_data(const FLASHER_SPI_CFG_T *ptCfg, const unsigned char *pucOutData, unsigned char *pucInData, size_t sizData)
{
	unsigned char *pucInDataEnd;


	/* send data */
	pucInDataEnd = pucInData + sizData;
	while( pucInData<pucInDataEnd )
	{
		*pucInData = spi_exchange_byte(ptCfg, *(pucOutData++));
		++pucInData;
	}

	return 0;
}


static void spi_set_new_speed(const FLASHER_SPI_CFG_T *ptCfg, unsigned long ulDeviceSpecificSpeed)
{
	HOSTADEF(SPI) *ptSpiUnit;
	unsigned long ulValue;


	/* Get the pointer to the registers. */
	ptSpiUnit = ptCfg->pvUnit;

	ulDeviceSpecificSpeed &= HOSTMSK(spi_cr0_sck_muladd) | HOSTMSK(spi_cr0_filter_in);

	ulValue  = ptSpiUnit->aulSpi_cr[0];
	ulValue &= ~(HOSTMSK(spi_cr0_sck_muladd)|HOSTMSK(spi_cr0_filter_in));
	ulValue |= ulDeviceSpecificSpeed;
	ptSpiUnit->aulSpi_cr[0] = ulValue;
}


static void spi_deactivate(const FLASHER_SPI_CFG_T *ptCfg)
{
	HOSTADEF(SPI) *ptSpiUnit;
	unsigned long ulValue;


	/* Get the pointer to the registers. */
	ptSpiUnit = ptCfg->pvUnit;

	/* Deactivate irqs. */
	ptSpiUnit->ulSpi_imsc = 0;
	/* Clear all pending irqs. */
	ulValue  = HOSTMSK(spi_icr_RORIC);
	ulValue |= HOSTMSK(spi_icr_RTIC);
	ulValue |= HOSTMSK(spi_icr_RXIC);
	ulValue |= HOSTMSK(spi_icr_TXIC);
	ulValue |= HOSTMSK(spi_icr_rxneic);
	ulValue |= HOSTMSK(spi_icr_rxfic);
	ulValue |= HOSTMSK(spi_icr_txeic);
	ptSpiUnit->ulSpi_icr = ulValue;
	/* Deactivate IRQ routing to the CPUs. */
#if ASIC_TYP==ASIC_TYP_NETX500
	ptSpiUnit->ulSpi_irq_cpu_sel = 0;
#elif ASIC_TYP==ASIC_TYP_NETX50
	ptSpiUnit->ulSpi_imsc = 0;
#elif ASIC_TYP==ASIC_TYP_NETX10
	ptSpiUnit->ulSpi_imsc = 0;
	ptSpiUnit->ulSpi_irq_cpu_sel = 0;
#endif

	/* Deactivate DMAs. */
	ptSpiUnit->ulSpi_dmacr = 0;

	/* Deactivate the unit. */
	ptSpiUnit->aulSpi_cr[0] = 0;
	ptSpiUnit->aulSpi_cr[1] = 0;

	/* Deactivate the spi pins. */
#if ASIC_TYP==ASIC_TYP_NETX50 || ASIC_TYP==ASIC_TYP_NETX10 || ASIC_TYP==ASIC_TYP_NETX56 
	mmio_deactivate(ptCfg->aucMmio, sizeof(ptCfg->aucMmio), aatMmioValues[ptCfg->uiChipSelect]);
	
#elif ASIC_TYP==ASIC_TYP_NETX4000
	/* In order to pass the proper MMIO config table, we'd need the unit number, which is not passed in FLASHER_SPI_CFG_T.
	However, mmio_deactivate() only checks if the MMIO config values are 0xff or not, and all the
	MMIO config tables for the netX 4000 are equal in this respect. */
	mmio_deactivate(ptCfg->aucMmio, sizeof(ptCfg->aucMmio), aatMmioValues_SPI0[0]);
#endif

}


int flasher_drv_spi_init(FLASHER_SPI_CFG_T *ptCfg, const FLASHER_SPI_CONFIGURATION_T *ptSpiCfg)
{
	unsigned long ulValue;
	int iResult;
	unsigned int uiIdleCfg;
	unsigned char ucIdleChar;
	HOSTADEF(SPI) *ptSpiUnit;
	unsigned int uiChipSelect;


	iResult = 0;

	/* Get the pointer to the registers. */
	ptSpiUnit = ptCfg->pvUnit;

	/* Get the chip select value. */
	uiChipSelect = ptSpiCfg->uiChipSelect;
	ptCfg->ulSpeed = ptSpiCfg->ulInitialSpeedKhz;            /* initial device speed in kHz */
	ptCfg->ulMaximumSpeedKhz = ptSpiCfg->ulMaximumSpeedKhz;  /* The maximum allowed speed on the interface. */
	ptCfg->uiIdleCfg = ptSpiCfg->uiIdleCfg;                  /* the idle configuration */
	ptCfg->tMode = ptSpiCfg->uiMode;                         /* bus mode */
	ptCfg->uiChipSelect = 1U<<uiChipSelect;                  /* chip select */

	/* set the function pointers */
	ptCfg->pfnSelect = spi_slave_select;
	ptCfg->pfnSendIdle = spi_send_idle;
	ptCfg->pfnSendData = spi_send_data;
	ptCfg->pfnReceiveData = spi_receive_data;
	ptCfg->pfnExchangeData = spi_exchange_data;
	ptCfg->pfnSetNewSpeed = spi_set_new_speed;
	ptCfg->pfnExchangeByte = spi_exchange_byte;
	ptCfg->pfnGetDeviceSpeedRepresentation = spi_get_device_speed_representation;
	ptCfg->pfnDeactivate = spi_deactivate;
	ptCfg->pfnGetKHzSpeedRepresentation = spi_get_khz_speed_representation;

	/* copy the mmio pins */
	memcpy(ptCfg->aucMmio, ptSpiCfg->aucMmio, sizeof(ptSpiCfg->aucMmio));

	/* do not use irqs in bootloader */
	ptSpiUnit->ulSpi_imsc = 0;
	/* clear all pending irqs */
	ulValue  = HOSTMSK(spi_icr_RORIC);
	ulValue |= HOSTMSK(spi_icr_RTIC);
	ulValue |= HOSTMSK(spi_icr_RXIC);
	ulValue |= HOSTMSK(spi_icr_TXIC);
	ulValue |= HOSTMSK(spi_icr_rxneic);
	ulValue |= HOSTMSK(spi_icr_rxfic);
	ulValue |= HOSTMSK(spi_icr_txeic);
	ptSpiUnit->ulSpi_icr = ulValue;
	/* Do not route the irqs to a cpu. */
#if ASIC_TYP==ASIC_TYP_NETX500
	ptSpiUnit->keineahnungwas = 0;
#elif ASIC_TYP==ASIC_TYP_NETX50
	ptSpiUnit->ulSpi_imsc = 0;
#elif ASIC_TYP==ASIC_TYP_NETX10
	ptSpiUnit->ulSpi_imsc = 0;
	ptSpiUnit->ulSpi_irq_cpu_sel = 0;
#elif ASIC_TYP==ASIC_TYP_NETX56
	/* what about the netx 56? */
#elif ASIC_TYP==ASIC_TYP_NETX4000
	ptSpiUnit->ulSpi_imsc = 0;
#endif

	/* do not use dmas */
	ptSpiUnit->ulSpi_dmacr = 0;

	/* set 8 bits */
	ulValue  = 7 << HOSTSRT(spi_cr0_datasize);
	/* set speed and filter */
	ulValue |= spi_get_device_speed_representation(ptCfg, ptCfg->ulSpeed);
	/* set the clock polarity  */
	if( (ptCfg->tMode==FLASHER_SPI_MODE2) || (ptCfg->tMode==FLASHER_SPI_MODE3) )
	{
		ulValue |= HOSTMSK(spi_cr0_SPO);
	}
	/* set the clock phase     */
	if( (ptCfg->tMode==FLASHER_SPI_MODE1) || (ptCfg->tMode==FLASHER_SPI_MODE3) )
	{
		ulValue |= HOSTMSK(spi_cr0_SPH);
	}
	ptSpiUnit->aulSpi_cr[0] = ulValue;


	/* manual chipselect */
	ulValue  = HOSTMSK(spi_cr1_fss_static);
	/* enable the interface */
	ulValue |= HOSTMSK(spi_cr1_SSE);
	/* clear both fifos */
	ulValue |= HOSTMSK(spi_cr1_rx_fifo_clr)|HOSTMSK(spi_cr1_tx_fifo_clr);
	ptSpiUnit->aulSpi_cr[1] = ulValue;

	/* transfer control base is unused in this driver */
	ptCfg->ulTrcBase = 0;

	/* set the idle char from the tx config */
	uiIdleCfg = ptCfg->uiIdleCfg;
	if( (uiIdleCfg&MSK_SQI_CFG_IDLE_IO1_OUT)!=0 )
	{
		ucIdleChar = 0xffU;
	}
	else
	{
		ucIdleChar = 0x00U;
	}
	ptCfg->ucIdleChar = ucIdleChar;

	/* activate the spi pins */
#if ASIC_TYP==ASIC_TYP_NETX50 || ASIC_TYP==ASIC_TYP_NETX10 || ASIC_TYP==ASIC_TYP_NETX56 
	mmio_activate(ptCfg->aucMmio, sizeof(ptCfg->aucMmio), aatMmioValues[uiChipSelect]);
	
#elif ASIC_TYP==ASIC_TYP_NETX4000
	if (ptSpiCfg->uiUnit == 2) 
	{
		mmio_activate(ptCfg->aucMmio, sizeof(ptCfg->aucMmio), aatMmioValues_SPI0[uiChipSelect]);
	}
	else if (ptSpiCfg->uiUnit == 3)
	{
		mmio_activate(ptCfg->aucMmio, sizeof(ptCfg->aucMmio), aatMmioValues_SPI1[uiChipSelect]);
	}
#elif ASIC_TYP==ASIC_TYP_NETIOL
	/* Select mux for SPI master:  2=010 AI-pins with CS */
	HOSTDEF(ptAsicCtrlArea)
	unsigned long ulVal = ptAsicCtrlArea->aulAsic_ctrl_io_config[0];
	ulVal &= ~MSK_NIOL_asic_ctrl_io_config0_sel_spi;
	ulVal |= (0x2UL <<  SRT_NIOL_asic_ctrl_io_config0_sel_spi);
	ulVal |= (0x1cUL << SRT_NIOL_asic_ctrl_io_config0_pw);
	ptAsicCtrlArea->aulAsic_ctrl_io_config[0] = ulVal;
	
#endif
	return iResult;
}

