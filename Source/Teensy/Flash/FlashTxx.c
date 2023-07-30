//******************************************************************************
// Flash write/erase functions (TLC/T3x/T4x/TMM), LMEM cache functions for T3.6
//******************************************************************************
// WARNING:  you can destroy your MCU with flash erase or write!
// This code may or may not protect you from that.
//
// Original by Niels A. Moseley, 2015.
// Modifications for OTA updates by Jon Zeeff, Deb Hollenback
// Paul Stoffregen's T4.x flash routines from Teensy4 core added by Jon Zeeff
// Frank Boesing's T3.x flash routines adapted for OTA by Joe Pasquariello
// This code is released into the public domain.
//******************************************************************************

// [<------- code ------->][<--------- buffer --------->][<-- FLASH_RESERVE -->]
// [<------------------------------ FLASH_SIZE ------------------------------->]
// ^FLASH_BASE_ADDR

#include <Arduino.h>		// Serial, etc. (if used)
#include <malloc.h>		// malloc(), free()
#include <string.h>		// memset()
#include "FlashTxx.h"		// FLASH_BASE_ADDRESS, FLASH_SECTOR_SIZE, etc.

static int leave_interrupts_disabled = 0;

//******************************************************************************
// compute addr/size for firmware buffer and return NO/RAM/FLASH_BUFFER_TYPE
//******************************************************************************
int firmware_buffer_init( uint32_t *buffer_addr, uint32_t *buffer_size )
{
  //#if defined(__MK66FX1M0__)     // for T3.6 only
  //LMEM_EnableCodeCache( false ); // disable LMEM code cache for flash operations
  //#endif
  //
  //#if defined(__IMXRT1062__) && (RAM_BUFFER_SIZE > 0)
  //// attempt to malloc() RAM for buffer and return success or failure
  //*buffer_addr = (uint32_t)malloc( RAM_BUFFER_SIZE );
  //if (*buffer_addr != 0) {
  //  *buffer_size = RAM_BUFFER_SIZE;
  //  memset( (void*)*buffer_addr, 0xFF, *buffer_size ); // 0xFF like erased flash
  //  return( RAM_BUFFER_TYPE );
  //}
  //return( NO_BUFFER_TYPE );
  //#endif

  // buffer will begin at first sector ABOVE code and below FLASH_RESERVE
  // start at bottom of FLASH_RESERVE and work down until non-erased flash found
  //0x60000000 + 0x800000 - 0x4000 - 4 = 607FBFFC (607fc000-4)   original FLASH_RESERVE was 0x4000 (4k)
  //0x60000000 + 0x800000 - 0x40000 - 4 = 607BFFFC (607c0000-4)  increased to 0x40000 (256k)
  *buffer_addr = FLASH_BASE_ADDR + FLASH_SIZE - FLASH_RESERVE - 4;
  
  while (*buffer_addr > 0 && *((uint32_t *)*buffer_addr) == 0xFFFFFFFF) *buffer_addr -= 4;

  *buffer_addr += 4; // first address above code
   
  //finds occupied upper flash locations if lower address comes back too high
  //Serial.printf( "addr limit: %08lX\n", *buffer_addr);
  //uint32_t NextAddr = *buffer_addr;
  //uint32_t MaxAddrs = 100;
  //while (NextAddr > 0 && MaxAddrs > 0)
  //{
  //   if(*((uint32_t *)NextAddr) != 0xFFFFFFFF)
  //   {
  //      Serial.printf(" %08lX: %08lX\n", NextAddr, *((uint32_t *)NextAddr));
  //      MaxAddrs--;
  //   }
  //   NextAddr-=4;
  //}
  
  // increase buffer_addr to next sector boundary (if not on a sector boundary)
  if ((*buffer_addr % FLASH_SECTOR_SIZE) > 0)
    *buffer_addr += FLASH_SECTOR_SIZE - (*buffer_addr % FLASH_SECTOR_SIZE);
 
  *buffer_size = FLASH_BASE_ADDR - *buffer_addr + FLASH_SIZE - FLASH_RESERVE;

  return( FLASH_BUFFER_TYPE );
}

//******************************************************************************
// compute addr/size for firmware buffer and return NO/RAM/FLASH_BUFFER_TYPE
//******************************************************************************
void firmware_buffer_free( uint32_t buffer_addr, uint32_t buffer_size )
{
  if (IN_FLASH(buffer_addr))
    flash_erase_block( buffer_addr, buffer_size );
  else
    free( (void*)buffer_addr );
}

//******************************************************************************
// search buffer for string FLASH_ID to verify code was built for correct TARGET
//******************************************************************************
int check_flash_id( uint32_t buffer, uint32_t size )
{
  for (uint32_t i = buffer; i < buffer + size - strlen(FLASH_ID); ++i) {
    if (strncmp((char *)i, FLASH_ID, strlen(FLASH_ID)) == 0)
      return 1;
  }
  return 0;
}

#if defined(KINETISK) || defined(KINETISL) // T3x or TLC

#include <kinetis.h>

#define FLASH_ALIGN(address,align) (address &= ~(align-1))

#define FCMD_READ_1S_SECTION		(0x01)
#define FCMD_PROGRAM_CHECK		(0x02)
#define FCMD_PROGRAM_LONG_WORD		(0x06)
#define FCMD_PROGRAM_PHRASE		(0x07)
#define FCMD_ERASE_FLASH_SECTOR		(0x09)
#define FCMD_READ_ONCE			(0x41)
#define FCMD_PROGRAM_ONCE		(0x43)

#define FTFL_READ_MARGIN_NORMAL		(0x00)
#define FTFL_READ_MARGIN_USER		(0x01)
#define FTFL_READ_MARGIN_FACTORY	(0x02)

RAMFUNC static void flash_exec( void ) 
{
  __disable_irq();				// disable interrupts
  kinetis_hsrun_disable();			// disable high-speed run
  FTFL_FSTAT = FTFL_FSTAT_CCIF;			// execute!
  while (!(FTFL_FSTAT & FTFL_FSTAT_CCIF)) {;}	// wait for done
  kinetis_hsrun_enable();			// re-enable high-speed run
  if (!leave_interrupts_disabled)		// if OK to enable interrupts
    __enable_irq();				//   re-enable interrupts
}

RAMFUNC static void flash_init_command( uint8_t command, uint32_t address )
{
  // wait for ready, clear error flags, init command and address registers
  while (!(FTFL_FSTAT & FTFL_FSTAT_CCIF)) {;}
  FTFL_FSTAT  = 0x30;
  FTFL_FCCOB0 = command;
  FTFL_FCCOB1 = address >> 16;
  FTFL_FCCOB2 = address >> 8;
  FTFL_FCCOB3 = address >> 0;
}

#if (FLASH_WRITE_SIZE==4) // TLC, T30, T31, T32

//******************************************************************************
// flash_word()		write 4-byte word to flash - must run from ram
//******************************************************************************
// aFSEC = allow FSEC sector      (set aFSEC = true to write in FSEC sector)
// oFSEC = overwrite FSEC value   (set BOTH  = true to write to FSEC address)
RAMFUNC int flash_word( uint32_t address, uint32_t value, int aFSEC, int oFSEC )
{
  FLASH_ALIGN( address, FLASH_WRITE_SIZE );

  if (address == (0x40C & ~(FLASH_SECTOR_SIZE - 1)))
    if (aFSEC == 0)
      return 0;
  if (address == 0x40C)
    if (oFSEC == 0)
      return 0;

  flash_init_command( FCMD_PROGRAM_LONG_WORD, address );

  FTFL_FCCOB4 = value >> 24;
  FTFL_FCCOB5 = value >> 16;
  FTFL_FCCOB6 = value >> 8;
  FTFL_FCCOB7 = value >> 0;

  flash_exec();

  return (FTFL_FSTAT & (FTFL_FSTAT_ACCERR | FTFL_FSTAT_FPVIOL | FTFL_FSTAT_MGSTAT0));
}

#elif (FLASH_WRITE_SIZE==8) // T35, T36

//******************************************************************************
// flash_phrase()	write 8-byte phrase to flash - must run from ram
//******************************************************************************
// aFSEC = allow FSEC sector      (set aFSEC = true to write in FSEC sector)
// oFSEC = overwrite FSEC value   (set BOTH  = true to write to FSEC address)
RAMFUNC int flash_phrase( uint32_t address, uint64_t value, int aFSEC, int oFSEC )
{
  FLASH_ALIGN( address, FLASH_WRITE_SIZE );

  if (address == (0x408 & ~(FLASH_SECTOR_SIZE - 1)))
    if (aFSEC == 0)
      return 0;
  if (address == 0x408)
    if (oFSEC == 0)
      return 0;

  flash_init_command( FCMD_PROGRAM_PHRASE, address );

  FTFL_FCCOB4 = value >> 24;
  FTFL_FCCOB5 = value >> 16;
  FTFL_FCCOB6 = value >> 8;
  FTFL_FCCOB7 = value >> 0;
  
  FTFL_FCCOB8 = value >> 56;
  FTFL_FCCOB9 = value >> 48;
  FTFL_FCCOBA = value >> 40;
  FTFL_FCCOBB = value >> 32;

  flash_exec();

  return (FTFL_FSTAT & (FTFL_FSTAT_ACCERR | FTFL_FSTAT_FPVIOL | FTFL_FSTAT_MGSTAT0));
}

#endif // FLASH_WRITE_SIZE

//******************************************************************************
// flash_erase_sector()		erase sector at address - must run from RAM
//******************************************************************************
// aFSEC = allow FSEC sector      (set aFSEC = true to write in FSEC sector)
RAMFUNC int flash_erase_sector( uint32_t address, int aFSEC )
{
  FLASH_ALIGN( address, FLASH_SECTOR_SIZE );

  if (address == (0x400 & ~(FLASH_SECTOR_SIZE - 1)))
    if (aFSEC == 0)
      return 0;

  flash_init_command( FCMD_ERASE_FLASH_SECTOR, address );

  flash_exec();

  return (FTFL_FSTAT & (FTFL_FSTAT_ACCERR | FTFL_FSTAT_FPVIOL | FTFL_FSTAT_MGSTAT0));
}

//******************************************************************************
// flash_sector_not_erased()	returns 0 if erased and !0 (error) if NOT erased
//******************************************************************************
RAMFUNC int flash_sector_not_erased( uint32_t address )
{
  uint16_t num = (FLASH_SECTOR_SIZE / FLASH_WRITE_SIZE);
  FLASH_ALIGN( address, FLASH_SECTOR_SIZE );
  flash_init_command( FCMD_READ_1S_SECTION, address );

  FTFL_FCCOB4 = num >> 8;
  FTFL_FCCOB5 = num >> 0;
  FTFL_FCCOB6 = FTFL_READ_MARGIN_NORMAL;

  flash_exec();

  return (FTFL_FSTAT & (FTFL_FSTAT_ACCERR | FTFL_FSTAT_FPVIOL | FTFL_FSTAT_MGSTAT0));
}

#elif defined(__IMXRT1062__) // T4.x

//******************************************************************************
// flash_sector_not_erased()	returns 0 if erased and !0 (error) if NOT erased
//******************************************************************************
RAMFUNC int flash_sector_not_erased( uint32_t address )
{
  uint32_t *sector = (uint32_t*)(address & ~(FLASH_SECTOR_SIZE - 1));
  for (int i=0; i<FLASH_SECTOR_SIZE/4; i++) {
    if (*sector++ != 0xFFFFFFFF)
      return 1; // NOT erased
  }
  return 0; // erased
}

#endif // __IMXRT1062__

//******************************************************************************
// move from source to destination (flash), erasing destination sectors as we go
// DANGER: this is critical and cannot be interrupted, else T3.x can be damaged
//******************************************************************************
RAMFUNC void flash_move( uint32_t dst, uint32_t src, uint32_t size )
{
  uint32_t offset=0, error=0, addr;
  
  // set global flag leave_interrupts_disabled = 1 to prevent the T3.x flash
  // write and erase functions from re-enabling interrupts when they complete 
  leave_interrupts_disabled = 1;
  
  // move size bytes containing new program from source to destination
  while (offset < size && error == 0) {

    addr = dst + offset;

    // if new sector, erase, then immediately write FSEC/FOPT if in this sector
    // this is the ONLY place that FSEC values are written, so it's the only
    // place where calls to KINETIS flash write functions have aFSEC = oFSEC = 1
    if ((addr & (FLASH_SECTOR_SIZE - 1)) == 0) {
      if (flash_sector_not_erased( addr )) {
        #if defined(__IMXRT1062__)
          eepromemu_flash_erase_sector( (void *)addr );
        #elif (FLASH_WRITE_SIZE==4)
          error |= flash_erase_sector( addr, 1 );
          if (addr == (0x40C & ~(FLASH_SECTOR_SIZE - 1)))
            error |= flash_word( 0x40C, 0xfffff9de, 1, 1 );
        #elif (FLASH_WRITE_SIZE==8)
          error |= flash_erase_sector( addr, 1 );
          if (addr == (0x408 & ~(FLASH_SECTOR_SIZE - 1)))
            error |= flash_phrase( 0x408, 0xfffff9deffffffff, 1, 1 );
        #endif
      }
    }
    
    // for KINETIS, these writes may be to the sector containing FSEC, but the
    // FSEC location was written by the code above, so use aFSEC=1, oFSEC=0
    #if defined(__IMXRT1062__)
      // for T4.x, data address passed to flash_write() must be in RAM
      uint32_t value = *(uint32_t *)(src + offset);     
      eepromemu_flash_write( (void*)addr, &value, 4 );
    #elif (FLASH_WRITE_SIZE==4)
      error |= flash_word( addr, *(uint32_t *)(src + offset), 1, 0 );
    #elif (FLASH_WRITE_SIZE==8)
      error |= flash_phrase( addr, *(uint64_t *)(src + offset), 1, 0 );
    #endif

    offset += FLASH_WRITE_SIZE;
  }
  
  // move is complete. if the source buffer (src) is in FLASH, erase the buffer
  // by erasing all sectors from top of new program to bottom of FLASH_RESERVE,
  // which leaves FLASH in same state as if code was loaded using TeensyDuino.
  // For KINETIS, this erase cannot include FSEC, so erase uses aFSEC=0.
  if (IN_FLASH(src)) {
    while (offset < (FLASH_SIZE - FLASH_RESERVE) && error == 0) {
      addr = dst + offset;
      if ((addr & (FLASH_SECTOR_SIZE - 1)) == 0) {
        if (flash_sector_not_erased( addr )) {
          #if defined(__IMXRT1062__)
            eepromemu_flash_erase_sector( (void*)addr );
          #else
            error |= flash_erase_sector( addr, 0 );
          #endif
	}
      }
      offset += FLASH_WRITE_SIZE;
    }   
  }

  // for T3.x, at least, must REBOOT here (via macro) because original code has
  // been erased and overwritten, so return address is no longer valid
  REBOOT;
  // wait here until REBOOT actually happens 
  for (;;) {}
}

//******************************************************************************
// flash_erase_block()	erase sectors from (start) to (start + size)
//******************************************************************************
int flash_erase_block( uint32_t start, uint32_t size )
{
  int error = 0;
  uint32_t address = start;
  while (address < (start + size) && error == 0) { 
    if ((address & (FLASH_SECTOR_SIZE - 1)) == 0) {
      if (flash_sector_not_erased( address )) {
	#if defined(__IMXRT1062__)
          eepromemu_flash_erase_sector( (void*)address );
        #elif defined(KINETISK) || defined(KINETISL)
          error = flash_erase_sector( address, 0 );
	#endif
      }
    }
    address += FLASH_SECTOR_SIZE;
  }
  return( error );
}

//******************************************************************************
// take a 32-bit aligned array of 32-bit values and write it to erased flash
//******************************************************************************
int flash_write_block( uint32_t addr, char *data, uint32_t count )
{
  // static (aligned) variables to guarantee 32-bit or 64-bit-aligned writes
  #if (FLASH_WRITE_SIZE == 4)				// #if 4-byte writes
  static uint32_t buf __attribute__ ((aligned (4)));	//   4-byte buffer
  #elif (FLASH_WRITE_SIZE == 8)				// #elif 8-byte writes
  static uint64_t buf __attribute__ ((aligned (8)));	//   8-byte buffer
  #endif						//
  static uint32_t buf_count = 0;			// bytes in buffer
  static uint32_t next_addr = 0;			// expected address
  
  int ret = 0;						// return value
  uint32_t data_i = 0;					// index to data array

  if ((addr % 4) != 0 || (count % 4) != 0) {		// if not 32-bit aligned
    return 1;	// "flash_block align error\n"		//   return error code 1
  }

  if (buf_count > 0 && addr != next_addr) {		// if unexpected address   
    return 2;	// "unexpected address\n"		//   return error code 2   
  }
  next_addr = addr + count;				//   compute next address
  addr -= buf_count;					//   address of data[0]

  while (data_i < count) {				// while more data
    ((char*)&buf)[buf_count++] = data[data_i++];	//   copy a byte to buf
    if (buf_count < FLASH_WRITE_SIZE) {			//   if buf not complete
      continue;						//     continue while()
    }							//   
    #if defined(__IMXRT1062__)				//   #if T4.x 4-byte
      eepromemu_flash_write((void*)addr,(void*)&buf,4);	//     flash_write()
    #elif (FLASH_WRITE_SIZE==4)				//   #elif T3.x 4-byte 
      ret = flash_word( addr, buf, 0, 0 );		//     flash_word()
    #elif (FLASH_WRITE_SIZE==8)				//   #elif T3.x 8-byte
      ret = flash_phrase( addr, buf, 0, 0 );		//     flash_phrase()
    #endif
    if (ret != 0) {					//   if write error
      return 3;	// "flash write error %d\n"		//     error code
    }
    buf_count = 0;					//   re-init buf count
    addr += FLASH_WRITE_SIZE;				//   advance address
  }  
  return 0;						// return success
}

#if defined(__MK66FX1M0__) // T3.6 only

  // MCU Local Memory PCCCR Register Bit Definitions (request to add to kinetis.h?)
  #define LMEM_PCCCR_GO      ((uint32_t)0x80000000)    //LMC Initiate Cache Command
  #define LMEM_PCCCR_PUSHW1  ((uint32_t)0x08000000)    //LMC Push all modified lines in way 1
  #define LMEM_PCCCR_INVW1   ((uint32_t)0x04000000)    //LMC Invalidate Way 1
  #define LMEM_PCCCR_PUSHW0  ((uint32_t)0x02000000)    //LMC Push all modified lines in way 0
  #define LMEM_PCCCR_INVW0   ((uint32_t)0x01000000)    //LMC Invalidate Way 0
  #define LMEM_PCCCR_PCCR3   ((uint32_t)0x00000008)    //LMC Forces no allocation on cache misses (must also have PCCR2 asserted)
  #define LMEM_PCCCR_PCCR2   ((uint32_t)0x00000004)    //LMC all cacheable areas write through
  #define LMEM_PCCCR_ENWRBUF ((uint32_t)0x00000002)    //LMC write buffer enable
  #define LMEM_PCCCR_ENCACHE ((uint32_t)0x00000001)    //LMC cache enable  

/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
 */

void LMEM_EnableCodeCache(bool enable)
{
    if (enable)
    {
        /* First, invalidate the entire cache. */
        LMEM_CodeCacheInvalidateAll();

        /* Now enable the cache. */
        LMEM_PCCCR |= LMEM_PCCCR_ENCACHE;
    }
    else
    {
        /* First, push any modified contents. */
        LMEM_CodeCachePushAll();

        /* Now disable the cache. */
        LMEM_PCCCR &= ~LMEM_PCCCR_ENCACHE;
    }
}

void LMEM_CodeCacheInvalidateAll(void)
{
    /* Enables the processor code bus to invalidate all lines in both ways.
    and Initiate the processor code bus code cache command. */
    LMEM_PCCCR |= LMEM_PCCCR_INVW0 | LMEM_PCCCR_INVW1 | LMEM_PCCCR_GO;

    /* Wait until the cache command completes. */
    while (LMEM_PCCCR & LMEM_PCCCR_GO)
    {
    }

    /* As a precaution clear the bits to avoid inadvertently re-running this command. */
    LMEM_PCCCR &= ~(LMEM_PCCCR_INVW0 | LMEM_PCCCR_INVW1);
}

void LMEM_CodeCachePushAll(void)
{
    /* Enable the processor code bus to push all modified lines. */
    LMEM_PCCCR |= LMEM_PCCCR_PUSHW0 | LMEM_PCCCR_PUSHW1 | LMEM_PCCCR_GO;

    /* Wait until the cache command completes. */
    while (LMEM_PCCCR & LMEM_PCCCR_GO)
    {
    }

    /* As a precaution clear the bits to avoid inadvertently re-running this command. */
    LMEM_PCCCR &= ~(LMEM_PCCCR_PUSHW0 | LMEM_PCCCR_PUSHW1);
}

void LMEM_CodeCacheClearAll(void)
{
    /* Push and invalidate all. */
    LMEM_PCCCR |= LMEM_PCCCR_PUSHW0 | LMEM_PCCCR_INVW0
                | LMEM_PCCCR_PUSHW1 | LMEM_PCCCR_INVW1
                | LMEM_PCCCR_GO;

    /* Wait until the cache command completes. */
    while (LMEM_PCCCR & LMEM_PCCCR_GO)
    {
    }

    /* As a precaution clear the bits to avoid inadvertently re-running this command. */
    LMEM_PCCCR &= ~(LMEM_PCCCR_PUSHW0 | LMEM_PCCCR_INVW0
                  | LMEM_PCCCR_PUSHW1 | LMEM_PCCCR_INVW1);
}

#endif

