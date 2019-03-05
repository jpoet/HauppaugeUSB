#ifndef __USBIF_H_
#define __USBIF_H_

#include <libusb.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <tuple>
#include <functional>

//#include "common.h"
#include "log.h"
#include "baseif.h"

using DeviceID = std::tuple<int, int, std::string, std::string,
                            uint8_t, uint8_t>;
using DeviceIDVec = std::vector<DeviceID>;

//struct USBWrapperControlMessage_t;
//class USBWrapperAsyncCtx_t;

//USB wrapper

#define USBWRAP_CM_DEVICE_VENDOR_WR 0x40
#define USBWRAP_CM_DEVICE_VENDOR_RD 0xC0

typedef enum {
        USBWRAP_SUCCESS = 0,
        USBWRAP_ERROR_IO = -1,
        USBWRAP_ERROR_INVALID_PARAM = -2,
        USBWRAP_ERROR_ACCESS = -3,
        USBWRAP_ERROR_NO_DEVICE = -4,
        USBWRAP_ERROR_NOT_FOUND = -5,
        USBWRAP_ERROR_BUSY = -6,
        USBWRAP_ERROR_TIMEOUT = -7,
        USBWRAP_ERROR_OVERFLOW = -8,
        USBWRAP_ERROR_PIPE = -9,
        USBWRAP_ERROR_INTERRUPTED = -10,
        USBWRAP_ERROR_NO_MEM = -11,
        USBWRAP_ERROR_NOT_SUPPORTED = -12,
        USBWRAP_ERROR_OTHER = -99,
        USBWRAP_PENDING = -101
} USBWrapperError_t;

typedef enum {
        USBWRAP_TT_CONTROL,
        USBWRAP_TT_BULK
} USBWrapperTT_t;

typedef struct {
        uint8_t bmRequestType;
        uint8_t bRequest;
        uint16_t wValue;
        uint16_t wIndex;
        uint16_t wLength;
} USBWrapperControlMessage_t;

class USBWrapperAsyncCtx_t
{
  protected:
    pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
    bool finished;
  public:
    int result;
    uint32_t size;
    USBWrapperAsyncCtx_t(): finished(true), result(USBWRAP_PENDING)
    {
        pthread_mutex_init(&m_mutex, NULL);
        pthread_cond_init(&m_cond, NULL);
    }
    ~USBWrapperAsyncCtx_t()
    {
        wait();
        pthread_cond_destroy(&m_cond);
        pthread_mutex_destroy(&m_mutex);
    }

    inline void init()
    {
        finished = false;
        result = USBWRAP_PENDING;
    }
    inline void set(int r, uint32_t s)
    {
        pthread_mutex_lock(&m_mutex);
        finished = true;
        result = r;
        size = s;
        pthread_cond_signal(&m_cond);
        pthread_mutex_unlock(&m_mutex);
    }
    inline int getState()
    {
        pthread_mutex_lock(&m_mutex);
        int r = result;
        pthread_mutex_unlock(&m_mutex);
        return r;
    }
    inline void wait()
    {
        pthread_mutex_lock(&m_mutex);
        while(!finished) pthread_cond_wait(&m_cond, &m_mutex);
        pthread_mutex_unlock(&m_mutex);
    }
};

class USBWrapper_t
{
  public:
    using callback_t = std::function<void()>;

    USBWrapper_t(void);
    ~USBWrapper_t(void);

    bool Open(const std::string& serial, callback_t * error_cb = nullptr);
    void Close(void);

    bool DeviceList(DeviceIDVec& devs);
    void USBDevDesc(std::string& desc);

    std::string MsgString(void);
    std::string ErrorString(void);

    /* For USBWrapper_t */
    int controlMessage(USBWrapperControlMessage_t &msg, uint8_t *buf,
                       uint32_t timeout);
    int bulkRead(uint8_t num, uint8_t *buf, uint32_t len, uint32_t timeout);
    int bulkReadAsync(USBWrapperAsyncCtx_t &ctx, uint8_t num,
                      uint8_t *buf, uint32_t len, uint32_t timeout);
    int bulkWrite(uint8_t num, const uint8_t *buf, uint32_t len,
                  uint32_t timeout);
    int clearStall(uint8_t num);
    int abort(uint8_t num);
    int abortControl();

    void setErrorCB(callback_t & cb) { m_error_cb = cb; m_use_error_cb = true; }

  protected:
    callback_t  m_error_cb;
    bool        m_use_error_cb;

  private:
    bool DevName(std::string& name, struct libusb_device_descriptor& desc);

    libusb_context       *m_ctx;
    libusb_device       **m_dev_list;
    int                   m_dev_cnt;
    libusb_device        *m_device;
    struct libusb_device_descriptor m_desc;
    std::string           m_name;
    libusb_device_handle *m_handle;

    std::ostringstream    m_errmsg;
    std::ostringstream    m_msg;
};

#endif
