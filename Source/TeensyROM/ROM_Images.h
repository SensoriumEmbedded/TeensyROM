
#include "ROMs\TeensyROMC64.h"

#include "ROMs\Jupiter_Lander.h" 
#include "ROMs\1541_Diagnostics.h" 
#include "ROMs\Joystick_Tester.h" 
#include "ROMs\Keyboard_Tester.h" 
#include "ROMs\SID_Tester.h" 
#include "ROMs\586220ast_Diagnostics.h" 
#include "ROMs\781220_Dead_Test.h" 
#include "ROMs\Donkey_Kong.h" 
#include "ROMs\ember_head.prg.h"
#include "ROMs\fb64.prg.h"
#include "ROMs\ccgms_2021.prg.h"
#include "ROMs\80columns.prg.h"
#include "ROMs\DualCopy.prg.h"
#include "ROMs\disp_fract.prg.h"
#include "ROMs\draw01.prg.h"
#include "ROMs\joust.prg.h"
#include "ROMs\epyx.prg.h"
#include "ROMs\game_controller_tester.prg.h"
#include "ROMs\hex_mon.prg.h"
#include "ROMs\rom_chksum_137kernals.prg.h"


StructMenuItem ROMMenu[] = 
{
   rtNone, "Test/Diags:----------------", NULL                                 , 1, //sepparator
   rt8kHi, "  781220 Dead Test"         , (uint8_t*)a781220_Dead_Test_BIN      , 1, //size not needed for ROMs
   rt8kLo, "  586220* Diagnostics"      , (uint8_t*)a586220ast_Diagnostics_BIN , 1, //size not needed for ROMs
   rt8kLo, "  1541 Diagnostics"         , (uint8_t*)a1541_Diagnostics_BIN      , 1, //size not needed for ROMs
   rt8kLo, "  Keyboard Tester"          , (uint8_t*)Keyboard_Tester_BIN        , 1, //size not needed for ROMs
   rt8kLo, "  SID Tester"               , (uint8_t*)SID_Tester_BIN             , 1, //size not needed for ROMs
   rt8kLo, "  Joystick Tester"          , (uint8_t*)Joystick_Tester_BIN        , 1, //size not needed for ROMs
   rtPrg , "  Game Controller Tester"   , (uint8_t*)game_controller_tester_prg , sizeof(game_controller_tester_prg) ,
   rtPrg , "  ROM Checksum read"        , (uint8_t*)rom_chksum_137kernals_prg  , sizeof(rom_chksum_137kernals_prg) ,

   rtNone, "Utilities:-----------------", NULL                                 , 1, //sepparator
   rtPrg , "  Epyx Fast Load"           , (uint8_t*)epyx_prg                   , sizeof(epyx_prg) ,
   rtPrg , "  Hex Mon"                  , (uint8_t*)hex_mon_prg                , sizeof(hex_mon_prg) ,
   rtPrg , "  80 Columns"               , (uint8_t*)a80columns_prg             , sizeof(a80columns_prg) ,
   rtPrg , "  File Browser 64"          , (uint8_t*)fb64_prg                   , sizeof(fb64_prg      ) ,
   rtPrg , "  DualCopy"                 , (uint8_t*)DualCopy_prg               , sizeof(DualCopy_prg  ) ,
   rtPrg , "  CCGMS 2021"               , (uint8_t*)ccgms_2021_prg             , sizeof(ccgms_2021_prg) ,

   rtNone, "Games:---------------------", NULL                                 , 1, //sepparator
   rt16k , "  Donkey Kong"              , (uint8_t*)Donkey_Kong_BIN            , 1, //size not needed for ROMs      
   rtPrg , "  Joust!"                   , (uint8_t*)joust_prg                  , sizeof(joust_prg) ,
   rt8kHi, "  Jupiter Lander"           , (uint8_t*)Jupiter_Lander_BIN         , 1,  //graphics messed up...

   rtNone, "Trav PRGs:-----------------", NULL                                 , 1, //sepparator
   rtPrg , "  Ember Head"               , (uint8_t*)ember_head_prg             , sizeof(ember_head_prg) ,
   rtPrg , "  Display Fractal"          , (uint8_t*)disp_fract_prg             , sizeof(disp_fract_prg) ,
   rtPrg , "  Draw!!!                :)", (uint8_t*)draw01_prg                 , sizeof(draw01_prg    ) , //max Name length  :)

};


