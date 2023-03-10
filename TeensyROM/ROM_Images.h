

#include "ROMs\TeensyROMC64.h"
#include "ROMs\Jupiter_Lander.h" 
#include "ROMs\1541_Diagnostics.h" 
#include "ROMs\Joystick_Tester.h" 
#include "ROMs\Keyboard_Tester.h" 
#include "ROMs\SID_Tester.h" 
#include "ROMs\586220_Diagnostics.h" 
#include "ROMs\586220ast_Diagnostics.h" 
#include "ROMs\781220_Dead_Test.h" 
#include "ROMs\Donkey_Kong.h" 
#include "ROMs\ember_head.prg.h"
#include "ROMs\Dig_Dug.bin.h"
#include "ROMs\fb64.prg.h"
#include "ROMs\ccgms_2021.prg.h"
#include "ROMs\80columns.prg.h"
#include "ROMs\DualCopy.prg.h"
#include "ROMs\disp_fract.prg.h"
#include "ROMs\draw01.prg.h"
#include "ROMs\joust.prg.h"
#include "ROMs\sound.prg.h"

StructMenuItem ROMMenu[] = 
{
   rtNone, "Diags:---------------------", NULL                       , 1, //sepparator
   rt8kLo, "  1541 Diagnostics"         , a1541_Diagnostics_BIN      , 1, //size not needed for ROMs
   rt8kLo, "  Joystick Tester"          , Joystick_Tester_BIN        , 1, //size not needed for ROMs
   rt8kLo, "  Keyboard Tester"          , Keyboard_Tester_BIN        , 1, //size not needed for ROMs
   rt8kLo, "  SID Tester"               , SID_Tester_BIN             , 1, //size not needed for ROMs
   rt8kLo, "  586220 Diagnostics"       , a586220_Diagnostics_BIN    , 1, //size not needed for ROMs
   rt8kLo, "  586220* Diagnostics"      , a586220ast_Diagnostics_BIN , 1, //size not needed for ROMs
   rt8kHi, "  781220 Dead Test"         , a781220_Dead_Test_BIN      , 1, //size not needed for ROMs

   rtNone, "Utilities:-----------------", NULL                       , 1, //sepparator
   rtPrg , "  80 Columns"               , a80columns_prg             , sizeof(a80columns_prg) ,
   rtPrg , "  File Browser 64"          , fb64_prg                   , sizeof(fb64_prg      ) ,
   rtPrg , "  DualCopy"                 , DualCopy_prg               , sizeof(DualCopy_prg  ) ,
   rtPrg , "  CCGMS 2021"               , ccgms_2021_prg             , sizeof(ccgms_2021_prg) ,

   rtNone, "Games:---------------------", NULL                       , 1, //sepparator
   rt16k , "  Donkey Kong"              , Donkey_Kong_BIN            , 1, //size not needed for ROMs      
   rt16k , "  Dig Dug"                  , Dig_Dug_bin                , 1, //size not needed for ROMs
   rtPrg , "  Joust!"                   , joust_prg                  , sizeof(joust_prg) ,

   rtNone, "My Devel:------------------", NULL                       , 1, //sepparator
   rtPrg , "  Ember Head"               , ember_head_prg             , sizeof(ember_head_prg) ,
   rtPrg , "  Display Fractal"          , disp_fract_prg             , sizeof(disp_fract_prg) ,
   rtPrg , "  Draw!!!                :)", draw01_prg                 , sizeof(draw01_prg    ) , //max Name length  :)
   rtPrg , "  BASIC test prg"           , sound_prg                  , sizeof(sound_prg) ,

   rtNone, "Need work:-----------------", NULL                       , 1, //sepparator
   //not working:                       
   rt8kHi, "  Jupiter Lander"           , Jupiter_Lander_BIN         , 1,  //graphics messed up...
};


