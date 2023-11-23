/**
  ******************************************************************************
  * @file    nvram_kv.c
  * @author  Yuri Martenstev <yurimartens@gmail.com>
  * @brief
  ******************************************************************************
  */

#include "nvram_kv.h"
#include <string.h>




#define PREAMBLE                0x1FACADE1


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
    uint32_t                    FileId;
    uint32_t                    FileIdInv;    
	uint32_t                    DataSize;
    uint32_t                    DataSizeInv;
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
    Page = page;
    
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
  * @brief      todo: BST
  * @param
  * @retval
  */
NVRError_t NVROpenFile(uint32_t id, uint32_t *size)
{
    if (NotReady) return NVR_ERROR_INIT;
    if (size == 0) return NVR_ERROR_ARGUMENT;
    
    uint32_t start = MemoryStartAddr;
    uint32_t end = MemoryStartAddr + MemorySize;    
    NVRError_t ret;
    uint32_t tempFileId = 0, tempFileAddr = 0, tempFileSize = 0;
    
    TryToOpen = 1;
    FileFound = FoundFileId = FoundFileAddr = FoundFileSize = 0;
    while (start < end) {
        switch (ret = NVRCheckHeader(start, &tempFileAddr, &tempFileId, &tempFileSize)) {
            case NVR_ERROR_NONE:
                start = tempFileAddr + tempFileSize;  // next addr to scan
                tempFileAddr += HeaderSize;
                tempFileSize -= HeaderSize;
                if (id == tempFileId) {
                    FileFound = 1; 
                    FoundFileId = tempFileId;
                    FoundFileAddr = tempFileAddr;
                    FoundFileSize = tempFileSize;
                }                
            break;
            case NVR_ERROR_HEADER:
                start += PageSize - HeaderSize; // not to miss start of a header
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
        FoundFileId = tempFileId;
        FoundFileAddr = tempFileAddr;
        FoundFileSize = tempFileSize;
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
    uint32_t addr = FoundFileAddr + pos; 
    if ((addr + size > FoundFileAddr + FoundFileSize) || (data == 0)) return NVR_ERROR_ARGUMENT;
    
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
        if (0 == NVRReadData(addr + offset, &data[offset], chunkSize)) {
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
static NVRError_t NVRWrite(uint32_t addr, uint8_t *data, uint32_t size)
{
    if (NotReady) return NVR_ERROR_INIT;
    if (TryToOpen == 0) return NVR_ERROR_NOT_FOUND;
    if (data == 0) return NVR_ERROR_ARGUMENT;
    
    NVRError_t ret = NVR_ERROR_NONE;
    uint32_t chunkSize = 0, sectorChunkSize;
    uint32_t remain = size, offset = 0;
    uint32_t stop = 0, stopS = 0;
    uint32_t finishSector = (addr % SectorSize) ? 1 : 0;
    uint32_t sectorRemain = SectorSize - (addr % SectorSize);
    uint32_t pageRemain = PageSize - (addr % PageSize);
    uint32_t endMem = MemoryStartAddr + MemorySize;
       
    do {
        if (addr == endMem) addr = MemoryStartAddr;
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
NVRError_t NVRWriteFile(uint32_t id, uint32_t pos, uint32_t partSize, uint8_t *data, uint32_t fullSize)
{
    if (NotReady) return NVR_ERROR_INIT;
    if (TryToOpen == 0) return NVR_ERROR_NOT_FOUND;
    if (data == 0) return NVR_ERROR_ARGUMENT;
    
    NVRError_t ret = NVR_ERROR_NONE;
    uint32_t addr = (FoundFileAddr == 0) ? FoundFileAddr : FoundFileAddr + FoundFileSize;
    
    if (pos == 0) { // first part
        if ((addr + HeaderSize) > (MemoryStartAddr + MemorySize)) addr = MemoryStartAddr;
        NVRHeader_t *h = (NVRHeader_t *)Page;    
        memset(Page, 0, HeaderSize);
        h->Preamble = PREAMBLE;
        h->FileId = id;
        h->FileIdInv = ~id;
        h->DataSize = fullSize;  
        h->DataSizeInv = ~fullSize;  
        
        if (0 != (ret = NVRWrite(addr, Page, HeaderSize))) return ret;
        pos += HeaderSize;
    }    
    return NVRWrite(addr + pos, data, partSize);   
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
    NVRError_t ret = NVR_ERROR_HEADER;
    
    uint32_t offset = 0;
    uint32_t endMem = MemoryStartAddr + MemorySize;
    if ((addr + HeaderSize) > endMem) return NVR_ERROR_END_MEM;
    
    uint32_t bytesToRead = (endMem - addr > PageSize) ? PageSize : endMem - addr;
    
    if (0 != NVRReadData(addr, Page, bytesToRead)) {
        return NVR_ERROR_HW;
    }    
    while (offset < bytesToRead - HeaderSize) {        
        NVRHeader_t *h = (NVRHeader_t *)&Page[offset];         
        if ((h->Preamble == PREAMBLE) && (h->FileId == ~h->FileIdInv) && (h->DataSize != 0) && (h->DataSize == ~h->DataSizeInv)) {
            *currAddr = addr + offset;
            *currId = h->FileId;                                    
            *currSize = HeaderSize + h->DataSize; 
            ret = NVR_ERROR_NONE;    
            break;
        } else {
            offset++;
        }        
    }   
    return ret;
}
    

//------------------------------------------------------------------------------
// END
//------------------------------------------------------------------------------


