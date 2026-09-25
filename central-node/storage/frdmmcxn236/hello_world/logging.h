/*
 * logging.h - Central node storage (Person B / Sarah)
 *
 * Binary LogEntry_t logging to microSD via FATFS, with file rotation and
 * error handling (card full, write failure). Sits on top of ff.c/diskio.c;
 * does not touch SPI/SD hardware directly.
 *
 * Usage:
 *   log_status_t st = logging_init();
 *   if (st != LOG_OK) { handle error }
 *   ...
 *   LogEntry_t e = { ... };            // filled from Person A's CAN queue
 *   st = logging_write_entry(&e);
 *   if (st == LOG_ERR_CARD_FULL) { handle: stop logging / alert }
 *   ...
 *   logging_close();                   // shutdown, before low-power entry, etc.
 */

#ifndef LOGGING_H
#define LOGGING_H

#include "log_entry.h"
#include <stdint.h>
#include <stdbool.h>

/* Rotate to a new file once the current one reaches this size. */
#define LOG_MAX_FILE_SIZE     (1UL * 1024UL * 1024UL) /* 1 MB per file */

/* Stop accepting new writes once free space drops below this, to avoid
   filling the card completely (FAT metadata needs headroom too). */
#define LOG_MIN_FREE_BYTES    (64UL * 1024UL) /* 64 KB */

#define LOG_FILENAME_PREFIX   "LOG"
#define LOG_FILENAME_EXT      ".BIN"
#define LOG_FILENAME_MAXLEN   32

typedef enum
{
    LOG_OK = 0,
    LOG_ERR_MOUNT,     /* f_mount failed */
    LOG_ERR_OPEN,      /* could not create a log file */
    LOG_ERR_WRITE,     /* f_write or f_sync failed, or a short record was written */
    LOG_ERR_CARD_FULL, /* free space below LOG_MIN_FREE_BYTES, or could not be read */
    LOG_ERR_NOT_INIT   /* no open log file: logging_init() not called, or after logging_close() */
} log_status_t;

/* Mounts the SD card, scans for existing LOG####.BIN files, and creates the
   next file in sequence for new entries. Safe on a card that already has other
   files: only files matching LOG####.BIN are considered, everything else is
   left untouched, and existing log files are never overwritten (new entries
   always go to a fresh, higher-numbered file). Calling it again after
   logging_close() resumes logging in a new file. */
log_status_t logging_init(void);

/* Writes one LogEntry_t as a binary record to the current log file and syncs
   it to the card. Rotates to a new file automatically at LOG_MAX_FILE_SIZE.
   Returns LOG_ERR_CARD_FULL if free space is too low to continue safely; the
   caller should stop logging and/or raise an alert, since this module never
   deletes old files on its own. */
log_status_t logging_write_entry(const LogEntry_t *entry);

/* Flushes and closes the current log file. Call before power-down or a
   low-power sleep cycle. Further writes return LOG_ERR_NOT_INIT until
   logging_init() is called again. */
void logging_close(void);

#endif /* LOGGING_H */