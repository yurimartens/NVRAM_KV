/**
  ******************************************************************************
  * @file    nvram_kv.c
  * @author  Yuri Martentsev <yurimartens@gmail.com>, <yuri.martens@yandex.ru>
  * @brief
  ******************************************************************************
  */

#include "nvram_kv.h"
#include <string.h>

#include "crc32.h"




#define PREAMBLE                0x1FACADE1






typedef struct {
	uint32_t                    Preamble;       
    uint64_t                    FileId;
    uint64_t                    FileIdInv;    
	uint32_t                    DataSize;
    uint32_t                    DataSizeInv;
    uint32_t                    DataCRC32;
} NVRHeader_t;

const uint32_t                  NVRHeaderSize = sizeof(NVRHeader_t);
static NVRHeader_t              EmptyNVRHeader;




static NVRError_t NVRCheckHeader(NVRamKV_t *nvr, uint32_t addr, uint32_t *currAddr, uint64_t *currId, uint32_t *currSize, uint32_t *crc);
static NVRError_t NVRWrite(NVRamKV_t *nvr, uint32_t addr, uint8_t *data, uint32_t size, uint32_t flags);


/**
  * @brief
  * @param
  * @retval
  */
NVRError_t NVRInit(NVRamKV_t *nvr, uint32_t pageSize, uint32_t sectorSize, uint32_t startAddr, uint32_t memSize, uint8_t *page, uint32_t flags)
{    
    if ((pageSize == 0) || (sectorSize == 0) || (memSize == 0) || (sectorSize < pageSize) || (page == 0)) return NVR_ERROR_INIT;
    nvr->NotReady = 1;
    nvr->LastFileId = nvr->LastFileAddr = nvr->LastFileSize = 0;
    nvr->FileFound = nvr->TryToOpen = 0;
    
    nvr->PageSize = pageSize;
    nvr->SectorSize = sectorSize;
    nvr->MemoryStartAddr = startAddr;
    nvr->MemorySize = memSize;
    nvr->Page = page;
    nvr->Flags = flags;
    
    memset(&EmptyNVRHeader, 0xFF, NVRHeaderSize);
    
    return NVR_ERROR_NONE;
}

/**
  * @brief
  * @param
  * @retval
  */
NVRError_t NVRInitLL(NVRamKV_t *nvr, NVRReadData_t nvrRead, NVRWriteData_t nvrWrite, NVREraseSector_t nvrErase)
{
    if ((nvrRead == 0) || (nvrWrite == 0) || (nvrErase == 0)) return NVR_ERROR_INIT;
    nvr->NVRReadDataLL = nvrRead;
    nvr->NVRWriteDataLL = nvrWrite;
    nvr->NVREraseSectorLL = nvrErase;
       
    nvr->NotReady--;
    
    return NVR_ERROR_NONE;
}

/**
  * @brief      todo: BST
  * @param
  * @retval
  */
NVRError_t NVROpenFile(NVRamKV_t *nvr, uint64_t id, uint32_t *size, uint32_t flags, uint32_t emptyPagesLim)
{
    if (nvr->NotReady) return NVR_ERROR_INIT;
    if (size == 0) return NVR_ERROR_ARGUMENT;   // check pointer
    
    NVRError_t ret = NVR_ERROR_NONE;
    
    uint32_t start, end, half, emptyPages = 0; 
    uint64_t lastFileId = 0;
    
    *size = 0;
    
    if (((flags & NVR_OPEN_FLAGS_FROM_CURRENT_POS) == 0) || (nvr->FoundFileAddr == 0)) {
        if (flags & NVR_OPEN_FLAGS_BINARY_SEARCH) {
            half = nvr->MemorySize / 2; 
            start = nvr->MemoryStartAddr + half;         
        } else {
            start = nvr->MemoryStartAddr;          
        }
    } else {
        start = nvr->MemoryStartAddr + nvr->FoundFileAddr - NVRHeaderSize;
    }
    end = nvr->MemoryStartAddr + nvr->MemorySize; 
    
    uint32_t addr, s, crc, exit = 0;     
    nvr->TryToOpen = 1;
    nvr->FileFound = nvr->FoundFileAddr = nvr->FoundFileSize = nvr->LastFileId = nvr->LastFileAddr = nvr->LastFileSize = 0;
    while ((start < end) && (exit == 0)) {
        switch (ret = NVRCheckHeader(nvr, start, &addr, &lastFileId, &s, &crc)) {
            case NVR_ERROR_NONE:
                start = addr + s;  // next addr to scan
                if (nvr->Flags & NVR_FLAGS_PAGE_ALIGN) {
                   uint32_t pageFilled = start % nvr->PageSize;
                   start += nvr->PageSize - pageFilled;
                } 
                nvr->LastFileAddr = addr + NVRHeaderSize - nvr->MemoryStartAddr;       // - startAddr => make relative addr
                nvr->LastFileSize = s - NVRHeaderSize;
                if ((id == lastFileId) || (flags & NVR_OPEN_FLAGS_ANY_ID)) {
                    nvr->FileFound = 1; 
                    nvr->FoundFileAddr = nvr->LastFileAddr;
                    nvr->FoundFileSize = nvr->LastFileSize;  
                    nvr->CRC32Temp = crc;
                    exit = flags & NVR_OPEN_FLAGS_FIRST_MATCH;
                } else {
                    if (flags & NVR_OPEN_FLAGS_NEAREST) {
                        if ((id > nvr->LastFileId) && (id < lastFileId)) {
                            nvr->FileFound = 1; 
                            nvr->FoundFileAddr = nvr->LastFileAddr;
                            nvr->FoundFileSize = nvr->LastFileSize;   
                            nvr->CRC32Temp = crc;
                            exit = 1;
                        }
                    }
                }
                nvr->LastFileId = lastFileId;
            break;
            case NVR_ERROR_EMPTY:
                if (emptyPages < emptyPagesLim) emptyPages++;
                else exit = 1;
                if (lastFileId == 0) {
                    if (flags & NVR_OPEN_FLAGS_BINARY_SEARCH) {
                        if (half >= nvr->PageSize * 2) {
                            half /= 2;
                        }                        
                        if (start >= nvr->MemoryStartAddr + half) start -= half;
                        else { 
                            if (start == nvr->MemoryStartAddr) return NVR_ERROR_EMPTY;
                            else start = nvr->MemoryStartAddr;
                        }
                    } else {
                        start += nvr->PageSize;
                    }
                } else {
                    exit = 1;
                }
                if (nvr->FileFound) exit = 1;   // a file was found in a prev cycle
            break;
            case NVR_ERROR_HEADER:
                if (emptyPages < emptyPagesLim) emptyPages++;
                else exit = 1;
                if (lastFileId == 0) {
                    if (flags & NVR_OPEN_FLAGS_BINARY_SEARCH) {
                        if (start >= nvr->MemoryStartAddr + half) start -= half;
                        else { 
                            if (start == nvr->MemoryStartAddr) return NVR_ERROR_EMPTY;
                            else start = nvr->MemoryStartAddr;
                        }
                    } else {
                        start += nvr->PageSize;
                    }
                } else {
                    exit = 1;
                }
                if (nvr->FileFound) exit = 1;   // a file was found in a prev cycle
            break; 
            default:
                return ret;
            break;
        }
    }
    if (nvr->FileFound) {
        *size = nvr->FoundFileSize;
        ret = NVR_ERROR_OPENED;
    } else {   
        nvr->FoundFileAddr = nvr->LastFileAddr;
        nvr->FoundFileSize = nvr->LastFileSize;
        ret = NVR_ERROR_NOT_FOUND;
    }
    return ret;
}

    
/**
  * @brief      
  * @param
  * @retval
  */
uint32_t NVRGetNextAddr(NVRamKV_t *nvr)
{
    return nvr->LastFileAddr + nvr->LastFileSize;
}

/**
  * @brief      
  * @param
  * @retval
  */
uint32_t NVRGetFileCRC(NVRamKV_t *nvr)
{
    return nvr->CRC32Temp;
}

/**
  * @brief      
  * @param
  * @retval
  */
void NVRMoveToStart(NVRamKV_t *nvr)
{
    nvr->FileFound = nvr->FoundFileAddr = nvr->FoundFileSize = 0;
}

/**
  * @brief      
  * @param
  * @retval
  */
NVRError_t NVRMoveToNextFile(NVRamKV_t *nvr)
{
    if (nvr->FileFound) {
        if (nvr->Flags & NVR_FLAGS_PAGE_ALIGN) {
            uint32_t start = nvr->FoundFileAddr + nvr->FoundFileSize - NVRHeaderSize;
            uint32_t pageFilled = start % nvr->PageSize;
            start += nvr->PageSize - pageFilled;
            nvr->FoundFileAddr = start + NVRHeaderSize;
        } else { 
            nvr->FoundFileAddr += nvr->FoundFileSize;
        }
        return NVR_ERROR_NONE;
    } else {
        return NVR_ERROR_NOT_FOUND;
    }    
}

/**
  * @brief      
  * @param
  * @retval
  */
uint64_t NVRGetLastId(NVRamKV_t *nvr)
{
    return nvr->LastFileId;
}

/**
  * @brief
  * @param
  * @retval
  */
NVRError_t NVRReadFile(NVRamKV_t *nvr, uint32_t pos, uint8_t *data, uint32_t size)
{
    if (nvr->NotReady) return NVR_ERROR_INIT;
    if ((nvr->FileFound == 0) || (nvr->TryToOpen == 0)) return NVR_ERROR_NOT_FOUND;
    uint32_t addr = nvr->FoundFileAddr + pos; 
    uint32_t pageFilled = addr % nvr->PageSize;
    if ((addr + size > nvr->FoundFileAddr + nvr->FoundFileSize) || (data == 0) || (size == 0)) return NVR_ERROR_ARGUMENT;
    
    NVRError_t ret = NVR_ERROR_NONE;
    uint32_t chunkSize = 0;
    uint32_t remain = size, offset = 0;
    
    addr += nvr->MemoryStartAddr;       // make absolute addr
    
    if ((pageFilled) && ((pageFilled + size) > nvr->PageSize)) {
        int s = nvr->PageSize - pageFilled;
        if (0 == nvr->NVRReadDataLL(addr, data, s)) {
            offset += s;
            remain -= s;
        } else {
            return NVR_ERROR_HW;            
        }
    }
    uint32_t stop = 0;
    do {
        if (remain > nvr->PageSize) {
            chunkSize = nvr->PageSize;
            remain -= nvr->PageSize;
        } else {
            chunkSize = remain;
            stop = 1;
        }
        if (0 == nvr->NVRReadDataLL(addr + offset, &data[offset], chunkSize)) {
            offset += chunkSize;
        } else {
            ret = NVR_ERROR_HW;
            stop = 1;
        }
    } while (stop == 0); 
    
    if (ret != NVR_ERROR_NONE) return ret;    
    if (CalcCRC32(data, size, 0) != nvr->CRC32Temp) return NVR_ERROR_CRC;
    else return NVR_ERROR_NONE;
}


/**
  * @brief
  * @param
  * @retval
  */
NVRError_t NVRWriteFile(NVRamKV_t *nvr, uint64_t id, uint8_t *data, uint32_t size)
{
    if (nvr->NotReady) return NVR_ERROR_INIT;
    if (nvr->TryToOpen == 0) return NVR_ERROR_NOT_FOUND;
    if (data == 0) return NVR_ERROR_ARGUMENT;
    
    NVRError_t ret = NVR_ERROR_NONE;
    uint32_t owf = 0;
    uint32_t addr = (nvr->LastFileAddr == 0) ? 0 : nvr->LastFileAddr + nvr->LastFileSize;
    uint32_t pageFilled = addr % nvr->PageSize;
    uint32_t pageRemain = nvr->PageSize - pageFilled;
    if ((pageFilled && (nvr->Flags & NVR_FLAGS_PAGE_ALIGN)) || (pageRemain < NVRHeaderSize)) {     // align to the nearest start of the page if the flag is set or there is no place for the whole header
        addr += pageRemain;
        pageFilled = 0;
    }       
    
    if ((addr + NVRHeaderSize + size) > nvr->MemorySize) {
        addr = 0;
        owf = 1;
    }
    nvr->FoundFileAddr = nvr->LastFileAddr = addr + NVRHeaderSize;  // to allaw instant access to the file after writing
    nvr->FoundFileSize = nvr->LastFileSize = size;
    nvr->LastFileId = id;
    
    addr += nvr->MemoryStartAddr;       // make absolute addr        
    
    NVRHeader_t *h = (NVRHeader_t *)nvr->Page;    // fill header and copy to the page buff
    memset(nvr->Page, 0, NVRHeaderSize);
    h->Preamble = PREAMBLE;
    h->FileId = id;
    h->FileIdInv = ~id;
    h->DataSize = size;  
    h->DataSizeInv = ~size;  
    h->DataCRC32 = CalcCRC32(data, size, 0);
                        
    if ((pageFilled + NVRHeaderSize + size) > nvr->PageSize) {
        int s = nvr->PageSize - pageFilled - NVRHeaderSize;        
        memcpy(&nvr->Page[NVRHeaderSize], data, s);    
        size -= s;
        if (0 != (ret = NVRWrite(nvr, addr, nvr->Page, nvr->PageSize - pageFilled, nvr->Flags))) return ret;
        ret = NVRWrite(nvr, addr + NVRHeaderSize + s, &data[s], size, nvr->Flags);
        if (owf) return NVR_ERROR_END_MEM;
        else return ret;
    } else {
        memcpy(&nvr->Page[NVRHeaderSize], data, size); 
        ret = NVRWrite(nvr, addr, nvr->Page, size + NVRHeaderSize, nvr->Flags);
        if (owf) return NVR_ERROR_END_MEM;
        else return ret;
    }    
}


/**
  * @brief
  * @param
  * @retval
  */
NVRError_t NVRCloseFile(NVRamKV_t *nvr, uint64_t id)
{
    if (nvr->NotReady) return NVR_ERROR_INIT;
    nvr->TryToOpen = nvr->FileFound = 0;
    return NVR_ERROR_NONE;
}

/**
  * @brief
  * @param
  * @retval
  */
NVRError_t NVREraseAll(NVRamKV_t *nvr)
{        
    NVRError_t ret = NVR_ERROR_NONE;
    uint32_t addr = nvr->MemoryStartAddr;
    uint32_t endMem = nvr->MemoryStartAddr + nvr->MemorySize;
       
    for (; addr < endMem; addr += nvr->SectorSize) {
        if (0 != (ret = nvr->NVREraseSectorLL(addr))) {
            break;
        }
    }  
    nvr->FileFound = nvr->FoundFileAddr = nvr->FoundFileSize = nvr->LastFileId = nvr->LastFileAddr = nvr->LastFileSize = 0;
    return ret;
}

/* ----------------------------------------- Private functions -------------------------------------------------*/    

/**
  * @brief
  * @param
  * @retval
  */
static NVRError_t NVRCheckHeader(NVRamKV_t *nvr, uint32_t addr, uint32_t *currAddr, uint64_t *currId, uint32_t *currSize, uint32_t *crc)
{    
    uint32_t offset = 0;
    uint32_t endMem = nvr->MemoryStartAddr + nvr->MemorySize;
    if ((addr + NVRHeaderSize) > endMem) return NVR_ERROR_END_MEM;
    
    uint32_t bytesToRead = (endMem - addr > nvr->PageSize) ? nvr->PageSize : endMem - addr;
    
    if (0 != nvr->NVRReadDataLL(addr, nvr->Page, bytesToRead)) {
        return NVR_ERROR_HW;
    }
    uint8_t empty = 1;
    while (offset < bytesToRead - NVRHeaderSize) {        
        NVRHeader_t *h = (NVRHeader_t *)&nvr->Page[offset];         
        if ((h->Preamble == PREAMBLE) && (h->FileId == ~h->FileIdInv) && (h->DataSize != 0) && (h->DataSize == ~h->DataSizeInv)) {      // the write procedure doesnt split the header into two pages
            *currAddr = addr + offset;
            *currId = h->FileId;                                    
            *currSize = NVRHeaderSize + h->DataSize; 
            *crc = h->DataCRC32;
            return NVR_ERROR_NONE;                
        } else {
            if (0 != memcmp(h, &EmptyNVRHeader, NVRHeaderSize)) {
                empty = 0;
            }
            offset++;
        }        
    }
    if (empty) return NVR_ERROR_EMPTY;
    else return NVR_ERROR_HEADER;
}

/**
  * @brief
  * @param
  * @retval
  */
static NVRError_t NVRWrite(NVRamKV_t *nvr, uint32_t addr, uint8_t *data, uint32_t size, uint32_t flags)
{
    if (nvr->NotReady) return NVR_ERROR_INIT;
    if (nvr->TryToOpen == 0) return NVR_ERROR_NOT_FOUND;
    if (data == 0) return NVR_ERROR_ARGUMENT;
    
    NVRError_t ret = NVR_ERROR_NONE;
    uint32_t chunkSize = 0, sectorChunkSize;
    uint32_t remain = size, offset = 0;
    uint32_t stop = 0, stopS = 0;
    uint32_t finishSector = (addr % nvr->SectorSize) ? 1 : 0;
    uint32_t sectorRemain = nvr->SectorSize - (addr % nvr->SectorSize);
    uint32_t pageRemain = nvr->PageSize - (addr % nvr->PageSize);
    uint32_t endMem = nvr->MemoryStartAddr + nvr->MemorySize;
       
    do {
        if (addr == endMem) addr = nvr->MemoryStartAddr;
        if (remain > sectorRemain) {
            sectorChunkSize = sectorRemain;
            remain -= sectorRemain;
            sectorRemain = nvr->SectorSize;
        } else {
            sectorChunkSize = remain;
            stop = 1;
        }
        if (finishSector) {
            finishSector = 0;
        } else {
            nvr->NVREraseSectorLL(addr);            
        }
        
        stopS = 0;
        do {
            if (sectorChunkSize > pageRemain) {
                chunkSize = pageRemain;
                sectorChunkSize -= pageRemain;
                pageRemain = nvr->PageSize;
            } else {
                chunkSize = sectorChunkSize;
                stopS = 1;
            }  
            if (0 == nvr->NVRWriteDataLL(addr, &data[offset], chunkSize)) {
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


