/* MSC Teensy36 USB Host Mass Storage library
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

//MassStorageDriver.cpp

#include <Arduino.h>
#include "USBHost_t36.h"  // Read this header first for key info
#include "utility/USBFilesystemFormatter.h"
#define print   USBHost::print_
#define println USBHost::println_

// Uncomment this to display function usage and sequencing.
//#define DBGprint 1
#ifdef DBGprint
#define DBGPrintf Serial.printf
#define DBGFlush() Serial.flush()
#else
void inline DBGPrintf(...) {};
void inline DBGFlush() {};
#endif


USBFSBase *USBFSBase::s_first_fs = nullptr;
bool USBFSBase::s_any_fs_changed_state = false;


USBDrive *USBDrive::s_first_drive = nullptr;
bool USBDrive::s_connected_filesystems_changed;
int USBDrive::s_when_to_update = UPDATE_TASK; // default to Task()

static const uint8_t mbdpGuid[16] PROGMEM = {0xA2, 0xA0, 0xD0, 0xEB, 0xE5, 0xB9, 0x33, 0x44, 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7};

// Big Endian/Little Endian
#define swap32(x) ((x >> 24) & 0xff) | \
				  ((x << 8) & 0xff0000) | \
				  ((x >> 8) & 0xff00) |  \
                  ((x << 24) & 0xff000000)

void USBDrive::init()
{
	contribute_Pipes(mypipes, sizeof(mypipes)/sizeof(Pipe_t));
	contribute_Transfers(mytransfers, sizeof(mytransfers)/sizeof(Transfer_t));
	contribute_String_Buffers(mystring_bufs, sizeof(mystring_bufs)/sizeof(strbuf_t));
	driver_ready_for_device(this);

	// Keep a list of drives. 
	_next_drive = s_first_drive;
	s_first_drive = this;
}


bool USBDrive::claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len)
{
	println("USBDrive claim this=", (uint32_t)this, HEX);
	// only claim at interface level

	if (type != 1) return false;
	if (len < 9+7+7) return false; // Interface descriptor + 2 endpoint decriptors 

	print_hexbytes(descriptors, len);

	uint32_t numendpoint = descriptors[4];
	if (numendpoint < 1) return false; 
	if (descriptors[5] != 8) return false; // bInterfaceClass, 8 = MASS Storage class
	if (descriptors[6] != 6) return false; // bInterfaceSubClass, 6 = SCSI transparent command set (SCSI Standards)
	if (descriptors[7] != 80) return false; // bInterfaceProtocol, 80 = BULK-ONLY TRANSPORT

	bInterfaceNumber = descriptors[2];

	uint8_t desc_index = 9;
	uint8_t in_index = 0xff, out_index = 0xff;

	println("numendpoint=", numendpoint, HEX);
	while (numendpoint--) {
		if ((descriptors[desc_index] != 7) || (descriptors[desc_index+1] != 5)) return false; // not an end point
		if (descriptors[desc_index+3] == 2) {  // Bulk end point
			if (descriptors[desc_index+2] & 0x80)
				in_index = desc_index;
			else
				out_index = desc_index;
		}
		desc_index += 7;	// point to next one...
	}
	if ((in_index == 0xff) || (out_index == 0xff)) return false;	// did not find end point
	endpointIn = descriptors[in_index+2]; // bulk-in descriptor 1 81h
	endpointOut = descriptors[out_index+2]; // bulk-out descriptor 2 02h

	println("endpointIn=", endpointIn, HEX);
	println("endpointOut=", endpointOut, HEX);

	uint32_t sizeIn = descriptors[in_index+4] | (descriptors[in_index+5] << 8);
	println("packet size in (USBDrive) = ", sizeIn);

	uint32_t sizeOut = descriptors[out_index+4] | (descriptors[out_index+5] << 8);
	println("packet size out (USBDrive) = ", sizeOut);
	packetSizeIn = sizeIn;	
	packetSizeOut = sizeOut;	

	uint32_t intervalIn = descriptors[in_index+6];
	uint32_t intervalOut = descriptors[out_index+6];

	println("polling intervalIn = ", intervalIn);
	println("polling intervalOut = ", intervalOut);
	datapipeIn = new_Pipe(dev, 2, endpointIn & 0x0F, 1, packetSizeIn, intervalIn);
	datapipeOut = new_Pipe(dev, 2, endpointOut, 0, packetSizeOut, intervalOut);
	datapipeIn->callback_function = callbackIn;
	datapipeOut->callback_function = callbackOut;

	idVendor = dev->idVendor;
	idProduct = dev->idProduct;
	hubNumber = dev->hub_address;
	deviceAddress = dev->address;
	hubPort = dev->hub_port; // Used for device ID with multiple drives.

	msOutCompleted = false;
	msInCompleted = false;
	msControlCompleted = false;
	deviceAvailable = true;
	msDriveInfo.initialized = false;
	msDriveInfo.connected = true;
	_drive_connect_fs_status = USBDRIVE_CONNECTED;
	_cGPTParts = 0; // have not cached this yet GPT

#ifdef DBGprint
	print("   connected = ");
	println(msDriveInfo.connected);
	print("   initialized = ");
	println(msDriveInfo.initialized);
#endif	
	return true;
}

void USBDrive::disconnect()
{
	// We need to go through and release an patitions we are holding onto.
	DBGPrintf("USBDrive::disconnect %p %p\n", this, device);
	USBFSBase *usbfs = USBFSBase::s_first_fs; 
	while (usbfs) {
		DBGPrintf("\t %p %p\n", usbfs, usbfs->mydevice);
		if (usbfs->mydevice == device) {
			usbfs->releasePartition(); // lets release the partition.
			usbfs->mydevice = nullptr;
			s_connected_filesystems_changed = true; // 
		}
		usbfs = usbfs->_next;
	}
 	// BUGBUG:: maybe check to see if this information is
 	// already covered... 
 	_drive_connect_fs_status = USBDRIVE_NOT_CONNECTED;


	deviceAvailable = false;
	println("Device Disconnected...");
	msDriveInfo.connected = false;
	msDriveInfo.initialized = false;
	memset(&msDriveInfo, 0, sizeof(msDriveInfo_t));

#ifdef DBGprint
	print("   connected ");
	println(msDriveInfo.connected);
	print("   initialized ");
	println(msDriveInfo.initialized);
#endif
}

void USBDrive::Task()
{
	if (s_when_to_update != UPDATE_TASK) return;
	if (_drive_connect_fs_status == USBDRIVE_CONNECTED) {
		DBGPrintf("\n === Task() Drive %p connected ===\n", this);
		startFilesystems();
		DBGPrintf("\nTry Partition list");

		#ifdef DBGprint
		printPartionTable(Serial);
		#endif
	}
}


void USBDrive::control(const Transfer_t *transfer)
{
	println("control CallbackIn (USBDrive)");
	print_hexbytes(report, 8);
	msControlCompleted = true;

}

void USBDrive::callbackIn(const Transfer_t *transfer)
{
	println("USBDrive CallbackIn (static)");
	if (transfer->driver) {
		print("transfer->qtd.token = ");
		println(transfer->qtd.token & 255);
		((USBDrive *)(transfer->driver))->new_dataIn(transfer);
	}
}

void USBDrive::callbackOut(const Transfer_t *transfer)
{
	println("USBDrive CallbackOut (static)");
	if (transfer->driver) {
		print("transfer->qtd.token = ");
		println(transfer->qtd.token & 255);
		((USBDrive *)(transfer->driver))->new_dataOut(transfer);
	}
}

void USBDrive::new_dataOut(const Transfer_t *transfer)
{
	uint32_t len = transfer->length - ((transfer->qtd.token >> 16) & 0x7FFF);
	println("USBDrive dataOut (static)", len, DEC);
	print_hexbytes((uint8_t*)transfer->buffer, (len < 32)? len : 32 );
	msOutCompleted = true; // Last out transaction is completed.
}

void USBDrive::new_dataIn(const Transfer_t *transfer)
{
	uint32_t len = transfer->length - ((transfer->qtd.token >> 16) & 0x7FFF);
	println("USBDrive dataIn (static): ", len, DEC);
	print_hexbytes((uint8_t*)transfer->buffer, (len < 32)? len : 32 );
	if (_read_sectors_callback) {
		_emlastRead = 0; // remember that we received something. 
		(*_read_sectors_callback)(_read_sectors_token, (uint8_t*)transfer->buffer);
		_read_sectors_remaining--;
		if (_read_sectors_remaining > 1) queue_Data_Transfer(datapipeIn, transfer->buffer, len, this);
		if (!_read_sectors_remaining) {
			_read_sectors_callback = nullptr;
			msInCompleted = true; // Last in transaction is completed.
		}
#if defined(DBGprint) && (DBGprint > 1)
		Serial.write('@');
		if ((_read_sectors_remaining & 0x3f) == 0) Serial.printf("\n");
#endif
	}
	else msInCompleted = true; // Last in transaction is completed.
}

// Initialize Mass Storage Device
uint8_t USBDrive::mscInit(void) {
#ifdef DBGprint
	println("mscIint()");
#endif
	uint8_t msResult = MS_CBW_PASS;

	CBWTag = 0;
	uint32_t start = millis();
	// Check if device is connected.
	do {
		if((millis() - start) >= MSC_CONNECT_TIMEOUT) {
			return MS_NO_MEDIA_ERR;  // Not connected Error.
		}
		yield();
	} while(!available());

// Uncommenting "msReset()" will cause certain USB flash drives to fail to init or read/write.
// Several SanDisk devices have been proven to fail.
// Possibly due to clearing default power on settings.
//	msReset(); 
	// delay(500); // Not needed any more.
	maxLUN = msGetMaxLun();

//	msResult = msReportLUNs(&maxLUN);
//println("maxLUN = ");
//println(maxLUN);
//	delay(150);
	//-------------------------------------------------------
	msResult = msStartStopUnit(1);
	msResult = WaitMediaReady();
	if(msResult)
		return msResult;
		
	// Retrieve drive information.
	msDriveInfo.initialized = true;
	msDriveInfo.hubNumber = getHubNumber();			// Which HUB.
	msDriveInfo.hubPort = getHubPort();				// Which HUB port.
	msDriveInfo.deviceAddress = getDeviceAddress();	// Device addreess.
	msDriveInfo.idVendor = getIDVendor();  			// USB Vendor ID.
	msDriveInfo.idProduct = getIDProduct();  		// USB Product ID.
	msResult = msDeviceInquiry(&msInquiry);			// Config Info.
	if(msResult)
		return msResult;
	msResult = msReadDeviceCapacity(&msCapacity);	// Size Info.
	if(msResult)
		return msResult;
	memcpy(&msDriveInfo.inquiry, &msInquiry, sizeof(msInquiryResponse_t));
	memcpy(&msDriveInfo.capacity, &msCapacity, sizeof(msSCSICapacity_t));
	return msResult;
}

//---------------------------------------------------------------------------
// Perform Mass Storage Reset
void USBDrive::msReset(void) {
#ifdef DBGprint
	println("msReset()");
#endif
	DBGPrintf(">>msReset()\n"); DBGFlush();
	mk_setup(setup, 0x21, 0xff, 0, bInterfaceNumber, 0);
	queue_Control_Transfer(device, &setup, NULL, this);
	while (!msControlCompleted) yield();
	msControlCompleted = false;
}

//---------------------------------------------------------------------------
// Get MAX LUN
uint8_t USBDrive::msGetMaxLun(void) {
#ifdef DBGprint
	println("msGetMaxLun()");
#endif
	report[0] = 0;
	mk_setup(setup, 0xa1, 0xfe, 0, bInterfaceNumber, 1);
	queue_Control_Transfer(device, &setup, report, this);
	while (!msControlCompleted) yield();
	msControlCompleted = false;
	maxLUN = report[0];
	return maxLUN;
}

uint8_t USBDrive::WaitMediaReady() {
	uint8_t msResult;
	uint32_t start = millis();
#ifdef DBGprint
	println("WaitMediaReady()");
#endif
	do {
		if((millis() - start) >= MEDIA_READY_TIMEOUT) {
			return MS_UNIT_NOT_READY;  // Not Ready Error.
		}
		msResult = msTestReady();
		yield();
	 } while(msResult == 1);
	return msResult;
}

// Check if drive is connected and Initialized.
uint8_t USBDrive::checkConnectedInitialized(void) {
	uint8_t msResult = MS_CBW_PASS;
#ifdef DBGprint
	print("checkConnectedInitialized()");
#endif
	if(!msDriveInfo.connected) {
		return MS_NO_MEDIA_ERR;
	}
	if(!msDriveInfo.initialized) {
		msResult = mscInit();
		if(msResult != MS_CBW_PASS) return MS_UNIT_NOT_READY; // Not Initialized
	}
	return MS_CBW_PASS;
}

//---------------------------------------------------------------------------
// Send SCSI Command
// Do a complete 3 stage transfer.
uint8_t USBDrive::msDoCommand(msCommandBlockWrapper_t *CBW,	void *buffer)
{
	uint8_t CSWResult = 0;
	mscTransferComplete = false;
#ifdef DBGprint
	println("msDoCommand()");
#endif	
	if(CBWTag == 0xFFFFFFFF) CBWTag = 1;
	// digitalWriteFast(2, HIGH);
	queue_Data_Transfer(datapipeOut, CBW, sizeof(msCommandBlockWrapper_t), this); // Command stage.
	while(!msOutCompleted) yield();
	// digitalWriteFast(2, LOW);
	msOutCompleted = false;
	if((CBW->Flags == CMD_DIR_DATA_IN)) { // Data stage from device.
		queue_Data_Transfer(datapipeIn, buffer, CBW->TransferLength, this);
	while(!msInCompleted) yield();
	// digitalWriteFast(2, HIGH);
	msInCompleted = false;
	} else {							  // Data stage to device.
		queue_Data_Transfer(datapipeOut, buffer, CBW->TransferLength, this);
	while(!msOutCompleted) yield();
	// digitalWriteFast(2, LOW);
	msOutCompleted = false;
	}
	CSWResult = msGetCSW(); // Status stage.
	// All stages of this transfer have completed.
	//Check for special cases. 
	//If test for unit ready command is given then
	//  return the CSW status byte.
	//Bit 0 == 1 == not ready else
	//Bit 0 == 0 == ready.
	//And the Start/Stop Unit command as well.
	if((CBW->CommandData[0] == CMD_TEST_UNIT_READY) ||
	   (CBW->CommandData[0] == CMD_START_STOP_UNIT))
		return CSWResult;
	else // Process possible SCSI errors.
		return msProcessError(CSWResult);
}

//---------------------------------------------------------------------------
// Get Command Status Wrapper
uint8_t USBDrive::msGetCSW(void) {
#ifdef DBGprint
	println("msGetCSW()");
#endif
	msCommandStatusWrapper_t StatusBlockWrapper = (msCommandStatusWrapper_t)
	{
		.Signature = CSW_SIGNATURE,
		.Tag = 0,
		.DataResidue = 0, // TODO: Proccess this if received.
		.Status = 0
	};
	queue_Data_Transfer(datapipeIn, &StatusBlockWrapper, sizeof(StatusBlockWrapper), this);
	while(!msInCompleted) yield();
	msInCompleted = false;
	mscTransferComplete = true;
	if(StatusBlockWrapper.Signature != CSW_SIGNATURE) return msProcessError(MS_CSW_SIG_ERROR); // Signature error
	if(StatusBlockWrapper.Tag != CBWTag) return msProcessError(MS_CSW_TAG_ERROR); // Tag mismatch error
	return StatusBlockWrapper.Status;
}

//---------------------------------------------------------------------------
// Test Unit Ready
uint8_t USBDrive::msTestReady() {
#ifdef DBGprint
	println("msTestReady()");
#endif
	msCommandBlockWrapper_t CommandBlockWrapper = (msCommandBlockWrapper_t)
	{
		.Signature          = CBW_SIGNATURE,
		.Tag                = ++CBWTag,
		.TransferLength     = 0,
		.Flags              = CMD_DIR_DATA_IN,
		.LUN                = currentLUN,
		.CommandLength      = 6,
		.CommandData        = {CMD_TEST_UNIT_READY, 0x00, 0x00, 0x00, 0x00, 0x00}
	};
	queue_Data_Transfer(datapipeOut, &CommandBlockWrapper, sizeof(CommandBlockWrapper), this);
	while(!msOutCompleted) yield();
	msOutCompleted = false;
	return msGetCSW();
}

//---------------------------------------------------------------------------
// Start/Stop unit
uint8_t USBDrive::msStartStopUnit(uint8_t mode) {
#ifdef DBGprint
	println("msStartStopUnit()");
#endif
	msCommandBlockWrapper_t CommandBlockWrapper = (msCommandBlockWrapper_t)
	{
		.Signature          = CBW_SIGNATURE,
		.Tag                = ++CBWTag,
		.TransferLength     = 0,
		.Flags              = CMD_DIR_DATA_IN,
		.LUN                = currentLUN,
		.CommandLength      = 6,
		.CommandData        = {CMD_START_STOP_UNIT, 0x01, 0x00, 0x00, mode, 0x00}
	};
	queue_Data_Transfer(datapipeOut, &CommandBlockWrapper, sizeof(CommandBlockWrapper), this);
	while(!msOutCompleted) yield();
	msOutCompleted = false;
	return msGetCSW();
}

//---------------------------------------------------------------------------
// Read Mass Storage Device Capacity (Number of Blocks and Block Size)
uint8_t USBDrive::msReadDeviceCapacity(msSCSICapacity_t * const Capacity) {
#ifdef DBGprint
	println("msReadDeviceCapacity()");
#endif
	uint8_t result = 0;
	msCommandBlockWrapper_t CommandBlockWrapper = (msCommandBlockWrapper_t)
	{
		.Signature          = CBW_SIGNATURE,
		.Tag                = ++CBWTag,
		.TransferLength     = sizeof(msSCSICapacity_t),
		.Flags              = CMD_DIR_DATA_IN,
		.LUN                = currentLUN,
		.CommandLength      = 10,
		.CommandData        = {CMD_RD_CAPACITY_10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}
	};
	result = msDoCommand(&CommandBlockWrapper, Capacity);
	Capacity->Blocks = swap32(Capacity->Blocks);
	Capacity->BlockSize = swap32(Capacity->BlockSize);
	return result;
}

//---------------------------------------------------------------------------
// Do Mass Storage Device Inquiry
uint8_t USBDrive::msDeviceInquiry(msInquiryResponse_t * const Inquiry)
{
#ifdef DBGprint
	println("msDeviceInquiry()");
#endif
	msCommandBlockWrapper_t CommandBlockWrapper = (msCommandBlockWrapper_t)
	{
		.Signature          = CBW_SIGNATURE,
		.Tag                = ++CBWTag,
		.TransferLength     = sizeof(msInquiryResponse_t),
		.Flags              = CMD_DIR_DATA_IN,
		.LUN                = currentLUN,
		.CommandLength      = 6,
		.CommandData        = {CMD_INQUIRY,0x00,0x00,0x00,sizeof(msInquiryResponse_t),0x00}
	};
	return msDoCommand(&CommandBlockWrapper, Inquiry);
}

//---------------------------------------------------------------------------
// Request Sense Data
uint8_t USBDrive::msRequestSense(msRequestSenseResponse_t * const Sense)
{
#ifdef DBGprint
	println("msRequestSense()");
#endif
	msCommandBlockWrapper_t CommandBlockWrapper = (msCommandBlockWrapper_t)
	{
		.Signature          = CBW_SIGNATURE,
		.Tag                = ++CBWTag,
		.TransferLength     = sizeof(msRequestSenseResponse_t),
		.Flags              = CMD_DIR_DATA_IN,
		.LUN                = currentLUN,
		.CommandLength      = 6,
		.CommandData        = {CMD_REQUEST_SENSE, 0x00, 0x00, 0x00, sizeof(msRequestSenseResponse_t), 0x00}
	};
	return msDoCommand(&CommandBlockWrapper, Sense);
}

//---------------------------------------------------------------------------
// Report LUNs
uint8_t USBDrive::msReportLUNs(uint8_t *Buffer)
{
#ifdef DBGprint
	println("msReportLuns()");
#endif
	msCommandBlockWrapper_t CommandBlockWrapper = (msCommandBlockWrapper_t)
	{
		.Signature          = CBW_SIGNATURE,
		.Tag                = ++CBWTag,
		.TransferLength     = MAXLUNS,
		.Flags              = CMD_DIR_DATA_IN,
		.LUN                = currentLUN,
		.CommandLength      = 12,
		.CommandData        = {CMD_REPORT_LUNS, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, MAXLUNS, 0x00, 0x00}
	};
	return msDoCommand(&CommandBlockWrapper, Buffer);
}


//---------------------------------------------------------------------------
// Read Sectors (Multi Sector Capable)
uint8_t USBDrive::msReadBlocks(
									const uint32_t BlockAddress,
									const uint16_t Blocks,
									const uint16_t BlockSize,
									void * sectorBuffer)
	{
	println("msReadBlocks()");
#if defined(DBGprint) && (DBGprint > 1)
	Serial.printf("<<< msReadBlocks(%x %x %x)\n", BlockAddress, Blocks, BlockSize);
#endif
	uint8_t BlockHi = (Blocks >> 8) & 0xFF;
	uint8_t BlockLo = Blocks & 0xFF;
	msCommandBlockWrapper_t CommandBlockWrapper = (msCommandBlockWrapper_t)
	{
		.Signature          = CBW_SIGNATURE,
		.Tag                = ++CBWTag,
		.TransferLength     = (uint32_t)(Blocks * BlockSize),
		.Flags              = CMD_DIR_DATA_IN,
		.LUN                = currentLUN,
		.CommandLength      = 10,
		.CommandData        = {CMD_RD_10, 0x00,
							  (uint8_t)(BlockAddress >> 24),
							  (uint8_t)(BlockAddress >> 16),
							  (uint8_t)(BlockAddress >> 8),
							  (uint8_t)(BlockAddress & 0xFF),
							   0x00, BlockHi, BlockLo, 0x00}
	};
	#if defined(__IMXRT1062__)
	if ((uint32_t)sectorBuffer >= 0x20200000u)  arm_dcache_flush_delete(sectorBuffer, CommandBlockWrapper.TransferLength);
	#endif
	return msDoCommand(&CommandBlockWrapper, sectorBuffer);
}

//---------------------------------------------------------------------------
// Read Sectors (Multi Sector Capable)

uint8_t USBDrive::msReadSectorsWithCB(
			const uint32_t BlockAddress,
			const uint16_t Blocks,
			void (*callback)(uint32_t, uint8_t *),
			uint32_t token)
	{
#if defined(DBGprint) && (DBGprint > 1)
	Serial.printf("<<< msReadSectorsWithCB(%x %u %x)\n", BlockAddress, Blocks, (uint32_t)callback);
#endif
	if ((callback == nullptr) || (!Blocks)) return MS_CBW_FAIL;

	uint8_t BlockHi = (Blocks >> 8) & 0xFF;
	uint8_t BlockLo = Blocks & 0xFF;
	static const uint16_t BlockSize = 512;

	msCommandBlockWrapper_t CommandBlockWrapper = (msCommandBlockWrapper_t)
	{
	
		.Signature          = CBW_SIGNATURE,
		.Tag                = ++CBWTag,
		.TransferLength     = (uint32_t)(Blocks * BlockSize),
		.Flags              = CMD_DIR_DATA_IN,
		.LUN                = currentLUN,
		.CommandLength      = 10,
		.CommandData        = {CMD_RD_10, 0x00,
					  (uint8_t)(BlockAddress >> 24),
					  (uint8_t)(BlockAddress >> 16),
					  (uint8_t)(BlockAddress >> 8),
					  (uint8_t)(BlockAddress & 0xFF),
					   0x00, BlockHi, BlockLo, 0x00}
	};

	// We need to remember how many blocks and call back function
	_read_sectors_callback = callback;
	_read_sectors_remaining = Blocks;
	_read_sectors_token = token;
	_emlastRead = 0; // reset the timeout. 

	// lets unwrap the msDoCommand here...
	uint8_t CSWResult = 0;
	mscTransferComplete = false;

	if(CBWTag == 0xFFFFFFFF) CBWTag = 1;
	// digitalWriteFast(2, HIGH);
	queue_Data_Transfer(datapipeOut, &CommandBlockWrapper, sizeof(msCommandBlockWrapper_t), this); // Command stage.

	while(!msOutCompleted && (_emlastRead < READ_CALLBACK_TIMEOUT_MS)) yield();
	// digitalWriteFast(2, LOW);

	msOutCompleted = false;

	queue_Data_Transfer(datapipeIn, _read_sector_buffer1, BlockSize, this);
	if (_read_sectors_remaining > 1) {
		queue_Data_Transfer(datapipeIn, _read_sector_buffer2, BlockSize, this);
	}

	while(!msInCompleted && (_emlastRead < READ_CALLBACK_TIMEOUT_MS)) ;
	// digitalWriteFast(2, HIGH);

	if (!msInCompleted) {
		// clear this out..
		#ifdef DBGprint
			Serial.printf("!!! msReadBlocks Timed Out(%u)\n", _read_sectors_remaining);
		#endif
		_read_sectors_callback = nullptr;
		_read_sectors_remaining = 0;
		return MS_CBW_FAIL;
	}	

	msInCompleted = false;

	CSWResult = msGetCSW(); // Status stage.
	#ifdef DBGprint
	Serial.printf("  CSWResult: %x CD:%x\n", CSWResult, CommandBlockWrapper.CommandData[0] );
	#endif
	// All stages of this transfer have completed.
	//Check for special cases. 
	//If test for unit ready command is given then
	//  return the CSW status byte.
	//Bit 0 == 1 == not ready else
	//Bit 0 == 0 == ready.
	//And the Start/Stop Unit command as well.
	if((CommandBlockWrapper.CommandData[0] == CMD_TEST_UNIT_READY) ||
	   (CommandBlockWrapper.CommandData[0] == CMD_START_STOP_UNIT))
		return CSWResult;
	return msProcessError(CSWResult);
}


//---------------------------------------------------------------------------
// Write Sectors (Multi Sector Capable)
uint8_t USBDrive::msWriteBlocks(
                                  const uint32_t BlockAddress,
                                  const uint16_t Blocks,
                                  const uint16_t BlockSize,
								  const void * sectorBuffer)
	{
#ifdef DBGprint
	println("msWriteBlocks()");
#endif
	uint8_t BlockHi = (Blocks >> 8) & 0xFF;
	uint8_t BlockLo = Blocks & 0xFF;
	msCommandBlockWrapper_t CommandBlockWrapper = (msCommandBlockWrapper_t)
	{
		.Signature          = CBW_SIGNATURE,
		.Tag                = ++CBWTag,
		.TransferLength     = (uint32_t)(Blocks * BlockSize),
		.Flags              = CMD_DIR_DATA_OUT,
		.LUN                = currentLUN,
		.CommandLength      = 10,
		.CommandData        = {CMD_WR_10, 0x00,
		                      (uint8_t)(BlockAddress >> 24),
							  (uint8_t)(BlockAddress >> 16),
							  (uint8_t)(BlockAddress >> 8),
							  (uint8_t)(BlockAddress & 0xFF),
							  0x00, BlockHi, BlockLo, 0x00}
	};
	#if defined(__IMXRT1062__)
	if ((uint32_t)sectorBuffer >= 0x20200000u)  arm_dcache_flush((void*)sectorBuffer, CommandBlockWrapper.TransferLength);
	#endif
	return msDoCommand(&CommandBlockWrapper, (void *)sectorBuffer);
}

// Proccess Possible SCSI errors
uint8_t USBDrive::msProcessError(uint8_t msStatus) {
#ifdef DBGprint
	println("msProcessError()");
#endif
	uint8_t msResult = 0;
	switch(msStatus) {
		case MS_CBW_PASS:
			return MS_CBW_PASS;
			break;
		case MS_CBW_PHASE_ERROR:
			print("SCSI Phase Error: ");
			println(msStatus);
			return MS_SCSI_ERROR;
			break;
		case MS_CSW_TAG_ERROR:
			print("CSW Tag Error: ");
			println(MS_CSW_TAG_ERROR);
			return MS_CSW_TAG_ERROR;
			break;
		case MS_CSW_SIG_ERROR:
			print("CSW Signature Error: ");
			println(MS_CSW_SIG_ERROR);
			return MS_CSW_SIG_ERROR;
			break;
		case MS_CBW_FAIL:
			if((msResult = msRequestSense(&msSense))) {
				print("Failed to get sense codes. Returned code: ");
				println(msResult);
			}
			return MS_CBW_FAIL;
			break;
		default:
			print("SCSI Error: ");
			println(msStatus);
			return msStatus;
	}
}



#include "mscSenseKeyList.h"


//==============================================================================

bool USBDrive::begin() {
	m_errorCode = MS_CBW_PASS;
	mscInit(); // Do initial init of each instance of a MSC object.
	m_errorCode = checkConnectedInitialized();
	if (m_errorCode) { // Check for Connected USB drive.
		m_initDone = false;
	} else {
		m_initDone = true;
	}
	return m_initDone;
}

//------------------------------------------------------------------------------
bool USBDrive::readSector(uint32_t sector, uint8_t* dst) {
	return readSectors(sector, dst, 1);
}
//------------------------------------------------------------------------------
bool USBDrive::readSectors(uint32_t sector, uint8_t* dst, size_t n) {
	// Check if device is plugged in and initialized
	m_errorCode = checkConnectedInitialized();
	if (m_errorCode != MS_CBW_PASS) {
		return false;
	}
	m_errorCode = msReadBlocks(sector, n, (uint16_t)msDriveInfo.capacity.BlockSize, dst);
	if (m_errorCode) {
		return false;
	}
	return true;
}

//------------------------------------------------------------------------------
bool USBDrive::readSectorsWithCB(uint32_t sector, size_t ns,
	void (*callback)(uint32_t, uint8_t *), uint32_t token)
{
	// Check if device is plugged in and initialized
	m_errorCode = checkConnectedInitialized();
	if (m_errorCode != MS_CBW_PASS) {
		return false;
	}
	m_errorCode = msReadSectorsWithCB(sector, ns, callback, token);
	if (m_errorCode) {
		return false;
	}
	return true;
}
//------------------------------------------------------------------------------
static void callback_shim(uint32_t token, uint8_t *data)
{
	uint32_t *state = (uint32_t *)token;
	uint32_t sector = state[0];
	void (*callback)(uint32_t, uint8_t *, void *) =
		(void (*)(uint32_t, uint8_t *, void *))(state[1]);
	void *context = (void *)(state[2]);
	callback(sector, data, context);
	state[0]++;
}
bool USBDrive::readSectorsCallback(uint32_t sector, uint8_t* dst, size_t numSectors,
	void (*callback)(uint32_t sector, uint8_t *buf, void *context), void *context)
{
	uint32_t state[3] = {sector, (uint32_t)callback, (uint32_t)context};
	return readSectorsWithCB(sector, numSectors, callback_shim, (uint32_t)state);
}
//------------------------------------------------------------------------------
bool USBDrive::writeSector(uint32_t sector, const uint8_t* src) {
	return writeSectors(sector, src, 1);
}
//------------------------------------------------------------------------------
bool USBDrive::writeSectors(uint32_t sector, const uint8_t* src, size_t n) {
	// Check if device is plugged in and initialized
	m_errorCode = checkConnectedInitialized();
	if (m_errorCode != MS_CBW_PASS) {
		return false;
	}
	m_errorCode = msWriteBlocks(sector, n, (uint16_t)msDriveInfo.capacity.BlockSize, src);
	if (m_errorCode) {
		return false;
	}
	return true;
}


static const char *decodeSenseKey(uint8_t senseKey) {
	static char msg[64];
#undef SENSE_KEY_MAP
	switch (senseKey) {
#define SENSE_KEY_MAP(_name_, _val_) \
		case _val_: return #_name_ ;
		SENSE_KEY_LIST
	}
#undef SENSE_KEY_MAP

	snprintf(msg, sizeof(msg), "UNKNOWN SENSE KEY(%02Xh)", senseKey);
	return msg;
}

static const char *decodeAscAscq(uint8_t asc, uint8_t ascq) {
	static char msg[64];
	uint16_t ascAscq = asc<<8 | ascq;

	switch (ascAscq) {
#define SENSE_CODE_KEYED(_asc_, _fmt_)
#define SENSE_CODE(_asc_, _ascq_, _msg_) case _asc_<<8 | _ascq_: return _msg_;
	ASC_NUM_LIST
#undef SENSE_CODE
#undef SENSE_CODE_KEYED
	}

#define SENSE_CODE_KEYED(_asc_, _fmt_) if (asc == _asc_) { snprintf(msg, sizeof(msg), _fmt_, ascq); return msg; }
#define SENSE_CODE(_asc_, _ascq_, _msg_)
	ASC_NUM_LIST
#undef SENSE_CODE
#undef SENSE_CODE_KEYED

	snprintf(msg, sizeof(msg), "UNKNOWN ASC/ASCQ (%02Xh/%02Xh)", asc, ascq);
	return msg;
}

//------------------------------------------------------------------------------
static void printMscAscError(print_t* pr, USBDrive *pDrive)
{
	Serial.printf(" --> Type: %s Cause: %s\n",
		decodeSenseKey(pDrive->msSense.SenseKey),
		decodeAscAscq(pDrive->msSense.AdditionalSenseCode,
		pDrive->msSense.AdditionalSenseQualifier));

}

#undef print
#undef println

// Print error info and return.
//


void USBDrive::printPartionTable(Print &p) {
  DBGPrintf(">>USBDrive::printPartionTable\n");
  if (!msDriveInfo.initialized) return;
  const uint32_t device_sector_count = msDriveInfo.capacity.Blocks;
  // TODO: check device_sector_count
  MbrSector_t mbr;
  bool gpt_disk = false;
  bool ext_partition;
  uint32_t next_free_sector = 8192;  // Some inital value this is default for Win32 on SD...
  if (!readSector(0, (uint8_t*)&mbr)) {
    p.printf("\nread MBR failed, error code 0x%02X.\n", errorCode());
    return;
  }
  p.print("\nPartition Table\n");
  p.print("\tpart,boot,bgnCHS[3],type,endCHS[3],start,length\n");
  for (uint8_t ip = 1; ip < 5; ip++) {
    MbrPart_t *pt = &mbr.part[ip - 1];
    uint32_t starting_sector = getLe32(pt->relativeSectors);
    uint32_t total_sector = getLe32(pt->totalSectors);
    ext_partition = false;
    if (starting_sector > next_free_sector) {
      p.printf("\t < unused area starting at: %u length %u >\n", next_free_sector, starting_sector-next_free_sector);
    }
    switch (pt->type) {
    case 1:
      p.print("FAT12:\t");
      break;
    case 4:
    case 6:
    case 0xe:
      p.print("FAT16:\t");
      break;
    case 11:
    case 12:
      p.print("FAT32:\t");
      break;
    case 7:
      p.print("exFAT:\t");
      break;
    case 5:
    case 0xf:
      p.print("Extend:\t");
      ext_partition = true;
      break;
    case 0x83:
      p.print("ext2/3/4:\t");
      break;
    case 0xee:
      p.print(F("*** GPT Disk WIP ***\nGPT guard:\t"));
      gpt_disk = true;
      break;
    default:
      p.print("pt_#");
      p.print(pt->type);
      p.print(":\t");
      break;
    }
    p.print( int(ip)); p.print( ',');
    p.print(int(pt->boot), HEX); p.print( ',');
    for (int i = 0; i < 3; i++ ) {
      p.print("0x"); p.print(int(pt->beginCHS[i]), HEX); p.print( ',');
    }
    p.print("0x"); p.print(int(pt->type), HEX); p.print( ',');
    for (int i = 0; i < 3; i++ ) {
      p.print("0x"); p.print(int(pt->endCHS[i]), HEX); p.print( ',');
    }
    p.print(starting_sector, DEC); p.print(',');
    p.println(total_sector);
    if (ext_partition) {
      printExtendedPartition(&mbr, ip, p);
      readSector(0, (uint8_t*)&mbr); // maybe need to restore
    }

    // Lets get the max of start+total
    if (starting_sector && total_sector)  next_free_sector = starting_sector + total_sector;
  }
  if (next_free_sector < device_sector_count) {
    p.printf("\t < unused area starting at: %u length %u >\n",
      next_free_sector, device_sector_count-next_free_sector);
  }
  if (gpt_disk) printGUIDPartitionTable(p);
}

void dump_hexbytes(const void *ptr, int len, Print &pr)
{
  if (ptr == NULL || len <= 0) return;
  const uint8_t *p = (const uint8_t *)ptr;
  while (len > 0) {
    for (uint8_t i = 0; i < 32; i++) {
      if (i > len) break;
      pr.printf("%02X ", p[i]);
    }
    pr.print(":");
    for (uint8_t i = 0; i < 32; i++) {
      if (i > len) break;
      pr.printf("%c", ((p[i] >= ' ') && (p[i] <= '~')) ? p[i] : '.');
    }
    pr.println();
    p += 32;
    len -= 32;
  }
}

void USBDrive::printExtendedPartition(MbrSector_t *mbr, uint8_t ipExt, Print &p) {
  // Extract the data from EX partition block...
  MbrPart_t *pt = &mbr->part[ipExt - 1];
  uint32_t ext_starting_sector = getLe32(pt->relativeSectors);
  //uint32_t ext_total_sector = getLe32(pt->totalSectors);
  uint32_t next_mbr = ext_starting_sector;
  uint8_t ext_index = 0;

  while (next_mbr) {
    ext_index++;
    if (!readSector(next_mbr, (uint8_t*)mbr)) break;
    pt = &mbr->part[0];
    //dump_hexbytes((uint8_t*)pt, sizeof(MbrPart_t)*2, p);
    uint32_t starting_sector = getLe32(pt->relativeSectors);
    uint32_t total_sector = getLe32(pt->totalSectors);
    switch (pt->type) {
    case 1:
      p.print(F("FAT12:\t"));
      break;
    case 4:
    case 6:
    case 0xe:
      p.print(F("FAT16:\t"));
      break;
    case 11:
    case 12:
      p.print(F("FAT32:\t"));
      break;
    case 7:
      p.print(F("exFAT:\t"));
      break;
    case 0xf:
      p.print(F("Extend:\t"));
      break;
    case 0x83:
      p.print(F("ext2/3/4:\t")); break;
    default:
      p.print(F("pt_#"));
      p.print(pt->type);
      p.print(":\t");
      break;
    }
    // TODO: extended partition numbers increment from 5
    p.print( int(ipExt)); p.print(":"); p.print(ext_index); p.print( ',');
    p.print(int(pt->boot), HEX); p.print( ',');
    for (int i = 0; i < 3; i++ ) {
      p.print("0x"); p.print(int(pt->beginCHS[i]), HEX); p.print( ',');
    }
    p.print("0x"); p.print(int(pt->type), HEX); p.print( ',');
    for (int i = 0; i < 3; i++ ) {
      p.print("0x"); p.print(int(pt->endCHS[i]), HEX); p.print( ',');
    }
    p.printf("%u(%u),", next_mbr + starting_sector, starting_sector);
    //p.print(ext_starting_sector + starting_sector, DEC); p.print(',');
    p.print(total_sector);

    // Now lets see what is in the 2nd one...
    pt = &mbr->part[1];
    p.printf(" (%x)\n", pt->type);
    starting_sector = getLe32(pt->relativeSectors);
    if (pt->type && starting_sector) next_mbr = starting_sector + ext_starting_sector;
    else next_mbr = 0;
  }
}

#if 0
typedef struct {
  uint8_t  signature[8];
  uint8_t  revision[4];
  uint8_t  headerSize[4];
  uint8_t  crc32[4];
  uint8_t  reserved[4];
  uint8_t  currentLBA[8];
  uint8_t  backupLBA[8];
  uint8_t  firstLBA[8];
  uint8_t  lastLBA[8];
  uint8_t  diskGUID[16];
  uint8_t  startLBAArray[8];
  uint8_t  numberPartitions[4];
  uint8_t  sizePartitionEntry[4];
  uint8_t  crc32PartitionEntries[4];
  uint8_t  unused[420]; // should be 0;
} GPTPartitionHeader_t;

typedef struct {
  uint8_t  partitionTypeGUID[16];
  uint8_t  uniqueGUID[16];
  uint8_t  firstLBA[8];
  uint8_t  lastLBA[8];
  uint8_t  attributeFlags[8];
  uint16_t name[36];
} GPTPartitionEntryItem_t;

typedef struct {
  GPTPartitionEntryItem_t items[4];
} GPTPartitionEntrySector_t;
#endif

typedef struct {
  uint32_t  q1;
  uint16_t  w2;
  uint16_t  w3;
  uint8_t   b[8];
} guid_t;


void printGUID(uint8_t* pbguid, Print &p) {
  // Windows basic partion guid is: EBD0A0A2-B9E5-4433-87C0-68B6B72699C7
  // raw dump of it: A2 A0 D0 EB E5 B9 33 44 87 C0 68 B6 B7 26 99 C7
  guid_t *pg = (guid_t*)pbguid;
  p.printf("%08X-%04X-%04X-%02X%02X-", pg->q1, pg->w2, pg->w3, pg->b[0], pg->b[1]);
  for (uint8_t i=2;i<8; i++) p.printf("%02X", pg->b[i]);
}

uint32_t USBDrive::printGUIDPartitionTable(Print &Serialx) {
  union {
    MbrSector_t mbr;
    partitionBootSector pbs;
    GPTPartitionHeader_t gpthdr;
    GPTPartitionEntrySector_t gptes;
    uint8_t buffer[512];
  } sector;

  // Lets verify that we are an GPT...
  if (!readSector(0, (uint8_t*)&sector.mbr)) {
    Serialx.print(F("\nread MBR failed.\n"));
    //errorPrint();
    return (uint32_t)-1;
  }
  // verify that the first partition is the guard...
  MbrPart_t *pt = &sector.mbr.part[0];
  if (pt->type != 0xee) {
    Serialx.print(F("\nMBR is not an gpt guard\n"));
    return (uint32_t)-1;
  }

  if (!readSector(1, (uint8_t*)&sector.buffer)) {
    Serialx.print(F("\nread Partition Table Header failed.\n"));
    return (uint32_t)-1;
  }
  // Do quick test for signature:
  if (memcmp(sector.gpthdr.signature, "EFI PART", 8)!= 0) {
    Serialx.println("GPT partition header signature did not match");
    dump_hexbytes(&sector.buffer, 512, Serialx);
  }
  Serialx.printf("\nGPT partition header revision: %x\n", getLe32(sector.gpthdr.revision));
  Serialx.printf("LBAs current:%llu backup:%llu first:%llu last:%llu\nDisk GUID:",
    getLe64(sector.gpthdr.currentLBA), getLe64(sector.gpthdr.backupLBA),
    getLe64(sector.gpthdr.firstLBA), getLe64(sector.gpthdr.lastLBA));
  printGUID(sector.gpthdr.diskGUID, Serialx);

  //dump_hexbytes(&sector.gpthdr.diskGUID, 16);
  uint32_t cParts = getLe32(sector.gpthdr.numberPartitions);
  Serialx.printf("Start LBA Array: %llu Count: %u size:%u\n",
      getLe64(sector.gpthdr.startLBAArray), cParts, getLe32(sector.gpthdr.sizePartitionEntry));
  uint32_t sector_number = 2;
  Serialx.println("Part\t Type Guid, Unique Guid, First, last, attr, name");
  for (uint8_t part = 0; part < cParts ; part +=4) {
    if (readSector(sector_number, (uint8_t*)&sector.buffer)) {
      //dump_hexbytes(&sector.buffer, 512);
      for (uint8_t ipei = 0; ipei < 4; ipei++) {
        GPTPartitionEntryItem_t *pei = &sector.gptes.items[ipei];
        // see if the entry has any data in it...
        uint32_t end_addr = (uint32_t)pei + sizeof(GPTPartitionEntryItem_t);
        uint32_t *p = (uint32_t*)pei;
        for (; (uint32_t)p < end_addr; p++) {
          if (*p) break; // found none-zero.
        }
        if ((uint32_t)p < end_addr) {
          // So entry has data:
          Serialx.printf("%u\t", part + ipei);
          printGUID(pei->partitionTypeGUID, Serialx);
          Serialx.print(", ");
          printGUID(pei->uniqueGUID, Serialx);
          Serialx.printf(", %llu, %llu, %llX, ", getLe64(pei->firstLBA), getLe64(pei->lastLBA),
              getLe64(pei->attributeFlags));
          for (uint8_t i = 0; i < 36; i++) {
            if ((pei->name[i]) == 0) break;
            Serialx.write((uint8_t)pei->name[i]);
          }
          Serialx.println();
          if (memcmp((uint8_t *)pei->partitionTypeGUID, mbdpGuid, 16) == 0) {
            Serialx.print(">>> Microsoft Basic Data Partition\n");
            // See if we can read in the first sector
            if (readSector(getLe64(pei->firstLBA), (uint8_t*)&sector.buffer)) {
              //dump_hexbytes(sector.buffer, 512);

              // First see if this is exFat...
              // which starts with:
              static const uint8_t exfatPBS[] PROGMEM = {0xEB, 0x76, 0x90, //Jmp instruction
                   'E', 'X', 'F', 'A', 'T', ' ', ' ', ' '};
              if (memcmp(sector.buffer, exfatPBS, 11) == 0) {
                Serial.println("    EXFAT:");
              }

            }
            // Bugbug reread that sector...
            readSector(sector_number, (uint8_t*)&sector.buffer);
          }
        }
      }
    }
    sector_number++;
  }
  return 0;
}


//=============================================================================
// FindPartition - 
//=============================================================================
int USBDrive::findPartition(int partition, int &type, uint32_t &firstSector, uint32_t &numSectors, 
							uint32_t &mbrLBA, uint8_t &mbrPart, uint8_t *guid)
{
	if (partition == 0) {
		type = 6; // assume whole drive is FAT16 (SdFat will detect actual format)
		firstSector = 0;
		numSectors = msDriveInfo.capacity.Blocks;
		return MBR_VOL;
	}
	union {
		MbrSector_t mbr;
		partitionBootSector pbs;
		GPTPartitionHeader_t gpthdr;
		GPTPartitionEntrySector_t gptes;
		uint8_t buffer[512];
	} sector;


	partition--;  // zero bias it. 
	if (!readSector(0, (uint8_t*)&sector.mbr)) return INVALID_VOL;
	MbrPart_t *pt = &sector.mbr.part[0];
	if (pt->type == 0xee) {
		// See if we have already cached number of partitions
		if (_cGPTParts == 0) {
			if (!readSector(1, (uint8_t*)&sector.buffer)) return INVALID_VOL;
	  		_cGPTParts = (int)getLe32(sector.gpthdr.numberPartitions);
	  		DBGPrintf(">>Find Partition GPT cParts=%d\n", _cGPTParts);
		}
		// GUID Partition Table
		//  TODO: should we read sector 1, check # of entries and entry size = 128?
		if (partition >= _cGPTParts) return INVALID_VOL; // ran off end
		mbrLBA = 2 + (partition >> 2);
		mbrPart = partition & 0x3;
		if (!readSector(mbrLBA, (uint8_t*)&sector.mbr)) return INVALID_VOL;

		GPTPartitionEntryItem_t *entry = &sector.gptes.items[mbrPart];
		// if we have an empty item we figure we are done.
        uint32_t *end_addr = (uint32_t*)((uint32_t)entry + sizeof(GPTPartitionEntryItem_t));
        uint32_t *p = (uint32_t*)entry;
        for (; p < end_addr; p++) {
          if (*p) break; // found none-zero.
        }
        
       	if (p < end_addr) {
			uint64_t first64 = getLe64(entry->firstLBA);
			if (first64 > 0x00000000FFFFFFFFull) return INVALID_VOL;
			uint32_t first32 = first64;
			uint64_t last64 = getLe64(entry->lastLBA);
			if (last64 > 0x00000000FFFFFFFFull) return INVALID_VOL;
			uint32_t last32 = last64;
			if (first32 > last32) return INVALID_VOL;
			firstSector = first32;
			numSectors = last32 - first32 + 1;
			// bugbug should be caller that knows which guids they deal with.
			// Not sure if I hould try to remove this yet, or if
			// we may want to extend list of guids if others are found
			// we understatnd
			if (guid) memcpy(guid, entry->partitionTypeGUID, 16);
			type = 6;
			return GPT_VOL;
		}
   		return INVALID_VOL;
	}
	if (partition >= 0 && partition <= 3) {
		// Master Boot Record
		pt = &sector.mbr.part[partition];
        // try quick way through
      	if (((pt->boot == 0) || (pt->boot == 0X80)) && (pt->type != 0) && (pt->type != 0xf)) {
			type = pt->type;
			firstSector = getLe32(pt->relativeSectors);
			numSectors = getLe32(pt->totalSectors);
			mbrLBA = 0;
			mbrPart = partition; // zero based
			return MBR_VOL;
		}
	}

    // So must be extended or invalid.
    uint8_t index_part;
    for (index_part = 0; index_part < 4; index_part++) {
      pt = &sector.mbr.part[index_part];
      if ((pt->boot != 0 && pt->boot != 0X80) || pt->type == 0 || index_part > partition) return INVALID_VOL;
      if (pt->type == 0xf) break;
    }

    if (index_part == 4) return INVALID_VOL; // no extended partition found. 

    // Our partition if it exists is in extended partition. 
    uint32_t next_mbr = getLe32(pt->relativeSectors);
    for(;;) {
	  if (!readSector(next_mbr, (uint8_t*)&sector.mbr)) return INVALID_VOL;

      if (index_part == partition) break; // should be at that entry
      // else we need to see if it points to others...
      pt = &sector.mbr.part[1];
      uint32_t  relSec = getLe32(pt->relativeSectors);
      //Serial.printf("    Check for next: type: %u start:%u\n ", pt->type, volumeStartSector);
      if ((pt->type == 5) && relSec) {
        next_mbr = next_mbr + relSec;
        index_part++; 
      } else return INVALID_VOL;
    }
   
    // If we are here than we should hopefully be at start of segment...
    pt = &sector.mbr.part[0];
	type = pt->type;
	firstSector = getLe32(pt->relativeSectors) + next_mbr;
	numSectors = getLe32(pt->totalSectors);
	mbrLBA = next_mbr;
	mbrPart = 0; // zero based
    return EXT_VOL;
  }



//=============================================================================
// startFilesystems - enumerate all of the partitons of a drive and ask the different
// filesystem objects if they would like to claim the partition. 
// returns - true if our enumeration  has any partitions claimed. 
//=============================================================================
bool USBDrive::startFilesystems()
{
	// first repeat calling findPartition()
	int type;
	uint32_t firstSector;
	uint32_t numSectors;
	int voltype;
	bool file_system_claimed = false;
	uint32_t mbrLBA;
	uint8_t mbrPart;

	uint8_t guid[16];

	DBGPrintf(">> USBDrive::startFilesystems called %p\n", this);

	if (!begin()) { // make sure we are initialized
		DBGPrintf("\t >> begin() failed");
		return false;
	}

	for (int part = 1; ;part++) {
		voltype = findPartition(part, type, firstSector, numSectors, mbrLBA, mbrPart, guid);
		if (voltype == INVALID_VOL) break;
		DBGPrintf("\t>>Partition %d VT:%u T:%U %u %u\n", part, voltype, type, firstSector, numSectors);
		// Now see if there is any file systems that wish to claim this partition.
		USBFSBase *usbfs = USBFSBase::s_first_fs;

		while (usbfs) {
			// If the usbfs is not claimed, try to claim it.
			if ((usbfs->mydevice == nullptr) 
				&& usbfs->claimPartition(this, part, voltype, type, firstSector, numSectors, guid)) break;
			usbfs = usbfs->_next;
		}
		if (usbfs) {
			// Mark it claimed by stashing back link to us in their mydevice
			// and put a link to us 
			usbfs->mydevice = device;
			file_system_claimed = true;
		 	s_connected_filesystems_changed = true;
		}
	}
	_drive_connect_fs_status = USBDRIVE_FS_STARTED;
	return file_system_claimed;
} 


//=============================================================================
// updateConnectedFilesystems()
// Will go through all of the USBDrive objects and see if the status has
// changed since the last call and if so, will call the startFilesystem call
// that will walk the partitions.  
//=============================================================================
bool USBDrive::updateConnectedFilesystems()
{
  // lets chec each of the drives.
  //bool drive_list_changed = false;
  DBGPrintf("USBDrive::updateConnectedFilesystems called\n");
  bool file_system_started = false;
  USBDrive *pdrive = s_first_drive;
  while (pdrive) {
  	if (pdrive->_drive_connect_fs_status == USBDRIVE_CONNECTED) {
		DBGPrintf("\n === Drive %p connected ===\n", pdrive);
		file_system_started |= pdrive->startFilesystems();
		DBGPrintf("\nTry Partition list");

		#ifdef DBGprint
		pdrive->printPartionTable(Serial);
		#endif

	}
	pdrive = pdrive->_next_drive;
  }
  return file_system_started;
}


//=============================================================================
// USBFSBase methods
//=============================================================================
USBFSBase::USBFSBase() {
	_next = NULL;
	if (s_first_fs == NULL) {
		s_first_fs = this;
	} else {
		USBFSBase *last = s_first_fs;
		while (last->_next) last = last->_next;
		last->_next = this;
	}		
}

USBFSBase *USBFSBase::nextFS(USBFSBase *pfs) {
	if (pfs == nullptr) return s_first_fs;
	return pfs->_next;
}


//=============================================================================
// USBFileSystem methods
//=============================================================================

void USBFilesystem::init()
{
}

FLASHMEM
void USBFilesystem::printError(Print &p) {
	const uint8_t err = device->errorCode();
	if (err) {
		if (err == 0x28) {
			p.println(F("No USB drive detected, plugged in?"));
		}
		p.print(F("USB drive error: "));
		p.print(F("0x"));
		p.print(err, HEX);
		p.print(F(",0x"));
		p.print(device->errorData(), HEX);
		printMscAscError(&p, device);
	} else if (!mscfs.fatType()) {
		p.println(F("Check USB drive format."));
	}
}


// We only support a limited number of GUIDS (currently 1)
bool USBFilesystem::check_voltype_guid(int voltype, uint8_t *guid) {
	// Microsoft Basic Data Partition
	DBGPrintf(">>USBFilesystem::check_voltype_guid(%d, %p)\n", voltype, guid);
	if (voltype == USBDrive::GPT_VOL) {
		#ifdef DBGprint
		printGUID(guid, Serial);
		#endif
		if (memcmp(guid, mbdpGuid, 16) == 0) return true;
		DBGPrintf("USBFilesystem - Unsupporteded GUID\n");
		return false;
	}
	return true;
}

void USBFilesystem::end() {
	mscfs.end();
	_state_changed = USBFS_STATE_CHANGE_CONNECTION;
	s_any_fs_changed_state = true;
	device = nullptr;
}


bool USBFilesystem::claimPartition(USBDrive *pdevice, int part,int voltype, int type, uint32_t firstSector, uint32_t numSectors, uint8_t *guid) {
	// May add in some additional stuff
	DBGPrintf("\t>>USBFilesystem::claimPartition %p called ");

	// For GUID file systems only continue if this is a guid to a type we know. 
	if (!check_voltype_guid(voltype, guid)) return false; // not something we understand;

	if (mscfs.begin(pdevice, true, firstSector, numSectors)) {
		device = pdevice;
		partition = part;
		partitionType = type;
		volumeLabel[0] = 0xFF;
		_state_changed = USBFS_STATE_CHANGE_CONNECTION;
		s_any_fs_changed_state = true;
		DBGPrintf("+ Claimed\n");
		return true;		
	}
	DBGPrintf("- Not Claimed\n");
	return false;
}

void USBFilesystem::releasePartition() {
	DBGPrintf("\t USBFilesystem::releasePartition %p called\n");
	end();
}

bool USBFilesystem::format(int type, char progressChar, Print& pr) {
	// setup instance of formatter object;
	uint8_t *buf = (uint8_t *)malloc(512+32);
	if (!buf) return false; // unable to allocate memory
	// lets align the buffer
    uint8_t *aligned_buf = (uint8_t *)(((uintptr_t)buf + 31) & ~((uintptr_t)(31)));
	USBFilesystemFormatter formatter; 
	//Serial.printf("$$call formatter.format(%p, 0, %p %p...)\n", this, buf, aligned_buf);
	bool ret = formatter.format(*this, 0, aligned_buf, &pr);

	free(buf);

	if (ret) {
		pr.println("Format Completed restart filesystem");
		
		// Maybe not call as this may write out dirty stuff.
		//mscfs.end();  // release the old data

		// Now lets try to restart it	
		int type;
		uint32_t firstSector;
		uint32_t numSectors;
		uint32_t mbrLBA;
		uint8_t mbrPart;

		uint8_t guid[16];

		int voltype = device->findPartition(partition, type, firstSector, numSectors, mbrLBA, mbrPart, guid);
		if (voltype == USBDrive::INVALID_VOL) return false;
		pr.printf("\tPart:%d Type:%x First:%u num:%u\n", partition, type, firstSector, numSectors);
		// now lets try to start it again.
		partitionType = type;
		ret = mscfs.begin(device, true, firstSector, numSectors);
		pr.printf("\tbegin return: %u\n", ret);
		volumeLabel[0] = 0xFF;
		_state_changed = USBFS_STATE_CHANGE_FORMAT;
		s_any_fs_changed_state = true;
	}
	return ret;
}
