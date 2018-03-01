/*
	I2C C API for ADIAPI and such ones
*/

#include "I2Cif.h"

#include "log.h"

static FX2Device_t *_ctx = NULL;

void I2CWrapper_setCtx(FX2Device_t *ctx) {
	_ctx = ctx;
}

int I2CWrapper_writeRead(uint8_t addr, const uint8_t *outbuf, size_t outlen, uint8_t *inbuf, size_t inlen) {
	if(!_ctx) return 0;
	return _ctx->I2CWriteRead(addr, outbuf, outlen, inbuf, inlen);
}

int I2CWrapper_read(uint8_t addr, uint8_t *inbuf, size_t inlen) {
	if(!_ctx) return 0;
	return _ctx->I2CRead(addr, inbuf, inlen);
}

int I2CWrapper_write(uint8_t addr, const uint8_t *outbuf, size_t outlen) {
	if(!_ctx) return 0;
	return _ctx->I2CWrite(addr, outbuf, outlen);
}
