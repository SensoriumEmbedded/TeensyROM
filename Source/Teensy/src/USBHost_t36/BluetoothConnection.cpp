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
 *
 * information about the BlueTooth HCI comes from logic analyzer captures
 * plus... http://affon.narod.ru/BT/bluetooth_app_c10.pdf
 */

//=============================================================================
// Bluetooth connections code
//=============================================================================


#include <Arduino.h>
#include "USBHost_t36.h"  // Read this header first for key info
#include "utility/bt_defines.h"

#define print   USBHost::print_
#define println USBHost::println_//#define DEBUG_BT

//#define DEBUG_BT
//#define DEBUG_BT_VERBOSE

#ifndef DEBUG_BT
#undef DEBUG_BT_VERBOSE
void inline DBGPrintf(...) {};
#else
#define DBGPrintf USBHDBGSerial.printf
#endif

#ifndef DEBUG_BT_VERBOSE
void inline VDBGPrintf(...) {};
#else
#define VDBGPrintf USBHDBGSerial.printf
#endif


// This is a list of all the drivers inherited from the BTHIDInput class.
// Unlike the list of USBDriver (managed in enumeration.cpp), drivers stay
// on this list even when they have claimed a top level collection.
BluetoothConnection *BluetoothConnection::s_first_ = NULL;

// When a new top level collection is found, this function asks drivers
// if they wish to claim it.  The driver taking ownership of the
// collection is returned, or NULL if no driver wants it.

//=============================================================================
// initialize the connection data
//=============================================================================
void BluetoothConnection::initializeConnection(BluetoothController *btController, uint8_t bdaddr[6], uint32_t class_of_device, 
        bool inquire_mode)
{
    device_driver_ = nullptr;
    btController_ = btController;  // back pointer to main object

    // lets setup a connection for this timer
    bt_connection_timer_.init(btController_); // so it will use the main device
    bt_connection_timer_.pointer = (void*)this; // but rememember us. 


    connection_rxid_ = 0;
    control_dcid_ = 0x70;
    interrupt_dcid_ = 0x71;
    sdp_dcid_ = 0x40;
    connection_complete_ = 0;
    connection_started_ = false;
    connection_started_by_timer_ = false;
    //if (!connection_started_) {
    //    connection_started_ = true;
    //    btController_->setTimer(nullptr, 0); // clear out timer
    //}
    use_hid_protocol_ = false;
    sdp_connected_ = false;
    supports_SSP_ = false;
    pending_control_tx_ = 0;
    inquire_mode_ = inquire_mode; 
    find_driver_type_1_called_ = false; // bugbug should combine:

    // We need to save away the BDADDR and class link type?
    for (uint8_t i = 0; i < 6; i++) device_bdaddr_[i] = bdaddr[i];
    device_class_ = class_of_device;
}

void BluetoothConnection::remoteNameComplete(const uint8_t *device_name)
{
    // hack for now remember the device_name if we have one
    if (device_name)strcpy((char*)remote_name_, (const char *)device_name);
    else remote_name_[0] = 0; // null terminated string.

    device_driver_ = find_driver(device_name, 0);
}


//=============================================================================
// Find a driver
//=============================================================================
BTHIDInput * BluetoothConnection::find_driver(const uint8_t *remoteName, int type)
{
    if (type == 1) find_driver_type_1_called_ = true;
    DBGPrintf("BluetoothController::find_driver(%x) type: %d\n", device_class_, type);
    if (device_class_ & 0x2000) DBGPrintf("  (0x2000)Limited Discoverable Mode\n");
    DBGPrintf("  (0x500)Peripheral device\n");
    if (device_class_ & 0x80) DBGPrintf("    Mouse\n");
    if (device_class_ & 0x40) DBGPrintf("    Keyboard\n");
    switch (device_class_ & 0x3c) {
    case 4: DBGPrintf("    Joystick\n"); break;
    case 8: DBGPrintf("    Gamepad\n"); break;
    case 0xc: DBGPrintf("    Remote Control\n"); break;
    }
    check_for_hid_descriptor_ = false;
    BTHIDInput *driver = BluetoothController::available_bthid_drivers_list;
    while (driver) {
        DBGPrintf("  driver %x\n", (uint32_t)driver);
        // should make const all the way through
        hidclaim_t claim_type = driver->claim_bluetooth(this, device_class_, (uint8_t*)remoteName, type);
        if (claim_type == CLAIM_INTERFACE) {
            check_for_hid_descriptor_ = false;
            DBGPrintf("    *** Claimed ***\n");
            return driver;
        } else if (claim_type == CLAIM_REPORT) {
            DBGPrintf("    *** Check for HID  ***\n");
            check_for_hid_descriptor_ = true;
        }
        driver = driver->next;
    }
    return NULL;
}

//=============================================================================
// Handle the rx2_
//=============================================================================
void BluetoothConnection::rx2_data(uint8_t *rx2buf) // called from rx2_data of BluetoothController
{
    // need to detect if these are L2CAP or SDP or ...
    uint16_t data_len = rx2buf[4] + ((uint16_t)rx2buf[5] << 8);
    uint16_t dcid = rx2buf[6] + ((uint16_t)rx2buf[7] << 8);
    //DBGPrintf("@@@@@@ SDP MSG? %x %x %x @@@@@", dcid, sdp_dcid_, rx2buf[8]);

    if (dcid == sdp_dcid_) {
        switch (rx2buf[8]) {
        case SDP_SERVICE_SEARCH_REQUEST:
            process_sdp_service_search_request(&rx2buf[8]);
            break;
        case SDP_SERVICE_SEARCH_RESPONSE:
            process_sdp_service_search_response(&rx2buf[8]);
            break;
        case SDP_SERVICE_ATTRIBUTE_REQUEST:
            process_sdp_service_attribute_request(&rx2buf[8]);
            break;
        case SDP_SERVICE_ATTRIBUTE_RESPONSE:
            process_sdp_service_attribute_response(&rx2buf[8]);
            break;
        case SDP_SERVICE_SEARCH_ATTRIBUTE_REQUEST:
            process_sdp_service_search_attribute_request(&rx2buf[8]);
            break;
        case SDP_SERVICE_SEARCH_ATTRIBUTE_RESPONSE:
            process_sdp_service_search_attribute_response(&rx2buf[8]);
            break;
        }
    } else {
        switch (rx2buf[8]) {
        case L2CAP_CMD_CONNECTION_REQUEST:
            process_l2cap_connection_request(&rx2buf[8], data_len);
            break;
        case L2CAP_CMD_CONNECTION_RESPONSE:
            process_l2cap_connection_response(&rx2buf[8], data_len);
            break;
        case L2CAP_CMD_CONFIG_REQUEST:
            process_l2cap_config_request(&rx2buf[8], data_len);
            break;
        case L2CAP_CMD_CONFIG_RESPONSE:
            process_l2cap_config_response(&rx2buf[8], data_len);
            break;

        case HID_THDR_DATA_INPUT:
            handleHIDTHDRData(rx2buf);    // Pass the whole buffer...
            break;
        case L2CAP_CMD_COMMAND_REJECT:
            process_l2cap_command_reject(&rx2buf[8], data_len);
            break;
        case L2CAP_CMD_DISCONNECT_REQUEST:
            process_l2cap_disconnect_request(&rx2buf[8], data_len);
            break;
        }
    }

}

//=============================================================================
// Process tx_data and state
//=============================================================================
void BluetoothConnection::tx_data(uint8_t *data, uint16_t length)
{

#ifdef DEBUG_BT_VERBOSE
    DBGPrintf("tx_data callback (bluetooth): %d : ", pending_control_tx_);
    for (uint8_t i = 0; i < length; i++) DBGPrintf("%02x ", data[i]);
    DBGPrintf("\n");
#endif
    switch (pending_control_tx_) {
    case STATE_TX_SEND_CONNECT_INT:
        delay(1);
        connection_rxid_++;
        sendl2cap_ConnectionRequest(device_connection_handle_, connection_rxid_, interrupt_dcid_, HID_INTR_PSM);
        pending_control_tx_ = 0;
        break;
    case STATE_TX_SEND_CONECT_RSP_SUCCESS:
        delay(1);
        // Tell the device we are ready
        sendl2cap_ConnectionResponse(device_connection_handle_, connection_rxid_++, control_dcid_, control_scid_, SUCCESSFUL);
        pending_control_tx_ = STATE_TX_SEND_CONFIG_REQ;
        break;
    case STATE_TX_SEND_CONFIG_REQ:
        delay(1);
        sendl2cap_ConfigRequest(device_connection_handle_, connection_rxid_, control_scid_);
        pending_control_tx_ = 0;
        break;
    case STATE_TX_SEND_CONECT_ISR_RSP_SUCCESS:
        delay(1);
        // Tell the device we are ready
        sendl2cap_ConnectionResponse(device_connection_handle_, connection_rxid_++, interrupt_dcid_, interrupt_scid_, SUCCESSFUL);
        pending_control_tx_ = STATE_TX_SEND_CONFIG_ISR_REQ;
        break;
    case STATE_TX_SEND_CONFIG_ISR_REQ:
        delay(1);
        sendl2cap_ConfigRequest(device_connection_handle_, connection_rxid_, interrupt_scid_);
        pending_control_tx_ = 0;
        break;
    case STATE_TX_SEND_CONECT_SDP_RSP_SUCCESS:
        delay(1);
        sendl2cap_ConnectionResponse(device_connection_handle_, connection_rxid_, sdp_dcid_, sdp_scid_, SUCCESSFUL);
        pending_control_tx_ = STATE_TX_SEND_CONFIG_SDP_REQ;
        break;
    case STATE_TX_SEND_CONFIG_SDP_REQ:
        delay(1);
        sendl2cap_ConfigRequest(device_connection_handle_, connection_rxid_, sdp_scid_);
        pending_control_tx_ = 0;
        break;
    }
}

//=============================================================================
// Moved handling of L2CAP and HID messages
//=============================================================================
// Experiment to see if we can get SDP connectin setup
void BluetoothConnection::connectToSDP() {
    DBGPrintf("$$$ BluetoothController::connectToSDP() Called\n");
    connection_rxid_++;
    sendl2cap_ConnectionRequest(device_connection_handle_, connection_rxid_,
                                sdp_dcid_, SDP_PSM);
    pending_control_tx_ = 0;
}


void  BluetoothConnection::process_l2cap_connection_request(uint8_t *data, uint16_t length) {
    //       ID   LEN  LEN PSM  PSM  SCID SCID
    // 0x02 0x02 0x04 0x00 0x11 0x00 0x43 0x00

    uint16_t psm = data[4] + ((uint16_t)data[5] << 8);
    uint16_t scid = data[6] + ((uint16_t)data[7] << 8);
    connection_started_ = true;
    btController_->setTimer(nullptr, 0); // clear out timer
    connection_rxid_ = data[1];
    DBGPrintf("    recv L2CAP Connection Request: ID: %d, PSM: %x, SCID: %x\n", connection_rxid_, psm, scid);

    // Assuming not pair mode Send response like:
    //      RXID Len  LEN  DCID DCID  SCID SCID RES 0     0    0
    // 0x03 0x02 0x08 0x00 0x70 0x00 0x43 0x00 0x01 0x00 0x00 0x00
    if (psm == HID_CTRL_PSM) {
        control_scid_ = scid;
        sendl2cap_ConnectionResponse(device_connection_handle_, connection_rxid_, control_dcid_, control_scid_, PENDING);
        pending_control_tx_ = STATE_TX_SEND_CONECT_RSP_SUCCESS;
    } else if (psm == HID_INTR_PSM) {
        interrupt_scid_ = scid;
        sendl2cap_ConnectionResponse(device_connection_handle_, connection_rxid_, interrupt_dcid_, interrupt_scid_, PENDING);
        pending_control_tx_ = STATE_TX_SEND_CONECT_ISR_RSP_SUCCESS;

    } else if (psm == SDP_PSM) {
        DBGPrintf("        <<< SDP PSM >>>\n");
        sdp_scid_ = scid;
        sendl2cap_ConnectionResponse(device_connection_handle_, connection_rxid_, sdp_dcid_, sdp_scid_, PENDING);
        pending_control_tx_ = STATE_TX_SEND_CONECT_SDP_RSP_SUCCESS;
    }
}

// Process the l2cap_connection_response...
void BluetoothConnection::process_l2cap_connection_response(uint8_t *data, uint16_t length) {

    uint16_t scid = data[4] + ((uint16_t)data[5] << 8);
    uint16_t dcid = data[6] + ((uint16_t)data[7] << 8);
    uint16_t result = data[8] + ((uint16_t)data[9] << 8);

    DBGPrintf("    recv L2CAP Connection Response: ID: %d, Dest:%x, Source:%x, Result:%x, Status: %x pending control: %x %x\n",
              data[1], scid, dcid,
              result, data[10] + ((uint16_t)data[11] << 8), btController_->pending_control_, pending_control_tx_);

    // Experiment ignore if: pending_control_tx_ = STATE_TX_SEND_CONFIG_REQ;
    if ((pending_control_tx_ == STATE_TX_SEND_CONFIG_REQ) || (pending_control_tx_ == STATE_TX_SEND_CONECT_RSP_SUCCESS)) {
        DBGPrintf("    *** State == STATE_TX_SEND_CONFIG_REQ - try ignoring\n");
        return;
    }

    //48 20 10 0 | c 0 1 0 | 3 0 8 0 44 0 70 0 0 0 0 0
    if (dcid == interrupt_dcid_) {
        interrupt_scid_ = scid;
        DBGPrintf("      Interrupt Response\n");
        connection_rxid_++;
        sendl2cap_ConfigRequest(device_connection_handle_, connection_rxid_, scid);
    } else if (dcid == control_dcid_) {
        control_scid_ = scid;
        DBGPrintf("      Control Response\n");
        sendl2cap_ConfigRequest(device_connection_handle_, connection_rxid_, scid);
    } else if (dcid == sdp_dcid_) {
        // Check for failure!
        DBGPrintf("      SDP Response\n");
        if (result != 0) {
            DBGPrintf("      Failed - No SDP!\n");
            // Enable SCan to page mode
            sdp_connected_ = false;
            connection_complete_ |= CCON_SDP;
            if (connection_complete_ == CCON_ALL)
                btController_->sendHCIWriteScanEnable(2);
        } else {
            sdp_scid_ = scid;
            sendl2cap_ConfigRequest(device_connection_handle_, connection_rxid_, scid);
        }
    } else {
        DBGPrintf("      Unknown Response\n");
        // Unknown dcid... Guess
        if (((connection_complete_ | CCON_SDP) == CCON_ALL) && (result !=0)) {
            DBGPrintf("      Guess SDP Response failure\n");
            connection_complete_ |= CCON_SDP;
            btController_->sendHCIWriteScanEnable(2);
        }
    }
}

void BluetoothConnection::process_l2cap_config_request(uint8_t *data, uint16_t length) {
    //48 20 10 00 0c 00 01 00 * 04 02 08 00 70 00 00 00 01 02 30 00
    //48 20 10 00 0C 00 01 00 * 04 03 08 00 70 00 00 00 01 02 48 00 
    uint16_t dcid = data[4] + ((uint16_t)data[5] << 8);
    DBGPrintf("    recv L2CAP config Request: ID: %d, Dest:%x, Flags:%x,  Options:",
              data[1], dcid, data[6] + ((uint16_t)data[7] << 8));
    for (uint16_t i = 8; i < length; i++) DBGPrintf(" %02x", data[i]);
    uint16_t mtu = 0x2A0;  // hard coded what we were returning....
    for (uint16_t i = 8; i < length; i++) {
        if ((data[i] == 1) && (data[i+1] == 2)) {
            mtu = data[i+2] + (data[i+3] << 8);
            DBGPrintf(" MTU:%u", mtu);
        }
        i += data[i+1] + 2;
    }
    DBGPrintf("\n");

    // Now see which dest was specified
    if (dcid == control_dcid_) {
        DBGPrintf("      Control Configuration request\n");
        sendl2cap_ConfigResponse(device_connection_handle_, data[1], control_scid_, mtu);
        connection_complete_ |= CCON_CONT;
    } else if (dcid == interrupt_dcid_) {
        DBGPrintf("      Interrupt Configuration request\n");
        sendl2cap_ConfigResponse(device_connection_handle_, data[1], interrupt_scid_, mtu);
        connection_complete_ |= CCON_INT;
    } else if (dcid == sdp_dcid_) {
        DBGPrintf("      SDP Configuration request\n");
        sendl2cap_ConfigResponse(device_connection_handle_, data[1], sdp_scid_, mtu);

        connection_complete_ |= CCON_SDP;
        sdp_connected_ = true;

    } else if (dcid == sdp_scid_) {
        DBGPrintf("      SDP Configuration request (But our SCID?)\n");
        // maybe we should change the ids around?
        sendl2cap_ConfigResponse(device_connection_handle_, data[1], sdp_scid_, mtu);
        // We see this with some PS4? 

        // Enable SCan to page mode
        connection_complete_ |= CCON_SDP;
        sdp_connected_ = true;
    }
}

void BluetoothConnection::process_l2cap_config_response(uint8_t *data, uint16_t length) {
    // 48 20 12 0 e 0 1 0 5 0 a 0 70 0 0 0 0 0 1 2 30 0
    uint16_t scid = data[4] + ((uint16_t)data[5] << 8);
    uint16_t result = data[8] + ((uint16_t)data[9] << 8);
    DBGPrintf("    recv L2CAP config Response: ID: %d, Source:%x, Flags:%x, Result:%x",
              data[1], scid, data[6] + ((uint16_t)data[7] << 8), result );

    switch (result) {
        case 0x0000: DBGPrintf("(Success)"); break;
        case 0x0001: DBGPrintf("(Failure – unacceptable parameters)"); break;
        case 0x0002: DBGPrintf("(Failure – rejected (no reason provided))"); break;
        case 0x0003: DBGPrintf("(Failure – unknown options)"); break;
        case 0x0004: DBGPrintf("(Pending)"); break;
        case 0x0005: DBGPrintf("(Failure - flow spec rejected)"); break;
    }

    if (length > 10) {
        DBGPrintf(", Config: ");
        for (uint16_t i = 10; i < length; i++) DBGPrintf(" %02x", data[i]);
        for (uint16_t i = 10; i < length; i++) {
            if ((data[i] == 1) && (data[i+1] == 2)) {
                uint16_t mtu = data[i+2] + (data[i+3] << 8);
                DBGPrintf(" MTU:%u", mtu);
            }
            i += data[i+1] + 2;
        }
    }
    DBGPrintf("\n");

    if (scid == control_dcid_) {
        // Maybe we should avoid setting HID mode and only do 
        // so if the class of device is either/or MOUSE or Keyboard.
        // Set HID Boot mode
        // Don't do if PS3... Or if class told us not to
        if (device_class_ & 0xC0) {
            if (use_hid_protocol_) {
                // see what happens if I tell it to
                btController_->setHIDProtocol(HID_RPT_PROTOCOL);

            } else {
                // don't call through Null pointer
                if ((device_driver_ == nullptr) ||
                        !(device_driver_->special_process_required & BTHIDInput::SP_PS3_IDS)) {
                    btController_->setHIDProtocol(HID_BOOT_PROTOCOL);  //
                }
            }
        }
        //setHIDProtocol(HID_RPT_PROTOCOL);  //HID_RPT_PROTOCOL
        if ((btController_->do_pair_device_ || btController_->do_pair_ssp_) && !(device_driver_ && (device_driver_->special_process_required & BTHIDInput::SP_DONT_NEED_CONNECT))) {
            pending_control_tx_ = STATE_TX_SEND_CONNECT_INT;
        } else if (device_driver_ && (device_driver_->special_process_required & BTHIDInput::SP_NEED_CONNECT)) {
            DBGPrintf("   Needs connect to device INT(PS4?)\n");
            // The PS4 requires a connection request to it.
            pending_control_tx_ = STATE_TX_SEND_CONNECT_INT;
        } else if (connection_started_by_timer_) {            
            DBGPrintf("   Connection started by timeout continue to interrupt\n");
            // The PS4 requires a connection request to it.
            pending_control_tx_ = STATE_TX_SEND_CONNECT_INT;
        } else {
            btController_->pending_control_ = 0;
        }
        // Could be cleaner
        if (((device_class_ & 0xC0) == 0) && (pending_control_tx_ == STATE_TX_SEND_CONNECT_INT)) {
            // if we are not mouse and keybboard and we need to send the connect int
            // do it now.
            delay(1);
            connection_rxid_++;
            sendl2cap_ConnectionRequest(device_connection_handle_, connection_rxid_, interrupt_dcid_, HID_INTR_PSM);
            pending_control_tx_ = 0;
        }
    
        DBGPrintf("\tNew Pending control pair:%u driver:%p HID:%u TX: %x\n", btController_->do_pair_device_, device_driver_, check_for_hid_descriptor_, pending_control_tx_);
        connection_complete_ |= CCON_CONT;
    } else if (scid == interrupt_dcid_) {
        // Lets try SDP connect if we are not already connected.
        if (connection_started_by_timer_) {            
            DBGPrintf("   Connection started by timeout experiment continue to SDP\n");
        } else    
        if (!check_for_hid_descriptor_) connection_complete_ |= CCON_SDP;  // Don't force connect if no is asking for HID
        if ((connection_complete_ & CCON_SDP) == 0) connectToSDP(); // temp to see if we can do this later...

        // Enable SCan to page mode
        //connection_complete_ = true;
        //sendHCIWriteScanEnable(2);
        connection_complete_ |= CCON_INT;
    } else if (scid == sdp_dcid_) {
        // Enable SCan to page mode
        DBGPrintf("    SDP configuration complete\n");
        // Enable SCan to page mode
        connection_complete_ |= CCON_SDP;
        sdp_connected_ = true;
    }

    if (connection_complete_ == CCON_ALL) {
        btController_->sendHCIWriteScanEnable(2);
    }
}

void BluetoothConnection::process_l2cap_command_reject(uint8_t *data, uint16_t length) {
    // 48 20 b 0 7 0 70 0 *1 0 0 0 2 0 4
    DBGPrintf("    recv L2CAP command reject: ID: %d, length:%x, Reason:%x,  Data: %x %x \n",
              data[1], data[2] + ((uint16_t)data[3] << 8), data[4], data[5], data[6]);

}

void BluetoothConnection::process_l2cap_disconnect_request(uint8_t *data, uint16_t length) {
    uint16_t dcid = data[4] + ((uint16_t)data[5] << 8);
    uint16_t scid = data[6] + ((uint16_t)data[7] << 8);
    DBGPrintf("    recv L2CAP disconnect request: ID: %d, Length:%x, Dest:%x, Source:%x\n",
              data[1], data[2] + ((uint16_t)data[3] << 8), dcid, scid);
    // May need to respond in some cases...
    if (dcid == control_dcid_) {
        DBGPrintf("      Control disconnect request\n");
    } else if (dcid == interrupt_dcid_) {
        DBGPrintf("      Interrupt disconnect request\n");
    } else if (dcid == sdp_dcid_) {
        DBGPrintf("      SDP disconnect request\n");
        sdp_connected_ = false; // say we are not connected
        sendl2cap_DisconnectResponse(device_connection_handle_, data[1],
                                     sdp_dcid_,
                                     sdp_scid_);
    }

}

void BluetoothConnection::handleHIDTHDRData(uint8_t *data) {
    // Example
    //                      T HID data
    //48 20 d 0 9 0 71 0 a1 3 8a cc c5 a 23 22 79
    uint16_t len = data[4] + ((uint16_t)data[5] << 8);
    DBGPrintf("HID HDR Data: len: %d, Type: %d Con:%p\n", len, data[9], this);

    // ??? How to parse??? Use HID object???
    #if 0
    if (!find_driver_type_1_called_ && !device_driver_ && !have_hid_descriptor_) {
        // If we got to here and don't have driver or ... try once to get one
        DBGPrintf("\t $$$ No HID or Driver: See if one wants it now\n");
        // BUGBUG we initialize descriptor with name...
        device_driver_ = find_driver(descriptor_, 1);
    }
    #endif
    if (device_driver_) {
        device_driver_->process_bluetooth_HID_data(&data[9], len - 1); // We skip the first byte...
    } else if (have_hid_descriptor_) {
        // nead to bias to location within data.
        parse(0x0100 | data[9], &data[10], len - 2);
    } else {
        switch (data[9]) {
        case 1:
            DBGPrintf("    Keyboard report type\n");
            break;
        case 2:
            DBGPrintf("    Mouse report type\n");
            break;
        case 3:
            DBGPrintf("    Combo keyboard/pointing\n");
            break;
        default:
            DBGPrintf("    Unknown report\n");
        }
    }
}

void BluetoothConnection::handle_HCI_WRITE_SCAN_ENABLE_complete(uint8_t *rxbuf)
{
    // See if we have driver and a remote
    DBGPrintf("Write_Scan_enable Completed - connection state: %x\n", connection_complete_);
    if (connection_complete_ == CCON_ALL) {    // We have a driver call their
        if (device_driver_) {
            device_driver_->connectionComplete();
        } else if (check_for_hid_descriptor_) {
            have_hid_descriptor_ = false;
            sdp_buffer_ = descriptor_;
            sdp_buffer_len_ = sizeof(descriptor_);

            if (!startSDP_ServiceSearchAttributeRequest(0x206, 0x206, sdp_buffer_, sdp_buffer_len_)) {
                // Maybe try to claim_driver as we won't get a HID report.
                DBGPrintf("Failed to start SDP attribute request - so lets try again to find a driver");
                device_driver_ = find_driver(descriptor_, 1);
            }
        }
        connection_complete_ = 0;  // only call once
    }

}

void BluetoothConnection::handle_HCI_OP_ROLE_DISCOVERY_complete(uint8_t *rxbuf)
{
    // PS4 looks something like: 0E 07 01 09 08 00 47 00 01
    // Others look like        : 0E 07 01 09 08 00 48 00 00
    // last byte is the interesting one says what role. 
    DBGPrintf("ROLE_DISCOVERY Completed - Role: %x ", rxbuf[8]);
    if (rxbuf[8] == 0) {
        DBGPrintf("(Central)\n");

        // Set a timeout
        btController_->setTimer(this, CONNECTION_TIMEOUT_US);

    } else {
        DBGPrintf("(Peripheral)\n");

        // Should maybe do some state checking before doing this, but...
        DBGPrintf("\t** We will issue connection requsts **\n ");
        connection_started_ = true;
        sendl2cap_ConnectionRequest(device_connection_handle_, connection_rxid_, control_dcid_, HID_CTRL_PSM);
    }
}

void BluetoothConnection::timer_event(USBDriverTimer *whichTimer)
{
    DBGPrintf("BluetoothConnection::timer_event(%p) : %u\n", this, connection_started_);

    if (whichTimer == &bt_connection_timer_) {
        if (device_driver_) {
            device_driver_->bt_hid_timer_event(whichTimer);
        }
    } else if (!connection_started_) {
        DBGPrintf("\t** Timed out now try issue connection requsts **\n ");
        connection_started_ = true;
        connection_started_by_timer_ = true;
        connection_rxid_++;
        sendl2cap_ConnectionRequest(device_connection_handle_, connection_rxid_, control_dcid_, HID_CTRL_PSM);
    }
}    

// l2cap support functions.
void BluetoothConnection::sendl2cap_ConnectionResponse(uint16_t handle, uint8_t rxid, uint16_t dcid, uint16_t scid, uint8_t result) {
    uint8_t l2capbuf[12];
    l2capbuf[0] = L2CAP_CMD_CONNECTION_RESPONSE; // Code
    l2capbuf[1] = rxid; // Identifier
    l2capbuf[2] = 0x08; // Length
    l2capbuf[3] = 0x00;
    l2capbuf[4] = dcid & 0xff; // Destination CID
    l2capbuf[5] = dcid >> 8;
    l2capbuf[6] = scid & 0xff; // Source CID
    l2capbuf[7] = scid >> 8;
    l2capbuf[8] = result; // Result: Pending or Success
    l2capbuf[9] = 0x00;
    l2capbuf[10] = 0x00; // No further information
    l2capbuf[11] = 0x00;

    DBGPrintf("\nSend L2CAP_CMD_CONNECTION_RESPONSE(RXID:%x, DCID:%x, SCID:%x RES:%x)\n", rxid, dcid, scid, result);
    btController_->sendL2CapCommand(handle, l2capbuf, sizeof(l2capbuf));
}



void BluetoothConnection::sendl2cap_ConnectionRequest(uint16_t handle, uint8_t rxid, uint16_t scid, uint16_t psm) {
    uint8_t l2capbuf[8];
    l2capbuf[0] = L2CAP_CMD_CONNECTION_REQUEST; // Code
    l2capbuf[1] = rxid; // Identifier
    l2capbuf[2] = 0x04; // Length
    l2capbuf[3] = 0x00;
    l2capbuf[4] = (uint8_t)(psm & 0xff); // PSM
    l2capbuf[5] = (uint8_t)(psm >> 8);
    l2capbuf[6] = scid & 0xff; // Source CID
    l2capbuf[7] = (scid >> 8) & 0xff;

    DBGPrintf("\nSend ConnectionRequest (RXID:%x, SCID:%x, PSM:%x)\n", rxid, scid, psm);
    btController_->sendL2CapCommand(handle, l2capbuf, sizeof(l2capbuf));
}

void BluetoothConnection::sendl2cap_ConfigRequest(uint16_t handle, uint8_t rxid, uint16_t dcid) {
    uint8_t l2capbuf[12];
    l2capbuf[0] = L2CAP_CMD_CONFIG_REQUEST; // Code
    l2capbuf[1] = rxid; // Identifier
    l2capbuf[2] = 0x08; // Length
    l2capbuf[3] = 0x00;
    l2capbuf[4] = dcid & 0xff; // Destination CID
    l2capbuf[5] = (dcid >> 8) & 0xff;
    l2capbuf[6] = 0x00; // Flags
    l2capbuf[7] = 0x00;
    l2capbuf[8] = 0x01; // Config Opt: type = MTU (Maximum Transmission Unit) - Hint
    l2capbuf[9] = 0x02; // Config Opt: length
    l2capbuf[10] = 0xFF; // MTU
    l2capbuf[11] = 0xFF;

    DBGPrintf("\nsend L2CAP_ConfigRequest(RXID:%x, DCID:%x)\n", rxid, dcid);
    btController_->sendL2CapCommand(handle, l2capbuf, sizeof(l2capbuf));
}

void BluetoothConnection::sendl2cap_ConfigResponse(uint16_t handle, uint8_t rxid, uint16_t scid, uint16_t mtu) {
    uint8_t l2capbuf[10];
    l2capbuf[0] = L2CAP_CMD_CONFIG_RESPONSE; // Code
    l2capbuf[1] = rxid; // Identifier
    l2capbuf[2] = 0x06; // try not sending mtu 0x0A; // Length
    l2capbuf[3] = 0x00;
    l2capbuf[4] = scid & 0xff; // Source CID
    l2capbuf[5] = (scid >> 8) & 0xff;
    l2capbuf[6] = 0x00; // Flag
    l2capbuf[7] = 0x00;
    l2capbuf[8] = 0x00; // Result
    l2capbuf[9] = 0x00;
    //l2capbuf[10] = 0x01; // Config
    //l2capbuf[11] = 0x02;
    //l2capbuf[12] = mtu & 0xff; //0xA0;
    //l2capbuf[13] = mtu >> 8; //0x02;

    DBGPrintf("\nsend L2CAP_ConfigResponse(RXID:%x, SCID:%x)\n", rxid, scid);
    btController_->sendL2CapCommand(handle, l2capbuf, sizeof(l2capbuf));
}

void BluetoothConnection::sendl2cap_DisconnectResponse(uint16_t handle, uint8_t rxid, uint16_t dcid, uint16_t scid) {
    uint8_t l2capbuf[8];
    l2capbuf[0] = L2CAP_CMD_DISCONNECT_RESPONSE; // Code
    l2capbuf[1] = rxid; // Identifier
    l2capbuf[2] = 0x04; // Length
    l2capbuf[3] = 0x00;
    l2capbuf[4] = dcid & 0xff; // dcid CID
    l2capbuf[5] = (dcid >> 8) & 0xff;
    l2capbuf[6] = scid & 0xff; // SCID CID
    l2capbuf[7] = (scid >> 8) & 0xff;

    DBGPrintf("\nsend L2CAP_DisconnectResponse(RXID:%x, DCID:%x, SCID:%x)\n", rxid, dcid, scid);
    btController_->sendL2CapCommand(handle, l2capbuf, sizeof(l2capbuf));
}

//=============================================================================
// Process the SDP stuff.
//=============================================================================
bool BluetoothConnection::startSDP_ServiceSearchAttributeRequest(uint16_t range_low, uint16_t range_high, uint8_t *buffer, uint32_t cb)
{
    if (!sdp_connected_) return false;

    sdp_request_range_low_ = range_low;
    sdp_reqest_range_high_ = range_high;
    sdp_request_buffer_ = buffer;
    sdp_request_buffer_cb_ = cb;
    sdp_request_buffer_used_cnt_ = 0; // cnt in bytes used.
    sdp_request_completed_ = false;

    // So start it up
    send_SDP_ServiceSearchAttributeRequest(nullptr, 0);
    return true;
}


void BluetoothConnection::send_SDP_ServiceSearchRequest(uint8_t *continue_state, uint8_t cb)
{
    // Example message: first part l2cap. Which will let them fill that in
    //  l2cap: 0x47 0x0 0x18 0x0 0x14 0x0 0x42 0x0
    //  PSM header: 0x6 0x0 0x0 0x0 0xf
    //  req: 0x35 0x3 0x19 0x1 0x0 0xff 0xff 0x35 0x5 0xa 0x0 0x0 0xff 0xff 0x0

    uint8_t sdpcmdbuf[30]; // 20 + up to 10 continue
    seq_number_++;
    uint16_t packet_size = (13 - 5) + cb;

    sdpcmdbuf[0] = 0x2; // SDP_ServiceSearchRequest
    sdpcmdbuf[1] = seq_number_ >> 8;   // Sequence number
    sdpcmdbuf[2] = seq_number_ & 0xff; //
    sdpcmdbuf[3] = packet_size >> 8;   // Data in the rest...
    sdpcmdbuf[4] = packet_size & 0xff; //

    // Now lets build the attribute request data.
    // 0x35 0x3 0x19 0x1 0x0 0xff 0xff 0x35 0x5 0xa 0x0 0x0 0xff 0xff 0x2 0x0 0x26

    sdpcmdbuf[5] = 0x35; // type sequence
    sdpcmdbuf[6] = 0x03;  //   3 bytes
    sdpcmdbuf[7] = 0x19;  //  UUID 2 bytes
    sdpcmdbuf[8] = 0x01;  //   UUID
    sdpcmdbuf[9] = 0x00;  //   UUID
    sdpcmdbuf[10] = 0xff;  //  MAX attributes
    sdpcmdbuf[11] = 0xff; //
    sdpcmdbuf[12] = cb;    // Count of continuation bytes
    for (uint8_t i = 0; i < cb; i++) sdpcmdbuf[13 + i] = continue_state[i];

    // Now lets try to send the packet
    btController_->sendL2CapCommand(sdpcmdbuf, 13 + cb, sdp_scid_);
}

void BluetoothConnection::send_SDP_ServiceSearchAttributeRequest(uint8_t *continue_state, uint8_t cb)
{
    // Example message: first part l2cap. Which will let them fill that in
    //  l2cap: 0x47 0x0 0x18 0x0 0x14 0x0 0x42 0x0
    //  PSM header: 0x6 0x0 0x0 0x0 0xf
    //  req: 0x35 0x3 0x19 0x1 0x0 0xff 0xff 0x35 0x5 0xa 0x0 0x0 0xff 0xff 0x0

    uint16_t continue_data_offset = 17;
    uint8_t sdpcmdbuf[37]; // 20 + up to 17 continue
    seq_number_++;


    sdpcmdbuf[0] = 0x6; // SDP_ServiceSearchAttributeReques
    sdpcmdbuf[1] = seq_number_ >> 8;   // Sequence number
    sdpcmdbuf[2] = seq_number_ & 0xff; //
    //sdpcmdbuf[3] = packet_size >> 8;   // Data in the rest...
    //sdpcmdbuf[4] = packet_size & 0xff; //

    // Now lets build the attribute request data.
    // 0x35 0x3 0x19 0x1 0x0 0xff 0xff 0x35 0x5 0xa 0x0 0x0 0xff 0xff 0x2 0x0 0x26

    sdpcmdbuf[5] = 0x35; // type sequence
    sdpcmdbuf[6] = 0x03;  //   3 bytes
    sdpcmdbuf[7] = 0x19;  //  UUID 2 bytes
    sdpcmdbuf[8] = 0x01;  //   UUID
    sdpcmdbuf[9] = 0x00;  //   UUID
    sdpcmdbuf[10] = 0xff;  //  MAX size
    sdpcmdbuf[11] = 0xff; //
    sdpcmdbuf[12] = 0x35; // Sequence

    if (sdp_request_range_low_ == sdp_reqest_range_high_) {
        // doing specific
        sdpcmdbuf[13] = 0x03;  //  3 bytes
        sdpcmdbuf[14] = 0x09;  //  2 byte integer
        sdpcmdbuf[15] = sdp_request_range_low_ >> 8;  //    Attribute low
        sdpcmdbuf[16] = sdp_request_range_low_ & 0xff;  //
    } else {
        // doing range
        sdpcmdbuf[13] = 0x05;  //  5 bytes
        sdpcmdbuf[14] = 0x0A;  //  4 byte integer
        sdpcmdbuf[15] = sdp_request_range_low_ >> 8;  //    Attribute low
        sdpcmdbuf[16] = sdp_request_range_low_ & 0xff;  //
        sdpcmdbuf[17] = sdp_reqest_range_high_ & 0xff;  //    high
        sdpcmdbuf[18] = sdp_reqest_range_high_ & 0xff;  //    high
        continue_data_offset = 19;
    }
    sdpcmdbuf[continue_data_offset++] = cb;    // Count of continuation bytes
    for (uint8_t i = 0; i < cb; i++) sdpcmdbuf[continue_data_offset + i] = continue_state[i];

    uint16_t packet_size = (continue_data_offset - 5) + cb;
    sdpcmdbuf[3] = packet_size >> 8;   // Data in the rest...
    sdpcmdbuf[4] = packet_size & 0xff; //

    // Now lets try to send the packet
    btController_->sendL2CapCommand(sdpcmdbuf, continue_data_offset + cb, sdp_scid_);
}

//----------------------------------------------------------------
// Some SDP stuff.
void BluetoothConnection::process_sdp_service_search_request(uint8_t *data) {
    DBGPrintf("process_sdp_service_search_request\n");
}

void BluetoothConnection::process_sdp_service_search_response(uint8_t *data) {
    DBGPrintf("process_sdp_service_search_response\n");

    // (processed before us)(0)  48 20 1A 00 16 00 40 00
    // (0)  03 00 01 00 11
    // (5) 00 03 00 03 00 00 00 00 00 01 00 00 00 01 00 01 00
    uint16_t total_count = (data[5] << 8) + data[6];
    uint16_t current_count = (data[7] << 8) + data[8];
    uint8_t offset = 9;
    DBGPrintf("\tTotal Count:%u Current:%u", total_count, current_count);
    for (uint8_t i = 0; i < current_count; i++) {
        uint32_t srh = ((uint32_t)data[offset + 0] << 24) + ((uint32_t)data[offset + 1] << 16) +
                       ((uint32_t)data[offset + 2] << 8) + (data[offset + 3]);
        DBGPrintf(" %08X", srh);
        offset += 4;
    }
    uint8_t continue_state_count = data[offset++];
    DBGPrintf(" Cont CB:%u", continue_state_count);
    for (uint8_t i = 0; i < continue_state_count; i++) DBGPrintf(" %02X",  data[offset++]);
    DBGPrintf("\n");
}

void BluetoothConnection::process_sdp_service_attribute_request(uint8_t *data) {
    DBGPrintf("process_sdp_service_attribute_request\n");

}

void BluetoothConnection::process_sdp_service_attribute_response(uint8_t *data) {
    DBGPrintf("process_sdp_service_attribute_response:\n");
}

void BluetoothConnection::process_sdp_service_search_attribute_request(uint8_t *data) {
    DBGPrintf("\n### process_sdp_service_search_attribute_request ###\n");
    // Print out the data like UHS2
#ifdef DEBUG_BT
    uint16_t service = data[1] << 8 | data[2];
    uint16_t length = data[3] << 8 | data[4];
    uint16_t uuid = (data[8] << 8 | data[9]);
    if (uuid == 0x0000) // Check if it's sending the UUID as a 128-bit UUID
        uuid = (data[10] << 8 | data[11]);

    DBGPrintf("  Service:%x UUID:%x Length:%u\n", service, uuid, length);
    DBGPrintf("  Data:");
    for (uint8_t i = 0; i < length; i++) {
        DBGPrintf("%02x ", data[5 + i]);
    }
    DBGPrintf("\n");
#endif
    // lets respond saying we don't support the service
    uint8_t l2capbuf[10];
    l2capbuf[0] = SDP_SERVICE_SEARCH_ATTRIBUTE_RESPONSE;
    l2capbuf[1] = data[1];
    l2capbuf[2] = data[2];
    l2capbuf[3] = 0x00; // MSB Parameter Length
    l2capbuf[4] = 0x05; // LSB Parameter Length = 5
    l2capbuf[5] = 0x00; // MSB AttributeListsByteCount
    l2capbuf[6] = 0x02; // LSB AttributeListsByteCount = 2

    /* Attribute ID/Value Sequence: */
    l2capbuf[7] = 0x35; // Data element sequence - length in next byte
    l2capbuf[8] = 0x00; // Length = 0
    l2capbuf[9] = 0x00; // No continuation state

    DBGPrintf("Send SDP_SERVICE_SEARCH_ATTRIBUTE_RESPONSE not supported\n");
    btController_->sendL2CapCommand(device_connection_handle_, l2capbuf, sizeof(l2capbuf),
                                    sdp_scid_ & 0xff, sdp_scid_ >> 8);

}

void BluetoothConnection::process_sdp_service_search_attribute_response(uint8_t *data) {
    //before :0B 20 34 00 30 00 40 00
    // (00) 07 00 01 00 2B
    // (5) 00 26  // cb_data
    // (7) 36 03 A2 36 00 8E 09 00 00 0A 00 00 00 00 09 00 01 35 03 19 10 00 09 00 04 35 0D 35 06 19 01 00 09 00 01 35 03 19 // data
    // (7 + cb_data) 02 00 26 //

    uint16_t cb_data = (data[5] << 8) + data[6];
    uint8_t cb_cont = data[7 + cb_data];
    uint8_t *pb_cont = &data[8 + cb_data];

    DBGPrintf("process_sdp_service_search_attribute_response: cb dat:%u cont:%u ", cb_data, cb_cont);
    for (uint8_t i = 0; i < cb_cont; i++) DBGPrintf(" %02X", pb_cont[i]);
    DBGPrintf("\n");

    // lets copy the data in...
    uint16_t cb_space = sdp_request_buffer_cb_ - sdp_request_buffer_used_cnt_;
    if (cb_data > cb_space) cb_data = cb_space;
    memcpy(&sdp_request_buffer_[sdp_request_buffer_used_cnt_], &data[7], cb_data);
    sdp_request_buffer_used_cnt_ += cb_data;

    // Now see if we are done or not, if not start up next request
    if ((cb_cont == 0) || (sdp_request_buffer_used_cnt_ == sdp_request_buffer_cb_)) {
        sdp_request_completed_ = true;
        if (device_driver_) {
            device_driver_->sdp_command_completed(true); // We skip the first byte...
        } else {
            // Must be our own... we should now try to proces it.
            completeSDPRequest(true);
        }
    } else {
        send_SDP_ServiceSearchAttributeRequest(pb_cont, cb_cont);

    }
}

//=============================================================================
// HID SUPPORT - maybe split out
//=============================================================================
// lets use the SDP Support functions to try to retrieve the
// HID Descriptor using SDP
//=============================================================================

bool BluetoothConnection::completeSDPRequest(bool success)
{

    if (!success) return false;
    // Now real hack:
    // Lets see if we can now print out the report descriptor.
    uint32_t cb_left = sdp_request_buffer_used_cnt_;
    uint8_t *pb = sdp_buffer_; // start at second byte;

    sdp_element_t sdpe;
    bool found_report_attribute = false; 
    while (cb_left > 0) {
        int cb = extract_next_SDP_Token(pb, cb_left, sdpe);
        if (cb <= 0 ) break;
        // Should do a lot more validation, but ...
        // At least check that we see a value with our attribute number
        if ((sdpe.dtype == DU32) && (sdpe.data.uw == 0x206)) found_report_attribute = true;

        if (found_report_attribute && (sdpe.element_type == 4) && (sdpe.dtype == DPB)) {
            descsize_ = sdpe.element_size;
            memcpy(descriptor_, sdpe.data.pb, descsize_);
            dumpHIDReportDescriptor();
            have_hid_descriptor_ = true;
            parse();
            return true;
        }

        cb_left -= cb;
        pb += cb;
    }
    return false;
}




//BUGBUG: move to class or ...

int BluetoothConnection::extract_next_SDP_Token(uint8_t *pbElement, int cb_left, sdp_element_t &sdpe) {
    uint8_t element = *pbElement; // first byte is type of element
    sdpe.element_type = element >> 3;
    sdpe.element_size = element & 7;
    sdpe.data.luw = 0; // start off 0

    int size_of_element = -1; // should never use this initialized value
    switch (sdpe.element_size) {
        case 0: size_of_element = 2; break;
        case 1: size_of_element = 3; break;
        case 2: size_of_element = 5; break;
        case 3: size_of_element = 9; break;
        case 4: size_of_element = 17; break;
        case 5: size_of_element = pbElement[1] + 2; break;
        case 6: size_of_element = (pbElement[1] << 8) + pbElement[2] + 3; break;
        case 7: size_of_element = (uint32_t)(pbElement[1] << 24) + (uint32_t)(pbElement[2] << 16) + (pbElement[3] << 8) + pbElement[4] + 5; break;
    }

    DBGPrintf("extract_next_SDP_Token %p %x %u %u ", pbElement , element, sdpe.element_type, sdpe.element_size);

    switch (element) {
    case 0: // nil
        sdpe.dtype = DNIL;
        VDBGPrintf("(NIL)");
        break;

    case 0x08: // unsigned one byte
    case 0x18: // UUID one byte
    case 0x28: // bool one byte
        sdpe.dtype = DU32;
        sdpe.data.uw = pbElement[1];
        VDBGPrintf("(DU32 %u)", sdpe.data.uw);
        break;

    case 0x09: // unsigned 2  byte
    case 0x19: // uuid 2  byte
        sdpe.dtype = DU32;
        sdpe.data.uw = (pbElement[1] << 8) + pbElement[2];
        VDBGPrintf("(DU32 %u)", sdpe.data.uw);
        break;
    case 0x0A: // unsigned 4  byte
    case 0x1A: // UUID 4  byte
        sdpe.dtype = DU32;
        sdpe.data.uw =  (uint32_t)(pbElement[1] << 24) + (uint32_t)(pbElement[2] << 16) + (pbElement[3] << 8) + pbElement[4];
        VDBGPrintf("(DU32 %u)", sdpe.data.uw);
        break;
    case 0x0B: // unsigned 8  byte
        sdpe.dtype = DU64;
        sdpe.data.luw =  ((uint64_t)pbElement[1] << 52) + ((uint64_t)pbElement[2] << 48) + ((uint64_t)pbElement[3] << 40) + ((uint64_t)pbElement[4] << 32) +
                         (uint32_t)(pbElement[5] << 24) + (uint32_t)(pbElement[6] << 16) + (pbElement[7] << 8) + pbElement[8];
        VDBGPrintf("(DU64 %llu)", sdpe.data.luw);
        break;

    // type = 2 signed
    case 0x10: // unsigned one byte
        sdpe.dtype = DS32;
        sdpe.data.sw = (int8_t)pbElement[1];
        VDBGPrintf("(DS32 %u)", sdpe.data.uw);
        break;
    case 0x11: // unsigned 2  byte
        sdpe.dtype = DS32;
        sdpe.data.sw = (int16_t)((pbElement[1] << 8) + pbElement[2]);
        break;

    case 0x12: // unsigned 4  byte
        sdpe.dtype = DS32;
        sdpe.data.sw =  (int32_t)((uint32_t)(pbElement[1] << 24) + (uint32_t)(pbElement[2] << 16) + (pbElement[3] << 8) + pbElement[4]);
        VDBGPrintf("(DS32 %u)", sdpe.data.uw);
        break;
    case 0x13: //
        sdpe.dtype = DS64;
        sdpe.data.lsw =  (int64_t)(((uint64_t)pbElement[1] << 52) + ((uint64_t)pbElement[2] << 48) + ((uint64_t)pbElement[3] << 40) + ((uint64_t)pbElement[4] << 32) +
                                   (uint32_t)(pbElement[5] << 24) + (uint32_t)(pbElement[6] << 16) + (pbElement[7] << 8) + pbElement[8]);
        VDBGPrintf("(DS32 %u)", sdpe.data.uw);
        break;

    // string one byte size.
    case 0x25:
        sdpe.dtype = DPB;
        sdpe.element_size = pbElement[1];
        sdpe.data.pb = &pbElement[2];
        VDBGPrintf("(DPB %02x %02x %02x)", sdpe.data.pb[0], sdpe.data.pb[1], sdpe.data.pb[2]);
        break;

    case 0x26:
        sdpe.dtype = DPB;
        sdpe.element_size = (pbElement[1] << 8) + pbElement[2];
        sdpe.data.pb = &pbElement[3];
        VDBGPrintf("(DPB %02x %02x %02x)", sdpe.data.pb[0], sdpe.data.pb[1], sdpe.data.pb[2]);
        break;

    // type = 7 Data element sequence
    case 0x35: //
    case 0x3D: //
        sdpe.dtype = DLVL;
        sdpe.element_size = pbElement[1];
        sdpe.data.pb = &pbElement[2];
        size_of_element = 2; // we don't advance through hole thing only header
        VDBGPrintf("(DLVL)");
        break;
    case 0x36: //
    case 0x3E: //
        sdpe.dtype = DLVL;
        sdpe.element_size = (pbElement[1] << 8) + pbElement[2];
        sdpe.data.pb = &pbElement[3];
        size_of_element = 3; // we don't advance through hole thing only header
        VDBGPrintf("(DLVL)");
        break;
    case 0x37: //
    case 0x3F: //
        sdpe.dtype = DLVL;
        sdpe.element_size = (uint32_t)(pbElement[1] << 24) + (uint32_t)(pbElement[2] << 16) + (pbElement[3] << 8) + pbElement[4];
        sdpe.data.pb = &pbElement[5];
        size_of_element = 5; // we don't advance through hole thing only header
        VDBGPrintf("(DLVL)");
        break;
    default:
        sdpe.dtype = DUNKOWN;
        DBGPrintf("### DECODE failed %x type:%u size:%u cb:%u ###\n", element, sdpe.element_type, sdpe.element_size, size_of_element);
        break;
    }
    DBGPrintf(" %u %u\n", sdpe.dtype, size_of_element);
    return size_of_element;
}




void BluetoothConnection::dumpHIDReportDescriptor() {
#ifdef DEBUG_BT_VERBOSE
    uint8_t *pb = descriptor_;
    uint16_t cb = descsize_;
    const uint8_t *p = pb;
    uint16_t report_size = cb;

    const uint8_t *pend = p + report_size;
    uint8_t collection_level = 0;
    uint16_t usage_page = 0;
    enum { USAGE_LIST_LEN = 24 };
    uint16_t usage[USAGE_LIST_LEN] = { 0, 0 };
    uint8_t usage_count = 0;
    uint32_t topusage;
    //cnt_feature_reports_ = 0;
    //uint8_t last_report_id = 0;
    DBGPrintf("\nHID Report Descriptor (%p) size: %u\n", p, report_size);
    while (p < pend) {
        uint8_t tag = *p;
        for (uint8_t i = 0; i < collection_level; i++) DBGPrintf("  ");
        DBGPrintf("  %02X", tag);

        if (tag == 0xFE) {  // Long Item (unsupported)
            p += p[1] + 3;
            continue;
        }
        uint32_t val;
        switch (tag & 0x03) {  // Short Item data
        case 0:
            val = 0;
            p++;
            break;
        case 1:
            val = p[1];
            // could be better;
            DBGPrintf(" %02X", p[1]);
            p += 2;
            break;
        case 2:
            val = p[1] | (p[2] << 8);
            DBGPrintf(" %02X %02X", p[1], p[2]);
            p += 3;
            break;
        case 3:
            val = p[1] | (p[2] << 8) | (p[3] << 16) | (p[4] << 24);
            DBGPrintf(" %02X %02X %02X %02X", p[1], p[2], p[3], p[4]);
            p += 5;
            break;
        }
        if (p > pend) break;

        bool reset_local = false;
        switch (tag & 0xfc) {
        case 0x4:  //usage Page
        {
            usage_page = val;
            DBGPrintf("\t// Usage Page(%x) - ", val);
            switch (usage_page) {
            case 0x01: DBGPrintf("Generic Desktop"); break;
            case 0x06: DBGPrintf("Generic Device Controls"); break;
            case 0x07: DBGPrintf("Keycode"); break;
            case 0x08: DBGPrintf("LEDs"); break;
            case 0x09: DBGPrintf("Button"); break;
            case 0x0C: DBGPrintf("Consumer"); break;
            case 0x0D:
            case 0xFF0D: DBGPrintf("Digitizer"); break;
            default:
                if (usage_page >= 0xFF00) DBGPrintf("Vendor Defined");
                else DBGPrintf("Other ?");
                break;
            }
        }
        break;
        case 0x08:  //usage
            DBGPrintf("\t// Usage(%x) -", val);
            printUsageInfo(usage_page, val);
            if (usage_count < USAGE_LIST_LEN) {
                // Usages: 0 is reserved 0x1-0x1f is sort of reserved for top level things like
                // 0x1 - Pointer - A collection... So lets try ignoring these
                if (val > 0x1f) {
                    usage[usage_count++] = val;
                }
            }
            break;
        case 0x14:  // Logical Minimum (global)
            DBGPrintf("\t// Logical Minimum(%x)", val);
            break;
        case 0x24:  // Logical Maximum (global)
            DBGPrintf("\t// Logical maximum(%x)", val);
            break;
        case 0x74:  // Report Size (global)
            DBGPrintf("\t// Report Size(%x)", val);
            break;
        case 0x94:  // Report Count (global)
            DBGPrintf("\t// Report Count(%x)", val);
            break;
        case 0x84:  // Report ID (global)
            DBGPrintf("\t// Report ID(%x)", val);
            //last_report_id = val;
            break;
        case 0x18:  // Usage Minimum (local)
            usage[0] = val;
            usage_count = 255;
            DBGPrintf("\t// Usage Minimum(%x) - ", val);
            printUsageInfo(usage_page, val);
            break;
        case 0x28:  // Usage Maximum (local)
            usage[1] = val;
            usage_count = 255;
            DBGPrintf("\t// Usage Maximum(%x) - ", val);
            printUsageInfo(usage_page, val);
            break;
        case 0xA0:  // Collection
            DBGPrintf("\t// Collection(%x)", val);
            // discard collection info if not top level, hopefully that's ok?
            if (collection_level == 0) {
                topusage = ((uint32_t)usage_page << 16) | usage[0];
                DBGPrintf(" top Usage(%x)", topusage);
                collection_level++;
            }
            reset_local = true;
            break;
        case 0xC0:  // End Collection
            DBGPrintf("\t// End Collection");
            if (collection_level > 0) collection_level--;
            break;

        case 0x80:  // Input
            DBGPrintf("\t// Input(%x)\t// (", val);
            print_input_output_feature_bits(val);
            reset_local = true;
            break;
        case 0x90:  // Output
            DBGPrintf("\t// Output(%x)\t// (", val);
            print_input_output_feature_bits(val);
            reset_local = true;
            break;
        case 0xB0:  // Feature
            DBGPrintf("\t// Feature(%x)\t// (", val);
            print_input_output_feature_bits(val);
            //if (cnt_feature_reports_ < MAX_FEATURE_REPORTS) {
            //  feature_report_ids_[cnt_feature_reports_++] = last_report_id;
            //}
            reset_local = true;
            break;

        case 0x34:  // Physical Minimum (global)
            DBGPrintf("\t// Physical Minimum(%x)", val);
            break;
        case 0x44:  // Physical Maximum (global)
            DBGPrintf("\t// Physical Maximum(%x)", val);
            break;
        case 0x54:  // Unit Exponent (global)
            DBGPrintf("\t// Unit Exponent(%x)", val);
            break;
        case 0x64:  // Unit (global)
            DBGPrintf("\t// Unit(%x)", val);
            break;
        }
        if (reset_local) {
            usage_count = 0;
            usage[0] = 0;
            usage[1] = 0;
        }

        DBGPrintf("\n");
    }
#endif
}

#ifdef DEBUG_BT_VERBOSE
void BluetoothConnection::print_input_output_feature_bits(uint8_t val) {
    DBGPrintf((val & 0x01) ? "Constant" : "Data");
    DBGPrintf((val & 0x02) ? ", Variable" : ", Array");
    DBGPrintf((val & 0x04) ? ", Relative" : ", Absolute");
    if (val & 0x08) DBGPrintf(", Wrap");
    if (val & 0x10) DBGPrintf(", Non Linear");
    if (val & 0x20) DBGPrintf(", No Preferred");
    if (val & 0x40) DBGPrintf(", Null State");
    if (val & 0x80) DBGPrintf(", Volatile");
    if (val & 0x100) DBGPrintf(", Buffered Bytes");
    DBGPrintf(")");
}

void BluetoothConnection::printUsageInfo(uint8_t usage_page, uint16_t usage) {
  switch (usage_page) {
    case 1:  // Generic Desktop control:
      switch (usage) {
        case 0x01: DBGPrintf("(Pointer)"); break;
        case 0x02: DBGPrintf("(Mouse)"); break;
        case 0x04: DBGPrintf("(Joystick)"); break;
        case 0x05: DBGPrintf("(Gamepad)"); break;
        case 0x06: DBGPrintf("(Keyboard)"); break;
        case 0x07: DBGPrintf("(Keypad)"); break;

        case 0x30: DBGPrintf("(X)"); break;
        case 0x31: DBGPrintf("(Y)"); break;
        case 0x32: DBGPrintf("(Z)"); break;
        case 0x33: DBGPrintf("(Rx)"); break;
        case 0x34: DBGPrintf("(Ry)"); break;
        case 0x35: DBGPrintf("(Rz)"); break;
        case 0x36: DBGPrintf("(Slider)"); break;
        case 0x37: DBGPrintf("(Dial)"); break;
        case 0x38: DBGPrintf("(Wheel)"); break;
        case 0x39: DBGPrintf("(Hat)"); break;
        case 0x3D: DBGPrintf("(Start)"); break;
        case 0x3E: DBGPrintf("(Select)"); break;
        case 0x40: DBGPrintf("(Vx)"); break;
        case 0x41: DBGPrintf("(Vy)"); break;
        case 0x42: DBGPrintf("(Vz)"); break;
        case 0x43: DBGPrintf("(Vbrx)"); break;
        case 0x44: DBGPrintf("(Vbry)"); break;
        case 0x45: DBGPrintf("(Vbrz)"); break;
        case 0x46: DBGPrintf("(Vno)"); break;
        case 0x81: DBGPrintf("(System Power Down)"); break;
        case 0x82: DBGPrintf("(System Sleep)"); break;
        case 0x83: DBGPrintf("(System Wake Up)"); break;
        case 0x90: DBGPrintf("(D-Up)"); break;
        case 0x91: DBGPrintf("(D-Dn)"); break;
        case 0x92: DBGPrintf("(D-Right)"); break;
        case 0x93: DBGPrintf("(D-Left)"); break;
        default:
          DBGPrintf("(?)");
          break;
      }
      break;
    case 6:  // Generic Desktop control:
      switch (usage) {
        case 0x020: DBGPrintf("(Battery Strength)"); break;
        case 0x21: DBGPrintf("(Wireless Channel)"); break;
        case 0x22: DBGPrintf("(Wireless ID)"); break;
        case 0x23: DBGPrintf("(Discover Wireless Ctrl)"); break;
        case 0x24: DBGPrintf("(Security Code Entered)"); break;
        case 0x25: DBGPrintf("(Security Code erase)"); break;
        case 0x26: DBGPrintf("(Security Code cleared)");
        default: DBGPrintf("(?)"); break;
      }
      break;
    case 7: // keyboard/keycode
      switch (usage) {
        case 0x04: DBGPrintf("(a and A)"); break;
        case 0x05: DBGPrintf("(b and B)"); break;
        case 0x06: DBGPrintf("(c and C)"); break;
        case 0x07: DBGPrintf("(d and D)"); break;
        case 0x08: DBGPrintf("(e and E)"); break;
        case 0x09: DBGPrintf("(f and F)"); break;
        case 0x0A: DBGPrintf("(g and G)"); break;
        case 0x0B: DBGPrintf("(h and H)"); break;
        case 0x0C: DBGPrintf("(i and I)"); break;
        case 0x0D: DBGPrintf("(j and J)"); break;
        case 0x0E: DBGPrintf("(k and K)"); break;
        case 0x0F: DBGPrintf("(l and L)"); break;
        case 0x10: DBGPrintf("(m and M)"); break;
        case 0x11: DBGPrintf("(n and N)"); break;
        case 0x12: DBGPrintf("(o and O)"); break;
        case 0x13: DBGPrintf("(p and P)"); break;
        case 0x14: DBGPrintf("(q and Q)"); break;
        case 0x15: DBGPrintf("(r and R)"); break;
        case 0x16: DBGPrintf("(s and S)"); break;
        case 0x17: DBGPrintf("(t and T)"); break;
        case 0x18: DBGPrintf("(u and U)"); break;
        case 0x19: DBGPrintf("(v and V)"); break;
        case 0x1A: DBGPrintf("(w and W)"); break;
        case 0x1B: DBGPrintf("(x and X)"); break;
        case 0x1C: DBGPrintf("(y and Y)"); break;
        case 0x1D: DBGPrintf("(z and Z)"); break;
        case 0x1E: DBGPrintf("(1 and !)"); break;
        case 0x1F: DBGPrintf("(2 and @)"); break;
        case 0x20: DBGPrintf("(3 and #)"); break;
        case 0x21: DBGPrintf("(4 and $)"); break;
        case 0x22: DBGPrintf("(5 and %)"); break;
        case 0x23: DBGPrintf("(6 and ^)"); break;
        case 0x24: DBGPrintf("(7 and &)"); break;
        case 0x25: DBGPrintf("(8 and *)"); break;
        case 0x26: DBGPrintf("(9 and ()"); break;
        case 0x27: DBGPrintf("(0 and ))"); break;
        case 0x28: DBGPrintf("(Return (ENTER))"); break;
        case 0x29: DBGPrintf("(ESCAPE)"); break;
        case 0x2A: DBGPrintf("(DELETE (Backspace))"); break;
        case 0x2B: DBGPrintf("(Tab)"); break;
        case 0x2C: DBGPrintf("(Spacebar)"); break;
        case 0x2D: DBGPrintf("(- and (underscore))"); break;
        case 0x2E: DBGPrintf("(= and +)"); break;
        case 0x2F: DBGPrintf("([ and {)"); break;
        case 0x30: DBGPrintf("(] and })"); break;
        case 0x31: DBGPrintf("(\and |)"); break;
        case 0x32: DBGPrintf("(Non-US # and ˜)"); break;
        case 0x33: DBGPrintf("(; and :)"); break;
        case 0x34: DBGPrintf("(‘ and “)"); break;
        case 0x35: DBGPrintf("(Grave Accent and Tilde)"); break;
        case 0x36: DBGPrintf("(, and <)"); break;
        case 0x37: DBGPrintf("(. and >)"); break;
        case 0x38: DBGPrintf("(/ and ?)"); break;
        case 0x39: DBGPrintf("(Caps Lock)"); break;
        case 0x3A: DBGPrintf("(F1)"); break;
        case 0x3B: DBGPrintf("(F2)"); break;
        case 0x3C: DBGPrintf("(F3)"); break;
        case 0x3D: DBGPrintf("(F4)"); break;
        case 0x3E: DBGPrintf("(F5)"); break;
        case 0x3F: DBGPrintf("(F6)"); break;
        case 0x40: DBGPrintf("(F7)"); break;
        case 0x41: DBGPrintf("(F8)"); break;
        case 0x42: DBGPrintf("(F9)"); break;
        case 0x43: DBGPrintf("(F10)"); break;
        case 0x44: DBGPrintf("(F11)"); break;
        case 0x45: DBGPrintf("(F12)"); break;
        case 0x46: DBGPrintf("(PrintScreen)"); break;
        case 0x47: DBGPrintf("(Scroll Lock)"); break;
        case 0x48: DBGPrintf("(Pause)"); break;
        case 0x49: DBGPrintf("(Insert)"); break;
        case 0x4A: DBGPrintf("(Home)"); break;
        case 0x4B: DBGPrintf("(PageUp)"); break;
        case 0x4C: DBGPrintf("(Delete Forward)"); break;
        case 0x4D: DBGPrintf("(End)"); break;
        case 0x4E: DBGPrintf("(PageDown)"); break;
        case 0x4F: DBGPrintf("(RightArrow)"); break;
        case 0x50: DBGPrintf("(LeftArrow)"); break;
        case 0x51: DBGPrintf("(DownArrow)"); break;
        case 0x52: DBGPrintf("(UpArrow)"); break;
        case 0x53: DBGPrintf("(Keypad Num Lock and Clear)"); break;
        case 0x54: DBGPrintf("(Keypad /)"); break;
        case 0x55: DBGPrintf("(Keypad *)"); break;
        case 0x56: DBGPrintf("(Keypad -)"); break;
        case 0x57: DBGPrintf("(Keypad +)"); break;
        case 0x58: DBGPrintf("(Keypad ENTER)"); break;
        case 0x59: DBGPrintf("(Keypad 1 and End)"); break;
        case 0x5A: DBGPrintf("(Keypad 2 and Down Arrow)"); break;
        case 0x5B: DBGPrintf("(Keypad 3 and PageDn)"); break;
        case 0x5C: DBGPrintf("(Keypad 4 and Left Arrow)"); break;
        case 0x5D: DBGPrintf("(Keypad 5)"); break;
        case 0x5E: DBGPrintf("(Keypad 6 and Right Arrow)"); break;
        case 0x5F: DBGPrintf("(Keypad 7 and Home)"); break;
        case 0x60: DBGPrintf("(Keypad 8 and Up Arrow)"); break;
        case 0x61: DBGPrintf("(Keypad 9 and PageUp)"); break;
        case 0x62: DBGPrintf("(Keypad 0 and Insert)"); break;
        case 0x63: DBGPrintf("(Keypad . and Delete)"); break;
        case 0x64: DBGPrintf("(Non-US \and |)"); break;
        case 0x65: DBGPrintf("(Application)"); break;
        case 0x66: DBGPrintf("(Power)"); break;
        case 0x67: DBGPrintf("(Keypad =)"); break;
        case 0x68: DBGPrintf("(F13)"); break;
        case 0x69: DBGPrintf("(F14)"); break;
        case 0x6A: DBGPrintf("(F15)"); break;
        case 0x6B: DBGPrintf("(F16)"); break;
        case 0x6C: DBGPrintf("(F17)"); break;
        case 0x6D: DBGPrintf("(F18)"); break;
        case 0x6E: DBGPrintf("(F19)"); break;
        case 0x6F: DBGPrintf("(F20)"); break;
        case 0x70: DBGPrintf("(F21)"); break;
        case 0x71: DBGPrintf("(F22)"); break;
        case 0x72: DBGPrintf("(F23)"); break;
        case 0x73: DBGPrintf("(F24)"); break;
        case 0x74: DBGPrintf("(Execute)"); break;
        case 0x75: DBGPrintf("(Help)"); break;
        case 0x76: DBGPrintf("(Menu)"); break;
        case 0x77: DBGPrintf("(Select)"); break;
        case 0x78: DBGPrintf("(Stop)"); break;
        case 0x79: DBGPrintf("(Again)"); break;
        case 0x7A: DBGPrintf("(Undo)"); break;
        case 0x7B: DBGPrintf("(Cut)"); break;
        case 0x7C: DBGPrintf("(Copy)"); break;
        case 0x7D: DBGPrintf("(Paste)"); break;
        case 0x7E: DBGPrintf("(Find)"); break;
        case 0x7F: DBGPrintf("(Mute)"); break;
        case 0x80: DBGPrintf("(Volume Up)"); break;
        case 0x81: DBGPrintf("(Volume Down)"); break;
        case 0x82: DBGPrintf("(Locking Caps Lock)"); break;
        case 0x83: DBGPrintf("(Locking Num Lock)"); break;
        case 0x84: DBGPrintf("(Locking Scroll Lock)"); break;
        case 0x85: DBGPrintf("(Keypad Comma)"); break;
        case 0x99: DBGPrintf("(Alternate Erase)"); break;
        case 0x9A: DBGPrintf("(SysReq/Attention)"); break;
        case 0x9B: DBGPrintf("(Cancel)"); break;
        case 0x9C: DBGPrintf("(Clear)"); break;
        case 0x9D: DBGPrintf("(Prior)"); break;
        case 0x9E: DBGPrintf("(Return)"); break;
        case 0x9F: DBGPrintf("(Separator)"); break;
        case 0xA0: DBGPrintf("(Out)"); break;
        case 0xA1: DBGPrintf("(Oper)"); break;
        case 0xA2: DBGPrintf("(Clear/Again)"); break;
        case 0xA3: DBGPrintf("(CrSel/Props)"); break;
        case 0xA4: DBGPrintf("(ExSel)"); break;
        case 0xA5: DBGPrintf("(AF Reserved)"); break;
        case 0xB0: DBGPrintf("(Keypad 00)"); break;
        case 0xB1: DBGPrintf("(Keypad 000)"); break;
        case 0xB2: DBGPrintf("(Thousands Separator)"); break;
        case 0xB3: DBGPrintf("(Decimal Separator)"); break;
        case 0xB4: DBGPrintf("(Currency Unit)"); break;
        case 0xB5: DBGPrintf("(Currency Sub-unit)"); break;
        case 0xB6: DBGPrintf("(Keypad ()"); break;
        case 0xB7: DBGPrintf("(Keypad ))"); break;
        case 0xB8: DBGPrintf("(Keypad {)"); break;
        case 0xB9: DBGPrintf("(Keypad })"); break;
        case 0xBA: DBGPrintf("(Keypad Tab)"); break;
        case 0xBB: DBGPrintf("(Keypad Backspace)"); break;
        case 0xBC: DBGPrintf("(Keypad A)"); break;
        case 0xBD: DBGPrintf("(Keypad B)"); break;
        case 0xBE: DBGPrintf("(Keypad C)"); break;
        case 0xBF: DBGPrintf("(Keypad D)"); break;
        case 0xC0: DBGPrintf("(Keypad E)"); break;
        case 0xC1: DBGPrintf("(Keypad F)"); break;
        case 0xC2: DBGPrintf("(Keypad XOR)"); break;
        case 0xC3: DBGPrintf("(Keypad ^)"); break;
        case 0xC4: DBGPrintf("(Keypad %)"); break;
        case 0xC5: DBGPrintf("(Keypad <)"); break;
        case 0xC6: DBGPrintf("(Keypad >)"); break;
        case 0xC7: DBGPrintf("(Keypad &)"); break;
        case 0xC8: DBGPrintf("(Keypad &&)"); break;
        case 0xC9: DBGPrintf("(Keypad |)"); break;
        case 0xCA: DBGPrintf("(Keypad ||)"); break;
        case 0xCB: DBGPrintf("(Keypad :)"); break;
        case 0xCC: DBGPrintf("(Keypad #)"); break;
        case 0xCD: DBGPrintf("(Keypad Space)"); break;
        case 0xCE: DBGPrintf("(Keypad @)"); break;
        case 0xCF: DBGPrintf("(Keypad !)"); break;
        case 0xD0: DBGPrintf("(Keypad Memory Store)"); break;
        case 0xD1: DBGPrintf("(Keypad Memory Recall)"); break;
        case 0xD2: DBGPrintf("(Keypad Memory Clear)"); break;
        case 0xD3: DBGPrintf("(Keypad Memory Add)"); break;
        case 0xD4: DBGPrintf("(Keypad Memory Subtract)"); break;
        case 0xD5: DBGPrintf("(Keypad Memory Multiply)"); break;
        case 0xD6: DBGPrintf("(Keypad Memory Divide)"); break;
        case 0xD7: DBGPrintf("(Keypad +/-)"); break;
        case 0xD8: DBGPrintf("(Keypad Clear)"); break;
        case 0xD9: DBGPrintf("(Keypad Clear Entry)"); break;
        case 0xDA: DBGPrintf("(Keypad Binary)"); break;
        case 0xDB: DBGPrintf("(Keypad Octal)"); break;
        case 0xDC: DBGPrintf("(Keypad Decimal)"); break;
        case 0xDD: DBGPrintf("(Keypad Hexadecimal)"); break;
        
        case 0xE0: DBGPrintf("(Left Control)"); break;
        case 0xE1: DBGPrintf("(Left Shift)"); break;
        case 0xE2: DBGPrintf("(Left Alt)"); break;
        case 0xE3: DBGPrintf("(Left GUI)"); break;
        case 0xE4: DBGPrintf("(Right Control)"); break;
        case 0xE5: DBGPrintf("(Right Shift)"); break;
        case 0xE6: DBGPrintf("(Right Alt)"); break;
        case 0xE7: DBGPrintf("(Right GUI)"); break;
        default:
          DBGPrintf("(Keycode %u)", usage);
          break;
      }
      break;
    case 9:  // Button
      DBGPrintf(" (BUTTON %d)", usage);
      break;
    case 0xC:  // Consummer page
      switch (usage) {
        case 0x01: DBGPrintf("(Consumer Controls)"); break;
        case 0x20: DBGPrintf("(+10)"); break;
        case 0x21: DBGPrintf("(+100)"); break;
        case 0x22: DBGPrintf("(AM/PM)"); break;
        case 0x30: DBGPrintf("(Power)"); break;
        case 0x31: DBGPrintf("(Reset)"); break;
        case 0x32: DBGPrintf("(Sleep)"); break;
        case 0x33: DBGPrintf("(Sleep After)"); break;
        case 0x34: DBGPrintf("(Sleep Mode)"); break;
        case 0x35: DBGPrintf("(Illumination)"); break;
        case 0x36: DBGPrintf("(Function Buttons)"); break;
        case 0x40: DBGPrintf("(Menu)"); break;
        case 0x41: DBGPrintf("(Menu  Pick)"); break;
        case 0x42: DBGPrintf("(Menu Up)"); break;
        case 0x43: DBGPrintf("(Menu Down)"); break;
        case 0x44: DBGPrintf("(Menu Left)"); break;
        case 0x45: DBGPrintf("(Menu Right)"); break;
        case 0x46: DBGPrintf("(Menu Escape)"); break;
        case 0x47: DBGPrintf("(Menu Value Increase)"); break;
        case 0x48: DBGPrintf("(Menu Value Decrease)"); break;
        case 0x60: DBGPrintf("(Data On Screen)"); break;
        case 0x61: DBGPrintf("(Closed Caption)"); break;
        case 0x62: DBGPrintf("(Closed Caption Select)"); break;
        case 0x63: DBGPrintf("(VCR/TV)"); break;
        case 0x64: DBGPrintf("(Broadcast Mode)"); break;
        case 0x65: DBGPrintf("(Snapshot)"); break;
        case 0x66: DBGPrintf("(Still)"); break;
        case 0x80: DBGPrintf("(Selection)"); break;
        case 0x81: DBGPrintf("(Assign Selection)"); break;
        case 0x82: DBGPrintf("(Mode Step)"); break;
        case 0x83: DBGPrintf("(Recall Last)"); break;
        case 0x84: DBGPrintf("(Enter Channel)"); break;
        case 0x85: DBGPrintf("(Order Movie)"); break;
        case 0x86: DBGPrintf("(Channel)"); break;
        case 0x87: DBGPrintf("(Media Selection)"); break;
        case 0x88: DBGPrintf("(Media Select Computer)"); break;
        case 0x89: DBGPrintf("(Media Select TV)"); break;
        case 0x8A: DBGPrintf("(Media Select WWW)"); break;
        case 0x8B: DBGPrintf("(Media Select DVD)"); break;
        case 0x8C: DBGPrintf("(Media Select Telephone)"); break;
        case 0x8D: DBGPrintf("(Media Select Program Guide)"); break;
        case 0x8E: DBGPrintf("(Media Select Video Phone)"); break;
        case 0x8F: DBGPrintf("(Media Select Games)"); break;
        case 0x90: DBGPrintf("(Media Select Messages)"); break;
        case 0x91: DBGPrintf("(Media Select CD)"); break;
        case 0x92: DBGPrintf("(Media Select VCR)"); break;
        case 0x93: DBGPrintf("(Media Select Tuner)"); break;
        case 0x94: DBGPrintf("(Quit)"); break;
        case 0x95: DBGPrintf("(Help)"); break;
        case 0x96: DBGPrintf("(Media Select Tape)"); break;
        case 0x97: DBGPrintf("(Media Select Cable)"); break;
        case 0x98: DBGPrintf("(Media Select Satellite)"); break;
        case 0x99: DBGPrintf("(Media Select Security)"); break;
        case 0x9A: DBGPrintf("(Media Select Home)"); break;
        case 0x9B: DBGPrintf("(Media Select Call)"); break;
        case 0x9C: DBGPrintf("(Channel Increment)"); break;
        case 0x9D: DBGPrintf("(Channel Decrement)"); break;
        case 0x9E: DBGPrintf("(Media Select SAP)"); break;
        case 0xA0: DBGPrintf("(VCR Plus)"); break;
        case 0xA1: DBGPrintf("(Once)"); break;
        case 0xA2: DBGPrintf("(Daily)"); break;
        case 0xA3: DBGPrintf("(Weekly)"); break;
        case 0xA4: DBGPrintf("(Monthly)"); break;
        case 0xB0: DBGPrintf("(Play)"); break;
        case 0xB1: DBGPrintf("(Pause)"); break;
        case 0xB2: DBGPrintf("(Record)"); break;
        case 0xB3: DBGPrintf("(Fast Forward)"); break;
        case 0xB4: DBGPrintf("(Rewind)"); break;
        case 0xB5: DBGPrintf("(Scan Next Track)"); break;
        case 0xB6: DBGPrintf("(Scan Previous Track)"); break;
        case 0xB7: DBGPrintf("(Stop)"); break;
        case 0xB8: DBGPrintf("(Eject)"); break;
        case 0xB9: DBGPrintf("(Random Play)"); break;
        case 0xBA: DBGPrintf("(Select DisC)"); break;
        case 0xBB: DBGPrintf("(Enter Disc)"); break;
        case 0xBC: DBGPrintf("(Repeat)"); break;
        case 0xBD: DBGPrintf("(Tracking)"); break;
        case 0xBE: DBGPrintf("(Track Normal)"); break;
        case 0xBF: DBGPrintf("(Slow Tracking)"); break;
        case 0xC0: DBGPrintf("(Frame Forward)"); break;
        case 0xC1: DBGPrintf("(Frame Back)"); break;
        case 0xC2: DBGPrintf("(Mark)"); break;
        case 0xC3: DBGPrintf("(Clear Mark)"); break;
        case 0xC4: DBGPrintf("(Repeat From Mark)"); break;
        case 0xC5: DBGPrintf("(Return To Mark)"); break;
        case 0xC6: DBGPrintf("(Search Mark Forward)"); break;
        case 0xC7: DBGPrintf("(Search Mark Backwards)"); break;
        case 0xC8: DBGPrintf("(Counter Reset)"); break;
        case 0xC9: DBGPrintf("(Show Counter)"); break;
        case 0xCA: DBGPrintf("(Tracking Increment)"); break;
        case 0xCB: DBGPrintf("(Tracking Decrement)"); break;
        case 0xCD: DBGPrintf("(Pause/Continue)"); break;
        case 0xE0: DBGPrintf("(Volume)"); break;
        case 0xE1: DBGPrintf("(Balance)"); break;
        case 0xE2: DBGPrintf("(Mute)"); break;
        case 0xE3: DBGPrintf("(Bass)"); break;
        case 0xE4: DBGPrintf("(Treble)"); break;
        case 0xE5: DBGPrintf("(Bass Boost)"); break;
        case 0xE6: DBGPrintf("(Surround Mode)"); break;
        case 0xE7: DBGPrintf("(Loudness)"); break;
        case 0xE8: DBGPrintf("(MPX)"); break;
        case 0xE9: DBGPrintf("(Volume Up)"); break;
        case 0xEA: DBGPrintf("(Volume Down)"); break;
        case 0xF0: DBGPrintf("(Speed Select)"); break;
        case 0xF1: DBGPrintf("(Playback Speed)"); break;
        case 0xF2: DBGPrintf("(Standard Play)"); break;
        case 0xF3: DBGPrintf("(Long Play)"); break;
        case 0xF4: DBGPrintf("(Extended Play)"); break;
        case 0xF5: DBGPrintf("(Slow)"); break;
        case 0x100: DBGPrintf("(Fan Enable)"); break;
        case 0x101: DBGPrintf("(Fan Speed)"); break;
        case 0x102: DBGPrintf("(Light)"); break;
        case 0x103: DBGPrintf("(Light Illumination Level)"); break;
        case 0x104: DBGPrintf("(Climate Control Enable)"); break;
        case 0x105: DBGPrintf("(Room Temperature)"); break;
        case 0x106: DBGPrintf("(Security Enable)"); break;
        case 0x107: DBGPrintf("(Fire Alarm)"); break;
        case 0x108: DBGPrintf("(Police Alarm)"); break;
        case 0x150: DBGPrintf("(Balance Right)"); break;
        case 0x151: DBGPrintf("(Balance Left)"); break;
        case 0x152: DBGPrintf("(Bass Increment)"); break;
        case 0x153: DBGPrintf("(Bass Decrement)"); break;
        case 0x154: DBGPrintf("(Treble Increment)"); break;
        case 0x155: DBGPrintf("(Treble Decrement)"); break;
        case 0x160: DBGPrintf("(Speaker System)"); break;
        case 0x161: DBGPrintf("(Channel Left)"); break;
        case 0x162: DBGPrintf("(Channel Right)"); break;
        case 0x163: DBGPrintf("(Channel Center)"); break;
        case 0x164: DBGPrintf("(Channel Front)"); break;
        case 0x165: DBGPrintf("(Channel Center Front)"); break;
        case 0x166: DBGPrintf("(Channel Side)"); break;
        case 0x167: DBGPrintf("(Channel Surround)"); break;
        case 0x168: DBGPrintf("(Channel Low Frequency Enhancement)"); break;
        case 0x169: DBGPrintf("(Channel Top)"); break;
        case 0x16A: DBGPrintf("(Channel Unknown)"); break;
        case 0x170: DBGPrintf("(Sub-channel)"); break;
        case 0x171: DBGPrintf("(Sub-channel Increment)"); break;
        case 0x172: DBGPrintf("(Sub-channel Decrement)"); break;
        case 0x173: DBGPrintf("(Alternate Audio Increment)"); break;
        case 0x174: DBGPrintf("(Alternate Audio Decrement)"); break;
        case 0x180: DBGPrintf("(Application Launch Buttons)"); break;
        case 0x181: DBGPrintf("(AL Launch Button Configuration Tool)"); break;
        case 0x182: DBGPrintf("(AL Programmable Button Configuration)"); break;
        case 0x183: DBGPrintf("(AL Consumer Control Configuration)"); break;
        case 0x184: DBGPrintf("(AL Word Processor)"); break;
        case 0x185: DBGPrintf("(AL Text Editor)"); break;
        case 0x186: DBGPrintf("(AL Spreadsheet)"); break;
        case 0x187: DBGPrintf("(AL Graphics Editor)"); break;
        case 0x188: DBGPrintf("(AL Presentation App)"); break;
        case 0x189: DBGPrintf("(AL Database App)"); break;
        case 0x18A: DBGPrintf("(AL Email Reader)"); break;
        case 0x18B: DBGPrintf("(AL Newsreader)"); break;
        case 0x18C: DBGPrintf("(AL Voicemail)"); break;
        case 0x18D: DBGPrintf("(AL Contacts/Address Book)"); break;
        case 0x18E: DBGPrintf("(AL Calendar/Schedule)"); break;
        case 0x18F: DBGPrintf("(AL Task/Project Manager)"); break;
        case 0x190: DBGPrintf("(AL Log/Journal/Timecard)"); break;
        case 0x191: DBGPrintf("(AL Checkbook/Finance)"); break;
        case 0x192: DBGPrintf("(AL Calculator)"); break;
        case 0x193: DBGPrintf("(AL A/V Capture/Playback)"); break;
        case 0x194: DBGPrintf("(AL Local Machine Browser)"); break;
        case 0x195: DBGPrintf("(AL LAN/WAN Browser)"); break;
        case 0x196: DBGPrintf("(AL Internet Browser)"); break;
        case 0x197: DBGPrintf("(AL Remote Networking/ISP Connect)"); break;
        case 0x198: DBGPrintf("(AL Network Conference)"); break;
        case 0x199: DBGPrintf("(AL Network Chat)"); break;
        case 0x19A: DBGPrintf("(AL Telephony/Dialer)"); break;
        case 0x19B: DBGPrintf("(AL Logon)"); break;
        case 0x19C: DBGPrintf("(AL Logoff)"); break;
        case 0x19D: DBGPrintf("(AL Logon/Logoff)"); break;
        case 0x19E: DBGPrintf("(AL Terminal Lock/Screensaver)"); break;
        case 0x19F: DBGPrintf("(AL Control Panel)"); break;
        case 0x1A0: DBGPrintf("(AL Command Line Processor/Run)"); break;
        case 0x1A1: DBGPrintf("(AL Process/Task Manager)"); break;
        case 0x1A2: DBGPrintf("(AL Select Tast/Application)"); break;
        case 0x1A3: DBGPrintf("(AL Next Task/Application)"); break;
        case 0x1A4: DBGPrintf("(AL Previous Task/Application)"); break;
        case 0x1A5: DBGPrintf("(AL Preemptive Halt Task/Application)"); break;
        case 0x200: DBGPrintf("(Generic GUI Application Controls)"); break;
        case 0x201: DBGPrintf("(AC New)"); break;
        case 0x202: DBGPrintf("(AC Open)"); break;
        case 0x203: DBGPrintf("(AC Close)"); break;
        case 0x204: DBGPrintf("(AC Exit)"); break;
        case 0x205: DBGPrintf("(AC Maximize)"); break;
        case 0x206: DBGPrintf("(AC Minimize)"); break;
        case 0x207: DBGPrintf("(AC Save)"); break;
        case 0x208: DBGPrintf("(AC Print)"); break;
        case 0x209: DBGPrintf("(AC Properties)"); break;
        case 0x21A: DBGPrintf("(AC Undo)"); break;
        case 0x21B: DBGPrintf("(AC Copy)"); break;
        case 0x21C: DBGPrintf("(AC Cut)"); break;
        case 0x21D: DBGPrintf("(AC Paste)"); break;
        case 0x21E: DBGPrintf("(AC Select All)"); break;
        case 0x21F: DBGPrintf("(AC Find)"); break;
        case 0x220: DBGPrintf("(AC Find and Replace)"); break;
        case 0x221: DBGPrintf("(AC Search)"); break;
        case 0x222: DBGPrintf("(AC Go To)"); break;
        case 0x223: DBGPrintf("(AC Home)"); break;
        case 0x224: DBGPrintf("(AC Back)"); break;
        case 0x225: DBGPrintf("(AC Forward)"); break;
        case 0x226: DBGPrintf("(AC Stop)"); break;
        case 0x227: DBGPrintf("(AC Refresh)"); break;
        case 0x228: DBGPrintf("(AC Previous Link)"); break;
        case 0x229: DBGPrintf("(AC Next Link)"); break;
        case 0x22A: DBGPrintf("(AC Bookmarks)"); break;
        case 0x22B: DBGPrintf("(AC History)"); break;
        case 0x22C: DBGPrintf("(AC Subscriptions)"); break;
        case 0x22D: DBGPrintf("(AC Zoom In)"); break;
        case 0x22E: DBGPrintf("(AC Zoom Out)"); break;
        case 0x22F: DBGPrintf("(AC Zoom)"); break;
        case 0x230: DBGPrintf("(AC Full Screen View)"); break;
        case 0x231: DBGPrintf("(AC Normal View)"); break;
        case 0x232: DBGPrintf("(AC View Toggle)"); break;
        case 0x233: DBGPrintf("(AC Scroll Up)"); break;
        case 0x234: DBGPrintf("(AC Scroll Down)"); break;
        case 0x235: DBGPrintf("(AC Scroll)"); break;
        case 0x236: DBGPrintf("(AC Pan Left)"); break;
        case 0x237: DBGPrintf("(AC Pan Right)"); break;
        case 0x238: DBGPrintf("(AC Pan)"); break;
        case 0x239: DBGPrintf("(AC New Window)"); break;
        case 0x23A: DBGPrintf("(AC Tile Horizontally)"); break;
        case 0x23B: DBGPrintf("(AC Tile Vertically)"); break;
        case 0x23C: DBGPrintf("(AC Format)"); break;
        default: DBGPrintf("(?)"); break;
      }
      break;
  }
}

#endif


//=============================================================================
// Lets try copy of the HID Parse code and see what happens with with it.
//=============================================================================

// Extract 1 to 32 bits from the data array, starting at bitindex.
static uint32_t bitfield(const uint8_t *data, uint32_t bitindex, uint32_t numbits)
{
    uint32_t output = 0;
    uint32_t bitcount = 0;
    data += (bitindex >> 3);
    uint32_t offset = bitindex & 7;
    if (offset) {
        output = (*data++) >> offset;
        bitcount = 8 - offset;
    }
    while (bitcount < numbits) {
        output |= (uint32_t)(*data++) << bitcount;
        bitcount += 8;
    }
    if (bitcount > numbits && numbits < 32) {
        output &= ((1 << numbits) - 1);
    }
    return output;
}

// convert a number with the specified number of bits from unsigned to signed,
// so the result is a proper 32 bit signed integer.
static int32_t signext(uint32_t num, uint32_t bitcount)
{
    if (bitcount < 32 && bitcount > 0 && (num & (1 << (bitcount - 1)))) {
        num |= ~((1 << bitcount) - 1);
    }
    return (int32_t)num;
}

// convert a tag's value to a signed integer.
static int32_t signedval(uint32_t num, uint8_t tag)
{
    tag &= 3;
    if (tag == 1) return (int8_t)num;
    if (tag == 2) return (int16_t)num;
    return (int32_t)num;
}

// This no-inputs parse is meant to be used when we first get the
// HID report descriptor.  It finds all the top level collections
// and allows drivers to claim them.  This is always where we
// learn whether the reports will or will not use a Report ID byte.
void BluetoothConnection::parse()
{
    DBGPrintf("BluetoothConnection::parse() called\n");
    const uint8_t *p = descriptor_;
    const uint8_t *end = p + descsize_;
    uint16_t usage_page = 0;
    uint16_t usage = 0;
    uint8_t collection_level = 0;
    uint8_t topusage_count = 0;

    //bool use_report_id = false;
    while (p < end) {
        uint8_t tag = *p;
        if (tag == 0xFE) { // Long Item
            p += *p + 3;
            continue;
        }
        uint32_t val = 0;
        switch (tag & 0x03) { // Short Item data
        case 0: val = 0;
            p++;
            break;
        case 1: val = p[1];
            p += 2;
            break;
        case 2: val = p[1] | (p[2] << 8);
            p += 3;
            break;
        case 3: val = p[1] | (p[2] << 8) | (p[3] << 16) | (p[4] << 24);
            p += 5;
            break;
        }
        if (p > end) break;

        switch (tag & 0xFC) {
        case 0x84: // Report ID (global)
            //use_report_id = true;
            break;
        case 0x04: // Usage Page (global)
            usage_page = val;
            break;
        case 0x08: // Usage (local)
            usage = val;
            break;
        case 0xA0: // Collection
            if (collection_level == 0 && topusage_count < TOPUSAGE_LIST_LEN) {
                uint32_t topusage = ((uint32_t)usage_page << 16) | usage;
                println("Found top level collection ", topusage, HEX);
                DBGPrintf("\ttopusage:%x\n", topusage);

                //topusage_list[topusage_count] = topusage;
                topusage_drivers[topusage_count] = find_driver(topusage);
                topusage_count++;
            }
            collection_level++;
            usage = 0;
            break;
        case 0xC0: // End Collection
            if (collection_level > 0) {
                collection_level--;
            }
        case 0x80: // Input
        case 0x90: // Output
        case 0xB0: // Feature
            usage = 0;
            break;
        }
    }
    while (topusage_count < TOPUSAGE_LIST_LEN) {
        //topusage_list[topusage_count] = 0;
        topusage_drivers[topusage_count] = NULL;
        topusage_count++;
    }
}

BTHIDInput * BluetoothConnection::find_driver(uint32_t topusage)
{
    println("find_driver");
    BTHIDInput *driver = BluetoothController::available_bthid_drivers_list;
    hidclaim_t claim_type;
    while (driver) {
        //println("  driver ", (uint32_t)driver, HEX);
        if ((claim_type = driver->bt_claim_collection(this, device_class_, topusage)) != CLAIM_NO) {
            //if (claim_type == CLAIM_INTERFACE) hid_driver_claimed_control_ = true;
            return driver;
        }
        driver = driver->next;
    }
    println("No Driver claimed topusage: ", topusage, HEX);
    return NULL;
}

void BluetoothConnection::parse(uint16_t type_and_report_id, const uint8_t *data, uint32_t len)
{
    const bool use_report_id = true;
    const uint8_t *p = descriptor_;
    const uint8_t *end = p + descsize_;
    BTHIDInput *driver = NULL;
    // USBHIDInput *driver = hidi_; // hack for now everything feeds back to us...
    uint32_t topusage = 0;
    uint8_t topusage_index = 0;
    uint8_t collection_level = 0;
    uint16_t usage[USAGE_LIST_LEN] = {0, 0};
    uint8_t usage_count = 0;
    uint8_t usage_min_max_count = 0;
    uint8_t usage_min_max_mask = 0;
    uint8_t report_id = 0;
    uint16_t report_size = 0;
    uint16_t report_count = 0;
    uint16_t usage_page = 0;
    uint32_t last_usage = 0;
    int32_t logical_min = 0;
    int32_t logical_max = 0;
    uint32_t bitindex = 0;

    while (p < end) {
        uint8_t tag = *p;
        if (tag == 0xFE) { // Long Item (unsupported)
            p += p[1] + 3;
            continue;
        }
        uint32_t val = 0;
        switch (tag & 0x03) { // Short Item data
        case 0: val = 0;
            p++;
            break;
        case 1: val = p[1];
            p += 2;
            break;
        case 2: val = p[1] | (p[2] << 8);
            p += 3;
            break;
        case 3: val = p[1] | (p[2] << 8) | (p[3] << 16) | (p[4] << 24);
            p += 5;
            break;
        }
        if (p > end) break;
        bool reset_local = false;
        switch (tag & 0xFC) {
        case 0x04: // Usage Page (global)
            usage_page = val;
            break;
        case 0x14: // Logical Minimum (global)
            logical_min = signedval(val, tag);
            break;
        case 0x24: // Logical Maximum (global)
            logical_max = signedval(val, tag);
            break;
        case 0x74: // Report Size (global)
            report_size = val;
            break;
        case 0x94: // Report Count (global)
            report_count = val;
            break;
        case 0x84: // Report ID (global)
            report_id = val;
            break;
        case 0x08: // Usage (local)
            if (usage_count < USAGE_LIST_LEN) {
                // Usages: 0 is reserved 0x1-0x1f is sort of reserved for top level things like
                // 0x1 - Pointer - A collection... So lets try ignoring these
                if (val > 0x1f) {
                    usage[usage_count++] = val;
                }
            }
            break;
        case 0x18: // Usage Minimum (local)
            // Note: Found a report with multiple min/max
            if (usage_count != 255) {
                usage_count = 255;
                usage_min_max_count = 0;
                usage_min_max_mask = 0;
            }
            usage[usage_min_max_count * 2] = val;
            usage_min_max_mask |= 1;
            if (usage_min_max_mask == 3) {
                usage_min_max_count++;
                usage_min_max_mask = 0;
            }
            break;
        case 0x28: // Usage Maximum (local)
            if (usage_count != 255) {
                usage_count = 255;
                usage_min_max_count = 0;
                usage_min_max_mask = 0;
            }
            usage[usage_min_max_count * 2 + 1] = val;
            usage_min_max_mask |= 2;
            if (usage_min_max_mask == 3) {
                usage_min_max_count++;
                usage_min_max_mask = 0;
            }
            break;
        case 0xA0: // Collection
            if (collection_level == 0) {
                topusage = ((uint32_t)usage_page << 16) | usage[0];
                driver = NULL;
                if (topusage_index < TOPUSAGE_LIST_LEN) {
                    driver = topusage_drivers[topusage_index++];
                }
            }
            // discard collection info if not top level, hopefully that's ok?
            collection_level++;
            reset_local = true;
            break;
        case 0xC0: // End Collection
            if (collection_level > 0) {
                collection_level--;
                if (collection_level == 0 && driver != NULL) {
                    driver->bt_hid_input_end();
                    //driver = NULL;
                }
            }
            reset_local = true;
            break;
        case 0x80: // Input
            if (use_report_id && (report_id != (type_and_report_id & 0xFF))) {
                // completely ignore and do not advance bitindex
                // for descriptors of other report IDs
                reset_local = true;
                break;
            }
            if ((val & 1) || (driver == NULL)) {
                // skip past constant fields or when no driver is listening
                bitindex += report_count * report_size;
            } else {
                println("begin, usage=", topusage, HEX);
                println("       type= ", val, HEX);
                println("       min=  ", logical_min);
                println("       max=  ", logical_max);
                println("       reportcount=", report_count);
                println("       usage count=", usage_count);
                println("       usage min max count=", usage_min_max_count);

                driver->bt_hid_input_begin(topusage, val, logical_min, logical_max);
                println("Input, total bits=", report_count * report_size);
                if ((val & 2)) {
                    // ordinary variable format
                    uint32_t uindex = 0;
                    uint32_t uindex_max = 0xffff; // assume no MAX
                    bool uminmax = false;
                    uint8_t uminmax_index = 0;
                    if (usage_count > USAGE_LIST_LEN) {
                        // usage numbers by min/max, not from list
                        uindex = usage[0];
                        uindex_max = usage[1];
                        uminmax = true;
                    } else if ((report_count > 1) && (usage_count <= 1)) {
                        // Special cases:  Either only one or no usages specified and there are more than one
                        // report counts .
                        if (usage_count == 1) {
                            uindex = usage[0];
                        } else {
                            // BUGBUG:: Not sure good place to start?  maybe round up from last usage to next higher group up of 0x100?
                            uindex = (last_usage & 0xff00) + 0x100;
                        }
                        uminmax = true;
                    }
                    //USBHDBGDBGSerial.printf("TU:%x US:%x %x %d %d: C:%d, %d, MM:%d, %x %x\n", topusage, usage_page, val, logical_min, logical_max,
                    //      report_count, usage_count, uminmax, usage[0], usage[1]);
                    for (uint32_t i = 0; i < report_count; i++) {
                        uint32_t u;
                        if (uminmax) {
                            u = uindex;
                            if (uindex < uindex_max) uindex++;
                            else if (uminmax_index < usage_min_max_count) {
                                uminmax_index++;
                                uindex = usage[uminmax_index * 2];
                                uindex_max = usage[uminmax_index * 2 + 1];
                                //USBHDBGPDBGSerial.printf("$$ next min/max pair: %u %u %u\n", uminmax_index, uindex, uindex_max);
                            }
                        } else {
                            u = usage[uindex++];
                            if (uindex >= USAGE_LIST_LEN - 1) {
                                uindex = USAGE_LIST_LEN - 1;
                            }
                        }
                        last_usage = u; // remember the last one we used...
                        u |= (uint32_t)usage_page << 16;
                        print("  usage = ", u, HEX);

                        uint32_t n = bitfield(data, bitindex, report_size);
                        if (logical_min >= 0) {
                            println("  data = ", n);
                            driver->bt_hid_input_data(u, n);
                        } else {
                            int32_t sn = signext(n, report_size);
                            println("  sdata = ", sn);
                            driver->bt_hid_input_data(u, sn);
                        }
                        bitindex += report_size;
                    }
                } else {
                    // array format, each item is a usage number
                    // maybe act like the 2 case...
                    if (usage_min_max_count && (report_size == 1)) {
                        uint32_t uindex = usage[0];
                        uint32_t uindex_max = usage[1];
                        uint8_t uminmax_index = 0;
                        uint32_t u;

                        for (uint32_t i = 0; i < report_count; i++) {
                            u = uindex;
                            if (uindex < uindex_max) uindex++;
                            else if (uminmax_index < usage_min_max_count) {
                                uminmax_index++;
                                uindex = usage[uminmax_index * 2];
                                uindex_max = usage[uminmax_index * 2 + 1];
                                //USBHDBGPDBGSerial.printf("$$ next min/max pair: %u %u %u\n", uminmax_index, uindex, uindex_max);
                            }

                            u |= (uint32_t)usage_page << 16;
                            uint32_t n = bitfield(data, bitindex, report_size);
                            if (logical_min >= 0) {
                                println("  data = ", n);
                                driver->bt_hid_input_data(u, n);
                            } else {
                                int32_t sn = signext(n, report_size);
                                println("  sdata = ", sn);
                                driver->bt_hid_input_data(u, sn);
                            }

                            bitindex += report_size;
                        }

                    } else {
                        for (uint32_t i = 0; i < report_count; i++) {
                            uint32_t u = bitfield(data, bitindex, report_size);
                            int n = u;
                            if (n >= logical_min && n <= logical_max) {
                                u |= (uint32_t)usage_page << 16;
                                print("  usage = ", u, HEX);
                                println("  data = 1");
                                driver->bt_hid_input_data(u, 1);
                            } else {
                                print ("  usage =", u, HEX);
                                print(" out of range: ", logical_min, HEX);
                                println(" ", logical_max, HEX);
                            }
                            bitindex += report_size;
                        }
                    }
                }
            }
            reset_local = true;
            break;
        case 0x90: // Output
            // TODO.....
            reset_local = true;
            break;
        case 0xB0: // Feature
            // TODO.....
            reset_local = true;
            break;

        case 0x34: // Physical Minimum (global)
        case 0x44: // Physical Maximum (global)
        case 0x54: // Unit Exponent (global)
        case 0x64: // Unit (global)
            break; // Ignore these commonly used tags.  Hopefully not needed?

        case 0xA4: // Push (yikes! Hope nobody really uses this?!)
        case 0xB4: // Pop (yikes! Hope nobody really uses this?!)
        case 0x38: // Designator Index (local)
        case 0x48: // Designator Minimum (local)
        case 0x58: // Designator Maximum (local)
        case 0x78: // String Index (local)
        case 0x88: // String Minimum (local)
        case 0x98: // String Maximum (local)
        case 0xA8: // Delimiter (local)
        default:
            println("Ruh Roh, unsupported tag, not a good thing Scoob ", tag, HEX);
            break;
        }
        if (reset_local) {
            usage_count = 0;
            usage_min_max_count = 0;
            usage[0] = 0;
            usage[1] = 0;
        }
    }
}
