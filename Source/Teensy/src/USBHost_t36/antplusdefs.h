#ifndef _LIBANT_H_
#define _LIBANT_H_

#define WHEEL_CIRCUMFERENCE                     2122

#define ANT_TRANSMISSION_SLAVE			0x00
#define ANT_TRANSMISSION_MASTER			0x05

#define ANT_PERIOD_HRM					8070
#define ANT_PERIOD_SPDCAD				8086
#define ANT_PERIOD_POWER				8182
#define ANT_PERIOD_STRIDE				8134		// footpod
#define ANT_PERIOD_SPEED				8118
#define ANT_PERIOD_CADENCE				8102

#define ANT_PERIOD_CONTROL				8192
#define ANT_PERIOD_KICKR				2048
#define ANT_PERIOD_MOXY					8192
#define ANT_PERIOD_TACX_VORTEX			8192
#define ANT_PERIOD_ITNESS_EQUIPMENT		8192
#define ANT_PERIOD_QUARQ				(8182*4)
#define ANT_PERIOD_QUARQ_FAST			(8182/16)


#define ANT_FREQUENCY_SPORT				57
#define ANT_FREQUENCY_STRIDE			57
#define ANT_FREQUENCY_FAST_QUARQ		61
#define ANT_FREQUENCY_QUARQ				61
#define ANT_FREQUENCY_KICKR				52
#define ANT_FREQUENCY_MOXY				57
#define ANT_FREQUENCY_TACX_VORTEX		66
#define ANT_FREQUENCY_FITNESS_EQUIPMENT	57


#define ANT_DEVICE_ANTFS				1
#define ANT_DEVICE_POWER				11
//#define ANT_DEVICE_QUARQ_FAST_OLD		11
#define ANT_DEVICE_ENVIRONMENT_LEGACY	12
#define ANT_DEVICE_MULTISPORT_SPDST		15
#define ANT_DEVICE_CONTROL				16
#define ANT_DEVICE_FITNESS_EQUIPMENT	17
#define ANT_DEVICE_BLOOD_PRESSURE		18
#define ANT_DEVICE_GEOCACHE_NODE		19
#define ANT_DEVICE_LIGHT_VEHICLE		20
#define ANT_DEVICE_ENV_SENSOR			25
#define ANT_DEVICE_MOXY					31
#define ANT_DEVICE_TACX_VORTEX			61
#define ANT_DEVICE_QUARQ_FAST			96
#define ANT_DEVICE_QUARQ				96
#define ANT_DEVICE_WEIGHT_SCALE			119
#define ANT_DEVICE_HRM					120
#define ANT_DEVICE_SPDCAD				121
#define ANT_DEVICE_CADENCE				122
#define ANT_DEVICE_SPEED				123
#define ANT_DEVICE_STRIDE				124			// footpod


// ANT_DEVICE_POWER broadcast types
#define ANT_POWER_STANDARD				0x10
#define ANT_POWER_WHEELTORQUE			0x11
#define ANT_POWER_CRANKTORQUE			0x12
#define ANT_POWER_TE_AND_PS				0x13
#define ANT_POWER_CRANKSRM				0x20


#define ANT_CHANNEL_STATUS_UNASSIGNED	0
#define ANT_CHANNEL_STATUS_ASSIGNED		1
#define ANT_CHANNEL_STATUS_SEARCHING	2
#define ANT_CHANNEL_STATUS_TRACKING		3
#define ANT_CHANNEL_STATUS_UNKNOWN		4			/*shouldn't see this */
#define ANT_CHANNEL_STATUS_MASK			0x03


#define ANT_CHANNEL_TYPE_SLAVE			((uint8_t)0x00)		//Slave channel (PARAMETER_RX_NOT_TX).
#define ANT_CHANNEL_TYPE_MASTER			((uint8_t)0x10)		//Master channel (PARAMETER_TX_NOT_RX).
#define ANT_CHANNEL_TYPE_SLAVE_RX_ONLY	((uint8_t)0x40)		//Slave rx only channel (PARAMETER_RX_NOT_TX | PARAMETER_RX_ONLY).
#define ANT_CHANNEL_TYPE_MASTER_TX_ONLY	((uint8_t)0x50)		//Master tx only channel (PARAMETER_TX_NOT_RX | PARAMETER_NO_TX_GUARD_BAND).
#define ANT_CHANNEL_TYPE_SHARED_SLAVE	((uint8_t)0x20)		//Shared slave channel (PARAMETER_RX_NOT_TX | PARAMETER_SHARED_CHANNEL).
#define ANT_CHANNEL_TYPE_SHARED_MASTER	((uint8_t)0x30)		//Shared master channel (PARAMETER_TX_NOT_RX | PARAMETER_SHARED_CHANNEL). 
 
 
#define ANT_STARTUP_RESET_POWERON		0x00
#define ANT_STARTUP_RESET_HARDWARE		0x01
#define ANT_STARTUP_RESET_WATCHDOG		0x02
#define ANT_STARTUP_RESET_COMMAND		0x20
#define ANT_STARTUP_RESET_SYNC			0x40
#define ANT_STARTUP_RESET_SUSPEND		0x80
 
	
#define STREAM_SYNC					0
#define STREAM_LENGTH				1
#define STREAM_MESSAGE				2
#define STREAM_DATA					3
#define STREAM_CHANNEL				STREAM_DATA

#define STREAM_CHANNEL_ID			0
#define STREAM_CHANNEL_STATUS		1

#define STREAM_EVENT_CHANNEL_ID		0
#define STREAM_EVENT_RESPONSE_ID	1

#define STREAM_CAP_MAXCHANNELS		0
#define STREAM_CAP_MAXNETWORKS		1
#define STREAM_CAP_STDOPTIONS		2
#define STREAM_CAP_ADVANCED			3
#define STREAM_CAP_ADVANCED2		4

#define STREAM_STARTUP_REASON		0

#define STREAM_CHANNELSTATUS_STATUS 1

#define STREAM_VERSION_STRING		0

#define STREAM_CHANNELID_DEVNO_LO	1
#define STREAM_CHANNELID_DEVNO_HI	2
#define STREAM_CHANNELID_DEVTYPE	3
#define STREAM_CHANNELID_TRANTYPE	4

#define STREAM_EVENT_EVENTID		2

#define STREAM_RXBROADCAST_DEV120_BEATLO	5
#define STREAM_RXBROADCAST_DEV120_BEATHI	6
#define STREAM_RXBROADCAST_DEV120_SEQ		7
#define STREAM_RXBROADCAST_DEV120_HR		8


#define RESET_FLAGS_MASK                           ((uint8_t)0xE0)
#define RESET_SUSPEND                              ((uint8_t)0x80)              // this must follow bitfield def
#define RESET_SYNC                                 ((uint8_t)0x40)              // this must follow bitfield def
#define RESET_CMD                                  ((uint8_t)0x20)              // this must follow bitfield def
#define RESET_WDT                                  ((uint8_t)0x02)
#define RESET_RST                                  ((uint8_t)0x01)
#define RESET_POR                                  ((uint8_t)0x00)



//////////////////////////////////////////////
// Assign Channel Parameters
//////////////////////////////////////////////
#define PARAMETER_RX_NOT_TX                        ((uint8_t)0x00)
#define PARAMETER_TX_NOT_RX                        ((uint8_t)0x10)
#define PARAMETER_SHARED_CHANNEL                   ((uint8_t)0x20)
#define PARAMETER_NO_TX_GUARD_BAND                 ((uint8_t)0x40)
#define PARAMETER_ALWAYS_RX_WILD_CARD_SEARCH_ID    ((uint8_t)0x40)                 //Pre-AP2
#define PARAMETER_RX_ONLY                          ((uint8_t)0x40)

//////////////////////////////////////////////
// Ext. Assign Channel Parameters
//////////////////////////////////////////////
#define EXT_PARAM_ALWAYS_SEARCH                    ((uint8_t)0x01)
#define EXT_PARAM_FREQUENCY_AGILITY                ((uint8_t)0x04)


/////////////////////////////////////////////////////////////////////////////
// Message Format
// Messages are in the format:
//
// AX XX YY -------- CK
//
// where: AX    is the 1 byte sync byte either transmit or recieve
//        XX    is the 1 byte size of the message (0-249) NOTE: THIS WILL BE LIMITED BY THE EMBEDDED RECEIVE BUFFER SIZE
//        YY    is the 1 byte ID of the message (1-255, 0 is invalid)
//        ----- is the data of the message (0-249 bytes of data)
//        CK    is the 1 byte Checksum of the message
/////////////////////////////////////////////////////////////////////////////
#define MESG_TX_SYNC                         ((uint8_t)0xA4)
#define MESG_RX_SYNC                         ((uint8_t)0xA5)
#define MESG_SYNC_SIZE                       ((uint8_t)1)
#define MESG_SIZE_SIZE                       ((uint8_t)1)
#define MESG_ID_SIZE                         ((uint8_t)1)
#define MESG_CHANNEL_NUM_SIZE                ((uint8_t)1)
#define MESG_EXT_MESG_BF_SIZE                ((uint8_t)1)  // NOTE: this could increase in the future
#define MESG_CHECKSUM_SIZE                   ((uint8_t)1)
#define MESG_DATA_SIZE                       ((uint8_t)9)

// The largest serial message is an ANT data message with all of the extended fields
#define MESG_ANT_MAX_PAYLOAD_SIZE            ANT_STANDARD_DATA_PAYLOAD_SIZE

#define MESG_MAX_EXT_DATA_SIZE               (ANT_EXT_MESG_DEVICE_ID_FIELD_SIZE + 4 + 2) // ANT device ID (4 bytes) +  (4 bytes) +  (2 bytes)

#define MESG_MAX_DATA_SIZE                   (MESG_ANT_MAX_PAYLOAD_SIZE + MESG_EXT_MESG_BF_SIZE + MESG_MAX_EXT_DATA_SIZE) // ANT data payload (8 bytes) + extended bitfield (1 byte) + extended data (10 bytes)
#define MESG_MAX_SIZE_VALUE                  (MESG_MAX_DATA_SIZE + MESG_CHANNEL_NUM_SIZE)  // this is the maximum value that the serial message size value is allowed to be
#define MESG_BUFFER_SIZE                     (MESG_SIZE_SIZE + MESG_ID_SIZE + MESG_CHANNEL_NUM_SIZE + MESG_MAX_DATA_SIZE + MESG_CHECKSUM_SIZE)
#define MESG_FRAMED_SIZE                     (MESG_ID_SIZE + MESG_CHANNEL_NUM_SIZE + MESG_MAX_DATA_SIZE)
#define MESG_HEADER_SIZE                     (MESG_SYNC_SIZE + MESG_SIZE_SIZE + MESG_ID_SIZE)
#define MESG_FRAME_SIZE                      (MESG_HEADER_SIZE + MESG_CHECKSUM_SIZE)
#define MESG_MAX_SIZE                        (MESG_MAX_DATA_SIZE + MESG_FRAME_SIZE)

#define MESG_SIZE_OFFSET                     (MESG_SYNC_SIZE)
#define MESG_ID_OFFSET                       (MESG_SYNC_SIZE + MESG_SIZE_SIZE)
#define MESG_DATA_OFFSET                     (MESG_HEADER_SIZE)
#define MESG_RECOMMENDED_BUFFER_SIZE         ((uint8_t)ANTPLUS_MAXPACKETSIZE)    


//////////////////////////////////////////////
// Message ID's
//////////////////////////////////////////////
#define MESG_INVALID_ID                      ((uint8_t)0x00)
#define MESG_EVENT_ID                        ((uint8_t)0x01)

#define MESG_VERSION_ID                      ((uint8_t)0x3E)
#define MESG_RESPONSE_EVENT_ID               ((uint8_t)0x40)

#define MESG_UNASSIGN_CHANNEL_ID             ((uint8_t)0x41)
#define MESG_ASSIGN_CHANNEL_ID               ((uint8_t)0x42)
#define MESG_CHANNEL_MESG_PERIOD_ID          ((uint8_t)0x43)
#define MESG_CHANNEL_SEARCH_TIMEOUT_ID       ((uint8_t)0x44)
#define MESG_CHANNEL_RADIO_FREQ_ID           ((uint8_t)0x45)
#define MESG_NETWORK_KEY_ID                  ((uint8_t)0x46)
#define MESG_RADIO_TX_POWER_ID               ((uint8_t)0x47)
#define MESG_RADIO_CW_MODE_ID                ((uint8_t)0x48)
#define MESG_SEARCH_WAVEFORM_ID              ((uint8_t)0x49)

#define MESG_SYSTEM_RESET_ID                 ((uint8_t)0x4A)
#define MESG_OPEN_CHANNEL_ID                 ((uint8_t)0x4B)
#define MESG_CLOSE_CHANNEL_ID                ((uint8_t)0x4C)
#define MESG_REQUEST_ID                      ((uint8_t)0x4D)

#define MESG_BROADCAST_DATA_ID               ((uint8_t)0x4E)
#define MESG_ACKNOWLEDGED_DATA_ID            ((uint8_t)0x4F)
#define MESG_BURST_DATA_ID                   ((uint8_t)0x50)

#define MESG_CHANNEL_ID_ID                   ((uint8_t)0x51)
#define MESG_CHANNEL_STATUS_ID               ((uint8_t)0x52)
#define MESG_RADIO_CW_INIT_ID                ((uint8_t)0x53)
#define MESG_CAPABILITIES_ID                 ((uint8_t)0x54)

#define MESG_STACKLIMIT_ID                   ((uint8_t)0x55)

#define MESG_SCRIPT_DATA_ID                  ((uint8_t)0x56)
#define MESG_SCRIPT_CMD_ID                   ((uint8_t)0x57)

#define MESG_ID_LIST_ADD_ID                  ((uint8_t)0x59)
#define MESG_ID_LIST_CONFIG_ID               ((uint8_t)0x5A)
#define MESG_OPEN_RX_SCAN_ID                 ((uint8_t)0x5B)

#define MESG_EXT_CHANNEL_RADIO_FREQ_ID       ((uint8_t)0x5C)  // OBSOLETE: (for 905 radio)
#define MESG_EXT_BROADCAST_DATA_ID           ((uint8_t)0x5D)
#define MESG_EXT_ACKNOWLEDGED_DATA_ID        ((uint8_t)0x5E)
#define MESG_EXT_BURST_DATA_ID               ((uint8_t)0x5F)

#define MESG_CHANNEL_RADIO_TX_POWER_ID       ((uint8_t)0x60)
#define MESG_GET_SERIAL_NUM_ID               ((uint8_t)0x61)
#define MESG_GET_TEMP_CAL_ID                 ((uint8_t)0x62)
#define MESG_SET_LP_SEARCH_TIMEOUT_ID        ((uint8_t)0x63)
#define MESG_SET_TX_SEARCH_ON_NEXT_ID        ((uint8_t)0x64)
#define MESG_SERIAL_NUM_SET_CHANNEL_ID_ID    ((uint8_t)0x65)
#define MESG_RX_EXT_MESGS_ENABLE_ID          ((uint8_t)0x66)  
#define MESG_RADIO_CONFIG_ALWAYS_ID          ((uint8_t)0x67)
#define MESG_ENABLE_LED_FLASH_ID             ((uint8_t)0x68)
#define MESG_XTAL_ENABLE_ID                  ((uint8_t)0x6D)
#define MESG_STARTUP_MESG_ID                 ((uint8_t)0x6F)
#define MESG_AUTO_FREQ_CONFIG_ID             ((uint8_t)0x70)
#define MESG_PROX_SEARCH_CONFIG_ID           ((uint8_t)0x71)

#define MESG_CUBE_CMD_ID                     ((uint8_t)0x80)

#define MESG_GET_PIN_DIODE_CONTROL_ID        ((uint8_t)0x8D)
#define MESG_PIN_DIODE_CONTROL_ID            ((uint8_t)0x8E)
#define MESG_FIT1_SET_AGC_ID                 ((uint8_t)0x8F)

#define MESG_FIT1_SET_EQUIP_STATE_ID         ((uint8_t)0x91)  // *** CONFLICT: w/ Sensrcore, Fit1 will never have sensrcore enabled

// Sensrcore Messages
#define MESG_SET_CHANNEL_INPUT_MASK_ID       ((uint8_t)0x90)
#define MESG_SET_CHANNEL_DATA_TYPE_ID        ((uint8_t)0x91)
#define MESG_READ_PINS_FOR_SECT_ID           ((uint8_t)0x92)
#define MESG_TIMER_SELECT_ID                 ((uint8_t)0x93)
#define MESG_ATOD_SETTINGS_ID                ((uint8_t)0x94)
#define MESG_SET_SHARED_ADDRESS_ID           ((uint8_t)0x95)
#define MESG_ATOD_EXTERNAL_ENABLE_ID         ((uint8_t)0x96)
#define MESG_ATOD_PIN_SETUP_ID               ((uint8_t)0x97)
#define MESG_SETUP_ALARM_ID                  ((uint8_t)0x98)
#define MESG_ALARM_VARIABLE_MODIFY_TEST_ID   ((uint8_t)0x99)
#define MESG_PARTIAL_RESET_ID                ((uint8_t)0x9A)
#define MESG_OVERWRITE_TEMP_CAL_ID           ((uint8_t)0x9B)
#define MESG_SERIAL_PASSTHRU_SETTINGS_ID     ((uint8_t)0x9C)

#define MESG_READ_SEGA_ID                    ((uint8_t)0xA0)
#define MESG_SEGA_CMD_ID                     ((uint8_t)0xA1)
#define MESG_SEGA_DATA_ID                    ((uint8_t)0xA2)
#define MESG_SEGA_ERASE_ID                   ((uint8_t)0xA3)
#define MESG_SEGA_WRITE_ID                   ((uint8_t)0xA4)
#define AVOID_USING_SYNC_BYTES_FOR_MESG_IDS  ((uint8_t)0xA5)

#define MESG_SEGA_LOCK_ID                    ((uint8_t)0xA6)
#define MESG_FLASH_PROTECTION_CHECK_ID       ((uint8_t)0xA7)
#define MESG_UARTREG_ID                      ((uint8_t)0xA8)
#define MESG_MAN_TEMP_ID                     ((uint8_t)0xA9)
#define MESG_BIST_ID                         ((uint8_t)0xAA)
#define MESG_SELFERASE_ID                    ((uint8_t)0xAB)
#define MESG_SET_MFG_BITS_ID                 ((uint8_t)0xAC)
#define MESG_UNLOCK_INTERFACE_ID             ((uint8_t)0xAD)
#define MESG_SERIAL_ERROR_ID                 ((uint8_t)0xAE)
#define MESG_SET_ID_STRING_ID                ((uint8_t)0xAF)

#define MESG_IO_STATE_ID                     ((uint8_t)0xB0)
#define MESG_CFG_STATE_ID                    ((uint8_t)0xB1)
#define MESG_BLOWFUSE_ID                     ((uint8_t)0xB2)
#define MESG_MASTERIOCTRL_ID                 ((uint8_t)0xB3)
#define MESG_PORT_GET_IO_STATE_ID            ((uint8_t)0xB4)
#define MESG_PORT_SET_IO_STATE_ID            ((uint8_t)0xB5)

#define MESG_SLEEP_ID                        ((uint8_t)0xC5)
#define MESG_GET_GRMN_ESN_ID                 ((uint8_t)0xC6)

#define MESG_DEBUG_ID                        ((uint8_t)0xF0)  // use 2 byte sub-index identifier

//////////////////////////////////////////////
// Message Sizes
//////////////////////////////////////////////
#define MESG_INVALID_SIZE                    ((uint8_t)0)

#define MESG_VERSION_SIZE                    ((uint8_t)13)
#define MESG_RESPONSE_EVENT_SIZE             ((uint8_t)3)
#define MESG_CHANNEL_STATUS_SIZE             ((uint8_t)2)

#define MESG_UNASSIGN_CHANNEL_SIZE           ((uint8_t)1)
#define MESG_ASSIGN_CHANNEL_SIZE             ((uint8_t)3)
#define MESG_CHANNEL_ID_SIZE                 ((uint8_t)5)
#define MESG_CHANNEL_MESG_PERIOD_SIZE        ((uint8_t)3)
#define MESG_CHANNEL_SEARCH_TIMEOUT_SIZE     ((uint8_t)2)
#define MESG_CHANNEL_RADIO_FREQ_SIZE         ((uint8_t)2)
#define MESG_CHANNEL_RADIO_TX_POWER_SIZE     ((uint8_t)2)
#define MESG_NETWORK_KEY_SIZE                ((uint8_t)9)
#define MESG_RADIO_TX_POWER_SIZE             ((uint8_t)2)
#define MESG_RADIO_CW_MODE_SIZE              ((uint8_t)3)
#define MESG_RADIO_CW_INIT_SIZE              ((uint8_t)1)
#define MESG_SYSTEM_RESET_SIZE               ((uint8_t)1)
#define MESG_OPEN_CHANNEL_SIZE               ((uint8_t)1)
#define MESG_CLOSE_CHANNEL_SIZE              ((uint8_t)1)
#define MESG_REQUEST_SIZE                    ((uint8_t)2)

#define MESG_CAPABILITIES_SIZE               ((uint8_t)6)
#define MESG_STACKLIMIT_SIZE                 ((uint8_t)2)

#define MESG_SCRIPT_DATA_SIZE                ((uint8_t)10)
#define MESG_SCRIPT_CMD_SIZE                 ((uint8_t)3)

#define MESG_ID_LIST_ADD_SIZE                ((uint8_t)6)
#define MESG_ID_LIST_CONFIG_SIZE             ((uint8_t)3)
#define MESG_OPEN_RX_SCAN_SIZE               ((uint8_t)1)
#define MESG_EXT_CHANNEL_RADIO_FREQ_SIZE     ((uint8_t)3)

#define MESG_RADIO_CONFIG_ALWAYS_SIZE        ((uint8_t)2)
#define MESG_RX_EXT_MESGS_ENABLE_SIZE        ((uint8_t)2)
#define MESG_SET_TX_SEARCH_ON_NEXT_SIZE      ((uint8_t)2)
#define MESG_SET_LP_SEARCH_TIMEOUT_SIZE      ((uint8_t)2)

#define MESG_SERIAL_NUM_SET_CHANNEL_ID_SIZE  ((uint8_t)3)
#define MESG_ENABLE_LED_FLASH_SIZE           ((uint8_t)2)
#define MESG_GET_SERIAL_NUM_SIZE             ((uint8_t)4)
#define MESG_GET_TEMP_CAL_SIZE               ((uint8_t)4)
#define MESG_CLOCK_DRIFT_DATA_SIZE           ((uint8_t)9)

#define MESG_AGC_CONFIG_SIZE                 ((uint8_t)2)
#define MESG_RUN_SCRIPT_SIZE                 ((uint8_t)2)
#define MESG_ANTLIB_CONFIG_SIZE              ((uint8_t)2)
#define MESG_XTAL_ENABLE_SIZE                ((uint8_t)1)
#define MESG_STARTUP_MESG_SIZE               ((uint8_t)1)
#define MESG_AUTO_FREQ_CONFIG_SIZE           ((uint8_t)4)
#define MESG_PROX_SEARCH_CONFIG_SIZE         ((uint8_t)2)

#define MESG_GET_PIN_DIODE_CONTROL_SIZE      ((uint8_t)1)
#define MESG_PIN_DIODE_CONTROL_ID_SIZE       ((uint8_t)2)
#define MESG_FIT1_SET_EQUIP_STATE_SIZE       ((uint8_t)2)
#define MESG_FIT1_SET_AGC_SIZE               ((uint8_t)3)

#define MESG_READ_SEGA_SIZE                  ((uint8_t)2)
#define MESG_SEGA_CMD_SIZE                   ((uint8_t)3)
#define MESG_SEGA_DATA_SIZE                  ((uint8_t)10)
#define MESG_SEGA_ERASE_SIZE                 ((uint8_t)0)
#define MESG_SEGA_WRITE_SIZE                 ((uint8_t)3)
#define MESG_SEGA_LOCK_SIZE                  ((uint8_t)1)
#define MESG_FLASH_PROTECTION_CHECK_SIZE     ((uint8_t)1)
#define MESG_UARTREG_SIZE                    ((uint8_t)2)
#define MESG_MAN_TEMP_SIZE                   ((uint8_t)2)
#define MESG_BIST_SIZE                       ((uint8_t)6)
#define MESG_SELFERASE_SIZE                  ((uint8_t)2)
#define MESG_SET_MFG_BITS_SIZE               ((uint8_t)2)
#define MESG_UNLOCK_INTERFACE_SIZE           ((uint8_t)1)
#define MESG_SET_SHARED_ADDRESS_SIZE         ((uint8_t)3)

#define MESG_GET_GRMN_ESN_SIZE               ((uint8_t)5)

#define MESG_IO_STATE_SIZE                   ((uint8_t)2)
#define MESG_CFG_STATE_SIZE                  ((uint8_t)2)
#define MESG_BLOWFUSE_SIZE                   ((uint8_t)1)
#define MESG_MASTERIOCTRL_SIZE               ((uint8_t)1)
#define MESG_PORT_SET_IO_STATE_SIZE          ((uint8_t)5)

#define MESG_SLEEP_SIZE                      ((uint8_t)1)
#define MESG_EXT_DATA_SIZE                   ((uint8_t)13)

//////////////////////////////////////////////
// PC Application Event Codes
//////////////////////////////////////////////

#define EVENT_RX_BROADCAST                         ((uint8_t)0x9A)           // returned when module receives broadcast data
#define EVENT_RX_ACKNOWLEDGED                      ((uint8_t)0x9B)           // returned when module receives acknowledged data
#define EVENT_RX_BURST_PACKET                      ((uint8_t)0x9C)           // returned when module receives burst data

#define EVENT_RX_EXT_BROADCAST                     ((uint8_t)0x9D)           // returned when module receives broadcast data
#define EVENT_RX_EXT_ACKNOWLEDGED                  ((uint8_t)0x9E)           // returned when module receives acknowledged data
#define EVENT_RX_EXT_BURST_PACKET                  ((uint8_t)0x9F)           // returned when module receives burst data


#define EVENT_RX_FLAG_BROADCAST                    ((uint8_t)0xA3)          // returned when module receives broadcast data with flag attached
#define EVENT_RX_FLAG_ACKNOWLEDGED                 ((uint8_t)0xA4)          // returned when module receives acknowledged data with flag attached
#define EVENT_RX_FLAG_BURST_PACKET                 ((uint8_t)0xA5)          // returned when module receives burst data with flag attached           


//////////////////////////////////////////////
// Response / Event Codes
//////////////////////////////////////////////
#define RESPONSE_NO_ERROR                          ((uint8_t)0x00)
#define NO_EVENT                                   ((uint8_t)0x00)

#define EVENT_RX_SEARCH_TIMEOUT                    ((uint8_t)0x01)
#define EVENT_RX_FAIL                              ((uint8_t)0x02)
#define EVENT_TX                                   ((uint8_t)0x03)
#define EVENT_TRANSFER_RX_FAILED                   ((uint8_t)0x04)
#define EVENT_TRANSFER_TX_COMPLETED                ((uint8_t)0x05)
#define EVENT_TRANSFER_TX_FAILED                   ((uint8_t)0x06)
#define EVENT_CHANNEL_CLOSED                       ((uint8_t)0x07)
#define EVENT_RX_FAIL_GO_TO_SEARCH                 ((uint8_t)0x08)
#define EVENT_CHANNEL_COLLISION                    ((uint8_t)0x09)
#define EVENT_TRANSFER_TX_START                    ((uint8_t)0x0A)           // a pending transmit transfer has begun

#define EVENT_CHANNEL_ACTIVE                       ((uint8_t)0x0F)

#define EVENT_TRANSFER_TX_NEXT_MESSAGE             ((uint8_t)0x11)           // only enabled in FIT1

#define CHANNEL_IN_WRONG_STATE                     ((uint8_t)0x15)           // returned on attempt to perform an action from the wrong channel state
#define CHANNEL_NOT_OPENED                         ((uint8_t)0x16)           // returned on attempt to communicate on a channel that is not open
#define CHANNEL_ID_NOT_SET                         ((uint8_t)0x18)           // returned on attempt to open a channel without setting the channel ID
#define CLOSE_ALL_CHANNELS                         ((uint8_t)0x19)           // returned when attempting to start scanning mode, when channels are still open

#define TRANSFER_IN_PROGRESS                       ((uint8_t)0x1F)           // returned on attempt to communicate on a channel with a TX transfer in progress
#define TRANSFER_SEQUENCE_NUMBER_ERROR             ((uint8_t)0x20)           // returned when sequence number is out of order on a Burst transfer
#define TRANSFER_IN_ERROR                          ((uint8_t)0x21)
#define TRANSFER_BUSY                              ((uint8_t)0x22)

#define MESSAGE_SIZE_EXCEEDS_LIMIT                 ((uint8_t)0x27)           // returned if a data message is provided that is too large
#define INVALID_MESSAGE                            ((uint8_t)0x28)           // returned when the message has an invalid parameter
#define INVALID_NETWORK_NUMBER                     ((uint8_t)0x29)           // returned when an invalid network number is provided
#define INVALID_LIST_ID                            ((uint8_t)0x30)           // returned when the provided list ID or size exceeds the limit
#define INVALID_SCAN_TX_CHANNEL                    ((uint8_t)0x31)           // returned when attempting to transmit on channel 0 when in scan mode
#define INVALID_PARAMETER_PROVIDED                 ((uint8_t)0x33)           // returned when an invalid parameter is specified in a configuration message

#define EVENT_QUE_OVERFLOW                         ((uint8_t)0x35)           // ANT event que has overflowed and lost 1 or more events

#define EVENT_CLK_ERROR                            ((uint8_t)0x36)           //!! debug XOSC16M

#define SCRIPT_FULL_ERROR                          ((uint8_t)0x40)           // error writing to script, memory is full
#define SCRIPT_WRITE_ERROR                         ((uint8_t)0x41)           // error writing to script, bytes not written correctly
#define SCRIPT_INVALID_PAGE_ERROR                  ((uint8_t)0x42)           // error accessing script page
#define SCRIPT_LOCKED_ERROR                        ((uint8_t)0x43)           // the scripts are locked and can't be dumped

#define NO_RESPONSE_MESSAGE                        ((uint8_t)0x50)           // returned to the Command_SerialMessageProcess function, so no reply message is generated
#define RETURN_TO_MFG                              ((uint8_t)0x51)           // default return to any mesg when the module determines that the mfg procedure has not been fully completed

#define FIT_ACTIVE_SEARCH_TIMEOUT                  ((uint8_t)0x60)           // Fit1 only event added for timeout of the pairing state after the Fit module becomes active
#define FIT_WATCH_PAIR                             ((uint8_t)0x61)           // Fit1 only
#define FIT_WATCH_UNPAIR                           ((uint8_t)0x62)           // Fit1 only

// Internal only events below this point
#define INTERNAL_ONLY_EVENTS                       ((uint8_t)0x80)
#define EVENT_RX                                   ((uint8_t)0x80)           // INTERNAL: Event for a receive message
#define EVENT_NEW_CHANNEL                          ((uint8_t)0x81)           // INTERNAL: EVENT for a new active channel
#define EVENT_PASS_THRU                            ((uint8_t)0x82)           // INTERNAL: Event to allow an upper stack events to pass through lower stacks
#define EVENT_TRANSFER_RX_COMPLETED                ((uint8_t)0x83)           // INTERNAL: Event for RX completed that indicates ANT library is ready for new messasges

#define EVENT_BLOCKED                              ((uint8_t)0xFF)           // INTERNAL: Event to replace any event we do not wish to go out, will also zero the size of the Tx message


#endif

