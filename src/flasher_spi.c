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

 /**
  * @file
  * This file contains the functions to read and write SPI flashes.
  *
  * A general note on the end address/offset:
  * The end address/offset is the address/offset of the last byte + 1,
  * or the start address/offset plus the number of bytes to read/write/erase.
  * For example, to read 4KB from offset 0x1000 to 0x1fff,
  * you have to pass ulStartAddr=0x1000 and ulEndAddr=0x2000.
  */
 
// TODO can we really just remove this??
//#include <string.h>

#include "flasher_spi.h"
#include "spi_flash.h"

#include "progress_bar.h"
#include "uprintf.h"
/** @file spi.h */
/** @file spi_flash.h */

// TODO are all these correct? FastErase includes
#include "flasher_header.h"
#include "systime.h"
#include "sfdp.h"

/*-----------------------------------*/

#define SPI_BUFFER_SIZE 8192  // TODO This is changed to 4096 in the FastErase branch, why??
unsigned char pucSpiBuffer[SPI_BUFFER_SIZE];

/*-----------------------------------*/


/*-----------MY DEFINES--------------*/ // TODO CLEAN THESE UP!

#define SIMULATION 0 // No real erase just simulating the algorithm
#define DEBUG 1 // Debugging output
#define PARANOIAMODE 0 // Checks _everything_ NOT FULLY IMPLEMENTED YET

#define ZERO 0xFF
#define FLASH_SIZE_KB 4096
#define FLASH_SIZE_BYTE FLASH_SIZE_KB * 1024

#define ERASE_BLOCK_MIN_KB 4
#define ERASE_BLOCK_MIN_BYTE 4096

#define ERASE_SECTOR_SIZE_KB 64
#define ERASE_SECTOR_SIZE_BYTE ERASE_SECTOR_SIZE_KB * 1024

#define BLOCKSIZE_BYTE ERASE_BLOCK_MIN_BYTE // the as Erase_min_byte because the smallest eraseoperation defines the pagesize (for my purposes)

#define MAP_LENGTH FLASH_SIZE_KB / ERASE_BLOCK_MIN_KB

// XXX commented out the attribute, does it compile now?
unsigned long long int totalMemory;// __attribute__ ((section (".data")));
unsigned char * memStarPtr;// __attribute__ ((section (".data")));
unsigned char * memCurrentPtr;// __attribute__ ((section (".data")));
unsigned char * memEndPtr;// __attribute__ ((section (".data")));
unsigned long long int freeMem;// __attribute__ ((section (".data")));

// NOTE: Enum maps on eraseblock size in byte
enum eraseOperations
{
	PAGE_ERASE_256 = 256, PAGE_ERASE_512 = 512, BLOCK_ERASE_4K = 4096, BLOCK_ERASE_32K = 32768, SECTOR_ERASE_64K = 65536, CHIP_ERASE = 1
};

/*-----------------------------------*/

static NETX_CONSOLEAPP_RESULT_T spi_write_with_progress(const FLASHER_SPI_FLASH_T *ptFlashDev, unsigned long ulFlashStartAdr, unsigned long ulDataByteLen, const unsigned char *pucDataStartAdr)
{
	const unsigned char *pucDC;
	unsigned long ulC, ulE;	
	unsigned long ulSegSize;
	unsigned long ulMaxSegSize;
	unsigned long ulPageSize;
	unsigned long ulPageStartAdr;
	unsigned long ulProgressCnt;
	unsigned long ulOffset;
	int iResult;
	NETX_CONSOLEAPP_RESULT_T tResult;


	/* Expect success. */
	tResult = NETX_CONSOLEAPP_RESULT_OK;

	/* use the pagesize as segmentation */
	ulPageSize = ptFlashDev->tAttributes.ulPageSize;
	if( ulPageSize>SPI_BUFFER_SIZE )
	{
		uprintf("! pagesize exceeds reserved buffer.\n");
		tResult = NETX_CONSOLEAPP_RESULT_ERROR;
	}
	else
	{
		/* write the complete data */
		uprintf("# Writing...\n");

		/* loop over all data */
		ulC = ulFlashStartAdr;
		ulE = ulC + ulDataByteLen;
		pucDC = pucDataStartAdr;

		ulProgressCnt = 0;
		progress_bar_init( ulDataByteLen );

		/* start inside a page? */
		ulOffset = ulFlashStartAdr % ulPageSize;
		if( ulOffset!=0 )
		{
			/* yes, start inside a page */

			/* get the startaddress of the page */
			ulPageStartAdr = ulFlashStartAdr - ulOffset;

			/* get the new max segment size for the rest of the page */
			ulMaxSegSize = ulPageSize - ulOffset;

			/* get the next segment, limit it to 'ulMaxSegSize' */
			ulSegSize = ulE - ulC;
			if( ulSegSize>ulMaxSegSize )
			{
				ulSegSize = ulMaxSegSize;
			}

			/* read the whole page */
			iResult = Drv_SpiReadFlash(ptFlashDev, ulPageStartAdr, pucSpiBuffer, ulPageSize);
			if( iResult!=0 )
			{
				tResult = NETX_CONSOLEAPP_RESULT_ERROR;
			}
			else
			{
				/* modify the rest of the page */
				memcpy(pucSpiBuffer+ulOffset, pucDC, ulSegSize);

				/* write the modified buffer */
				iResult = Drv_SpiWritePage(ptFlashDev, ulPageStartAdr, pucSpiBuffer, ulPageSize);
/*				iResult = Drv_SpiEraseAndWritePage(ptFlashDev, ulPageStartAdr, ulPageSize, pucSpiBuffer); */
				if( iResult!=0 )
				{
					uprintf("! write error\n");
					tResult = NETX_CONSOLEAPP_RESULT_ERROR;
				}
				else
				{
					/* next segment */
					ulC += ulSegSize;
					pucDC += ulSegSize;

					/* inc progress */
					ulProgressCnt += ulSegSize;
					progress_bar_set_position(ulProgressCnt);
				}
			}
		}

		if( tResult==NETX_CONSOLEAPP_RESULT_OK )
		{
			/* process complete pages */
			while( ulC+ulPageSize<ulE )
			{
				/* write one page */
				iResult = Drv_SpiWritePage(ptFlashDev, ulC, pucDC, ulPageSize);
/*				iResult = Drv_SpiEraseAndWritePage(ptFlashDev, ulC, ulPageSize, pucDC); */
				if( iResult!=0 )
				{
					uprintf("! write error\n");
					tResult = NETX_CONSOLEAPP_RESULT_ERROR;
					break;
				}

				/* next segment */
				ulC += ulPageSize;
				pucDC += ulPageSize;

				/* inc progress */
				ulProgressCnt += ulPageSize;
				progress_bar_set_position(ulProgressCnt);
			}

			if( tResult==NETX_CONSOLEAPP_RESULT_OK )
			{
				/* part of a page left? */
				if( ulC<ulE )
				{
					/* yes, start inside a page -> get the next segment */
					ulSegSize = ulE - ulC;

					/* modify the beginning of the page */
					memcpy(pucSpiBuffer, pucDC, ulSegSize);
					/* read the rest of the buffer */
					iResult = Drv_SpiReadFlash(ptFlashDev, ulC+ulSegSize, pucSpiBuffer+ulSegSize, ulPageSize-ulSegSize);
					if( iResult!=0 )
					{
						tResult = NETX_CONSOLEAPP_RESULT_ERROR;
					}
					else
					{
						/* write the buffer */
						iResult = Drv_SpiWritePage(ptFlashDev, ulC, pucSpiBuffer, ulPageSize);
/*						iResult = Drv_SpiEraseAndWritePage(ptFlashDev, ulC, ulPageSize, pucSpiBuffer); */
						if( iResult!=0 )
						{
							uprintf("! write error\n");
							tResult = NETX_CONSOLEAPP_RESULT_ERROR;
						}
					}
				}
			}
		}
	}

	progress_bar_finalize();

	if( tResult==NETX_CONSOLEAPP_RESULT_OK )
	{
		uprintf(". write ok\n");
	}

	return tResult;
}


static NETX_CONSOLEAPP_RESULT_T spi_verify_with_progress(const FLASHER_SPI_FLASH_T *ptFlashDev, unsigned long ulFlashStartAdr, unsigned long ulDataByteLen, const unsigned char *pucDataStartAdr)
{
	int iResult;
	unsigned long       ulC, ulE;
	unsigned long       ulSegSize, ulMaxSegSize;
	unsigned long       ulProgressCnt;
	unsigned char       *pucCmp0;
	const unsigned char *pucCmp1;
	const unsigned char *pucDC;
	size_t sizCmpCnt;


	uprintf("# Verifying...\n");

	ulMaxSegSize = SPI_BUFFER_SIZE;

	/* loop over all data */
	ulC = ulFlashStartAdr;
	ulE = ulC + ulDataByteLen;
	pucDC = pucDataStartAdr;

	ulProgressCnt = 0;
	progress_bar_init(ulDataByteLen);

	while( ulC<ulE )
	{
		/* get the next segment, limit it to 'ulMaxSegSize' */
		ulSegSize = ulE - ulC;
		if( ulSegSize>ulMaxSegSize )
		{
			ulSegSize = ulMaxSegSize;
		}

		/* read the segment */
		iResult = Drv_SpiReadFlash(ptFlashDev, ulC, pucSpiBuffer, ulSegSize);
		if( iResult!=0 )
		{
			return NETX_CONSOLEAPP_RESULT_ERROR;
		}

		/* compare... */
		pucCmp0 = pucSpiBuffer;
		pucCmp1 = pucDC;
		sizCmpCnt = 0;
		while( sizCmpCnt<ulSegSize )
		{
			if( pucCmp0[sizCmpCnt]!=pucCmp1[sizCmpCnt] )
			{
				uprintf(". verify error at offset 0x%08x. buffer: 0x%02x, flash: 0x%02x.\n", ulC + sizCmpCnt, pucCmp1[sizCmpCnt], pucCmp0[sizCmpCnt]);
				return NETX_CONSOLEAPP_RESULT_ERROR;
			}
			++sizCmpCnt;
		}

		/* next segment */
		ulC += ulSegSize;
		pucDC += ulSegSize;

		/* inc progress */
		ulProgressCnt += ulSegSize;
		progress_bar_set_position(ulProgressCnt);
	}

	progress_bar_finalize();
	uprintf(". verify ok\n");

	/* compare ok! */
	return NETX_CONSOLEAPP_RESULT_OK;
}


static NETX_CONSOLEAPP_RESULT_T spi_read_with_progress(const FLASHER_SPI_FLASH_T *ptFlashDev, unsigned long ulFlashStartAdr, unsigned long ulFlashEndAdr, unsigned char *pucDataAdr)
{
	unsigned long ulSegSize, ulMaxSegSize;
	unsigned long ulProgressCnt;
	int iResult;


	uprintf("# Reading...\n");

	ulMaxSegSize = SPI_BUFFER_SIZE;

	ulProgressCnt = 0;
	progress_bar_init(ulFlashEndAdr-ulFlashStartAdr);

	while( ulFlashStartAdr<ulFlashEndAdr )
	{
		/* get the next segment, limit it to 'ulMaxSegSize' */
		ulSegSize = ulFlashEndAdr - ulFlashStartAdr;
		if( ulSegSize>ulMaxSegSize )
		{
			ulSegSize = ulMaxSegSize;
		}

		/* read the segment */
		iResult = Drv_SpiReadFlash(ptFlashDev, ulFlashStartAdr, pucDataAdr, ulSegSize);
		if( iResult!=0 )
		{
			return NETX_CONSOLEAPP_RESULT_ERROR;
		}

		/* next segment */
		ulFlashStartAdr += ulSegSize;
		pucDataAdr += ulSegSize;

		/* inc progress */
		ulProgressCnt += ulSegSize;
		progress_bar_set_position(ulProgressCnt);
	}

	progress_bar_finalize();
	uprintf(". read ok\n");

	/* read ok! */
	return NETX_CONSOLEAPP_RESULT_OK;
}

#if CFG_INCLUDE_SHA1!=0
static NETX_CONSOLEAPP_RESULT_T spi_sha1_with_progress(const FLASHER_SPI_FLASH_T *ptFlashDev, unsigned long ulFlashStartAdr, unsigned long ulFlashEndAdr, SHA_CTX *ptSha1Context)
{
	unsigned long ulSegSize, ulMaxSegSize;
	unsigned long ulProgressCnt;
	int iResult;

	uprintf("# Calculating hash...\n");

	ulMaxSegSize = SPI_BUFFER_SIZE;

	ulProgressCnt = 0;
	progress_bar_init(ulFlashEndAdr-ulFlashStartAdr);

	while( ulFlashStartAdr<ulFlashEndAdr )
	{
		/* get the next segment, limit it to 'ulMaxSegSize' */
		ulSegSize = ulFlashEndAdr - ulFlashStartAdr;
		if( ulSegSize>ulMaxSegSize )
		{
			ulSegSize = ulMaxSegSize;
		}

		/* read the segment */
		iResult = Drv_SpiReadFlash(ptFlashDev, ulFlashStartAdr, pucSpiBuffer, ulSegSize);
		if( iResult!=0 )
		{
			return NETX_CONSOLEAPP_RESULT_ERROR;
		}
		
		SHA1_Update(ptSha1Context, (const void*)pucSpiBuffer, ulSegSize);

		/* next segment */
		ulFlashStartAdr += ulSegSize;

		/* inc progress */
		ulProgressCnt += ulSegSize;
		progress_bar_set_position(ulProgressCnt);
	}

	progress_bar_finalize();
	uprintf(". hash done\n");

	/* read ok! */
	return NETX_CONSOLEAPP_RESULT_OK;
}
#endif

static NETX_CONSOLEAPP_RESULT_T spi_erase_with_progress(const FLASHER_SPI_FLASH_T *ptFlashDev, unsigned long ulStartAdr, unsigned long ulEndAdr)
{
	NETX_CONSOLEAPP_RESULT_T tResult;
	unsigned long ulPageSize;
	unsigned long ulSectorSize;
	unsigned long ulPageOffsetStart;
	unsigned long ulPageOffsetEnd;
	unsigned long ulSectorOffsetStart;
	unsigned long ulSectorOffsetEnd;
	unsigned long ulAddress;
	unsigned long ulProgressCnt;
	int iResult;
	int iCanUsePageErase;
	int iCanUseSectorErase;
	unsigned long ulEraseChunk;


	uprintf("# Erase flash...\n");

	/* Assume success. */
	tResult = NETX_CONSOLEAPP_RESULT_OK;

	/* Be pessimistic. */
	iCanUsePageErase = 0;
	iCanUseSectorErase = 0;

	/* Get the page and sector size. */
	ulPageSize = ptFlashDev->tAttributes.ulPageSize;
	ulSectorSize = ptFlashDev->ulSectorSize;

	/* Only check for page erase if the page erase opcode is not 0. */
	if( ptFlashDev->tAttributes.ucErasePageOpcode!=0x00 )
	{
		/* Get the offsets of the start and end address to the page border. */
		ulPageOffsetStart = ulStartAdr % ulPageSize;
		ulPageOffsetEnd = ulEndAdr % ulPageSize;
		/* The offsets are 0 if they are exactly on the page boundaries. */
		if( ulPageOffsetStart==0 && ulPageOffsetEnd==0 )
		{
			iCanUsePageErase = 1;
		}
	}

	/* The sector erase command is always present. */

	/* Get the offsets of the start and end address to the sector border. */
	ulSectorOffsetStart = ulStartAdr % ulSectorSize;
	ulSectorOffsetEnd = ulEndAdr % ulSectorSize;
	/* The offsets are 0 if they are exactly on the sector boundaries. */
	if( ulSectorOffsetStart==0 && ulSectorOffsetEnd==0 )
	{
		iCanUseSectorErase = 1;
	}

	/* Prefer sector over page erase. It is faster. */
	if( iCanUseSectorErase!=0 )
	{
		iCanUsePageErase = 0;
		ulEraseChunk = ulSectorSize;
	}
	else if( iCanUsePageErase!=0 )
	{
		iCanUseSectorErase = 0;
		ulEraseChunk = ulPageSize;
	}
	/* Fallback to the old behavior if page and sector does not match.
	 * Adjust the start and end to the next sector boundaries.
	 */
	else
	{
		if( ulSectorOffsetStart!=0 )
		{
			uprintf("Warning: the start address is not aligned to a sector border!\n");
			uprintf("Warning: changing the start address from 0x%08x", ulStartAdr);
			ulStartAdr -= ulSectorOffsetStart;
			uprintf(" to 0x%08x.\n", ulStartAdr);
		}
		if( ulSectorOffsetEnd!=0 )
		{
			uprintf("Warning: the end address is not aligned to a sector border!\n");
			uprintf("Warning: changing the end address from 0x%08x", ulEndAdr);
			ulEndAdr += ulSectorSize - ulSectorOffsetEnd;
			uprintf(" to 0x%08x.\n", ulEndAdr);
		}

		iCanUsePageErase = 0;
		iCanUseSectorErase = 1;
		ulEraseChunk = ulSectorSize;
	}

	/* Show the start and the end address of the erase area. */
	uprintf(". erase 0x%08x - 0x%08x\n", ulStartAdr, ulEndAdr);

	ulProgressCnt = 0;
	progress_bar_init( ulEndAdr-ulStartAdr );

	/* Erase the complete area. */
	ulAddress = ulStartAdr;
	while( ulAddress<ulEndAdr )
	{
		if( iCanUsePageErase!=0 )
		{
			iResult = Drv_SpiEraseFlashPage(ptFlashDev, ulAddress);
		}
		else
		{
			iResult = Drv_SpiEraseFlashSector(ptFlashDev, ulAddress);
		}
		if( iResult!=0 )
		{
			uprintf("! erase failed at address 0x%08x\n", ulAddress);
			tResult = NETX_CONSOLEAPP_RESULT_ERROR;
			break;
		}

		/* Move to the next segment. */
		ulAddress += ulEraseChunk;

		/* Increment the progress bar. */
		ulProgressCnt += ulEraseChunk;
		progress_bar_set_position(ulProgressCnt);
	}

	progress_bar_finalize();
	uprintf(". erase OK\n");

	/* Return the result. */
	return tResult;
}

/*-----------------------------------*/
/**
 * @brief Write data from RAM into the flash device.
 *
 * Writes ulDataByteSize bytes from RAM at address pucDataStartAdr to 
 * the flash starting at offset ulFlashStartAdr.
 *
 * @param ptFlashDescription  [in]  Device information returned by spi_detect.
 * @param ulFlashStartAdr     [in]  Start offset in the flash memory.
 * @param ulDataByteSize      [in]  Size of the data to be written in bytes.
 * @param pucDataStartAdr     [in]  Address of the data to be written in RAM.
 *
 * @return
 * - NETX_CONSOLEAPP_RESULT_OK: the data has been written to the flash and verified. 
 * - NETX_CONSOLEAPP_RESULT_ERROR: An error has occurred.
 */

NETX_CONSOLEAPP_RESULT_T spi_flash(const FLASHER_SPI_FLASH_T *ptFlashDescription, unsigned long ulFlashStartAdr, unsigned long ulDataByteSize, const unsigned char *pucDataStartAdr)
{
	NETX_CONSOLEAPP_RESULT_T tResult;


	tResult = NETX_CONSOLEAPP_RESULT_OK;
	
	/* write data */
	tResult = spi_write_with_progress(ptFlashDescription, ulFlashStartAdr, ulDataByteSize, pucDataStartAdr);
	if( tResult!=NETX_CONSOLEAPP_RESULT_OK )
	{
		uprintf("! write error\n");
	}
	else
	{
		/* verify data */
		tResult = spi_verify_with_progress(ptFlashDescription, ulFlashStartAdr, ulDataByteSize, pucDataStartAdr);
	}

	return tResult;
}

/*-----------------------------------*/


/**
 * NOTE: Changed the parameter of smartErase to tRead because have to read mem again
 * TODO: own typedef CMD_PARAMETER_SMART_ERASE_T *DONE*
 * tRead contains the same data as tErase but also contains: unsigned char *pucData;
 *
 */
NETX_CONSOLEAPP_RESULT_T spi_smart_erase(CMD_PARAMETER_SMART_ERASE_T *ptParameter)
{
	if (myData.isValid == 0)
	{
#if DEBUG
		uprintf("\n\n !!!!!Ok we didn't get any SFDP information so we have to perform simple erase.!!!\n\n");
#endif
		/**
		 * here we can cast from frem smart_erase to erase because in fact it's the same
		 * but watch if pucData is needed.
		 */
		// TODO I had to manually fix this cause parameter was incorrect
		//spi_erase((CMD_PARAMETER_ERASE_T*) ptParameter); // Copied version
		spi_erase(&(ptParameter->ptDeviceDescription->uInfo.tSpiInfo), ptParameter->ulStartAdr, ptParameter->ulEndAdr);
		return 0;
	}

	initMemory();
	NETX_CONSOLEAPP_RESULT_T tResult;
	const FLASHER_SPI_FLASH_T *ptFlashDescription;
	unsigned long ulStartAdr;
	unsigned long ulEndAdr;
	unsigned long ulCnt;
	unsigned char *pucCnt;
	unsigned char *pucEnd;
	unsigned long ulSegSize, ulMaxSegSize;
	unsigned long ulProgressCnt;
	int iResult;
	unsigned long ulErased;

	systime_init();
	unsigned long tstart = systime_get_ms();
	uprintf("SYSTIME: %d", tstart);

	unsigned char * cHexMapByte = 0;
	newArray(&cHexMapByte, FLASH_SIZE_BYTE / myData.eraseOperation1);

	/* expect success */
	tResult = NETX_CONSOLEAPP_RESULT_OK;
	ptFlashDescription = &(ptParameter->ptDeviceDescription->uInfo.tSpiInfo);
	ulStartAdr = ptParameter->ulStartAdr;
	ulEndAdr = ptParameter->ulEndAdr;

	ulErased = 0xffU;

	uprintf("# Checking data...\n");

	ulMaxSegSize = ERASE_BLOCK_MIN_BYTE;

	/* loop over all data */
	ulCnt = ulStartAdr;
	ulProgressCnt = 0;
	progress_bar_init(ulEndAdr - ulStartAdr);

	unsigned int counter = 0;
	while (ulCnt < ulEndAdr)
	{
		/* get the next segment, limit it to 'ulMaxSegSize' */
		ulSegSize = ulEndAdr - ulCnt;
		if (ulSegSize > ulMaxSegSize)
		{
			ulSegSize = ulMaxSegSize;
		}

		/* read the segment */
		iResult = Drv_SpiReadFlash(ptFlashDescription, ulCnt, pucSpiBuffer, ulSegSize);
		if (iResult != 0)
		{
			tResult = NETX_CONSOLEAPP_RESULT_ERROR;
			break;
		}

#if DEBUG
		uprintf("\n\n Reading Segment: %d\n", ulCnt);
#endif
		ulErased = 0xffU;

		pucCnt = pucSpiBuffer;
		pucEnd = pucSpiBuffer + ulSegSize;
		while (pucCnt < pucEnd)
		{
			ulErased &= *(pucCnt++);
		}

		if (ulErased != 0xff)
		{
			setValue(cHexMapByte, counter, 1);
#if DEBUG
			uprintf("Seg: %d is Dirty\n", counter);
#endif
		}

		/* next segment */
		ulCnt += ulSegSize;
		pucCnt += ulSegSize;

		/* inc progress */
		ulProgressCnt += ulSegSize;
		progress_bar_set_position(ulProgressCnt);
		counter++;
	}

	progress_bar_finalize();
#if DEBUG
	dumpBoolArray2(cHexMapByte, FLASH_SIZE_BYTE / myData.eraseOperation1, "First Map");
#endif
	analyzeMap(cHexMapByte, ptParameter);

	unsigned long end = systime_get_ms();
	uprintf("\n\nThe alg took %d mSecs. \n\n\n", end - tstart);
	return tResult;

}

/*-----------------------------------*/


/**
 * @brief Erase a range of the memory. 
 *
 * Erases the memory from offset ulStartAdr to ulEndAdr-1.
 * ulStartAdr to ulEndAdr-1 must be a complete erase block, or range of erase blocks.
 * Use spi_getEraseArea to compute this range from an arbitrary start and end offset.
 * 
 * @param ptFlashDescription [in]  Device information returned by spi_detect.
 * @param ulStartAdr         [in]  Start offset of the first erase block to be erased.
 * @param ulEndAdr           [in]  End offset of the last erase block to be erased (offset of the last byte + 1).
 *
 * @return
 * - NETX_CONSOLEAPP_RESULT_OK: success, the memory has been erased.
 * - NETX_CONSOLEAPP_RESULT_ERROR: An error has occurred.
 */

NETX_CONSOLEAPP_RESULT_T spi_erase(const FLASHER_SPI_FLASH_T *ptFlashDescription, unsigned long ulStartAdr, unsigned long ulEndAdr)
{
	NETX_CONSOLEAPP_RESULT_T tResult;


	/* erase the block */
	tResult = spi_erase_with_progress(ptFlashDescription, ulStartAdr, ulEndAdr);
	if( tResult!=NETX_CONSOLEAPP_RESULT_OK )
	{
		uprintf("! erase error\n");
	}

	return tResult;
}

/*-----------------------------------*/

/**
 * @brief Read data from the flash.
 *
 * Reads the memory from offset ulStartAdr to ulEndAdr-1 to RAM at pucData.
 * 
 * @param ptFlashDescription  [in]  Device information returned by spi_detect.
 * @param ulStartAdr          [in]  Start offset in the flash memory.
 * @param ulEndAdr            [in]  End offset (offset of the last byte + 1).
 * @param pucData             [out] Destination address for the data in RAM.
 *
 * @return
 * - NETX_CONSOLEAPP_RESULT_OK: The data has successfully been read.
 * - NETX_CONSOLEAPP_RESULT_ERROR: An error occurred.
 */

NETX_CONSOLEAPP_RESULT_T spi_read(const FLASHER_SPI_FLASH_T *ptFlashDescription, unsigned long ulStartAdr, unsigned long ulEndAdr, unsigned char *pucData)
{
	NETX_CONSOLEAPP_RESULT_T tResult;


	/* Expect success. */
	tResult = NETX_CONSOLEAPP_RESULT_OK;

	/* read data */
	tResult = spi_read_with_progress(ptFlashDescription, ulStartAdr, ulEndAdr, pucData);
	if( tResult!=NETX_CONSOLEAPP_RESULT_OK )
	{
		uprintf("! read error\n");
	}

	return tResult;
}


#if CFG_INCLUDE_SHA1!=0
NETX_CONSOLEAPP_RESULT_T spi_sha1(const FLASHER_SPI_FLASH_T *ptFlashDescription, unsigned long ulStartAdr, unsigned long ulEndAdr, SHA_CTX *ptSha1Context)
{
	NETX_CONSOLEAPP_RESULT_T tResult;


	/* read data */
	tResult = spi_sha1_with_progress(ptFlashDescription, ulStartAdr, ulEndAdr, ptSha1Context);
	if( tResult!=NETX_CONSOLEAPP_RESULT_OK )
	{
		uprintf("! error calculating hash\n");
	}

	return tResult;
}
#endif

/*-----------------------------------*/

/**
 * @brief Compare data in flash to RAM.
 *
 * Compares the data in the flash at offset ulFlashStartAdr to ulFlashEndAdr-1 to the data in RAM
 * at address pucData.
 *
 * @param ptFlashDescription  [in]  Device information returned by spi_detect.
 * @param ulFlashStartAdr     [in]  Start offset in the flash memory.
 * @param ulFlashEndAdr       [in]  End offset (offset of the last byte + 1).
 * @param pucData             [in]  Address of the data to be verified in RAM.
 * @param ppvReturnMessage    [Out] Result of the compare operation.
 *
 * @return
 * - NETX_CONSOLEAPP_RESULT_OK, *ppvReturnMessage == NETX_CONSOLEAPP_RESULT_OK: Verify succeeded, the data was compared and is equal.
 * - NETX_CONSOLEAPP_RESULT_ERROR, *ppvReturnMessage == NETX_CONSOLEAPP_RESULT_ERROR Verify failed, or the data was compared and is not equal.
 */

NETX_CONSOLEAPP_RESULT_T spi_verify(const FLASHER_SPI_FLASH_T *ptFlashDescription, unsigned long ulFlashStartAdr, unsigned long ulFlashEndAdr, const unsigned char *pucData, void **ppvReturnMessage)
{
	NETX_CONSOLEAPP_RESULT_T tResult;
	unsigned long ulDataByteSize;


	ulDataByteSize  = ulFlashEndAdr - ulFlashStartAdr;

	/* verify data */
	tResult = spi_verify_with_progress(ptFlashDescription, ulFlashStartAdr, ulDataByteSize, pucData);
	
	*ppvReturnMessage = (void*)tResult;

	return tResult;
}


/*-----------------------------------*/

/**
 * @brief Initialize SPI interface and detect serial flash.
 *
 * The flasher initializes the SPI interface specified in ptSpiConfiguration and tries to
 * detect a serial flash.
 * It tests for a number of known flash devices with specific identification sequences.
 * If none of these known flashes is found, it tries to read the SFDP information.
 * When successful, it returns a device description in ptFlashDescription, which is passed
 * to subsequently called functions that access the flash.
 *
 * @param ptSpiConfiguration [in]  Configuration of the SPI interface, e.g. the clock frequency.
 * @param ptFlashDescription [out] Information about the flash device, if any was identified.
 * @param pcBufferEnd        [in]  Pointer to the end of a buffer at least 8 KB in size.
 *
 * @return
 * - NETX_CONSOLEAPP_RESULT_OK: a device was detected and the device information is stored in ptFlashDescription.
 * - NETX_CONSOLEAPP_RESULT_ERROR: no device was detected or an error occurred.
 */

NETX_CONSOLEAPP_RESULT_T spi_detect(FLASHER_SPI_CONFIGURATION_T *ptSpiConfiguration, FLASHER_SPI_FLASH_T *ptFlashDescription, char *pcBufferEnd)
{
	NETX_CONSOLEAPP_RESULT_T tResult;
	int iResult;


	/* try to detect flash */
	uprintf(". Detecting SPI flash on unit %d, chip select %d...\n", ptSpiConfiguration->uiUnit, ptSpiConfiguration->uiChipSelect);
	ptFlashDescription->uiSlaveId = ptSpiConfiguration->uiChipSelect;
	iResult = Drv_SpiInitializeFlash(ptSpiConfiguration, ptFlashDescription, pcBufferEnd);
	if( iResult!=0 )
	{
		/* failed to detect the SPI flash */
		uprintf("! failed to detect flash!\n");

		tResult = NETX_CONSOLEAPP_RESULT_ERROR;
	}
	else
	{
		uprintf(". OK, found %s\n", ptFlashDescription->tAttributes.acName);

		tResult = NETX_CONSOLEAPP_RESULT_OK;
	}

	return tResult;
}


/*-----------------------------------*/

/**
 * @brief Check if an area of the flash memory is erased. 
 *
 * Checks if the area from offset ulStartAdr to ulEndAdr-1 in the flash is erased, 
 * that is, all bytes read as 0xff.
 * 
 * @param ptFlashDescription [in]  Device information returned by spi_detect.
 * @param ulStartAdr         [in]  Start offset in the flash memory.
 * @param ulEndAdr           [in]  End offset (offset of the last byte to be checked + 1).
 * @param ppvReturnMessage   [out] Return value.
 *
 * @return
 * - NETX_CONSOLEAPP_RESULT_OK, *ppvReturnMessage == 0xff: The memory has been checked and is erased.
 * - NETX_CONSOLEAPP_RESULT_OK, *ppvReturnMessage != 0xff The memory has been checked but is not erased.
 * - NETX_CONSOLEAPP_RESULT_ERROR: An error has occurred.
 */
NETX_CONSOLEAPP_RESULT_T spi_isErased(const FLASHER_SPI_FLASH_T *ptFlashDescription, unsigned long ulStartAdr, unsigned long ulEndAdr, void **ppvReturnMessage)
{
	NETX_CONSOLEAPP_RESULT_T  tResult;
	unsigned long ulCnt;
	unsigned char *pucCnt;
	unsigned char *pucEnd;
	unsigned long ulSegSize, ulMaxSegSize;
	unsigned long ulProgressCnt;
	int iResult;
	unsigned long ulErased;


	/* expect success */
	tResult = NETX_CONSOLEAPP_RESULT_OK;

	ulErased = 0xffU;

	uprintf("# Checking data...\n");

	ulMaxSegSize = SPI_BUFFER_SIZE;

	/* loop over all data */
	ulCnt = ulStartAdr;
	ulProgressCnt = 0;
	progress_bar_init(ulEndAdr-ulStartAdr);

	while( ulCnt<ulEndAdr )
	{
		/* get the next segment, limit it to 'ulMaxSegSize' */
		ulSegSize = ulEndAdr - ulCnt;
		if( ulSegSize>ulMaxSegSize )
		{
			ulSegSize = ulMaxSegSize;
		}

		/* read the segment */
		iResult = Drv_SpiReadFlash(ptFlashDescription, ulCnt, pucSpiBuffer, ulSegSize);
		if( iResult!=0 )
		{
			tResult = NETX_CONSOLEAPP_RESULT_ERROR;
			break;
		}

		pucCnt = pucSpiBuffer;
		pucEnd = pucSpiBuffer + ulSegSize;
		while( pucCnt<pucEnd )
		{
			ulErased &= *pucCnt;
			if( ulErased!=0xff )
			{
				uprintf("! Memory not erased at offset 0x%08x - expected: 0x%02x found: 0x%02x\n", 
					ulCnt + (unsigned long)(pucCnt - pucSpiBuffer), 0xff, ulErased);
				/* exit inner loop */
				break;
			}
			
			++pucCnt;
		}

		if( ulErased!=0xff )
		{
			/* exit outer loop */
			break;
		}

		/* next segment */
		ulCnt += ulSegSize;
		pucCnt += ulSegSize;

		/* increment progress */
		ulProgressCnt += ulSegSize;
		progress_bar_set_position(ulProgressCnt);
	}

	progress_bar_finalize();

	if( tResult==NETX_CONSOLEAPP_RESULT_OK )
	{
		if( ulErased==0xff )
		{
			uprintf(". CLEAN! The area is erased.\n");
		}
		else
		{
			uprintf(". DIRTY! The area is not erased.\n");
		}
		*ppvReturnMessage = (void*)ulErased;
	}

	return tResult;
}


/*-----------------------------------*/
/**
 * @brief Compute the range of blocks to erase.
 *
 * For a given range of bytes that we want to erase, this function computes a range of erase blocks 
 * that have to be erased. This range of erase blocks must be passed to spi_erase().
 * The size of erase blocks used is internal to the flasher and cannot be selected.
 * ulStartAdr is rounded down to the beginning of the erase block containing ulStartAdr.
 * ulEndAdr is rounded up to the end of the erase block containing ulEndAdr-1.
 * 
 * @param ptFlashDescription [in]  Device information returned by spi_detect.
 * @param ulStartAdr         [in]  Start offset in the flash memory.
 * @param ulEndAdr           [in]  End offset (offset of the last byte +1).
 * @param pulStartAdr        [out] Start offset of the erase area containing the area to be erased.
 * @param pulEndAdr          [out] End offset of the erase area (offset of the last byte +1).
 *
 * @return
 * - NETX_CONSOLEAPP_RESULT_OK: The start and end address of the erase area are in pulStartAdr/pulEndAdr
 * - NETX_CONSOLEAPP_RESULT_ERROR: An error has occurred
 */

NETX_CONSOLEAPP_RESULT_T spi_getEraseArea(const FLASHER_SPI_FLASH_T *ptFlashDescription, unsigned long ulStartAdr, unsigned long ulEndAdr, unsigned long *pulStartAdr, unsigned long *pulEndAdr)
{
	NETX_CONSOLEAPP_RESULT_T  tResult;
	unsigned long ulEraseBlockSize;


	/* NOTE: this code assumes that the serial flash has uniform erase block sizes. */
	ulEraseBlockSize = ptFlashDescription->ulSectorSize;
	uprintf("erase block size: 0x%08x\n", ulEraseBlockSize);
	uprintf("0x%08x - 0x%08x\n", ulStartAdr, ulEndAdr);

	/* round down the first address */
	ulStartAdr /= ulEraseBlockSize;
	ulStartAdr *= ulEraseBlockSize;
	/* round up the last address */
	ulEndAdr += ulEraseBlockSize - 1;
	ulEndAdr /= ulEraseBlockSize;
	ulEndAdr *= ulEraseBlockSize;

	uprintf("0x%08x - 0x%08x\n", ulStartAdr, ulEndAdr);

	*pulStartAdr = ulStartAdr;
	*pulEndAdr = ulEndAdr;

	tResult = NETX_CONSOLEAPP_RESULT_OK;


	/* all OK */
	return tResult;
}

/*-----------------------------------*/


// TODO from here until EOF verify everything

/**
 * first stupid approach:
 */
void analyzeMap(unsigned char * cHexMap, CMD_PARAMETER_SMART_ERASE_T *ptParameter)
{
	unsigned char * eraseMap2 = 0;
	unsigned char * eraseMap3 = 0;
	unsigned char * eraseMap4 = 0;
	unsigned int iCounter = 0;

	if (myData.eraseOperation2 != 0)
	{
		//assume flash space is dividable by eraseOperation
		newArray(&eraseMap2, FLASH_SIZE_BYTE / myData.eraseOperation2);

		unsigned int multiplicator = myData.eraseOperation2 / myData.eraseOperation1;

		for (unsigned int i = 0; i < FLASH_SIZE_BYTE / myData.eraseOperation2; i++)
		{
			/* Check the first 16 4K Blocks (= 64k Sector) if set*/
			for (unsigned int j = 0; j < multiplicator; j++)
			{
				if (getValue(cHexMap, j + i * multiplicator) == 1)
				{
					iCounter++;
				}
				if (iCounter > multiplicator / 2) // todo comment
				{ // then its better to perform sec erase
					setValue(eraseMap2, i, 1);
					for (unsigned int k = 0; k < multiplicator; k++)
					{
						setValue(cHexMap, k + i * multiplicator, 0);
					}

					iCounter = 0;
					break;
				}
			}
			iCounter = 0;
		}
#if DEBUG == 1
		dumpBoolArray2(eraseMap2, FLASH_SIZE_BYTE / myData.eraseOperation2, "Adapted second Map: ");
		dumpBoolArray2(cHexMap, FLASH_SIZE_BYTE / myData.eraseOperation1, "Adapted first map: ");
#endif
	}

	unsigned int iCounter2 = 0;

	if (myData.eraseOperation3 != 0)
	{
		newArray(&eraseMap3, FLASH_SIZE_BYTE / myData.eraseOperation3);

		unsigned int multiplicator = myData.eraseOperation3 / myData.eraseOperation2;

		for (unsigned int i = 0; i < FLASH_SIZE_BYTE / myData.eraseOperation3; i++)
		{
			/* Check the first 16 4K Blocks (= 64k Sector) if set*/
			for (unsigned int j = 0; j < multiplicator; j++)
			{
				if (getValue(eraseMap2, j + i * multiplicator) == 1)
				{
					iCounter2++;
				}
				if (iCounter2 > multiplicator / 2U) // this must be more dynamic
				{ // then its better to perform sec err
					setValue(eraseMap3, i, 1);
					for (unsigned int k = 0; k < multiplicator; k++)
					{
						setValue(eraseMap2, k + i * multiplicator, 0);
					}

					iCounter2 = 0;
					break;
				}
			}
			iCounter2 = 0;

		}
#if DEBUG == 1
		dumpBoolArray2(eraseMap3, FLASH_SIZE_BYTE / myData.eraseOperation3, "Third Map: ");
		dumpBoolArray2(eraseMap2, FLASH_SIZE_BYTE / myData.eraseOperation2, "Adapted second Map: ");
		dumpBoolArray2(cHexMap, FLASH_SIZE_BYTE / myData.eraseOperation1, "Adapted first map: ");
#endif
	}

	if (myData.eraseOperation4 != 0)
	{
		newArray(&eraseMap4, FLASH_SIZE_BYTE / myData.eraseOperation4);

		unsigned int multiplicator = myData.eraseOperation3 / myData.eraseOperation2;

		for (unsigned int i = 0; i < FLASH_SIZE_BYTE / myData.eraseOperation4; i++)
		{
			/* Check the first 16 4K Blocks (= 64k Sector) if set*/
			for (unsigned int j = 0; j < multiplicator; j++)
			{
				if (getValue(eraseMap3, j + i * multiplicator) == 1)
				{
					iCounter++;
				}
				if (iCounter > 8) // this must be more dynamic
				{ // than its better to perform sec err
					setValue(eraseMap4, i, 1);
					for (unsigned int k = 0; k < multiplicator; k++)
					{
						setValue(eraseMap3, k + i * multiplicator, 0);
					}
					iCounter = 0;
					break;
				}
			}
			iCounter = 0;
		}
#if DEBUG == 1
		dumpBoolArray2(eraseMap4, FLASH_SIZE_BYTE / myData.eraseOperation4, "Fourth Map: ");
		dumpBoolArray2(eraseMap3, FLASH_SIZE_BYTE / myData.eraseOperation3, "Third Map: ");
		dumpBoolArray2(eraseMap2, FLASH_SIZE_BYTE / myData.eraseOperation2, "Second Map: ");
		dumpBoolArray2(cHexMap, FLASH_SIZE_BYTE / myData.eraseOperation1, "First Map 2: ");
#endif
	}

//perform erase
	if (myData.eraseOperation1 != 0)
	{

		for (unsigned long i = 0; i < MAP_LENGTH; i++)
		{
			if (1U == getValue(cHexMap, i))
			{
				performErase(myData.eraseOperation1, myData.eraseInstruction1, i, ptParameter);
			}
		}
	}
	if (myData.eraseOperation2 != 0)
	{
		for (unsigned long i = 0; i < myData.pFlashDeviceInfo->ulSize / myData.eraseOperation2; i++)
		{
			if (1U == getValue(eraseMap2, i))
			{
				performErase(myData.eraseOperation2, myData.eraseInstruction2, i, ptParameter);
			}
		}
	}
	if (myData.eraseOperation3 != 0)
	{
		for (unsigned long i = 0; i < FLASH_SIZE_KB / ERASE_SECTOR_SIZE_KB; i++)
		{
			if (1U == getValue(eraseMap3, i))
			{
				performErase(myData.eraseOperation3, myData.eraseInstruction3, (unsigned long) i, ptParameter);
			}
		}
	}
	if (myData.eraseOperation4 != 0)
	{
		for (unsigned long i = 0; i < FLASH_SIZE_KB / ERASE_SECTOR_SIZE_KB; i++)
		{
			if (1U == getValue(eraseMap4, i))
			{
				performErase(myData.eraseOperation4, myData.eraseInstruction4, (unsigned long) i, ptParameter);
			}
		}
	}
}

/**
 *
 */
void performErase(unsigned int eraseMode, unsigned char eraseInstruction, unsigned long startSector, CMD_PARAMETER_SMART_ERASE_T *ptParameter)
{
	unsigned long errMem = 0;
	NETX_CONSOLEAPP_RESULT_T tResult;

	const FLASHER_SPI_FLASH_T *ptFlashDescription;
	unsigned long ulStartAdr;
	unsigned long ulEndAdr;
	int iResult;

	/* erase the block */
	unsigned long ulSectorSize;
	unsigned long ulAddress;

	switch (eraseMode)
	{
	case PAGE_ERASE_256:
#if DEBUG
		uprintf("\nok we're asked to erase 256B at block %d ", startSector);
		uprintf("this block starts at %d in real mem", errMem);
#endif
		errMem = startSector * BLOCKSIZE_BYTE;

		uprintf("Erase at: %d", errMem);

		/* expect success */
		tResult = NETX_CONSOLEAPP_RESULT_OK;

		ptFlashDescription = &(ptParameter->ptDeviceDescription->uInfo.tSpiInfo);
		ulStartAdr = errMem;
		ulEndAdr = errMem + ERASE_BLOCK_MIN_BYTE;

		/* Assume success. */
		tResult = NETX_CONSOLEAPP_RESULT_OK;

		/* Get the sector size. */
		ulSectorSize = ptFlashDescription->ulSectorSize;
		/* Get the offset in the first sector. */

		/* Show the start and the end address of the erase area. */
		uprintf(". erase 0x%08x - 0x%08x\n", ulStartAdr, ulEndAdr);

		ulAddress = ulStartAdr;
		while (ulAddress < ulEndAdr)
		{
#if SIMULATION == 0
			iResult = Drv_SpiEraseFlashArea(ptFlashDescription, ulAddress, (unsigned char) eraseInstruction);
#elif SIMULATION == 1
			iResult = 0;
#endif
			if (iResult != 0)
			{
				uprintf("! erase failed at address 0x%08x\n", ulAddress);
				tResult = NETX_CONSOLEAPP_RESULT_ERROR;
				break;
			}

			/* Move to the next segment. */
			ulAddress += PAGE_ERASE_256; //ulSectorSize;
		}

		if (tResult != NETX_CONSOLEAPP_RESULT_OK)
		{
			uprintf("! erase error\n");
		}

		break;
	case PAGE_ERASE_512:
#if DEBUG
		uprintf("\nok we're asked to erase 512B at block %d ", startSector);
		uprintf("this block starts at %d in real mem", errMem);
#endif
		errMem = startSector * BLOCKSIZE_BYTE;

		uprintf("Erase at: %d", errMem);

		/* expect success */
		tResult = NETX_CONSOLEAPP_RESULT_OK;

		ptFlashDescription = &(ptParameter->ptDeviceDescription->uInfo.tSpiInfo);
		ulStartAdr = errMem;
		ulEndAdr = errMem + ERASE_BLOCK_MIN_BYTE;

		/* Assume success. */
		tResult = NETX_CONSOLEAPP_RESULT_OK;

		/* Get the sector size. */
		ulSectorSize = ptFlashDescription->ulSectorSize;
		/* Get the offset in the first sector. */

		/* Show the start and the end address of the erase area. */
		uprintf(". erase 0x%08x - 0x%08x\n", ulStartAdr, ulEndAdr);

		/* Erase the complete area. Should be one iteration if 4k erase is supported. */
		ulAddress = ulStartAdr;
		while (ulAddress < ulEndAdr)
		{
#if SIMULATION == 0
			iResult = Drv_SpiEraseFlashArea(ptFlashDescription, ulAddress, (unsigned char) eraseInstruction);
#elif SIMULATION == 1
			iResult = 0;
#endif
			if (iResult != 0)
			{
				uprintf("! erase failed at address 0x%08x\n", ulAddress);
				tResult = NETX_CONSOLEAPP_RESULT_ERROR;
				break;
			}

			/* Move to the next segment. */
			ulAddress += PAGE_ERASE_512;
		}

		if (tResult != NETX_CONSOLEAPP_RESULT_OK)
		{
			uprintf("! erase error\n");
		}

		break;
	case BLOCK_ERASE_4K:
#if DEBUG
		uprintf("\nok we're asked to erase 4k at block %d ", startSector);
		uprintf("this block starts at %d in real mem", errMem);
#endif
		errMem = startSector * BLOCKSIZE_BYTE;

		uprintf("Erase at: %d", errMem);

		/* expect success */
		tResult = NETX_CONSOLEAPP_RESULT_OK;

		ptFlashDescription = &(ptParameter->ptDeviceDescription->uInfo.tSpiInfo);
		ulStartAdr = errMem;
		ulEndAdr = errMem + BLOCK_ERASE_4K;

		/* Assume success. */
		tResult = NETX_CONSOLEAPP_RESULT_OK;

		/* Get the sector size. */
		ulSectorSize = ptFlashDescription->ulSectorSize;
		/* Show the start and the end address of the erase area. */
#if DEBUG
		uprintf(". erase 0x%08x - 0x%08x\n", ulStartAdr, ulEndAdr);
#endif
		/* Erase the complete area. Should be 1 iterations*/
		ulAddress = ulStartAdr;
		while (ulAddress < ulEndAdr)
		{

#if SIMULATION == 0
			iResult = Drv_SpiEraseFlashSector(ptFlashDescription, ulAddress);
#elif SIMULATION == 1
			iResult = 0;
#endif
			if (iResult != 0)
			{
				uprintf("! erase failed at address 0x%08x\n", ulAddress);
				tResult = NETX_CONSOLEAPP_RESULT_ERROR;
				break;
			}

			/* Move to the next segment. */
			ulAddress += ulSectorSize;
		}

		if (tResult != NETX_CONSOLEAPP_RESULT_OK)
		{
			uprintf("! erase error\n");
		}

		break;
	case BLOCK_ERASE_32K:
#if DEBUG
		uprintf("\nok we're asked to erase 32K at block %d ", startSector);
		uprintf("this block starts at %d in real mem", errMem);
#endif
		errMem = startSector * BLOCK_ERASE_32K;

		uprintf("Erase at: %d", errMem);

		/* expect success */
		tResult = NETX_CONSOLEAPP_RESULT_OK;

		ptFlashDescription = &(ptParameter->ptDeviceDescription->uInfo.tSpiInfo);
		ulStartAdr = errMem;
		ulEndAdr = errMem + BLOCK_ERASE_32K;

		/* Assume success. */
		tResult = NETX_CONSOLEAPP_RESULT_OK;

		/* Show the start and the end address of the erase area. */
#if DEBUG
		uprintf(". erase 0x%08x - 0x%08x\n", ulStartAdr, ulEndAdr);
#endif
		/* Erase the complete area. Should be one iteration if 4k erase is supported. */
		ulAddress = ulStartAdr;
		while (ulAddress < ulEndAdr)
		{
#if SIMULATION == 0
			iResult = Drv_SpiEraseFlashArea(ptFlashDescription, ulAddress, (unsigned char) eraseInstruction);
#elif SIMULATION == 1
			iResult = 0;
#endif
			if (iResult != 0)
			{
				uprintf("! erase failed at address 0x%08x\n", ulAddress);
				tResult = NETX_CONSOLEAPP_RESULT_ERROR;
				break;
			}

			/* Move to the next segment. */
			ulAddress += BLOCK_ERASE_32K;
		}

		if (tResult != NETX_CONSOLEAPP_RESULT_OK)
		{
			uprintf("! erase error\n");
		}

		break;
	case SECTOR_ERASE_64K:
#if DEBUG
		uprintf("\nok we're asked to erase 64k at block %d ", startSector);
		uprintf("this block starts at %d in real mem", errMem);
#endif
		errMem = startSector * ERASE_SECTOR_SIZE_BYTE; // UND DAS HIER ist redundant

		/* expect success */
		tResult = NETX_CONSOLEAPP_RESULT_OK;

		ptFlashDescription = &(ptParameter->ptDeviceDescription->uInfo.tSpiInfo);
		ulStartAdr = errMem;
		ulEndAdr = errMem + ERASE_SECTOR_SIZE_BYTE;

		/* Assume success. */
		tResult = NETX_CONSOLEAPP_RESULT_OK;

		/* Get the sector size. */
		ulSectorSize = ptFlashDescription->ulSectorSize;

		/* Show the start and the end address of the erase area. */
#if DEBUG
		uprintf(". erase 0x%08x - 0x%08x\n", ulStartAdr, ulEndAdr);
#endif

		/* Erase the complete area.*/
		ulAddress = ulStartAdr;
		while (ulAddress < ulEndAdr)
		{

#if SIMULATION == 0
			iResult = Drv_SpiEraseFlashArea(ptFlashDescription, ulAddress, (unsigned char) eraseInstruction);
#elif SIMULATION == 1
			iResult = 0;
#endif
			if (iResult != 0)
			{
				uprintf("! erase failed at address 0x%08x\n", ulAddress);
				tResult = NETX_CONSOLEAPP_RESULT_ERROR;
				break;
			}

			/* Move to the next segment. */
			ulAddress += SECTOR_ERASE_64K;
		}

		if (tResult != NETX_CONSOLEAPP_RESULT_OK)
		{
			uprintf("! erase error\n");
		}

		break;
	case CHIP_ERASE:
#if DEBUG
		uprintf("\nok we're asked to erase the whole chip");
		uprintf("Erase at: %d", errMem);
#endif
		uprintf("\n\n\n!!!!!!!!!!!!!!!!!!!!!\n!!! NOT SUPPORTED !!!\n!!!!!!!!!!!!!!!!!!!!!\n\n\n", errMem);

		break;
	default:
		break;
	}
}

void initMemory()
{
	totalMemory = (unsigned) (flasher_version.pucBuffer_End - flasher_version.pucBuffer_Data); //THIS MUST (!) BE UNSIGNED --> SANATY CHECKING?
	memStarPtr = flasher_version.pucBuffer_Data;
	memCurrentPtr = memStarPtr;
	freeMem = totalMemory;
	memEndPtr = flasher_version.pucBuffer_End;
#if DEBUG
	uprintf("---\n- DEBUGGING: \n- Total Mem: %d\n- StartPtr: %d", totalMemory, memStarPtr);
#endif

}

unsigned char * getMemory(unsigned long long int sizeByte)
{
	unsigned char * retPtr;
	if (sizeByte > freeMem)
	{
		uprintf("out of mem "); // do some error handling here
		return 0;
	}
	else
	{
		retPtr = memCurrentPtr;
		memCurrentPtr = memCurrentPtr + sizeByte;
		freeMem = freeMem - sizeByte;
	}
#if DEBUG
	uprintf("---\n- DEBUGGING: \n- Allocated Mem size: %d\n- Free Mem size: %d\n- StartPtr: %d", memCurrentPtr - retPtr, freeMem, retPtr);
#endif
	return retPtr;
}

void newArray(unsigned char ** boolArray, unsigned long long int dimension)
{
	if (dimension % 8 != 0)
		dimension = dimension + 8;

	*boolArray = getMemory(dimension);

	for (unsigned int i = 0; i < dimension; i++)
	{
		setValue(*boolArray, i, 0);
	}

#if PARANOIAMODE
	for (unsigned int i = 0; i < dimension; i++)
	{
		if(getValue(*boolArray, i) != 0)
		uprintf("\n\n!!!Mem Dirty!!!\n\n");
	}
#endif
}

/*
 * some sanaty checking should be done
 */
int setValue(unsigned char * array, unsigned long long int index, unsigned char val)
{
	unsigned long long int indexByte = index / 8U;
	unsigned long long int indexBit = index % 8U;
	unsigned long long int x = array[indexByte]; //??? needed

	if (val == 0)
	{
		x = x & ~((unsigned long long int)1U << indexBit); // XXX added explicit cast to supress error for now, need to verify this is the intended behaviour
	}
	else if (val == 1)
	{
		x = x | (unsigned long long int)val << indexBit; // XXX added explicit cast to supress error for now, need to verify this is the intended behaviour
	}
	else
	{
		return -1;
	}

	array[indexByte] = (char) x;
	return 0;
}

unsigned char getValue(unsigned char * array, unsigned long long int index)
{
	unsigned long long int indexByte = index / 8;
	unsigned long long int indexBit = index % 8;
	unsigned long long int a = array[indexByte];
	unsigned long long int b = a & ((unsigned long long int)1 << indexBit); // XXX added explicit cast to supress error for now, need to verify this is the intended behaviour
	unsigned long long int c = b >> indexBit;
	unsigned char yx = (unsigned char) c;

	return yx;
}

void dumpBoolArray16(unsigned char * map, unsigned int len, const char * description)
{
	uprintf("\n%s\n0 \t", description);

	for (unsigned int i = 0; i < len / 8; i++)
	{
		uprintf("0x%02x ", map[i]);
		if (i % 16 == 15)
		{
			if (i % 16 == 16 - 1)
			{
				uprintf("\n%02x\t", (i + 1) * 8);

			}
			else
			{
				uprintf("\n\t");
			}
		}
	}
	uprintf("\n");
}

void dumpBoolArray2(unsigned char * map, unsigned int len, const char * description)
{
	uprintf("\n%s\n", description);

	for (unsigned int i = 0; i < len; i++)
	{
		uprintf("%d", getValue(map, i));
		if (i % 16 == 15)
		{
			uprintf("\n");
		}
	}
	uprintf("\n");
}

/*
 * Goal: get structured information about the erase operations supported by the connected memory.
 */
void setSFDPData(unsigned char isValid, unsigned int eraseOperation1, unsigned char eraseInstruction1, unsigned int eraseOperation2, unsigned char eraseInstruction2, unsigned int eraseOperation3, unsigned char eraseInstruction3,
		unsigned int eraseOperation4, unsigned char eraseInstruction4, SPIFLASH_ATTRIBUTES_T * flashAttributes)
{
	myData.isValid = isValid;

	myData.eraseOperation1 = 0;
	myData.eraseInstruction1 = 0;
	myData.eraseOperation2 = 0;
	myData.eraseInstruction2 = 0;
	myData.eraseOperation3 = 0;
	myData.eraseInstruction3 = 0;
	myData.eraseOperation4 = 0;
	myData.eraseInstruction4 = 0;

	if (eraseOperation1 != 0)
	{
		myData.eraseOperation1 = 1U << eraseOperation1;
		myData.eraseInstruction1 = eraseInstruction1;
	}
	if (eraseOperation2 != 0)
	{
		myData.eraseOperation2 = 1U << eraseOperation2;
		myData.eraseInstruction2 = eraseInstruction2;
	}
	if (eraseOperation3 != 0)
	{
		myData.eraseOperation3 = 1U << eraseOperation3;
		myData.eraseInstruction3 = eraseInstruction3;
	}
	if (eraseOperation4 != 0)
	{
		myData.eraseOperation4 = 1U << eraseOperation4;
		myData.eraseInstruction4 = eraseInstruction4;
	}

	uprintf("\n------- Received SFDP Data -------");
	if (1 == myData.isValid)
	{
		uprintf("\n| SFDP Data is valid:\t\t|");
		uprintf("\n| ERASE OP1: %05d\t| INST: %02x\t|", myData.eraseOperation1, myData.eraseInstruction1);
		uprintf("\n| ERASE OP2: %05d\t| INST: %02x\t|", myData.eraseOperation2, myData.eraseInstruction2);
		uprintf("\n| ERASE OP3: %05d\t| INST: %02x\t|", myData.eraseOperation3, myData.eraseInstruction3);
		uprintf("\n| ERASE OP4: %05d\t| INST: %02x\t|", myData.eraseOperation4, myData.eraseInstruction4);
	}
	else
	{
		uprintf("\n| SFDP Data is invalid performing normal erase\t|");
	}
	uprintf("\n----------------------------------\n\n");

	myData.pFlashDeviceInfo = flashAttributes;
}