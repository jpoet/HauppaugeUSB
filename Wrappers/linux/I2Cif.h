/*
        I2C C API for ADIAPI and such ones
*/

#ifndef __I2CIF_H_
#define __I2CIF_H_

#include <stdint.h>

#include "FX2Device.h"

void I2CWrapper_setCtx(FX2Device_t *ctx);

int I2CWrapper_writeRead(uint8_t addr, const uint8_t *outbuf, size_t outlen, uint8_t *inbuf, size_t inlen);
int I2CWrapper_read(uint8_t addr, uint8_t *inbuf, size_t inlen);
int I2CWrapper_write(uint8_t addr, const uint8_t *outbuf, size_t outlen);

#endif
