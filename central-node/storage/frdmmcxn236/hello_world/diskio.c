/*
 * FATFS disk layer for SD-over-SPI on the FRDM-MCXN236.
 * Bridges FATFS to NXP's generic fsl_sdspi driver (middleware/sdmmc);
 * the LPSPI3 host callbacks live in lpspi_sdspi_host.c.
 */

#include "ff.h" /* Must come before diskio.h: defines BYTE/DWORD/UINT/LBA_t */
#include "diskio.h"
#include "fsl_debug_console.h"
#include "lpspi_sdspi_host.h"

/*
 * Timestamps: while FF_FS_NORTC is 1 in ffconf.h, FATFS uses the fixed
 * FF_NORTC_* date and never calls get_fattime(), so the RTC driver is not
 * needed. To use the real RTC once the driver is fixed, set FF_FS_NORTC to 0
 * in ffconf.h and re-enable fsl_rtc.c in CMakeLists.txt.
 */
#if !FF_FS_NORTC
#include "fsl_rtc.h"
#endif

static volatile DSTATUS driveStatus = STA_NOINIT;

#if !FF_FS_NORTC
DWORD get_fattime(void)
{
    rtc_datetime_t datetime;
    RTC_GetDatetime(RTC, &datetime);

    return ((DWORD)(datetime.year - 1980) << 25) |
           ((DWORD)datetime.month << 21) |
           ((DWORD)datetime.day << 16) |
           ((DWORD)datetime.hour << 11) |
           ((DWORD)datetime.minute << 5) |
           ((DWORD)(datetime.second / 2));
}
#endif

DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != 0)
    {
        return STA_NOINIT;
    }
    return driveStatus;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != 0)
    {
        return STA_NOINIT;
    }

    sdspi_host_init();

    status_t status = SDSPI_Init(&g_card);
    if (status != kStatus_Success)
    {
        PRINTF("SDSPI_Init failed: %d\r\n", (int)status);
        driveStatus = STA_NOINIT;
    }
    else
    {
        driveStatus &= ~STA_NOINIT;
    }
    return driveStatus;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0 || count == 0)
    {
        return RES_PARERR;
    }
    if (driveStatus & STA_NOINIT)
    {
        return RES_NOTRDY;
    }

    return (SDSPI_ReadBlocks(&g_card, buff, sector, count) == kStatus_Success) ? RES_OK : RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    if (pdrv != 0 || count == 0)
    {
        return RES_PARERR;
    }
    if (driveStatus & STA_NOINIT)
    {
        return RES_NOTRDY;
    }

    return (SDSPI_WriteBlocks(&g_card, (uint8_t *)buff, sector, count) == kStatus_Success) ? RES_OK : RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (pdrv != 0)
    {
        return RES_PARERR;
    }

    switch (cmd)
    {
        case CTRL_SYNC:
            return RES_OK; /* SDSPI writes are synchronous */

        case GET_SECTOR_COUNT:
            if (!buff)
            {
                return RES_PARERR;
            }
            *(DWORD *)buff = g_card.blockCount;
            return RES_OK;

        case GET_SECTOR_SIZE:
            if (!buff)
            {
                return RES_PARERR;
            }
            *(WORD *)buff = (WORD)g_card.blockSize;
            return RES_OK;

        default:
            return RES_PARERR;
    }
}