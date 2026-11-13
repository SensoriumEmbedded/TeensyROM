#ifndef MENU_ITEM_NAME_H
#define MENU_ITEM_NAME_H

#include <stdbool.h>
#include <string.h>

// The name the C64 menu prints for an item, into Buf, cut to Size-1 characters.
// Called from the IO1 write handler, so always inlined: it stays on the PHI2 path
// with no call and no FLASHMEM.
//
// The extension's dot is looked for in the whole name, before the cut: in the cut
// copy of "TeensyROM+_0.9.0.69-DMA-noisr_full.hex" the last dot is inside the
// version, and the item showed as "TeensyROM+_0.9.0".
static inline __attribute__((always_inline))
void MenuItemDisplayName(char *Buf, size_t Size, const char *Name, bool HideExt)
{
   size_t Dot = strlcpy(Buf, Name, Size);
   if (!HideExt) return;
   while (Dot && Name[--Dot] != '.') ;
   if (Name[Dot] == '.' && Dot < Size) Buf[Dot] = 0;
}

#endif
