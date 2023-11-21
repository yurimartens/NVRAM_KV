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
#define HEADER_CHECK_FIELD_LEN  24              // to cover preamble, fileId and DataSize


static uint32_t                 PageSize;
static uint32_t                 SectorSize;  
static uint32_t                 MemoryStartAddr;  
static uint32_t                 MemorySize;  
static uint8_t                  *Page;

static NVRReadData_t            NVRReadData;
static NVRWriteData_t           NVRWriteData;
static NVREraseSector_t         NVREraseSector;
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

static uint8_t                  TryToOpen = 0;
static uint32_t                 FoundFileId, FoundFileAddr, FoundFileSize;
static uint8_t                  FileFound = 0;



static NVRError_t NVRCheckHeader(uint32_t addr, uint32_t *currAddr, uint32_t *currId, uint32_t *currSize);


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
NVRError_t NVRInitCB(NVRReadData_t nvrRead, NVRWriteData_t nvrWrite, NVREraseSector_t nvrErase)
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
NVRError_t NVROpenFile(uint32_t id, uint32_t *size, uint32_t flags)
{
    if (NotReady) return NVR_ERROR_INIT;
    if (size == 0) return NVR_ERROR_ARGUMENT;
    
    uint32_t start = MemoryStartAddr;
    uint32_t end = MemoryStartAddr + MemorySize;    
    NVRError_t ret;
    
    TryToOpen = 1;
    FileFound = FoundFileId = FoundFileAddr = FoundFileSize = 0;
    while (start < end) {
        switch (ret = NVRCheckHeader(start, &FoundFileAddr, &FoundFileId, &FoundFileSize)) {
            case NVR_ERROR_NONE:
                if (id == FoundFileId) FileFound = 1;                
                start = FoundFileAddr + FoundFileSize;  // next addr to scan
            break;
            case NVR_ERROR_HEADER:
                start += PageSize - HEADER_CHECK_FIELD_LEN; // not to miss start of a header
            break;            
            default:
                return ret;
            break;
        }
    }
    if (FileFound) {
        *size = FoundFileSize;
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
    if ((FileFound == 0) && (TryToOpen == 0)) return NVR_ERROR_NOT_FOUND;
    if ((FoundFileAddr + pos + size > FoundFileAddr + FoundFileSize) || (data == 0)) return NVR_ERROR_ARGUMENT;
    
    NVRError_t ret = NVR_ERROR_NONE;
    uint32_t chunkSize = 0;
    uint32_t remain = size, offset = 0;
    uint32_t stop = 0;
    do {
        if (remain > PageSize) {
            chunkSize = PageSize;
            remain -= PageSize;
        } else {
            chunkSize = remain;
            stop = 1;
        }
        if (0 == NVRReadData(FoundFileAddr + pos + offset, &data[offset], chunkSize)) {
            offset += chunkSize;
        } else {
            ret = NVR_ERROR_HW;
            stop = 1;
        }
    } while (stop == 0);  
    
    return ret;
}

/**
  * @brief
  * @param
  * @retval
  */
NVRError_t NVRWriteFile(uint32_t id, uint32_t pos, uint32_t size, uint8_t *data)
{
    if (NotReady) return NVR_ERROR_INIT;
    if (TryToOpen == 0) return NVR_ERROR_NOT_FOUND;
    if (data == 0) return NVR_ERROR_ARGUMENT;
    
    NVRError_t ret = NVR_ERROR_NONE;
    uint32_t addr = FoundFileAddr + pos;
    uint32_t chunkSize = 0, sectorChunkSize;
    uint32_t remain = size, offset = 0;
    uint32_t stop = 0, stopS = 0;
    uint32_t finishSector = (addr % SectorSize) ? 1 : 0;
    uint32_t sectorRemain = SectorSize - (addr % SectorSize);
    uint32_t pageRemain = PageSize - (addr % PageSize);
    
    do {
        if (remain > sectorRemain) {
            sectorChunkSize = sectorRemain;
            remain -= sectorRemain;
            sectorRemain = SectorSize;
        } else {
            sectorChunkSize = sectorRemain;
            stop = 1;
        }
        if (sectorChunkSize > remain) sectorChunkSize = remain;
        
        if (finishSector == 0) {
            NVREraseSector(addr);
        } else {
            finishSector = 1;
        }
        
        do {
            if (sectorChunkSize > pageRemain) {
                chunkSize = pageRemain;
                sectorChunkSize -= pageRemain;
                pageRemain = PageSize;
            } else {
                chunkSize = sectorChunkSize;
                stopS = 1;
            }            
            if (0 == NVRWriteData(addr, &data[offset], chunkSize)) {
                offset += chunkSize;
                addr += chunkSize;
            } else {
                ret = NVR_ERROR_HW;
                stop = stopS = 1;
            }
        } while (stopS == 0);        
    } while (stop == 0);    
    
    return ret;
}

/**
  * @brief
  * @param
  * @retval
  */
NVRError_t NVRCloseFile(uint32_t id)
{
    if (NotReady) return NVR_ERROR_INIT;
    TryToOpen = FileFound = 0;
    return NVR_ERROR_NONE;
}

/* Private functions ---------------------------------------------------------*/    

/**
  * @brief
  * @param
  * @retval
  */
static NVRError_t NVRCheckHeader(uint32_t addr, uint32_t *currAddr, uint32_t *currId, uint32_t *currSize)
{    
    NVRError_t ret = NVR_ERROR_NONE;
    
    uint32_t a = addr + 8, chunkSize = PageSize, remain = 0;          
    uint32_t stop = 0;
    uint32_t offset = 0;
    
    if (0 == NVRReadData(addr, Page, PageSize)) {
        remain = PageSize;
    } else {
        ret = NVR_ERROR_HW;
        stop = 1;    
    }
    
    while (stop == 0) {
        if (offset > PageSize - HEADER_CHECK_FIELD_LEN) {
            ret = NVR_ERROR_HEADER;
            break;
        }
        NVRHeader_t *h = (NVRHeader_t *)&Page[offset];         
        if ((h->Preamble == PREAMBLE) && (h->FileId == ~h->FileIdInv) && (h->DataSize != 0)) {
            uint32_t fileSize = HeaderSize + h->FileNameSize + h->DataSize;
            uint32_t crc = 0;
            
            if (fileSize > remain) {
                do {
                    if (remain > PageSize) {
                        chunkSize = PageSize;
                        remain -= PageSize;
                    } else {
                        chunkSize = remain;
                        stop = 1;
                    }
                        if (0 == NVRReadData(a, Page, chunkSize)) {
                            crc = CalcCRC32(Page, chunkSize, crc);
                            if (stop) {
                                if (h->CRC32 == crc) {
                                    *currAddr = addr + offset;
                                    *currId = h->FileId;                                    
                                    *currSize = fileSize + 8;                                    
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
                    *currAddr = addr + offset;
                    *currId = h->FileId;                                    
                    *currSize = fileSize + 8;                       
                } else {
                    ret = NVR_ERROR_CRC;
                }
            }                        
        } else {
            ret = NVR_ERROR_HEADER;
            offset++;
            //remain--;
        }        
    }
    
    
    
    
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
                        chunkSize = PageSize;
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
                                //*nextAddr = addr + 8 + fileSize;  // next addr for scan                          
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


