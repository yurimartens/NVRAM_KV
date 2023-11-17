/**
  ******************************************************************************
  * @file    nvram_kv.h
  * @author  Yuri Martenstev <yurimartens@gmail.com>
  * @brief
  ******************************************************************************
  */
#ifndef _NVRAM_KV_H
#define _NVRAM_KV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>


#include "gd25qxx.h"
    
    
#define PREAMBLE                0x1FACADE1




typedef struct {
	uint32_t                    Preamble;
    uint32_t                    ItemId;
    uint32_t                    ItemIdInv;
    uint32_t                    CRC32;
    uint32_t                    Reserved;
	uint32_t                    DataSize;
    uint32_t                    WriteCounter;
    uint32_t                    TimeStamp;
    uint32_t                    State;
    uint32_t                    NameSize;
    uint32_t                    ItemAddrPrev;
    uint32_t                    ItemAddrNext;
} NVRHeader_t;



void NVRInit(void);
int32_t NVRSearchItem(uint32_t id, uint32_t *addr, uint32_t *size);
int32_t NVRReadItem(uint32_t id, uint32_t addr, uint32_t size, uint8_t *data);
int32_t NVRWriteItem(uint32_t id, uint32_t addr, uint32_t size, uint8_t *data);




#ifdef __cplusplus
}
#endif

#endif // _NVRAM_KV_H
//------------------------------------------------------------------------------
// END
//------------------------------------------------------------------------------
