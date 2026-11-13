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
// Bluetooth Main controller code
//=============================================================================


#include <Arduino.h>
#include "USBHost_t36.h"  // Read this header first for key info
#include "utility/bt_defines.h"
#include <EEPROM.h>

#define print   USBHost::print_
#define println USBHost::println_

//#define DEBUG_BT
//#define DEBUG_BT_VERBOSE

#ifndef DEBUG_BT
#undef DEBUG_BT_VERBOSE
void inline DBGPrintf(...) {};
void inline DBGFlush() {};
#else
#define DBGPrintf USBHDBGSerial.printf
#define DBGFlush  USBHDBGSerial.flush
#endif
elapsedMillis em_rx_tx2 = 0;
elapsedMillis em_rx_tx = 0;

#ifndef DEBUG_BT_VERBOSE
void inline VDBGPrintf(...) {};
#else
#define VDBGPrintf USBHDBGSerial.printf
#endif

// Lets use a boolean to determine if we have SSP available_bthid_drivers_list

bool has_key = false;

// This is a list of all the drivers inherited from the BTHIDInput class.
// Unlike the list of USBDriver (managed in enumeration.cpp), drivers stay
// on this list even when they have claimed a top level collection.
BTHIDInput * BluetoothController::available_bthid_drivers_list = NULL;

// default forward.
hidclaim_t BTHIDInput::claim_bluetooth(BluetoothConnection *btconnection, uint32_t bluetooth_class, uint8_t *remoteName, int type)
{
    return claim_bluetooth(btconnection->btController_, bluetooth_class, remoteName) ? CLAIM_INTERFACE : CLAIM_NO;
}

const uint8_t *BTHIDInput::manufacturer()
{  
    return nullptr; // so far don't have one
}

const uint8_t *BTHIDInput::product()
{  
    if (btconnect == nullptr) return nullptr;
    return btconnect->remote_name_;
}

const uint8_t *BTHIDInput::serialNumber()
{
    return nullptr;
}




void BluetoothController::driver_ready_for_bluetooth(BTHIDInput *driver)
{
    driver->next = NULL;
    if (available_bthid_drivers_list == NULL) {
        available_bthid_drivers_list = driver;
    } else {
        BTHIDInput *last = available_bthid_drivers_list;
        while (last->next) last = last->next;
        last->next = driver;
    }
}

bool BluetoothController::queue_Data_Transfer_Debug(Pipe_t *pipe, void *buffer, 
    uint32_t len, USBDriver *driver, uint32_t line) 
{
    if ((pipe == nullptr) || (driver == nullptr) || ((len > 0) && (buffer == nullptr))) {
        // something wrong:
        USBHDBGSerial.printf("\n !!!!!!!!!!! BluetoothController::queue_Data_Transfer called with bad data line: %u\n", line);
        USBHDBGSerial.printf("\t pipe:%p buffer:%p len:%u driver:%p\n", pipe, buffer, len, driver);
        return false;
    }
    return queue_Data_Transfer(pipe, buffer, len, driver);
}



//12 01 00 02 FF 01 01 40 5C 0A E8 21 12 01 01 02 03 01
//VendorID = 0A5C, ProductID = 21E8, Version = 0112
//Class/Subclass/Protocol = 255 / 1 / 1
BluetoothController::product_vendor_mapping_t BluetoothController::pid_vid_mapping[] = {
    { 0xA5C, 0x21E8 }
};

/************************************************************/
//  Initialization and claiming of devices & interfaces
/************************************************************/

void BluetoothController::init()
{
    contribute_Pipes(mypipes, sizeof(mypipes) / sizeof(Pipe_t));
    contribute_Transfers(mytransfers, sizeof(mytransfers) / sizeof(Transfer_t));
    contribute_String_Buffers(mystring_bufs, sizeof(mystring_bufs) / sizeof(strbuf_t));
    driver_ready_for_device(this);
}

bool BluetoothController::claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len)
{
    // only claim at device level
    println("BluetoothController claim this=", (uint32_t)this, HEX);

    if (type != 0) return false; // claim at the device level

    // Lets try to support the main USB Bluetooth class...
    // http://www.usb.org/developers/defined_class/#BaseClassE0h
    if (dev->bDeviceClass != 0xe0)  {
        bool special_case_device = false;
        for (uint8_t i = 0; i < (sizeof(pid_vid_mapping) / sizeof(pid_vid_mapping[0])); i++) {
            if ((pid_vid_mapping[i].idVendor == dev->idVendor) && (pid_vid_mapping[i].idProduct == dev->idProduct)) {
                special_case_device = true;
                break;
            }
        }
        if (!special_case_device) return false;
    }
    if ((dev->bDeviceSubClass != 1) || (dev->bDeviceProtocol != 1)) return false; // Bluetooth Programming Interface

    DBGPrintf("BluetoothController claim this=%x vid:pid=%x:%x\n    ", (uint32_t)this, dev->idVendor,  dev->idProduct);
    if (len > 512) {
        DBGPrintf("  Descriptor length %d only showing first 512\n    ");
        len = 512;
    }
    for (uint16_t i = 0; i < len; i++) {
        DBGPrintf("%02x ", descriptors[i]);
        if ((i & 0x3f) == 0x3f) DBGPrintf("\n    ");
    }
    DBGPrintf("\n  ");

    // Lets try to process the first Interface and get the end points...
    // Some common stuff for both XBoxs
    uint32_t count_end_points = descriptors[4];
    if (count_end_points < 2) return false;
    uint32_t rxep = 0;
    uint32_t rx2ep = 0;
    uint32_t txep = 0;
    uint8_t rx_interval = 0;
    uint8_t rx2_interval = 0;
    uint8_t tx_interval = 0;
    rx_size_ = 0;
    rx2_size_ = 0;
    tx_size_ = 0;
    uint32_t descriptor_index = 9;
    while (count_end_points-- /*&& ((rxep == 0) || txep == 0) */) {
        if (descriptors[descriptor_index] != 7) return false; // length 7
        if (descriptors[descriptor_index + 1] != 5) return false; // ep desc
        if ((descriptors[descriptor_index + 4] <= 64)
                && (descriptors[descriptor_index + 5] == 0)) {
            // have a bulk EP size
            if (descriptors[descriptor_index + 2] & 0x80 ) {
                if (descriptors[descriptor_index + 3] == 3)     { // Interrupt
                    rxep = descriptors[descriptor_index + 2];
                    rx_size_ = descriptors[descriptor_index + 4];
                    rx_interval = descriptors[descriptor_index + 6];
                } else if  (descriptors[descriptor_index + 3] == 2)     { // bulk
                    rx2ep = descriptors[descriptor_index + 2];
                    rx2_size_ = descriptors[descriptor_index + 4];
                    rx2_interval = descriptors[descriptor_index + 6];
                }
            } else {
                txep = descriptors[descriptor_index + 2];
                tx_size_ = descriptors[descriptor_index + 4];
                tx_interval = descriptors[descriptor_index + 6];
            }
        }
        descriptor_index += 7;  // setup to look at next one...
    }
    if ((rxep == 0) || (txep == 0)) {
        USBHDBGSerial.printf("Bluetooth end points not found: %d %d\n", rxep, txep);
        return false; // did not find two end points.
    }
    DBGPrintf("    rxep=%d(%d) txep=%d(%d) rx2ep=%d(%d)\n", rxep & 15, rx_size_, txep, tx_size_,
              rx2ep & 15, rx2_size_);

    print("BluetoothController, rxep=", rxep & 15);
    print("(", rx_size_);
    print("), txep=", txep);
    print("(", tx_size_);
    println(")");
    rxpipe_ = new_Pipe(dev, 3, rxep & 15, 1, rx_size_, rx_interval);
    if (!rxpipe_) return false;
    txpipe_ = new_Pipe(dev, 3, txep, 0, tx_size_, tx_interval);
    if (!txpipe_) {
        //free_Pipe(rxpipe_);
        return false;
    }
    rx2pipe_ = new_Pipe(dev, 2, rx2ep & 15, 1, rx2_size_, rx2_interval);
    if (!rx2pipe_)  {
        // Free other pipes...
        return false;
    }

    rxpipe_->callback_function = rx_callback;
    queue_Data_Transfer_Debug(rxpipe_, rxbuf_, rx_size_, this, __LINE__);

    rx2pipe_->callback_function = rx2_callback;
    queue_Data_Transfer_Debug(rx2pipe_, rx2buf_, rx2_size_, this, __LINE__);

    txpipe_->callback_function = tx_callback;

    // Send out the reset
    device = dev; // yes this is normally done on return from this but should not hurt if we do it here.
    sendResetHCI();
    pending_control_ = PC_RESET;
    //pending_control_tx_ = 0;    //

    return true;
}


void BluetoothController::disconnect()
{
    USBHDBGSerial.printf("Bluetooth Disconnect");
    // lets clear out any active connecitons
    current_connection_ = BluetoothConnection::s_first_;
    while (current_connection_) {
        // see if this one is in use         
        if (current_connection_->btController_ == this) {
            if (current_connection_->device_driver_) {
                current_connection_->device_driver_->release_bluetooth();
                current_connection_->remote_name_[0] = 0;
                current_connection_->device_driver_ = nullptr;
            }
            current_connection_->btController_ = nullptr;
        }
        current_connection_ = current_connection_->next_;
    }
    // Maybe leave it pointing to first one just in case.
    count_connections_ = 0;
    timer_connection_ = nullptr;
    current_connection_ = BluetoothConnection::s_first_;
    USBHDBGSerial.printf("Bluetooth Disconnect complete"); USBHDBGSerial.flush();
}

void BluetoothController::timer_event(USBDriverTimer *whichTimer)
{
    DBGPrintf("BT::Timer_event(%p)->%p\n", whichTimer, whichTimer->pointer);DBGFlush();
    if (whichTimer == &timer_) {
        if (timer_connection_) timer_connection_->timer_event(whichTimer);
    } else if (whichTimer->pointer) {
        BluetoothConnection* btc = (BluetoothConnection*)(whichTimer->pointer);
        btc->timer_event(whichTimer);
    }

}


void BluetoothController::control(const Transfer_t *transfer)
{
    println("    control callback (bluetooth) ", pending_control_, HEX);
#ifdef DEBUG_BT_VERBOSE
    DBGPrintf("    Control callback (bluetooth): %d : ", pending_control_);
    uint8_t *buffer = (uint8_t*)transfer->buffer;
    for (uint8_t i = 0; i < transfer->length; i++) DBGPrintf("%02x ", buffer[i]);
    DBGPrintf("\n");
#endif
}

bool BluetoothController::setTimer(BluetoothConnection *connection, uint32_t us)  // set to NULL ptr will clear:
{
    static uint32_t millis_last = 0;
    DBGPrintf("BluetoothController::setTimer(%p, %u) TO:%u, dt:%u\n", connection, us,
        millis(), millis()-millis_last);
    millis_last = millis();
    if (connection == nullptr) {
        timer_connection_ = nullptr;
        timer_.stop();
        return true;
    } else if ((timer_connection_ == nullptr) || (connection == timer_connection_)) {
        timer_connection_ = connection;
        timer_.start(us);
        return true;
    }
    return false;
}


/************************************************************/
//  Try starting a pairing operation after sketch starts
/************************************************************/
bool BluetoothController::startDevicePairing(const char *pin, bool pair_ssp)
{
    // What should we verify before starting this mode?
    if (pending_control_ != 0) {
        DBGPrintf("Pending control not zero.");
        return false;
    }

    // BUGBUG:: probably should make copy of pin...
    pair_pincode_ = pin;
    do_pair_ssp_ = pair_ssp;

    // Try simple approach first to see if I can simply start it
    do_pair_device_ = !do_pair_ssp_;
    pending_control_ = PC_SEND_WRITE_INQUIRE_MODE;
    queue_next_hci_command();

    return true;
}

#ifdef DEBUG_BT
void print_error_codes(uint8_t error_code) {
    switch (error_code) {
        case 0x01: DBGPrintf(" ( Unknown HCI Command)\n"); break;
        case 0x02: DBGPrintf(" ( Unknown Connection Identifier)\n"); break;
        case 0x03: DBGPrintf(" ( Hardware Failure)\n"); break;
        case 0x04: DBGPrintf(" ( Page Timeout)\n"); break;
        case 0x05: DBGPrintf(" ( Authentication Failure)\n"); break;
        case 0x06: DBGPrintf(" ( PIN or Key Missing)\n"); break;
        case 0x07: DBGPrintf(" ( Memory Capacity Exceeded)\n"); break;
        case 0x08: DBGPrintf(" ( Connection Timeout)\n"); break;
        case 0x09: DBGPrintf(" ( Connection Limit Exceeded)\n"); break;
        case 0x0A: DBGPrintf(" ( Synchronous Connection Limit To A Device Exceeded)\n"); break;
        case 0x0B: DBGPrintf(" ( Connection Already Exists)\n"); break;
        case 0x0C: DBGPrintf(" ( Command Disallowed)\n"); break;
        case 0x0D: DBGPrintf(" ( Connection Rejected due to Limited Resources)\n"); break;
        case 0x0E: DBGPrintf(" ( Connection Rejected Due To Security Reasons)\n"); break;
        case 0x0F: DBGPrintf(" ( Connection Rejected due to Unacceptable BD_ADDR)\n"); break;
        case 0x10: DBGPrintf(" ( Connection Accept Timeout Exceeded)\n"); break;
        case 0x11: DBGPrintf(" ( Unsupported Feature or Parameter Value)\n"); break;
        case 0x12: DBGPrintf(" ( Invalid HCI Command Parameters)\n"); break;
        case 0x13: DBGPrintf(" ( Remote User Terminated Connection)\n"); break;
        case 0x14: DBGPrintf(" ( Remote Device Terminated Connection due to Low Resources)\n"); break;
        case 0x15: DBGPrintf(" ( Remote Device Terminated Connection due to Power Off)\n"); break;
        case 0x16: DBGPrintf(" ( Connection Terminated By Local Host)\n"); break;
        case 0x17: DBGPrintf(" ( Repeated Attempts)\n"); break;
        case 0x18: DBGPrintf(" ( Pairing Not Allowed)\n"); break;
        case 0x19: DBGPrintf(" ( Unknown LMP PDU)\n"); break;
        case 0x1A: DBGPrintf(" ( Unsupported Remote Feature)\n"); break;
        case 0x1B: DBGPrintf(" ( SCO Offset Rejected)\n"); break;
        case 0x1C: DBGPrintf(" ( SCO Interval Rejected)\n"); break;
        case 0x1D: DBGPrintf(" ( SCO Air Mode Rejected)\n"); break;
        case 0x1E: DBGPrintf(" ( Invalid LMP Parameters / Invalid LL Parameters)\n"); break;
        case 0x1F: DBGPrintf(" ( Unspecified Error)\n"); break;
        case 0x20: DBGPrintf(" ( Unsupported LMP Parameter Value / Unsupported LL Parameter Value)\n"); break;
        case 0x21: DBGPrintf(" ( Role Change Not Allowed)\n"); break;
        case 0x22: DBGPrintf(" ( LMP Response Timeout / LL Response Timeout)\n"); break;
        case 0x23: DBGPrintf(" ( LMP Error Transaction Collision / LL Procedure Collision)\n"); break;
        case 0x24: DBGPrintf(" ( LMP PDU Not Allowed)\n"); break;
        case 0x25: DBGPrintf(" ( Encryption Mode Not Acceptable)\n"); break;
        case 0x26: DBGPrintf(" ( Link Key cannot be Changed)\n"); break;
        case 0x27: DBGPrintf(" ( Requested QoS Not Supported)\n"); break;
        case 0x28: DBGPrintf(" ( Instant Passed)\n"); break;
        case 0x29: DBGPrintf(" ( Pairing With Unit Key Not Supported)\n"); break;
        case 0x2A: DBGPrintf(" ( Different Transaction Collision)\n"); break;
        case 0x2B: DBGPrintf(" ( Reserved for future use)\n"); break;
        case 0x2C: DBGPrintf(" ( QoS Unacceptable Parameter)\n"); break;
        case 0x2D: DBGPrintf(" ( QoS Rejected)\n"); break;
        case 0x2E: DBGPrintf(" ( Channel Classification Not Supported)\n"); break;
        case 0x2F: DBGPrintf(" ( Insufficient Security)\n"); break;
        case 0x30: DBGPrintf(" ( Parameter Out Of Mandatory Range)\n"); break;
        case 0x31: DBGPrintf(" ( Reserved for future use)\n"); break;
        case 0x32: DBGPrintf(" ( Role Switch Pending)\n"); break;
        case 0x33: DBGPrintf(" ( Reserved for future use)\n"); break;
        case 0x34: DBGPrintf(" ( Reserved Slot Violation)\n"); break;
        case 0x35: DBGPrintf(" ( Role Switch Failed)\n"); break;
        case 0x36: DBGPrintf(" ( Extended Inquiry Response Too Large)\n"); break;
        case 0x37: DBGPrintf(" ( Secure Simple Pairing Not Supported By Host)\n"); break;
        case 0x38: DBGPrintf(" ( Host Busy - Pairing)\n"); break;
        case 0x39: DBGPrintf(" ( Connection Rejected due to No Suitable Channel Found)\n"); break;
        case 0x3A: DBGPrintf(" ( Controller Busy)\n"); break;
        case 0x3B: DBGPrintf(" ( Unacceptable Connection Parameters)\n"); break;
        case 0x3C: DBGPrintf(" ( Advertising Timeout)\n"); break;
        case 0x3D: DBGPrintf(" ( Connection Terminated due to MIC Failure)\n"); break;
        case 0x3E: DBGPrintf(" ( Connection Failed to be Established / Synchronization Timeout)\n"); break;
        case 0x3F: DBGPrintf(" ( Previously used)\n"); break;
        case 0x40: DBGPrintf(" ( Coarse Clock Adjustment Rejected but Will Try to Adjust Using Clock Dragging)\n"); break;
        case 0x41: DBGPrintf(" ( Type0 Submap Not Defined)\n"); break;
        case 0x42: DBGPrintf(" ( Unknown Advertising Identifier)\n"); break;
        case 0x43: DBGPrintf(" ( Limit Reached)\n"); break;
        case 0x44: DBGPrintf(" ( Operation Cancelled by Host)\n"); break;
        case 0x45: DBGPrintf(" ( Packet Too Long)\n"); break;
        default: DBGPrintf("\n");
    }    
}
#endif

/************************************************************/
//  Interrupt-based Data Movement
/************************************************************/

void BluetoothController::rx_callback(const Transfer_t *transfer)
{
    if (!transfer->driver) return;
    ((BluetoothController *)(transfer->driver))->rx_data(transfer);
}

void BluetoothController::rx2_callback(const Transfer_t *transfer)
{
    uint32_t len = transfer->length - ((transfer->qtd.token >> 16) & 0x7FFF);
    print_hexbytes((uint8_t*)transfer->buffer, len);
//  DBGPrintf("<<(00 : %d): ", len);
//    DBGPrintf("<<(02 %u %p %u):", (uint32_t)em_rx_tx2, transfer->driver, len);
//    em_rx_tx2 = 0;
//    uint8_t *buffer = (uint8_t*)transfer->buffer;
//    for (uint8_t i = 0; i < len; i++) DBGPrintf("%02X ", buffer[i]);
//    DBGPrintf("\n");

    if (!transfer->driver) return;
    ((BluetoothController *)(transfer->driver))->rx2_data(transfer);
}

void BluetoothController::tx_callback(const Transfer_t *transfer)
{
    if (!transfer->driver) return;
    ((BluetoothController *)(transfer->driver))->tx_data(transfer);
}


void BluetoothController::rx_data(const Transfer_t *transfer)
{
    uint32_t len = transfer->length - ((transfer->qtd.token >> 16) & 0x7FFF);
    print_hexbytes((uint8_t*)transfer->buffer, len);
//  DBGPrintf("<<(00 : %d): ", len);
    DBGPrintf(rx_packet_data_remaining_? "<<C(01, %u):":"<<(01, %u):", (uint32_t)em_rx_tx);
    em_rx_tx = 0;
    uint8_t *buffer = (uint8_t*)transfer->buffer;
    for (uint8_t i = 0; i < len; i++) DBGPrintf("%02X ", buffer[i]);
    DBGPrintf("\n");

    // Note the logical packets returned from the device may be larger
    // than can fit in one of our packets, so we will detect this and
    // the next read will be continue in or rx_buf_ in the next logical
    // location.  We will only go into process the next logical state
    // when we have the full response read in...
    if (rx_packet_data_remaining_ == 0) {   // Previous command was fully handled
        if (len == 0) {
            DBGPrintf("<< Empty Packet >>\n");
            // probably could combine with below.
            queue_Data_Transfer_Debug(rxpipe_, rxbuf_, rx_size_, this, __LINE__);
            return;
        }
        rx_packet_data_remaining_ = rxbuf_[1] + 2;  // length of data plus the two bytes at start...
    }
    // Now see if the data
    rx_packet_data_remaining_ -= len;   // remove the length of this packet from length

    if (rx_packet_data_remaining_ == 0) {   // read started at beginning of packet so get the total length of packet
        switch (rxbuf_[0]) { // Switch on event type
        case EV_COMMAND_COMPLETE: //0x0e
            handle_hci_command_complete();// Check if command succeeded
            break;
        case EV_COMMAND_STATUS: //0x0f
            handle_hci_command_status();
            break;
        case EV_INQUIRY_COMPLETE: // 0x01
            handle_hci_inquiry_complete();
            break;
        case EV_INQUIRY_RESULT:   // 0x02
            handle_hci_inquiry_result(false);
            break;
        case EV_CONNECT_COMPLETE:   // 0x03
            handle_hci_connection_complete();
            break;
        case EV_INCOMING_CONNECT:   // 0x04
            handle_hci_incoming_connect();
            break;
        case EV_DISCONNECT_COMPLETE: // 0x05
            handle_hci_disconnect_complete();
            break;
        case EV_AUTHENTICATION_COMPLETE:// 0x06
            handle_hci_authentication_complete();
            break;
        case EV_REMOTE_NAME_COMPLETE: // 0x07
            handle_hci_remote_name_complete();
            break;
        case EV_READ_REMOTE_VERSION_INFORMATION_COMPLETE:
            handle_hci_remote_version_information_complete();
            break;
        case EV_PIN_CODE_REQUEST: // 0x16
            handle_hci_pin_code_request();
            break;
			
		//use simple pairing
        case EV_READ_REMOTE_SUPPORTED_FEATURES_COMPLETE: //0x0B
            USBHDBGSerial.printf(" Remote read features complete: status:%x ", rxbuf_[2]);
            #if defined(DEBUG_BT_VERBOSE) 
				print_error_codes(rxbuf_[2]);
			#endif
            USBHDBGSerial.printf(" Requested to use SSP Pairing: %d\n", do_pair_ssp_);
            if ( (rxbuf_[11]) & (0x01 << 3)) {
                if (current_connection_) {
                    current_connection_->supports_SSP_ = true;
                    USBHDBGSerial.printf("%d\n", current_connection_->supports_SSP_);
                }
                //sendHCIRemoteNameRequest();
            } else {
                USBHDBGSerial.printf("No Support for SPP\n");
                //USBHDBGSerial.printf("Try just say yes\n");
                //current_connection_->supports_SSP_ = true;

            }
#if 1
            // Try bypass role discovery did not help much anyway
            // Note We may need to check for PS4 or the like and
            // start connection. Let me try by setting timer like
            // Set a timeout
            setTimer(current_connection_, BluetoothConnection::CONNECTION_TIMEOUT_US);

#else            
            sendHCIRoleDiscoveryRequest();
#endif            
            break;

        case EV_READ_REMOTE_EXTENDED_FEATURES_COMPLETE:  //0x23
            USBHDBGSerial.printf(" Extended features read complete: status:%x ", rxbuf_[2]);
            #if defined(DEBUG_BT_VERBOSE)
				print_error_codes(rxbuf_[2]);
			#endif
			USBHDBGSerial.printf(" Requested to use SSP Pairing: %d\n", do_pair_ssp_);
            if ( ((rxbuf_[7] >> 0) & 0x01) == 1) {
                if (current_connection_) {
                    current_connection_->supports_SSP_ = true;
                    USBHDBGSerial.printf("%d\n", current_connection_->supports_SSP_);
                }
                //sendHCIRemoteNameRequest();
            } else {
                USBHDBGSerial.printf("No Support for SPP\n");
            }
            sendHCIRoleDiscoveryRequest();
            break;
		case EV_ENCRYPTION_CHANGE:// < UseSimplePairing
			if(has_key == true ) {
				USBHDBGSerial.printf(" Change to Link Encryption:  ");
				sendHCISetConnectionEncryption();  // use simple pairing   hangs the link 
				has_key = false;
			}
			//handle_hci_encryption_change_complete();
			break;		
		case EV_RETURN_LINK_KEYS:
            handle_hci_return_link_keys();
			break;
		case EV_SIMPLE_PAIRING_COMPLETE:
			if(!rxbuf_[2]) { // Check if pairing was Complete
                USBHDBGSerial.printf("\r\nSimple Pairing Complete\n");
            } else {
                USBHDBGSerial.printf("\r\nPairing Failed: \n");
            }
			break;
		case EV_MAX_SLOTS_CHANGE:
			USBHDBGSerial.printf("Received Max Slot change Msg\n");
			break;
		case EV_USER_CONFIRMATION_REQUEST:
			handle_hci_user_confirmation_request_reply();
			break;
		

        case EV_LINK_KEY_REQUEST:   // 0x17
            handle_hci_link_key_request();
            break;
        case EV_LINK_KEY_NOTIFICATION: // 0x18
            handle_hci_link_key_notification();
            break;
        case EV_INQUIRY_RESULTS_WITH_RSSI:
            handle_hci_inquiry_result(true);
            break;
        case EV_EXTENDED_INQUIRY_RESULT:
            #if 0
            handle_hci_extended_inquiry_result();
            #else
            DBGPrintf("$$$ EV_EXTENDED_INQUIRY_RESULT Disabled\n");
            #endif
            break;
        case EV_IO_CAPABILITY_RESPONSE:
            handle_HCI_IO_CAPABILITY_REQUEST_REPLY();
            break;
        case EV_IO_CAPABILITY_REQUEST:
            handle_hci_io_capability_request();
            break;
        case EV_ROLE_CHANGED:
            // 12 08 00 16 AC B3 26 3F C8 00
            VDBGPrintf("    EV_ROLE_CHANGED: st:%u bdaddr: %02x:%02x:%02x:%02x:%02x:%02x role:%u(%s)\n",
                rxbuf_[2], rxbuf_[3], rxbuf_[4], rxbuf_[5], rxbuf_[6], rxbuf_[7], rxbuf_[8], 
                rxbuf_[9], rxbuf_[9]?  "Peripheral" : "Central" );
            break;
        case EV_NUM_COMPLETE_PKT: //13 05 01 47 00 01 00 
            VDBGPrintf("    NUM_COMPLETE_PKT: ch:%u fh:%04x comp:%u\n",
                rxbuf_[2], rxbuf_[3] + (rxbuf_[4] << 8), rxbuf_[rxbuf_[1]] + (rxbuf_[rxbuf_[1] + 1] << 8) );
            break;
        case EV_LE_META_EVENT:
            handle_ev_meta_event(); // 0x3e
            break;
        default:
            break;
        }
        // Start read at start of buffer.
        queue_Data_Transfer_Debug(rxpipe_, rxbuf_, rx_size_, this, __LINE__);
    } else {
        // Continue the read - Todo - maybe verify len == rx_size_
        queue_Data_Transfer_Debug(rxpipe_, buffer + rx_size_, rx_size_, this, __LINE__);
        return;     // Don't process the message yet as we still have data to receive.
    }
}

#ifdef DEBUG_BT_VERBOSE
void print_supported_commands(uint8_t *cmd_data) { 
    typedef struct {
        uint8_t octet;
        uint8_t bit;
        const char *name;
    } pss_data_t;
    static const pss_data_t pssd[] = {
    { 0, 0, /*0x0401*/  "HCI_INQUIRY"},                         
    { 0, 1, /*0x0402*/  "HCI_INQUIRY_CANCEL"},                  
    { 0, 4, /*0x0405*/  "HCI_CREATE_CONNECTION"},               
    { 1, 0, /*0x0409*/  "HCI_OP_ACCEPT_CONN_REQ"},              
    { 1, 1, /*0x040A*/  "HCI_OP_REJECT_CONN_REQ"},              
    { 1, 2, /*0x040B*/  "HCI_LINK_KEY_REQUEST_REPLY"},            
    { 1, 3, /*0x040C*/  "HCI_LINK_KEY_NEG_REPLY"},              
    { 1, 4, /*0x040D*/  "HCI_PIN_CODE_REPLY"},                  
    { 1, 7, /*0x0411*/  "HCI_AUTH_REQUESTED"},                  
    { 2, 0, /*0x0413*/  "HCI_SET_CONN_ENCRYPTION"},             
    { 2, 3, /*0x0419*/  "HCI_OP_REMOTE_NAME_REQ"},              
    { 2, 4, /*0x041a*/  "HCI_OP_REMOTE_NAME_REQ_CANCEL"},       
    { 2, 5, /*0x041b*/  "HCI_OP_READ_REMOTE_FEATURES"},         
    { 2, 6, /*0x041c*/  "HCI_OP_READ_REMOTE_EXTENDED_FEATURE"}, 
    { 2, 7, /*0x041D*/  "HCI_OP_READ_REMOTE_VERSION_INFORMATION"}, 
    { 18, 7, /*0x042B*/  "HCI_IO_CAPABILITY_REQUEST_REPLY"},          
    { 19, 0, /*0x042C*/  "HCI_USER_CONFIRMATION_REQUEST"},       
    { 4, 7, /*0x0809*/  "HCI_OP_ROLE_DISCOVERY"},               
    { 5, 4, /*0x080f*/  "HCI_Write_Default_Link_Policy_Settings"},  
    { 5, 6, /*0x0c01*/  "HCI_Set_Event_Mask"},                  
    { 5, 7, /*0x0c03*/  "HCI_RESET"},                           
    { 6, 0, /*0x0c05*/  "HCI_SET_EVENT_FILTER"},          
    { 6, 5, /*0x0c0d*/  "HCI_READ_STORED_LINK_KEY"},            
    { 6, 7, /*0x0c12*/  "HCI_DELETE_STORED_LINK_KEY"},          
    { 7, 0, /*0x0c13*/  "HCI_WRITE_LOCAL_NAME"},                
    { 7, 1, /*0x0c14*/  "HCI_Read_Local_Name"},                 
    { 7, 3, /*0x0c16*/  "HCI_Write_Connection_Accept_Timeout"},     
    { 7, 6, /*0x0c1a*/  "HCI_WRITE_SCAN_ENABLE"},               
    { 8, 0, /*0x0c1b*/  "HCI_Read_Page_Scan_Activity"},         
    { 9, 0, /*0x0c23*/  "HCI_READ_CLASS_OF_DEVICE"},            
    { 9, 1, /*0x0C24*/  "HCI_WRITE_CLASS_OF_DEV"},              
    { 9, 2, /*0x0c25*/  "HCI_Read_Voice_Setting"},              
    { 11, 2, /*0x0c38*/  "HCI_Read_Number_Of_Supported_IAC"},    
    { 11, 3, /*0x0c39*/  "HCI_Read_Current_IAC_LAP"},            
    { 12, 6, /*0x0c45*/  "HCI_WRITE_INQUIRY_MODE"},              
    { 13, 1, /*0x0c46*/  "HCI_Read_Page_Scan_Type"},             
    { 17, 1, /*0x0c52*/  "HCI_WRITE_EXTENDED_INQUIRY_RESPONSE"},                       
    { 17, 5, /*0x0c55*/  "HCI_READ_SIMPLE_PAIRING_MODE"},                   
    { 17, 6, /*0x0c56*/  "HCI_WRITE_SIMPLE_PAIRING_MODE"},                  
    { 18, 0, /*0x0c58*/  "HCI_Read_Inquiry_Response_Transmit_Power_Level"}, 
    { 24, 5, /*???? */  "HCI_READ_LE_HOST_SUPPORTED"},         
    { 24, 6, /*0x0c6d*/  "HCI_WRITE_LE_HOST_SUPPORTED"},         

    #if 0
    { 0, 0, /*0x1001*/  "HCI_Read_Local_Version_Information"},  
    { 0, 0, /*0x1002*/  "HCI_Read_Local_Supported_Commands"},   
    { 25, 2, /*0x1003*/  "HCI_Read_Local_Supported_Features"},   
    { 0, 0, /*0x1004*/  "HCI_Read_Local_Extended_Features"},    
    { 25, 1, /*0x1005*/  "HCI_Read_Buffer_Size"},                
    { 0, 0, /*0x1009*/  "HCI_Read_BD_ADDR"},                    
    { 0, 0, /*0x1408*/  "HCI_READ_ENCRYPTION_KEY_SIZE"},        
    { 0, 0, /*0x2001*/  "HCI_LE_SET_EVENT_MASK"},               
    { 0, 0, /*0x2002*/  "HCI_LE_Read_Buffer_Size"},             
    { 0, 0, /*0x2003*/  "HCI_LE_Read_Local_supported_Features"}, 
    { 0, 0, /*0x2007*/  "HCI_LE_READ_ADV_TX_POWER"},            
    { 0, 0, /*0x2008*/  "HCI_LE_SET_ADV_DATA"},                 
    { 0, 0, /*0x2009*/  "HCI_LE_SET_SCAN_RSP_DATA"},            
    { 0, 0, /*0x200f*/  "HCI_LE_READ_WHITE_LIST_SIZE"},         
    { 0, 0, /*0x2010*/  "HCI_LE_CLEAR_WHITE_LIST"},             
    { 0, 0, /*0x201c*/  "HCI_LE_Supported_States"},             
    #endif
    };

    USBHDBGSerial.printf("\n### Local Supported Commands ###\n");

    for (uint16_t i = 0; i < (sizeof(pssd) / sizeof(pssd[0])); i++) {
        uint8_t mask = 1 << pssd[i].bit;
        USBHDBGSerial.printf("\t%s - ", pssd[i].name);
        if (cmd_data[pssd[i].octet] & mask) USBHDBGSerial.printf("yes\n");
        else USBHDBGSerial.printf("** NO ***\n");
    }
}
#endif

#ifdef DEBUG_BT_VERBOSE
void print_supported_features(uint8_t *cmd_data) { 
    typedef struct {
        uint8_t octet;
        uint8_t bit;
        const char *name;
    } pss_data_t;
    static const pss_data_t pssd[] = {
        {0, 0, "3 slot packets"}, 
        {0, 1, "5 slot packets"}, 
        {0, 2, "Encryption"}, 
        {0, 3, "Slot offset"}, 
        {0, 4, "Timing accuracy"}, 
        {0, 5, "Role switch"}, 
        {0, 6, "Hold mode"}, 
        {0, 7, "Sniff mode"}, 
        {1, 0, "Reserved"}, 
        {1, 1, "Power control requests"}, 
        {1, 2, "Channel quality driven data rate (CQDDR)"}, 
        {1, 3, "SCO link"}, 
        {1, 4, "HV2 packets"}, 
        {1, 5, "HV3 packets"}, 
        {1, 6, "μ-law log synchronous data"}, 
        {1, 7, "A-law log synchronous data"}, 
        {2, 0, "CVSD synchronous data"}, 
        {2, 1, "Paging parameter negotiation"}, 
        {2, 2, "Power control"}, 
        {2, 3, "Transparent synchronous data"}, 
        {2, 4, "Flow control lag (least significant bit)"}, 
        {2, 5, "Flow control lag (middle bit)"}, 
        {2, 6, "Flow control lag (most significant bit)"}, 
        {2, 7, "Broadcast Encryption"}, 
        {3, 1, "Enhanced Data Rate ACL 2 Mb/s mode"}, 
        {3, 2, "Enhanced Data Rate ACL 3 Mb/s mode"}, 
        {3, 3, "Enhanced inquiry scan"}, 
        {3, 4, "Interlaced inquiry scan"}, 
        {3, 5, "Interlaced page scan"}, 
        {3, 6, "RSSI with inquiry results"}, 
        {3, 7, "Extended SCO link (EV3 packets)"}, 
        {4, 0, "EV4 packets"}, 
        {4, 1, "EV5 packets"}, 
        {4, 3, "AFH capable slave"}, 
        {4, 4, "AFH classification slave"}, 
        {4, 5, "BR/EDR Not Supported"}, 
        {4, 6, "LE Supported (Controller)"}, 
        {4, 7, "3-slot Enhanced Data Rate ACL packets"}, 
        {5, 0, "5-slot Enhanced Data Rate ACL packets"}, 
        {5, 1, "Sniff subrating"}, 
        {5, 2, "Pause encryption"}, 
        {5, 3, "AFH capable master"}, 
        {5, 4, "AFH classification master"}, 
        {5, 5, "Enhanced Data Rate eSCO 2 Mb/s mode"}, 
        {5, 6, "Enhanced Data Rate eSCO 3 Mb/s mode"}, 
        {5, 7, "3-slot Enhanced Data Rate eSCO packets"}, 
        {6, 0, "Extended Inquiry Response"}, 
        {6, 1, "Simultaneous LE and BR/EDR to Same Device Capable(Controller)"}, 
        {6, 3, "Secure Simple Pairing"}, 
        {6, 4, "Encapsulated PDU"}, 
        {6, 5, "Erroneous Data Reporting"}, 
        {6, 6, "Non-flushable Packet Boundary Flag"}, 
        {7, 0, "Link Supervision Timeout Changed Event"}, 
        {7, 1, "Inquiry TX Power Level"}, 
        {7, 2, "Enhanced Power Control"}, 
        {7, 7, "Extended features"}
    };
    USBHDBGSerial.printf("\n### Local Supported Features ###\n");

    for (uint16_t i = 0; i < (sizeof(pssd) / sizeof(pssd[0])); i++) {
        uint8_t mask = 1 << pssd[i].bit;
        USBHDBGSerial.printf("\t%s - ", pssd[i].name);
        if (cmd_data[pssd[i].octet] & mask) USBHDBGSerial.printf("yes\n");
        else USBHDBGSerial.printf("** NO ***\n");
    }
}
#endif


//===================================================================
// Called when an HCI command completes.
void BluetoothController::handle_hci_command_complete()
{
    uint16_t hci_command = rxbuf_[3] + (rxbuf_[4] << 8);
    uint8_t buffer_index;
    #ifdef DEBUG_BT_VERBOSE
    if (!rxbuf_[5]) {
        VDBGPrintf("    Command Completed! \n");
    } else {
        VDBGPrintf("    Command(%x) Completed - Error: 0x%x!", hci_command, rxbuf_[5], rxbuf_[5]);
        // BUGBUG:: probably need to queue something?
        print_error_codes(rxbuf_[5]);
    }
    #endif
    switch (hci_command) {
    case HCI_OP_REMOTE_NAME_REQ:
        break;
    case HCI_RESET: //0x0c03
        if (!rxbuf_[5]) pending_control_++;
        //  If it fails, will retry. maybe should have repeat max...
        break;
    case HCI_SET_EVENT_FILTER:    //0x0c05
        break;
    case HCI_Read_Local_Name:   //0x0c14
        // received name back...
    {
        //BUGBUG:: probably want to grab string object and copy to
        USBHDBGSerial.printf("    Local name: %s\n", &rxbuf_[6]);
        /*
                        uint8_t len = rxbuf_[1]+2;  // Length field +2 for total bytes read
                        for (uint8_t i=6; i < len; i++) {
                            if (rxbuf_[i] == 0) {
                                break;
                            }
                            USBHDBGSerial.printf("%c", rxbuf_[i]);
                        }
                        USBHDBGSerial.printf("\n"); */
    }
    break;
    case Write_Connection_Accept_Timeout:   //0x0c16
        break;
    case HCI_READ_CLASS_OF_DEVICE:  // 0x0c23
        break;
    case HCI_Read_Voice_Setting:    //0x0c25
        break;
    case HCI_Read_Number_Of_Supported_IAC:  //0x0c38
        break;
    case HCI_Read_Current_IAC_LAP:  //0x0c39
        break;
    case HCI_WRITE_INQUIRY_MODE:    //0x0c45
        break;
    case HCI_Read_Inquiry_Response_Transmit_Power_Level: //0x0c58
        break;
    case HCI_Read_Local_Supported_Commands: //0x1002
        DBGPrintf("    HCI_Read_Local_Supported_Commands\n");
        #ifdef DEBUG_BT_VERBOSE
        print_supported_commands(&rxbuf_[6]);
        #endif
        break;
    
    case HCI_Read_Local_Supported_Features: //0x1003
        DBGPrintf("    HCI_Read_Local_Supported_Features\n");
        #ifdef DEBUG_BT_VERBOSE
        print_supported_features(&rxbuf_[6]);
        #endif
        // Remember the features supported by local...
        for (buffer_index = 0; buffer_index < 8; buffer_index++) {
            features[buffer_index] = rxbuf_[buffer_index + 6];
        }
        break;
    case HCI_Read_Buffer_Size:  // 0x1005
        break;
    case HCI_Read_BD_ADDR:  //0x1009
    {
        for (uint8_t i = 0; i < 6; i++) my_bdaddr_[i] = rxbuf_[6 + i];
        DBGPrintf("   BD Addr %x:%x:%x:%x:%x:%x\n", my_bdaddr_[5], my_bdaddr_[4], my_bdaddr_[3], my_bdaddr_[2], my_bdaddr_[1], my_bdaddr_[0]);
    }
    break;
    case HCI_Read_Local_Version_Information:    //0x1001
        hciVersion = rxbuf_[6];     // Should do error checking above...
        DBGPrintf("    Local Version: %x\n", hciVersion);
		if( do_pair_ssp_ ) {
			pending_control_ = PC_SEND_WRITE_INQUIRE_MODE;
		} else {
			pending_control_ = (do_pair_device_) ? PC_SEND_WRITE_INQUIRE_MODE : PC_WRITE_SCAN_PAGE;
		}
        break;
    case HCI_LE_Read_Buffer_Size:   //0x2002
        {
            DBGPrintf("LE Buffer Size: Size:%u, cnt:%u\n",
                rxbuf_[6] + (rxbuf_[7] << 8), rxbuf_[8]);
        }
        break;
    case HCI_LE_Read_Local_supported_Features:  //0x2003
        break;
    case HCI_LE_Supported_States:   //0x201c
        break;

    case HCI_Read_Local_Extended_Features:  //0x1004
        break;
    case HCI_Set_Event_Mask:                    //0x0c01
        break;
    case HCI_READ_STORED_LINK_KEY:          //0x0c0d
        DBGPrintf("    HCI_READ_STORED_LINK_KEY keys total:%u read:%u\n", 
            rxbuf_[6] + (rxbuf_[7] << 8), rxbuf_[8]);
        break;
    case HCI_WRITE_STORED_LINK_KEY:          //0xc011
        DBGPrintf("    HCI_WRITE_STORED_LINK_KEY keys written:%u\n", rxbuf_[6]);
        break;
    case HCI_Write_Default_Link_Policy_Settings:    //0x080f
        break;
    case HCI_Read_Page_Scan_Activity:           //0x0c1b
        break;
    case HCI_Read_Page_Scan_Type:               //0x0c46
        break;
    case HCI_LE_SET_EVENT_MASK:             //0x2001
        break;
    case HCI_LE_READ_ADV_TX_POWER:          //0x2007
        break;
    case HCI_LE_READ_WHITE_LIST_SIZE:           //0x200f
        break;
    case HCI_LE_CLEAR_WHITE_LIST:               //0x2010
        break;
    case HCI_DELETE_STORED_LINK_KEY:            //0x0c12
        break;
    case HCI_WRITE_LOCAL_NAME:              //0x0c13
        break;
    case HCI_WRITE_SCAN_ENABLE:             //0x0c1a
        current_connection_->handle_HCI_WRITE_SCAN_ENABLE_complete(rxbuf_);
        break;
	case HCI_READ_SIMPLE_PAIRING_MODE:                    //0x0c55
		break;
    case HCI_WRITE_SIMPLE_PAIRING_MODE:                    //0x0c56
		//sendHCIReadSimplePairingMode();
        break;
    case HCI_WRITE_EXTENDED_INQUIRY_RESPONSE:                     //0x0c52
        break;
    case HCI_WRITE_LE_HOST_SUPPORTED:           //0x0c6d
        break;
    case HCI_LE_SET_SCAN_RSP_DATA:          //0x2009
        break;
    case HCI_LINK_KEY_NEG_REPLY:
        //if (current_connection_->device_class_ == 0x2508) {
        //  DBGPrintf("Hack see if we can catch the Terios here");
        //  pending_control_ = PC_CONNECT_AFTER_SDP_DISCONNECT;
        //}
        break;

    case HCI_OP_ROLE_DISCOVERY: // 0x0809
        current_connection_->handle_HCI_OP_ROLE_DISCOVERY_complete(rxbuf_);
        break;

    case HCI_INQUIRY_CANCEL: // 0x0402
        // Question should I setup up PC for this or do inline
        sendHCIRemoteNameRequest();

        break;

	case HCI_OP_READ_REMOTE_FEATURES:
		break; //0x041b
	case  HCI_OP_READ_REMOTE_EXTENDED_FEATURE:
		break;	//0x041c
	case HCI_SET_CONN_ENCRYPTION:
		break;
	case HCI_READ_ENCRYPTION_KEY_SIZE:
		break;
	case HCI_LINK_KEY_REQUEST_REPLY:
		DBGPrintf("\tHCI_LINK_KEY_REQUEST_REPLY\n");
		break;
	case HCI_USER_CONFIRMATION_REQUEST: //0x042C
		DBGPrintf("User Confirmation Request Reply\n");
		break;
	case HCI_IO_CAPABILITY_REQUEST_REPLY:  //0x042B
		break;
    }
    // And queue up the next command
    queue_next_hci_command();
}

void BluetoothController::queue_next_hci_command()
{
    // Ok We completed a command now see if we need to queue another command
    // Still probably need to reorganize...
    switch (pending_control_) {
    // Initial setup states.
    case PC_RESET:
        sendResetHCI();
        break;
    case PC_READ_LOCAL_SUPPORTED_COMMANDS:
        sendHCIReadLocalSupportedCommands();
        pending_control_++;
        break;
    case PC_READ_LOCAL_SUPPORTED_FEATURES:
        sendHCIReadLocalSupportedFeatures();
        pending_control_++;
        break;
    case PC_SEND_SET_EVENT_MASK:
        sendHCISetEventMask();  // Set the event mask to include extend inquire event
        pending_control_++;
        break;
    // ----------------------
    // Added some LE setup as experiment
    case PC_SET_LE_EVENT_MASK:
        {
            DBGPrintf("HCI_LE_SET_EVENT_MASK\n");
            static const uint8_t hci_le_event_mask_data[8] = {
                0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
            sendHCICommand(HCI_LE_SET_EVENT_MASK, sizeof(hci_le_event_mask_data), hci_le_event_mask_data);
        }  // default plus extended inquiry mode
        pending_control_++;
        break;
    case PC_LE_READ_BUFFER_SIZE:
        DBGPrintf("HCI_LE_Read_Buffer_Size\n");
        sendHCICommand(HCI_LE_Read_Buffer_Size, 0, nullptr);
        pending_control_++;
        break;
    case PC_WRITE_CLASS_DEVICE:
        sendHDCWriteClassOfDev();
        pending_control_++;
        break;

    case PC_MAYBE_WRITE_SIMPLE_PAIR:
        pending_control_++;
        if(do_pair_ssp_) {
            sendHCISimplePairingMode();
             break;
        }
        // otherwise fall through

    case PC_MAYBE_READ_SIMPLE_PAIR:
        pending_control_++;
        if(do_pair_ssp_) {
            sendHCIReadSimplePairingMode();
            break;
        }
        // otherwise fall through

    case PC_READ_BDADDR:
        sendHCIReadBDAddr();
        pending_control_++;
        break;

    case PC_READ_LOCAL_VERSION:
        sendHCIReadLocalVersionInfo();
        pending_control_++;
        break;

    // These are used when we are pairing.
    case PC_SEND_WRITE_INQUIRE_MODE:
        sendHCIHCIWriteInquiryMode(1/*2 */);  // lets set into extended inquire mode
        pending_control_++;
        break;


    case PC_SEND_INQUIRE:
        sendHCI_INQUIRY();
        pending_control_++;
        break;
    case PC_INQUIRE_CANCEL:
        // lets try to create a connection...
        sendHCICreateConnection();
        pending_control_++;
        break;
    case PC_AUTHENTICATION_REQUESTED:
        break;
    case PC_LINK_KEY_NEGATIVE:
        break;
    case PC_PIN_CODE_REPLY:
        break;
    case PC_SEND_REMOTE_SUPPORTED_FEATURES:
        pending_control_++;
        break;
    case PC_SEND_REMOTE_EXTENDED_FEATURES:
        pending_control_++;
        break;
    case PC_CONNECT_AFTER_SDP_DISCONNECT:
        // Hack see if we can get the Joystick to initiate the create a connection...
        current_connection_->sendl2cap_ConnectionRequest(current_connection_->device_connection_handle_, current_connection_->connection_rxid_, current_connection_->control_dcid_, HID_CTRL_PSM);
        pending_control_ = 0;  //
        break;
    // Done Pair mode

    case PC_WRITE_SCAN_PAGE:
        sendHCIWriteScanEnable(2);
        pending_control_ = 0;   //
        break;
    default:
        break;
    }

}


void BluetoothController::handle_hci_command_status()
{
    // <event type><param count><status><num packets allowed to be sent><CMD><CMD>
    uint16_t hci_command = rxbuf_[4] + (rxbuf_[5] << 8);
    #ifdef DEBUG_BT_VERBOSE
    DBGPrintf("    Command %x(", hci_command);
    switch (hci_command) {
        case 0x0401: DBGPrintf("HCI_INQUIRY"); break;
        case 0x0402: DBGPrintf("HCI_INQUIRY_CANCEL"); break;
        case 0x0405: DBGPrintf("HCI_CREATE_CONNECTION"); break;
        case 0x0409: DBGPrintf("HCI_OP_ACCEPT_CONN_REQ"); break;
        case 0x040A: DBGPrintf("HCI_OP_REJECT_CONN_REQ"); break;
        case 0x040C: DBGPrintf("HCI_LINK_KEY_NEG_REPLY"); break;
        case 0x040D: DBGPrintf("HCI_PIN_CODE_REPLY"); break;
        case 0x0411: DBGPrintf("HCI_AUTH_REQUESTED"); break;
        case 0x0419: DBGPrintf("HCI_OP_REMOTE_NAME_REQ"); break;
        case 0x041a: DBGPrintf("HCI_OP_REMOTE_NAME_REQ_CANCEL"); break;
        case 0x041b: DBGPrintf("HCI_OP_READ_REMOTE_FEATURES"); break;
        case 0x041c: DBGPrintf("HCI_OP_READ_REMOTE_EXTENDED_FEATURE"); break;
        case 0x041D: DBGPrintf("HCI_OP_READ_REMOTE_VERSION_INFORMATION"); break;
        case 0x0809: DBGPrintf("HCI_OP_ROLE_DISCOVERY"); break;
        case 0x080f: DBGPrintf("HCI_Write_Default_Link_Policy_Settings"); break;
        case 0x0c01: DBGPrintf("HCI_Set_Event_Mask"); break;
        case 0x0c03: DBGPrintf("HCI_RESET"); break;
        case 0x0c05: DBGPrintf("HCI_SET_EVENT_FILTER"); break;
        case 0x0c14: DBGPrintf("HCI_Read_Local_Name"); break;
        case 0x0c0d: DBGPrintf("HCI_READ_STORED_LINK_KEY"); break;
        case 0x0c12: DBGPrintf("HCI_DELETE_STORED_LINK_KEY"); break;
        case 0x0c13: DBGPrintf("HCI_WRITE_LOCAL_NAME"); break;
        case 0x0c16: DBGPrintf("Write_Connection_Accept_Timeout"); break;
        case 0x0c1a: DBGPrintf("HCI_WRITE_SCAN_ENABLE"); break;
        case 0x0c1b: DBGPrintf("HCI_Read_Page_Scan_Activity"); break;
        case 0x0c23: DBGPrintf("HCI_READ_CLASS_OF_DEVICE"); break;
        case 0x0C24: DBGPrintf("HCI_WRITE_CLASS_OF_DEV"); break;
        case 0x0c25: DBGPrintf("HCI_Read_Voice_Setting"); break;
        case 0x0c38: DBGPrintf("HCI_Read_Number_Of_Supported_IAC"); break;
        case 0x0c39: DBGPrintf("HCI_Read_Current_IAC_LAP"); break;
        case 0x0c45: DBGPrintf("HCI_WRITE_INQUIRY_MODE"); break;
        case 0x0c46: DBGPrintf("HCI_Read_Page_Scan_Type"); break;
        case 0x0c52: DBGPrintf("HCI_WRITE_EXTENDED_INQUIRY_RESPONSE"); break;
        case 0x0c56: DBGPrintf("HCI_WRITE_SIMPLE_PAIRING_MODE"); break;
        case 0x0c58: DBGPrintf("HCI_Read_Inquiry_Response_Transmit_Power_Level"); break;
        case 0x0c6d: DBGPrintf("HCI_WRITE_LE_HOST_SUPPORTED"); break;
        case 0x1003: DBGPrintf("HCI_Read_Local_Supported_Features"); break;
        case 0x1004: DBGPrintf("HCI_Read_Local_Extended_Features"); break;
        case 0x1005: DBGPrintf("HCI_Read_Buffer_Size"); break;
        case 0x1009: DBGPrintf("HCI_Read_BD_ADDR"); break;
        case 0x1001: DBGPrintf("HCI_Read_Local_Version_Information"); break;
        case 0x1002: DBGPrintf("HCI_Read_Local_Supported_Commands"); break;
        case 0x2001: DBGPrintf("HCI_LE_SET_EVENT_MASK"); break;
        case 0x2002: DBGPrintf("HCI_LE_Read_Buffer_Size"); break;
        case 0x2003: DBGPrintf("HCI_LE_Read_Local_supported_Features"); break;
        case 0x2007: DBGPrintf("HCI_LE_READ_ADV_TX_POWER"); break;
        case 0x2008: DBGPrintf("HCI_LE_SET_ADV_DATA"); break;
        case 0x2009: DBGPrintf("HCI_LE_SET_SCAN_RSP_DATA"); break;
        case 0x200f: DBGPrintf("HCI_LE_READ_WHITE_LIST_SIZE"); break;
        case 0x2010: DBGPrintf("HCI_LE_CLEAR_WHITE_LIST"); break;
        case 0x201c: DBGPrintf("HCI_LE_Supported_States"); break;
    }
    DBGPrintf(") Status %x", rxbuf_[2]);
    #endif

    if (rxbuf_[2]) {
        #ifdef DEBUG_BT_VERBOSE
        DBGPrintf(" - ");
        print_error_codes(rxbuf_[2]);
        #endif

        // lets try recovering from some errors...
        switch (hci_command) {
        case HCI_OP_ACCEPT_CONN_REQ:
            // We assume that the connection failed...
            DBGPrintf("### Connection Failed ###");
            if (count_connections_) count_connections_--;
            break;
        default:
            break;
        }

    } else {
        VDBGPrintf("\n");
    }
}

void BluetoothController::handle_hci_inquiry_result(bool fRSSI)
{
    // result - versus result with RSSI
    //                              | Diverge here...
    //  2 f 1 79 22 23 a c5 cc 1 2  0 40 25 0 3b 2
    // 22 f 1 79 22 23 a c5 cc 1 2 40 25 0 3e 31 d2

    // Wondered if multiple items if all of the BDADDR are first then next field...
    // looks like it is that way...
    // Section 7.7.2
    if (fRSSI) DBGPrintf("    Inquiry Result with RSSI - Count: %d\n", rxbuf_[2]);
    else DBGPrintf("    Inquiry Result - Count: %d\n", rxbuf_[2]);
    for (uint8_t i = 0; i < rxbuf_[2]; i++) {
        uint8_t index_bd = 3 + (i * 6);
        uint8_t index_ps = 3 + (6 * rxbuf_[2]) + i;
        uint8_t index_class = 3 + (9 * rxbuf_[2]) + i;
        uint8_t index_clock_offset = 3 + (12 * rxbuf_[2]) + i;
        if (fRSSI) {
            // Handle the differences in offsets here...
            index_class = 3 + (8 * rxbuf_[2]) + i;
            index_clock_offset = 3 + (11 * rxbuf_[2]) + i;
        }
        uint32_t bluetooth_class = rxbuf_[index_class] + ((uint32_t)rxbuf_[index_class + 1] << 8) + ((uint32_t)rxbuf_[index_class + 2] << 16);
        DBGPrintf("      BD:%x:%x:%x:%x:%x:%x, PS:%d, class: %x\n",
                  rxbuf_[index_bd], rxbuf_[index_bd + 1], rxbuf_[index_bd + 2], rxbuf_[index_bd + 3], rxbuf_[index_bd + 4], rxbuf_[index_bd + 5],
                  rxbuf_[index_ps], bluetooth_class);
        // See if we know the class
        if (((bluetooth_class & 0xff00) == 0x2500) || ((bluetooth_class & 0xff00) == 0x500)) {
            DBGPrintf("      Peripheral device\n");
            if (bluetooth_class & 0x80) DBGPrintf("        Mouse\n");
            if (bluetooth_class & 0x40) DBGPrintf("        Keyboard\n");
            switch (bluetooth_class & 0x3c) {
            case 4: DBGPrintf("        Joystick\n"); break;
            case 8: DBGPrintf("        Gamepad\n"); break;
            case 0xc: DBGPrintf("        Remote Control\n"); break;
            }

            if (pairing_cb_) {
                if (!pairing_cb_->useInquireResult( &rxbuf_[index_bd], bluetooth_class, nullptr)) {
                    DBGPrintf("    $$ Callback return NO\n");
                    continue;
                }
            }
            // We need to allocate a connection for this.
            current_connection_ = BluetoothConnection::s_first_;
            while (current_connection_) {
                if (current_connection_->btController_ == nullptr) break;
                current_connection_ = current_connection_->next_;
            }
            if (current_connection_ == nullptr) {
                DBGPrintf("\tError no free BluetoothConnection object\n");
                return;
            }
            count_connections_++;

            // BUGBUG, lets hard code to go to new state...
            current_connection_->device_ps_repetion_mode_ = rxbuf_[index_ps]; // mode
            current_connection_->device_clock_offset_[0] = rxbuf_[index_clock_offset];
            current_connection_->device_clock_offset_[1] = rxbuf_[index_clock_offset + 1];

            current_connection_->initializeConnection(this, &rxbuf_[index_bd], bluetooth_class, true);


            // lets get the remote name now.
            // But first need to cancel the inquiry. 
            //sendHCIRemoteNameRequest();
            sendHCIInquiryCancel();
            pending_control_ = 0; //PC_INQUIRE_CANCEL;
#if 0
            if (current_connection_->device_driver_  || current_connection_->check_for_hid_descriptor_) {
                // Now we need to bail from inquiry and setup to try to connect...
                sendHCIInquiryCancel();
                pending_control_ = PC_INQUIRE_CANCEL;
                break;
            }
#endif            
        }
    }
}
#if 0
void BluetoothController::handle_hci_extended_inquiry_result()
{
    DBGPrintf("    Extended Inquiry Result - Count: %d\n", rxbuf_[2]);
    // Should always be only one result here.
    uint8_t index_bd = 3;
    uint8_t index_ps = 9;
    uint8_t index_class = 11;
    uint8_t index_clock_offset = 14;
    //uint8_t index_rssi = 16;
    uint8_t index_eir_data = 17;
    uint8_t index_local_name = 0;
    uint8_t size_local_name = 0;
    uint32_t bluetooth_class = rxbuf_[index_class] + ((uint32_t)rxbuf_[index_class + 1] << 8) + ((uint32_t)rxbuf_[index_class + 2] << 16);
    DBGPrintf("      BD:%x:%x:%x:%x:%x:%x, PS:%d, class: %x\n",
              rxbuf_[index_bd], rxbuf_[index_bd + 1], rxbuf_[index_bd + 2], rxbuf_[index_bd + 3], rxbuf_[index_bd + 4], rxbuf_[index_bd + 5],
              rxbuf_[index_ps], bluetooth_class);
    // Lets see if we can find a name
    while (index_eir_data < 256) {
        if (rxbuf_[index_eir_data] == 0) break; // no more data
        switch (rxbuf_[index_eir_data + 1]) {
        case 0x08: // Shortened local name
        case 0x09: // complete local name
            index_local_name = index_eir_data + 2;
            size_local_name = rxbuf_[index_eir_data] - 1;
            break;
        }
        index_eir_data += rxbuf_[index_eir_data] + 1;   // point to the next item

    }
    if (index_local_name && size_local_name) {
        // Hack lets null teminate the string
        rxbuf_[index_local_name + size_local_name] = 0;

        DBGPrintf("      Local Name: %s\n", &rxbuf_[index_local_name]);
    }

    // See if we know the class
    if (((bluetooth_class & 0xff00) == 0x2500) || ((bluetooth_class & 0xff00) == 0x500)) {
        DBGPrintf("      Peripheral device\n");
        if (bluetooth_class & 0x80) DBGPrintf("        Mouse\n");
        if (bluetooth_class & 0x40) DBGPrintf("        Keyboard\n");
        switch (bluetooth_class & 0x3c) {
        case 4: DBGPrintf("        Joystick\n"); break;
        case 8: DBGPrintf("        Gamepad\n"); break;
        case 0xc: DBGPrintf("        Remote Control\n"); break;
        }

        // try callback. 
        if (pairing_cb_) {
            if (!pairing_cb_->useInquireResult( &rxbuf_[index_bd], bluetooth_class, index_local_name ? &rxbuf_[index_local_name] : nullptr)) {
                DBGPrintf("    $$ Callback return NO\n");
                return;
            }
        }
        // We need to allocate a connection for this.
        current_connection_ = BluetoothConnection::s_first_;
        while (current_connection_) {
            if (current_connection_->btController_ == nullptr) break;
            current_connection_ = current_connection_->next_;
        }
        if (current_connection_ == nullptr) {
            DBGPrintf("\tError no free BluetoothConnection object\n");
            return;
        }
        count_connections_++;


        // BUGBUG, lets hard code to go to new state...
        current_connection_->initializeConnection(this, &rxbuf_[index_bd], bluetooth_class, index_local_name ? &rxbuf_[index_local_name] : nullptr);

        current_connection_->device_ps_repetion_mode_  = rxbuf_[index_ps]; // mode
        current_connection_->device_clock_offset_[0] = rxbuf_[index_clock_offset];
        current_connection_->device_clock_offset_[1] = rxbuf_[index_clock_offset + 1];

        // and if we found a driver, save away the name
        if (current_connection_->device_driver_ && index_local_name && size_local_name) {
            uint8_t buffer_index;
            for (buffer_index = 0; size_local_name && (buffer_index < BTHIDInput::REMOTE_NAME_SIZE - 1); buffer_index++) {
                current_connection_->device_driver_->remote_name_[buffer_index] = rxbuf_[index_local_name + buffer_index];
                size_local_name--;
            }
            current_connection_->device_driver_->remote_name_[buffer_index] = 0;    // make sure null terminated
        }

        // Now we need to bail from inquiry and setup to try to connect...
        sendHCIInquiryCancel();
        pending_control_ = PC_INQUIRE_CANCEL;
    }
}
#endif

void BluetoothController::handle_hci_io_capability_request()
{
    DBGPrintf("    Received IO Capability Request: %d\n", rxbuf_[2] );
    handle_hci_io_capability_request_reply();
}

void BluetoothController::handle_hci_io_capability_request_reply()
{
    uint8_t hcibuf[9];
    hcibuf[0] = current_connection_->device_bdaddr_[0]; // 6 octet bdaddr
    hcibuf[1] = current_connection_->device_bdaddr_[1];
    hcibuf[2] = current_connection_->device_bdaddr_[2];
    hcibuf[3] = current_connection_->device_bdaddr_[3];
    hcibuf[4] = current_connection_->device_bdaddr_[4];
    hcibuf[5] = current_connection_->device_bdaddr_[5];
    hcibuf[6] = 0x03; // NoInputNoOutput
    hcibuf[7] = 0x00; // OOB authentication data not present
    hcibuf[8] = 0x00; // MITM Protection Not Required ? No Bonding. Numeric comparison with automatic accept allowed

    DBGPrintf("HCI_IO_CAPABILITY_REPLY\n");
    sendHCICommand(HCI_IO_CAPABILITY_REQUEST_REPLY, sizeof(hcibuf), hcibuf);
}

void BluetoothController::handle_HCI_IO_CAPABILITY_REQUEST_REPLY()
{
    DBGPrintf("    Received IO Capability Response:\n");
    DBGPrintf("    IO capability: ");
    DBGPrintf ("%x\n", rxbuf_[8]);
    DBGPrintf("    OOB data present: ");
    DBGPrintf("%x\n", rxbuf_[9], 0x80);
    DBGPrintf("    nAuthentication request: ");
    DBGPrintf("%x\n", rxbuf_[10]);
	
	
}

void BluetoothController::handle_hci_user_confirmation_request_reply()
{
    uint8_t hcibuf[6];
    hcibuf[0] = current_connection_->device_bdaddr_[0]; // 6 octet bdaddr
    hcibuf[1] = current_connection_->device_bdaddr_[1];
    hcibuf[2] = current_connection_->device_bdaddr_[2];
    hcibuf[3] = current_connection_->device_bdaddr_[3];
    hcibuf[4] = current_connection_->device_bdaddr_[4];
    hcibuf[5] = current_connection_->device_bdaddr_[5];

    DBGPrintf("HCI_IO_CAPABILITY_REPLY\n");
    sendHCICommand(HCI_USER_CONFIRMATION_REQUEST, sizeof(hcibuf), hcibuf);
}


void BluetoothController::handle_hci_inquiry_complete() {
    VDBGPrintf("    Inquiry Complete - status: %d\n", rxbuf_[2]);
}

void BluetoothController::handle_hci_connection_complete() {
    //  0  1  2  3  4  5  6  7  8 9  10 11 12
    //       ST CH CH BD BD BD BD BD BD LT EN
    // 03 0b 04 00 00 40 25 00 58 4b 00 01 00
    current_connection_->device_connection_handle_ = rxbuf_[3] + (uint16_t)(rxbuf_[4] << 8);


    DBGPrintf("    Connection Complete - ST:%x LH:%x\n", rxbuf_[2], current_connection_->device_connection_handle_);
	DBGPrintf("Pairing - USE SSP PAIRING: %d,  DO PAIRING: %d\n", do_pair_ssp_, do_pair_device_);
    //sendHCIRoleDiscoveryRequest();
    if ((do_pair_device_ || do_pair_ssp_) && !(current_connection_->device_driver_ && (current_connection_->device_driver_->special_process_required & BTHIDInput::SP_DONT_NEED_CONNECT))) {
        sendHCIAuthenticationRequested();
        pending_control_ = PC_AUTHENTICATION_REQUESTED;
        // Best place to turn it off?
        //do_pair_device_ = false;
    } else {
        //sendHCIReadRemoteExtendedFeatures();
        sendHCIReadRemoteSupportedFeatures();
        pending_control_ = 0;
#if 0 // see if we can automatically do this by looking at roles
    } else if (current_connection_->device_driver_ && (current_connection_->device_driver_->special_process_required & BTHIDInput::SP_NEED_CONNECT)) {
        DBGPrintf("   Needs connect to device(PS4?)");
        // The PS4 requires a connection request to it.
        // But maybe not clones
        if (current_connection_->device_class_ == 0x2508) {
            DBGPrintf(" Yes\n");
            delay(1);
            current_connection_->sendl2cap_ConnectionRequest(current_connection_->device_connection_handle_, current_connection_->connection_rxid_, current_connection_->control_dcid_, HID_CTRL_PSM);
        } else {

            DBGPrintf(" No - Clone\n");
        }
#if 0
        delay(1);

        uint8_t packet[2];
        memset(packet, 0, sizeof(packet));
        packet[0] = 0x43;
        packet[1] = 0x02;      // Report ID
        USBHDBGSerial.printf("SixAxis Command Issued!\r\n");
        sendL2CapCommand(packet, sizeof(packet), 0x40);
#endif
#endif
    }

#if 0    
    static const uint8_t hci_event_mask_data[2] = {
        // Default: 0x0000 1FFF FFFF FFFF
        rxbuf_[3], rxbuf_[4]
    };  // default plus extended inquiry mode

    static const uint8_t hci_event_mask_data1[3] = {
        // Default: 0x0000 1FFF FFFF FFFF
        rxbuf_[3], rxbuf_[4], 0x01
    };  // default plus extended inquiry mode
    USBHDBGSerial.printf("Send Read Remote Supported Features");
    sendHCICommand(HCI_OP_READ_REMOTE_FEATURES, sizeof(hci_event_mask_data), hci_event_mask_data);
    pending_control_ = PC_SEND_REMOTE_SUPPORTED_FEATURES;
    delay(100);

    USBHDBGSerial.printf("Send Read Remote Extended Features");
    sendHCICommand(HCI_OP_READ_REMOTE_EXTENDED_FEATURE, sizeof(hci_event_mask_data1), hci_event_mask_data1);
    pending_control_ = PC_SEND_REMOTE_EXTENDED_FEATURES;
    delay(100);
    //USBHDBGSerial.printf("useSSP %d\n", useSSP);
    //if(useSSP == true) sendHCIRemoteNameRequest();
#endif
}

void BluetoothController::handle_hci_incoming_connect() {
    //           BD    BD   BD    BD  BD   BD  CL    CL    CL  LT
    // 0x04 0x0A 0x79 0x22 0x23 0x0A 0xC5 0xCC 0x40 0x05 0x00 0x01
    uint32_t class_of_device  = rxbuf_[8] + (uint16_t)(rxbuf_[9] << 8) + (uint32_t)(rxbuf_[10] << 16);
    DBGPrintf("    Event: Incoming Connect -  %x:%x:%x:%x:%x:%x CL:%x LT:%x\n",
              rxbuf_[2], rxbuf_[3], rxbuf_[4], rxbuf_[5], rxbuf_[6], rxbuf_[7], class_of_device, rxbuf_[11]);
    if (((class_of_device & 0xff00) == 0x2500) || ((class_of_device & 0xff00) == 0x500)) {
        DBGPrintf("      Peripheral device\n");
        if (class_of_device & 0x80) DBGPrintf("        Mouse\n");
        if (class_of_device & 0x40) DBGPrintf("        Keyboard\n");
        switch (class_of_device & 0x3c) {
        case 4: DBGPrintf("        Joystick\n"); break;
        case 8: DBGPrintf("        Gamepad\n"); break;
        case 0xc: DBGPrintf("        Remote Control\n"); break;
        }

        // BUGBUG: Should reject connection if no more room...
        current_connection_ = BluetoothConnection::s_first_;
        while (current_connection_) {
            if (current_connection_->btController_ == nullptr) break;
            current_connection_ = current_connection_->next_;
        }
        if (current_connection_ == nullptr) {
            // this one has to be special as don't have connection object to extract data from
            // reject for limited resources.
            sendHCIRejectConnectionRequest(&rxbuf_[2], 0xd);
            return;
        }
        count_connections_++;

        // Lets reinitialize some of the fields of this back to startup settings.
        current_connection_->device_ps_repetion_mode_ = 1; // mode we were always using?
        current_connection_->device_clock_offset_[0] = 0;
        current_connection_->device_clock_offset_[1] = 0;
        current_connection_->initializeConnection(this, &rxbuf_[2], class_of_device, false);

        sendHCIRemoteNameRequest();
    }

//  sendHCIAuthenticationRequested();
//  pending_control_ = PC_AUTHENTICATION_REQUESTED;
}


void BluetoothController::handle_hci_pin_code_request() {
    // 0x16 0x06 0x79 0x22 0x23 0x0A 0xC5 0xCC
    DBGPrintf("    Event: Pin Code Request %x:%x:%x:%x:%x:%x\n",
              rxbuf_[2], rxbuf_[3], rxbuf_[4], rxbuf_[5], rxbuf_[6], rxbuf_[7]);
    sendHCIPinCodeReply();
    pending_control_ = PC_PIN_CODE_REPLY;
}

void BluetoothController::handle_hci_link_key_request() {
    // 17 6 79 22 23 a c5 cc
    DBGPrintf("    Event: Link Key Request %x:%x:%x:%x:%x:%x\n",
              rxbuf_[2], rxbuf_[3], rxbuf_[4], rxbuf_[5], rxbuf_[6], rxbuf_[7]);

    // Now here is where we need to decide to say we have key or tell them to
    // cancel key...  right now hard code to cancel...
    #if 1
    // see if we can read in stored key...
    // two ways out of this 
    // 1 we will receive an event with a kay
    // 2 we won't receive event but the command complete will show 0 returned.
    bool send_link_key_reply = false;
    uint8_t link_key[16];
    if (!do_pair_ssp_) {
        send_link_key_reply = readLinkKey(current_connection_->device_bdaddr_, link_key);
    }

    if (send_link_key_reply) { 
        sendHCILinkKeyRequestReply(link_key);
        pending_control_ = 0; // NOT sure what this should be?

    } else {
        sendHCILinkKeyNegativeReply();
        pending_control_ = PC_LINK_KEY_NEGATIVE;
    }

    #else    
    sendHCILinkKeyNegativeReply();
    pending_control_ = PC_LINK_KEY_NEGATIVE;
    #endif
}

void BluetoothController::handle_hci_link_key_notification() {
    // 0   1  2  3  4 5  6  7  8  9 10  1  2  3  4  5  6  7  8  9 20  1  2  3 4
    // 18 17 79 22 23 a c5 cc 5e 98 d4 5e bb 15 66 da 67 fe 4f 87 2b 61 46 b4 0
    DBGPrintf("    Event: Link Key Notificaton %x:%x:%x:%x:%x:%x Type:%x\n    key:",
              rxbuf_[2], rxbuf_[3], rxbuf_[4], rxbuf_[5], rxbuf_[6], rxbuf_[7], rxbuf_[24]);
    for (uint8_t i = 8; i < 24; i++) DBGPrintf("%02x ", rxbuf_[i]);
    DBGPrintf("\n");

#if 1
    // Should we always try to save away the key? 
    sendHCIWriteStoredLinkKey(&rxbuf_[8]);
    has_key = true;
    //pending_control_ ???

#else
    // Now here is where we need to decide to say we have key or tell them to
    // cancel key...  right now hard code to cancel...
	
	uint8_t hcibuf[22];
        //hcibuf[0] = 0x0B; // HCI OCF = 0B
        //hcibuf[1] = 0x01 << 2; // HCI OGF = 1
        //hcibuf[2] = 0x16; // parameter length 22
        for(uint8_t i = 0; i < 6; i++) hcibuf[i] = current_connection_->device_bdaddr_[i]; // 6 octet bdaddr
//        hcibuf[3] = disc_bdaddr[0]; // 6 octet bdaddr
        for(uint8_t i = 0; i < 16; i++) hcibuf[i + 6] = rxbuf_[i+8]; // 16 octet link_key
//        hcibuf[9] = link_key[0]; // 16 octet link_key

    if(do_pair_ssp_) {
		sendHCICommand(HCI_LINK_KEY_REQUEST_REPLY,  sizeof(hcibuf), hcibuf);   //use simple pairing
		pending_control_++;
		has_key = true;
	}
#endif    
}
void BluetoothController::handle_hci_return_link_keys()
{
    DBGPrintf("Returned Link Keys .... \n");

}


void BluetoothController::handle_hci_disconnect_complete()
{
    //5 4 0 48 0 13
    DBGPrintf("    Event: HCI Disconnect complete(%d): handle: %x, reason:%x", rxbuf_[2],
              rxbuf_[3] + (rxbuf_[4] << 8), rxbuf_[5]);
    #ifdef DEBUG_BT
    print_error_codes(rxbuf_[5]);
    #endif 

    // bail if no current connection
    if (!current_connection_) return;

    if (current_connection_->device_driver_) {
        current_connection_->device_driver_->release_bluetooth();
        current_connection_->remote_name_[0] = 0;
        current_connection_->device_driver_ = nullptr;

        // Restore to normal...
        current_connection_->control_dcid_ = 0x70;
        current_connection_->interrupt_dcid_ = 0x71;
    }
    // Probably should clear out connection data.
#if 0
    current_connection_->device_connection_handle_ = 0;
    current_connection_->device_class_ = 0;
    memset(current_connection_->device_bdaddr_, 0, sizeof(current_connection_->device_bdaddr_));
#endif
    // Now we need to remove that item from our list of connections.
    count_connections_--;
    if (count_connections_ == 0) {
        // reset the next connection counts back to initial states.
        next_dcid_ = 0x70;      // Lets try not hard coding control and interrupt dcid
    }

    // now lets simply set the back pointer to 0 to say we are not handling it
    current_connection_->btController_ = nullptr; // don't use it
    current_connection_->device_connection_handle_ = 0xffff; // make sure it does not match
    current_connection_ = nullptr;

}

void BluetoothController::handle_hci_authentication_complete()
{
    //  6 3 13 48 0
    DBGPrintf("    Event: HCI Authentication complete(%d): handle: %x\n", rxbuf_[2],
              rxbuf_[3] + (rxbuf_[4] << 8));
    // Start up lcap connection...
    if (pairing_cb_) pairing_cb_->authenticationComplete(); 
    current_connection_->connection_rxid_ = 0;
    current_connection_->sendl2cap_ConnectionRequest(current_connection_->device_connection_handle_, current_connection_->connection_rxid_, current_connection_->control_dcid_, HID_CTRL_PSM);
}



void BluetoothController::handle_hci_remote_name_complete() {
    //           STAT bd   bd   bd   bd    bd  bd
    // 0x07 0xFF 0x00 0x79 0x22 0x23 0x0A 0xC5 0xCC 0x42 0x6C 0x75 0x65 0x74 0x6F 0x6F ...
    DBGPrintf("    Event: handle_hci_remote_name_complete(%d)\n", rxbuf_[2]);
    if (rxbuf_[2] == 0) {
        DBGPrintf("    Remote Name: ");
        for (uint8_t *psz = &rxbuf_[9]; *psz; psz++) DBGPrintf("%c", *psz);
        DBGPrintf("\n");
    }

    current_connection_->remoteNameComplete((rxbuf_[2] == 0)? &rxbuf_[9] : nullptr);

    if (current_connection_->inquire_mode_) {
        // we are in some form of inquire pairing mode
        if (current_connection_->device_driver_  || current_connection_->check_for_hid_descriptor_) {
            // Now we need to bail from inquiry and setup to try to connect...
            sendHCICreateConnection();
            pending_control_ = 0;
            //sendHCIInquiryCancel();
            //            
            //pending_control_ = PC_INQUIRE_CANCEL;
        }

    } else {
        // This was from an incomming connect.
        sendHCIAcceptConnectionRequest();
    }

#if 0
    // not sure yet where this was used in newer stuff...

    if (current_connection_->supports_SSP_ == false) {
        if (current_connection_->device_driver_) {
            if (!current_connection_->device_driver_->remoteNameComplete(&rxbuf_[9])) {
                current_connection_->device_driver_->release_bluetooth();
                current_connection_->device_driver_ = nullptr;
            }
        }
        if (!current_connection_->device_driver_) {
            current_connection_->device_driver_ = current_connection_->find_driver( &rxbuf_[9], 0);
            // not sure I should call remote name again, but they already process...
            if (current_connection_->device_driver_) {
                current_connection_->device_driver_->remoteNameComplete(&rxbuf_[9]);
            }
        }
        if (current_connection_->device_driver_) {
            // lets save away the string.
            uint8_t buffer_index;
            for (buffer_index = 0; buffer_index < BTHIDInput::REMOTE_NAME_SIZE - 1; buffer_index++) {
                current_connection_->device_driver_->remote_name_[buffer_index] = rxbuf_[9 + buffer_index];
                if (!current_connection_->device_driver_->remote_name_[buffer_index]) break;
            }
            current_connection_->device_driver_->remote_name_[buffer_index] = 0;    // make sure null terminated

            if (current_connection_->device_driver_->special_process_required & BTHIDInput::SP_PS3_IDS) {
                // Real hack see if PS3...
                current_connection_->control_dcid_ = 0x40;
                current_connection_->interrupt_dcid_ = 0x41;
            } else {
                current_connection_->control_dcid_ = next_dcid_++;
                current_connection_->interrupt_dcid_ = next_dcid_++;
            }
        }

        // If we are in the connection complete mode, then this is a pairing state and needed to call
        // get remote name later.
        if (current_connection_->connection_complete_) {
            if (current_connection_->device_driver_) {  // We have a driver call their
                current_connection_->device_driver_->connectionComplete();
                current_connection_->connection_complete_ = false;  // only call once
            }
        } else {
            sendHCIAcceptConnectionRequest();
        }
    } else {
        sendHCIAuthenticationRequested();
    }
#endif    
}

void BluetoothController::handle_hci_remote_version_information_complete() {
    //           STAT bd   bd   bd   bd    bd  bd
    //c 8 0 48 0 5 45 0 0 0
    current_connection_->remote_ver_ = rxbuf_[6];
    current_connection_->remote_man_ = rxbuf_[7] + ((uint16_t)rxbuf_[8] << 8);
    current_connection_->remote_subv_ = rxbuf_[9];

    DBGPrintf("    Event: handle_hci_remote_version_information_complete(%d): ", rxbuf_[2]);
    DBGPrintf(" Handle: %x, Ver:%x, Man: %x, SV: %x\n",
              rxbuf_[3] + ((uint16_t)rxbuf_[4] << 8), current_connection_->remote_ver_, current_connection_->remote_man_, current_connection_->remote_subv_);
    // Lets now try to accept the connection.
    sendHCIAcceptConnectionRequest();
}

void BluetoothController::rx2_data(const Transfer_t *transfer)
{
    uint32_t len = transfer->length - ((transfer->qtd.token >> 16) & 0x7FFF);
    #ifdef DEBUG_BT
    if (rx2_continue_packet_expected_) DBGPrintf("<<P(02 %u %u):", (uint32_t)em_rx_tx2, len);
    else if (rx2_packet_data_remaining_) DBGPrintf("<<C(02 %u %u):", (uint32_t)em_rx_tx2, len);
    else DBGPrintf("\n=====================\n<<(02 %u %u):", (uint32_t)em_rx_tx2, len);
    em_rx_tx2 = 0;
    uint8_t *buffer = (uint8_t*)transfer->buffer;
    for (uint8_t i = 0; i < len; i++) DBGPrintf("%02X ", buffer[i]);
    DBGPrintf("\n");
    #endif

    // Note the logical packets returned from the device may be larger
    // than can fit in one of our packets, so we will detect this and
    // the next read will be continue in or rx_buf_ in the next logical 
    // location.  We will only go into process the next logical state
    // when we have the full response read in... 

    // Note two types of continue data. One where the device
    // gives us a full return packet, but that packet won't fit into 
    // one of our own packets. (PS4 response for example)


    uint16_t hci_length = rx2buf_[2] + ((uint16_t)rx2buf_[3] << 8);
    uint16_t l2cap_length = rx2buf_[4] + ((uint16_t)rx2buf_[5] << 8);


    // call backs.  See if this is an L2CAP reply. example
    //  HCI       | l2cap
    //48 20 10 00 | 0c 00 10 00 | 3 0 8 0 44 0 70 0 0 0 0 0
    // BUGBUG need to do more verification, like the handle
    // Note: we also need to handle cases where are message is split up into
    // multiple.  Example: we request attribute search first line
    // next couple of lines shows two messages coming back
    // followed by the message received on Linux
    //>>(02):48 20 18 00 14 00 41 00 06 00 01 00 0F 35 03 19 01 00 FF FF 35 05 0A 00 00 FF FF 00

    //        0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 ...
    //<<(02):48 20 1B 00 30 00 40 00 07 00 01 00 2B 00 26 36 03 AD 36 00 8E 09 00 00 0A 00 00 00 00 09 00
    //<<(02):48 10 19 00 01 35 03 19 10 00 09 00 04 35 0D 35 06 19 01 00 09 00 01 35 03 19 02 00 26

    //linux :47 20 34 00 30 00 40 00 07 00 00 00 2b 00 26 36 03 ad 36 00 8e 09 00 00 0a 00 00 00 00 09 00 $$
    //...                01 35 03 19 10 00 09 00 04 35 0d 35 06 19 01 00 09 00 01 35 03 19 02 00 26
    // first attempt... combine by memcpy and update count...
    
    if (rx2_continue_packet_expected_) {

        // Combine...
        if ((rx2buf2_[1] & 0x10) == 0) DBGPrintf("Expected continue in Packet Boundary (PB flag)");
        uint16_t hci_copy_length = rx2buf2_[2] + ((uint16_t)rx2buf2_[3] << 8);
        memcpy(&rx2buf_[hci_length + 4], &rx2buf2_[4], hci_copy_length);
        rx2buf_[2] += rx2buf2_[2]; // update the size here...
        hci_length = rx2buf_[2] + ((uint16_t)rx2buf_[3] << 8);

        rx2_packet_data_remaining_ = 0; // I am asuming only two for all of this
        DBGPrintf("<<(2 comb):");
        for (uint8_t i = 0; i < (hci_length + 4); i++) DBGPrintf("%02X ", rx2buf_[i]);
        DBGPrintf("\n");
        rx2_packet_data_remaining_ = 0;
        rx2_continue_packet_expected_ = 0; // don't expect another one. 
    } else {
        if (rx2_packet_data_remaining_ == 0) {    // Previous command was fully handled
            rx2_packet_data_remaining_ = hci_length + 4;   // length plus size of hci header
        }       
        // Now see if the data 
        rx2_packet_data_remaining_ -= len;    // remove the length of this packet from length
    }

    //
    //  uint16_t rsp_packet_length = rx2buf_[10] + ((uint16_t)rx2buf_[11]<<8);
    if (rx2_packet_data_remaining_ == 0) {    // read started at beginning of packet so get the total length of packet
        if ((hci_length == (l2cap_length + 4)) /*&& (hci_length == (rsp_packet_length+8))*/) {
            // All the lengths appear to be correct...  need to do more...
            // See if we should set the current_connection...

            if (!current_connection_ || (current_connection_->device_connection_handle_ != rx2buf_[0])) {
                BluetoothConnection  *previous_connection = current_connection_;    // need to figure out when this changes and/or...
                current_connection_ = BluetoothConnection::s_first_;
                while (current_connection_) {
                    if (current_connection_->device_connection_handle_ == rx2buf_[0]) break;
                    current_connection_ = current_connection_->next_;
                }
                if (current_connection_ == nullptr) {
                    current_connection_ = previous_connection;
                }
            }

            // Let the connection processes the message:
            if (current_connection_) current_connection_->rx2_data(rx2buf_);
            else DBGPrintf("??? There are no device Connections ignore packet Handle: %x\n", rx2buf_[0]);

            // Queue up for next read...
            queue_Data_Transfer_Debug(rx2pipe_, rx2buf_, rx2_size_, this, __LINE__);
        } else {
            // size issue?
            rx2_packet_data_remaining_ = (l2cap_length + 4) - hci_length;
            rx2_continue_packet_expected_ = 1;
            //DBGPrintf("?? expect continue packet ?? len:%u, hci_len=%u l2cap_len=%u expect=%u\n", len, hci_length, l2cap_length, rx2_packet_data_remaining_);
            // Queue up for next read secondary buffer.
            queue_Data_Transfer_Debug(rx2pipe_, rx2buf2_, rx2_size_, this, __LINE__);
        }
    } else {
        // Need to retrieve the last few bytes of data.
        //
        //DBGPrintf("?? RX2_Read continue on 2nd packet ?? len:%u, hci_len=%u l2cap_len=%u expect=%u\n", len, hci_length, l2cap_length, rx2_packet_data_remaining_);
        //queue_Data_Transfer_Debug(rx2pipe_, rx2buf2_, rx2_size_, this, __LINE__);
        queue_Data_Transfer_Debug(rx2pipe_, (uint8_t*)transfer->buffer + rx2_size_, rx2_size_, this, __LINE__);
        rx2_continue_packet_expected_ = 0;
        return;     // Don't process the message yet as we still have data to receive. 
    }


}


void BluetoothController::sendHCICommand(uint16_t hciCommand, uint16_t cParams, const uint8_t* data)
{
    txbuf_[0] = hciCommand & 0xff;
    txbuf_[1] = (hciCommand >> 8) & 0xff;
    txbuf_[2] = cParams;
    if (cParams) {
        memcpy(&txbuf_[3], data, cParams);  // copy in the commands parameters.
    }
    uint8_t nbytes = cParams + 3;
    DBGPrintf(">>(00, %u):", (uint32_t)em_rx_tx);
    em_rx_tx = 0;
    for (uint8_t i = 0; i < nbytes; i++) DBGPrintf("%02X ", txbuf_[i]);
    DBGPrintf("\n");
    mk_setup(setup, 0x20, 0x0, 0, 0, nbytes);
    if (device == nullptr) {
        // something wrong:
        USBHDBGSerial.printf("\n !!!!!!!!!!! BluetoothController::sendHCICommand called with device == nullptr\n");
        return; // don't send it. 
    }
    queue_Control_Transfer(device, &setup, txbuf_, this);
}

//---------------------------------------------
void  BluetoothController::sendHCIHCIWriteInquiryMode(uint8_t inquiry_mode) {
    // Setup Inquiry mode
    if (pairing_cb_) {
        if (!pairing_cb_->writeInquiryMode(inquiry_mode)) {
            DBGPrintf("WRITE_INQUIRE_MODE aborted\n");
            return ; //false;
        }
    }

    DBGPrintf("HCI_WRITE_INQUIRY_MODE\n");
    sendHCICommand(HCI_WRITE_INQUIRY_MODE, 1, &inquiry_mode);
}

void BluetoothController::sendHCIReadLocalSupportedCommands() {
    DBGPrintf("HCI_Read_Local_Supported_Commands\n");
    sendHCICommand(HCI_Read_Local_Supported_Commands, 0, nullptr);

}

void BluetoothController::sendHCIReadLocalSupportedFeatures() {
    DBGPrintf("HCI_Read_Local_Supported_Features\n");
    sendHCICommand(HCI_Read_Local_Supported_Features, 0, nullptr);

}

void BluetoothController::sendHCISetEventMask() {
    // Setup Inquiry mode
    DBGPrintf("HCI_Set_Event_Mask\n");
	if(do_pair_ssp_) {
	    static const uint8_t hci_event_mask_data_ssp[8] = {
			// Default: 0x0000 1FFF FFFF FFFF
            //0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x1F, 0xFF, 0x00
            // turned off max slot changing event
            0xFF, 0xFF, 0xFF, 0xFB, 0xFF, 0x1F, 0xFF, 0x00
		};  // default plus extended inquiry mode
		sendHCICommand(HCI_Set_Event_Mask, sizeof(hci_event_mask_data_ssp), hci_event_mask_data_ssp);
	} else {
		static const uint8_t hci_event_mask_data[8] = {
			// Default: 0x0000 1FFF FFFF FFFF
			//0xff, 0xff, 0xff, 0xff, 0xff, 0x5f, 0x00, 0x00
            // 0    8    16    24    32    40    48   54
            //0xff, 0xff, 0xff, 0xfb, 0xff, 0x1f, 0x00, 0x00
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3f
		};  // default plus extended inquiry mode
		sendHCICommand(HCI_Set_Event_Mask, sizeof(hci_event_mask_data), hci_event_mask_data);
	}
}

//---------------------------------------------
void BluetoothController::sendHCI_INQUIRY() {
    // Start unlimited inqury, set timeout to max and
    DBGPrintf("HCI_INQUIRY\n");
    static const uint8_t hci_inquiry_data[ ] = {
        0x33, 0x8B, 0x9E,   // Bluetooth assigned number LAP 0x9E8B33 General/unlimited inquiry Access mode
        0x30, 0xa
    };          // Max inquiry time little over minute and up to 10 responses
    sendHCICommand(HCI_INQUIRY, sizeof(hci_inquiry_data), hci_inquiry_data);
}

//---------------------------------------------
void BluetoothController::sendHCIInquiryCancel() {
    DBGPrintf("HCI_INQUIRY_CANCEL\n");
    sendHCICommand(HCI_INQUIRY_CANCEL, 0, nullptr);
}

//---------------------------------------------
void BluetoothController::sendHCICreateConnection() {
    DBGPrintf("HCI_CREATE_CONNECTION\n");
    uint8_t connection_data[13];
    //  0    1    2    3    4    5    6    7    8   9    10   11   12
    // BD   BD   BD   BD   BD   BD   PT   PT  PRS   0    CS   CS   ARS
    //0x79 0x22 0x23 0x0A 0xC5 0xCC 0x18 0xCC 0x01 0x00 0x00 0x00 0x00
    //0x05 0x04 0x0D 0x79 0x22 0x23 0x0A 0xC5 0xCC 0x18 0xCC 0x01 0x00 0x00 0x00 0x00
    //  05   04   0d   40   25   00   c4   01   00   18   cc   01   00   00 00     00

    for (uint8_t i = 0; i < 6; i++) connection_data[i] = current_connection_->device_bdaddr_[i];
    connection_data[6] = 0x18; //DM1/DH1
    connection_data[7] = 0xcc; //
    connection_data[8] = current_connection_->device_ps_repetion_mode_;  // from device
    connection_data[9] = 0; //
    connection_data[10] = 0;  // clock offset
    connection_data[11] = 0;  // clock offset
    connection_data[12] = 0;  // allow role swith no
    sendHCICommand(HCI_CREATE_CONNECTION, sizeof(connection_data), connection_data);
}

//---------------------------------------------
void BluetoothController::sendHCIAcceptConnectionRequest() {
    DBGPrintf("HCI_OP_ACCEPT_CONN_REQ\n");
    uint8_t connection_data[7];

    //  0    1    2    3    4    5    6    7    8   9    10   11   12
    //  BD   BD   BD   BD   BD   BD  role
    //0x79 0x22 0x23 0x0A 0xC5 0xCC 0x00
    for (uint8_t i = 0; i < 6; i++) connection_data[i] = current_connection_->device_bdaddr_[i];
    connection_data[6] = 0; // Role as master
    sendHCICommand(HCI_OP_ACCEPT_CONN_REQ, sizeof(connection_data), connection_data);
}

void BluetoothController::sendHCIRejectConnectionRequest(uint8_t bdaddr[6], uint8_t error) {
    DBGPrintf("HCI_OP_REJECT_CONN_REQ\n");
    uint8_t connection_data[7];

    //  0    1    2    3    4    5    6    7    8   9    10   11   12
    //  BD   BD   BD   BD   BD   BD  role
    //0x79 0x22 0x23 0x0A 0xC5 0xCC 0x00
    for (uint8_t i = 0; i < 6; i++) connection_data[i] = bdaddr[i];
    connection_data[6] = error; // Role as master
    sendHCICommand(HCI_OP_REJECT_CONN_REQ, sizeof(connection_data), connection_data);
}

//---------------------------------------------
void BluetoothController::sendHCIAuthenticationRequested() {
    DBGPrintf("HCI_AUTH_REQUESTED\n");
    uint8_t connection_data[2];
    connection_data[0] = current_connection_->device_connection_handle_ & 0xff;
    connection_data[1] = (current_connection_->device_connection_handle_ >> 8) & 0xff;
    sendHCICommand(HCI_AUTH_REQUESTED, sizeof(connection_data), connection_data);
}

//---------------------------------------------
void BluetoothController::sendHCILinkKeyRequestReply(uint8_t link_key[16]) {
    DBGPrintf("HCI_LINK_KEY_REQUEST_REPLY: bdaddr: %02x:%02x:%02x:%02x:%02x:%02x key: ",
        current_connection_->device_bdaddr_[0],current_connection_->device_bdaddr_[1],current_connection_->device_bdaddr_[2],
        current_connection_->device_bdaddr_[3],current_connection_->device_bdaddr_[4],current_connection_->device_bdaddr_[5]);
    for (uint8_t i = 0; i < 16; i++) DBGPrintf(" %02X", link_key[i]);
    DBGPrintf("\n");
    uint8_t connection_data[22];
    for (uint8_t i = 0; i < 6; i++) connection_data[i] = current_connection_->device_bdaddr_[i];
    for(uint8_t i = 0; i < 16; i++) connection_data[i + 6] = link_key[i]; // 16 octet link_key
    sendHCICommand(HCI_LINK_KEY_REQUEST_REPLY, sizeof(connection_data), connection_data);
}

//---------------------------------------------
void BluetoothController::sendHCILinkKeyNegativeReply() {
    DBGPrintf("HCI_LINK_KEY_NEG_REPLY\n");
    uint8_t connection_data[6];
    for (uint8_t i = 0; i < 6; i++) connection_data[i] = current_connection_->device_bdaddr_[i];
    sendHCICommand(HCI_LINK_KEY_NEG_REPLY, sizeof(connection_data), connection_data);
}

//---------------------------------------------
bool BluetoothController::sendHCIReadStoredLinkKey(uint8_t link_key[16]) {
#if 0
    uint8_t link_key_data[7];
    for (uint8_t i = 0; i < 6; i++) link_key_data[i] = current_connection_->device_bdaddr_[i];
    link_key_data[6] = 0; // return the BDADDR key only
    sendHCICommand(HCI_READ_STORED_LINK_KEY, sizeof(link_key_data), link_key_data);
#endif    
    return true;
}

//---------------------------------------------
// Save away the pairing keys storage information.
//---------------------------------------------
void BluetoothController::setPairingKeyStorageLocation(FS *pfs, int eeprom_index, int max_keys) {
    pairing_keys_fs_ = pfs;
    pairing_keys_eeprom_start_index_ = eeprom_index;
    pairing_keys_max_ = max_keys;
}

//---------------------------------------------
// Write the link key - 
//---------------------------------------------
static const char s_PairingFileName[] = "PairingKeys.dat";

bool BluetoothController::writeLinkKey(uint8_t bdaddr[6], uint8_t link_key[16]) {
    DBGPrintf("writeLinkKey %p %p\n", bdaddr, link_key);
    if (pairing_cb_) {
        if (pairing_cb_->writeLinkKey(bdaddr, link_key)) {
            // either the caller handled it or does not want it handled
            return true;
        }
    }
    // We will store the data either in FS or EEPROM
    if (pairing_keys_fs_) {
        if (bdaddr == nullptr) {
            pairing_keys_fs_->remove(s_PairingFileName);
            return true;;
        }
        File pairingFile;
        pairingFile = pairing_keys_fs_->open(s_PairingFileName, FILE_WRITE_BEGIN);
        if (pairingFile) {
            // Lets see if we can find this item already in the file if not
            // append it. 
            uint8_t bdaddr_link_key_item[22];
            for (int key_index = 0; ;key_index++) {
                if (pairingFile.read(bdaddr_link_key_item, sizeof(bdaddr_link_key_item)) != sizeof(bdaddr_link_key_item)) {
                    // Did not find key... should check count, but...
                    if (link_key == nullptr) {
                        DBGPrintf("\tkey to delete, not found");
                        return false;
                    }
                    DBGPrintf("\tSaved in slot: %u\n", key_index);

                    pairingFile.seek(key_index * sizeof(bdaddr_link_key_item), SEEK_SET);  // make sure right place 
                    pairingFile.write(bdaddr, 6);
                    pairingFile.write(link_key, 16);
                    break;
                }
                if (memcmp(&bdaddr_link_key_item[0], bdaddr, 6) == 0) {
                    // matched should we see if it changed before 
                    if (link_key == nullptr) {
                        // Asked to delete this key... Should we simply clear it out?
                        // or should we compress it out?  For now, I am going to simply wipe it out. 
                        DBGPrintf("\tremoved key in slot: %u\n", key_index);
                        pairingFile.seek(key_index * sizeof(bdaddr_link_key_item), SEEK_SET);  // make sure right place 
                        memset(bdaddr_link_key_item, 0xff, sizeof(bdaddr_link_key_item)); 
                        pairingFile.write(bdaddr_link_key_item, sizeof(bdaddr_link_key_item));
                    } else if (memcmp(&bdaddr_link_key_item[6], link_key, 16) != 0) {
                        DBGPrintf("\tupdated in slot: %u\n", key_index);
                        // Key changed
                        pairingFile.seek(key_index * sizeof(bdaddr_link_key_item) + 6, SEEK_SET);  // make sure right place 
                        pairingFile.write(link_key, 16);
                    }
                    break;
                }
            }
            pairingFile.close();
        }
    } else {
        if (pairing_keys_max_ < 0) pairing_keys_max_ = 5;
        if (pairing_keys_eeprom_start_index_ < 0) {
             pairing_keys_eeprom_start_index_ = EEPROM.length() - (pairing_keys_max_ * 22 - pairing_keys_eeprom_start_index_);
        } 
        DBGPrintf("\tEEPROM start addr: %u, max_keys: %u\n", pairing_keys_eeprom_start_index_, pairing_keys_max_);

        if (bdaddr == nullptr) {
            // passed in nullptr here.. we will assume that the user is asking us to erase the eeprom area of the link keys
            uint32_t end_index = pairing_keys_eeprom_start_index_ + (pairing_keys_max_ * 22);
            for (uint32_t i = pairing_keys_eeprom_start_index_; i < end_index; i++) {
                EEPROM.write(i, 0xff);
            }
            DBGPrintf("\t*** Link Keys Erased ***");
            return true;;
        }

        for (uint8_t i = 0; i < pairing_keys_max_; i++) {
            uint16_t addr = pairing_keys_eeprom_start_index_ + (i * 22); // bddr plus link_key
            bool key_matched = true;
            bool key_empty = true;
            for (uint8_t j=0; j < 6; j++) {
                uint8_t val = EEPROM.read(addr++);
                if (val != bdaddr[j]) {
                    key_matched = false;
                    if (val != 0xff) {
                        key_empty = false;
                        break;
                    }
                }
            }
            if (key_empty) {
                addr = pairing_keys_eeprom_start_index_ + (i * 22); // bddr plus link_key
                for (uint8_t j=0; j < 6; j++) EEPROM.write(addr++, bdaddr[j]);
            }
            if (key_empty || key_matched) {
                DBGPrintf("\tSaved in slot: %u\n", i);
                for (uint8_t j=0; j < 16; j++) EEPROM.write(addr++, link_key[j]);
                break;
            }
        }
    }
    return true;
}

bool BluetoothController::readLinkKey(uint8_t bdaddr[6], uint8_t link_key[16]) {
    // Now here is where we need to decide to say we have key or tell them to
    // cancel key...  right now hard code to cancel...
    // see if we can read in stored key...
    // two ways out of this 
    // 1 we will receive an event with a kay
    // 2 we won't receive event but the command complete will show 0 returned.
    DBGPrintf("readLinkKey(%p, %p) do_ssp:%x cb:%p\n", bdaddr, link_key, do_pair_ssp_, pairing_cb_);
    if (do_pair_ssp_) return false;

    bool send_link_key_reply = false;
    if (pairing_cb_) {
        int ret = pairing_cb_->readLinkKey(bdaddr, link_key);
        if (ret > 0) return true;
        else if (ret < 0) return false;
    }

    // else use one of our built in versions
    // We will store the data either in FS or EEPROM

    // First see of we are using FS version
    if (pairing_keys_fs_) {
        File pairingFile;
        pairingFile = pairing_keys_fs_->open(s_PairingFileName, FILE_READ);
        if (!pairingFile) return false;

        // Lets see if we can find this item already in the file if not
        // append it. 
        uint8_t bdaddr_link_key_item[6];
        for (int key_index = 0; ;key_index++) {
            pairingFile.seek(key_index * sizeof(bdaddr_link_key_item), SEEK_SET);  // make sure right place 
            if (pairingFile.read(bdaddr_link_key_item, sizeof(bdaddr_link_key_item)) != sizeof(bdaddr_link_key_item)) {
                break; // did not find it
            }
            if (memcmp(&bdaddr_link_key_item[0], bdaddr, 6) == 0) {
                if (pairingFile.read(link_key, 16) == 16) {
                    DBGPrintf("\tFound in File, index:%u\n", key_index);
                    send_link_key_reply = true;
                }
                break;
            }
        }
        pairingFile.close();
    } else {
        if (pairing_keys_max_ < 0) pairing_keys_max_ = 5;
        if (pairing_keys_eeprom_start_index_ < 0) {
             pairing_keys_eeprom_start_index_ = EEPROM.length() - (pairing_keys_max_ * 22 - pairing_keys_eeprom_start_index_);
        } 
        DBGPrintf("\tEEPROM start addr: %u, max_keys: %u\n", pairing_keys_eeprom_start_index_, pairing_keys_max_);
        for (uint8_t i = 0; i < pairing_keys_max_; i++) {
            uint16_t addr = pairing_keys_eeprom_start_index_ + (i * 22); // bddr plus link_key
            send_link_key_reply = true;
            for (uint8_t j=0; j < 6; j++) {
                uint8_t val = EEPROM.read(addr++);
                if (val != bdaddr[j]) {
                    send_link_key_reply = false;
                    break;
                }
            }
            if (send_link_key_reply) {
                DBGPrintf("\tFound in slot: %u\n", i);
                for (uint8_t j=0; j < 16; j++) link_key[j] = EEPROM.read(addr++);
                break;
            }
        }
    }
    return send_link_key_reply;
}

//---------------------------------------------
void BluetoothController::sendHCIWriteStoredLinkKey(uint8_t link_key[16]) {

    DBGPrintf("sendHCIWriteStoredLinkKey: bdaddr: %02x:%02x:%02x:%02x:%02x:%02x key: ",
        current_connection_->device_bdaddr_[0],current_connection_->device_bdaddr_[1],current_connection_->device_bdaddr_[2],
        current_connection_->device_bdaddr_[3],current_connection_->device_bdaddr_[4],current_connection_->device_bdaddr_[5]);
    for (uint8_t i = 0; i < 16; i++) DBGPrintf(" %02X", link_key[i]);
    DBGPrintf("\n");
#if 1 
    writeLinkKey(current_connection_->device_bdaddr_, link_key); // could probably have simply renamed
#else 
    // You can not query the data back so not sure how much it helps  
    uint8_t link_key_data[23];
    link_key_data[0] = 1; // only store 1 key
    for (uint8_t i = 0; i < 6; i++) link_key_data[i+1] = current_connection_->device_bdaddr_[i];
    for(uint8_t i = 0; i < 16; i++) link_key_data[i + 7] = link_key[i]; // 16 octet link_key
    sendHCICommand(HCI_WRITE_STORED_LINK_KEY, sizeof(link_key_data), link_key_data);
#endif
}



//---------------------------------------------
// BUGBUG:: hard code string for this pass.

void BluetoothController::sendHCIPinCodeReply() {
    // 0x0D 0x04 0x17 0x79 0x22 0x23 0x0A 0xC5 0xCC 0x04 0x30 0x30 0x30 0x30 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00
    DBGPrintf("HCI_PIN_CODE_REPLY\n");
    uint8_t connection_data[23];
    uint8_t i;
    // maybe allow editing?
    if (pairing_cb_) pairing_cb_->sendPinCode(pair_pincode_);

    for (i = 0; i < 6; i++) connection_data[i] = current_connection_->device_bdaddr_[i];

    for (i = 0; pair_pincode_[i] != 0; i++) connection_data[7 + i] = pair_pincode_[i];
    connection_data[6] = i; // remember the length
    for (uint8_t i = 7 + connection_data[6]; i < 23; i++) connection_data[i] = 0;
    sendHCICommand(HCI_PIN_CODE_REPLY, sizeof(connection_data), connection_data);
}

//---------------------------------------------
void BluetoothController::sendResetHCI() {
    DBGPrintf("HCI_RESET\n");
    sendHCICommand(HCI_RESET, 0, nullptr);
}

void BluetoothController::sendHDCWriteClassOfDev() {
    // 0x24 0x0C 0x03 0x04 0x08 0x00
    const static uint8_t device_class_data[] = {BT_CLASS_DEVICE & 0xff, (BT_CLASS_DEVICE >> 8) & 0xff, (BT_CLASS_DEVICE >> 16) & 0xff};
    DBGPrintf("HCI_WRITE_CLASS_OF_DEV\n");
    sendHCICommand(HCI_WRITE_CLASS_OF_DEV, sizeof(device_class_data), device_class_data);
}



void BluetoothController::sendHCIReadBDAddr() {
    DBGPrintf("HCI_Read_BD_ADDR\n");
    sendHCICommand(HCI_Read_BD_ADDR, 0, nullptr);
}

void BluetoothController::sendHCIReadLocalVersionInfo() {
    DBGPrintf("HCI_Read_Local_Version_Information\n");
    sendHCICommand(HCI_Read_Local_Version_Information, 0, nullptr);
}


void BluetoothController::sendHCIWriteScanEnable(uint8_t scan_op)   {//             0x0c1a
// 0x1A 0x0C 0x01 0x02
    DBGPrintf("HCI_WRITE_SCAN_ENABLE\n");
    sendHCICommand(HCI_WRITE_SCAN_ENABLE, 1, &scan_op);
}

void BluetoothController::sendHCIRemoteNameRequest() {      // 0x0419
    //               BD   BD   BD   BD   BD   BD   PS   0    CLK   CLK
    //0x19 0x04 0x0A 0x79 0x22 0x23 0x0A 0xC5 0xCC 0x01 0x00 0x00 0x00

    DBGPrintf("HCI_OP_REMOTE_NAME_REQ\n");
    uint8_t connection_data[10];
    for (uint8_t i = 0; i < 6; i++) connection_data[i] = current_connection_->device_bdaddr_[i];
    connection_data[6] = current_connection_->device_ps_repetion_mode_; // page scan repeat mode...
    connection_data[7] = 0;  // 0
    connection_data[8] = current_connection_->device_clock_offset_[0]; // Clk offset
    connection_data[9] = current_connection_->device_clock_offset_[1];
    sendHCICommand(HCI_OP_REMOTE_NAME_REQ, sizeof(connection_data), connection_data);
}

void BluetoothController::sendHCIRemoteVersionInfoRequest() {       // 0x041D
    DBGPrintf("HCI_OP_READ_REMOTE_VERSION_INFORMATION\n");
    uint8_t connection_data[2];
    connection_data[0] = current_connection_->device_connection_handle_ & 0xff;
    connection_data[1] = (current_connection_->device_connection_handle_ >> 8) & 0xff;
    sendHCICommand(HCI_OP_READ_REMOTE_VERSION_INFORMATION, sizeof(connection_data), connection_data);
}

void BluetoothController::sendHCIRoleDiscoveryRequest() {
    DBGPrintf("HCI_OP_ROLE_DISCOVERY\n");
    uint8_t connection_data[2];
    connection_data[0] = current_connection_->device_connection_handle_ & 0xff;
    connection_data[1] = (current_connection_->device_connection_handle_ >> 8) & 0xff;
    sendHCICommand(HCI_OP_ROLE_DISCOVERY, sizeof(connection_data), connection_data);
}

//*******************************************************************
//*******************************************************************
// SSP Stuff
//*******************************************************************

void BluetoothController::sendHCISimplePairingMode() {
	uint8_t hcibuf[1];
        hcibuf[0] = 0x01; // enable = 1
		DBGPrintf("HCI_Set Simple Pairing Mode\n");
		sendHCICommand(HCI_WRITE_SIMPLE_PAIRING_MODE, sizeof(hcibuf), hcibuf);
}

void BluetoothController::sendHCIReadSimplePairingMode() {
	static const uint8_t hcibuf[2] = {0, 0} ; // could probably toss and pass nullptr
	DBGPrintf("HCI_Read Simple Pairing Mode\n");
	sendHCICommand(HCI_READ_SIMPLE_PAIRING_MODE, 0, hcibuf);
}

void BluetoothController::sendHCISetConnectionEncryption() {
	uint8_t hcibuf[3];
    hcibuf[0] = current_connection_->device_connection_handle_ & 0xff; //Connection_Handle - low byte
    hcibuf[1] = (current_connection_->device_connection_handle_ >> 8) & 0xff; //Connection_Handle - high byte
    hcibuf[2] = 0x01; // 0x00=OFF 0x01=ON 
    
    sendHCICommand(HCI_SET_CONN_ENCRYPTION, sizeof(hcibuf), hcibuf);
}


void BluetoothController::handle_hci_encryption_change_complete() {
    DBGPrintf("    Event: Encryption Change status:%x handle:%x mode:%x\n",
              rxbuf_[2], rxbuf_[3] + (rxbuf_[4] << 8), rxbuf_[5]);
	DBGPrintf("\nRead Encryption Key Size!\n");
	uint8_t hcibuf[2];
	hcibuf[0] = rxbuf_[3];
	hcibuf[1] = rxbuf_[4];
	sendHCICommand(HCI_READ_ENCRYPTION_KEY_SIZE, sizeof(hcibuf), hcibuf);
}

void inline BluetoothController::sendHCIReadRemoteSupportedFeatures() {
    uint8_t connection_data[2];
    connection_data[0] = current_connection_->device_connection_handle_ & 0xff;
    connection_data[1] = (current_connection_->device_connection_handle_ >> 8) & 0xff;
    sendHCICommand(HCI_OP_READ_REMOTE_FEATURES, sizeof(connection_data), connection_data);
}


void inline BluetoothController::sendHCIReadRemoteExtendedFeatures() {
    uint8_t connection_data[3];
    connection_data[0] = current_connection_->device_connection_handle_ & 0xff;
    connection_data[1] = (current_connection_->device_connection_handle_ >> 8) & 0xff;
    connection_data[2] = 1;
    sendHCICommand(HCI_OP_READ_REMOTE_EXTENDED_FEATURE, sizeof(connection_data), connection_data);
}

void BluetoothController::sendInfoRequest() {
    // Should verify protocol is boot or report
   uint8_t l2capbuf[10];
	
	//l2capbuf[0] = 0x00;
	//l2capbuf[1] = 0x00;
	
	//l2capbuf[0] = 0x0a;
	//l2capbuf[1] = 0x00;
	
    l2capbuf[0] = 0x06;
	l2capbuf[1] = 0x00;
	l2capbuf[2] = 0x01;
	l2capbuf[3] = 0x00;
			
    l2capbuf[4] = 0x0a;
	l2capbuf[5] = 0x01;
	l2capbuf[6] = 0x02;
	l2capbuf[7] = 0x00;
	l2capbuf[8] = 0x02;
	l2capbuf[9] = 0x00;
	
    DBGPrintf("sendInfoRequest \n");
	
	current_connection_->device_connection_handle_ = rxbuf_[3] + (uint16_t)(rxbuf_[4]>>8);
	DBGPrintf("device_connection_handle_ = %x\n", current_connection_->device_connection_handle_);
    sendL2CapCommand(current_connection_->device_connection_handle_, l2capbuf, sizeof(l2capbuf));

}

//*******************************************************************
//*******************************************************************
void BluetoothController::tx_data(const Transfer_t *transfer)
{
    // We assume the current connection should process this but lets make sure.
    uint8_t *buffer = (uint8_t*)transfer->buffer;
    #if 0
    // device connection handle is only valid for data packets not command packets.
    if (!current_connection_ || (current_connection_->device_connection_handle_ != buffer[0])) {
        BluetoothConnection  *previous_connection = current_connection_;    // need to figure out when this changes and/or...
        current_connection_ = BluetoothConnection::s_first_;
        while (current_connection_) {
            if (current_connection_->device_connection_handle_ == buffer[0]) break;
            current_connection_ = current_connection_->next_;
        }
        if (current_connection_ == nullptr) {
            current_connection_ = previous_connection; 

            DBGPrintf("Error (tx_data): did not find device_connection_handle_ use previous(%p)== %x\n", current_connection_, buffer[0]);
            return;
        }
    }
    #endif

    // Let the connection processes the message:
    if (current_connection_) current_connection_->tx_data(buffer, transfer->length);
}


//*******************************************************************
//
// HCI ACL Packets
// HCI Handle Low, HCI_Handle_High (PB, BC), Total length low, TLH  - HCI ACL Data packet
// length Low, length high, channel id low, channel id high - L2CAP header
// code, identifier, length, ... - Control-frame

/************************************************************/
/*                    L2CAP Commands                        */

// Public wrrapper function
void BluetoothController::sendL2CapCommand(uint8_t* data, uint8_t nbytes, int channel) {
    uint16_t channel_out;
    switch (channel) {
    case CONTROL_SCID:
        channel_out = current_connection_->control_scid_;
        break;
    case INTERRUPT_SCID:
        channel_out = current_connection_->interrupt_scid_;
        break;
    case SDP_SCID:
        channel_out = current_connection_->sdp_scid_;
        DBGPrintf("@@@@@@ SDP SCID:%x DCID:%x\n", current_connection_->sdp_scid_, current_connection_->sdp_dcid_);
        break;
    default:
        channel_out = (uint16_t)channel;
    }
    DBGPrintf("sendL2CapCommand: %x %d %x\n", (uint32_t)data, nbytes, channel, channel_out);
    sendL2CapCommand (current_connection_->device_connection_handle_, data, nbytes, channel_out & 0xff, (channel_out >> 8) & 0xff);
}


void BluetoothController::sendL2CapCommand(uint16_t handle, uint8_t* data, uint8_t nbytes, uint8_t channelLow, uint8_t channelHigh)
{
    txbuf_[0] = handle & 0xff; // HCI handle with PB,BC flag
    txbuf_[1] = (((handle >> 8) & 0x0f) | 0x20);
    txbuf_[2] = (uint8_t)((4 + nbytes) & 0xff); // HCI ACL total data length
    txbuf_[3] = (uint8_t)((4 + nbytes) >> 8);
    txbuf_[4] = (uint8_t)(nbytes & 0xff); // L2CAP header: Length
    txbuf_[5] = (uint8_t)(nbytes >> 8);
    txbuf_[6] = channelLow;
    txbuf_[7] = channelHigh;
    if (nbytes) {
        memcpy(&txbuf_[8], data, nbytes);   // copy in the commands parameters.
    }
    nbytes = nbytes + 8;
    DBGPrintf(">>(02 %u):", (uint32_t)em_rx_tx2);
    em_rx_tx2 = 0;
    for (uint8_t i = 0; i < nbytes; i++) DBGPrintf("%02X ", txbuf_[i]);
    DBGPrintf("\n");

    if (!queue_Data_Transfer_Debug(txpipe_, txbuf_, nbytes, this, __LINE__)) {
        println("sendL2CapCommand failed");
    }
}


void BluetoothController::useHIDProtocol(bool useHID) {
    // BUGBUG hopefully set at right time.
    current_connection_->use_hid_protocol_ = useHID;
}

// Hack to see if I can update it later
void BluetoothController::updateHIDProtocol(uint8_t protocol) {
    //pending_control_tx_ = 0; // make sure we are not processing this...
    setHIDProtocol(protocol);
}

void BluetoothController::setHIDProtocol(uint8_t protocol) {
    // Should verify protocol is boot or report
    uint8_t l2capbuf[1];
    l2capbuf[0] = 0x70 | protocol; // Set Protocol, see Bluetooth HID specs page 33
    DBGPrintf("Set HID Protocol %d (", protocol);
    sendL2CapCommand(current_connection_->device_connection_handle_, l2capbuf, sizeof(l2capbuf), current_connection_->control_scid_ & 0xff, current_connection_->control_scid_ >> 8);
}

// BLE experiments
// Experiments to enable LE scanning
bool BluetoothController::setLEScanEnable(uint8_t enable, uint8_t filter_duplicates) 
{
    DBGPrintf("HCI_LE_SET_SCAN_ENABLE(%x %x)\n", enable, filter_duplicates);
    uint8_t hci_data[2];
    hci_data[0] = enable;
    hci_data[1] = filter_duplicates; 
    sendHCICommand(HCI_LE_SET_SCAN_ENABLE, sizeof(hci_data), hci_data);
    return true;
}
bool BluetoothController::setLEScanParameters(uint8_t scan_type, uint16_t scan_interval, uint16_t scan_window, uint8_t own_address_type, uint8_t filter_policy)
{
    DBGPrintf("HCI_LE_SET_SCAN_PARAMETERS(%x %x %x %x)\n", scan_type, scan_interval, scan_window, own_address_type, filter_policy);
    uint8_t hci_data[7];
    hci_data[0] = scan_type;
    hci_data[1] = scan_interval & 0xff; 
    hci_data[2] = scan_interval >> 8; 
    hci_data[3] = scan_window & 0xff; 
    hci_data[4] = scan_window >> 8; 
    hci_data[5] = own_address_type; 
    hci_data[6] = filter_policy; 

    sendHCICommand(HCI_LE_SET_SCAN_PARAMETERS, sizeof(hci_data), hci_data);
    return true;
}

void BluetoothController::handle_ev_meta_event() { // 0x3e
//<<(01, 74):3E 2B 02 | 01 03 01 90 A4 0E 8F BA 20 1F 1E FF 06 00 01 09 20 22 1E 4D 54 BF 0B 07 41 F7 D8 91 12 A9 A0 76 00 A6 74 19 26 2B F5 82 EF BC 
    USBHDBGSerial.printf("handle_ev_meta_event SubEvent: %u", rxbuf_[2]);
    switch (rxbuf_[2]) {
    case EV_LE_Connection_Complete:
        break;
    case EV_LE_ADVERTISING_REPORT:
        {
            uint8_t cnt_reports = rxbuf_[3];
            USBHDBGSerial.printf(" Advertise Report: Num reports: %u ET:",  rxbuf_[3]);
            uint8_t index = 4;
            for (uint8_t i = 0; i < cnt_reports; i++) {
                USBHDBGSerial.printf(" %u", rxbuf_[index]);
                switch (rxbuf_[index]) {
                case 0: USBHDBGSerial.printf("(ADV_IND)"); break;
                case 1: USBHDBGSerial.printf("(ADV_DIRECT_IND)"); break; 
                case 2: USBHDBGSerial.printf("(ADV_SCAN_IND)"); break; 
                case 3: USBHDBGSerial.printf("(ADV_NONCONN_IND)"); break; 
                case 4: USBHDBGSerial.printf("(SCAN_RSP)"); break; 
                }
                index++;
            }
            USBHDBGSerial.printf(" AT:");
            for (uint8_t i = 0; i < cnt_reports; i++) {
                USBHDBGSerial.printf(" %u", rxbuf_[index]);
                switch (rxbuf_[index]) {
                case 0: USBHDBGSerial.printf("(Public Device Address)"); break;
                case 1: USBHDBGSerial.printf("(Random Device Address)"); break; 
                case 2: USBHDBGSerial.printf("(Public Identity Address)"); break; 
                case 3: USBHDBGSerial.printf("(Random static)"); break; 
                }
                index++;
            }
            for (uint8_t i = 0; i < cnt_reports; i++) {
                USBHDBGSerial.printf("%02X:%02X:%02X:%02X:%02X:%02X", rxbuf_[index+5], rxbuf_[index+4],
                    rxbuf_[index+3], rxbuf_[index+2], rxbuf_[index+1], rxbuf_[index+0]);
                index += 6;
            }
            // lets loop in the data:
            USBHDBGSerial.printf("\n");
            uint8_t data_index = index + cnt_reports;
            for (uint8_t i = 0; i < cnt_reports; i++) {
                USBHDBGSerial.printf("\t(%u):", i);
                uint8_t data_length = rxbuf_[index++];
                uint8_t ad_start = 0;
                while (ad_start < data_length) {
                    uint8_t ads_len = rxbuf_[data_index + ad_start];
                    uint8_t ads_type = rxbuf_[data_index + ad_start+1];
                    USBHDBGSerial.printf(" || len:%u type:%x data:", ads_len, ads_type);
                    if ((ads_type == 0x9) || (ads_type == 0x08)) {
                        USBHDBGSerial.printf("'");
                        USBHDBGSerial.write(&rxbuf_[data_index + ad_start+2], ads_len-1);
                        USBHDBGSerial.printf("'");
                    } else {
                        for (uint8_t j=1; j < ads_len; j++ ) USBHDBGSerial.printf(" %02X", rxbuf_[data_index + ad_start+1+j]);
                    }
                    ad_start += ads_len + 1;
                }
                USBHDBGSerial.printf("\n");
                data_index += data_length;
            }
        }
        break;
    case EV_LE_CONNECTION_UPDATE_COMPLETE:
        break;
    case EV_LE_READ_REMOTE_FEATURES_COMPLETE:
        break;
    case EV_LE_LONG_TERM_KEY_REQUEST:
        break;
    //default:    
    }
}

