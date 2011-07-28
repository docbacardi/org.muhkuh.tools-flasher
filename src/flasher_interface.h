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

#ifndef __FLASHER_INTERFACE_H__
#define __FLASHER_INTERFACE_H__


#include <string.h>

#include "spi_flash.h"
#include "spi_atmega_types.h"
#include "cfi_flash.h"

/*-------------------------------------*/

#define FLASHER_INTERFACE_VERSION 0x00020000


typedef enum
{
	BUS_ParFlash                    = 0,    /*  Parallel flash */
	BUS_SPI                         = 1,    /*  Serial flash on spi bus. */
	BUS_SPI_ATMega                  = 2     /*  ATMega on SPI bus */
} BUS_T;

typedef enum
{
	OPERATION_MODE_Flash            = 0,
	OPERATION_MODE_Erase            = 1,
	OPERATION_MODE_Read             = 2,
	OPERATION_MODE_Verify           = 3,
	OPERATION_MODE_Checksum         = 4,    /* build a checksum over the contents of a specified area of a device */
	OPERATION_MODE_Detect           = 5,    /* detect a device */
	OPERATION_MODE_IsErased         = 6,    /* check if the specified area of a device is erased */
	OPERATION_MODE_GetEraseArea     = 7,    /* expand an area to the erase block borders */
	OPERATION_MODE_GetBoardInfo     = 8,    /* get bus and unit information */
	OPERATION_MODE_WriteFuseBits    = 9,
	OPERATION_MODE_WriteLockBits    = 10
} OPERATION_MODE_T;


typedef struct
{
	int fIsValid;                           /* a value of !=0 means the description is valid */
	size_t sizThis;                         /* size of the complete structure in bytes */
	unsigned long ulVersion;                /* interface version of this description (this is the same as the version in the input parameters) */

	BUS_T tSourceTyp;
	union
	{
		FLASH_DEVICE_T tParFlash;
		SPI_FLASH_T tSpiInfo;
		SPI_ATMEGA_T tSpiAtmegaInfo;
	} uInfo;
} DEVICE_DESCRIPTION_T;


/* 
    ulStartAddr/ulEndAddr are relative offsets into the flash,
    pucData is an absolute pointer to the RAM buffer.
*/
   
typedef struct
{
	const DEVICE_DESCRIPTION_T *ptDeviceDescription;
	unsigned long ulStartAdr;
	unsigned long ulDataByteSize;
	unsigned char *pucData;
} CMD_PARAMETER_FLASH_T;



typedef struct
{
	const DEVICE_DESCRIPTION_T *ptDeviceDescription;
	unsigned long ulStartAdr;
	unsigned long ulEndAdr;
} CMD_PARAMETER_ERASE_T;


typedef struct
{
	const DEVICE_DESCRIPTION_T *ptDeviceDescription;
	unsigned long ulStartAdr;
	unsigned long ulEndAdr;
	unsigned char *pucData;
} CMD_PARAMETER_READ_T;


typedef struct
{
	const DEVICE_DESCRIPTION_T *ptDeviceDescription;
	unsigned long ulStartAdr;
	unsigned long ulEndAdr;
	unsigned char *pucData;
} CMD_PARAMETER_VERIFY_T;


typedef struct
{
	const DEVICE_DESCRIPTION_T *ptDeviceDescription;
	unsigned long ulStartAdr;
	unsigned long ulEndAdr;
	unsigned char aucSha1[20];
} CMD_PARAMETER_CHECKSUM_T;


typedef struct
{
	BUS_T tSourceTyp;
	union
	{
		PARFLASH_CONFIGURATION_T tParFlash;
		SPI_CONFIGURATION_T tSpi;
	} uSourceParameter;
	DEVICE_DESCRIPTION_T *ptDeviceDescription;
} CMD_PARAMETER_DETECT_T;


typedef struct
{
	const DEVICE_DESCRIPTION_T *ptDeviceDescription;
	unsigned long ulStartAdr;
	unsigned long ulEndAdr;
} CMD_PARAMETER_ISERASED_T;


typedef struct
{
	const DEVICE_DESCRIPTION_T *ptDeviceDescription;
	unsigned long ulStartAdr;
	unsigned long ulEndAdr;
} CMD_PARAMETER_GETERASEAREA_T;


typedef struct
{
	unsigned long ulBusId;
	unsigned long ulUnitId;
	unsigned char *pucBuffer;
	size_t sizBuffer;
} CMD_PARAMETER_GETBOARDINFO_T;


typedef struct
{
	const DEVICE_DESCRIPTION_T *ptDeviceDescription;
	unsigned char ucFuseBitsLowVal;
	unsigned char ucFuseBitsHighVal;
	unsigned char ucFuseBitsExtVal;
	unsigned char ucUnused;
	unsigned char ucFuseBitsLowFlag;
	unsigned char ucFuseBitsHighFlag;
	unsigned char ucFuseBitsExtFlag;
	unsigned char ucUnused2;
} CMD_PARAMETER_FUSES_T;

typedef struct
{
	const DEVICE_DESCRIPTION_T *ptDeviceDescription;
	unsigned char ucLockBitsVal;
	unsigned char ucUnused1;
	unsigned char ucUnused2;
	unsigned char ucUnused3;
} CMD_PARAMETER_LOCK_BITS_T;



typedef struct
{
	unsigned long ulParamVersion;
	OPERATION_MODE_T tOperationMode;
	union
	{
		CMD_PARAMETER_FLASH_T tFlash;
		CMD_PARAMETER_ERASE_T tErase;
		CMD_PARAMETER_READ_T tRead;
		CMD_PARAMETER_VERIFY_T tVerify;
		CMD_PARAMETER_CHECKSUM_T tChecksum;
		CMD_PARAMETER_DETECT_T tDetect;
		CMD_PARAMETER_ISERASED_T tIsErased;
		CMD_PARAMETER_GETERASEAREA_T tGetEraseArea;
		CMD_PARAMETER_GETBOARDINFO_T tGetBoardInfo;
		CMD_PARAMETER_FUSES_T tFuses;
		CMD_PARAMETER_LOCK_BITS_T tLockBits;
	} uParameter;
} tFlasherInputParameter;


/*-------------------------------------------------------------------------*/

#endif  /*__FLASHER_INTERFACE_H__ */
