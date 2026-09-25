/*
 * logging.c - Central node storage (FATFS + microSD binary logging).
 * See logging.h for usage.
 */

#include "logging.h"
#include "ff.h"
#include <stdio.h>  /* snprintf */
#include <stdlib.h> /* strtoul */
#include <string.h>

static FATFS s_fatfs;
static FIL   s_file;
static bool  s_mounted      = false;
static bool  s_fileOpen     = false;
static DWORD s_currentIndex = 0; /* Numeric suffix of the open log file */
static DWORD s_currentSize  = 0; /* Bytes written so far to the open file */

/* Builds "LOG####.BIN" into out (out must be >= LOG_FILENAME_MAXLEN bytes). */
static void build_filename(char *out, DWORD index)
{
    snprintf(out, LOG_FILENAME_MAXLEN, "%s%04lu%s",
             LOG_FILENAME_PREFIX, (unsigned long)index, LOG_FILENAME_EXT);
}

/*
 * Scans the root directory for existing LOG####.BIN files (ignoring any other
 * files on the card) and returns the highest index found, or 0 if none exist.
 * New logging starts at (highest + 1), so an existing file is never reopened
 * or overwritten.
 */
static DWORD find_highest_existing_index(void)
{
    DIR dir;
    FILINFO fno;
    DWORD highest = 0;
    size_t prefixLen = strlen(LOG_FILENAME_PREFIX);
    size_t extLen    = strlen(LOG_FILENAME_EXT);

    if (f_opendir(&dir, "/") != FR_OK)
    {
        return 0;
    }

    for (;;)
    {
        if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0)
        {
            break; /* End of directory or error */
        }
        if (fno.fattrib & AM_DIR)
        {
            continue;
        }

        size_t nameLen = strlen(fno.fname);
        if (nameLen <= prefixLen + extLen ||
            strncmp(fno.fname, LOG_FILENAME_PREFIX, prefixLen) != 0 ||
            strcmp(fno.fname + nameLen - extLen, LOG_FILENAME_EXT) != 0)
        {
            continue; /* Not one of our log files */
        }

        /* Parse the digits between prefix and extension */
        char digits[LOG_FILENAME_MAXLEN];
        size_t digitLen = nameLen - prefixLen - extLen;
        if (digitLen >= sizeof(digits))
        {
            continue;
        }
        memcpy(digits, fno.fname + prefixLen, digitLen);
        digits[digitLen] = '\0';

        DWORD idx = (DWORD)strtoul(digits, NULL, 10);
        if (idx > highest)
        {
            highest = idx;
        }
    }

    f_closedir(&dir);
    return highest;
}

/* Returns LOG_OK if there is enough free space to keep logging. */
static log_status_t check_free_space(void)
{
    FATFS *fs;
    DWORD freeClusters;

    if (f_getfree("0:", &freeClusters, &fs) != FR_OK)
    {
        /* Cannot check: be conservative and treat the card as full */
        return LOG_ERR_CARD_FULL;
    }

    uint64_t freeBytes = (uint64_t)freeClusters * fs->csize * FF_MAX_SS;
    return (freeBytes < LOG_MIN_FREE_BYTES) ? LOG_ERR_CARD_FULL : LOG_OK;
}

/* Creates a new log file at s_currentIndex. Fails if it already exists
   rather than overwriting it. */
static log_status_t open_new_log_file(void)
{
    char filename[LOG_FILENAME_MAXLEN];
    build_filename(filename, s_currentIndex);

    if (f_open(&s_file, filename, FA_WRITE | FA_CREATE_NEW) != FR_OK)
    {
        return LOG_ERR_OPEN;
    }

    s_fileOpen    = true;
    s_currentSize = 0;
    return LOG_OK;
}

log_status_t logging_init(void)
{
    if (f_mount(&s_fatfs, "0:", 1) != FR_OK) /* 1 = mount now, not lazily */
    {
        s_mounted = false;
        return LOG_ERR_MOUNT;
    }
    s_mounted = true;

    s_currentIndex = find_highest_existing_index() + 1;
    return open_new_log_file();
}

log_status_t logging_write_entry(const LogEntry_t *entry)
{
    if (!s_mounted || !s_fileOpen)
    {
        return LOG_ERR_NOT_INIT;
    }

    log_status_t spaceStatus = check_free_space();
    if (spaceStatus != LOG_OK)
    {
        return spaceStatus; /* Caller decides what to do when the card is full */
    }

    /* Rotate to a new file if this entry would exceed the size cap */
    if (s_currentSize + sizeof(LogEntry_t) > LOG_MAX_FILE_SIZE)
    {
        f_close(&s_file);
        s_fileOpen = false;
        s_currentIndex++;

        log_status_t openStatus = open_new_log_file();
        if (openStatus != LOG_OK)
        {
            return openStatus;
        }
    }

    UINT bytesWritten = 0;
    FRESULT res = f_write(&s_file, entry, sizeof(LogEntry_t), &bytesWritten);
    if (res != FR_OK || bytesWritten != sizeof(LogEntry_t))
    {
        f_sync(&s_file); /* Save earlier entries before reporting the error */
        return LOG_ERR_WRITE;
    }
    s_currentSize += sizeof(LogEntry_t);

    /* Sync on every write: for a black box, not losing data on power loss
       matters more than write speed. If profiling shows this is a bottleneck,
       sync every N entries instead and accept a small risk window. */
    if (f_sync(&s_file) != FR_OK)
    {
        return LOG_ERR_WRITE;
    }

    return LOG_OK;
}

void logging_close(void)
{
    if (s_fileOpen)
    {
        f_sync(&s_file);
        f_close(&s_file);
        s_fileOpen = false;
    }
}