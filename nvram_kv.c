/**
  ******************************************************************************
  * @file    nvram_kv.c
  * @author  Yuri Martenstev <yurimartens@gmail.com>
  * @brief
  ******************************************************************************
  */

#include "nvram_kv.h"
#include <string.h>

#if 0

/**
  * @brief
  * @param
  * @retval
  */
void GD25QxxInit(GD25Q_t *gd25, SPIAl_t *spi, uint32_t csi)
{
    gd25->spi = spi;
    gd25->CSIdx = csi;
}

/**
  * @brief
  * @param
  * @retval
  */
int32_t GD25QxxReadStatus(GD25Q_t *gd25)
{    
    if (AL_ERROR_BUSY == SPIIsBusyCS(gd25->spi, gd25->CSIdx)) return -1;
    uint8_t cmd[2] = {GD25QXX_READ_STATUS_REG_0_7, 0};
    SPITransmitReceive(gd25->spi, gd25->CSIdx, cmd, cmd, 2, 2);
    return cmd[1];
}

/**
  * @brief
  * @param
  * @retval
  */
static int32_t GD25QxxWaitForReady(GD25Q_t *gd25)
{   
    int32_t st;
    do {
        st = GD25QxxReadStatus(gd25);
    } while (st & GD25QXX_STATUS_BIT_WIP);
    return 0;
}

/**
  * @brief
  * @param
  * @retval
  */
int32_t GD25QxxWriteEnable(GD25Q_t *gd25)
{    
    if (AL_ERROR_BUSY == SPIIsBusyCS(gd25->spi, gd25->CSIdx)) return -1;
    uint8_t cmd = GD25QXX_WRITE_ENABLE;
    SPITransmitReceive(gd25->spi, gd25->CSIdx, &cmd, &cmd, 1, 1);
    return 0;
}

/**
  * @brief
  * @param
  * @retval
  */
int32_t GD25QxxWriteDisable(GD25Q_t *gd25)
{    
    if (AL_ERROR_BUSY == SPIIsBusyCS(gd25->spi, gd25->CSIdx)) return -1;
    uint8_t cmd = GD25QXX_WRITE_DISABLE;
    SPITransmitReceive(gd25->spi, gd25->CSIdx, &cmd, &cmd, 1, 1);
    return 0;
}

/**
  * @brief
  * @param
  * @retval
  */
int32_t GD25QxxReadManTypeCap(GD25Q_t *gd25)
{    
    if (AL_ERROR_BUSY == SPIIsBusyCS(gd25->spi, gd25->CSIdx)) return -1;
    uint8_t cmd[] = {GD25QXX_READ_MAN_TYPE_CAP, 0, 0, 0};   
    SPITransmitReceive(gd25->spi, gd25->CSIdx, cmd, cmd, 4, 4);
    return (cmd[1] << 16) | (cmd[2] << 8) | cmd[3];
}

/**
  * @brief
  * @param
  * @retval
  */
int32_t GD25QxxReadUnique128ID(GD25Q_t *gd25, uint8_t *id)
{    
    if (AL_ERROR_BUSY == SPIIsBusyCS(gd25->spi, gd25->CSIdx)) return -1;
    uint8_t cmd[5] = {GD25QXX_READ_UNIQUE_ID, 0, 0, 0, 0};  
    SPILockCS(gd25->spi, gd25->CSIdx);
    SPITransmitReceive(gd25->spi, gd25->CSIdx, cmd, cmd, 5, 5);
    SPIUnlockCS(gd25->spi, gd25->CSIdx);
    SPITransmitReceive(gd25->spi, gd25->CSIdx, id, id, 16, 16);
    return 0;
}


/**
  * @brief
  * @param
  * @retval
  */
int32_t GD25QxxReadData(GD25Q_t *gd25, uint32_t addr, uint8_t *buf, uint32_t size, uint32_t flags)
{    
    if (AL_ERROR_BUSY == SPIIsBusyCS(gd25->spi, gd25->CSIdx)) return -1;
    uint8_t cmd[4] = {GD25QXX_READ_DATA, addr >> 16, addr >> 8, addr};
    SPILockCS(gd25->spi, gd25->CSIdx);
    SPITransmitReceive(gd25->spi, gd25->CSIdx, cmd, cmd, 4, 4);
    SPIUnlockCS(gd25->spi, gd25->CSIdx);
    if (flags & GD25QXX_FLAG_NONBLOCK_CALL) {
        if (flags & GD25QXX_FLAG_NONBLOCK_CALL_DMA) {
            SPITransmitReceiveDMA(gd25->spi, gd25->CSIdx, buf, buf, size, size);
        } else {
            SPITransmitReceiveIT(gd25->spi, gd25->CSIdx, buf, buf, size, size);
        }
    } else {
        SPITransmitReceive(gd25->spi, gd25->CSIdx, buf, buf, size, size);
    }
    return 0;
}

/**
  * @brief
  * @param
  * @retval
  */
int32_t GD25QxxWritePage(GD25Q_t *gd25, uint32_t addr, uint8_t *buf, uint32_t size, uint32_t flags)
{    
    if (AL_ERROR_BUSY == SPIIsBusyCS(gd25->spi, gd25->CSIdx)) return -1;
    if (size == 0) return -2;
    uint8_t cmd[4] = {GD25QXX_PAGE_PROGRAM, addr >> 16, addr >> 8, addr};
    GD25QxxWriteEnable(gd25);
    SPILockCS(gd25->spi, gd25->CSIdx);
    SPITransmitReceive(gd25->spi, gd25->CSIdx, cmd, cmd, 4, 4);
    SPIUnlockCS(gd25->spi, gd25->CSIdx);
    SPITransmitReceive(gd25->spi, gd25->CSIdx, buf, buf, size, size);
    GD25QxxWaitForReady(gd25);    
    GD25QxxWriteDisable(gd25);
    return 0;
}

/**
  * @brief
  * @param
  * @retval
  */
int32_t GD25QxxEraseSector(GD25Q_t *gd25, uint32_t addr)
{    
    if (AL_ERROR_BUSY == SPIIsBusyCS(gd25->spi, gd25->CSIdx)) return -1;
    uint8_t cmd[4] = {GD25QXX_SECTOR_ERASE, addr >> 16, addr >> 8, addr};
    GD25QxxWriteEnable(gd25);
    SPITransmitReceive(gd25->spi, gd25->CSIdx, cmd, cmd, 4, 4);
    GD25QxxWaitForReady(gd25);    
    GD25QxxWriteDisable(gd25);
    return 0;
}
#endif

//------------------------------------------------------------------------------
// END
//------------------------------------------------------------------------------


