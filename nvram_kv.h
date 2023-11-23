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
    


typedef enum {
    NVR_ERROR_NONE = 0,
    NVR_ERROR_INIT = -1,
    NVR_ERROR_HW = -2,
    NVR_ERROR_BUSY = -3,
    NVR_ERROR_HEADER = -4,
    NVR_ERROR_END_MEM = -5,
    NVR_ERROR_NOT_FOUND = -6,
    NVR_ERROR_ARGUMENT = -7,
        
    NVR_ERROR_OPENED = 1,
} NVRError_t;


typedef int32_t (*NVRReadData_t)(uint32_t addr, uint8_t *data, uint32_t size);
typedef int32_t (*NVRWriteData_t)(uint32_t addr, uint8_t *data, uint32_t size);
typedef int32_t (*NVREraseSector_t)(uint32_t addr); 


NVRError_t NVRInit(uint32_t pageSize, uint32_t sectorSize, uint32_t startAddr, uint32_t memSize, uint8_t *page);
NVRError_t NVRInitCB(NVRReadData_t nvrRead, NVRWriteData_t nvrWrite, NVREraseSector_t nvrErase);
NVRError_t NVROpenFile(uint32_t id, uint32_t *size);
NVRError_t NVRReadFile(uint32_t id, uint32_t pos, uint32_t size, uint8_t *data);
NVRError_t NVRWriteFile(uint32_t id, uint32_t pos, uint32_t partSize, uint8_t *data, uint32_t wholeSize);
NVRError_t NVRCloseFile(uint32_t id);




#ifdef __cplusplus
}
#endif

#endif // _NVRAM_KV_H
//------------------------------------------------------------------------------
// END
//------------------------------------------------------------------------------
