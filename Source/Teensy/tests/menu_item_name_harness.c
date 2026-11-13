// SPDX-License-Identifier: MIT
// menu_item_name_harness: reads "hide name" lines from stdin (hide is 0 or 1) and
// prints the name the C64 menu would show for each, between brackets.
#include <stdint.h>
#include <stdio.h>

#include "../MinimalBoot/Common/MenuItemName.h"
#include "../MinimalBoot/Common/Menu_Regs.h"

int main(void)
{
   char Line[512], Buf[MaxItemDispLength];
   while (fgets(Line, sizeof Line, stdin))
   {
      Line[strcspn(Line, "\n")] = 0;
      MenuItemDisplayName(Buf, sizeof Buf, Line + 2, Line[0] == '1');
      printf("[%s]\n", Buf);
   }
   return 0;
}
