#include <cstring>
#include <set>
#include <fstream>
#include <iostream>
#include <libusb.h>

#include "USBif.h"

using namespace std;

USBWrapper_t::USBWrapper_t(void)
    : m_ctx(nullptr),
      m_dev_list(nullptr),
      m_dev_cnt(0),
      m_device(nullptr),
      m_handle(nullptr)
{
    int ret;

    if ((ret = libusb_init(&m_ctx)) < 0)
    {
        m_errmsg << "Failed to initialize libusb: " << ret << endl;
        return;
    }

    /*
      LIBUSB_LOG_LEVEL_NONE = 0, LIBUSB_LOG_LEVEL_ERROR,
      LIBUSB_LOG_LEVEL_WARNING, LIBUSB_LOG_LEVEL_INFO,
      LIBUSB_LOG_LEVEL_DEBUG
      logging goes to stderr!!! ick!!!
    */
    libusb_set_debug(m_ctx, LIBUSB_LOG_LEVEL_NONE);
}

USBWrapper_t::~USBWrapper_t(void)
{
    Close();
    if (m_ctx)
    {
        libusb_exit(m_ctx);
        m_ctx = NULL;
    }
    if (m_dev_list)
        //free the list, unref the devices in it
        libusb_free_device_list(m_dev_list, 0);

}

bool USBWrapper_t::DevName(string& name, struct libusb_device_descriptor& desc)

{
    if (desc.idVendor != 0x2040)
        return false;

    if (desc.idProduct == 0xe504)
    {
        // 157220
        name = "HD PVR 2 Gaming Edition Plus w/SPDIF";
        return true;
    }
    if (desc.idProduct == 0xe505)
    {
        // 157320
        name = "HD PVR 2 Gaming Edition Plus w/SPDIF w/MIC";
        return true;
    }
    if (desc.idProduct == 0xe524)
    {
        // 157210
        name = "HD PVR 2 Gaming Edition";
        return true;
    }
    if (desc.idProduct == 0xe514)
    {
        // 157221
        name = "HD PVR 2 model 1512 w/SPDIF, Alt bling";
        return true;
    }
    if (desc.idProduct == 0xe515)
    {
        // 157321
        name = "HD PVR 2 model 1512 w/SPDIF w/MIC Alt blink";
        return true;
    }
    if (desc.idProduct == 0xe524)
    {
        // Siena2-02
        name = "HD PVR 2 (unknown version)";
        return true;
    }
    if (desc.idProduct == 0xe525)
    {
        // 157310
        name = "HD PVR 2 Gaming Edition w/MIC";
        return true;
    }
    if (desc.idProduct == 0xe52c)
    {
        // Siena2 StreamEez
        name = "HD PVR 2 StreamEez";
        return true;
    }
    if (desc.idProduct == 0xe554)
    {
        // 157222
        name = "HD PVR 2 Gaming Edition Plus w/SPDIF, Blue LEDs";
        return true;
    }
    if (desc.idProduct == 0xe585)
    {
        name = "Colossus 2";
        return true;
    }

    if (desc.idProduct >= 0xe500 && desc.idProduct < 0xeffff)
    {
        name = "Guessing it is a HD-PVR, but version unknown";
        return true;
    }

    return false;
}

void USBWrapper_t::USBDevDesc(string& descstr)
{
    int ret;
    ostringstream msg;

    struct libusb_device_descriptor desc;
    if ((ret = libusb_get_device_descriptor(m_device, &desc)) < 0)
    {
        m_errmsg << "Failed to get USB description for dev.\n";
        return;
    }

    msg << "Number of possible configurations: "
          << (int)desc.bNumConfigurations<<"  ";
    msg << "Device Class: " << (int)desc.bDeviceClass << "  ";
    msg << "VendorID: " << desc.idVendor << "  ";
    msg << "ProductID: " << desc.idProduct << "\n";

    unsigned char              strDesc[257];

    if (desc.iManufacturer > 0)
    {
        libusb_get_string_descriptor_ascii(m_handle, desc.iManufacturer,
                                           strDesc, 256);
        msg << "Manufacturer: " << strDesc << "\n";
    }

    if (desc.iSerialNumber > 0)
    {
        libusb_get_string_descriptor_ascii(m_handle, desc.iSerialNumber,
                                           strDesc, 256);
        msg << "Serial: " << strDesc << "\n";
    }

    libusb_config_descriptor *config;
    libusb_get_config_descriptor(m_device, 0, &config);

    msg << "Interfaces: " << (int)config->bNumInterfaces << " ||| ";

    const libusb_interface *inter;
    const libusb_interface_descriptor *interdesc;
    const libusb_endpoint_descriptor *epdesc;

    for(int i = 0; i < (int)config->bNumInterfaces; ++i)
    {
        inter = &config->interface[i];

        msg << "Number of alternate settings: " << inter->num_altsetting<<" | ";

        for(int j = 0; j < inter->num_altsetting; ++j)
        {
            interdesc = &inter->altsetting[j];
            msg << "Interface Number: "
                  << (int)interdesc->bInterfaceNumber << " | ";
            msg << "Number of endpoints: " << (int)interdesc->bNumEndpoints
                  << " | ";
            for(int k = 0; k < (int)interdesc->bNumEndpoints; ++k)
            {
                epdesc = &interdesc->endpoint[k];
                msg << "Descriptor Type: " << (int)epdesc->bDescriptorType
                      << " | ";
                msg << "EP Address: " << (int)epdesc->bEndpointAddress << " | ";
            }
        }
    }
    msg << "\n\n";
    descstr = msg.str();

    libusb_free_config_descriptor(config);
}

bool USBWrapper_t::DeviceList(DeviceIDVec& devs)
{
    int idx = 0;

    if (m_ctx == NULL)
    {
        m_errmsg << "libusb not initialized.\n";
        return false;
    }

    if (m_dev_list == nullptr)
    {
        if ((m_dev_cnt = libusb_get_device_list(m_ctx, &m_dev_list)) < 0)
        {
            m_errmsg << "Failed to find any usb devices: " << m_dev_cnt << "\n";
            return false;
        }
    }

    if (m_dev_cnt == 0)
    {
        m_errmsg << "No USB devices found.\n";
        return false;
    }

    string name;

    for (idx = 0; idx < m_dev_cnt; ++idx)
    {
        libusb_device *device = m_dev_list[idx];
        libusb_device_descriptor desc = {0};

        if (libusb_get_device_descriptor(device, &desc) == 0 &&
            DevName(name, desc) && desc.iSerialNumber > 0)
        {
            libusb_device_handle *handle;
            char strDesc[257];

            uint8_t bus  = libusb_get_bus_number(m_dev_list[idx]);
            uint8_t port = libusb_get_port_number(m_dev_list[idx]);

            if (libusb_open(m_dev_list[idx], &handle) < 0)
            {
                m_errmsg << "Failed to open dev.\n";
                continue;
            }

            libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber,
                              reinterpret_cast<unsigned char *>(strDesc), 256);
            devs.push_back(make_tuple(desc.idVendor, desc.idProduct,
                                      strDesc, name, bus, port));

            libusb_close(handle);
        }
    }

    return !devs.empty();
}

#if 0
bool USBWrapper_t::ResetBus(uint8_t bus)
{
    libusb_device_handle * dev;

    if ((r = libusb_reset_device(dev)) != 0)
    {
        m_errmsg << "ResetBus failed: "
        if (r == LIBUSB_ERROR_NOT_FOUND)
            m_errmsg << "Re-enumeration is required, "
                     << "or if the device has been disconnected.";
        else
            m_errmsg << "Unknown error.";
    }
}
#endif

bool USBWrapper_t::Open(const string& serial)
{
    int idx = 0;
    int ret = 0;

    if (m_ctx == NULL)
    {
        m_errmsg << "libusb not initialized.\n";
        return false;
    }

    if (m_dev_list == nullptr)
    {
        if ((m_dev_cnt = libusb_get_device_list(m_ctx, &m_dev_list)) < 0)
        {
            m_errmsg << "Failed to find any usb devices: " << m_dev_cnt << "\n";
            return false;
        }
    }

    string name;
    char   strDesc[257];
    int    serial_sz = serial.size();
    libusb_device_handle *handle;

    for (int idx = 0; idx < m_dev_cnt; ++idx)
    {
        struct libusb_device_descriptor desc;
        if ((ret = libusb_get_device_descriptor(m_dev_list[idx], &desc)) < 0)
        {
            m_errmsg << "Failed to get USB description for dev.\n";
            continue;
        }

        if (!DevName(name, desc))
            continue;

        if ((ret = libusb_open(m_dev_list[idx], &handle)) != 0)
        {
            m_errmsg << "Failed to open dev.";
            continue;
        }

        libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber,
                           reinterpret_cast<unsigned char *>(strDesc), 256);

        if (serial.compare(0, serial_sz, strDesc, serial_sz) != 0)
        {
            // Wrong serial #
            libusb_close(handle);
            continue;
        }

        m_handle = handle;
        m_device = m_dev_list[idx];
        m_desc = desc;
        m_name = name;
        break;
    }

    if (idx == m_dev_cnt)
    {
        m_errmsg << "Unable to find serial " << serial << " in USB dev list.\n";
        return false;
    }

    m_msg << "Matched " << hex << m_desc.idVendor
          << ":0x" << m_desc.idProduct << " " << strDesc << endl;

    if ((ret = libusb_reset_device(m_handle)) != 0)
    {
        if (ret == LIBUSB_ERROR_NOT_FOUND)
        {
            m_errmsg << "Device needs re-opened!\n";
        }
        else
        {
            m_errmsg << "Reset failed.\n";
        }
    }

    //find out if kernel driver is attached
    if (libusb_kernel_driver_active(m_handle, 0) == 1)
    {
        //detach it
        if (libusb_detach_kernel_driver(m_handle, 0) < 0)
        {
            m_errmsg << "Failed to detach driver from kernel.\n";
        }
    }

    if (libusb_claim_interface(m_handle, 0) < 0)
    {
        m_errmsg << "Failed to claim interface.\n";
    }

    return true;
}

void USBWrapper_t::Close(void)
{
    if (m_handle)
    {
        libusb_release_interface(m_handle, 0);
        libusb_close(m_handle);
        m_handle = NULL;
    }
}

string USBWrapper_t::MsgString(void)
{
    string msg = m_msg.str();
    m_msg.str("");
    return msg;
}

string USBWrapper_t::ErrorString(void)
{
    string msg = m_errmsg.str();
    m_errmsg.str("");
    return msg;
}


/**
 * For USBWrapper
 **/

static int retMsg(int result)
{
    return result; // the same as in libusb 1.0
}

static const char *strMsg(int result)
{
    return libusb_strerror((libusb_error)result);
}

static int retTrSt(int st)
{
    switch(st)
    {
        case LIBUSB_TRANSFER_COMPLETED:
          return USBWRAP_SUCCESS;
        case LIBUSB_TRANSFER_ERROR:
          return USBWRAP_ERROR_IO;
        case LIBUSB_TRANSFER_TIMED_OUT:
          return USBWRAP_ERROR_TIMEOUT;
        case LIBUSB_TRANSFER_CANCELLED:
          return USBWRAP_ERROR_INTERRUPTED;
        case LIBUSB_TRANSFER_STALL:
          return USBWRAP_ERROR_PIPE;
        case LIBUSB_TRANSFER_NO_DEVICE:
          return USBWRAP_ERROR_NO_DEVICE;
        case LIBUSB_TRANSFER_OVERFLOW:
          return USBWRAP_ERROR_OVERFLOW;
        default:
          return USBWRAP_ERROR_OTHER;
    }
}

static const char *strTrSt(int st)
{
    return strMsg(retTrSt(st)); // the same as in libusb 1.0
}

#define ASSERT_OBJ_CMD(C, V, M)                 \
    do {                                        \
        if(!(V))                                \
        {                                       \
            wrapLogError((M));                  \
            int ret = USBWRAP_ERROR_NO_DEVICE;  \
            C;                                  \
            return ret;                         \
        }                                       \
    }                                           \
    while(0)

#define ASSERT_OBJ(V, M) ASSERT_OBJ_CMD(, V, M)

static void async_callback (libusb_transfer *t)
{
    USBWrapperAsyncCtx_t *ctx = (USBWrapperAsyncCtx_t*)t->user_data;

    if (t->status != LIBUSB_TRANSFER_COMPLETED)
        wrapLogError("cannot finish async bulk read from endpoint --: (%d) %s",
                     t->status, strTrSt(t->status));
//      else wrapLogDebug("async Ok, length: %d", t->actual_length);
    if (ctx)
        ctx->set(retTrSt(t->status), t->actual_length);
}

int USBWrapper_t::controlMessage(USBWrapperControlMessage_t &msg,
                          uint8_t *buf, uint32_t timeout)
{
    ASSERT_OBJ(m_handle, "cannot send control message: device is not opened");
    usleep(1);

    int r = libusb_control_transfer(m_handle, msg.bmRequestType,
                                    msg.bRequest, msg.wValue, msg.wIndex,
                                    buf, msg.wLength, timeout);
    if (r < 0)
    {
        wrapLogError("cannot send control message: %s", strMsg(r));
        return retMsg(r);
    }

    return r;
}

int USBWrapper_t::bulkRead(uint8_t num, uint8_t *buf, uint32_t len,
                           uint32_t timeout)
{
    ASSERT_OBJ(m_handle, "cannot bulk read: device is not opened");

    int l = len;
    int r = libusb_bulk_transfer(m_handle, num | 0x80, buf, len, &l, timeout);

    if (r != LIBUSB_SUCCESS)
    {
        if (r ==  LIBUSB_ERROR_TIMEOUT)
        {
#if 0
            wrapLogWarn("cannot bulk read from endpoint 0x%02X: %s "
                        "(allowed timeout: %d milliseconds)",
                        num, strMsg(r), timeout);
#endif
        }
        else
        {
            wrapLogError("cannot bulk read from endpoint 0x%02X: %s",
                         num, strMsg(r));
        }
        return retMsg(r);
    }

    return l;
}

int USBWrapper_t::bulkReadAsync(USBWrapperAsyncCtx_t &ctx, uint8_t num,
                         uint8_t *buf, uint32_t len, uint32_t timeout)
{
    ctx.init();
    ASSERT_OBJ_CMD(ctx.set(ret, 0), m_handle,
                   "cannot async bulk read: device is not opened");
    libusb_transfer *t = libusb_alloc_transfer(0);

    if (t == NULL) {
        wrapLogError("cannot async bulk read: no memory");
        return USBWRAP_ERROR_NO_MEM;
    }

    libusb_fill_bulk_transfer(t, m_handle, num | 0x80, buf, len,
                              async_callback, &ctx, timeout);
    t->flags |= LIBUSB_TRANSFER_FREE_TRANSFER;
    int r = libusb_submit_transfer(t);

    if (r)
    {
        wrapLogError("cannot async bulk read from endpoint 0x%02X: %s",
                     num, strMsg(r));
        int _r = retMsg(r);
        ctx.set(_r, 0);
        return _r;
    }

    return USBWRAP_SUCCESS;
}

int USBWrapper_t::bulkWrite(uint8_t num, const uint8_t *buf,
                     uint32_t len, uint32_t timeout)
{
    ASSERT_OBJ(m_handle, "cannot bulk write: device is not opened");

    int l = len;
    int r = libusb_bulk_transfer(m_handle, num & ~0x80, (uint8_t*)buf,
                                 len, &l, timeout);
    if (r != LIBUSB_SUCCESS)
    {
        wrapLogError("cannot bulk write to endpoint 0x%02X: %s", num, strMsg(r));
        return retMsg(r);
    }
    return len;
}

int USBWrapper_t::clearStall(uint8_t num)
{
    ASSERT_OBJ(m_handle, "cannot clear stall: device is not opened");

    int r = libusb_clear_halt(m_handle, num);
    if (r != LIBUSB_SUCCESS)
    {
        wrapLogError("cannot clear stall of endpoint 0x%02X: %s",
                     num, strMsg(r));
    }
    return retMsg(r);
}

int USBWrapper_t::abort(uint8_t num)
{
    // stub yet
    return USBWRAP_SUCCESS;
}

int USBWrapper_t::abortControl()
{
    // stub yet
    return USBWRAP_SUCCESS;
}
