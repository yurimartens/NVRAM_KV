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

static NVRReadData_t            NVRReadDataLL;
static NVRWriteData_t           NVRWriteDataLL;
static NVREraseSector_t         NVREraseSectorLL;
static uint8_t                  NotReady = 2;



typedef struct {
	uint32_t                    Preamble;       
    uint32_t                    FileId;
    uint32_t                    FileIdInv;    
	uint32_t                    DataSize;
    uint32_t                    DataSizeInv;
} NVRHeader_t;

const uint32_t                  HeaderSize = sizeof(NVRHeader_t);

static uint8_t                  TryToOpen = 0, Flags = 0;
static uint32_t                 FoundFileAddr, FoundFileSize;
static uint32_t                 LastFileId = 0, LastFileAddr = 0, LastFileSize = 0;
static uint8_t                  FileFound = 0;




static NVRError_t NVRCheckHeader(uint32_t addr, uint32_t *currAddr, uint32_t *currId, uint32_t *currSize);
static NVRError_t NVRWrite(uint32_t addr, uint8_t *data, uint32_t size, uint32_t flags);


/**
  * @brief
  * @param
  * @retval
  */
NVRError_t NVRInit(uint32_t pageSize, uint32_t sectorSize, uint32_t startAddr, uint32_t memSize, uint8_t *page, uint32_t flags)
{
    if ((pageSize == 0) || (sectorSize == 0) || (memSize == 0) || (sectorSize < pageSize) || (page == 0)) return NVR_ERROR_INIT;
    PageSize = pageSize;
    SectorSize = sectorSize;
    MemoryStartAddr = startAddr;
    MemorySize = memSize;
    Page = page;
    Flags = flags;
    
    NotReady--;
    
    return NVR_ERROR_NONE;
}

/**
  * @brief
  * @param
  * @retval
  */
NVRError_t NVRInitLL(NVRReadData_t nvrRead, NVRWriteData_t nvrWrite, NVREraseSector_t nvrErase)
{
    if ((nvrRead == 0) || (nvrWrite == 0) || (nvrErase == 0)) return NVR_ERROR_INIT;
    NVRReadDataLL = nvrRead;
    NVRWriteDataLL = nvrWrite;
    NVREraseSectorLL = nvrErase;
       
    NotReady--;
    
    return NVR_ERROR_NONE;
}

/**
  * @brief      todo: BST
  * @param
  * @retval
  */
NVRError_t NVROpenFile(uint32_t id, uint32_t *size, uint32_t flags)
{
    if (NotReady) return NVR_ERROR_INIT;
    if (size == 0) return NVR_ERROR_ARGUMENT;   // check pointer
    
    NVRError_t ret = NVR_ERROR_NONE;
    
    uint32_t start;
    uint32_t end;
    uint32_t exit = 0;
    
    *size = 0;
    
    if (((flags & NVR_OPEN_FLAGS_FROM_CURRENT_POS) == 0) || (FoundFileAddr == 0)) {
        start = MemoryStartAddr;          
    } else {
        start = MemoryStartAddr + FoundFileAddr - HeaderSize;
    }
    end = MemoryStartAddr + MemorySize; 
        
    TryToOpen = 1;
    FileFound = FoundFileAddr = FoundFileSize = LastFileId = LastFileAddr = LastFileSize = 0;
    while ((start < end) && (exit == 0)) {
        switch (ret = NVRCheckHeader(start, &LastFileAddr, &LastFileId, &LastFileSize)) {
            case NVR_ERROR_NONE:
                start = LastFileAddr + LastFileSize;  // next addr to scan
                if (Flags & NVR_FLAGS_PAGE_ALIGN) {
                   uint32_t pageFilled = start % PageSize;
                   start += PageSize - pageFilled;
                } 
                LastFileAddr += HeaderSize;
                LastFileSize -= HeaderSize;
                if (id == LastFileId) {
                    FileFound = 1; 
                    FoundFileAddr = LastFileAddr;
                    FoundFileSize = LastFileSize;
                    exit = flags & NVR_OPEN_FLAGS_FIRST_MATCH;
                }                
            break;
            case NVR_ERROR_HEADER:
                start += PageSize;
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
        FoundFileAddr = LastFileAddr;
        FoundFileSize = LastFileSize;
        ret = NVR_ERROR_NOT_FOUND;
    }
    return ret;
}

/**
  * @brief      todo: BST
  * @param
  * @retval
  */
NVRError_t NVRSearchForLastFile(uint32_t *lastId, uint32_t *nextAddr)
{
    if (NotReady) return NVR_ERROR_INIT;
    
    uint32_t start = MemoryStartAddr;
    uint32_t end = MemoryStartAddr + MemorySize; 
    NVRError_t ret = NVR_ERROR_NONE;
        
    TryToOpen = 1;
    FileFound = FoundFileAddr = FoundFileSize = LastFileId = LastFileAddr = LastFileSize = 0;
    while (start < end) {
        switch (ret = NVRCheckHeader(start, &LastFileAddr, &LastFileId, &LastFileSize)) {
            case NVR_ERROR_NONE:
                start = LastFileAddr + LastFileSize;  // next addr to scan
                if (Flags & NVR_FLAGS_PAGE_ALIGN) {
                   uint32_t pageFilled = start % PageSize;
                   start += PageSize - pageFilled;
                } 
                LastFileAddr += HeaderSize;
                LastFileSize -= HeaderSize;  
                FileFound = 1;
            break;
            case NVR_ERROR_HEADER:
                start += PageSize;
            break; 
            default:
                return ret;
            break;
        }
    }  
    if (FileFound) {
        *lastId = LastFileId;
        *nextAddr = LastFileAddr + LastFileSize;
    }
    return ret;
}
    
/**
  * @brief      
  * @param
  * @retval
  */
uint32_t NVRGetNextAddr(void)
{
    return LastFileAddr + LastFileSize;
}

/**
  * @brief      
  * @param
  * @retval
  */
uint32_t NVRGetLastId(void)
{
    return LastFileId;
}

/**
  * @brief
  * @param
  * @retval
  */
NVRError_t NVRReadFile(uint32_t id, uint32_t pos, uint8_t *data, uint32_t size)
{
    if (NotReady) return NVR_ERROR_INIT;
    if ((FileFound == 0) && (TryToOpen == 0)) return NVR_ERROR_NOT_FOUND;
    uint32_t addr = FoundFileAddr + pos; 
    uint32_t pageFilled = addr % PageSize;
    if ((addr + size > FoundFileAddr + FoundFileSize) || (data == 0) || (size == 0)) return NVR_ERROR_ARGUMENT;
    
    NVRError_t ret = NVR_ERROR_NONE;
    uint32_t chunkSize = 0;
    uint32_t remain = size, offset = 0;
    
    if ((pageFilled) && ((pageFilled + size) > PageSize)) {
        int s = PageSize - pageFilled;
        if (0 == NVRReadDataLL(addr, data, s)) {
            offset += s;
            remain -= s;
        } else {
            return NVR_ERROR_HW;            
        }
    }
    uint32_t stop = 0;
    do {
        if (remain > PageSize) {
            chunkSize = PageSize;
            remain -= PageSize;
        } else {
            chunkSize = remain;
            stop = 1;
        }
        if (0 == NVRReadDataLL(addr + offset, &data[offset], chunkSize)) {
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
NVRError_t NVRWriteFile(uint32_t id, uint8_t *data, uint32_t size)
{
    if (NotReady) return NVR_ERROR_INIT;
    if (TryToOpen == 0) return NVR_ERROR_NOT_FOUND;
    if (data == 0) return NVR_ERROR_ARGUMENT;
    
    NVRError_t ret = NVR_ERROR_NONE;
    uint32_t owf = 0;
    uint32_t addr = (LastFileAddr == 0) ? LastFileAddr : LastFileAddr + LastFileSize;
    uint32_t pageFilled = addr % PageSize;
    uint32_t pageRemain = PageSize - pageFilled;
    if ((pageFilled && (Flags & NVR_FLAGS_PAGE_ALIGN)) || (pageRemain < HeaderSize)) {     // align to the nearest start of the page if the flag is set or there is no place for the whole header
        addr += pageRemain;
        pageFilled = 0;
    }          
    if ((addr + HeaderSize) > (MemoryStartAddr + MemorySize)) {
        addr = MemoryStartAddr;
        owf = 1;
    }
    NVRHeader_t *h = (NVRHeader_t *)Page;    
    memset(Page, 0, HeaderSize);
    h->Preamble = PREAMBLE;
    h->FileId = id;
    h->FileIdInv = ~id;
    h->DataSize = size;  
    h->DataSizeInv = ~size;  
    
    FoundFileAddr = LastFileAddr = addr + HeaderSize;  // to allaw instant access to the file after writing
    FoundFileSize = LastFileSize = size;
        
    if ((pageFilled + HeaderSize + size) > PageSize) {
        int s = PageSize - pageFilled - HeaderSize;        
        memcpy(&Page[HeaderSize], data, s);    
        size -= s;
        if (0 != (ret = NVRWrite(addr, Page, PageSize - pageFilled, Flags))) return ret;
        ret = NVRWrite(addr + HeaderSize + s, &data[s], size, Flags);
        if (owf) return NVR_ERROR_END_MEM;
        else return ret;
    } else {
        memcpy(&Page[HeaderSize], data, size); 
        ret = NVRWrite(addr, Page, size + HeaderSize, Flags);
        if (owf) return NVR_ERROR_END_MEM;
        else return ret;
    }    
}

/**
  * @brief
  * @param
  * @retval
  */
NVRError_t NVRWriteFilePart(uint32_t id, uint32_t pos, uint8_t *data, uint32_t partSize, uint32_t fullSize)
{
    if (NotReady) return NVR_ERROR_INIT;
    if (TryToOpen == 0) return NVR_ERROR_NOT_FOUND;
    if ((data == 0) || (fullSize == 0) || (partSize > fullSize)) return NVR_ERROR_ARGUMENT;
    
    NVRError_t ret = NVR_ERROR_NONE;
    uint32_t addr = (LastFileAddr == 0) ? LastFileAddr : LastFileAddr + LastFileSize;
    uint32_t pageFilled = addr % PageSize;
    uint32_t pageRemain = PageSize - pageFilled;
    if ((pageFilled && (Flags & NVR_FLAGS_PAGE_ALIGN)) || (pageRemain < HeaderSize)) {     // align to the nearest start of the page if the flag is set or there is no place for the whole header
        addr += pageRemain;
        pageFilled = 0;
    }    
    
    if (pos == 0) { // first part
        if ((addr + HeaderSize) > (MemoryStartAddr + MemorySize)) addr = MemoryStartAddr;
        NVRHeader_t *h = (NVRHeader_t *)Page;    
        memset(Page, 0, HeaderSize);
        h->Preamble = PREAMBLE;
        h->FileId = id;
        h->FileIdInv = ~id;
        h->DataSize = fullSize;  
        h->DataSizeInv = ~fullSize;  
        
        FoundFileAddr = LastFileAddr = addr + HeaderSize;  // to allaw instant access to the file after writing
        FoundFileSize = LastFileSize = fullSize;
        
        if ((pageFilled + HeaderSize + partSize) > PageSize) {
            int s = PageSize - pageFilled - HeaderSize;        
            memcpy(&Page[HeaderSize], data, s);    
            partSize -= s;
            if (0 != (ret = NVRWrite(addr, Page, PageSize - pageFilled, Flags))) return ret;
            return NVRWrite(addr + HeaderSize + s, &data[s], partSize, Flags);
        } else {
            memcpy(&Page[HeaderSize], data, partSize); 
            return NVRWrite(addr, Page, partSize + HeaderSize, Flags);
        } 
    } else {
        return NVRWrite(addr + HeaderSize + pos, data, partSize, Flags); 
    }    
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

/**
  * @brief
  * @param
  * @retval
  */
NVRError_t NVREraseAll()
{        
    NVRError_t ret = NVR_ERROR_NONE;
    uint32_t addr = MemoryStartAddr;
    uint32_t endMem = MemoryStartAddr + MemorySize;
       
    for (; addr < endMem; addr += SectorSize) {
        if (0 != (ret = NVREraseSectorLL(addr))) {
            break;
        }
    }  
    FileFound = FoundFileAddr = FoundFileSize = LastFileId = LastFileAddr = LastFileSize = 0;
    return ret;
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
    
    if (0 != NVRReadDataLL(addr, Page, bytesToRead)) {
        return NVR_ERROR_HW;
    }    
    while (offset < bytesToRead - HeaderSize) {        
        NVRHeader_t *h = (NVRHeader_t *)&Page[offset];         
        if ((h->Preamble == PREAMBLE) && (h->FileId == ~h->FileIdInv) && (h->DataSize != 0) && (h->DataSize == ~h->DataSizeInv)) {      // the write procedure doesnt split the header into two pages
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

/**
  * @brief
  * @param
  * @retval
  */
static NVRError_t NVRWrite(uint32_t addr, uint8_t *data, uint32_t size, uint32_t flags)
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
            sectorChunkSize = remain;
            stop = 1;
        }
        if (finishSector) {
            finishSector = 0;
        } else {
            NVREraseSectorLL(addr);            
        }
        
        stopS = 0;
        do {
            if (sectorChunkSize > pageRemain) {
                chunkSize = pageRemain;
                sectorChunkSize -= pageRemain;
                pageRemain = PageSize;
            } else {
                chunkSize = sectorChunkSize;
                stopS = 1;
            }  
            if (0 == NVRWriteDataLL(addr, &data[offset], chunkSize)) {
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
    

//------------------------------------------------------------------------------
// END
//------------------------------------------------------------------------------


