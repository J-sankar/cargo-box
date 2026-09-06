/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2022 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * ---------------------------------------------------------------------------
 * MODIFIED for cargo-box project (Person A, central_can_management):
 * - Wrapped into a FreeRTOS task (can_task) + minimal main().
 * - Demo now sends/receives a STRING typed at the console instead of a
 *   counter byte, for easier manual testing over CAN.
 * ---------------------------------------------------------------------------
 */

#include "FreeRTOS.h"
#include "task.h"

#include "fsl_debug_console.h"
#include "fsl_flexcan.h"
#include "board.h"
#include "app.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define LOG_INFO (void)PRINTF
#define DLC (8)

#define CAN_TASK_PRIORITY (configMAX_PRIORITIES - 2)
#define CAN_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE + 400)

/* Max characters we can fit in one classic CAN frame (8 data bytes). */
#define CAN_MSG_MAX_LEN 8

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void can_task(void *pvParameters);
static uint8_t CAN_ReadStringFromConsole(char *buf, uint8_t maxLen);
static void CAN_SetFrameDataFromString(flexcan_frame_t *f, const char *buf, uint8_t len);
static void CAN_PrintFrameAsString(flexcan_frame_t *f);

/*******************************************************************************
 * Variables
 ******************************************************************************/
flexcan_handle_t flexcanHandle;
volatile bool txComplete = false;
volatile bool rxComplete = false;
volatile bool wakenUp    = false;
flexcan_mb_transfer_t txXfer, rxXfer;
flexcan_frame_t frame;
uint32_t txIdentifier;
uint32_t rxIdentifier;

/*******************************************************************************
 * Code
 ******************************************************************************/

/*!
 * @brief Reads characters from the console until Enter is pressed or maxLen
 *        is reached. Echoes each character back so you can see what you type.
 *        Returns the number of characters read (not null-terminated in buf).
 */
static uint8_t CAN_ReadStringFromConsole(char *buf, uint8_t maxLen)
{
    uint8_t idx = 0;
    char c;

    while (idx < maxLen)
    {
        c = GETCHAR();
        if ((c == '\r') || (c == '\n'))
        {
            break;
        }
        PUTCHAR(c); /* local echo so you can see what you're typing */
        buf[idx++] = c;
    }
    PUTCHAR('\r');
    PUTCHAR('\n');
    return idx;
}

/*!
 * @brief Packs up to 8 bytes of a string into the frame's individual data
 *        byte fields. Unused trailing bytes are zero-filled.
 */
static void CAN_SetFrameDataFromString(flexcan_frame_t *f, const char *buf, uint8_t len)
{
    uint8_t data[CAN_MSG_MAX_LEN] = {0};
    uint8_t i;

    for (i = 0; (i < len) && (i < CAN_MSG_MAX_LEN); i++)
    {
        data[i] = (uint8_t)buf[i];
    }

    f->dataByte0 = data[0];
    f->dataByte1 = data[1];
    f->dataByte2 = data[2];
    f->dataByte3 = data[3];
    f->dataByte4 = data[4];
    f->dataByte5 = data[5];
    f->dataByte6 = data[6];
    f->dataByte7 = data[7];
}

/*!
 * @brief Prints a received frame's data bytes back out as a null-terminated
 *        string, using the frame's DLC (length) to know how many bytes
 *        are meaningful.
 */
static void CAN_PrintFrameAsString(flexcan_frame_t *f)
{
    char str[CAN_MSG_MAX_LEN + 1];
    uint8_t len = f->length;

    if (len > CAN_MSG_MAX_LEN)
    {
        len = CAN_MSG_MAX_LEN;
    }

    str[0] = (char)f->dataByte0;
    str[1] = (char)f->dataByte1;
    str[2] = (char)f->dataByte2;
    str[3] = (char)f->dataByte3;
    str[4] = (char)f->dataByte4;
    str[5] = (char)f->dataByte5;
    str[6] = (char)f->dataByte6;
    str[7] = (char)f->dataByte7;
    str[len] = '\0';

    LOG_INFO("Received: \"%s\" (DLC=%d, ID=0x%x)\r\n", str, f->length, f->id >> CAN_ID_STD_SHIFT);
}

static FLEXCAN_CALLBACK(flexcan_callback)
{
    switch (status)
    {
        case kStatus_FLEXCAN_RxIdle:
            if (RX_MESSAGE_BUFFER_NUM == result) { rxComplete = true; }
            break;
        case kStatus_FLEXCAN_TxIdle:
            if (TX_MESSAGE_BUFFER_NUM == result) { txComplete = true; }
            break;
        case kStatus_FLEXCAN_WakeUp:
            wakenUp = true;
            break;
        default:
            break;
    }
}

static void can_task(void *pvParameters)
{
    flexcan_config_t flexcanConfig;
    flexcan_rx_mb_config_t mbConfig;
    uint8_t node_type;
    char msgBuf[CAN_MSG_MAX_LEN];
    uint8_t msgLen;

    PRINTF("MCUX SDK version: %s\r\n", MCUXSDK_VERSION_FULL_STR);
    LOG_INFO("********* FLEXCAN Interrupt EXAMPLE (FreeRTOS, string mode) *********\r\n");

    do {
        LOG_INFO("Please select local node as A or B:\r\n");
        LOG_INFO("Note: Node B should start first.\r\n");
        LOG_INFO("Node:");
        node_type = GETCHAR();
        LOG_INFO("%c\r\n", node_type);
    } while ((node_type != 'A') && (node_type != 'B') && (node_type != 'a') && (node_type != 'b'));

    if ((node_type == 'A') || (node_type == 'a')) {
        txIdentifier = 0x321;
        rxIdentifier = 0x123;
    } else {
        txIdentifier = 0x123;
        rxIdentifier = 0x321;
    }

    FLEXCAN_GetDefaultConfig(&flexcanConfig);
    flexcanConfig.bitRate = 500000U;
    flexcanConfig.bitRateFD = 2000000U;

#if defined(EXAMPLE_CAN_CLK_SOURCE)
    flexcanConfig.clkSrc = EXAMPLE_CAN_CLK_SOURCE;
#endif
#if defined(EXAMPLE_CAN_BIT_RATE)
    flexcanConfig.bitRate = EXAMPLE_CAN_BIT_RATE;
#endif

    FLEXCAN_Init(EXAMPLE_CAN, &flexcanConfig, EXAMPLE_CAN_CLK_FREQ);
    FLEXCAN_TransferCreateHandle(EXAMPLE_CAN, &flexcanHandle, flexcan_callback, NULL);
    FLEXCAN_SetRxMbGlobalMask(EXAMPLE_CAN, FLEXCAN_RX_MB_STD_MASK(0x7FFU, 0, 0));

    mbConfig.format = kFLEXCAN_FrameFormatStandard;
    mbConfig.type   = kFLEXCAN_FrameTypeData;
    mbConfig.id     = FLEXCAN_ID_STD(rxIdentifier);
    FLEXCAN_SetRxMbConfig(EXAMPLE_CAN, RX_MESSAGE_BUFFER_NUM, &mbConfig, true);
    FLEXCAN_SetTxMbConfig(EXAMPLE_CAN, TX_MESSAGE_BUFFER_NUM, true);

    if ((node_type == 'A') || (node_type == 'a')) {
        LOG_INFO("Type a message (max %d chars) and press Enter to send it\r\n\r\n", CAN_MSG_MAX_LEN);
    } else {
        LOG_INFO("Waiting for a message from Node A\r\n\r\n");
    }

    while (true)
    {
        if ((node_type == 'A') || (node_type == 'a'))
        {
            LOG_INFO("Send: ");
            msgLen = CAN_ReadStringFromConsole(msgBuf, CAN_MSG_MAX_LEN);

            frame.id     = FLEXCAN_ID_STD(txIdentifier);
            frame.format = (uint8_t)kFLEXCAN_FrameFormatStandard;
            frame.type   = (uint8_t)kFLEXCAN_FrameTypeData;
            frame.length = msgLen;
            CAN_SetFrameDataFromString(&frame, msgBuf, msgLen);

            txXfer.mbIdx = (uint8_t)TX_MESSAGE_BUFFER_NUM;
            txXfer.frame = &frame;
            (void)FLEXCAN_TransferSendNonBlocking(EXAMPLE_CAN, &flexcanHandle, &txXfer);
            while (!txComplete) { taskYIELD(); }
            txComplete = false;

            rxXfer.mbIdx = (uint8_t)RX_MESSAGE_BUFFER_NUM;
            rxXfer.frame = &frame;
            (void)FLEXCAN_TransferReceiveNonBlocking(EXAMPLE_CAN, &flexcanHandle, &rxXfer);
            while (!rxComplete) { taskYIELD(); }
            rxComplete = false;

            CAN_PrintFrameAsString(&frame);
            LOG_INFO("\r\n");
        }
        else
        {
            if (wakenUp) { LOG_INFO("B has been waken up!\r\n\r\n"); }

            rxXfer.mbIdx = (uint8_t)RX_MESSAGE_BUFFER_NUM;
            rxXfer.frame = &frame;
            (void)FLEXCAN_TransferReceiveNonBlocking(EXAMPLE_CAN, &flexcanHandle, &rxXfer);
            while (!rxComplete) { taskYIELD(); }
            rxComplete = false;

            CAN_PrintFrameAsString(&frame);

            /* Echo the same frame content straight back to Node A. */
            frame.id     = FLEXCAN_ID_STD(txIdentifier);
            txXfer.mbIdx = (uint8_t)TX_MESSAGE_BUFFER_NUM;
            txXfer.frame = &frame;
            (void)FLEXCAN_TransferSendNonBlocking(EXAMPLE_CAN, &flexcanHandle, &txXfer);
            while (!txComplete) { taskYIELD(); }
            txComplete = false;
        }
    }
}

int main(void)
{
    BOARD_InitHardware();

    if (xTaskCreate(can_task, "CAN_Task", CAN_TASK_STACK_SIZE, NULL, CAN_TASK_PRIORITY, NULL) != pdPASS)
    {
        PRINTF("CAN task creation failed!\r\n");
        while (1) {}
    }

    vTaskStartScheduler();

    for (;;) {}
}
/***********************************************************************************************************************
 * EOF
 **********************************************************************************************************************/
