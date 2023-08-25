
//add PROGMEM to declaration of all main menu binaries:
#include "ROMs\ccgms_2021_Swiftlink_DE_38400.prg.h"
#include "ROMs\cynthcart_201.prg.h"
#include "ROMs\sta64_v2_6.prg.h"
#include "ROMs\Epyx_Fast_Load.crt.h"
#include "ROMs\80columns.prg.h"
#include "ROMs\hex_mon.prg.h"
#include "ROMs\586220ast_Diagnostics.h" 
#include "ROMs\781220_Dead_Test.h"
#include "ROMs\1541_Diagnostics.h" 
#include "ROMs\SID_Tester.h" 
#include "ROMs\game_controller_tester.prg.h"
#include "ROMs\rom_chksum_137kernals.prg.h"
#include "ROMs\C128_789010.crt.h"
#include "ROMs\Jupiter_Lander.h" 
#include "ROMs\joust.prg.h"
#include "ROMs\Terminator_2.crt.h" 
#include "ROMs\Beach_Head_II.crt.h" 
#include "ROMs\Robocop_2.crt.h" 
#include "ROMs\Joystick_Tester.h" 
#include "ROMs\Keyboard_Tester.h" 
#include "ROMs\Donkey_Kong.h" 
#include "ROMs\Dig_Dug.bin.h" 
#include "ROMs\DualCopy.prg.h"
#include "ROMs\ember_head.prg.h"
#include "ROMs\disp_fract.prg.h"
#include "ROMs\draw01.prg.h"
#include "ROMs\super.m.bros.64.prg.h"
#include "ROMs\Ms._Pac_Man.crt.h"
#include "ROMs\C64618_Gorf_8000.crt.h"
#include "ROMs\KawariQuickChange.prg.h"
#include "ROMs\Mario_Bros.prg.h"
#include "ROMs\minesweeper_game.prg.h"
#include "ROMs\sam.prg.h"
#include "ROMs\SID_check.prg.h"
#include "ROMs\swinth_LIGHT_FANTASTIC.PRG.h"
#include "ROMs\Tetris.prg.h"

StructMenuItem TeensyROMMenu[] = 
{
//regItemTypes,enumIOHandlers   
// ItemType , IOHndlrAssoc     , Name[MaxItemNameLength]          , *Code_Image                          , Size ,

   rtNone   , IOH_None         , "MIDI/Terminal------------------", NULL                                 , 0, //sepparator
   rtFilePrg, IOH_MIDI_Datel   , " Cynthcart 2.0.1    +Datel MIDI", (uint8_t*)cynthcart_201_prg          , sizeof(cynthcart_201_prg) ,    
   rtFilePrg, IOH_MIDI_Passport, " Station64 2.6   +Passport MIDI", (uint8_t*)sta64_v2_6_prg             , sizeof(sta64_v2_6_prg) ,
   rtFilePrg, IOH_Swiftlink    , " CCGMS 2021 Term     +SwiftLink", (uint8_t*)ccgms_2021_Swift_DE_38k_prg, sizeof(ccgms_2021_Swift_DE_38k_prg) ,
   rtNone   , IOH_None         , ""                               , NULL                                 , 0, //sepparator

   rtNone   , IOH_None         , "Games:-------------------------", NULL                                 , 0, //sepparator
   rtFilePrg, IOH_None         , " Super Mario Brothers"          , (uint8_t*)super_m_bros_64_prg        , sizeof(super_m_bros_64_prg) ,  
   rtBin8kHi, IOH_None         , " Jupiter Lander"                , (uint8_t*)Jupiter_Lander_BIN         , sizeof(Jupiter_Lander_BIN) ,  
   rtFilePrg, IOH_None         , " Joust!"                        , (uint8_t*)joust_prg                  , sizeof(joust_prg) ,
   rtBin16k , IOH_None         , " Donkey Kong"                   , (uint8_t*)Donkey_Kong_BIN            , sizeof(Donkey_Kong_BIN) ,      
   rtBin16k , IOH_None         , " Dig Dug"                       , (uint8_t*)Dig_Dug_bin                , sizeof(Dig_Dug_bin) ,      
   rtFileCrt, IOH_None         , " Beach Head II      256k MC"    , (uint8_t*)Beach_Head_II_crt          , sizeof(Beach_Head_II_crt) ,
   rtFileCrt, IOH_None         , " Robocop 2          256k Ocean" , (uint8_t*)Robocop_2_crt              , sizeof(Robocop_2_crt) ,
   rtBin16k , IOH_None         , " Dig Dug"                       , (uint8_t*)Dig_Dug_bin                , sizeof(Dig_Dug_bin) ,      
   rtFileCrt, IOH_None         , " Ms. Pac-Man"                   , (uint8_t*)Ms__Pac_Man_crt            , sizeof(Ms__Pac_Man_crt) ,
   rtFileCrt, IOH_None         , " Gorf!"                         , (uint8_t*)C64618_Gorf_8000_crt       , sizeof(C64618_Gorf_8000_crt) ,
   rtFilePrg, IOH_None         , " Mario Brothers"                , (uint8_t*)Mario_Bros_prg             , sizeof(Mario_Bros_prg) ,
   rtFilePrg, IOH_None         , " Minesweeper"                   , (uint8_t*)minesweeper_game_prg       , sizeof(minesweeper_game_prg) ,
   rtFilePrg, IOH_None         , " Tetris"                        , (uint8_t*)Tetris_prg                 , sizeof(Tetris_prg) ,
   //rtFileCrt, IOH_None         , " Terminator 2       512k Ocean" , (uint8_t*)Terminator_2_crt           , sizeof(Terminator_2_crt) ,
   rtNone   , IOH_None         , ""                               , NULL                                 , 0, //sepparator

   rtNone   , IOH_None         , "Utilities----------------------", NULL                                 , 0, //sepparator
   rtFileCrt, IOH_None         , " Epyx Fast Load Cart"           , (uint8_t*)Epyx_Fast_Load_crt         , sizeof(Epyx_Fast_Load_crt) ,
   rtFilePrg, IOH_None         , " S.A.M.    \x7dRECITER  SAY\"hello\"", (uint8_t*)sam_prg                    , sizeof(sam_prg) ,
   rtFilePrg, IOH_None         , " 80 Columns"                    , (uint8_t*)a80columns_prg             , sizeof(a80columns_prg) ,
   rtFilePrg, IOH_None         , " Hex Mon"                       , (uint8_t*)hex_mon_prg                , sizeof(hex_mon_prg) ,
   rtFilePrg, IOH_None         , " DualCopy"                      , (uint8_t*)DualCopy_prg               , sizeof(DualCopy_prg) ,
   rtFilePrg, IOH_None         , " Kawari Quick Change"           , (uint8_t*)KawariQuickChange_prg      , sizeof(KawariQuickChange_prg) ,
   rtNone   , IOH_None         , ""                               , NULL                                 , 0, //sepparator

   rtNone   , IOH_None         , "Test/Diags---------------------", NULL                                 , 0, //sepparator
   rtBin8kHi, IOH_None         , " 781220 C64 Dead Test"          , (uint8_t*)a781220_Dead_Test_BIN      , sizeof(a781220_Dead_Test_BIN) ,
   rtBin8kLo, IOH_None         , " 586220* C64 Diagnostics"       , (uint8_t*)a586220ast_Diagnostics_BIN , sizeof(a586220ast_Diagnostics_BIN) ,
   rtFileCrt, IOH_None         , " 789010 C128 Diagnostics"       , (uint8_t*)C128_789010_crt            , sizeof(C128_789010_crt) ,
   rtBin8kLo, IOH_None         , " SID Tester"                    , (uint8_t*)SID_Tester_BIN             , sizeof(SID_Tester_BIN) ,
   rtFilePrg, IOH_None         , " SID checker/finder"            , (uint8_t*)SID_check_prg              , sizeof(SID_check_prg) ,
   rtBin8kLo, IOH_None         , " Keyboard Tester"               , (uint8_t*)Keyboard_Tester_BIN        , sizeof(Keyboard_Tester_BIN) ,
   rtBin8kLo, IOH_None         , " Joystick Tester"               , (uint8_t*)Joystick_Tester_BIN        , sizeof(Joystick_Tester_BIN) ,
   rtFilePrg, IOH_None         , " Game Controller Tester"        , (uint8_t*)game_controller_tester_prg , sizeof(game_controller_tester_prg) ,
   rtFilePrg, IOH_None         , " ROM Checksum read"             , (uint8_t*)rom_chksum_137kernals_prg  , sizeof(rom_chksum_137kernals_prg) ,
   rtBin8kLo, IOH_None         , " 1541 Diagnostics"              , (uint8_t*)a1541_Diagnostics_BIN      , sizeof(a1541_Diagnostics_BIN) ,
   rtNone   , IOH_None         , ""                               , NULL                                 , 0, //sepparator

   rtNone   , IOH_None         , "Visuals/Other:-----------------", NULL                                 , 0, //sepparator
   rtFilePrg, IOH_None         , " Swinth/Light Fantastic"        , (uint8_t*)swinth_LIGHT_FANTASTIC_PRG , sizeof(swinth_LIGHT_FANTASTIC_PRG) ,
   rtFilePrg, IOH_None         , " Draw!"                         , (uint8_t*)draw01_prg                 , sizeof(draw01_prg) ,
   rtFilePrg, IOH_None         , " Display Fractal"               , (uint8_t*)disp_fract_prg             , sizeof(disp_fract_prg) ,
   rtFilePrg, IOH_None         , " Ember Head"                    , (uint8_t*)ember_head_prg             , sizeof(ember_head_prg) ,

};


