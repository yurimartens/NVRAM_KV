/**
  ******************************************************************************
  * @file    nvram_kv.h
  * @author  Yuri Martentsev <yurimartens@gmail.com>, <yuri.martens@yandex.ru>
  * @brief
  ******************************************************************************
  */
#ifndef _NVRAM_KV_H
#define _NVRAM_KV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>


   


#define NVR_FLAGS_PAGE_ALIGN                            (1 << 0) 
    

#define NVR_OPEN_FLAGS_FROM_CURRENT_POS                 (1 << 0) 
#define NVR_OPEN_FLAGS_FIRST_MATCH                      (1 << 1)     
#define NVR_OPEN_FLAGS_BACKWARD                         (1 << 2)         
    

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

struct NVRamKV;
typedef struct NVRamKV *NVRamKV_t;



NVRError_t NVRInit(NVRamKV_t nvr, uint32_t pageSize, uint32_t sectorSize, uint32_t startAddr, uint32_t memSize, uint8_t *page, uint32_t flags);
NVRError_t NVRInitLL(NVRamKV_t nvr, NVRReadData_t nvrRead, NVRWriteData_t nvrWrite, NVREraseSector_t nvrErase);
NVRError_t NVROpenFile(NVRamKV_t nvr, uint32_t id, uint32_t *size, uint32_t flags);
NVRError_t NVRSearchForLastFile(NVRamKV_t nvr, uint32_t *lastId, uint32_t *nextAddr);
uint32_t   NVRGetNextAddr(NVRamKV_t nvr);
uint32_t   NVRGetLastId(NVRamKV_t nvr);
NVRError_t NVRReadFile(NVRamKV_t nvr, uint32_t id, uint32_t pos, uint8_t *data, uint32_t size);
NVRError_t NVRWriteFile(NVRamKV_t nvr, uint32_t id, uint8_t *data, uint32_t size);
NVRError_t NVRWriteFilePart(NVRamKV_t nvr, uint32_t id, uint32_t pos, uint8_t *data, uint32_t partSize, uint32_t fullSize);
NVRError_t NVRCloseFile(NVRamKV_t nvr, uint32_t id);
NVRError_t NVREraseAll(NVRamKV_t nvr);




#ifdef __cplusplus
}
#endif

#endif // _NVRAM_KV_H
//------------------------------------------------------------------------------
// END
//------------------------------------------------------------------------------
