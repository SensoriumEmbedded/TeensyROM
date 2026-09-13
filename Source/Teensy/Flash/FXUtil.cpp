//******************************************************************************
// FXUTIL.H -- FlasherX utility functions
//******************************************************************************
#ifndef FXUTIL_HOST_TEST
#include <Arduino.h>
extern "C" {
  #include "FlashTxx.h"		// TLC/T3x/T4x/TMM flash primitives
}
#endif

//******************************************************************************
// hex_info_t	struct for hex record and hex file info
//******************************************************************************
typedef struct {	// 
  char *data;		// pointer to array allocated elsewhere
  unsigned int addr;	// address in intel hex record
  unsigned int code;	// intel hex record type (0=data, etc.)
  unsigned int num;	// number of data bytes in intel hex record
 
  uint32_t base;	// base address to be added to intel hex 16-bit addr
  uint32_t min;		// min address in hex file
  uint32_t max;		// max address in hex file
  
  int eof;		// set true on intel hex EOF (code = 1)
  int lines;		// number of hex records received  
} hex_info_t;


//******************************************************************************
// hex_info_t	struct for hex record and hex file info
//******************************************************************************
int  read_ascii_line( Stream *serial, char *line, int maxbytes );
int  parse_hex_line( const char *theline, char *bytes,
	unsigned int *addr, unsigned int *num, unsigned int *code );
int  process_hex_record( hex_info_t *hex );
void update_firmware( Stream *in, Stream *out,
			uint32_t buffer_addr, uint32_t buffer_size,
			uint32_t expected_crc, bool verify_crc );
static int read_ascii_line_crc( Stream *serial, char *line, int maxbytes, uint32_t *crc );

static uint32_t firmware_crc32_byte(uint32_t value, uint8_t byte)
{
  // A 16-entry table is much faster than the old bit-at-a-time loop without
  // spending a kilobyte of flash on a full 256-entry table.
  static const uint32_t table[16] = {
    0x00000000u,0x1db71064u,0x3b6e20c8u,0x26d930acu,
    0x76dc4190u,0x6b6b51f4u,0x4db26158u,0x5005713cu,
    0xedb88320u,0xf00f9344u,0xd6d6a3e8u,0xcb61b38cu,
    0x9b64c2b0u,0x86d3d2d4u,0xa00ae278u,0xbdbdf21cu
  };
  value ^= byte;
  value = (value >> 4) ^ table[value & 15];
  return (value >> 4) ^ table[value & 15];
}

//******************************************************************************
// update_firmware()	read hex file and write new firmware to program flash
//******************************************************************************
void update_firmware( Stream *in, Stream *out, 
				uint32_t buffer_addr, uint32_t buffer_size,
				uint32_t expected_crc, bool verify_crc )
{
  static char line[524];					// maximum Intel HEX record plus terminator
  static char data[256] __attribute__ ((aligned (8)));	// maximum record payload
  hex_info_t hex = {					// intel hex info struct
    data, 0, 0, 0,					//   data,addr,num,code
    0, 0xFFFFFFFF, 0, 					//   base,min,max,
    0, 0						//   eof,lines
  };

  bool has_data = false;
  uint32_t file_crc = UINT32_MAX;
  (void)out;
  SendMsgPrintfln("Reading hex file");  

  const uint64_t flash_limit = (uint64_t)FLASH_BASE_ADDR + buffer_size;
  const uint64_t buffer_limit = (uint64_t)buffer_addr + buffer_size;
  if (!in || !buffer_size || flash_limit > (uint64_t)UINT32_MAX + 1u ||
      buffer_limit > (uint64_t)UINT32_MAX + 1u) {
    SendMsgPrintfln("Invalid firmware buffer range");
    return;
  }

  // read and process intel hex lines until EOF or error
  while (!hex.eof)  {
    const int line_length = read_ascii_line_crc( in, line, sizeof(line),
      verify_crc ? &file_crc : NULL );
    if (line_length == -1) {
      SendMsgPrintfln("Unexpected end of hex file");
      return;
    }
    if (line_length == -2) {
      SendMsgPrintfln("Hex line too long");
      return;
    }
    //// reliability of transfer via USB is improved by this printf/flush
    //if (in == out && out == (Stream*)&Serial) {
    //  out->printf( "%s\n", line );
    //  out->flush();
    //}

    if (parse_hex_line( (const char*)line, hex.data, &hex.addr, &hex.num, &hex.code ) == 0) 
    {
      //out->printf( "abort - bad hex line %s\n", line );
      SendMsgPrintfln("Bad hex line: %s", line);
      return;
    }
    else if (hex.code == 0) {
      const uint64_t source = (uint64_t)hex.base + hex.addr;
      const uint64_t source_end = source + hex.num;
      const uint64_t destination = (uint64_t)buffer_addr + source - FLASH_BASE_ADDR;
      const uint64_t destination_end = destination + hex.num;
      if (source < FLASH_BASE_ADDR || source_end < source || source_end > flash_limit ||
          destination < buffer_addr || destination_end < destination || destination_end > buffer_limit) {
        SendMsgPrintfln("Firmware address out of range");
        return;
      }
      if (process_hex_record( &hex ) != 0) {
        SendMsgPrintfln("Invalid hex record");
        return;
      }
      has_data = has_data || hex.num != 0;
      const uint32_t addr = (uint32_t)destination;
      if (!IN_FLASH(buffer_addr)) {
        memcpy( (void*)(uintptr_t)addr, (void*)hex.data, hex.num );
      }
      else {
        const int error = flash_write_block( addr, hex.data, hex.num );
        if (error) {
          SendMsgPrintfln("Error %02X in flash_write_block", error);
          return;
        }
      }
    }
    else if (process_hex_record( &hex ) != 0)
    { // error on bad hex code
      //out->printf( "abort - invalid hex code %d\n", hex.code );
      SendMsgPrintfln("Invalid hex code %d", hex.code);
      return;
    }
    hex.lines++;
  }

  // The EOF record must be the logical end of the file. Silently accepting a
  // second image or damaged tail would make the confirmed file ambiguous.
  while (in->available()) {
    const int c = in->read();
    if (c < 0) break;
    if (verify_crc) file_crc = firmware_crc32_byte(file_crc,(uint8_t)c);
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
      SendMsgPrintfln("Data after hex EOF record");
      return;
    }
  }
  if (!has_data || hex.min == 0xFFFFFFFFu || hex.max <= FLASH_BASE_ADDR) {
    SendMsgPrintfln("Hex file contains no firmware data");
    return;
  }
  // flash_move() copies from FLASH_BASE_ADDR through the highest record. A
  // sparse image may contain erased gaps, but it must explicitly provide the
  // boot/vector prefix at the flash base.
  if (hex.min != FLASH_BASE_ADDR) {
    SendMsgPrintfln("Firmware image does not start at flash base");
    return;
  }
  if (verify_crc && ~file_crc != expected_crc) {
    SendMsgPrintfln("Firmware changed after confirmation");
    return;
  }

  const uint32_t image_size = hex.max - FLASH_BASE_ADDR;
    
  SendMsgPrintfln("Hex file: %1d lines, %1luK\r\n(%08lX - %08lX)",
			hex.lines, (hex.max-hex.min)/1024, hex.min, hex.max );

  // check FSEC value in new code -- abort if incorrect
  //#if defined(KINETISK) || defined(KINETISL)
  //uint32_t value = *(uint32_t *)(0x40C + buffer_addr);
  //if (value == 0xfffff9de) {
  //  out->printf( "new code contains correct FSEC value %08lX\n", value );
  //}
  //else {
  //  out->printf( "abort - FSEC value %08lX should be FFFFF9DE\n", value );
  //  return;
  //} 
  //#endif

  // check FLASH_ID in new code - abort if not found
#ifdef Fab04_Features
  SendMsgPrintfln("Verify file for TeensyROM+: ");  //27 chars
#else
  SendMsgPrintfln("Verify file for TeensyROM: ");  //27 chars
#endif
  // The upstream search subtracts strlen(FLASH_ID) from this unsigned size.
  // Guard short images here so malformed but checksummed input cannot
  // underflow that bound and scan outside the staging buffer.
  if (image_size >= strlen(FLASH_ID) && check_flash_id( buffer_addr, image_size, FLASH_ID ))
  {
    //out->printf( "new code contains correct target ID %s\n", FLASH_ID );
    SendMsgOK();
  }
  else 
  {
    //out->printf( "abort - new code missing string %s\n", FLASH_ID );
#ifndef Fab04_Features
    if (image_size >= strlen(FLASH_ID_ORIG) && check_flash_id( buffer_addr, image_size, FLASH_ID_ORIG ))
    {
       if (isFab2x())  //fab 0.2x boards can load older FLASH_ID_ORIG
       {
          SendMsgPrintf("Fab 0.2x OK");  //11 chars
       }
       else
       {
          SendMsgPrintfln("File is for Fab 0.2x TR only!");  
          return;
       }
    }
    else
#endif
    {
       SendMsgFailed();
       return;
    }
  }
  
  // get user input to write to flash or abort
  //int user_lines = -1;
  //while (user_lines != hex.lines && user_lines != 0) {
  //  out->printf( "enter %d to flash or 0 to abort\n", hex.lines );
  //  read_ascii_line( out, line, sizeof(line) );
  //  sscanf( line, "%d", &user_lines );
  //}
  //
  //if (user_lines == 0) {
  //  out->printf( "abort - user entered 0 lines\n" );
  //  return;
  //}
  //else {
  SendMsgPrintfln("Copying Buffer over main Flash area\r\n");

  //  out->printf( "calling flash_move() to load new firmware...\n" );
  //  out->flush();
  //}
  
  // move new program from buffer to flash, free buffer, and reboot
  //will run out of RAM to copy flash, disable interrupts
  
  detachInterrupt(digitalPinToInterrupt(Menu_Btn_In_PIN));
  detachInterrupt(digitalPinToInterrupt(PHI2_PIN));
  NVIC_DISABLE_IRQ(IRQ_ENET); 
  NVIC_DISABLE_IRQ(IRQ_PIT);
  //SetResetAssert;
  
  flash_move( FLASH_BASE_ADDR, buffer_addr, image_size );

  // should not return from flash_move(), but put REBOOT here as reminder
  REBOOT;
}

//******************************************************************************
// read_ascii_line()	read ascii characters until '\n', '\r', or max bytes
//******************************************************************************
int read_ascii_line( Stream *serial, char *line, int maxbytes )
{
  return read_ascii_line_crc(serial,line,maxbytes,NULL);
}

static int read_ascii_line_crc( Stream *serial, char *line, int maxbytes, uint32_t *crc )
{
  if (line && maxbytes > 0) line[0] = 0;
  if (!line || maxbytes < 2 || !serial) return -1;
  int nchar = 0;
  bool started = false, overflow = false;
  for (;;) {
    // Firmware files are synchronous streams. Treat no available byte as EOF
    // instead of spinning forever after removal or a truncated final line.
    if (!serial->available()) {
      line[nchar] = 0;
      if (!started) return -1;
      return overflow ? -2 : nchar;
    }
    const int c = serial->read();
    if (c < 0) {
      line[nchar] = 0;
      if (!started) return -1;
      return overflow ? -2 : nchar;
    }
    if (crc) *crc = firmware_crc32_byte(*crc,(uint8_t)c);
    if (c == '\n' || c == '\r') {
      if (!started) continue;
      line[nchar] = 0;
      return overflow ? -2 : nchar;
    }
    started = true;
    if (nchar + 1 < maxbytes) line[nchar++] = (char)c;
    else overflow = true;
  }
}

//******************************************************************************
// process_hex_record()		process record and return okay (0) or error (1)
//******************************************************************************
int process_hex_record( hex_info_t *hex )
{
  if (hex->code==0) { // data -- update min/max address so far
    if (hex->base + hex->addr + hex->num > hex->max)
      hex->max = hex->base + hex->addr + hex->num;
    if (hex->base + hex->addr < hex->min)
      hex->min = hex->base + hex->addr;
  }
  else if (hex->code==1) { // EOF (:flash command not received yet)
    if (hex->num != 0 || hex->addr != 0) return 1;
    hex->eof = 1;
  }
  else if (hex->code==2) { // extended segment address (top 16 of 24-bit addr)
    if (hex->num != 2 || hex->addr != 0) return 1;
    hex->base = ((((uint8_t)hex->data[0] << 8) | (uint8_t)hex->data[1]) << 4);
  }
  else if (hex->code==3) { // start segment address (80x86 real mode only)
    return 1;
  }
  else if (hex->code==4) { // extended linear address (top 16 of 32-bit addr)
    if (hex->num != 2 || hex->addr != 0) return 1;
    hex->base = ((uint32_t)(((uint8_t)hex->data[0] << 8) | (uint8_t)hex->data[1])) << 16;
  }
  else if (hex->code==5) { // start linear address (32-bit big endian addr)
    // This is an execution entry point, not a new base for later data records.
    if (hex->num != 4 || hex->addr != 0) return 1;
  }
  else {
    return 1;
  }

  return 0;
}

//******************************************************************************
// Intel Hex record format:
//
// Start code:  one character, ASCII colon ':'.
// Byte count:  two hex digits, number of bytes (hex digit pairs) in data field.
// Address:     four hex digits
// Record type: two hex digits, 00 to 05, defining the meaning of the data field.
// Data:        n bytes of data represented by 2n hex digits.
// Checksum:    two hex digits, computed value used to verify record has no errors.
//
// Examples:
//  :10 9D30 00 711F0000AD38000005390000F5460000 35
//  :04 9D40 00 01480000 D6
//  :00 0000 01 FF
//******************************************************************************

/* Intel HEX read/write functions, Paul Stoffregen, paul@ece.orst.edu */
/* This code is in the public domain.  Please retain my name and */
/* email address in distributed copies, and let me know about any bugs */

/* I, Paul Stoffregen, give no warranty, expressed or implied for */
/* this software and/or documentation provided, including, without */
/* limitation, warranty of merchantability and fitness for a */
/* particular purpose. */

// type modifications by Jon Zeeff

/* parses a line of intel hex code, stores the data in bytes[] */
/* and the beginning address in addr, and returns a 1 if the */
/* line was valid, or a 0 if an error occurred.  The variable */
/* num gets the number of bytes that were stored into bytes[] */

#include <stdio.h>		// sscanf(), etc.
#include <string.h>		// strlen(), etc.

int parse_hex_line( const char *theline, char *bytes, 
		unsigned int *addr, unsigned int *num, unsigned int *code )
{
  unsigned sum, len, cksum;
  const char *ptr;
  int temp;

  *num = 0;
  if (theline[0] != ':')
    return 0;
  if (strlen (theline) < 11)
    return 0;
  // scanf field widths are maximums, not exact widths: "%02x" accepts a
  // single nibble before a non-hex character. Reject every malformed nibble
  // before parsing any fixed-width Intel HEX field.
  for (const char *scan = theline + 1; *scan; ++scan) {
    const char c = *scan;
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') ||
          (c >= 'a' && c <= 'f'))) return 0;
  }
  ptr = theline + 1;
  if (!sscanf (ptr, "%02x", &len))
    return 0;
  ptr += 2;
  if (strlen (theline) != (11 + (len * 2)))
    return 0;
  if (!sscanf (ptr, "%04x", (unsigned int *)addr))
    return 0;
  ptr += 4;
  /* Serial.printf("Line: length=%d Addr=%d\n", len, *addr); */
  if (!sscanf (ptr, "%02x", code))
    return 0;
  ptr += 2;
  sum = (len & 255) + ((*addr >> 8) & 255) + (*addr & 255) + (*code & 255);
  while (*num != len)
  {
    if (!sscanf (ptr, "%02x", &temp))
      return 0;
    bytes[*num] = temp;
    ptr += 2;
    sum += bytes[*num] & 255;
    (*num)++;
    if (*num >= 256)
      return 0;
  }
  if (!sscanf (ptr, "%02x", &cksum))
    return 0;

  if (((sum & 255) + (cksum & 255)) & 255)
    return 0;     /* checksum error */
  return 1;
}
