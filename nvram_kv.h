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
/*


#define GD25QXX_PAGE_SIZE               256
    

#define GD25QXX_STATUS_BIT_WIP          (1 << 0)   
    
#define GD25QXX_FLAG_NONBLOCK_CALL      (1 << 0)       
#define GD25QXX_FLAG_NONBLOCK_CALL_DMA  (1 << 1)
    
    
typedef enum {
    GD25QXX_WRITE_ENABLE =              0x06,
    GD25QXX_WRITE_DISABLE =             0x04,
    GD25QXX_WRITE_ENABLE_VSR =          0x50,
    GD25QXX_READ_STATUS_REG_0_7 =       0x05,
    GD25QXX_READ_STATUS_REG_8_15 =      0x35,
    GD25QXX_WRITE_STATUS_REG =          0x01,
    GD25QXX_READ_STATUS_REG =           0x05,
    GD25QXX_READ_DATA =                 0x03,
    GD25QXX_FAST_READ_DATA =            0x0B,
    GD25QXX_PAGE_PROGRAM =              0x02,
    GD25QXX_SECTOR_ERASE =              0x20,
    GD25QXX_BLOCK32K_ERASE =            0x52,
    GD25QXX_BLOCK64K_ERASE =            0xD8,
    GD25QXX_CHIP_ERASE =                0x60,
    GD25QXX_DEEP_POWER_DOWN =           0xB9,
    GD25QXX_RELEASE_FROM_DEEP_POWER_DOWN = 0xAB,
    GD25QXX_READ_MAN_DEV_ID =           0x90,
    GD25QXX_READ_MAN_TYPE_CAP =         0x9F,
    GD25QXX_READ_UNIQUE_ID =            0x4B,
    GD25QXX_ENABLE_RESET =              0x66,
    GD25QXX_RESET =                     0x99,      
} GD25QxxCmd_t;






typedef struct {
	SPIAl_t                     *spi;
	uint32_t                    CSIdx;
    uint8_t                     Page[GD25QXX_PAGE_SIZE];
} GD25Q_t;


void GD25QxxInit(GD25Q_t *gd25, SPIAl_t *spi, uint32_t csi);
int32_t GD25QxxReadStatus(GD25Q_t *gd25);
int32_t GD25QxxWriteEnable(GD25Q_t *gd25);
int32_t GD25QxxWriteDisable(GD25Q_t *gd25);
int32_t GD25QxxReadManTypeCap(GD25Q_t *gd25);
int32_t GD25QxxReadUnique128ID(GD25Q_t *gd25, uint8_t *id);
int32_t GD25QxxReadData(GD25Q_t *gd25, uint32_t addr, uint8_t *buf, uint32_t size, uint32_t flags);
int32_t GD25QxxWritePage(GD25Q_t *gd25, uint32_t addr, uint8_t *buf, uint32_t size, uint32_t flags);
int32_t GD25QxxEraseSector(GD25Q_t *gd25, uint32_t addr);
*/


#ifdef __cplusplus
}
#endif

#endif // _NVRAM_KV_H
//------------------------------------------------------------------------------
// END
//------------------------------------------------------------------------------
