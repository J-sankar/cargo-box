/*
 * Copyright (c) 2013 - 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2017, 2024 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Central node storage test: initialises the logging module, writes one
 * LogEntry_t to a new LOG####.BIN file and closes it.
 */

#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "board.h"
#include "app.h"
#include "log_entry.h"
#include "logging.h"

int main(void)
{
    BOARD_InitHardware();

    log_status_t status = logging_init();
    if (status != LOG_OK)
    {
        PRINTF("logging_init failed: %d\r\n", (int)status);
    }
    else
    {
        LogEntry_t testEntry = {
            .can_id    = 0x123,
            .data      = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00, 0x00, 0x00},
            .dlc       = 4,
            .timestamp = 0 /* Placeholder until the RTC timestamp is available */
        };

        status = logging_write_entry(&testEntry);
        PRINTF("logging_write_entry returned: %d\r\n", (int)status);
        logging_close();
    }

    while (1)
    {
    }
}