/*
 * LPSPI host-callback bridge for NXP's generic fsl_sdspi.c driver.
 * Target: FRDM-MCXN236, LPSPI3 (SCK=P1_13, MOSI=P1_12, MISO=P1_14),
 * manual chip select on P1_17.
 *
 * SDK_OS_BAREMETAL and FSL_FEATURE_SDSPI_CARD_DETECTION_BY_GPIO are set in
 * CMakeLists.txt so they also reach fsl_sdspi.c.
 */

#include <stdbool.h>
#include <string.h>
#include "lpspi_sdspi_host.h"
#include "fsl_lpspi.h"
#include "fsl_gpio.h"
#include "fsl_port.h"
#include "fsl_clock.h"
#include "fsl_lpflexcomm.h"
#include "fsl_debug_console.h"

#define SD_CS_PORT_PIN 17U
#define SD_INIT_BAUD   400000U /* SD cards must be initialised at <= 400 kHz */

sdspi_card_t g_card;
static sdspi_host_t s_host;

/*******************************************************************************
 * Chip select: manual GPIO on P1_17 (LPSPI3 hardware PCS is not used)
 ******************************************************************************/
static void CS_Assert(void)
{
    GPIO_PortClear(GPIO1, 1u << SD_CS_PORT_PIN);
}

static void CS_Deassert(void)
{
    GPIO_PortSet(GPIO1, 1u << SD_CS_PORT_PIN);
}

static void spi_csActivePolarity(sdspi_cs_active_polarity_t polarity)
{
    if (polarity == kSDSPI_CsActivePolarityLow)
    {
        CS_Assert();
    }
    else
    {
        CS_Deassert();
    }
}

/*******************************************************************************
 * LPSPI3 bring-up
 ******************************************************************************/
static void spi_init(void)
{
    CLOCK_AttachClk(kFRO12M_to_FLEXCOMM3);
    CLOCK_SetClkDiv(kCLOCK_DivFlexcom3Clk, 1u);
    CLOCK_EnableClock(kCLOCK_Port1);

    /* MOSI/SCK/MISO on P1_12/P1_13/P1_14: FC3_P0/P1/P2, all ALT3 */
    const port_pin_config_t spiPinConfig = {
        kPORT_PullUp, kPORT_LowPullResistor, kPORT_FastSlewRate,
        kPORT_PassiveFilterDisable, kPORT_OpenDrainDisable, kPORT_LowDriveStrength,
        kPORT_MuxAlt3, kPORT_InputBufferEnable, kPORT_InputNormal, kPORT_UnlockRegister};

    PORT_SetPinConfig(PORT1, 12U, &spiPinConfig); /* MOSI, FC3_P0 */
    PORT_SetPinConfig(PORT1, 13U, &spiPinConfig); /* SCK,  FC3_P1 */
    PORT_SetPinConfig(PORT1, 14U, &spiPinConfig); /* MISO, FC3_P2 */

    /* Manual chip select, idle high (deselected) */
    const port_pin_config_t csPinConfig = {
        kPORT_PullUp, kPORT_LowPullResistor, kPORT_FastSlewRate,
        kPORT_PassiveFilterDisable, kPORT_OpenDrainDisable, kPORT_LowDriveStrength,
        kPORT_MuxAsGpio, kPORT_InputBufferEnable, kPORT_InputNormal, kPORT_UnlockRegister};
    PORT_SetPinConfig(PORT1, SD_CS_PORT_PIN, &csPinConfig);

    gpio_pin_config_t csConfig = {kGPIO_DigitalOutput, 1};
    GPIO_PinInit(GPIO1, SD_CS_PORT_PIN, &csConfig);

    /* LP_FLEXCOMM must be initialised (by instance number) before LPSPI */
    status_t fcStatus = LP_FLEXCOMM_Init(3U, LP_FLEXCOMM_PERIPH_LPSPI);
    if (fcStatus != kStatus_Success)
    {
        PRINTF("spi_init: LP_FLEXCOMM_Init failed, status=%d\r\n", (int)fcStatus);
    }

    uint32_t srcClock = CLOCK_GetLPFlexCommClkFreq(3U);
    if (srcClock == 0U)
    {
        PRINTF("spi_init: FC3 source clock is 0 Hz, check clock attach\r\n");
    }

    lpspi_master_config_t masterConfig;
    LPSPI_MasterGetDefaultConfig(&masterConfig);
    masterConfig.baudRate = SD_INIT_BAUD;
    LPSPI_MasterInit(LPSPI3, &masterConfig, srcClock);
}

static status_t spi_set_frequency(uint32_t frequency)
{
    uint32_t srcClock = CLOCK_GetLPFlexCommClkFreq(3U);
    if (srcClock == 0U)
    {
        PRINTF("spi_set_frequency: FC3 source clock is 0 Hz\r\n");
        return kStatus_Fail;
    }

    uint32_t tcrPrescaleValue;

    LPSPI_Enable(LPSPI3, false);
    uint32_t actual = LPSPI_MasterSetBaudRate(LPSPI3, frequency, srcClock, &tcrPrescaleValue);
    LPSPI_Enable(LPSPI3, true);

    return (actual == 0U) ? kStatus_Fail : kStatus_Success;
}

/* Full-duplex transfer. in == NULL means "clock out 0xFF" (read-only phase);
   out == NULL means "discard received bytes". */
static status_t spi_exchange(uint8_t *in, uint8_t *out, uint32_t size)
{
    static uint8_t dummyTx[520]; /* 512-byte block + 2 CRC bytes, with margin */
    static bool dummyTxReady = false;

    if (!dummyTxReady)
    {
        memset(dummyTx, 0xFF, sizeof(dummyTx));
        dummyTxReady = true;
    }

    lpspi_transfer_t xfer = {0};
    xfer.txData      = (in != NULL) ? in : dummyTx;
    xfer.rxData      = out;
    xfer.dataSize    = size;
    xfer.configFlags = kLPSPI_MasterPcsContinuous;

    status_t result = LPSPI_MasterTransferBlocking(LPSPI3, &xfer);
    return (result != kStatus_Success) ? kStatus_Fail : kStatus_Success;
}

/*******************************************************************************
 * Host registration: call once before SDSPI_Init()
 ******************************************************************************/
void sdspi_host_init(void)
{
    s_host.busBaudRate      = SD_INIT_BAUD;
    s_host.setFrequency     = spi_set_frequency;
    s_host.exchange         = spi_exchange;
    s_host.init             = spi_init;
    s_host.csActivePolarity = spi_csActivePolarity;

    g_card.host = &s_host;
}