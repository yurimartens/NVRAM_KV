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
#define NVR_OPEN_FLAGS_BINARY_SEARCH                    (1 << 1)     
#define NVR_OPEN_FLAGS_FIRST_MATCH                      (1 << 2)     
#define NVR_OPEN_FLAGS_NEAREST                          (1 << 3)         
#define NVR_OPEN_FLAGS_BACKWARD                         (1 << 4) 
#define NVR_OPEN_FLAGS_ANY_ID                           (1 << 5)
#define NVR_OPEN_FLAGS_SCAN_FILES                       (NVR_OPEN_FLAGS_FROM_CURRENT_POS | NVR_OPEN_FLAGS_ANY_ID | NVR_OPEN_FLAGS_FIRST_MATCH)    
#define NVR_OPEN_FLAGS_READ_SEQUENCE                    (NVR_OPEN_FLAGS_FROM_CURRENT_POS | NVR_OPEN_FLAGS_ANY_ID | NVR_OPEN_FLAGS_FIRST_MATCH)        
    
    
    

typedef enum {
    NVR_ERROR_NONE = 0,
    NVR_ERROR_INIT = -1,
    NVR_ERROR_HW = -2,
    NVR_ERROR_BUSY = -3,
    NVR_ERROR_HEADER = -4,
    NVR_ERROR_EMPTY = -5,
    NVR_ERROR_END_MEM = -6,
    NVR_ERROR_NOT_FOUND = -7,
    NVR_ERROR_ARGUMENT = -8,
    NVR_ERROR_CRC = -9,        
    NVR_ERROR_OPENED = 1,
} NVRError_t;


typedef int32_t (*NVRReadData_t)(uint32_t addr, uint8_t *data, uint32_t size);
typedef int32_t (*NVRWriteData_t)(uint32_t addr, uint8_t *data, uint32_t size);
typedef int32_t (*NVREraseSector_t)(uint32_t addr); 


typedef struct NVRamKV {
    uint32_t                    PageSize;
    uint32_t                    SectorSize;  
    uint32_t                    MemoryStartAddr;  
    uint32_t                    MemorySize;  
    uint8_t                     *Page;

    NVRReadData_t               NVRReadDataLL;
    NVRWriteData_t              NVRWriteDataLL;
    NVREraseSector_t            NVREraseSectorLL;
        
    uint64_t                    LastFileId;
    uint32_t                    LastFileAddr, LastFileSize; // relative addr
    uint32_t                    FoundFileAddr, FoundFileSize;       // relative addr    
    uint32_t                    Flags;
    uint32_t                    CRC32Temp;
    
    uint8_t                     FileFound;    
    uint8_t                     NotReady;
    uint8_t                     TryToOpen;
} NVRamKV_t;   




NVRError_t NVRInit(NVRamKV_t *nvr, uint32_t pageSize, uint32_t sectorSize, uint32_t startAddr, uint32_t memSize, uint8_t *page, uint32_t flags);
NVRError_t NVRInitLL(NVRamKV_t *nvr, NVRReadData_t nvrRead, NVRWriteData_t nvrWrite, NVREraseSector_t nvrErase);
NVRError_t NVROpenFile(NVRamKV_t *nvr, uint64_t id, uint32_t *size, uint32_t flags, uint32_t emptyPagesLim);
uint32_t   NVRGetNextAddr(NVRamKV_t *nvr);
void       NVRMoveToStart(NVRamKV_t *nvr);
NVRError_t NVRMoveToNextFile(NVRamKV_t *nvr);
uint64_t   NVRGetLastId(NVRamKV_t *nvr);
NVRError_t NVRReadFile(NVRamKV_t *nvr, uint32_t pos, uint8_t *data, uint32_t size);
NVRError_t NVRWriteFile(NVRamKV_t *nvr, uint64_t id, uint8_t *data, uint32_t size);
NVRError_t NVRCloseFile(NVRamKV_t *nvr, uint64_t id);
NVRError_t NVREraseAll(NVRamKV_t *nvr);


extern const uint32_t           NVRHeaderSize;

#ifdef __cplusplus
}
#endif

#endif // _NVRAM_KV_H
//------------------------------------------------------------------------------
// END
//------------------------------------------------------------------------------
