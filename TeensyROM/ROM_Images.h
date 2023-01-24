

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


enum IO1_Registers  //offset from 0xDE00, needs to match C64 code
{
   rRegStatus,  //loc 0(DE00) gets written to after/during reset
   
   rRegStrAddrLo,
   rRegStrAddrHi,  //zero when inactive (no transfer to zero page)
   rRegStrData,
   wRegControl, //execute specific functions
   rRegPresence1, //for HW detect: 0x55
   rRegPresence2, //for HW detect: 0xAA
   rwRegSelect,  //select ROM for name, type, execution, etc
   rRegNumROMs,
   rRegROMType,
   rRegROMName, //MAX_ROMNAME_CHARS in length (incl term)
};

enum RegCtlCommands
{
   RCtlVanish = 0,
   RCtlVanishReset,
   RCtlStartRom ,
   RCtlLoadFromSD ,
   RCtlLoadFromUSB ,
};

enum ROMTypes
{
   rt16k  = 0,
   rt8kHi = 1,
   rt8kLo = 2,
   rtPrg  = 3,
};
#define  MAX_ROMNAME_CHARS 25

struct StructROMDefs
{
  unsigned char HW_Config;
  const char Name[MAX_ROMNAME_CHARS];
  const unsigned char *ROM_Image;
  uint16_t Size;
};

const StructROMDefs ROMMenu[] = 
{
   rt8kLo, "1541 Diagnostics"        , a1541_Diagnostics_BIN      , 1, //size not needed for ROMs
   rt8kLo, "Joystick Tester"         , Joystick_Tester_BIN        , 1, //size not needed for ROMs
   rt8kLo, "Keyboard Tester"         , Keyboard_Tester_BIN        , 1, //size not needed for ROMs
   rt8kLo, "SID Tester"              , SID_Tester_BIN             , 1, //size not needed for ROMs
   rt8kLo, "586220 Diagnostics"      , a586220_Diagnostics_BIN    , 1, //size not needed for ROMs
   rt8kLo, "586220* Diagnostics"     , a586220ast_Diagnostics_BIN , 1, //size not needed for ROMs
   rt8kHi, "781220 Dead Test"        , a781220_Dead_Test_BIN      , 1, //size not needed for ROMs
   rtPrg , "File Browser 64"         , fb64_prg                   , sizeof(fb64_prg      ) ,
   rtPrg , "DualCopy"                , DualCopy_prg               , sizeof(DualCopy_prg  ) ,
   rtPrg , "CCGMS 2021"              , ccgms_2021_prg             , sizeof(ccgms_2021_prg) ,
   rt16k , " Donkey Kong"            , Donkey_Kong_BIN            , 1, //size not needed for ROMs      
   rt16k , " Dig Dug"                , Dig_Dug_bin                , 1, //size not needed for ROMs
   rtPrg , "  Ember Head"            , ember_head_prg             , sizeof(ember_head_prg) ,
   rtPrg , "  Display Fractal"       , disp_fract_prg             , sizeof(disp_fract_prg) ,
   rtPrg , "  Draw!!!             :)", draw01_prg                 , sizeof(draw01_prg    ) , //max Name length  :)
   //not working:                    
   rtPrg , "nw80 Columns"            , a80columns_prg             , sizeof(a80columns_prg) ,
   rt8kHi, "nwJupiter Lander"        , Jupiter_Lander_BIN         , 1,  //graphics messed up...

};


