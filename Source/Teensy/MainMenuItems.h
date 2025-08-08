
//add PROGMEM to declaration of all main menu binaries:
#include "ROMs/ccgms_2021_Swiftlink_DE_38400.prg.h"
#include "ROMs/SST.prg.h"
#include "ROMs/cynthcart_201.prg.h"
#include "ROMs/sta64_v2_6.prg.h"
#include "ROMs/sid_wizard_1.9.prg.h"
#include "ROMs/SID_Wizard_2SID.prg.h"
#include "ROMs/SW_1.9_UserManual.prg.h"
#include "ROMs/Epyx_Fast_Load.crt.h"
#include "ROMs/80columns.prg.h"
#include "ROMs/hex_mon.prg.h"
#include "ROMs/586220ast_Diagnostics.h" 
#include "ROMs/781220_Dead_Test.h"
#include "ROMs/1541_Diagnostics.h" 
#include "ROMs/SID_Tester.h" 
#include "ROMs/game_controller_tester.prg.h"
#include "ROMs/rom_chksum_137kernals.prg.h"
#include "ROMs/C128_789010.crt.h"
#include "ROMs/Jupiter_Lander.h" 
#include "ROMs/jupiter_lander_crack.prg.h" 
#include "ROMs/joust.prg.h"
#include "ROMs/Terminator_2.crt.h" 
#include "ROMs/Beach_Head_II.crt.h" 
#include "ROMs/Joystick_Tester.h" 
#include "ROMs/Keyboard_Tester.h" 
#include "ROMs/Donkey_Kong.h" 
#include "ROMs/Dig_Dug.bin.h" 
#include "ROMs/DualCopy.prg.h"
#include "ROMs/draw01.prg.h"
#include "ROMs/super.m.bros.64.prg.h"
#include "ROMs/Ms._Pac_Man.crt.h"
#include "ROMs/C64618_Gorf_8000.crt.h"
#include "ROMs/KawariQuickChange.prg.h"
#include "ROMs/Mario_Bros.prg.h"
#include "ROMs/minesweeper_game.prg.h"
#include "ROMs/sam.prg.h"
#include "ROMs/SID_check.prg.h"
#include "ROMs/swinth_LIGHT_FANTASTIC.PRG.h"
#include "ROMs/Tetris.prg.h"
#include "ROMs/super_expander_64.crt.h" 
#include "ROMs/Robocop_2_EasyFlash.crt.h" 
#include "ROMs/MIDI2SID.prg.h" 
#include "ROMs/ASIDPlayer.prg.h" 
#include "ROMs/TRCBC.prg.h"
#include "ROMs/destest_max.rom.h"
#include "ROMs/empty.prg.h"
#include "ROMs/DMACheck.prg.h"
#include "ROMs/cia_tod.prg.h"
#include "ROMs/Load8Run.prg.h"
#include "ROMs/retromate_teensyrom.prg.h"
//#include "ROMs/ember_head.prg.h"
//#include "ROMs/disp_fract.prg.h"

#include "Text_PETSCII/Text_Viewer_Instructions.seq.h"
#include "Text_PETSCII/color_weave.seq.h"
#include "Text_PETSCII/Cygnus_X_1_Book_I___The_Voyage.txt.h"
#include "Text_PETSCII/do_not_panic.seq.h"
#include "Text_PETSCII/electric_guitar.seq.h"
#include "Text_PETSCII/inter.circuits.seq.h"
#include "Text_PETSCII/irnmdn_aceshigh.seq.h"
#include "Text_PETSCII/Jousters___c_g179.seq.h"
#include "Text_PETSCII/living_color.1.seq.h"
#include "Text_PETSCII/Small_Kindnesses.txt.h"
#include "Text_PETSCII/Stripes__Shoeshine_Boy___13picturedisk_2.seq.h"
#include "Text_PETSCII/tiger.cg.seq.h"
#include "Text_PETSCII/Time_Travel__Shoeshine_Boy__11picturedisk_2.seq.h"
#include "Text_PETSCII/Trains_II__Yoda______12picturedisk_6.seq.h"
#include "Text_PETSCII/Wish_You_Were_Here.txt.h"

#include "SIDs/Echoes.sid.h"
#include "SIDs/SleepDirt_norm_ntsc_1000_6581.sid.h"
#include "SIDs/Aces_High.sid.h"
#include "SIDs/Comfortably_Numb.sid.h"
#include "SIDs/Medley.sid.h"
#include "SIDs/Pink_Floyd.sid.h"
#include "SIDs/Popcorn.sid.h"
#include "SIDs/Switch_625.sid.h"
#include "SIDs/Tom_Sawyer.sid.h"
#include "SIDs/Wish_You_Were_Here.sid.h"
#include "SIDs/Odisey_2001_AD.sid.h"
#include "SIDs/When_Im_64.sid.h"
//#include "SIDs/YYZ.sid.h"

#include "Pics/Ex_Pie2.art.h"
#include "Pics/HA_Bluejay.kla.h"
#include "Pics/HA_Hummingbird.kla.h"
#include "Pics/HA_Sugar_Skull.kla.h"
#include "Pics/KP_Bird.kla.h"
#include "Pics/KP_Burger.kla.h"
#include "Pics/KP_Koala.kla.h"
#include "Pics/KP_Tiger.kla.h"
#include "Pics/LP_Boston.kla.h"
#include "Pics/LP_ELP_Tarkus.kla.h"
#include "Pics/LP_Zappa_Apostrophe.kla.h"
#include "Pics/LP_Zappa_Sleep_Dirt.kla.h"
#include "Pics/T_BlackHole.kla.h"
#include "Pics/T_Ember_Head.kla.h"
#include "Pics/T_Sensorium_Heatmap.kla.h"
#include "Pics/T_TeensyROM.kla.h"


StructMenuItem dirGames[] = 
{
 /* 0 */ rtDirectory, IOH_None         , (char*)UpDirString                        , NULL, 0 , //one dir level max, up Dir is always root
 /* 1 */ rtFilePrg  , IOH_Swiftlink    , (char*)"Retromate Internet Chess+SwiftLnk", (uint8_t*)retromate_teensyrom_prg    , sizeof(retromate_teensyrom_prg) ,
 /* 2 */ rtFileCrt  , IOH_None         , (char*)"Beach Head II         256k MC"    , (uint8_t*)Beach_Head_II_crt          , sizeof(Beach_Head_II_crt) ,
 /* 3 */ rtBin16k   , IOH_None         , (char*)"Dig Dug"                          , (uint8_t*)Dig_Dug_bin                , sizeof(Dig_Dug_bin) ,      
 /* 4 */ rtBin16k   , IOH_None         , (char*)"Donkey Kong"                      , (uint8_t*)Donkey_Kong_BIN            , sizeof(Donkey_Kong_BIN) ,      
 /* 5 */ rtFileCrt  , IOH_None         , (char*)"Gorf!"                            , (uint8_t*)C64618_Gorf_8000_crt       , sizeof(C64618_Gorf_8000_crt) ,
 /* 6 */ rtFilePrg  , IOH_None         , (char*)"Joust!"                           , (uint8_t*)joust_prg                  , sizeof(joust_prg) ,
 /**7**/ rtBin8kHi  , IOH_None         , (char*)"Jupiter Lander"                   , (uint8_t*)Jupiter_Lander_BIN         , sizeof(Jupiter_Lander_BIN) ,  
 /* 8 */ rtFilePrg  , IOH_None         , (char*)"Jupiter Lander (Cracked)"         , (uint8_t*)jupiter_lander_crack_prg   , sizeof(jupiter_lander_crack_prg) ,  
 /* 9 */ rtFilePrg  , IOH_None         , (char*)"Mario Brothers"                   , (uint8_t*)Mario_Bros_prg             , sizeof(Mario_Bros_prg) ,
 /*10 */ rtFilePrg  , IOH_None         , (char*)"Minesweeper"                      , (uint8_t*)minesweeper_game_prg       , sizeof(minesweeper_game_prg) ,   
 /*11 */ rtFileCrt  , IOH_None         , (char*)"Ms. Pac-Man"                      , (uint8_t*)Ms__Pac_Man_crt            , sizeof(Ms__Pac_Man_crt) ,
 /*12 */ rtFileCrt  , IOH_None         , (char*)"Robocop 2             224k EZF"   , (uint8_t*)Robocop_2_EasyFlash_crt    , sizeof(Robocop_2_EasyFlash_crt) ,
 /*13 */ rtFilePrg  , IOH_None         , (char*)"Super Mario Brothers   48k PRG"   , (uint8_t*)super_m_bros_64_prg        , sizeof(super_m_bros_64_prg) ,  
 /*14 */ rtFileCrt  , IOH_None         , (char*)"Terminator 2 +MM+IM   512k Ocean" , (uint8_t*)Terminator_2_crt           , sizeof(Terminator_2_crt) ,
 /*15 */ rtFilePrg  , IOH_None         , (char*)"Tetris"                           , (uint8_t*)Tetris_prg                 , sizeof(Tetris_prg) ,
};

StructMenuItem dirTest_Diags[] = 
{
   rtDirectory, IOH_None         , (char*)UpDirString                        , NULL, 0 , //one dir level max, up Dir is always root
   rtBin8kLo  , IOH_None         , (char*)"1541 Diagnostics"                 , (uint8_t*)a1541_Diagnostics_BIN      , sizeof(a1541_Diagnostics_BIN) ,
   rtBin8kLo  , IOH_None         , (char*)"586220(ast) C64 Diagnostics"      , (uint8_t*)a586220ast_Diagnostics_BIN , sizeof(a586220ast_Diagnostics_BIN) ,
   rtBin8kHi  , IOH_None         , (char*)"781220 C64 Dead Test"             , (uint8_t*)a781220_Dead_Test_BIN      , sizeof(a781220_Dead_Test_BIN) ,
   rtBin8kHi  , IOH_None         , (char*)"DesTestMAX:  Desmond's RAM Test"  , (uint8_t*)destest_max_rom            , sizeof(destest_max_rom) ,
   rtFileCrt  , IOH_None         , (char*)"789010 C128 Diagnostics"          , (uint8_t*)C128_789010_crt            , sizeof(C128_789010_crt) ,
   rtFilePrg  , IOH_None         , (char*)"Game Controller Tester"           , (uint8_t*)game_controller_tester_prg , sizeof(game_controller_tester_prg) ,
   rtBin8kLo  , IOH_None         , (char*)"Joystick Tester"                  , (uint8_t*)Joystick_Tester_BIN        , sizeof(Joystick_Tester_BIN) ,
   rtBin8kLo  , IOH_None         , (char*)"Keyboard Tester"                  , (uint8_t*)Keyboard_Tester_BIN        , sizeof(Keyboard_Tester_BIN) ,
   rtFilePrg  , IOH_None         , (char*)"ROM Checksum read"                , (uint8_t*)rom_chksum_137kernals_prg  , sizeof(rom_chksum_137kernals_prg) ,
   rtBin8kLo  , IOH_None         , (char*)"SID Tester"                       , (uint8_t*)SID_Tester_BIN             , sizeof(SID_Tester_BIN) ,
   rtFilePrg  , IOH_None         , (char*)"SID checker+finder"               , (uint8_t*)SID_check_prg              , sizeof(SID_check_prg) ,
   rtFilePrg  , IOH_None         , (char*)"CIA TimeOfDay clock check"        , (uint8_t*)cia_tod_prg                , sizeof(cia_tod_prg) ,
   rtFilePrg  , IOH_TR_BASIC     , (char*)"DMA Pause Check (beta)"           , (uint8_t*)DMACheck_prg               , sizeof(DMACheck_prg) , 
};

StructMenuItem dirUtilities[] = 
{
 /* 0 */ rtDirectory, IOH_None         , (char*)UpDirString                        , NULL, 0 , //one dir level max, up Dir is always root
 /**1**/ rtFilePrg  , IOH_Swiftlink    , (char*)"CCGMS 2021 Term       +SwiftLink ", (uint8_t*)ccgms_2021_Swift_DE_38k_prg, sizeof(ccgms_2021_Swift_DE_38k_prg) ,
 /* 2 */ rtFilePrg  , IOH_None         , (char*)"Exit to BASIC"                    , (uint8_t*)empty_prg                  , sizeof(empty_prg) ,
 /* 3 */ rtFilePrg  , IOH_None         , (char*)"LOAD\"*\",8,1  and  RUN"          , (uint8_t*)Load8Run_prg               , sizeof(Load8Run_prg) ,
 /* 4 */ rtFilePrg  , IOH_TR_BASIC     , (char*)"BASIC with TeensyROM Commands"    , (uint8_t*)TRCBC_prg                  , sizeof(TRCBC_prg) ,
 /* 5 */ rtFileCrt  , IOH_None         , (char*)"Super Expander 64"                , (uint8_t*)super_expander_64_crt      , sizeof(super_expander_64_crt) ,
 /* 6 */ rtFileCrt  , IOH_None         , (char*)"Epyx Fast Load Cart"              , (uint8_t*)Epyx_Fast_Load_crt         , sizeof(Epyx_Fast_Load_crt) ,
 /* 7 */ rtBin8kLo  , IOH_TeensyROM    , (char*)"TeensyROM Menu Cart+TeensyROM IOH", (uint8_t*)TeensyROMC64_bin           , sizeof(TeensyROMC64_bin) ,
 /* 8 */ rtFilePrg  , IOH_None         , (char*)"80 Columns"                       , (uint8_t*)a80columns_prg             , sizeof(a80columns_prg) ,
 /* 9 */ rtFilePrg  , IOH_None         , (char*)"DualCopy"                         , (uint8_t*)DualCopy_prg               , sizeof(DualCopy_prg) ,
 /*10 */ rtFilePrg  , IOH_None         , (char*)"Hex Mon"                          , (uint8_t*)hex_mon_prg                , sizeof(hex_mon_prg) ,
 /*11 */ rtFilePrg  , IOH_None         , (char*)"Kawari Quick Change"              , (uint8_t*)KawariQuickChange_prg      , sizeof(KawariQuickChange_prg) ,
 /*12 */ rtFilePrg  , IOH_None         , (char*)"S.A.M.     ]RECITER   SAY\"hello\"", (uint8_t*)sam_prg                   , sizeof(sam_prg) ,
 /*13 */ rtFilePrg  , IOH_Swiftlink    , (char*)"Simple SwiftLink Term (dev ref)"  , (uint8_t*)SST_prg                    , sizeof(SST_prg) ,
};

StructMenuItem dirMultimedia[] = 
{
   rtDirectory, IOH_None         , (char*)UpDirString                        , NULL, 0 , //one dir level max, up Dir is always root
   rtFilePrg  , IOH_None         , (char*)"Swinth+Light Fantastic"           , (uint8_t*)swinth_LIGHT_FANTASTIC_PRG , sizeof(swinth_LIGHT_FANTASTIC_PRG) ,
   rtFilePrg  , IOH_None         , (char*)"Draw!"                            , (uint8_t*)draw01_prg                 , sizeof(draw01_prg) ,
//   rtFilePrg  , IOH_None         , (char*)"Display Fractal"                  , (uint8_t*)disp_fract_prg             , sizeof(disp_fract_prg) ,
//   rtFilePrg  , IOH_None         , (char*)"Ember Head"                       , (uint8_t*)ember_head_prg             , sizeof(ember_head_prg) ,
};

StructMenuItem dirSID_Files[] = 
{  //DefSIDName should match one of these
   rtDirectory, IOH_None         , (char*)UpDirString                        , NULL, 0 , //one dir level max, up Dir is always root
   rtFileSID  , IOH_None         , (char*)"Also sprach Zarathustra   Strauss", (uint8_t*)Odisey_2001_AD_sid         , sizeof(Odisey_2001_AD_sid) ,
   rtFileSID  , IOH_None         , (char*)"Medley                Various"    , (uint8_t*)Medley_sid                 , sizeof(Medley_sid) ,
   rtFileSID  , IOH_None         , (char*)"When I'm 64           The Beatles", (uint8_t*)When_Im_64_sid             , sizeof(When_Im_64_sid) ,
   rtFileSID  , IOH_None         , (char*)"Popcorn               G. Kingsley", (uint8_t*)Popcorn_sid                , sizeof(Popcorn_sid) ,
   rtFileSID  , IOH_None         , (char*)"Comfortably Numb      Pink Floyd" , (uint8_t*)Comfortably_Numb_sid       , sizeof(Comfortably_Numb_sid) ,
   rtFileSID  , IOH_None         , (char*)"Echoes                Pink Floyd" , (uint8_t*)Echoes_sid                 , sizeof(Echoes_sid) ,
   rtFileSID  , IOH_None         , (char*)"Wish You Were Here    Pink Floyd" , (uint8_t*)Wish_You_Were_Here_sid     , sizeof(Wish_You_Were_Here_sid) ,
   rtFileSID  , IOH_None         , (char*)"Another Brick In The Wall"        , (uint8_t*)Pink_Floyd_sid             , sizeof(Pink_Floyd_sid) ,
   rtFileSID  , IOH_None         , (char*)"Sleep Dirt            Frank Zappa", (uint8_t*)SleepDirt_norm_ntsc_1000_6581_sid , sizeof(SleepDirt_norm_ntsc_1000_6581_sid) ,
   rtFileSID  , IOH_None         , (char*)"Tom Sawyer            Rush"       , (uint8_t*)Tom_Sawyer_sid             , sizeof(Tom_Sawyer_sid) ,
//   rtFileSID  , IOH_None         , (char*)"YYZ                   Rush"       , (uint8_t*)YYZ_sid                    , sizeof(YYZ_sid) ,                  //  C64 mem conflict as of 0.6.2
   rtFileSID  , IOH_None         , (char*)"Aces High             Iron Maiden", (uint8_t*)Aces_High_sid              , sizeof(Aces_High_sid) ,
   rtFileSID  , IOH_None         , (char*)"Switch 625            Def Leppard", (uint8_t*)Switch_625_sid             , sizeof(Switch_625_sid) ,
};

StructMenuItem dirPic_Files[] = 
{
   rtDirectory, IOH_None         , (char*)UpDirString                        , NULL, 0 , //one dir level max, up Dir is always root
   rtFileKla  , IOH_None         , (char*)"TS: TeensyROM"                    , (uint8_t*)T_TeensyROM_kla            , sizeof(T_TeensyROM_kla) ,
   rtFileKla  , IOH_None         , (char*)"TS: BlackHole"                    , (uint8_t*)T_BlackHole_kla            , sizeof(T_BlackHole_kla) ,
   rtFileKla  , IOH_None         , (char*)"TS: Ember Head"                   , (uint8_t*)T_Ember_Head_kla           , sizeof(T_Ember_Head_kla) ,
   rtFileKla  , IOH_None         , (char*)"TS: Sensorium Heatmap"            , (uint8_t*)T_Sensorium_Heatmap_kla    , sizeof(T_Sensorium_Heatmap_kla) ,
   rtFileKla  , IOH_None         , (char*)"HA: Bluejay"                      , (uint8_t*)HA_Bluejay_kla             , sizeof(HA_Bluejay_kla) ,
   rtFileKla  , IOH_None         , (char*)"HA: Hummingbird"                  , (uint8_t*)HA_Hummingbird_kla         , sizeof(HA_Hummingbird_kla) ,
   rtFileKla  , IOH_None         , (char*)"HA: Sugar Skull"                  , (uint8_t*)HA_Sugar_Skull_kla         , sizeof(HA_Sugar_Skull_kla) ,
   rtFileKla  , IOH_None         , (char*)"LP: Boston"                       , (uint8_t*)LP_Boston_kla              , sizeof(LP_Boston_kla) ,
   rtFileKla  , IOH_None         , (char*)"LP: ELP Tarkus"                   , (uint8_t*)LP_ELP_Tarkus_kla          , sizeof(LP_ELP_Tarkus_kla) ,
   rtFileKla  , IOH_None         , (char*)"LP: Zappa Sleep Dirt"             , (uint8_t*)LP_Zappa_Sleep_Dirt_kla    , sizeof(LP_Zappa_Sleep_Dirt_kla) ,
   rtFileKla  , IOH_None         , (char*)"LP: Zappa Apostrophe (')"         , (uint8_t*)LP_Zappa_Apostrophe_kla    , sizeof(LP_Zappa_Apostrophe_kla) ,
   rtFileArt  , IOH_None         , (char*)"Pie2 Hi-Res"                      , (uint8_t*)Ex_Pie2_art                , sizeof(Ex_Pie2_art) ,
   rtFileKla  , IOH_None         , (char*)"KP: Bird"                         , (uint8_t*)KP_Bird_kla                , sizeof(KP_Bird_kla) ,
   rtFileKla  , IOH_None         , (char*)"KP: Burger"                       , (uint8_t*)KP_Burger_kla              , sizeof(KP_Burger_kla) ,
   rtFileKla  , IOH_None         , (char*)"KP: Koala"                        , (uint8_t*)KP_Koala_kla               , sizeof(KP_Koala_kla) ,
   rtFileKla  , IOH_None         , (char*)"KP: Tiger"                        , (uint8_t*)KP_Tiger_kla               , sizeof(KP_Tiger_kla) ,
};

StructMenuItem dirMIDI_ASID[] = 
{
 /* 0 */ rtDirectory, IOH_None           , (char*)UpDirString                        , NULL, 0 , //one dir level max, up Dir is always root
 /* 1 */ rtFilePrg  , IOH_ASID           , (char*)"TeensyROM ASID Player    +TR ASID", (uint8_t*)ASIDPlayer_prg             , sizeof(ASIDPlayer_prg) ,
 /* 2 */ rtFilePrg  , IOH_TeensyROM      , (char*)"MIDI2SID          +TeensyROM MIDI", (uint8_t*)MIDI2SID_prg               , sizeof(MIDI2SID_prg) ,
 /* 3 */ rtFilePrg  , IOH_MIDI_Datel     , (char*)"Cynthcart 2.0.1       +Datel MIDI", (uint8_t*)cynthcart_201_prg          , sizeof(cynthcart_201_prg) ,    
 /* 4 */ rtFilePrg  , IOH_MIDI_Passport  , (char*)"Station64 2.6      +Passport MIDI", (uint8_t*)sta64_v2_6_prg             , sizeof(sta64_v2_6_prg) ,
 /* 5 */ rtFilePrg  , IOH_MIDI_Sequential, (char*)"SID-Wizard V1.92        +Seq MIDI", (uint8_t*)sid_wizard_1_9_prg         , sizeof(sid_wizard_1_9_prg) ,
 /* 6 */ rtFilePrg  , IOH_MIDI_Sequential, (char*)"SID-Wizard_2SID V1.92   +Seq MIDI", (uint8_t*)SID_Wizard_2SID_prg        , sizeof(SID_Wizard_2SID_prg) ,
 /* 7 */ rtFilePrg  , IOH_None           , (char*)"SID-Wizard User Manual V1.92"     , (uint8_t*)SW_1_9_UserManual_prg      , sizeof(SW_1_9_UserManual_prg) ,
};

StructMenuItem dirTEXT_PETSCII[] = 
{
   rtDirectory   , IOH_None      , (char*)UpDirString                        , NULL, 0 , //one dir level max, up Dir is always root
   rtFilePETSCII , IOH_None      , (char*)"---Text Viewer Usage Document---" , (uint8_t*)Text_Viewer_Instructions_seq                   , sizeof(Text_Viewer_Instructions_seq) ,
   rtFileTxt     , IOH_None      , (char*)"Wish You Were Here"               , (uint8_t*)Wish_You_Were_Here_txt                         , sizeof(Wish_You_Were_Here_txt) ,
   rtFilePETSCII , IOH_None      , (char*)"Jousters"                         , (uint8_t*)Jousters___c_g179_seq                          , sizeof(Jousters___c_g179_seq) ,
   rtFilePETSCII , IOH_None      , (char*)"Color Weave"                      , (uint8_t*)color_weave_seq                                , sizeof(color_weave_seq) ,
   rtFileTxt     , IOH_None      , (char*)"Cygnus X-1, Book I: The_Voyage"   , (uint8_t*)Cygnus_X_1_Book_I___The_Voyage_txt             , sizeof(Cygnus_X_1_Book_I___The_Voyage_txt) ,
   rtFilePETSCII , IOH_None      , (char*)"Electric Guitar"                  , (uint8_t*)electric_guitar_seq                            , sizeof(electric_guitar_seq) ,
   rtFilePETSCII , IOH_None      , (char*)"Iron Maiden: Aces High"           , (uint8_t*)irnmdn_aceshigh_seq                            , sizeof(irnmdn_aceshigh_seq) ,
   rtFilePETSCII , IOH_None      , (char*)"Do Not Panic!"                    , (uint8_t*)do_not_panic_seq                               , sizeof(do_not_panic_seq) ,
   rtFileTxt     , IOH_None      , (char*)"Small Kindnesses"                 , (uint8_t*)Small_Kindnesses_txt                           , sizeof(Small_Kindnesses_txt) ,
   rtFilePETSCII , IOH_None      , (char*)"Inter Circuits"                   , (uint8_t*)inter_circuits_seq                             , sizeof(inter_circuits_seq) ,
   rtFilePETSCII , IOH_None      , (char*)"Living Color"                     , (uint8_t*)living_color_1_seq                             , sizeof(living_color_1_seq) ,
   rtFilePETSCII , IOH_None      , (char*)"Tiger"                            , (uint8_t*)tiger_cg_seq                                   , sizeof(tiger_cg_seq) ,
   rtFilePETSCII , IOH_None      , (char*)"Stripes by Shoeshine Boy"         , (uint8_t*)Stripes__Shoeshine_Boy___13picturedisk_2_seq   , sizeof(Stripes__Shoeshine_Boy___13picturedisk_2_seq) ,
   rtFilePETSCII , IOH_None      , (char*)"Time Travel by Shoeshine Boy"     , (uint8_t*)Time_Travel__Shoeshine_Boy__11picturedisk_2_seq, sizeof(Time_Travel__Shoeshine_Boy__11picturedisk_2_seq) ,
   rtFilePETSCII , IOH_None      , (char*)"Trains II by Yoda"                , (uint8_t*)Trains_II__Yoda______12picturedisk_6_seq       , sizeof(Trains_II__Yoda______12picturedisk_6_seq) ,
};

//define this last:
StructMenuItem TeensyROMMenu[] = 
{
//Ref# used for HotKeyLaunch, update MainMenu.asm if these change!
//DefSIDPath should be "/" or one of these names

// Ref  regItemTypes, enumIOHandlers   
//  #    ItemType   , IOHndlrAssoc       , Name                                      , *Code_Image                          , Size ,

 /* 0 */ rtDirectory, IOH_None           , (char*)"/Games"                           , (uint8_t*)dirGames                   , sizeof(dirGames) ,
 /* 1 */ rtDirectory, IOH_None           , (char*)"/MIDI + ASID"                     , (uint8_t*)dirMIDI_ASID               , sizeof(dirMIDI_ASID) ,
 /* 2 */ rtDirectory, IOH_None           , (char*)"/SID Cover Tunes"                 , (uint8_t*)dirSID_Files               , sizeof(dirSID_Files) ,
 /* 3 */ rtDirectory, IOH_None           , (char*)"/Pictures"                        , (uint8_t*)dirPic_Files               , sizeof(dirPic_Files) ,
 /* 4 */ rtDirectory, IOH_None           , (char*)"/Multimedia"                      , (uint8_t*)dirMultimedia              , sizeof(dirMultimedia) ,
 /* 5 */ rtDirectory, IOH_None           , (char*)"/Test+Diags"                      , (uint8_t*)dirTest_Diags              , sizeof(dirTest_Diags) ,
 /* 6 */ rtDirectory, IOH_None           , (char*)"/Utilities"                       , (uint8_t*)dirUtilities               , sizeof(dirUtilities) ,
 /* 7 */ rtDirectory, IOH_None           , (char*)"/Text + PETSCII + Docs"           , (uint8_t*)dirTEXT_PETSCII            , sizeof(dirTEXT_PETSCII) ,

};

