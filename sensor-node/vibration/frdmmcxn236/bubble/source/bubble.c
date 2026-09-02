/*
 * Copyright 2024 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_debug_console.h"
#include "board.h"
#include "math.h"
#include "fsl_fxls.h"
#include "fsl_ctimer.h"
#include "fsl_flexcan.h"
#include "app.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
/* Upper bound and lower bound angle values */
#define ANGLE_UPPER_BOUND 85U
#define ANGLE_LOWER_BOUND 5U

/* Sensor Node Definitions */
#define NODE_CAN_ID          0x100U    /* Vibration Node Identifier */
#define SHOCK_THRESHOLD_G    2.0       /* Local impact alert threshold in Gs */
#define SAMPLE_INTERVAL_MS   10        /* Sensor reading delay (~10ms) */
#define SEND_INTERVAL_MS     10000     /* 10-second periodic transmit */

typedef struct {
    uint32_t can_id;
    uint8_t  data[8];
    uint8_t  dlc;        // data length (bytes actually used)
    uint32_t timestamp;
} LogEntry_t;

/*******************************************************************************
 * Variables
 ******************************************************************************/
/* FXLS device address */
const uint8_t g_accel_address = 0x18U;
volatile uint32_t g_pwmPeriod   = 0U;
volatile uint32_t g_pulsePeriod = 0U;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void Pack_CAN_Payload(LogEntry_t *entry, int16_t xRaw, int16_t yRaw, int16_t zRaw, uint8_t dataScale, uint8_t sensorErrorFlag);

/*******************************************************************************
 * Code
 ******************************************************************************/
status_t CTIMER_GetPwmPeriodValue(uint32_t pwmFreqHz, uint8_t dutyCyclePercent, uint32_t timerClock_Hz)
{
    /* Calculate PWM period match value */
    g_pwmPeriod = (timerClock_Hz / pwmFreqHz) - 1U;

    /* Calculate pulse width match value */
    g_pulsePeriod = (g_pwmPeriod + 1U) * (100 - dutyCyclePercent) / 100;

    return kStatus_Success;
}

status_t CTIMER_UpdatePwmPulsePeriodValue(uint8_t dutyCyclePercent)
{
    /* Calculate pulse width match value */
    g_pulsePeriod = (g_pwmPeriod + 1U) * (100 - dutyCyclePercent) / 100;

    return kStatus_Success;
}

/* Initialize timer module */
static void Timer_Init(void)
{   
    ctimer_config_t config;
    uint32_t srcClock_Hz;
    uint32_t timerClock;
    
    srcClock_Hz = BOARD_TIMER_SOURCE_CLOCK;
    
    CTIMER_GetDefaultConfig(&config);
    timerClock = srcClock_Hz / (config.prescale + 1);
    
    CTIMER_Init(BOARD_TIMER_BASEADDR, &config);
    
    /* Get the PWM period match value and pulse width match value of 20Khz PWM signal with 50% dutycycle */
    CTIMER_GetPwmPeriodValue(20000, 50, timerClock);
    CTIMER_SetupPwmPeriod(BOARD_TIMER_BASEADDR, kCTIMER_Match_1, BOARD_FIRST_TIMER_CHANNEL, g_pwmPeriod, g_pulsePeriod, false);
    CTIMER_SetupPwmPeriod(BOARD_TIMER_BASEADDR, kCTIMER_Match_1, BOARD_SECOND_TIMER_CHANNEL, g_pwmPeriod, g_pulsePeriod, false);
    CTIMER_StartTimer(BOARD_TIMER_BASEADDR);
}

/* Update the duty cycle of an active pwm signal */
static void Board_UpdatePwm(uint16_t x, uint16_t y)
{
    /* Updated duty cycle */
    CTIMER_UpdatePwmPulsePeriodValue((uint8_t)x);
    CTIMER_UpdatePwmPulsePeriod(BOARD_TIMER_BASEADDR, BOARD_FIRST_TIMER_CHANNEL, g_pulsePeriod);
    CTIMER_UpdatePwmPulsePeriodValue((uint8_t)y);
    CTIMER_UpdatePwmPulsePeriod(BOARD_TIMER_BASEADDR, BOARD_SECOND_TIMER_CHANNEL, g_pulsePeriod);
}

/* Encapsulate raw sensor metrics into ID 0x100 specification layout (Little-Endian) */
static void Pack_CAN_Payload(LogEntry_t *entry, int16_t xRaw, int16_t yRaw, int16_t zRaw, uint8_t dataScale, uint8_t sensorErrorFlag)
{
    static uint8_t sampleCounter = 0;

    /* Scale values to 0.01 g/LSB units (signed int16_t) */
    int16_t xScaled = (int16_t)(((double)xRaw * (double)dataScale / 2048.0) * 100.0);
    int16_t yScaled = (int16_t)(((double)yRaw * (double)dataScale / 2048.0) * 100.0);
    int16_t zScaled = (int16_t)(((double)zRaw * (double)dataScale / 2048.0) * 100.0);

    /* Construct status flag byte (bit 0 = sensor error) */
    uint8_t statusFlags = sensorErrorFlag ? 0x01 : 0x00;

    entry->can_id    = NODE_CAN_ID;
    entry->dlc       = 8;
    entry->timestamp = 0; /* Timestamp appended by Central Node */

    /* Bytes 0–1: Accel X (int16_t, Little-Endian) */
    entry->data[0] = (uint8_t)(xScaled & 0xFF);
    entry->data[1] = (uint8_t)((xScaled >> 8) & 0xFF);

    /* Bytes 2–3: Accel Y (int16_t, Little-Endian) */
    entry->data[2] = (uint8_t)(yScaled & 0xFF);
    entry->data[3] = (uint8_t)((yScaled >> 8) & 0xFF);

    /* Bytes 4–5: Accel Z (int16_t, Little-Endian) */
    entry->data[4] = (uint8_t)(zScaled & 0xFF);
    entry->data[5] = (uint8_t)((zScaled >> 8) & 0xFF);

    /* Byte 6: Sample counter (wraps 0–255) */
    entry->data[6] = sampleCounter++;

    /* Byte 7: Status flags */
    entry->data[7] = statusFlags;
}

int main(void)
{
    fxls_handle_t fxlsHandle    = {0};
    fxls_accel_data_t accelData = {0};
    fxls_config_t config        = {0};
    status_t result;
    uint8_t sensorRange     = 0;
    uint8_t dataScale       = 0;
    int16_t xAngle          = 0;
    int16_t yAngle          = 0;
    int16_t xDuty           = 0;
    int16_t yDuty           = 0;

    uint32_t elapsedTimeMs  = 0;
    LogEntry_t txFrame      = {0};

    /* Board pin, clock, debug console init */
    BOARD_InitHardware();

    /* I2C initialize */
    BOARD_Accel_I2C_Init();

    /* Configure the I2C function */
    config.I2C_SendFunc    = BOARD_Accel_I2C_Send;
    config.I2C_ReceiveFunc = BOARD_Accel_I2C_Receive;
    config.slaveAddress    = g_accel_address;

    /* Initialize sensor devices */
    result = FXLS_Init(&fxlsHandle, &config);

    if (result != kStatus_Success)
    {
        PRINTF("\r\nSensor device initialize failed!\r\n");
        return -1;
    }

    /* Get sensor range */
    if (FXLS_ReadReg(&fxlsHandle, SENS_CONFIG1_REG, &sensorRange, 1) != kStatus_Success)
    {
        return -1;
    }

    sensorRange = (sensorRange & 0x6) >> 1;

    if (sensorRange == 0x00)
    {
        dataScale = 2U;
    }
    else if (sensorRange == 0x01)
    {
        dataScale = 4U;
    }
    else if (sensorRange == 0x10)
    {
        dataScale = 8U;
    }
    else if (sensorRange == 0x11)
    {
        dataScale = 16U;
    }

    /* Init timer */
    Timer_Init();

    PRINTF("\r\n--- Node 1: Vibration Monitor Online (CAN ID: 0x100) ---\r\n");

    /* FlexCAN Driver Setup */
    flexcan_config_t flexcanConfig;
    flexcan_frame_t txFrameHardware;

    /* Standard configuration (500 kbps) */
    FLEXCAN_GetDefaultConfig(&flexcanConfig);
    flexcanConfig.baudRate = 500000U;

    /* Initialize CAN0 hardware using system clock */
    FLEXCAN_Init(CAN0, &flexcanConfig, CLOCK_GetFlexcanClkFreq(0));

    /* Enable Transmit Message Buffer (MB 0) */
    FLEXCAN_SetTxMbConfig(CAN0, 0, true);

    /* Main loop */
    while (1)
    {
        /* Get new accelerometer data */
        if (FXLS_ReadAccelData(&fxlsHandle, &accelData) != kStatus_Success)
        {
            SDK_DelayAtLeastUs(SAMPLE_INTERVAL_MS * 1000, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
            continue;
        }

        int16_t xRaw = accelData.accelX;
        int16_t yRaw = accelData.accelY;
        int16_t zRaw = accelData.accelZ;

        /* Convert raw data to angle for local LED indicator */
        xAngle = (int16_t)floor((double)xRaw * (double)dataScale * 90 / 2048);
        if (xAngle < 0) xAngle *= -1;

        yAngle = (int16_t)floor((double)yRaw * (double)dataScale * 90 / 2048);
        if (yAngle < 0) yAngle *= -1;

        /* Update PWM Duty Cycles */
        if (xAngle > ANGLE_UPPER_BOUND) xDuty = 100;
        if (yAngle > ANGLE_UPPER_BOUND) yDuty = 100;
        if (xAngle < ANGLE_LOWER_BOUND) xDuty = 0;
        if (yAngle < ANGLE_LOWER_BOUND) yDuty = 0;

        Board_UpdatePwm(xDuty, yDuty);

        /* Compute 3D Shock Vector Magnitude for local evaluation */
        double fx = (double)xRaw;
        double fy = (double)yRaw;
        double fz = (double)zRaw;
        double totalG = sqrt(fx * fx + fy * fy + fz * fz) / 2048.0 * (double)dataScale;

        /* Check for impact alert threshold */
        uint8_t shockFlag = (totalG > SHOCK_THRESHOLD_G) ? 1 : 0;

        /* Track interval timer */
        elapsedTimeMs += SAMPLE_INTERVAL_MS;

        /* Trigger transmission every 10 seconds OR immediately on local shock alert */
        if ((elapsedTimeMs >= SEND_INTERVAL_MS) || shockFlag)
        {
            elapsedTimeMs = 0;

            /* Construct LogEntry_t CAN payload compliant with ID 0x100 spec */
            Pack_CAN_Payload(&txFrame, xRaw, yRaw, zRaw, dataScale, 0);

            /* Console output formatting */
            int32_t gInt = (int32_t)totalG;
            int32_t gDec = (int32_t)((totalG - (double)gInt) * 100);
            if (gDec < 0) gDec *= -1;

            PRINTF("[CAN TX Prep] ID: 0x%03X | DLC: %d | Data: ", txFrame.can_id, txFrame.dlc);
            for (int i = 0; i < txFrame.dlc; i++)
            {
                PRINTF("%02X ", txFrame.data[i]);
            }
            PRINTF("| Mag: %d.%02dg %s\r\n", gInt, gDec, shockFlag ? "[SHOCK EVENT!]" : "");

            /* Format hardware FlexCAN frame */
            txFrameHardware.format = kFLEXCAN_FrameFormatStandard;
            txFrameHardware.type   = kFLEXCAN_FrameTypeData;
            txFrameHardware.id     = FLEXCAN_ID_STD(txFrame.can_id);
            txFrameHardware.length = txFrame.dlc;

            /* Pack Little-Endian payload array into FlexCAN 32-bit registers */
            txFrameHardware.dataWord0 = ((uint32_t)txFrame.data[3] << 24) |
                                        ((uint32_t)txFrame.data[2] << 16) |
                                        ((uint32_t)txFrame.data[1] << 8)  |
                                        ((uint32_t)txFrame.data[0]);

            txFrameHardware.dataWord1 = ((uint32_t)txFrame.data[7] << 24) |
                                        ((uint32_t)txFrame.data[6] << 16) |
                                        ((uint32_t)txFrame.data[5] << 8)  |
                                        ((uint32_t)txFrame.data[4]);

            /* Transmit frame out onto physical bus */
            status_t status = FLEXCAN_WriteTxMb(CAN0, 0, &txFrameHardware);
            if (status == kStatus_Success)
            {
                PRINTF(" -> Bus Transmit OK!\r\n");
            }
            else
            {
                PRINTF(" -> Bus Transmit Failed (Code: 0x%X)\r\n", status);
            }
        }

        /* 10ms sampling tick */
        SDK_DelayAtLeastUs(SAMPLE_INTERVAL_MS * 1000, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    }
}
