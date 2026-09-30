/* USB EHCI Host for Teensy 3.6
 * Copyright 2017 Paul Stoffregen (paul@pjrc.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef USB_HOST_TEENSY36_
#define USB_HOST_TEENSY36_

#include <stdint.h>
#include <FS.h>

#if !defined(__MK66FX1M0__) && !defined(__IMXRT1052__) && !defined(__IMXRT1062__)
#error "USBHost_t36 only works with Teensy 3.6 or Teensy 4.x.  Please select it in Tools > Boards"
#endif
#include "utility/imxrt_usbhs.h"
#include "utility/msc.h"

// Dear inquisitive reader, USB is a complex protocol defined with
// very specific terminology.  To have any chance of understand this
// source code, you absolutely must have solid knowledge of specific
// USB terms such as host, device, endpoint, pipe, enumeration....
// You really must also have at least a basic knowledge of the
// different USB transfers: control, bulk, interrupt, isochronous.
//
// The USB 2.0 specification explains these in chapter 4 (pages 15
// to 24), and provides more detail in the first part of chapter 5
// (pages 25 to 55).  The USB spec is published for free at
// www.usb.org.  Here is a convenient link to just the main PDF:
//
// https://www.pjrc.com/teensy/beta/usb20.pdf
//
// This is a huge file, but chapter 4 is short and easy to read.
// If you're not familiar with the USB lingo, please do yourself
// a favor by reading at least chapter 4 to get up to speed on the
// meaning of these important USB concepts and terminology.
//
// If you wish to ask questions (which belong on the forum, not
// github issues) or discuss development of this library, you
// ABSOLUTELY MUST know the basic USB terminology from chapter 4.
// Please repect other people's valuable time & effort by making
// your best effort to read chapter 4 before asking USB questions!


// Uncomment this line to see lots of debugging info!
//#define USBHOST_PRINT_DEBUG


// This can let you control where to send the debugging messages
//#define USBHDBGSerial Serial1
#ifndef USBHDBGSerial
#define USBHDBGSerial   Serial
#endif


/************************************************/
/*  Data Types                                  */
/************************************************/

// These 6 types are the key to understanding how this USB Host
// library really works.

// USBHost is a static class controlling the hardware.
// All common USB functionality is implemented here.
class USBHost;

// These 3 structures represent the actual USB entities
// USBHost manipulates.  One Device_t is created for
// each active USB device.  One Pipe_t is create for
// each endpoint.  Transfer_t structures are created
// when any data transfer is added to the EHCI work
// queues, and then returned to the free pool after the
// data transfer completes and the driver has processed
// the results.
typedef struct Device_struct       Device_t;
typedef struct Pipe_struct         Pipe_t;
typedef struct Transfer_struct     Transfer_t;
typedef enum { CLAIM_NO = 0, CLAIM_REPORT, CLAIM_INTERFACE} hidclaim_t;

// All USB device drivers inherit use these classes.
// Drivers build user-visible functionality on top
// of these classes, which receive USB events from
// USBHost.
class USBDriver;
class USBDriverTimer;
class USBHIDInput;

/************************************************/
/*  Added Defines                               */
/************************************************/
// Keyboard special Keys
#define KEYD_UP         0xDA
#define KEYD_DOWN       0xD9
#define KEYD_LEFT       0xD8
#define KEYD_RIGHT      0xD7
#define KEYD_INSERT     0xD1
#define KEYD_DELETE     0xD4
#define KEYD_PAGE_UP    0xD3
#define KEYD_PAGE_DOWN  0xD6
#define KEYD_HOME       0xD2
#define KEYD_END        0xD5
#define KEYD_F1         0xC2
#define KEYD_F2         0xC3
#define KEYD_F3         0xC4
#define KEYD_F4         0xC5
#define KEYD_F5         0xC6
#define KEYD_F6         0xC7
#define KEYD_F7         0xC8
#define KEYD_F8         0xC9
#define KEYD_F9         0xCA
#define KEYD_F10        0xCB
#define KEYD_F11        0xCC
#define KEYD_F12        0xCD


// USBSerial formats - Lets encode format into bits
// Bits: 0-4 - Number of data bits
// Bits: 5-7 - Parity (0=none, 1=odd, 2 = even)
// bits: 8-9 - Stop bits. 0=1, 1=2


#define USBHOST_SERIAL_7E1 0x047
#define USBHOST_SERIAL_7O1 0x027
#define USBHOST_SERIAL_8N1 0x08
#define USBHOST_SERIAL_8N2 0x108
#define USBHOST_SERIAL_8E1 0x048
#define USBHOST_SERIAL_8O1 0x028



/************************************************/
/*  Data Structure Definitions                  */
/************************************************/

// setup_t holds the 8 byte USB SETUP packet data.
// These unions & structs allow convenient access to
// the setup fields.
typedef union {
    struct {
        union {
            struct {
                uint8_t bmRequestType;
                uint8_t bRequest;
            };
            uint16_t wRequestAndType;
        };
        uint16_t wValue;
        uint16_t wIndex;
        uint16_t wLength;
    };
    struct {
        uint32_t word1;
        uint32_t word2;
    };
} setup_t;

typedef struct {
    enum {STRING_BUF_SIZE = 50};
    enum {STR_ID_MAN = 0, STR_ID_PROD, STR_ID_SERIAL, STR_ID_CNT};
    uint8_t iStrings[STR_ID_CNT];   // Index into array for the three indexes
    uint8_t buffer[STRING_BUF_SIZE];
} strbuf_t;

#define DEVICE_STRUCT_STRING_BUF_SIZE 50

// Device_t holds all the information about a USB device
struct Device_struct {
    Pipe_t   *control_pipe;
    Pipe_t   *data_pipes;
    Device_t *next;
    USBDriver *drivers;
    strbuf_t *strbuf;
    uint8_t  speed; // 0=12, 1=1.5, 2=480 Mbit/sec
    uint8_t  address;
    uint8_t  hub_address;
    uint8_t  hub_port;
    uint8_t  enum_state;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t LanguageID;
};

// Pipe_t holes all information about each USB endpoint/pipe
// The first half is an EHCI QH structure for the pipe.
struct Pipe_struct {
    // Queue Head (QH), EHCI page 46-50
    struct {  // must be aligned to 32 byte boundary
        volatile uint32_t horizontal_link;
        volatile uint32_t capabilities[2];
        volatile uint32_t current;
        volatile uint32_t next;
        volatile uint32_t alt_next;
        volatile uint32_t token;
        volatile uint32_t buffer[5];
    } qh;
    Device_t *device;
    uint8_t  type; // 0=control, 1=isochronous, 2=bulk, 3=interrupt
    uint8_t  direction; // 0=out, 1=in (changes for control, others fixed)
    uint8_t  start_mask;
    uint8_t  complete_mask;
    Pipe_t   *next;
    void     (*callback_function)(const Transfer_t *);
    uint16_t periodic_interval;
    uint16_t periodic_offset;
    uint16_t bandwidth_interval;
    uint16_t bandwidth_offset;
    uint16_t bandwidth_shift;
    uint8_t  bandwidth_stime;
    uint8_t  bandwidth_ctime;
    uint32_t unused1;
    uint32_t unused2;
    uint32_t unused3;
    uint32_t unused4;
    uint32_t unused5;
};

// Transfer_t represents a single transaction on the USB bus.
// The first portion is an EHCI qTD structure.  Transfer_t are
// allocated as-needed from a memory pool, loaded with pointers
// to the actual data buffers, linked into a followup list,
// and placed on ECHI Queue Heads.  When the ECHI interrupt
// occurs, the followup lists are used to find the Transfer_t
// in memory.  Callbacks are made, and then the Transfer_t are
// returned to the memory pool.
struct Transfer_struct {
    // Queue Element Transfer Descriptor (qTD), EHCI pg 40-45
    struct {  // must be aligned to 32 byte boundary
        volatile uint32_t next;
        volatile uint32_t alt_next;
        volatile uint32_t token;
        volatile uint32_t buffer[5];
    } qtd;
    // Linked list of queued, not-yet-completed transfers
    Transfer_t *next_followup;
    Transfer_t *prev_followup;
    Pipe_t     *pipe;
    // Data to be used by callback function.  When a group
    // of Transfer_t are created, these fields and the
    // interrupt-on-complete bit in the qTD token are only
    // set in the last Transfer_t of the list.
    void       *buffer;
    uint32_t   length;
    setup_t    setup;
    USBDriver  *driver;
};


/************************************************/
/*  Main USB EHCI Controller                    */
/************************************************/

class USBHost {
public:
    static void begin();
    static void Task();
    static void countFree(uint32_t &devices, uint32_t &pipes, uint32_t &trans, uint32_t &strs);
protected:
    static Pipe_t * new_Pipe(Device_t *dev, uint32_t type, uint32_t endpoint,
                             uint32_t direction, uint32_t maxlen, uint32_t interval = 0);
    static bool queue_Control_Transfer(Device_t *dev, setup_t *setup,
                                       void *buf, USBDriver *driver);
    static bool queue_Data_Transfer(Pipe_t *pipe, void *buffer,
                                    uint32_t len, USBDriver *driver);
    static Device_t * new_Device(uint32_t speed, uint32_t hub_addr, uint32_t hub_port);
    static void disconnect_Device(Device_t *dev);
    static void enumeration(const Transfer_t *transfer);
    static void driver_ready_for_device(USBDriver *driver);
    static volatile bool enumeration_busy;
public: // Maybe others may want/need to contribute memory example HID devices may want to add transfers.
    static void contribute_Devices(Device_t *devices, uint32_t num);
    static void contribute_Pipes(Pipe_t *pipes, uint32_t num);
    static void contribute_Transfers(Transfer_t *transfers, uint32_t num);
    static void contribute_String_Buffers(strbuf_t *strbuf, uint32_t num);
private:
    static void isr();
    static void convertStringDescriptorToASCIIString(uint8_t string_index, Device_t *dev, const Transfer_t *transfer);
    static void claim_drivers(Device_t *dev);
    static uint32_t assign_address(void);
    static bool queue_Transfer(Pipe_t *pipe, Transfer_t *transfer);
    static void init_Device_Pipe_Transfer_memory(void);
    static Device_t * allocate_Device(void);
    static void delete_Pipe(Pipe_t *pipe);
    static void free_Device(Device_t *q);
    static Pipe_t * allocate_Pipe(void);
    static void free_Pipe(Pipe_t *q);
    static Transfer_t * allocate_Transfer(void);
    static void free_Transfer(Transfer_t *q);
    static strbuf_t * allocate_string_buffer(void);
    static void free_string_buffer(strbuf_t *strbuf);
    static bool allocate_interrupt_pipe_bandwidth(Pipe_t *pipe,
            uint32_t maxlen, uint32_t interval);
    static void add_qh_to_periodic_schedule(Pipe_t *pipe);
    static bool followup_Transfer(Transfer_t *transfer);
    static void followup_Error(void);
public: // Maybe others may want/need to contribute memory example HID devices may want to add transfers.
#ifdef USBHOST_PRINT_DEBUG
    static void print_(const Transfer_t *transfer);
    static void print_(const Transfer_t *first, const Transfer_t *last);
    static void print_token(uint32_t token);
    static void print_(const Pipe_t *pipe);
    static void print_driverlist(const char *name, const USBDriver *driver);
    static void print_qh_list(const Pipe_t *list);
    static void print_device_descriptor(const uint8_t *p);
    static void print_config_descriptor(const uint8_t *p, uint32_t maxlen);
    static void print_string_descriptor(const char *name, const uint8_t *p);
    static void print_hexbytes(const void *ptr, uint32_t len);
    static void print_(const char *s)   { USBHDBGSerial.print(s); }
    static void print_(int n)       { USBHDBGSerial.print(n); }
    static void print_(unsigned int n)  { USBHDBGSerial.print(n); }
    static void print_(long n)      { USBHDBGSerial.print(n); }
    static void print_(unsigned long n) { USBHDBGSerial.print(n); }
    static void println_(const char *s) { USBHDBGSerial.println(s); }
    static void println_(int n)     { USBHDBGSerial.println(n); }
    static void println_(unsigned int n)    { USBHDBGSerial.println(n); }
    static void println_(long n)        { USBHDBGSerial.println(n); }
    static void println_(unsigned long n)   { USBHDBGSerial.println(n); }
    static void println_()          { USBHDBGSerial.println(); }
    static void print_(uint32_t n, uint8_t b) { USBHDBGSerial.print(n, b); }
    static void println_(uint32_t n, uint8_t b) { USBHDBGSerial.println(n, b); }
    static void print_(const char *s, int n, uint8_t b = DEC) {
        USBHDBGSerial.print(s); USBHDBGSerial.print(n, b);
    }
    static void print_(const char *s, unsigned int n, uint8_t b = DEC) {
        USBHDBGSerial.print(s); USBHDBGSerial.print(n, b);
    }
    static void print_(const char *s, long n, uint8_t b = DEC) {
        USBHDBGSerial.print(s); USBHDBGSerial.print(n, b);
    }
    static void print_(const char *s, unsigned long n, uint8_t b = DEC) {
        USBHDBGSerial.print(s); USBHDBGSerial.print(n, b);
    }
    static void println_(const char *s, int n, uint8_t b = DEC) {
        USBHDBGSerial.print(s); USBHDBGSerial.println(n, b);
    }
    static void println_(const char *s, unsigned int n, uint8_t b = DEC) {
        USBHDBGSerial.print(s); USBHDBGSerial.println(n, b);
    }
    static void println_(const char *s, long n, uint8_t b = DEC) {
        USBHDBGSerial.print(s); USBHDBGSerial.println(n, b);
    }
    static void println_(const char *s, unsigned long n, uint8_t b = DEC) {
        USBHDBGSerial.print(s); USBHDBGSerial.println(n, b);
    }
    friend class USBDriverTimer; // for access to print & println
#else
    static void print_(const Transfer_t *transfer) {}
    static void print_(const Transfer_t *first, const Transfer_t *last) {}
    static void print_token(uint32_t token) {}
    static void print_(const Pipe_t *pipe) {}
    static void print_driverlist(const char *name, const USBDriver *driver) {}
    static void print_qh_list(const Pipe_t *list) {}
    static void print_device_descriptor(const uint8_t *p) {}
    static void print_config_descriptor(const uint8_t *p, uint32_t maxlen) {}
    static void print_string_descriptor(const char *name, const uint8_t *p) {}
    static void print_hexbytes(const void *ptr, uint32_t len) {}
    static void print_(const char *s) {}
    static void print_(int n) {}
    static void print_(unsigned int n) {}
    static void print_(long n) {}
    static void print_(unsigned long n) {}
    static void println_(const char *s) {}
    static void println_(int n) {}
    static void println_(unsigned int n) {}
    static void println_(long n) {}
    static void println_(unsigned long n) {}
    static void println_() {}
    static void print_(uint32_t n, uint8_t b) {}
    static void println_(uint32_t n, uint8_t b) {}
    static void print_(const char *s, int n, uint8_t b = DEC) {}
    static void print_(const char *s, unsigned int n, uint8_t b = DEC) {}
    static void print_(const char *s, long n, uint8_t b = DEC) {}
    static void print_(const char *s, unsigned long n, uint8_t b = DEC) {}
    static void println_(const char *s, int n, uint8_t b = DEC) {}
    static void println_(const char *s, unsigned int n, uint8_t b = DEC) {}
    static void println_(const char *s, long n, uint8_t b = DEC) {}
    static void println_(const char *s, unsigned long n, uint8_t b = DEC) {}
#endif
protected:
    static void mk_setup(setup_t &s, uint32_t bmRequestType, uint32_t bRequest,
                         uint32_t wValue, uint32_t wIndex, uint32_t wLength) {
        s.word1 = bmRequestType | (bRequest << 8) | (wValue << 16);
        s.word2 = wIndex | (wLength << 16);
    }
};


/************************************************/
/*  USB Device Driver Common Base Class         */
/************************************************/

// All USB device drivers inherit from this base class.
class USBDriver : public USBHost {
public:
    operator bool() {
        Device_t *dev = *(Device_t * volatile *)&device;
        return dev != nullptr;
    }
    uint16_t idVendor() {
        Device_t *dev = *(Device_t * volatile *)&device;
        return (dev != nullptr) ? dev->idVendor : 0;
    }
    uint16_t idProduct() {
        Device_t *dev = *(Device_t * volatile *)&device;
        return (dev != nullptr) ? dev->idProduct : 0;
    }
    const uint8_t *manufacturer() {
        Device_t *dev = *(Device_t * volatile *)&device;
        if (dev == nullptr || dev->strbuf == nullptr) return nullptr;
        return &dev->strbuf->buffer[dev->strbuf->iStrings[strbuf_t::STR_ID_MAN]];
    }
    const uint8_t *product() {
        Device_t *dev = *(Device_t * volatile *)&device;
        if (dev == nullptr || dev->strbuf == nullptr) return nullptr;
        return &dev->strbuf->buffer[dev->strbuf->iStrings[strbuf_t::STR_ID_PROD]];
    }
    const uint8_t *serialNumber() {
        Device_t *dev = *(Device_t * volatile *)&device;
        if (dev == nullptr || dev->strbuf == nullptr) return nullptr;
        return &dev->strbuf->buffer[dev->strbuf->iStrings[strbuf_t::STR_ID_SERIAL]];
    }
protected:
    USBDriver() : next(NULL), device(NULL) {}
    // Check if a driver wishes to claim a device or interface or group
    // of interfaces within a device.  When this function returns true,
    // the driver is considered bound or loaded for that device.  When
    // new devices are detected, enumeration.cpp calls this function on
    // all unbound driver objects, to give them an opportunity to bind
    // to the new device.
    //   device has its vid&pid, class/subclass fields initialized
    //   type is 0 for device level, 1 for interface level, 2 for IAD
    //   descriptors points to the specific descriptor data
    virtual bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len) = 0;

    // When an unknown (not chapter 9) control transfer completes, this
    // function is called for all drivers bound to the device.  Return
    // true means this driver originated this control transfer, so no
    // more drivers need to be offered an opportunity to process it.
    // This function is optional, only needed if the driver uses control
    // transfers and wishes to be notified when they complete.
    virtual void control(const Transfer_t *transfer) { }

    // When any of the USBDriverTimer objects a driver creates generates
    // a timer event, this function is called.
    virtual void timer_event(USBDriverTimer *whichTimer) { }

    // When the user calls USBHost::Task, this Task function for all
    // active drivers is called, so they may update state and/or call
    // any attached user callback functions.
    virtual void Task() { }

    // When a device disconnects from the USB, this function is called.
    // The driver must free all resources it allocated and update any
    // internal state necessary to deal with the possibility of user
    // code continuing to call its API.  However, pipes and transfers
    // are the handled by lower layers, so device drivers do not free
    // pipes they created or cancel transfers they had in progress.
    virtual void disconnect() = 0;

    // Drivers are managed by this single-linked list.  All inactive
    // (not bound to any device) drivers are linked from
    // available_drivers in enumeration.cpp.  When bound to a device,
    // drivers are linked from that Device_t drivers list.
    USBDriver *next;

    // The device this object instance is bound to.  In words, this
    // is the specific device this driver is using.  When not bound
    // to any device, this must be NULL.  Drivers may set this to
    // any non-NULL value if they are in a state where they do not
    // wish to claim any device or interface (eg, if getting data
    // from the HID parser).
    Device_t *device;
    friend class USBHost;
};

// Device drivers may create these timer objects to schedule a timer call
class USBDriverTimer {
public:
    USBDriverTimer() { }
    USBDriverTimer(USBDriver *d) : driver(d) { }

    void init(USBDriver *d) { driver = d; };
    void start(uint32_t microseconds);
    void stop();
    void *pointer;
    uint32_t integer;
    uint32_t started_micros; // testing only
private:
    USBDriver      *driver;
    uint32_t       usec;
    USBDriverTimer *next;
    USBDriverTimer *prev;
    friend class USBHost;
};

// Device drivers may inherit from this base class, if they wish to receive
// HID input data fully decoded by the USBHIDParser driver
class USBHIDParser;

class USBHIDInput {
public:
    operator bool() { return (mydevice != nullptr); }
    uint16_t idVendor() { return (mydevice != nullptr) ? mydevice->idVendor : 0; }
    uint16_t idProduct() { return (mydevice != nullptr) ? mydevice->idProduct : 0; }
    const uint8_t *manufacturer()
    {  return  ((mydevice == nullptr) || (mydevice->strbuf == nullptr)) ? nullptr : &mydevice->strbuf->buffer[mydevice->strbuf->iStrings[strbuf_t::STR_ID_MAN]]; }
    const uint8_t *product()
    {  return  ((mydevice == nullptr) || (mydevice->strbuf == nullptr)) ? nullptr : &mydevice->strbuf->buffer[mydevice->strbuf->iStrings[strbuf_t::STR_ID_PROD]]; }
    const uint8_t *serialNumber()
    {  return  ((mydevice == nullptr) || (mydevice->strbuf == nullptr)) ? nullptr : &mydevice->strbuf->buffer[mydevice->strbuf->iStrings[strbuf_t::STR_ID_SERIAL]]; }


private:
    virtual hidclaim_t claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage) = 0;
    virtual bool hid_process_in_data(const Transfer_t *transfer) {return false;}
    virtual bool hid_process_out_data(const Transfer_t *transfer) {return false;}
    virtual bool hid_process_control(const Transfer_t *transfer) {return false;}
    virtual void hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax) { }
    virtual void hid_input_data(uint32_t usage, int32_t value) { }
    virtual void hid_input_end() { }
    virtual void disconnect_collection(Device_t *dev) { }
    virtual void hid_timer_event(USBDriverTimer *whichTimer) { }
    USBHIDInput *next = NULL;
    friend class USBHIDParser;
    friend class BTHIDSupport;
protected:
    Device_t *mydevice = NULL;
};


// Device drivers may inherit from this base class, if they wish to receive
// HID input like data from Bluetooth HID device.
class BluetoothController;
class BluetoothConnection;

class BTHIDInput {
public:
    operator bool() { return (btdevice != nullptr); } // experiment to see if overriding makes sense here
    uint16_t idVendor() { return (btdevice != nullptr) ? btdevice->idVendor : 0; }
    uint16_t idProduct() { return (btdevice != nullptr) ? btdevice->idProduct : 0; }

    const uint8_t *manufacturer();
    const uint8_t *product();
    const uint8_t *serialNumber();

private:
    virtual bool claim_bluetooth(BluetoothController *driver, uint32_t bluetooth_class, uint8_t *remoteName) {return false;}
    // newer version that will allow called code to know when it is being called (0 - At the connect, 1 if NO HID...)
    // not sure if I should overload the return or not, but...
    virtual hidclaim_t claim_bluetooth(BluetoothConnection *btconnection, uint32_t bluetooth_class, uint8_t *remoteName, int type);
    virtual bool process_bluetooth_HID_data(const uint8_t *data, uint16_t length) {return false;}
    virtual void release_bluetooth() {};
    virtual bool remoteNameComplete(const uint8_t *remoteName) {return true;}
    virtual void connectionComplete(void) {};
    virtual void sdp_command_completed (bool success) {};

    virtual hidclaim_t bt_claim_collection(BluetoothConnection *btconnection, uint32_t bluetooth_class, uint32_t topusage) {return CLAIM_NO;}
    virtual void bt_hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax) {};
    virtual void bt_hid_input_data(uint32_t usage, int32_t value) {};
    virtual void bt_hid_input_end() {};
    virtual void bt_disconnect_collection(Device_t *dev) {};
    virtual void bt_hid_timer_event(USBDriverTimer *whichTimer) { }


    BTHIDInput *next = NULL;
    friend class BluetoothController;
    friend class BluetoothConnection;
    enum { TOPUSAGE_LIST_LEN = 6 };
    enum { USAGE_LIST_LEN = 24 };

protected:
    enum {SP_NEED_CONNECT = 0x1, SP_DONT_NEED_CONNECT = 0x02, SP_PS3_IDS = 0x4};
    uint8_t  special_process_required = 0;
    Device_t *btdevice = NULL;
    BluetoothConnection *btconnect = nullptr;

};


/************************************************/
/*  USB Device Drivers                          */
/************************************************/

class USBHub : public USBDriver {
public:
    USBHub(USBHost &host) : debouncetimer(this), resettimer(this) { init(); }
    USBHub(USBHost *host) : debouncetimer(this), resettimer(this) { init(); }
    // Hubs with more more than 7 ports are built from two tiers of hubs
    // using 4 or 7 port hub chips.  While the USB spec seems to allow
    // hubs to have up to 255 ports, in practice all hub chips on the
    // market are only 2, 3, 4 or 7 ports.
    enum { MAXPORTS = 7 };
    typedef uint8_t portbitmask_t;
    enum {
        PORT_OFF =        0,
        PORT_DISCONNECT = 1,
        PORT_DEBOUNCE1 =  2,
        PORT_DEBOUNCE2 =  3,
        PORT_DEBOUNCE3 =  4,
        PORT_DEBOUNCE4 =  5,
        PORT_DEBOUNCE5 =  6,
        PORT_RESET =      7,
        PORT_RECOVERY =   8,
        PORT_ACTIVE =     9
    };
protected:
    virtual bool claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len);
    virtual void control(const Transfer_t *transfer);
    virtual void timer_event(USBDriverTimer *whichTimer);
    virtual void disconnect();
    void init();
    bool can_send_control_now();
    void send_poweron(uint32_t port);
    void send_getstatus(uint32_t port);
    void send_clearstatus_connect(uint32_t port);
    void send_clearstatus_enable(uint32_t port);
    void send_clearstatus_suspend(uint32_t port);
    void send_clearstatus_overcurrent(uint32_t port);
    void send_clearstatus_reset(uint32_t port);
    void send_setreset(uint32_t port);
    void send_setinterface();
    static void callback(const Transfer_t *transfer);
    void status_change(const Transfer_t *transfer);
    void new_port_status(uint32_t port, uint32_t status);
    void start_debounce_timer(uint32_t port);
    void stop_debounce_timer(uint32_t port);
private:
    Device_t mydevices[MAXPORTS];
    Pipe_t mypipes[2] __attribute__ ((aligned(32)));
    Transfer_t mytransfers[4] __attribute__ ((aligned(32)));
    strbuf_t mystring_bufs[1];
    USBDriverTimer debouncetimer;
    USBDriverTimer resettimer;
    setup_t setup;
    Pipe_t *changepipe;
    Device_t *devicelist[MAXPORTS];
    uint32_t changebits;
    uint32_t statusbits;
    uint8_t  hub_desc[16];
    uint8_t  interface_count;
    uint8_t  interface_number;
    uint8_t  altsetting;
    uint8_t  protocol;
    uint8_t  endpoint;
    uint8_t  interval;
    uint8_t  numports;
    uint8_t  characteristics;
    uint8_t  powertime;
    uint8_t  sending_control_transfer;
    uint8_t  port_doing_reset;
    uint8_t  port_doing_reset_speed;
    uint8_t  portstate[MAXPORTS];
    portbitmask_t send_pending_poweron;
    portbitmask_t send_pending_getstatus;
    portbitmask_t send_pending_clearstatus_connect;
    portbitmask_t send_pending_clearstatus_enable;
    portbitmask_t send_pending_clearstatus_suspend;
    portbitmask_t send_pending_clearstatus_overcurrent;
    portbitmask_t send_pending_clearstatus_reset;
    portbitmask_t send_pending_setreset;
    portbitmask_t debounce_in_use;
    static volatile bool reset_busy;
};

//--------------------------------------------------------------------------


class USBHIDParser : public USBDriver {
public:
    USBHIDParser(USBHost &host) : hidTimer(this) { init(); }
    static void driver_ready_for_hid_collection(USBHIDInput *driver);
	bool sendPacket(const uint8_t *buffer, int cb=-1);
	void setTXBuffers(uint8_t *buffer1, uint8_t *buffer2, uint8_t cb, 
		// extended to optionaly allow more buffers. 
		uint8_t *buffer3=nullptr, uint8_t* buffer4=nullptr);
	void setRXBuffers(uint8_t *buffer1, uint8_t *buffer2, uint8_t cb,
		// extended to optionaly allow more buffers. 
		uint8_t *buffer3=nullptr, uint8_t* buffer4=nullptr);

    bool sendControlPacket(uint32_t bmRequestType, uint32_t bRequest,
                           uint32_t wValue, uint32_t wIndex, uint32_t wLength, void *buf);

    // Atempt for RAWhid and SEREMU to take over processing of data
    //
    uint16_t inSize(void) {return in_size;}
    uint16_t outSize(void) {return out_size;}
    uint8_t interfaceSubClass(void) { return bInterfaceSubClass; }
    uint8_t interfaceProtocol(void) { return bInterfaceProtocol; }
    void startTimer(uint32_t microseconds) {hidTimer.start(microseconds);}
    void stopTimer() {hidTimer.stop();}
    uint8_t interfaceNumber() { return bInterfaceNumber;}
    const uint8_t * getHIDReportDescriptor() {return _bigBuffer;}
    uint16_t getHIDReportDescriptorSize() { return descsize;}
protected:
    enum { TOPUSAGE_LIST_LEN = 6 };
    enum { USAGE_LIST_LEN = 24 };
    virtual bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len);
    virtual void control(const Transfer_t *transfer);
    virtual void disconnect();
    static void in_callback(const Transfer_t *transfer);
    static void out_callback(const Transfer_t *transfer);
    virtual void timer_event(USBDriverTimer *whichTimer);
    void in_data(const Transfer_t *transfer);
    void out_data(const Transfer_t *transfer);
    bool check_if_using_report_id();
    void parse();
    USBHIDInput * find_driver(uint32_t topusage);
    void parse(uint16_t type_and_report_id, const uint8_t *data, uint32_t len);
    void init();


	uint8_t activeSendMask(void) {return _tx_state;} 

private:
    Pipe_t *in_pipe;
    Pipe_t *out_pipe;
    static USBHIDInput *available_hid_drivers_list;
    //uint32_t topusage_list[TOPUSAGE_LIST_LEN];
    USBHIDInput *topusage_drivers[TOPUSAGE_LIST_LEN];
    uint16_t in_size;
    uint16_t out_size;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    setup_t setup;
    uint8_t report[64];
    uint8_t report2[64];
    uint16_t descsize;
    bool use_report_id;
    Pipe_t mypipes[3] __attribute__ ((aligned(32)));
    Transfer_t mytransfers[5] __attribute__ ((aligned(32)));
    strbuf_t mystring_bufs[1];
	uint8_t *_rx1 = nullptr;
	uint8_t *_rx2 = nullptr;
	uint8_t *_rx3 = nullptr;
	uint8_t *_rx4 = nullptr;
	uint8_t *_tx[4] = {nullptr, nullptr, nullptr, nullptr};
	uint8_t _tx_state = 0;
	uint8_t _tx_mask = 3;
    bool hid_driver_claimed_control_ = false;
    USBDriverTimer hidTimer;
	uint8_t _bigBuffer[800 + 64+64];
	uint8_t *_bigBufferEnd = _bigBuffer + sizeof(_bigBuffer);
	uint16_t _big_buffer_size = sizeof(_bigBuffer);
    uint8_t bInterfaceNumber = 0;
};

//--------------------------------------------------------------------------

class KeyboardController : public USBHIDInput, public BTHIDInput {
public:
    typedef union {
        struct {
            uint8_t numLock : 1;
            uint8_t capsLock : 1;
            uint8_t scrollLock : 1;
            uint8_t compose : 1;
            uint8_t kana : 1;
            uint8_t reserved : 3;
        };
        uint8_t byte;
    } KBDLeds_t;
public:
    KeyboardController(USBHost &host) { init(); }
    KeyboardController(USBHost *host) { init(); }
    // need their own versions as both USBDriver and USBHIDInput provide
    uint16_t idVendor();
    uint16_t idProduct();
    const uint8_t *manufacturer();
    const uint8_t *product();
    const uint8_t *serialNumber();

    operator bool() { return ((btdevice != nullptr) || (mydevice != nullptr)); }
    // Main boot keyboard functions.
    uint16_t getKey() { return keyCode; }
    uint8_t  getModifiers() { return modifiers_; }
    uint8_t  getOemKey() { return keyOEM_; }
    void     attachPress(void (*f)(int unicode)) { keyPressedFunction = f; }
    void     attachRelease(void (*f)(int unicode)) { keyReleasedFunction = f; }
    void     attachRawPress(void (*f)(uint8_t keycode)) { rawKeyPressedFunction = f; }
    void     attachRawRelease(void (*f)(uint8_t keycode)) { rawKeyReleasedFunction = f; }
    void     LEDS(uint8_t leds);
    uint8_t  LEDS() {return leds_.byte;}
    void     updateLEDS(void);
    bool     numLock() {return leds_.numLock;}
    bool     capsLock() {return leds_.capsLock;}
    bool     scrollLock() {return leds_.scrollLock;}
    void     numLock(bool f);
    void     capsLock(bool f);
    void     scrollLock(bool f);

    // return battery level in percentage.  0xff implies we don't know.
    uint8_t  batteryLevel() {return battery_level_;}
    // Added for extras information.
    void     attachExtrasPress(void (*f)(uint32_t top, uint16_t code)) { extrasKeyPressedFunction = f; }
    void     attachExtrasRelease(void (*f)(uint32_t top, uint16_t code)) { extrasKeyReleasedFunction = f; }
    void     forceBootProtocol();
    void     forceHIDProtocol();
    enum {MAX_KEYS_DOWN = 4};

protected:
    void init();

    // Bluetooth data
    virtual hidclaim_t claim_bluetooth(BluetoothConnection *btconnection, uint32_t bluetooth_class, uint8_t *remoteName, int type);
    //virtual bool claim_bluetooth(BluetoothController *driver, uint32_t bluetooth_class, uint8_t *remoteName);
    virtual bool process_bluetooth_HID_data(const uint8_t *data, uint16_t length);
    virtual bool remoteNameComplete(const uint8_t *remoteName);
    virtual void release_bluetooth();
    virtual void connectionComplete(void);
    virtual void sdp_command_completed (bool success);


protected:  // HID functions for extra keyboard data.
    virtual hidclaim_t claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage);
    virtual void hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax);
    virtual void hid_input_data(uint32_t usage, int32_t value);
    virtual void hid_input_end();
    virtual void disconnect_collection(Device_t *dev);
    virtual bool hid_process_in_data(const Transfer_t *transfer);
    void process_boot_keyboard_format(const uint8_t *report, bool process_mod_keys);

    virtual hidclaim_t bt_claim_collection(BluetoothConnection *btconnection, uint32_t bluetooth_class, uint32_t topusage);
    virtual void bt_hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax);
    virtual void bt_hid_input_data(uint32_t usage, int32_t value);
    virtual void bt_hid_input_end();
    virtual void bt_disconnect_collection(Device_t *dev);

private:
    void update();
    uint16_t convert_to_unicode(uint32_t mod, uint32_t key);
    void key_press(uint32_t mod, uint32_t key);
    void key_release(uint32_t mod, uint32_t key);
    bool process_hid_keyboard_data(uint32_t usage, int32_t value);
    void (*keyPressedFunction)(int unicode);
    void (*keyReleasedFunction)(int unicode);
    void (*rawKeyPressedFunction)(uint8_t keycode) = nullptr;
    void (*rawKeyReleasedFunction)(uint8_t keycode) = nullptr;
    Pipe_t *datapipe;
    setup_t setup;
    // Need two sets of structures to properly support some keyboards
    // that do N key roll-over.  They use the Boot report up to
    // 6 keys down and then they go to other format for additional
    // keys.
    // Boot format
    uint8_t report_[8];
    uint8_t prev_report_[8];

    // N Key reollover
    uint8_t key_states_[16];

    uint16_t keyCode;
    uint8_t modifiers_ = 0;
    uint8_t keyOEM_;

    KBDLeds_t leds_ = {0};

    // Added to process secondary HID data.
    void (*extrasKeyPressedFunction)(uint32_t top, uint16_t code);
    void (*extrasKeyReleasedFunction)(uint32_t top, uint16_t code);
    uint32_t topusage_ = 0;                 // What top report am I processing?
    uint32_t topusage_type_ = 0;
    int lgmin_ = 0;
    int lgmax_ = 0;
    uint32_t topusage_index_ = 0;
    uint8_t collections_claimed_ = 0;
    bool keyboard_uses_boot_format_  = false;
    volatile bool hid_input_begin_ = false;
    volatile bool hid_input_data_ = false;  // did we receive any valid data with report?

    uint8_t battery_level_ = 0xff; // battery level percent 0xff is we don't know

    uint8_t count_keys_down_ = 0;
    uint16_t keys_down[MAX_KEYS_DOWN];
    bool    force_boot_protocol;  // User or VID/PID said force boot protocol?
    bool control_queued = false;
    // keep back pointer for the three different op levels we claim
    BluetoothController *btdriver_ = nullptr;
    USBHIDParser *driver_[3] = {nullptr, nullptr, nullptr};
    static bool s_forceHIDMode;

    // Test probably temporary Bluetooth HID support object
    //BTHIDSupport bthids_;

};


class MouseController : public USBHIDInput, public BTHIDInput {
public:
    MouseController(USBHost &host) { init(); }
    bool    available() { return mouseEvent; }
    void    mouseDataClear();
    uint8_t getButtons() { return buttons; }
    int     getMouseX() { return mouseX; }
    int     getMouseY() { return mouseY; }
    int     getWheel() { return wheel; }
    int     getWheelH() { return wheelH; }
protected:
    virtual hidclaim_t claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage);
    virtual void hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax);
    virtual void hid_input_data(uint32_t usage, int32_t value);
    virtual void hid_input_end();
    virtual void disconnect_collection(Device_t *dev);

    // Bluetooth data
    virtual hidclaim_t claim_bluetooth(BluetoothConnection *btconnection, uint32_t bluetooth_class, uint8_t *remoteName, int type);
    //virtual bool claim_bluetooth(BluetoothController *driver, uint32_t bluetooth_class, uint8_t *remoteName);
    virtual bool process_bluetooth_HID_data(const uint8_t *data, uint16_t length);
    virtual void release_bluetooth();

    virtual hidclaim_t bt_claim_collection(BluetoothConnection *btconnection, uint32_t bluetooth_class, uint32_t topusage);
    virtual void bt_hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax);
    virtual void bt_hid_input_data(uint32_t usage, int32_t value);
    virtual void bt_hid_input_end();
    virtual void bt_disconnect_collection(Device_t *dev);


private:
    void init();
    BluetoothController *btdriver_ = nullptr;

    uint8_t collections_claimed = 0;
    volatile bool mouseEvent = false;
    volatile bool hid_input_begin_ = false;
    uint8_t buttons = 0;
    int     mouseX = 0;
    int     mouseY = 0;
    int     wheel = 0;
    int     wheelH = 0;
};

//--------------------------------------------------------------------------

class DigitizerController : public USBHIDInput, public BTHIDInput {
public:
    DigitizerController(USBHost &host) { init(); }
    bool    available() { return digitizerEvent; }
    void    digitizerDataClear();
    uint8_t getButtons() { return buttons; }
    int     getMouseX() { return mouseX; }
    int     getMouseY() { return mouseY; }
    int     getWheel() { return wheel; }
    int     getWheelH() { return wheelH; }
    int     getAxis(uint32_t index) { return (index < (sizeof(digiAxes) / sizeof(digiAxes[0]))) ? digiAxes[index] : 0; }

protected:
    virtual hidclaim_t claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage);
    virtual void hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax);
    virtual void hid_input_data(uint32_t usage, int32_t value);
    virtual void hid_input_end();
    virtual void disconnect_collection(Device_t *dev);


private:
    void init();

    uint8_t collections_claimed = 0;
    volatile bool digitizerEvent = false;
    volatile bool hid_input_begin_ = false;
    uint8_t buttons = 0;
    int     mouseX = 0;
    int     mouseY = 0;
    int     wheel = 0;
    int     wheelH = 0;
    int     digiAxes[16];
};


//--------------------------------------------------------------------------

class JoystickController : public USBDriver, public USBHIDInput, public BTHIDInput {
public:
    JoystickController(USBHost &host) { init(); }

    uint16_t idVendor();
    uint16_t idProduct();

    const uint8_t *manufacturer();
    const uint8_t *product();
    const uint8_t *serialNumber();
    operator bool() { return (((device != nullptr) || (mydevice != nullptr || (btdevice != nullptr))) && connected_); } // override as in both USBDriver and in USBHIDInput

    bool    available() { return joystickEvent; }
    void    joystickDataClear();

    // Returns the currently pressed buttons on the joystick
    uint32_t getButtons() { return buttons; }

    // Returns the HID Report ID
    uint8_t getReportID() { return report_id_;}

    // Returns the specified axis value
    int     getAxis(uint32_t index) { return (index < (sizeof(axis) / sizeof(axis[0]))) ? axis[index] : 0; }

    // Retuns bit mask showing which axis are defined for the current joystick
    uint64_t axisMask() {return axis_mask_;}

    // returns a bit mask showing which axis have changed since the last read. 
    uint64_t axisChangedMask() { return axis_changed_mask_;}
    uint64_t axisChangeNotifyMask() {return axis_change_notify_mask_;}
    void     axisChangeNotifyMask(uint64_t notify_mask) {axis_change_notify_mask_ = notify_mask;}

    // set functions functionality depends on underlying joystick.
    bool setRumble(uint8_t lValue, uint8_t rValue, uint8_t timeout = 0xff);
    // setLEDs on PS4(RGB), PS3 simple LED setting (only uses lb)
    bool setLEDs(uint8_t lr, uint8_t lg, uint8_t lb);  // sets Leds,
    bool inline setLEDs(uint32_t leds) {return setLEDs((leds >> 16) & 0xff, (leds >> 8) & 0xff, leds & 0xff);}  // sets Leds - passing one arg for all leds
    enum { STANDARD_AXIS_COUNT = 10, ADDITIONAL_AXIS_COUNT = 54, TOTAL_AXIS_COUNT = (STANDARD_AXIS_COUNT + ADDITIONAL_AXIS_COUNT) };
    typedef enum { UNKNOWN = 0, PS3, PS4, XBOXONE, XBOX360, PS3_MOTION, SpaceNav, SWITCH} joytype_t;
    joytype_t joystickType() {return joystickType_;}

    // PS3 pair function. hack, requires that it be connect4ed by USB and we have the address of the Bluetooth dongle...
    bool PS3Pair(uint8_t* bdaddr);

    bool PS4GetCurrentPairing(uint8_t* bdaddr);
    bool PS4Pair(uint8_t* bdaddr);
	
	void sw_sendCmd(uint8_t cmd, uint8_t *data, uint16_t size, uint32_t timeout=0);
	bool sw_getIMUCalValues(float *accel, float *gyro);

protected:
    // From USBDriver
    virtual bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len);
    virtual void control(const Transfer_t *transfer);
    virtual void disconnect();

    // From USBHIDInput
    virtual hidclaim_t claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage);
    virtual void hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax);
    virtual void hid_input_data(uint32_t usage, int32_t value);
    virtual bool hid_process_control(const Transfer_t *transfer);
    virtual void hid_input_end();
    virtual void disconnect_collection(Device_t *dev);
    virtual bool hid_process_out_data(const Transfer_t *transfer);
    virtual bool hid_process_in_data(const Transfer_t *transfer);
    virtual void hid_timer_event(USBDriverTimer *whichTimer);

    // Bluetooth data
    virtual hidclaim_t claim_bluetooth(BluetoothConnection *btconnection, uint32_t bluetooth_class, uint8_t *remoteName, int type);
    //virtual bool claim_bluetooth(BluetoothController *driver, uint32_t bluetooth_class, uint8_t *remoteName);
    virtual bool process_bluetooth_HID_data(const uint8_t *data, uint16_t length);
    virtual void release_bluetooth();
    virtual bool remoteNameComplete(const uint8_t *remoteName);
    virtual void connectionComplete(void);

    virtual hidclaim_t bt_claim_collection(BluetoothConnection *btconnection, uint32_t bluetooth_class, uint32_t topusage);
    virtual void bt_hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax);
    virtual void bt_hid_input_data(uint32_t usage, int32_t value);
    virtual void bt_hid_input_end();
    virtual void bt_disconnect_collection(Device_t *dev);
    virtual void bt_hid_timer_event(USBDriverTimer *whichTimer);



    joytype_t joystickType_ = UNKNOWN;
private:
    static bool queue_Data_Transfer_Debug(Pipe_t *pipe, void *buffer,
                                    uint32_t len, USBDriver *driver,
                                    uint32_t line);

    // Class specific
    void init();
    USBHIDParser *driver_ = nullptr;
    BluetoothController *btdriver_ = nullptr;

    joytype_t mapVIDPIDtoJoystickType(uint16_t idVendor, uint16_t idProduct, bool exclude_hid_devices);
    bool transmitPS4UserFeedbackMsg();
    bool transmitPS3UserFeedbackMsg();
    bool transmitPS3MotionUserFeedbackMsg();
    bool mapNameToJoystickType(const uint8_t *remoteName);
    //void sw_sendCmd(uint8_t cmd, uint8_t *data, uint16_t size);
	//void sw_sendCmdUSB(uint8_t cmd, uint8_t *data, uint8_t size);
    void sw_sendCmdUSB(uint8_t cmd, uint32_t timeout);
	void sw_sendSubCmdUSB(uint8_t sub_cmd, uint8_t *data, uint8_t size, uint32_t timeout = 0);
	void sw_parseAckMsg(const uint8_t *buf_);
    bool sw_handle_usb_init_of_joystick(uint8_t *buffer, uint16_t cb, bool timer_event);
    bool sw_handle_bt_init_of_joystick(const uint8_t *data, uint16_t length, bool timer_event);
    inline void sw_update_axis(uint8_t axis_index, int new_value);
    bool sw_process_HID_data(const uint8_t *data, uint16_t length);
	
	void CalcAnalogStick(float &pOutX, float &pOutY, int16_t x, int16_t y, bool isLeft);
	
	//kludge for switch having different button values
	bool initialPass_ = true;
	bool initialPassButton_ = true;
	bool initialPassBT_ = true;
	uint32_t buttonOffset_ = 0x00;
	
    uint8_t report_id_ = 0;
    bool anychange = false;
    volatile bool joystickEvent = false;
    uint32_t buttons = 0;
    int axis[TOTAL_AXIS_COUNT] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint64_t axis_mask_ = 0;    // which axis have valid data
    uint64_t axis_changed_mask_ = 0;
    uint64_t axis_change_notify_mask_ = 0x3ff;  // assume the low 10 values only.

    uint16_t additional_axis_usage_page_ = 0;
    uint16_t additional_axis_usage_start_ = 0;
    uint16_t additional_axis_usage_count_ = 0;

    // State values to output to Joystick.
    uint8_t rumble_lValue_ = 0;
    uint8_t rumble_rValue_ = 0;
    uint8_t rumble_timeout_ = 0;
    uint8_t leds_[3] = {0, 0, 0};
    uint8_t connected_ = 0; // what type of device if any is connected xbox 360...
    uint8_t connectedComplete_pending_ = 0;
    uint8_t sw_last_cmd_sent_ = 0;
    uint8_t sw_last_cmd_repeat_count = 0;
    enum {SW_CMD_TIMEOUT = 250000};
    elapsedMicros em_sw_;
    

    // Used by HID code
    uint8_t collections_claimed = 0;

    // Used by USBDriver code
    static void rx_callback(const Transfer_t *transfer);
    static void tx_callback(const Transfer_t *transfer);
    void rx_data(const Transfer_t *transfer);
    void tx_data(const Transfer_t *transfer);

    Pipe_t mypipes[3] __attribute__ ((aligned(32)));
    Transfer_t mytransfers[7] __attribute__ ((aligned(32)));
    strbuf_t mystring_bufs[1];

    uint8_t         rx_ep_ = 0; // remember which end point this object is...
    uint16_t        rx_size_ = 0;
    uint16_t        tx_size_ = 0;
    Pipe_t          *rxpipe_;
    Pipe_t          *txpipe_;
    uint8_t         rxbuf_[64]; // receive circular buffer
    uint8_t         txbuf_[64];     // buffer to use to send commands to joystick
    volatile bool       send_Control_packet_active_;
    // Mapping table to say which devices we handle
    typedef struct {
        uint16_t    idVendor;
        uint16_t    idProduct;
        joytype_t   joyType;
        bool        hidDevice;
    } product_vendor_mapping_t;
    static product_vendor_mapping_t pid_vid_mapping[];
};


//--------------------------------------------------------------------------

class MIDIDeviceBase : public USBDriver {
public:
    enum { SYSEX_MAX_LEN = 290 };

    // Message type names for compatibility with Arduino MIDI library 4.3.1
    enum MidiType {
        InvalidType           = 0x00, // For notifying errors
        NoteOff               = 0x80, // Note Off
        NoteOn                = 0x90, // Note On
        AfterTouchPoly        = 0xA0, // Polyphonic AfterTouch
        ControlChange         = 0xB0, // Control Change / Channel Mode
        ProgramChange         = 0xC0, // Program Change
        AfterTouchChannel     = 0xD0, // Channel (monophonic) AfterTouch
        PitchBend             = 0xE0, // Pitch Bend
        SystemExclusive       = 0xF0, // System Exclusive
        TimeCodeQuarterFrame  = 0xF1, // System Common - MIDI Time Code Quarter Frame
        SongPosition          = 0xF2, // System Common - Song Position Pointer
        SongSelect            = 0xF3, // System Common - Song Select
        TuneRequest           = 0xF6, // System Common - Tune Request
        Clock                 = 0xF8, // System Real Time - Timing Clock
        Start                 = 0xFA, // System Real Time - Start
        Continue              = 0xFB, // System Real Time - Continue
        Stop                  = 0xFC, // System Real Time - Stop
        ActiveSensing         = 0xFE, // System Real Time - Active Sensing
        SystemReset           = 0xFF, // System Real Time - System Reset
    };
    MIDIDeviceBase(USBHost &host, uint32_t *rx, uint32_t *tx1, uint32_t *tx2,
                   uint16_t bufsize, uint32_t *rqueue, uint16_t qsize) :
        txtimer(this), rx_buffer(rx), tx_buffer1(tx1), tx_buffer2(tx2),
        rx_queue(rqueue), max_packet_size(bufsize), rx_queue_size(qsize) {
        init();
    }
    void sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel, uint8_t cable = 0) {
        send(0x80, note, velocity, channel, cable);
    }
    void sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel, uint8_t cable = 0) {
        send(0x90, note, velocity, channel, cable);
    }
    void sendPolyPressure(uint8_t note, uint8_t pressure, uint8_t channel, uint8_t cable = 0) {
        send(0xA0, note, pressure, channel, cable);
    }
    void sendAfterTouchPoly(uint8_t note, uint8_t pressure, uint8_t channel, uint8_t cable = 0) {
        send(0xA0, note, pressure, channel, cable);
    }
    void sendControlChange(uint8_t control, uint8_t value, uint8_t channel, uint8_t cable = 0) {
        send(0xB0, control, value, channel, cable);
    }
    void sendProgramChange(uint8_t program, uint8_t channel, uint8_t cable = 0) {
        send(0xC0, program, 0, channel, cable);
    }
    void sendAfterTouch(uint8_t pressure, uint8_t channel, uint8_t cable = 0) {
        send(0xD0, pressure, 0, channel, cable);
    }
    void sendPitchBend(int value, uint8_t channel, uint8_t cable = 0) {
        if (value < -8192) {
            value = -8192;
        } else if (value > 8191) {
            value = 8191;
        }
        value += 8192;
        send(0xE0, value, value >> 7, channel, cable);
    }
    void sendSysEx(uint32_t length, const uint8_t *data, bool hasTerm = false, uint8_t cable = 0) {
        //if (cable >= MIDI_NUM_CABLES) return;
        if (hasTerm) {
            send_sysex_buffer_has_term(data, length, cable);
        } else {
            send_sysex_add_term_bytes(data, length, cable);
        }
    }
    void sendRealTime(uint8_t type, uint8_t cable = 0) {
        switch (type) {
        case 0xF8: // Clock
        case 0xFA: // Start
        case 0xFB: // Continue
        case 0xFC: // Stop
        case 0xFE: // ActiveSensing
        case 0xFF: // SystemReset
            send(type, 0, 0, 0, cable);
            break;
        default: // Invalid Real Time marker
            break;
        }
    }
    void sendTimeCodeQuarterFrame(uint8_t type, uint8_t value, uint8_t cable = 0) {
        send(0xF1, ((type & 0x07) << 4) | (value & 0x0F), 0, 0, cable);
    }
    void sendSongPosition(uint16_t beats, uint8_t cable = 0) {
        send(0xF2, beats, beats >> 7, 0, cable);
    }
    void sendSongSelect(uint8_t song, uint8_t cable = 0) {
        send(0xF3, song, 0, 0, cable);
    }
    void sendTuneRequest(uint8_t cable = 0) {
        send(0xF6, 0, 0, 0, cable);
    }
    void beginRpn(uint16_t number, uint8_t channel, uint8_t cable = 0) {
        sendControlChange(101, number >> 7, channel, cable);
        sendControlChange(100, number, channel, cable);
    }
    void sendRpnValue(uint16_t value, uint8_t channel, uint8_t cable = 0) {
        sendControlChange(6, value >> 7, channel, cable);
        sendControlChange(38, value, channel, cable);
    }
    void sendRpnIncrement(uint8_t amount, uint8_t channel, uint8_t cable = 0) {
        sendControlChange(96, amount, channel, cable);
    }
    void sendRpnDecrement(uint8_t amount, uint8_t channel, uint8_t cable = 0) {
        sendControlChange(97, amount, channel, cable);
    }
    void endRpn(uint8_t channel, uint8_t cable = 0) {
        sendControlChange(101, 0x7F, channel, cable);
        sendControlChange(100, 0x7F, channel, cable);
    }
    void beginNrpn(uint16_t number, uint8_t channel, uint8_t cable = 0) {
        sendControlChange(99, number >> 7, channel, cable);
        sendControlChange(98, number, channel, cable);
    }
    void sendNrpnValue(uint16_t value, uint8_t channel, uint8_t cable = 0) {
        sendControlChange(6, value >> 7, channel, cable);
        sendControlChange(38, value, channel, cable);
    }
    void sendNrpnIncrement(uint8_t amount, uint8_t channel, uint8_t cable = 0) {
        sendControlChange(96, amount, channel, cable);
    }
    void sendNrpnDecrement(uint8_t amount, uint8_t channel, uint8_t cable = 0) {
        sendControlChange(97, amount, channel, cable);
    }
    void endNrpn(uint8_t channel, uint8_t cable = 0) {
        sendControlChange(99, 0x7F, channel, cable);
        sendControlChange(98, 0x7F, channel, cable);
    }
    void send(uint8_t type, uint8_t data1, uint8_t data2, uint8_t channel, uint8_t cable = 0) {
        //if (cable >= MIDI_NUM_CABLES) return;
        if (type < 0xF0) {
            if (type < 0x80) return;
            type &= 0xF0;
            write_packed((type << 8) | (type >> 4) | ((cable & 0x0F) << 4)
                         | (((channel - 1) & 0x0F) << 8) | ((data1 & 0x7F) << 16)
                         | ((data2 & 0x7F) << 24));
        } else if (type >= 0xF8 || type == 0xF6) {
            write_packed((type << 8) | 0x0F | ((cable & 0x0F) << 4));
        } else if (type == 0xF1 || type == 0xF3) {
            write_packed((type << 8) | 0x02 | ((cable & 0x0F) << 4)
                         | ((data1 & 0x7F) << 16));
        } else if (type == 0xF2) {
            write_packed((type << 8) | 0x03 | ((cable & 0x0F) << 4)
                         | ((data1 & 0x7F) << 16) | ((data2 & 0x7F) << 24));
        }
    }
    void send_now(void) __attribute__((always_inline)) {
    }
    bool read(uint8_t channel = 0);
    uint8_t getType(void) {
        return msg_type;
    };
    uint8_t getCable(void) {
        return msg_cable;
    }
    uint8_t getChannel(void) {
        return msg_channel;
    };
    uint8_t getData1(void) {
        return msg_data1;
    };
    uint8_t getData2(void) {
        return msg_data2;
    };
    uint8_t * getSysExArray(void) {
        return msg_sysex;
    }
    uint16_t getSysExArrayLength(void) {
        return msg_data2 << 8 | msg_data1;
    }
    void setHandleNoteOff(void (*fptr)(uint8_t channel, uint8_t note, uint8_t velocity)) {
        // type: 0x80  NoteOff
        handleNoteOff = fptr;
    }
    void setHandleNoteOn(void (*fptr)(uint8_t channel, uint8_t note, uint8_t velocity)) {
        // type: 0x90  NoteOn
        handleNoteOn = fptr;
    }
    void setHandleVelocityChange(void (*fptr)(uint8_t channel, uint8_t note, uint8_t velocity)) {
        // type: 0xA0  AfterTouchPoly
        handleVelocityChange = fptr;
    }
    void setHandleAfterTouchPoly(void (*fptr)(uint8_t channel, uint8_t note, uint8_t pressure)) {
        // type: 0xA0  AfterTouchPoly
        handleVelocityChange = fptr;
    }
    void setHandleControlChange(void (*fptr)(uint8_t channel, uint8_t control, uint8_t value)) {
        // type: 0xB0  ControlChange
        handleControlChange = fptr;
    }
    void setHandleProgramChange(void (*fptr)(uint8_t channel, uint8_t program)) {
        // type: 0xC0  ProgramChange
        handleProgramChange = fptr;
    }
    void setHandleAfterTouch(void (*fptr)(uint8_t channel, uint8_t pressure)) {
        // type: 0xD0  AfterTouchChannel
        handleAfterTouch = fptr;
    }
    void setHandleAfterTouchChannel(void (*fptr)(uint8_t channel, uint8_t pressure)) {
        // type: 0xD0  AfterTouchChannel
        handleAfterTouch = fptr;
    }
    void setHandlePitchChange(void (*fptr)(uint8_t channel, int pitch)) {
        // type: 0xE0  PitchBend
        handlePitchChange = fptr;
    }
    void setHandleSysEx(void (*fptr)(const uint8_t *data, uint16_t length, bool complete)) {
        // type: 0xF0  SystemExclusive - multiple calls for message bigger than buffer
        handleSysExPartial = (void (*)(const uint8_t *, uint16_t, uint8_t))fptr;
    }
    void setHandleSystemExclusive(void (*fptr)(const uint8_t *data, uint16_t length, bool complete)) {
        // type: 0xF0  SystemExclusive - multiple calls for message bigger than buffer
        handleSysExPartial = (void (*)(const uint8_t *, uint16_t, uint8_t))fptr;
    }
    void setHandleSystemExclusive(void (*fptr)(uint8_t *data, unsigned int size)) {
        // type: 0xF0  SystemExclusive - single call, message larger than buffer is truncated
        handleSysExComplete = fptr;
    }
    void setHandleTimeCodeQuarterFrame(void (*fptr)(uint8_t data)) {
        // type: 0xF1  TimeCodeQuarterFrame
        handleTimeCodeQuarterFrame = fptr;
    }
    void setHandleSongPosition(void (*fptr)(uint16_t beats)) {
        // type: 0xF2  SongPosition
        handleSongPosition = fptr;
    }
    void setHandleSongSelect(void (*fptr)(uint8_t songnumber)) {
        // type: 0xF3  SongSelect
        handleSongSelect = fptr;
    }
    void setHandleTuneRequest(void (*fptr)(void)) {
        // type: 0xF6  TuneRequest
        handleTuneRequest = fptr;
    }
    void setHandleClock(void (*fptr)(void)) {
        // type: 0xF8  Clock
        handleClock = fptr;
    }
    void setHandleStart(void (*fptr)(void)) {
        // type: 0xFA  Start
        handleStart = fptr;
    }
    void setHandleContinue(void (*fptr)(void)) {
        // type: 0xFB  Continue
        handleContinue = fptr;
    }
    void setHandleStop(void (*fptr)(void)) {
        // type: 0xFC  Stop
        handleStop = fptr;
    }
    void setHandleActiveSensing(void (*fptr)(void)) {
        // type: 0xFE  ActiveSensing
        handleActiveSensing = fptr;
    }
    void setHandleSystemReset(void (*fptr)(void)) {
        // type: 0xFF  SystemReset
        handleSystemReset = fptr;
    }
    void setHandleRealTimeSystem(void (*fptr)(uint8_t realtimebyte)) {
        // type: 0xF8-0xFF - if more specific handler not configured
        handleRealTimeSystem = fptr;
    }
protected:
    virtual bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len);
    virtual void disconnect();
    virtual void timer_event(USBDriverTimer *timer);
    static void rx_callback(const Transfer_t *transfer);
    static void tx_callback(const Transfer_t *transfer);
    void rx_data(const Transfer_t *transfer);
    void tx_data(const Transfer_t *transfer);
    void init();
    void write_packed(uint32_t data);
    void send_sysex_buffer_has_term(const uint8_t *data, uint32_t length, uint8_t cable);
    void send_sysex_add_term_bytes(const uint8_t *data, uint32_t length, uint8_t cable);
    void sysex_byte(uint8_t b);
private:
    Pipe_t *rxpipe;
    Pipe_t *txpipe;
    USBDriverTimer txtimer;
    //enum { MAX_PACKET_SIZE = 64 };
    //enum { RX_QUEUE_SIZE = 80 }; // must be more than MAX_PACKET_SIZE/4
    //uint32_t rx_buffer[MAX_PACKET_SIZE/4];
    //uint32_t tx_buffer1[MAX_PACKET_SIZE/4];
    //uint32_t tx_buffer2[MAX_PACKET_SIZE/4];
    uint32_t * const rx_buffer;
    uint32_t * const tx_buffer1;
    uint32_t * const tx_buffer2;
    uint16_t rx_size;
    uint16_t tx_size;
    //uint32_t rx_queue[RX_QUEUE_SIZE];
    uint32_t * const rx_queue;
    volatile bool rx_packet_queued;
    const uint16_t max_packet_size;
    const uint16_t rx_queue_size;
    uint16_t rx_head;
    uint16_t rx_tail;
    volatile uint8_t tx1_count;
    volatile uint8_t tx2_count;
    uint8_t rx_ep;
    uint8_t tx_ep;
    uint8_t rx_ep_type;
    uint8_t tx_ep_type;
    uint8_t msg_cable;
    uint8_t msg_channel;
    uint8_t msg_type;
    uint8_t msg_data1;
    uint8_t msg_data2;
    uint8_t msg_sysex[SYSEX_MAX_LEN];
    uint16_t msg_sysex_len;
    void (*handleNoteOff)(uint8_t ch, uint8_t note, uint8_t vel);
    void (*handleNoteOn)(uint8_t ch, uint8_t note, uint8_t vel);
    void (*handleVelocityChange)(uint8_t ch, uint8_t note, uint8_t vel);
    void (*handleControlChange)(uint8_t ch, uint8_t control, uint8_t value);
    void (*handleProgramChange)(uint8_t ch, uint8_t program);
    void (*handleAfterTouch)(uint8_t ch, uint8_t pressure);
    void (*handlePitchChange)(uint8_t ch, int pitch);
    void (*handleSysExPartial)(const uint8_t *data, uint16_t length, uint8_t complete);
    void (*handleSysExComplete)(uint8_t *data, unsigned int size);
    void (*handleTimeCodeQuarterFrame)(uint8_t data);
    void (*handleSongPosition)(uint16_t beats);
    void (*handleSongSelect)(uint8_t songnumber);
    void (*handleTuneRequest)(void);
    void (*handleClock)(void);
    void (*handleStart)(void);
    void (*handleContinue)(void);
    void (*handleStop)(void);
    void (*handleActiveSensing)(void);
    void (*handleSystemReset)(void);
    void (*handleRealTimeSystem)(uint8_t rtb);
    Pipe_t mypipes[3] __attribute__ ((aligned(32)));
    Transfer_t mytransfers[7] __attribute__ ((aligned(32)));
    strbuf_t mystring_bufs[1];
};

class MIDIDevice : public MIDIDeviceBase {
public:
    MIDIDevice(USBHost &host) :
        MIDIDeviceBase(host, rx, tx1, tx2, MAX_PACKET_SIZE, queue, RX_QUEUE_SIZE) {};
    // MIDIDevice(USBHost *host) : ....
private:
    enum { MAX_PACKET_SIZE = 64 };
    enum { RX_QUEUE_SIZE = 80 }; // must be more than MAX_PACKET_SIZE/4
    uint32_t rx[MAX_PACKET_SIZE / 4];
    uint32_t tx1[MAX_PACKET_SIZE / 4];
    uint32_t tx2[MAX_PACKET_SIZE / 4];
    uint32_t queue[RX_QUEUE_SIZE];
};

class MIDIDevice_BigBuffer : public MIDIDeviceBase {
public:
    MIDIDevice_BigBuffer(USBHost &host) :
        MIDIDeviceBase(host, rx, tx1, tx2, MAX_PACKET_SIZE, queue, RX_QUEUE_SIZE) {};
    // MIDIDevice(USBHost *host) : ....
private:
    enum { MAX_PACKET_SIZE = 512 };
    enum { RX_QUEUE_SIZE = 400 }; // must be more than MAX_PACKET_SIZE/4
    uint32_t rx[MAX_PACKET_SIZE / 4];
    uint32_t tx1[MAX_PACKET_SIZE / 4];
    uint32_t tx2[MAX_PACKET_SIZE / 4];
    uint32_t queue[RX_QUEUE_SIZE];
};


//--------------------------------------------------------------------------

class USBSerialBase: public USBDriver, public Stream {
public:

    // FIXME: need different USBSerial, with bigger buffers for 480 Mbit & faster speed
    enum { BUFFER_SIZE = 648 }; // must hold at least 6 max size packets, plus 2 extra bytes
    enum { DEFAULT_WRITE_TIMEOUT = 3500};
    // The current know serial device types
    typedef enum { UNKNOWN = 0, CDCACM, FTDI, PL2303, CH341, CP210X } sertype_t;

    USBSerialBase(USBHost &host, uint32_t *big_buffer, uint16_t buffer_size,
                  uint16_t min_pipe_rxtx, uint16_t max_pipe_rxtx,
                  uint16_t vid_to_claim, uint16_t pid_to_claim, 
                  sertype_t vid_pid_sertype, int vid_pid_claim_at_type
                ) :
        txtimer(this),
        _bigBuffer(big_buffer),
        _big_buffer_size(buffer_size),
        _min_rxtx(min_pipe_rxtx),
        _max_rxtx(max_pipe_rxtx),
        _vid_to_claim(vid_to_claim), 
        _pid_to_claim(pid_to_claim), 
        _vid_pid_sertype(vid_pid_sertype), 
        _vid_pid_claim_at_type(vid_pid_claim_at_type)

    {

        init();
    }

    void begin(uint32_t baud, uint32_t format = USBHOST_SERIAL_8N1);
    void end(void);
    uint32_t writeTimeout() {return write_timeout_;}
    void writeTimeOut(uint32_t write_timeout) {write_timeout_ = write_timeout;} // Will not impact current ones.
    virtual int available(void);
    virtual int peek(void);
    virtual int read(void);
    virtual int availableForWrite();
    virtual size_t write(uint8_t c);
    virtual void flush(void);

    bool setDTR(bool fSet);
    bool setRTS(bool fSet);
    using Print::write;

protected:
    virtual bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len);
    virtual void control(const Transfer_t *transfer);
    virtual void disconnect();
    virtual void timer_event(USBDriverTimer *whichTimer);



private:
    static void rx_callback(const Transfer_t *transfer);
    static void tx_callback(const Transfer_t *transfer);
    void rx_data(const Transfer_t *transfer);
    void tx_data(const Transfer_t *transfer);
    void rx_queue_packets(uint32_t head, uint32_t tail);
    void init();
    static bool check_rxtx_ep(uint32_t &rxep, uint32_t &txep);
    bool init_buffers(uint32_t rsize, uint32_t tsize);
    void ch341_setBaud(uint8_t byte_index);
private:
    Pipe_t mypipes[3] __attribute__ ((aligned(32)));
    Transfer_t mytransfers[7] __attribute__ ((aligned(32)));
    strbuf_t mystring_bufs[1];
    USBDriverTimer txtimer;
    uint32_t *_bigBuffer;
    uint16_t _big_buffer_size;
    uint16_t  _min_rxtx;
    uint16_t _max_rxtx;

    // allow sketch to specify a VID/PID/Type setting that this instance can use
    // good for new or oddball devices
    uint16_t _vid_to_claim;
    uint16_t _pid_to_claim;
    sertype_t _vid_pid_sertype;  // what it should map to.
    int _vid_pid_claim_at_type;  // how we should claim it 0 - interface 1 - whole device


    setup_t setup;
    uint8_t setupdata[16]; //
    uint32_t baudrate = 115200;  // lets give it a default in case begin is not called
    uint32_t format_  = USBHOST_SERIAL_8N1; 
    uint32_t write_timeout_ = DEFAULT_WRITE_TIMEOUT;
    Pipe_t *rxpipe;
    Pipe_t *txpipe;
    uint8_t *rx1;   // location for first incoming packet
    uint8_t *rx2;   // location for second incoming packet
    uint8_t *rxbuf; // receive circular buffer
    uint8_t *tx1;   // location for first outgoing packet
    uint8_t *tx2;   // location for second outgoing packet
    uint8_t *txbuf;
    volatile uint16_t rxhead;// receive head
    volatile uint16_t rxtail;// receive tail
    volatile uint16_t txhead;
    volatile uint16_t txtail;
    uint16_t rxsize;// size of receive circular buffer
    uint16_t txsize;// size of transmit circular buffer
    volatile uint8_t  rxstate;// bitmask: which receive packets are queued
    volatile uint8_t  txstate;
    uint8_t pending_control;
    uint8_t setup_state;    // PL2303 - has several steps... Could use pending control?
    uint8_t pl2303_v1;      // Which version do we have
    uint8_t pl2303_v2;
    uint8_t interface;
    uint8_t dtr_rts_;       // save logical state for the two of them.
    volatile bool   control_queued; // Is there already a queued control messaged

    sertype_t sertype;

    typedef struct {
        uint16_t    idVendor;
        uint16_t    idProduct;
        sertype_t   sertype;
        int         claim_at_type;
    } product_vendor_mapping_t;
    static product_vendor_mapping_t pid_vid_mapping[];

};

// USBSerial class - is setup to handle most USB to serial devices that work at USB Full speed with max of 64 byte packets
class USBSerial : public USBSerialBase {
public:
    // Constructor
    // typically you just need to pass in the reference to the host object,
    // However optionally you can also pass in a Vendor ID, Product ID, Type, and claim at interface or object level
    // This hopefully allows you to easily try out some vendor specific devices that underlying it uses one of our known
    // usb interface chips
    USBSerial(USBHost &host,
              uint16_t vid_to_claim = 0, uint16_t pid_to_claim = 0, 
              sertype_t vid_pid_sertype = USBSerial::UNKNOWN, int vid_pid_claim_at_type = 0) :
        // hard code the normal one to 1 and 64 bytes for most likely most are 64
        USBSerialBase(host, bigbuffer, sizeof(bigbuffer), 1, 64, vid_to_claim, pid_to_claim, vid_pid_sertype, vid_pid_claim_at_type) {};
private:
    enum { BUFFER_SIZE = 648 }; // must hold at least 6 max size packets, plus 2 extra bytes
    uint32_t bigbuffer[(BUFFER_SIZE + 3) / 4];
};

class USBSerial_BigBuffer: public USBSerialBase {
public:
    // USBSerial_BigBuffer: handles devices that run at USB highspeed and can read and/or write up to 512 bytes per packet,
    // Parameters:
    //  host - reference to the main USB object
    //  min_rxtx - defaults to 65, set to 1 if you wish for it to also handle all USB to Serial objects
    //  vid_to_claim - Vendor ID Normally 0, but if you have device that is not supported you might specify it here.
    //  pid_to_claim - Product ID - only used with VID above
    //  vid_pid_sertype - Again not normally used unless pid, vid - then one of the following FTDI, PL2303, CH341, CP210X
    // void_pid_claim_at_type = like above but 0 if claim at interface (default)  or 1 claim whole device
    USBSerial_BigBuffer(USBHost &host, uint16_t min_rxtx = 65,
              uint16_t vid_to_claim = 0, uint16_t pid_to_claim = 0, 
              sertype_t vid_pid_sertype = USBSerial::UNKNOWN, int vid_pid_claim_at_type = 0) :
        USBSerialBase(host, bigbuffer, sizeof(bigbuffer), min_rxtx, 512, vid_to_claim, pid_to_claim, vid_pid_sertype, vid_pid_claim_at_type) {};
private:
    enum { BUFFER_SIZE = 4096 }; // must hold at least 6 max size packets, plus 2 extra bytes
    uint32_t bigbuffer[(BUFFER_SIZE + 3) / 4];
};

//--------------------------------------------------------------------------

class AntPlus: public USBDriver {
// Please post any AntPlus feedback or contributions on this forum thread:
// https://forum.pjrc.com/threads/43110-Ant-libarary-and-USB-driver-for-Teensy-3-5-6
public:
    AntPlus(USBHost &host) : /* txtimer(this),*/  updatetimer(this) { init(); }
    void begin(const uint8_t key = 0);
    void onStatusChange(void (*function)(int channel, int status)) {
        user_onStatusChange = function;
    }
    void onDeviceID(void (*function)(int channel, int devId, int devType, int transType)) {
        user_onDeviceID = function;
    }
    void onHeartRateMonitor(void (*f)(int bpm, int msec, int seqNum), uint32_t devid = 0) {
        profileSetup_HRM(&ant.dcfg[PROFILE_HRM], devid);
        memset(&hrm, 0, sizeof(hrm));
        user_onHeartRateMonitor = f;
    }
    void onSpeedCadence(void (*f)(float speed, float distance, float rpm), uint32_t devid = 0) {
        profileSetup_SPDCAD(&ant.dcfg[PROFILE_SPDCAD], devid);
        memset(&spdcad, 0, sizeof(spdcad));
        user_onSpeedCadence = f;
    }
    void onSpeed(void (*f)(float speed, float distance), uint32_t devid = 0) {
        profileSetup_SPEED(&ant.dcfg[PROFILE_SPEED], devid);
        memset(&spd, 0, sizeof(spd));
        user_onSpeed = f;
    }
    void onCadence(void (*f)(float rpm), uint32_t devid = 0) {
        profileSetup_CADENCE(&ant.dcfg[PROFILE_CADENCE], devid);
        memset(&cad, 0, sizeof(cad));
        user_onCadence = f;
    }
    void setWheelCircumference(float meters) {
        wheelCircumference = meters * 1000.0f;
    }
protected:
    virtual void Task();
    virtual bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len);
    virtual void disconnect();
    virtual void timer_event(USBDriverTimer *whichTimer);
private:
    static void rx_callback(const Transfer_t *transfer);
    static void tx_callback(const Transfer_t *transfer);
    void rx_data(const Transfer_t *transfer);
    void tx_data(const Transfer_t *transfer);
    void init();
    size_t write(const void *data, const size_t size);
    int read(void *data, const size_t size);
    void transmit();
private:
    Pipe_t mypipes[2] __attribute__ ((aligned(32)));
    Transfer_t mytransfers[3] __attribute__ ((aligned(32)));
    strbuf_t mystring_bufs[1];
    //USBDriverTimer txtimer;
    USBDriverTimer updatetimer;
    Pipe_t *rxpipe;
    Pipe_t *txpipe;
    bool first_update;
    uint8_t txbuffer[240];
    uint8_t rxpacket[64];
    volatile uint16_t txhead;
    volatile uint16_t txtail;
    volatile bool     txready;
    volatile uint8_t  rxlen;
    volatile bool     do_polling;
private:
    enum _eventi {
        EVENTI_MESSAGE = 0,
        EVENTI_CHANNEL,
        EVENTI_TOTAL
    };
    enum _profiles {
        PROFILE_HRM = 0,
        PROFILE_SPDCAD,
        PROFILE_POWER,
        PROFILE_STRIDE,
        PROFILE_SPEED,
        PROFILE_CADENCE,
        PROFILE_TOTAL
    };
    typedef struct {
        uint8_t channel;
        uint8_t RFFreq;
        uint8_t networkNumber;
        uint8_t stub;
        uint8_t searchTimeout;
        uint8_t channelType;
        uint8_t deviceType;
        uint8_t transType;
        uint16_t channelPeriod;
        uint16_t searchWaveform;
        uint32_t deviceNumber; // deviceId
        struct {
            uint8_t chanIdOnce;
            uint8_t keyAccepted;
            uint8_t profileValid;
            uint8_t channelStatus;
            uint8_t channelStatusOld;
        } flags;
    } TDCONFIG;
    struct {
        uint8_t initOnce;
        uint8_t key; // key index
        int iDevice; // index to the antplus we're interested in, if > one found
        TDCONFIG dcfg[PROFILE_TOTAL]; // channel config, we're using one channel per device
    } ant;
    void (*user_onStatusChange)(int channel, int status);
    void (*user_onDeviceID)(int channel, int devId, int devType, int transType);
    void (*user_onHeartRateMonitor)(int beatsPerMinute, int milliseconds, int sequenceNumber);
    void (*user_onSpeedCadence)(float speed, float distance, float cadence);
    void (*user_onSpeed)(float speed, float distance);
    void (*user_onCadence)(float cadence);
    void dispatchPayload(TDCONFIG *cfg, const uint8_t *payload, const int len);
    static const uint8_t *getAntKey(const uint8_t keyIdx);
    static uint8_t calcMsgChecksum (const uint8_t *buffer, const uint8_t len);
    static uint8_t * findStreamSync(uint8_t *stream, const size_t rlen, int *pos);
    static int msgCheckIntegrity(uint8_t *stream, const int len);
    static int msgGetLength(uint8_t *stream);
    int handleMessages(uint8_t *buffer, int tBytes);
    void sendMessageChannelStatus(TDCONFIG *cfg, const uint32_t channelStatus);
    void message_channel(const int chan, const int eventId,
                         const uint8_t *payload, const size_t dataLength);
    void message_response(const int chan, const int msgId,
                          const uint8_t *payload, const size_t dataLength);
    void message_event(const int channel, const int msgId,
                       const uint8_t *payload, const size_t dataLength);
    int ResetSystem();
    int RequestMessage(const int channel, const int message);
    int SetNetworkKey(const int netNumber, const uint8_t *key);
    int SetChannelSearchTimeout(const int channel, const int searchTimeout);
    int SetChannelPeriod(const int channel, const int period);
    int SetChannelRFFreq(const int channel, const int freq);
    int SetSearchWaveform(const int channel, const int wave);
    int OpenChannel(const int channel);
    int CloseChannel(const int channel);
    int AssignChannel(const int channel, const int channelType, const int network);
    int SetChannelId(const int channel, const int deviceNum, const int deviceType,
                     const int transmissionType);
    int SendBurstTransferPacket(const int channelSeq, const uint8_t *data);
    int SendBurstTransfer(const int channel, const uint8_t *data, const int nunPackets);
    int SendBroadcastData(const int channel, const uint8_t *data);
    int SendAcknowledgedData(const int channel, const uint8_t *data);
    int SendExtAcknowledgedData(const int channel, const int devNum, const int devType,
                                const int TranType, const uint8_t *data);
    int SendExtBroadcastData(const int channel, const int devNum, const int devType,
                             const int TranType, const uint8_t *data);
    int SendExtBurstTransferPacket(const int chanSeq, const int devNum,
                                   const int devType, const int TranType, const uint8_t *data);
    int SendExtBurstTransfer(const int channel, const int devNum, const int devType,
                             const int tranType, const uint8_t *data, const int nunPackets);
    static void profileSetup_HRM(TDCONFIG *cfg, const uint32_t deviceId);
    static void profileSetup_SPDCAD(TDCONFIG *cfg, const uint32_t deviceId);
    static void profileSetup_POWER(TDCONFIG *cfg, const uint32_t deviceId);
    static void profileSetup_STRIDE(TDCONFIG *cfg, const uint32_t deviceId);
    static void profileSetup_SPEED(TDCONFIG *cfg, const uint32_t deviceId);
    static void profileSetup_CADENCE(TDCONFIG *cfg, const uint32_t deviceId);
    struct {
        struct {
            uint8_t bpm;
            uint8_t sequence;
            uint16_t time;
        } previous;
    } hrm;
    void payload_HRM(TDCONFIG *cfg, const uint8_t *data, const size_t dataLength);
    struct {
        struct {
            uint16_t cadenceTime;
            uint16_t cadenceCt;
            uint16_t speedTime;
            uint16_t speedCt;
        } previous;
        float distance;
    } spdcad;
    void payload_SPDCAD(TDCONFIG *cfg, const uint8_t *data, const size_t dataLength);
    /* struct {
        struct {
            uint8_t sequence;
            uint16_t pedalPowerContribution;
            uint8_t pedalPower;
            uint8_t instantCadence;
            uint16_t sumPower;
            uint16_t instantPower;
        } current;
        struct {
            uint16_t stub;
        } previous;
    } pwr; */
    void payload_POWER(TDCONFIG *cfg, const uint8_t *data, const size_t dataLength);
    /* struct {
        struct {
            uint16_t speed;
            uint16_t cadence;
            uint8_t strides;
        } current;
        struct {
            uint8_t strides;
            uint16_t speed;
            uint16_t cadence;
        } previous;
    } stride; */
    void payload_STRIDE(TDCONFIG *cfg, const uint8_t *data, const size_t dataLength);
    struct {
        struct {
            uint16_t speedTime;
            uint16_t speedCt;
        } previous;
        float distance;
    } spd;
    void payload_SPEED(TDCONFIG *cfg, const uint8_t *data, const size_t dataLength);
    struct {
        struct {
            uint16_t cadenceTime;
            uint16_t cadenceCt;
        } previous;
    } cad;
    void payload_CADENCE(TDCONFIG *cfg, const uint8_t *data, const size_t dataLength);
    uint16_t wheelCircumference; // default is WHEEL_CIRCUMFERENCE (2122cm)
};

//--------------------------------------------------------------------------

class RawHIDController : public USBHIDInput {
public:
	RawHIDController(USBHost &host, uint32_t usage = 0, uint8_t *rx_tx_buffers=nullptr, uint16_t rx_tx_buffer_size = 0) : 
			fixed_usage_(usage), rx_tx_buffers_(rx_tx_buffers), rx_tx_buffer_size_(rx_tx_buffer_size) { init(); }
    uint32_t usage(void) {return usage_;}
    void attachReceive(bool (*f)(uint32_t usage, const uint8_t *data, uint32_t len)) {receiveCB = f;}
	bool sendPacket(const uint8_t *buffer, int cb = -1);
	uint16_t rxSize() { return rx_pipe_size_;}
	uint16_t txSize() { return tx_pipe_size_;}// size of transmit circular buffer
protected:
    virtual hidclaim_t claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage);
    virtual bool hid_process_in_data(const Transfer_t *transfer);
    virtual bool hid_process_out_data(const Transfer_t *transfer);
    virtual void hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax);
    virtual void hid_input_data(uint32_t usage, int32_t value);
    virtual void hid_input_end();
    virtual void disconnect_collection(Device_t *dev);
private:
    void init();
    USBHIDParser *driver_;
    enum { MAX_PACKET_SIZE = 64 };
    bool (*receiveCB)(uint32_t usage, const uint8_t *data, uint32_t len) = nullptr;
    uint8_t collections_claimed = 0;
    //volatile bool hid_input_begin_ = false;
    uint32_t fixed_usage_;
    uint32_t usage_ = 0;
	uint16_t rx_pipe_size_;// size of receive circular buffer
	uint16_t tx_pipe_size_;// size of transmit circular buffer
	uint8_t  *rx_tx_buffers_;
	uint16_t rx_tx_buffer_size_; 

    // See if we can contribute transfers
	Transfer_t mytransfers[4] __attribute__ ((aligned(32)));

};

//--------------------------------------------------------------------------

class USBSerialEmu : public USBHIDInput, public Stream {
public:
    USBSerialEmu(USBHost &host) { init(); }
    uint32_t usage(void) {return usage_;}

    // begin method added to make sketch code easier to swap with real searila objects
    void begin(uint32_t baud, uint32_t format = USBHOST_SERIAL_8N1) {}
    void end(void) {};


    // Stream stuff.
    uint32_t writeTimeout() {return write_timeout_;}
    void writeTimeOut(uint32_t write_timeout) {write_timeout_ = write_timeout;} // Will not impact current ones.
    virtual int available(void);
    virtual int peek(void);
    virtual int read(void);
    virtual int availableForWrite();
    virtual size_t write(uint8_t c);
    virtual void flush(void);

    using Print::write;


protected:
    virtual hidclaim_t claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage);
    virtual bool hid_process_in_data(const Transfer_t *transfer);
    virtual bool hid_process_out_data(const Transfer_t *transfer);
    virtual void hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax);
    virtual void hid_input_data(uint32_t usage, int32_t value);
    virtual void hid_input_end();
    virtual void disconnect_collection(Device_t *dev);
    virtual void hid_timer_event(USBDriverTimer *whichTimer);

    bool sendPacket();

private:
    void init();
    USBHIDParser *driver_;
    enum { MAX_PACKET_SIZE = 64 };
    bool (*receiveCB)(uint32_t usage, const uint8_t *data, uint32_t len) = nullptr;
    uint8_t collections_claimed = 0;
    uint32_t usage_ = 0;

    // We have max of 512 byte packets coming in.  How about enough room for 3+3
    enum { RX_BUFFER_SIZE = 1024, TX_BUFFER_SIZE = 512 };
    enum { DEFAULT_WRITE_TIMEOUT = 3500};

    uint8_t rx_buffer_[RX_BUFFER_SIZE];
    uint8_t tx_buffer_[TX_BUFFER_SIZE];

    volatile uint8_t tx_out_data_pending_ = 0;

    volatile uint16_t rx_head_;// receive head
    volatile uint16_t rx_tail_;// receive tail
    volatile uint16_t tx_head_;
    uint16_t rx_pipe_size_;// size of receive circular buffer
    uint16_t tx_pipe_size_;// size of transmit circular buffer
    uint32_t write_timeout_ = DEFAULT_WRITE_TIMEOUT;


    // See if we can contribute transfers
    Transfer_t mytransfers[2] __attribute__ ((aligned(32)));

};




//=============================================================================
// Top level Bluetooth controller class = Main class,
//=============================================================================
// Note we are moving more of the functionality to be per connection instead of indexing
// each time... So converted from structure to class.
class BluetoothConnection  {
public:
    BluetoothConnection() {init();}
    void init() {next_ = s_first_; s_first_ = this; }

    void initializeConnection(BluetoothController *btController, uint8_t bdaddr[6], uint32_t class_of_device, bool inquire_mode);
    void remoteNameComplete(const uint8_t *remote_name);

    void parse(void);
    void parse(uint16_t type_and_report_id, const uint8_t *data, uint32_t len);
    BTHIDInput * find_driver(uint32_t topusage);
    BTHIDInput * find_driver(const uint8_t *remoteName, int type);

    void startTimer(uint32_t microseconds) {bt_connection_timer_.start(microseconds);}
    void stopTimer() {bt_connection_timer_.stop();}

    void dumpHIDReportDescriptor();
    void print_input_output_feature_bits(uint8_t val);
    void printUsageInfo(uint8_t usage_page, uint16_t usage);
    void useHIDProtocol(bool useHID) {use_hid_protocol_ = useHID;}
    void connectToSDP(); // temp to see if we can do this later...
    void timer_event(USBDriverTimer *whichTimer);

    // member variables
    BluetoothConnection *next_ = nullptr;
    BTHIDInput *    device_driver_ = nullptr;
    BluetoothController *btController_ = nullptr;
    strbuf_t *      strbuf_ = nullptr;  // possible to hold onto string if we have one 
    uint16_t        connection_rxid_ = 0;
    uint16_t        control_dcid_ = 0x70;
    uint16_t        interrupt_dcid_ = 0x71;
    uint16_t        sdp_dcid_ = 0x40;
    uint16_t        interrupt_scid_;
    uint16_t        control_scid_;
    uint16_t        sdp_scid_;
    uint8_t         device_bdaddr_[6];// remember devices address
    uint8_t         device_ps_repetion_mode_ ; // mode
    uint8_t         device_clock_offset_[2];
    uint32_t        device_class_;  // class of device.
    uint16_t        device_connection_handle_;  // handle to connection
    uint8_t         remote_ver_;
    uint16_t        remote_man_;
    uint8_t         remote_subv_;
    bool            connection_started_ = false; // probably can be combined
    volatile uint8_t connection_complete_ = 0;   //
    bool            check_for_hid_descriptor_ = false;
    bool            find_driver_type_1_called_ = false; 
    uint8_t         seq_number_ = 0;
    bool            use_hid_protocol_ = false; //
    bool            inquire_mode_ = false; // inquire mode?  or incomming connect
    bool            sdp_connected_ = false;
    bool            supports_SSP_ = false; 
    bool            connection_started_by_timer_ = false; 
    uint16_t        pending_control_tx_ = 0;

    enum {DUNKOWN=0xff, DNIL = 0, DU32, DS32, DU64, DS64, DPB, DLVL};
    enum {CONNECTION_TIMEOUT_US = 50000};
    typedef struct {
        uint8_t  element_type;
        uint8_t dtype;
        uint16_t element_size;    // size of element
        union {
            uint32_t uw;
            int32_t sw;
            uint64_t luw;
            int64_t lsw;
            uint8_t *pb;
        } data;
    } sdp_element_t;


    // More HID stuff
    // wondering if we could share some with USB HID objects?
    bool have_hid_descriptor_ = false;
    uint8_t *sdp_buffer_ = nullptr;
    uint16_t sdp_buffer_len_ = 0;
    uint8_t descriptor_[800];
    enum {REMOTE_NAME_SIZE = 32};
    uint8_t remote_name_[REMOTE_NAME_SIZE] = {0};
    uint16_t descsize_;
    bool use_report_id = true;
    enum { TOPUSAGE_LIST_LEN = 6 };
    enum { USAGE_LIST_LEN = 24 };
    BTHIDInput *topusage_drivers[TOPUSAGE_LIST_LEN];

    static BluetoothConnection *s_first_;

    bool startSDP_ServiceSearchAttributeRequest(uint16_t range_low, uint16_t range_high, uint8_t *buffer, uint32_t cb);
    bool SDPRequestCompleted() {return sdp_request_completed_;}
    uint32_t SDPRequestBufferUsed() {return sdp_request_buffer_used_cnt_;}
    // Add starts of SDP processing.

    // Allow each connection to have it's own timer
    USBDriverTimer  bt_connection_timer_;

protected:
    friend class BluetoothController;

    void rx2_data(uint8_t *rx2buf); // called from rx2_data of BluetoothController
    void tx_data(uint8_t *data, uint16_t length);

    void process_l2cap_connection_request(uint8_t *data, uint16_t length);
    void process_l2cap_connection_response(uint8_t *data, uint16_t length);
    void process_l2cap_config_request(uint8_t *data, uint16_t length);
    void process_l2cap_config_response(uint8_t *data, uint16_t length);
    void process_l2cap_command_reject(uint8_t *data, uint16_t length);
    void process_l2cap_disconnect_request(uint8_t *data, uint16_t length);
    void sendl2cap_ConnectionResponse(uint16_t handle, uint8_t rxid, uint16_t dcid, uint16_t scid, uint8_t result);
    void sendl2cap_ConnectionRequest(uint16_t handle, uint8_t rxid, uint16_t scid, uint16_t psm);
    void sendl2cap_ConfigRequest(uint16_t handle, uint8_t rxid, uint16_t dcid);
    void sendl2cap_ConfigResponse(uint16_t handle, uint8_t rxid, uint16_t scid, uint16_t mtu);
    void sendl2cap_DisconnectResponse(uint16_t handle, uint8_t rxid, uint16_t dcid, uint16_t scid);

    void process_sdp_service_search_request(uint8_t *data);
    void process_sdp_service_search_response(uint8_t *data);
    void process_sdp_service_attribute_request(uint8_t *data);
    void process_sdp_service_attribute_response(uint8_t *data);
    void process_sdp_service_search_attribute_request(uint8_t *data);
    void process_sdp_service_search_attribute_response(uint8_t *data);

    void handleHIDTHDRData(uint8_t *buffer);    // Pass the whole buffer...

    void handle_HCI_WRITE_SCAN_ENABLE_complete(uint8_t *rxbuf);
    void handle_HCI_OP_ROLE_DISCOVERY_complete(uint8_t *rxbuf);

    void send_SDP_ServiceSearchRequest(uint8_t *continue_state, uint8_t cb);
    void send_SDP_ServiceSearchAttributeRequest(uint8_t *continue_state, uint8_t cb);
    uint16_t sdp_request_range_low_ = 0;
    uint16_t sdp_reqest_range_high_ = 0xffff;
    uint8_t *sdp_request_buffer_ = nullptr;
    uint32_t sdp_request_buffer_cb_ = 0;
    uint32_t sdp_request_buffer_used_cnt_ = 0; // cnt in bytes used.
    volatile bool sdp_request_completed_ = true;

    // More SDP/HID stuff
    bool startRetrieveHIDReportDescriptor();
    bool completeSDPRequest(bool success);

    int  extract_next_SDP_Token(uint8_t *pbElement, int cb_left, sdp_element_t &sdpe);
    void print_sdpe_val(sdp_element_t &sdpe, bool verbose);
    void decode_SDP_buffer(bool verbose_output = false);
    void decode_SDP_Data(bool by_user_command);


};

//=============================================================================
// Bluetooth Pairing Callback class
//=============================================================================
class BluetoothPairingCB {
public:
    //  About to send HCI_WRITE_INQUIRY_MODE
    virtual bool writeInquiryMode(uint8_t inquiry_mode) { return true;}

    // The inquiry is complete
    virtual bool inquiryComplete(uint8_t status) {return true;}

    // we received an Inquiry result, use it?
    virtual bool useInquireResult(uint8_t bdaddr[6], uint32_t bluetooth_class, const uint8_t *name)
        {return true;}


    // These return > 0 for success, 0 for false, -1, don't want to support link keys. 
    virtual int writeLinkKey(uint8_t bdaddr[6], uint8_t link_key[16]) {return 0;}
    virtual int readLinkKey(uint8_t bdaddr[6], uint8_t link_key[16]) {return 0;}

    // Asked for PinCode?
    virtual bool sendPinCode(const char *pinCode)
        {return true;}
    virtual bool pinCodeComplete()
        {return true;}

    virtual bool authenticationComplete()
        {return true;}

};


//=============================================================================
// Bluetooth Connection class
//  Will try to handle all of the processing of one Bluetooth connection.
//=============================================================================

class BluetoothController: public USBDriver {
public:
    enum { TOPUSAGE_LIST_LEN = 6 };
    enum { USAGE_LIST_LEN = 24 };
    static const uint8_t DEFAULT_CONNECTIONS = 2;




    BluetoothController(USBHost &host, bool pair = false, const char *pin = "0000", bool pair_ssp = false) : do_pair_device_(pair), pair_pincode_(pin), do_pair_ssp_(pair_ssp), timer_(this)
    { init(); }

    enum {MAX_ENDPOINTS = 4, NUM_SERVICES = 4, }; // Max number of Bluetooth services - if you need more than 4 simply increase this number
    enum {BT_CLASS_DEVICE = 0x0804}; // Toy - Robot
    static void driver_ready_for_bluetooth(BTHIDInput *driver);

    const uint8_t*  myBDAddr(void) {return my_bdaddr_;}

    // See if we can start up pairing after sketch is running. 
    bool startDevicePairing(const char *pin, bool pair_ssp = false);
    
    // Setup a bluetooth pairing callback to receive calls for information and
    // for the sketch to be able to monitor some of the pairing progress
    void setBluetoothPairingCB(BluetoothPairingCB *pairing_cb) {pairing_cb_ = pairing_cb;}

    // method to help control where or not all pairing keys should be stored
    // Can be pointer to File system, in which case we will create a file "PairingKeys" 
    // if FS is NULL, and EEPROM start is specified will store in EEPROM starting
    // at that location.  Note if location is < -1 it will end the have the end
    // of the storage that far from the end of the EEPROM
    void setPairingKeyStorageLocation(FS *pfs = nullptr, int eeprom = -1, int max_keys = -1);

    // Note write link with NULL link_key means delete null bdaddr and null link key delete them all
    bool writeLinkKey(uint8_t bdaddr[6], uint8_t link_key[16]);
    bool readLinkKey(uint8_t bdaddr[6], uint8_t link_key[16]);

    // Experiments to enable LE scanning
    bool setLEScanEnable(uint8_t enable, uint8_t filter_duplicates);
    bool setLEScanParameters(uint8_t scan_type, uint16_t scan_interval, uint16_t scan_window, uint8_t own_address_type, uint8_t filter_policy);


    // BUGBUG version to allow some of the controlled objects to call?
    enum {CONTROL_SCID = -1, INTERRUPT_SCID = -2, SDP_SCID = -3};
    void sendL2CapCommand(uint8_t* data, uint8_t nbytes, int channel = (int)0x0001);
    void sendL2CapCommand(uint16_t handle, uint8_t* data, uint8_t nbytes, uint8_t channelLow = 0x01, uint8_t channelHigh = 0x00);

    // Force the setting one way or the other.
    void useHIDProtocol(bool useHID);
    void updateHIDProtocol(uint8_t protocol);

    bool setTimer(BluetoothConnection *connection, uint32_t ms);  // set to NULL ptr will clear:


protected:
    virtual bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len);
    virtual void control(const Transfer_t *transfer);
    virtual void disconnect();
    virtual void timer_event(USBDriverTimer *whichTimer);

    // Hack to allow PS3 to maybe change values
    uint16_t        next_dcid_ = 0x70;      // Lets try not hard coding control and interrupt dcid

    BluetoothConnection connections_[DEFAULT_CONNECTIONS];
    uint8_t count_connections_ = 0;
    BluetoothConnection  *current_connection_ = nullptr;    // need to figure out when this changes and/or...
    BluetoothConnection  *timer_connection_ = nullptr;    // need to figure out when this changes and/or...
    BluetoothPairingCB   *pairing_cb_ = nullptr;

private:
    friend class BTHIDInput;
    friend class BluetoothConnection;
    static void rx_callback(const Transfer_t *transfer);
    static void rx2_callback(const Transfer_t *transfer);
    static void tx_callback(const Transfer_t *transfer);
    void rx_data(const Transfer_t *transfer);
    void rx2_data(const Transfer_t *transfer);
    void tx_data(const Transfer_t *transfer);

    void init();

    static bool queue_Data_Transfer_Debug(Pipe_t *pipe, void *buffer,
                                    uint32_t len, USBDriver *driver,
                                    uint32_t line);

    // HCI support functions...
    void sendHCICommand(uint16_t hciCommand, uint16_t cParams, const uint8_t* data);
    //void sendHCIReadLocalSupportedFeatures();
    void inline sendHCI_INQUIRY();
    void inline sendHCIInquiryCancel();
    void inline sendHCICreateConnection();
    void inline sendHCIAuthenticationRequested();
    void inline sendHCIAcceptConnectionRequest();
    void inline sendHCIRejectConnectionRequest(uint8_t bdaddr[6], uint8_t error);
    void inline sendHCILinkKeyRequestReply(uint8_t link_key[16]);
    void inline sendHCILinkKeyNegativeReply();
    void inline sendHCIPinCodeReply();
    void inline sendResetHCI();
    void inline sendHDCWriteClassOfDev();
    void inline sendHCIReadBDAddr();
    void inline sendHCIReadLocalSupportedCommands();
    void inline sendHCIReadLocalSupportedFeatures();
    void inline sendHCIReadLocalVersionInfo();
    void  sendHCIWriteScanEnable(uint8_t scan_op);
    void inline sendHCIHCIWriteInquiryMode(uint8_t inquiry_mode);
    void inline sendHCISetEventMask();

    void inline sendHCIRemoteNameRequest();
    void inline sendHCIRemoteVersionInfoRequest();
    void inline sendHCIRoleDiscoveryRequest();
    void inline sendHCIReadRemoteSupportedFeatures();
    void inline sendHCIReadRemoteExtendedFeatures();
	void inline sendHCISimplePairingMode();
	void inline sendHCIReadSimplePairingMode();
    bool inline sendHCIReadStoredLinkKey(uint8_t link_key[16]);
    void inline sendHCIWriteStoredLinkKey(uint8_t link_key[16]);

	void handle_hci_encryption_change_complete();
	void sendHCISetConnectionEncryption();
    void sendInfoRequest();

    void handle_hci_command_complete();
    void handle_hci_command_status();
    void handle_hci_inquiry_result(bool fRSSI = false);
    void handle_hci_extended_inquiry_result();
    void handle_hci_inquiry_complete();
    void handle_hci_incoming_connect();
    void handle_hci_connection_complete();
    void handle_hci_disconnect_complete();
    void handle_hci_authentication_complete();
    void handle_hci_remote_name_complete();
    void handle_hci_remote_version_information_complete();
    void handle_hci_pin_code_request();
    void handle_hci_link_key_notification();
    void handle_hci_link_key_request();
    void handle_hci_return_link_keys();
    void handle_ev_meta_event(); // 0x3e
    void queue_next_hci_command();

    void handle_HCI_IO_CAPABILITY_REQUEST_REPLY();
    void handle_hci_io_capability_request();
    void handle_hci_io_capability_request_reply();
	void handle_hci_user_confirmation_request_reply();

    void setHIDProtocol(uint8_t protocol);
    static BTHIDInput *available_bthid_drivers_list;

    setup_t setup;
    Pipe_t mypipes[4] __attribute__ ((aligned(32)));
    Transfer_t mytransfers[7] __attribute__ ((aligned(32)));
    strbuf_t mystring_bufs[2];      // 2 string buffers - one for our device - one for remote device...
    uint16_t        pending_control_ = 0;
    uint16_t        rx_size_ = 0;
    uint16_t        rx2_size_ = 0;
    uint16_t        tx_size_ = 0;
    Pipe_t          *rxpipe_;
    Pipe_t          *rx2pipe_;
    Pipe_t          *txpipe_;
    uint8_t         rxbuf_[256];    // used to receive data from RX, which may come with several packets...
    uint8_t         rx_packet_data_remaining_ = 0; // how much data remaining
    uint8_t         txbuf_[256];    // buffer to use to send commands to bluetooth
    uint8_t         rx2buf_[64];    // receive buffer from Bulk end point
    uint8_t         rx2buf2_[256];   // receive buffer from Bulk end point
    uint8_t         rx2_packet_data_remaining_ = 0; // how much data remaining
    uint8_t         rx2_continue_packet_expected_ = 0; // Are we expecting a continue packet. 
    uint8_t         hciVersion;     // what version of HCI do we have?

    bool            do_pair_device_;    // Should we do a pair for a new device?
    const char      *pair_pincode_; // What pin code to use for the pairing
    bool            do_pair_ssp_;   // pair device using SSP
    USBDriverTimer  timer_;
    uint8_t         my_bdaddr_[6];  // The bluetooth dongles Bluetooth address.
    uint8_t         features[8];    // remember our local features.
	
    // key storage info
    FS              *pairing_keys_fs_ = nullptr;
    int             pairing_keys_eeprom_start_index_ = -1;
    int             pairing_keys_max_ = 5;

    typedef struct {
        uint16_t    idVendor;
        uint16_t    idProduct;
    } product_vendor_mapping_t;
    static product_vendor_mapping_t pid_vid_mapping[];

};

class ADK: public USBDriver {
public:
    ADK(USBHost &host) { init(); }
    bool ready();
    void begin(char *adk_manufacturer, char *adk_model, char *adk_desc, char *adk_version, char *adk_uri, char *adk_serial);
    void end();
    int available(void);
    int peek(void);
    int read(void);
    size_t write(size_t len, uint8_t *buf);
protected:
    virtual bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len);
    virtual void disconnect();
    virtual void control(const Transfer_t *transfer);
    static void rx_callback(const Transfer_t *transfer);
    static void tx_callback(const Transfer_t *transfer);
    void rx_data(const Transfer_t *transfer);
    void tx_data(const Transfer_t *transfer);
    void init();
    void rx_queue_packets(uint32_t head, uint32_t tail);
    void sendStr(Device_t *dev, uint8_t index, char *str);
private:
    int state = 0;
    Pipe_t *rxpipe;
    Pipe_t *txpipe;
    enum { MAX_PACKET_SIZE = 512 };
    enum { RX_QUEUE_SIZE = 1024 }; // must be more than MAX_PACKET_SIZE
    uint8_t rx_buffer[MAX_PACKET_SIZE];
    uint8_t tx_buffer[MAX_PACKET_SIZE];
    uint16_t rx_size;
    uint16_t tx_size;
    uint8_t rx_queue[RX_QUEUE_SIZE];
    bool rx_packet_queued;
    uint16_t rx_head;
    uint16_t rx_tail;
    uint8_t rx_ep;
    uint8_t tx_ep;
    char *manufacturer;
    char *model;
    char *desc;
    char *version;
    char *uri;
    char *serial;
    Pipe_t mypipes[3] __attribute__ ((aligned(32)));
    Transfer_t mytransfers[7] __attribute__ ((aligned(32)));
};

//--------------------------------------------------------------------------

#include <SdFat.h>
// Use FILE_READ & FILE_WRITE as defined by FS.h
#if defined(FILE_READ) && !defined(FS_H)
#undef FILE_READ
#endif
#if defined(FILE_WRITE) && !defined(FS_H)
#undef FILE_WRITE
#endif
#include <FS.h>


class USBDrive;

// Simple File System base class that maintains list and defines methods for the Drive object
// to call to each of the file system objects such that they can decide if they will claim a partition
class USBFSBase : public FS {
public:
    USBFSBase();
    operator bool() { return (mydevice != nullptr); }
    enum {USBFS_STATE_CHANGE_CONNECTION = 0x01u, USBFS_STATE_CHANGE_FORMAT = 0x02};
    inline uint8_t stateChanged() { return _state_changed; }
    inline void stateChanged(uint8_t state) { _state_changed = state; }
    uint16_t idVendor() { return (mydevice != nullptr) ? mydevice->idVendor : 0; }
    uint16_t idProduct() { return (mydevice != nullptr) ? mydevice->idProduct : 0; }
    const uint8_t *manufacturer()
    {  return  ((mydevice == nullptr) || (mydevice->strbuf == nullptr)) ? nullptr : &mydevice->strbuf->buffer[mydevice->strbuf->iStrings[strbuf_t::STR_ID_MAN]]; }
    const uint8_t *product()
    {  return  ((mydevice == nullptr) || (mydevice->strbuf == nullptr)) ? nullptr : &mydevice->strbuf->buffer[mydevice->strbuf->iStrings[strbuf_t::STR_ID_PROD]]; }
    const uint8_t *serialNumber()
    {  return  ((mydevice == nullptr) || (mydevice->strbuf == nullptr)) ? nullptr : &mydevice->strbuf->buffer[mydevice->strbuf->iStrings[strbuf_t::STR_ID_SERIAL]]; }


    virtual bool getVolumeLabel(char *volume_label, size_t cb) { return false; }

    // Class level static methods.
    // code that can walk the list
    static USBFSBase *nextFS(USBFSBase *pfs);

    static inline bool anyFSChangedState() {return s_any_fs_changed_state;}
    static inline void anyFSChangedState(bool state) {s_any_fs_changed_state = state;}
private:
    // need to define claim functions
    virtual bool claimPartition(USBDrive *device, int partition, int voltype, int type, uint32_t firstSector, uint32_t numSectors, uint8_t *guid) = 0;
    virtual void releasePartition() = 0;

//  virtual hidclaim_t claim_collection(USBHIDParser *driver, Device_t *dev, uint32_t topusage);
    friend class USBDrive;
protected:
    static USBFSBase *s_first_fs;
    USBFSBase *_next = NULL;
    Device_t *mydevice = NULL;
    uint8_t _state_changed = 0;
    static bool s_any_fs_changed_state;
};



class USBDrive : public USBDriver, public FsBlockDeviceInterface {
public:
    USBDrive(USBHost &host) { init(); }
    USBDrive(USBHost *host) { init(); }


    msSCSICapacity_t msCapacity;
    msInquiryResponse_t msInquiry;
    msRequestSenseResponse_t msSense;
    msDriveInfo_t msDriveInfo;

    bool mscTransferComplete = false;
    uint8_t mscInit(void);
    void msReset(void);
    uint8_t msGetMaxLun(void);
    void msCurrentLun(uint8_t lun) {currentLUN = lun;}
    uint8_t msCurrentLun() {return currentLUN;}
    bool available() { delay(0); return deviceAvailable; }
    uint8_t checkConnectedInitialized(void);
    uint16_t getIDVendor() {return idVendor; }
    uint16_t getIDProduct() {return idProduct; }
    uint8_t getHubNumber() { return hubNumber; }
    uint8_t getHubPort() { return hubPort; }
    uint8_t getDeviceAddress() { return deviceAddress; }
    uint8_t WaitMediaReady();
    uint8_t msTestReady();
    uint8_t msReportLUNs(uint8_t *Buffer);
    uint8_t msStartStopUnit(uint8_t mode);
    uint8_t msReadDeviceCapacity(msSCSICapacity_t * const Capacity);
    uint8_t msDeviceInquiry(msInquiryResponse_t * const Inquiry);
    uint8_t msProcessError(uint8_t msStatus);
    uint8_t msRequestSense(msRequestSenseResponse_t * const Sense);
    uint8_t msRequestSense(void *Sense);

    uint8_t msReadBlocks(const uint32_t BlockAddress, const uint16_t Blocks,
                         const uint16_t BlockSize, void * sectorBuffer);
    uint8_t msReadSectorsWithCB(const uint32_t BlockAddress, const uint16_t Blocks, void (*callback)(uint32_t token, uint8_t* data), uint32_t token);
    uint8_t msWriteBlocks(const uint32_t BlockAddress, const uint16_t Blocks,
                          const uint16_t BlockSize,   const void * sectorBuffer);

    bool begin();
    // Not sure of good name here.
    // maybe startFilesystems(), enumFileSystems()...
    bool startFilesystems();
    bool filesystemsStarted() {return _drive_connect_fs_status == USBDRIVE_FS_STARTED;}

    bool updateConnectedFilesystems();

    // Schedule when the updatedConnectedFilesystems should be called
    // when = 0(manual), 1(MTP::loop)
    enum {UPDATE_MANUAL = 0, UPDATE_TASK = 1};
    void whenToUpdateConnectedFilesystems(int when) {s_when_to_update = when; }
    int whenToUpdateConnectedFilesystems() {return s_when_to_update;}

    static bool connectedFilesystemsChanged() {return s_connected_filesystems_changed;}
    static void connectedFilesystemsChanged(bool changed) {s_connected_filesystems_changed = changed;}

    void printPartionTable(Print &Serialx);
    void printExtendedPartition(MbrSector_t *mbr, uint8_t ipExt, Print &Serialx);
    uint32_t printGUIDPartitionTable(Print &Serialx);

    enum {INVALID_VOL = 0, MBR_VOL, EXT_VOL, GPT_VOL}; // what type of volume did the mapping return
    int findPartition(int partition, int &type, uint32_t &firstSector, uint32_t &numSectors,
                      uint32_t &mbrLBA, uint8_t &mbrPart, uint8_t *guid = nullptr);


public:
    // Functions for SdFat FsBlockDeviceInterface
    // return the number of 512 byte sectors for the whole drive
    uint32_t sectorCount() { return msDriveInfo.capacity.Blocks; }
    // return code for the last error.  (where is list of errors?)
    uint8_t errorCode() const { return m_errorCode; }
    // return error data for last error.
    uint32_t errorData() const { return 0; }
    // return error line for last error. Tmp function for debug.
    uint32_t errorLine() const { return m_errorLine; }
    // Check for busy
    bool isBusy() { return !m_initDone && !mscTransferComplete; }
    // Check for busy with MSC read operation
    bool isBusyRead() { return mscTransferComplete; }
    // Check for busy with MSC read operation
    bool isBusyWrite() { return mscTransferComplete; }
    // Read a USB drive's information. This contains the drive's identification
    // information such as Manufacturer ID, Product name, Product serial
    // number and Manufacturing date pluse more.
    bool readUSBDriveInfo(msDriveInfo_t * driveInfo) {
        memcpy(driveInfo, &msDriveInfo, sizeof(msDriveInfo_t));
        return true;
    }
    // Read a 512 byte sector from an USB MSC drive.
    bool readSector(uint32_t sector, uint8_t* dst);
    // Read multiple 512 byte sectors from an USB MSC drive.
    bool readSectors(uint32_t sector, uint8_t* dst, size_t numsectors);
    // return USB MSC drive status.
    uint32_t status() { return m_errorCode; }
    // return success if sync successful. Not for user apps.
    bool syncDevice() { return true; }
    // Writes a 512 byte sector to an USB MSC drive.
    bool writeSector(uint32_t sector, const uint8_t* src);
    // Write multiple 512 byte sectors to an USB MSC drive.
    bool writeSectors(uint32_t sector, const uint8_t* src, size_t ns);
    // Read multiple 512 byte sectors from an USB MSC drive, using
    // a callback per sector
    bool readSectorsWithCB(uint32_t sector, size_t ns,
                           void (*callback)(uint32_t, uint8_t *), uint32_t token);
    bool readSectorsCallback(uint32_t sector, uint8_t* dst, size_t numSectors,
                             void (*callback)(uint32_t sector, uint8_t *buf, void *context), void *context);
    //bool writeSectorsCallback(uint32_t sector, size_t numSectors,
    //  const uint8_t * (*callback)(uint32_t sector, void *context), void *context);

protected:
    virtual bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len);
    virtual void control(const Transfer_t *transfer);
    virtual void disconnect();
    virtual void Task();
    static void callbackIn(const Transfer_t *transfer);
    static void callbackOut(const Transfer_t *transfer);
    void new_dataIn(const Transfer_t *transfer);
    void new_dataOut(const Transfer_t *transfer);
    void init();
    uint8_t msDoCommand(msCommandBlockWrapper_t *CBW, void *buffer);
    uint8_t msGetCSW(void);
private:
    Pipe_t mypipes[3] __attribute__ ((aligned(32)));
    Transfer_t mytransfers[7] __attribute__ ((aligned(32)));
    strbuf_t mystring_bufs[1];
    uint32_t packetSizeIn;
    uint32_t packetSizeOut;
    Pipe_t *datapipeIn;
    Pipe_t *datapipeOut;
    uint8_t bInterfaceNumber;
    uint32_t endpointIn = 0;
    uint32_t endpointOut = 0;
    setup_t setup;
    uint8_t report[8];
    uint8_t maxLUN = 0;
    uint8_t currentLUN = 0;
//  msSCSICapacity_t msCapacity;
//  msInquiryResponse_t msInquiry;
//  msRequestSenseResponse_t msSense;
    uint16_t idVendor = 0;
    uint16_t idProduct = 0;
    uint8_t hubNumber = 0;
    uint8_t hubPort = 0;
    uint8_t deviceAddress = 0;
    volatile bool msOutCompleted = false;
    volatile bool msInCompleted = false;
    volatile bool msControlCompleted = false;
    uint32_t CBWTag = 0;
    bool deviceAvailable = false;
    // experiment with transfers with callbacks.
    void (*_read_sectors_callback)(uint32_t token, uint8_t* data) = nullptr;
    uint32_t _read_sectors_token = 0;
    uint16_t _read_sectors_remaining = 0;
    enum {READ_CALLBACK_TIMEOUT_MS = 250};
    elapsedMillis _emlastRead;
    uint8_t _read_sector_buffer1[512];
    uint8_t _read_sector_buffer2[512];
    bool m_initDone = false;
    uint8_t m_errorCode = MS_NO_MEDIA_ERR;
    uint32_t m_errorLine = 0;
    USBFSBase *claimed_filesystem_list = nullptr;
    enum {USBDRIVE_NOT_CONNECTED = 0, USBDRIVE_CONNECTED = 1, USBDRIVE_FS_STARTED = 2};
    int _drive_connect_fs_status = USBDRIVE_NOT_CONNECTED;
    int _cGPTParts = 0;  // if GPT cache of parts.

    static USBDrive *s_first_drive;
    static bool s_connected_filesystems_changed;
    static int s_when_to_update; // default to Task()
    USBDrive *_next_drive = nullptr;

};

#define MSC_MAX_FILENAME_LEN 256

class MSCFile : public FileImpl
{
private:
    // Classes derived from File are never meant to be constructed
    // anywhere other than open() in the parent FS class and
    // openNextFile() while traversing a directory.
    // Only the abstract File class which references these derived
    // classes is meant to have a public constructor!
    MSCFile(const FsFile &file) : mscfatfile(file), filename(nullptr) { }
    friend class USBFilesystem;
public:
    virtual ~MSCFile(void) {
        if (mscfatfile) mscfatfile.close();
        if (filename) free(filename);
    }
#ifdef FILE_WHOAMI
    virtual void whoami() {
        Serial.printf("   MSCFile this=%x, refcount=%u\n",
                      (int)this, getRefcount());
    }
#endif
    virtual size_t write(const void *buf, size_t size) {
        return mscfatfile.write(buf, size);
    }
    virtual int peek() {
        return mscfatfile.peek();
    }
    virtual int available() {
        return mscfatfile.available();
    }
    virtual void flush() {
        mscfatfile.flush();
    }
    virtual size_t read(void *buf, size_t nbyte) {
        return mscfatfile.read(buf, nbyte);
    }
    virtual bool truncate(uint64_t size = 0) {
        return mscfatfile.truncate(size);
    }
    virtual bool seek(uint64_t pos, int mode = SeekSet) {
        if (mode == SeekSet) return mscfatfile.seekSet(pos);
        if (mode == SeekCur) return mscfatfile.seekCur(pos);
        if (mode == SeekEnd) return mscfatfile.seekEnd(pos);
        return false;
    }
    virtual uint64_t position() {
        return mscfatfile.curPosition();
    }
    virtual uint64_t size() {
        return mscfatfile.size();
    }
    virtual void close() {
        if (filename) {
            free(filename);
            filename = nullptr;
        }
        mscfatfile.close();
    }
    virtual bool isOpen() {
        return mscfatfile.isOpen();
    }
    virtual const char * name() {
        if (!filename) {
            filename = (char *)malloc(MSC_MAX_FILENAME_LEN);
            if (filename) {
                mscfatfile.getName(filename, MSC_MAX_FILENAME_LEN);
            } else {
                static char zeroterm = 0;
                filename = &zeroterm;
            }
        }
        return filename;
    }
    virtual boolean isDirectory(void) {
        return mscfatfile.isDirectory();
    }
    virtual File openNextFile(uint8_t mode = 0) {
        FsFile file = mscfatfile.openNextFile();
        if (file) return File(new MSCFile(file));
        return File();
    }
    virtual void rewindDirectory(void) {
        mscfatfile.rewindDirectory();
    }
    virtual bool getCreateTime(DateTimeFields &tm) {
        uint16_t fat_date, fat_time;
        if (!mscfatfile.getCreateDateTime(&fat_date, &fat_time)) return false;
        if ((fat_date == 0) && (fat_time == 0)) return false;
        tm.sec = FS_SECOND(fat_time);
        tm.min = FS_MINUTE(fat_time);
        tm.hour = FS_HOUR(fat_time);
        tm.mday = FS_DAY(fat_date);
        tm.mon = FS_MONTH(fat_date) - 1;
        tm.year = FS_YEAR(fat_date) - 1900;
        return true;
    }
    virtual bool getModifyTime(DateTimeFields &tm) {
        uint16_t fat_date, fat_time;
        if (!mscfatfile.getModifyDateTime(&fat_date, &fat_time)) return false;
        if ((fat_date == 0) && (fat_time == 0)) return false;
        tm.sec = FS_SECOND(fat_time);
        tm.min = FS_MINUTE(fat_time);
        tm.hour = FS_HOUR(fat_time);
        tm.mday = FS_DAY(fat_date);
        tm.mon = FS_MONTH(fat_date) - 1;
        tm.year = FS_YEAR(fat_date) - 1900;
        return true;
    }
    virtual bool setCreateTime(const DateTimeFields &tm) {
        if (tm.year < 80 || tm.year > 207) return false;
        return mscfatfile.timestamp(T_CREATE, tm.year + 1900, tm.mon + 1,
                                    tm.mday, tm.hour, tm.min, tm.sec);
    }
    virtual bool setModifyTime(const DateTimeFields &tm) {
        if (tm.year < 80 || tm.year > 207) return false;
        return mscfatfile.timestamp(T_WRITE, tm.year + 1900, tm.mon + 1,
                                    tm.mday, tm.hour, tm.min, tm.sec);
    }

private:
    FsFile mscfatfile;
    char *filename;
};

class USBFilesystem : public USBFSBase
{
public:
    USBFilesystem(USBHost &host) : USBFSBase() { init(); }
    USBFilesystem(USBHost *host) : USBFSBase() { init(); }

	void end();
	void init();
	
    virtual bool getVolumeLabel(char *volume_label, size_t cb) { return mscfs.getVolumeLabel(volume_label, cb); }

    operator bool() {
        // use of volatile prevents compiler from optimizing away
        // re-reading the pointer if program repeated checks bool()
        USBDrive *dev = *(USBDrive * volatile *)&device;
        return dev != nullptr;
    }
    bool mediaPresent() { return *this; }
    const char *name() {
      if (volumeLabel[0] == 0xFF) { // 0xFF = volume label not yet read
        if (!getVolumeLabel(volumeLabel, sizeof(volumeLabel))) {
          volumeLabel[0] = 0; // 0x00 = volume label undefined
        }
      }
      return volumeLabel;
    }

    // will remove soon, older versions to detect formatted.
    inline bool changed() {return _state_changed == USBFS_STATE_CHANGE_FORMAT;;}
    inline void changed(bool fChanged) {
        if (fChanged) _state_changed = USBFS_STATE_CHANGE_FORMAT;
        else _state_changed &= ~USBFS_STATE_CHANGE_FORMAT;
    }

    File open(const char *filepath, uint8_t mode = FILE_READ) {
        oflag_t flags = O_READ;
        if (mode == FILE_WRITE) { flags = O_RDWR | O_CREAT | O_AT_END; }
        else if (mode == FILE_WRITE_BEGIN) { flags = O_RDWR | O_CREAT; }
        FsFile file = mscfs.open(filepath, flags);
        if (file) return File(new MSCFile(file));
        return File();
    }
    bool exists(const char *filepath) {
        return mscfs.exists(filepath);
    }
    bool mkdir(const char *filepath) {
        return mscfs.mkdir(filepath);
    }
    bool rename(const char *oldfilepath, const char *newfilepath) {
        return mscfs.rename(oldfilepath, newfilepath);
    }
    bool remove(const char *filepath) {
        return mscfs.remove(filepath);
    }
    bool rmdir(const char *filepath) {
        return mscfs.rmdir(filepath);
    }
    uint64_t usedSize() {
        return  (uint64_t)(mscfs.clusterCount() - mscfs.freeClusterCount())
                * (uint64_t)mscfs.bytesPerCluster();
    }
    uint64_t totalSize() {
        return (uint64_t)mscfs.clusterCount() * (uint64_t)mscfs.bytesPerCluster();
    }

    bool format(int type = 0, char progressChar = 0, Print& pr = Serial);

    void printError(Print &p = Serial);
protected:
    virtual bool claimPartition(USBDrive *device, int partition, int voltype, int type, uint32_t firstSector, uint32_t numSectors, uint8_t *guid = nullptr);
    virtual void releasePartition();
    bool check_voltype_guid(int voltype, uint8_t *guid);
    bool changed_ = false;
    char volumeLabel[12] = {0xFF};

public:
    FsVolume mscfs;      // SdFat API
    USBDrive *device;
    int partition;
    int partitionType;



};


// do not expose these defines in Arduino sketches or other libraries
#undef MSC_MAX_FILENAME_LEN



#endif
