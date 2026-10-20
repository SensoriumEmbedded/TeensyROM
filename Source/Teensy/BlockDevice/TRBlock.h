// SPDX-License-Identifier: MIT
// The register side of the TR block device, free of Arduino so a host test can drive it.
// The contract is docs/TR_Block_Device.md; offsets here are from the base that doc names.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define TRBLOCK_SIZE         256
#define TRBLOCK_WINDOW_SIZE  64
#define TRBLOCK_INFO_SIZE    5

enum TRBlockOffset
{
   TRBlockCmd    = 0x08,
   TRBlockAddr0  = 0x09,
   TRBlockAddr1  = 0x0a,
   TRBlockPage   = 0x0b,
   TRBlockToken  = 0x0c,
   TRBlockStatus = 0x0d,
   TRBlockEcho   = 0x0e,
   TRBlockId     = 0x10,
   TRBlockWindow = 0x40,
};

enum TRBlockCommand
{
   TRBlockRead  = 1,
   TRBlockWrite = 2,
   TRBlockFlush = 3,
   TRBlockInfo  = 4,
};

enum TRBlockStatusValue
{
   TRBlockNeverRun   = 0x00,
   TRBlockBusy       = 0x80,
   TRBlockDone       = 0x5a,
   TRBlockNoImage    = 0xe1,
   TRBlockBadAddress = 0xe2,
   TRBlockReadFailed = 0xe3,
   TRBlockWriteFailed = 0xe4,
   TRBlockReadOnly   = 0xe5,
   TRBlockBadCommand = 0xe6,
};

static const char TRBlockIdText[] = "TRB";
#define TRBLOCK_ID_SIZE (sizeof TRBlockIdText - 1)

// Where the ISR and the main loop meet: the ISR only fills registers and the window and starts a
// job, the main loop does the card's work.
struct TRBlock
{
   uint8_t Buffer[TRBLOCK_SIZE];
   uint8_t Addr0, Addr1, Page, Token;
   uint8_t JobCmd, JobAddr0, JobAddr1, JobToken;
   volatile uint8_t Status, Echo;
   volatile bool Pending;
};

struct TRBlockBackend
{
   void *Context;
   uint8_t (*Transfer)(void *Context, bool Write, uint8_t Addr0, uint8_t Addr1, uint8_t *Block);
   uint8_t (*Info)(void *Context, uint8_t *Out);   // TRBLOCK_INFO_SIZE bytes
   uint8_t (*Flush)(void *Context);
};

// ISR and main loop run on one core, so only the compiler's ordering matters
#define TRBLOCK_ORDER() __atomic_signal_fence(__ATOMIC_SEQ_CST)

static inline void TRBlockReset(struct TRBlock *Dev)
{
   memset((void *)Dev, 0, sizeof *Dev);
}

// The IO ISR's side: reads answer from registers or the window...
static inline bool TRBlockServes(uint8_t Offset)
{
   return Offset == TRBlockStatus || Offset == TRBlockEcho ||
          (Offset >= TRBlockId && Offset < TRBlockId + TRBLOCK_ID_SIZE) ||
          (Offset >= TRBlockWindow && Offset < TRBlockWindow + TRBLOCK_WINDOW_SIZE);
}

static inline uint8_t TRBlockReadAt(const struct TRBlock *Dev, uint8_t Offset)
{
   if (Offset == TRBlockStatus) return Dev->Status;
   if (Offset == TRBlockEcho) return Dev->Echo;
   if (Offset >= TRBlockId && Offset < TRBlockId + TRBLOCK_ID_SIZE) return (uint8_t)TRBlockIdText[Offset - TRBlockId];
   return Dev->Buffer[(Dev->Page & 3) * TRBLOCK_WINDOW_SIZE + Offset - TRBlockWindow];
}

// ...and writes go to registers or the window. The caller reads the data byte only when this says
// the offset takes one.
static inline bool TRBlockTakes(uint8_t Offset)
{
   return (Offset >= TRBlockCmd && Offset <= TRBlockToken) ||
          (Offset >= TRBlockWindow && Offset < TRBlockWindow + TRBLOCK_WINDOW_SIZE);
}

static inline void TRBlockWriteAt(struct TRBlock *Dev, uint8_t Offset, uint8_t Data)
{
   bool Busy = Dev->Status == TRBlockBusy;

   if (Offset >= TRBlockWindow)
   {
      if (!Busy) Dev->Buffer[(Dev->Page & 3) * TRBLOCK_WINDOW_SIZE + Offset - TRBlockWindow] = Data;
      return;
   }
   if (Offset == TRBlockAddr0) Dev->Addr0 = Data;
   else if (Offset == TRBlockAddr1) Dev->Addr1 = Data;
   else if (Offset == TRBlockPage) Dev->Page = Data;
   else if (Offset == TRBlockToken) Dev->Token = Data;
   else if (Offset == TRBlockCmd && !Busy)
   {
      Dev->JobCmd = Data;
      Dev->JobAddr0 = Dev->Addr0;
      Dev->JobAddr1 = Dev->Addr1;
      Dev->JobToken = Dev->Token;
      TRBLOCK_ORDER();
      Dev->Status = TRBlockBusy;
      TRBLOCK_ORDER();
      Dev->Pending = true;
   }
}

static uint8_t TRBlockRun(struct TRBlock *Dev, const struct TRBlockBackend *Backend)
{
   if (!Backend || !Backend->Context) return TRBlockNoImage;
   switch (Dev->JobCmd)
   {
      case TRBlockRead:
         return Backend->Transfer(Backend->Context, false, Dev->JobAddr0, Dev->JobAddr1, Dev->Buffer);
      case TRBlockWrite:
         return Backend->Transfer(Backend->Context, true, Dev->JobAddr0, Dev->JobAddr1, Dev->Buffer);
      case TRBlockFlush:
         return Backend->Flush(Backend->Context);
      case TRBlockInfo:
         return Backend->Info(Backend->Context, Dev->Buffer);
   }
   return TRBlockBadCommand;
}

// The main loop's side: runs a started command, true if there was one. The echo is published
// before the status, so a status that is not busy always comes with its token.
static inline bool TRBlockService(struct TRBlock *Dev, const struct TRBlockBackend *Backend)
{
   uint8_t Result;

   if (!Dev->Pending) return false;
   TRBLOCK_ORDER();
   Result = TRBlockRun(Dev, Backend);
   Dev->Pending = false;
   TRBLOCK_ORDER();
   Dev->Echo = Dev->JobToken;
   TRBLOCK_ORDER();
   Dev->Status = Result;
   return true;
}
