/**
  ******************************************************************************
  * @file    nvram_kv.c
  * @author  Yuri Martenstev <yurimartens@gmail.com>
  * @brief
  ******************************************************************************
  */

#include "nvram_kv.h"
#include <string.h>
#include "crc32.h"




#define PREAMBLE                0x1FACADE1


static uint32_t                 PageSize;
static uint32_t                 SectorSize;  
static uint32_t                 MemoryStartAddr;  
static uint32_t                 MemorySize;  
static uint8_t                  *Page;

static NVRReadData_cb           NVRReadData;
static NVRWriteData_cb          NVRWriteData;
static NVREraseSector_cb        NVREraseSector;
static uint8_t                  NotReady = 2;



typedef struct {
	uint32_t                    Preamble;
    uint32_t                    CRC32;
    uint32_t                    FileId;
    uint32_t                    FileIdInv;    
    uint32_t                    Reserved0;
	uint32_t                    DataSize;
    uint32_t                    WriteCounter;
    uint32_t                    TimeStamp;
    uint32_t                    State;
    uint32_t                    FileNameSize;
    uint32_t                    Reserved1;      // prev
    uint32_t                    Reserved2;      // next;
} NVRHeader_t;

const uint32_t                  HeaderSize = sizeof(NVRHeader_t);


static uint32_t                 OpenedFileAddr;



static NVRError_t NVRCheckHeader(uint32_t addr, uint32_t *currId, uint32_t *currSize, uint32_t *nextAddr);


/**
  * @brief
  * @param
  * @retval
  */
NVRError_t NVRInit(uint32_t pageSize, uint32_t sectorSize, uint32_t startAddr, uint32_t memSize, uint8_t *page)
{
    if ((pageSize == 0) || (sectorSize == 0) || (memSize == 0) || (sectorSize < pageSize) || (page == 0)) return NVR_ERROR_INIT;
    PageSize = pageSize;
    SectorSize = sectorSize;
    MemoryStartAddr = startAddr;
    MemorySize = memSize;
       
    NotReady--;
    
    return NVR_ERROR_NONE;
}

/**
  * @brief
  * @param
  * @retval
  */
NVRError_t NVRInitCB(NVRReadData_cb nvrRead, NVRWriteData_cb nvrWrite, NVREraseSector_cb nvrErase)
{
    if ((nvrRead == 0) || (nvrWrite == 0) || (nvrErase == 0)) return NVR_ERROR_INIT;
    NVRReadData = nvrRead;
    NVRWriteData = nvrWrite;
    NVREraseSector = nvrErase;
       
    NotReady--;
    
    return NVR_ERROR_NONE;
}

/**
  * @brief
  * @param
  * @retval
  */
NVRError_t NVROpenFile(uint32_t id, uint32_t *size)
{
    if (NotReady) return NVR_ERROR_INIT;
    uint32_t start = MemoryStartAddr;
    uint32_t end = MemoryStartAddr + MemorySize;
    uint32_t fileFound = 0, tempAddr = 0, tempSize, tempId;
    NVRError_t ret;
    while (start < end) {
        switch (ret = NVRCheckHeader(start, &tempId, &tempSize, &tempAddr)) {
            case NVR_ERROR_NONE:
                if (id == tempId) fileFound = 1;
            break;
            case NVR_ERROR_HEADER:
                start++;
            break;            
            default:
                return ret;
            break;
        }
    }
    if (fileFound) {
        OpenedFileAddr = tempAddr;
        *size = tempSize;
        ret = NVR_ERROR_OPENED;
    } else {
        ret = NVR_ERROR_NOT_FOUND;
    }
    return ret;
}

/**
  * @brief
  * @param
  * @retval
  */
NVRError_t NVRReadFile(uint32_t id, uint32_t pos, uint32_t size, uint8_t *data)
{
    if (NotReady) return NVR_ERROR_INIT;
}

/**
  * @brief
  * @param
  * @retval
  */
NVRError_t NVRWriteFile(uint32_t id, uint32_t pos, uint32_t size, uint8_t *data)
{
    if (NotReady) return NVR_ERROR_INIT;
}

/**
  * @brief
  * @param
  * @retval
  */
NVRError_t NVRCloseFile(uint32_t id)
{
    if (NotReady) return NVR_ERROR_INIT;
}
    
/**
  * @brief
  * @param
  * @retval
  */
static NVRError_t NVRCheckHeader(uint32_t addr, uint32_t *currId, uint32_t *currSize, uint32_t *nextAddr)
{    
    NVRError_t ret = NVR_ERROR_NONE;
    
    uint32_t a = addr + 8, chunkSize = PageSize, remain = 0;          
    uint32_t stop = 0;
    uint32_t offset = 0;
    do {
        if (0 == remain) {
            if (0 == NVRReadData(a, Page, PageSize)) {
                remain = PageSize;
            } else {
                ret = NVR_ERROR_HW;
                stop = 1;
            }   
        }
        NVRHeader_t *h = (NVRHeader_t *)&Page[offset];         
        if ((h->Preamble == PREAMBLE) && (h->FileId == ~h->FileIdInv) && (h->DataSize != 0)) {
            uint32_t fileSize = HeaderSize + h->FileNameSize + h->DataSize;
            uint32_t crc = 0;
            
            if (fileSize > remain) {
                do {
                    if (remain > PageSize) {
                        chunkSize = remain - PageSize;
                        remain -= PageSize;
                    } else {
                        chunkSize = remain;
                        stop = 1;
                    }
                        if (0 == NVRReadData(a, Page, chunkSize)) {
                            crc = CalcCRC32(Page, chunkSize, crc);
                            if (stop) {
                                if (h->CRC32 == crc) {
                                    *currSize = fileSize + 8;
                                    *currId = h->FileId;
                                    *nextAddr = addr + 8 + fileSize;  // next addr for scan                          
                                } else {
                                    ret = NVR_ERROR_CRC;
                                }
                            } else {
                                a += chunkSize;
                            }
                        } else {
                            ret = NVR_ERROR_HW;
                            stop = 1;
                        }
                } while (stop == 0);  
            } else {
                crc = CalcCRC32(&Page[offset + 8], fileSize - 8, crc);
                if (h->CRC32 == crc) {
                    *currSize = fileSize;
                    *currId = h->FileId;
                    *nextAddr = addr + fileSize;  // next addr for scan                          
                } else {
                    ret = NVR_ERROR_CRC;
                }
            }
            
            
            
        } else {
            ret = NVR_ERROR_HEADER;
            offset++;
            remain--;
        }        
    } while (remain);
    
    
    
    
    if (0 == NVRReadData(addr, Page, PageSize)) {  
        uint32_t offset = 0;
        while (offset < PageSize - 24) {  // 24 - to cover preamble, fileId and DataSize
            NVRHeader_t *h = (NVRHeader_t *)&Page[offset];         
            if ((h->Preamble == PREAMBLE) && (h->FileId == ~h->FileIdInv) && (h->DataSize != 0)) {
                uint32_t fileSize = HeaderSize + h->FileNameSize + h->DataSize;// - 8; // 8 - Preamble + CRC32            
                uint32_t crc = 0;
                uint32_t stop = 0;
                uint32_t a = addr + 8, chunkSize, remain;                
                
                do {
                    if (remain > PageSize) {
                        chunkSize = remain - PageSize;
                        remain -= PageSize;
                    } else {
                        chunkSize = remain;
                        stop = 1;
                    }
                    if (0 == NVRReadData(a, Page, chunkSize)) {
                        crc = CalcCRC32(Page, chunkSize, crc);
                        if (stop) {
                            if (h->CRC32 == crc) {
                                *currSize = fileSize + 8;
                                *currId = h->FileId;
                                *nextAddr = addr + 8 + fileSize;  // next addr for scan                          
                            } else {
                                ret = NVR_ERROR_CRC;
                            }
                        } else {
                            a += chunkSize;
                        }
                    } else {
                        ret = NVR_ERROR_HW;
                        stop = 1;
                    }
                } while (stop == 0);               
            } else {
                ret = NVR_ERROR_HEADER;
                offset++;
            }
        }
    } else {
         ret = NVR_ERROR_HW;
    }
    return ret;
}
    

//------------------------------------------------------------------------------
// END
//------------------------------------------------------------------------------


