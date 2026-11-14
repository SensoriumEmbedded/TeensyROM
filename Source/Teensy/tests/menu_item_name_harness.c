// SPDX-License-Identifier: MIT
// menu_item_name_harness: reads "hide name" lines from stdin (hide is 0 or 1) and
// prints the name the C64 menu would show for each, between brackets. Exits 2 if
// anything was written past the display width.
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// glibc hides strlcpy under -std=c99 and lacks it before 2.38, as does MinGW, so the
// harness brings its own; the #undef covers a C library that makes it a fortify macro.
static size_t harness_strlcpy(char *Dest, const char *Src, size_t Size)
{
   size_t Len = strlen(Src);
   if (Size) { size_t Copy = Len < Size - 1 ? Len : Size - 1; memcpy(Dest, Src, Copy); Dest[Copy] = 0; }
   return Len;
}
#undef strlcpy
#define strlcpy harness_strlcpy

#include "../MinimalBoot/Common/MenuItemName.h"
#include "../MinimalBoot/Common/Menu_Regs.h"

#define Canary '#'

int main(void)
{
   char Line[512], Buf[sizeof Line];
   while (fgets(Line, sizeof Line, stdin))
   {
      Line[strcspn(Line, "\n")] = 0;
      memset(Buf, Canary, sizeof Buf);
      MenuItemDisplayName(Buf, MaxItemDispLength, Line + 2, Line[0] == '1');
      for (size_t i = MaxItemDispLength; i < sizeof Buf; i++)
         if (Buf[i] != Canary)
         {
            fprintf(stderr, "wrote Buf[%zu], past the display width\n", i);
            return 2;
         }
      printf("[%s]\n", Buf);
   }
   return 0;
}
