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

#ifndef __BT_DEFINES_H_
#define __BT_DEFINES_H_

/************************************************************/
//  Define HCI Commands OGF HIgh byte OCF is low byte...
//     Actually shifted values...
/************************************************************/
#define HCI_INQUIRY                         0x0401
#define HCI_INQUIRY_CANCEL                  0x0402
#define HCI_CREATE_CONNECTION               0x0405
#define HCI_OP_ACCEPT_CONN_REQ              0x0409
#define HCI_OP_REJECT_CONN_REQ              0x040A
#define HCI_LINK_KEY_REQUEST_REPLY			0x040B
#define HCI_LINK_KEY_NEG_REPLY              0x040C
#define HCI_PIN_CODE_REPLY                  0x040D
#define HCI_AUTH_REQUESTED                  0x0411
#define HCI_SET_CONN_ENCRYPTION				0x0413
#define HCI_OP_REMOTE_NAME_REQ              0x0419
#define HCI_OP_REMOTE_NAME_REQ_CANCEL       0x041a
#define HCI_OP_READ_REMOTE_FEATURES         0x041b
#define HCI_OP_READ_REMOTE_EXTENDED_FEATURE 0x041c
#define HCI_OP_READ_REMOTE_VERSION_INFORMATION 0x041D
#define HCI_IO_CAPABILITY_REQUEST_REPLY		0x042B
#define HCI_USER_CONFIRMATION_REQUEST		0x042C

#define HCI_OP_ROLE_DISCOVERY               0x0809


#define HCI_Write_Default_Link_Policy_Settings  0x080f
#define HCI_Set_Event_Mask                  0x0c01
#define HCI_RESET                           0x0c03
#define HCI_SET_EVENT_FILTER                0x0c05
#define HCI_Read_Local_Name                 0x0c14
#define HCI_READ_STORED_LINK_KEY            0x0c0d
#define HCI_WRITE_STORED_LINK_KEY           0x0c11
#define HCI_DELETE_STORED_LINK_KEY          0x0c12
#define HCI_WRITE_LOCAL_NAME                0x0c13
#define Write_Connection_Accept_Timeout     0x0c16
#define HCI_WRITE_SCAN_ENABLE               0x0c1a
#define HCI_Read_Page_Scan_Activity         0x0c1b
#define HCI_READ_CLASS_OF_DEVICE            0x0c23
#define HCI_WRITE_CLASS_OF_DEV              0x0C24
#define HCI_Read_Voice_Setting              0x0c25
#define HCI_Read_Number_Of_Supported_IAC    0x0c38
#define HCI_Read_Current_IAC_LAP            0x0c39
#define HCI_WRITE_INQUIRY_MODE              0x0c45
#define HCI_Read_Page_Scan_Type             0x0c46
#define HCI_WRITE_EXTENDED_INQUIRY_RESPONSE                       0x0c52
#define HCI_READ_SIMPLE_PAIRING_MODE					0x0c55
#define HCI_WRITE_SIMPLE_PAIRING_MODE                  0x0c56
#define HCI_Read_Inquiry_Response_Transmit_Power_Level 0x0c58
#define HCI_WRITE_LE_HOST_SUPPORTED         0x0c6d

#define HCI_Read_Local_Supported_Features   0x1003
#define HCI_Read_Local_Extended_Features    0x1004
#define HCI_Read_Buffer_Size                0x1005
#define HCI_Read_BD_ADDR                    0x1009
#define HCI_Read_Local_Version_Information  0x1001
#define HCI_Read_Local_Supported_Commands   0x1002

#define HCI_READ_ENCRYPTION_KEY_SIZE		0x1408

#define HCI_LE_SET_EVENT_MASK               0x2001
#define HCI_LE_Read_Buffer_Size             0x2002
#define HCI_LE_Read_Local_supported_Features 0x2003
#define HCI_LE_READ_ADV_TX_POWER            0x2007
#define HCI_LE_SET_ADV_DATA                 0x2008
#define HCI_LE_SET_SCAN_RSP_DATA            0x2009
#define HCI_LE_SET_SCAN_PARAMETERS          0x200B
#define HCI_LE_SET_SCAN_ENABLE              0x200C
#define HCI_LE_READ_WHITE_LIST_SIZE         0x200f
#define HCI_LE_CLEAR_WHITE_LIST             0x2010
#define HCI_LE_Supported_States             0x201c

/* Bluetooth L2CAP PSM - see http://www.bluetooth.org/Technical/AssignedNumbers/logical_link.htm */
#define SDP_PSM         0x01 // Service Discovery Protocol PSM Value
#define HID_CTRL_PSM    0x11 // HID_Control PSM Value
#define HID_INTR_PSM    0x13 // HID_Interrupt PSM Value

// Used For Connection Response
#define PENDING     0x01
#define SUCCESSFUL  0x00

#define SDP_SERVICE_SEARCH_REQUEST                  0x02
#define SDP_SERVICE_SEARCH_RESPONSE                 0x03
#define SDP_SERVICE_ATTRIBUTE_REQUEST               0x04
#define SDP_SERVICE_ATTRIBUTE_RESPONSE              0x05
#define SDP_SERVICE_SEARCH_ATTRIBUTE_REQUEST        0x06 // See the RFCOMM specs
#define SDP_SERVICE_SEARCH_ATTRIBUTE_RESPONSE       0x07 // See the RFCOMM specs


/* L2CAP signaling commands */
#define L2CAP_CMD_COMMAND_REJECT        0x01
#define L2CAP_CMD_CONNECTION_REQUEST    0x02
#define L2CAP_CMD_CONNECTION_RESPONSE   0x03
#define L2CAP_CMD_CONFIG_REQUEST        0x04
#define L2CAP_CMD_CONFIG_RESPONSE       0x05
#define L2CAP_CMD_DISCONNECT_REQUEST    0x06
#define L2CAP_CMD_DISCONNECT_RESPONSE   0x07
#define L2CAP_CMD_INFORMATION_REQUEST   0x0A
#define L2CAP_CMD_INFORMATION_RESPONSE  0x0B

#define HID_THDR_DATA_INPUT             0xa1
// HID stuff
#define HID_BOOT_PROTOCOL               0x00
#define HID_RPT_PROTOCOL                0x01


/* HCI Events  */
enum {EV_INQUIRY_COMPLETE = 0x01, EV_INQUIRY_RESULT = 0x02, EV_CONNECT_COMPLETE = 0x03, EV_INCOMING_CONNECT = 0x04, EV_DISCONNECT_COMPLETE = 0x05
    , EV_AUTHENTICATION_COMPLETE = 0x06, EV_REMOTE_NAME_COMPLETE = 0x07, EV_ENCRYPTION_CHANGE = 0x08, EV_CHANGE_CONNECTION_LINK = 0x09, EV_READ_REMOTE_SUPPORTED_FEATURES_COMPLETE=0x0B,
    EV_ROLE_CHANGED = 0x12
    , EV_NUM_COMPLETE_PKT = 0x13, EV_MODE_CHANGE = 0x14, EV_PIN_CODE_REQUEST = 0x16, EV_LINK_KEY_REQUEST = 0x17, EV_LINK_KEY_NOTIFICATION = 0x18, EV_DATA_BUFFER_OVERFLOW = 0x1A
    , EV_MAX_SLOTS_CHANGE = 0x1B, EV_READ_REMOTE_VERSION_INFORMATION_COMPLETE = 0x0C, EV_QOS_SETUP_COMPLETE = 0x0D, EV_COMMAND_COMPLETE = 0x0E, EV_COMMAND_STATUS = 0x0F
    , EV_LOOPBACK_COMMAND = 0x19, EV_PAGE_SCAN_REP_MODE = 0x20, EV_INQUIRY_RESULTS_WITH_RSSI = 0x22,EV_READ_REMOTE_EXTENDED_FEATURES_COMPLETE = 0x23, EV_EXTENDED_INQUIRY_RESULT = 0x2F,
      EV_IO_CAPABILITY_REQUEST = 0x31, 
	  EV_IO_CAPABILITY_RESPONSE = 0x32, 
	  EV_USER_CONFIRMATION_REQUEST = 0x33,
	  EV_SIMPLE_PAIRING_COMPLETE = 0x36, 
	  EV_RETURN_LINK_KEYS = 0x15,
      EV_LE_META_EVENT = 0x3E,
     };
 
 enum {
    EV_LE_Connection_Complete = 0x01,
    EV_LE_ADVERTISING_REPORT = 0x02,
    EV_LE_CONNECTION_UPDATE_COMPLETE = 0x03,
    EV_LE_READ_REMOTE_FEATURES_COMPLETE = 0x04,
    EV_LE_LONG_TERM_KEY_REQUEST = 0x05,
 };



// Note: The states may be moved or splirt up.

// different modes
enum {PC_RESET = 1, PC_READ_LOCAL_SUPPORTED_COMMANDS, PC_READ_LOCAL_SUPPORTED_FEATURES, PC_SEND_SET_EVENT_MASK, 
      PC_SET_LE_EVENT_MASK, PC_LE_READ_BUFFER_SIZE,
      PC_WRITE_CLASS_DEVICE, 
      PC_MAYBE_WRITE_SIMPLE_PAIR, PC_MAYBE_READ_SIMPLE_PAIR,
      PC_READ_BDADDR, PC_READ_LOCAL_VERSION,
      // Pairing.mode
      PC_SEND_WRITE_SCAN_PAGE_0 = 0x20, // not sure if we will need a cancel before inquire...
      PC_SEND_WRITE_INQUIRE_MODE = 0x21, PC_SEND_INQUIRE,
      PC_INQUIRE_CANCEL = 100,
      PC_SEND_AUTHENTICATION_REQUEST = 110,
      PC_AUTHENTICATION_REQUESTED,

      PC_SEND_READ_REMOTE_EXTENDED_FEATURES = 115,

      PC_LINK_KEY_NEGATIVE = 120,
      PC_PIN_CODE_REPLY = 130,
      PC_CONNECT_AFTER_SDP_DISCONNECT = 140,
      PC_WRITE_SCAN_PAGE = 200,
      PC_SEND_REMOTE_SUPPORTED_FEATURES = 300,
      PC_SEND_REMOTE_EXTENDED_FEATURES = 310,
	  PC_SEND_SET_SIMPLE_PAIRING = 320

     };
//////////////
enum {CCON_INT = 0x01, CCON_CONT = 0x02, CCON_SDP = 0x04, CCON_ALL = 0x07};

//////////////


// Setup some states for the TX pipe where we need to chain messages
enum {STATE_TX_SEND_CONNECT_INT = 200, STATE_TX_SEND_CONECT_RSP_SUCCESS, STATE_TX_SEND_CONFIG_REQ, STATE_TX_SEND_CONECT_ISR_RSP_SUCCESS, STATE_TX_SEND_CONFIG_ISR_REQ,
      STATE_TX_SEND_CONECT_SDP_RSP_SUCCESS, STATE_TX_SEND_CONFIG_SDP_REQ
     };

#endif
