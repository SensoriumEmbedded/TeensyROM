


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
  uint8_t HW_Config;
  char Name[MAX_ROMNAME_CHARS];
  const unsigned char *ROM_Image;
};

const StructROMDefs ROMMenu[] = 
{
   rt8kLo, "1541 Diagnostics"   , a1541_Diagnostics_BIN      ,
   rt8kLo, "Joystick Tester"    , Joystick_Tester_BIN        ,
   rt8kLo, "Keyboard Tester"    , Keyboard_Tester_BIN        ,
   rt8kLo, "SID Tester"         , SID_Tester_BIN             ,
   rt8kLo, "586220 Diagnostics" , a586220_Diagnostics_BIN    ,
   rt8kLo, "586220* Diagnostics", a586220ast_Diagnostics_BIN ,
   rt8kHi, "781220 Dead Test"   , a781220_Dead_Test_BIN      ,
   rt16k , "Donkey Kong"        , Donkey_Kong_BIN            ,      
   rt16k , "Dig Dug"            , Dig_Dug_bin                ,
   rtPrg , "Ember Head"         , ember_head_prg             ,   
   //rt8kHi, "Jupiter Lander"     , Jupiter_Lander_BIN         , //graphics messed up...

};


