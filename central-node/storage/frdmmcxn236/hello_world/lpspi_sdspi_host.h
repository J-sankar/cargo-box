#ifndef LPSPI_SDSPI_HOST_H
#define LPSPI_SDSPI_HOST_H

#include "fsl_sdspi.h"

/* SD card handle shared between the SDSPI host layer and the FATFS disk layer */
extern sdspi_card_t g_card;

/* Wires the SDSPI host callbacks to LPSPI3 and attaches them to g_card */
void sdspi_host_init(void);

#endif /* LPSPI_SDSPI_HOST_H */