/***************************************************************************  
 *   Copyright (C) 2008 by Hilscher GmbH                                   *  
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

/***************************************************************************  
  File          : spi_flash.h                                                   
 ---------------------------------------------------------------------------- 
  Description:                                                                
                                                                              
      SPI Flash Functions
 ---------------------------------------------------------------------------- 
  Todo:                                                                       
                                                                              
 ---------------------------------------------------------------------------- 
  Known Problems:                                                             
                                                                              
    -                                                                         
                                                                              
 ---------------------------------------------------------------------------- 
 ***************************************************************************/ 

#ifndef __SPI_FLASH_H__
#define __SPI_FLASH_H__

#include "spi.h"
#include "spi_flash_types.h"

/* ------------------------------------- */

/**
 * This structure holds the information needed to access the specific flash device.
 * It is filled in by spi_detect(), if a flash device was found.
 */
typedef struct FLASHER_SPI_FLASH_STRUCT
{
	SPIFLASH_ATTRIBUTES_T tAttributes;      /**< @brief attributes of the flash.      */
	FLASHER_SPI_CFG_T tSpiDev;              /**< @brief SPI device and it's settings. */
	unsigned long ulSectorSize;             /**< @brief size of one sector in bytes.  */
	unsigned int uiSlaveId;                 /**< @brief SPI Slave Id of the flash.    */
	unsigned int uiPageAdrShift;            /**< @brief bit shift for the page part of the address, 0 means no page / byte split.  */
	unsigned int uiSectorAdrShift;          /**< @brief bit shift for one sector, 0 means no page / byte split.                    */
} FLASHER_SPI_FLASH_T;

/*-----------------------------------*/

int Drv_SpiInitializeFlash        (const FLASHER_SPI_CONFIGURATION_T *ptSpiCfg, FLASHER_SPI_FLASH_T *ptFlash, char *pcBufferEnd);
int Drv_SpiEraseFlashPage         (const FLASHER_SPI_FLASH_T *ptFlash, unsigned long ulLinearAddress);
// TODO check what's up with the following block. Is the algo *actually* better than the existing ones?
//----------
//int Drv_SpiEraseFlashPage256(const unsigned char opcodeErrPage256 /*new param */, const SPI_FLASH_T *ptFlash, unsigned long ulLinearAddress);
//int Drv_SpiEraseFlashPage512(const SPI_FLASH_T *ptFlash, unsigned long ulLinearAddress);
//int Drv_SpiEraseFlashBlock4k(const SPI_FLASH_T *ptFlash, unsigned long ulLinearAddress);
//int Drv_SpiEraseFlashBlock32k(const SPI_FLASH_T *ptFlash, unsigned long ulLinearAddress);

int Drv_SpiEraseFlashArea(const FLASHER_SPI_FLASH_T *ptFlash, unsigned long ulLinearAddress, const unsigned char eraseOpcode);
//----------
int Drv_SpiEraseFlashSector       (const FLASHER_SPI_FLASH_T *ptFlash, unsigned long ulLinearAddress);
int Drv_SpiEraseFlashMultiSectors (const FLASHER_SPI_FLASH_T *ptFlash, unsigned long ulLinearStartAddress, unsigned long ulLinearEndAddress);
int Drv_SpiEraseFlashComplete     (const FLASHER_SPI_FLASH_T *ptFlash);
int Drv_SpiWriteFlashPages        (const FLASHER_SPI_FLASH_T *ptFlash, unsigned long ulOffs, const unsigned char *pabSrc, unsigned long ulNum);
int Drv_SpiReadFlash              (const FLASHER_SPI_FLASH_T *ptFlash, unsigned long ulLinearAddress, unsigned char       *pucData, size_t sizData);
int Drv_SpiEraseAndWritePage      (const FLASHER_SPI_FLASH_T *ptFlash, unsigned long ulLinearAddress, const unsigned char *pucData, size_t sizData);
int Drv_SpiWritePage              (const FLASHER_SPI_FLASH_T *ptFlash, unsigned long ulLinearAddress, const unsigned char *pucData, size_t sizData);

const char *spi_flash_get_adr_mode_name(SPIFLASH_ADR_T tAdrMode);

int board_get_spi_driver(const FLASHER_SPI_CONFIGURATION_T *ptSpiCfg, FLASHER_SPI_CFG_T *ptSpiDev);

#endif

