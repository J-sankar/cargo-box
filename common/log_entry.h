#ifndef LOG_ENTRY_H
#define LOG_ENTRY_H

#include <stdint.h>

typedef struct {
    uint32_t can_id;
    uint8_t  data[8];
    uint8_t  dlc;        // data length (bytes actually used)
    uint32_t timestamp;
} LogEntry_t;

#endif // LOG_ENTRY_H