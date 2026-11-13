/*
 * MSC Teensy36 USB Host Mass Storage library
 * Copyright (c) 2017-2019 Warren Watson.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
 
 // msc.h

#ifndef _MSC_H_
#define _MSC_H_

// SCSI Commands (minimal set)
#define CBW_SIGNATURE	    0x43425355UL
#define CSW_SIGNATURE	    0x53425355UL
#define CMD_DIR_DATA_OUT	0x00
#define CMD_DIR_DATA_IN	    0x80
#define CMD_RD_CAPACITY_10	0x25
#define CMD_INQUIRY         0x12
#define CMD_RD_10			0x28
#define CMD_WR_10			0x2A
#define CMD_TEST_UNIT_READY 0x00
#define CMD_REQUEST_SENSE   0x03
#define CMD_START_STOP_UNIT 0x1B
#define CMD_REPORT_LUNS     0xA0
#define NO_RD_WR			0x00 // transfer type is not read or write blocks.

// Command Status Wrapper Error Codes
#define	MS_CBW_PASS 		0
#define	MS_CBW_FAIL  		1
#define	MS_CBW_PHASE_ERROR	2
#define MS_CSW_TAG_ERROR	253
#define MS_CSW_SIG_ERROR	254
#define MS_SCSI_ERROR		255

// SCSI Sense Key codes
#define MS_NOT_READY		0x02
#define MS_MEDIUM_ERROR		0x03
#define MS_ILLEGAL_REQUEST	0x05
#define MS_UNIT_ATTENTION	0x06
#define MS_LBA_OUT_OF_RANGE 0x21
#define MS_MEDIA_CHANGED    0x28
#define MS_MEDIUM_NOT_PRESENT  0x3A

// SCSI Error Codes
#define MS_MEDIA_CHANGED_ERR 0x2A
#define MS_NO_MEDIA_ERR		0x28
#define MS_UNIT_NOT_READY	0x23
#define MS_BAD_LBA_ERR		0x29
#define MS_CMD_ERR			0x26

#define	MS_INIT_PASS 		0
#define MAXLUNS				16

// These two defines are timeouts for detecting a connected drive
// and waiting for it to be operational.
#define MEDIA_READY_TIMEOUT	5000 // 1000
#define MSC_CONNECT_TIMEOUT	5000 // 4000

// Command Block Wrapper Struct
typedef struct
{
	uint32_t Signature;
	uint32_t Tag;
	uint32_t TransferLength;
	uint8_t Flags;
	uint8_t LUN;
	uint8_t CommandLength;
	uint8_t CommandData[16];
}  __attribute__((packed)) msCommandBlockWrapper_t;

// MSC Command Status Wrapper Struct
typedef struct
{
	uint32_t Signature;
	uint32_t Tag;
	uint32_t DataResidue;
	uint8_t  Status;
}  __attribute__((packed)) msCommandStatusWrapper_t;

// MSC Device Capacity Struct
typedef struct
{
	uint32_t Blocks;
	uint32_t BlockSize;
} msSCSICapacity_t;

// MSC Inquiry Command Reponse Struct
typedef struct
{
	unsigned DeviceType          : 5;
	unsigned PeripheralQualifier : 3;
	unsigned Reserved            : 7;
	unsigned Removable           : 1;
	uint8_t  Version;
	unsigned ResponseDataFormat  : 4;
	unsigned Reserved2           : 1;
	unsigned NormACA             : 1;
	unsigned TrmTsk              : 1;
	unsigned AERC                : 1;
	uint8_t  AdditionalLength;
	uint8_t  Reserved3[2];
	unsigned SoftReset           : 1;
	unsigned CmdQue              : 1;
	unsigned Reserved4           : 1;
	unsigned Linked              : 1;
	unsigned Sync                : 1;
	unsigned WideBus16Bit        : 1;
	unsigned WideBus32Bit        : 1;
	unsigned RelAddr             : 1;
	uint8_t  VendorID[8];
	uint8_t  ProductID[16];
	uint8_t  RevisionID[4];
}  __attribute__((packed)) msInquiryResponse_t;

// Request Sense Response Struct
typedef struct
{
	uint8_t  ResponseCode;
	uint8_t  SegmentNumber;
	unsigned SenseKey            : 4;
	unsigned Reserved            : 1;
	unsigned ILI                 : 1;
	unsigned EOM                 : 1;
	unsigned FileMark            : 1;
	uint8_t  Information[4];
	uint8_t  AdditionalLength;
	uint8_t  CmdSpecificInformation[4];
	uint8_t  AdditionalSenseCode;
	uint8_t  AdditionalSenseQualifier;
	uint8_t  FieldReplaceableUnitCode;
	uint8_t  SenseKeySpecific[3];
	uint8_t  padding[234];
}  __attribute__((packed)) msRequestSenseResponse_t;

// MSC Drive status/info struct
typedef struct {
	bool connected;    // Device is connected
	bool initialized;  // Device is initialized
	bool mounted;      // Device is mounted
	const char * drvName;
	uint32_t bufferSize;
	uint8_t hubNumber;
	uint8_t hubPort;
	uint8_t deviceAddress;
	uint16_t idVendor;
	uint16_t idProduct;
	msSCSICapacity_t capacity;
	msInquiryResponse_t inquiry;	
} __attribute__((packed)) msDriveInfo_t;

#endif //_MSC_H_
