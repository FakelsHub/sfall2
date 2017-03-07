/*
 *    sfall
 *    Copyright (C) 2008, 2009, 2010, 2011, 2012  The sfall team
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "main.h"

#include "AI.h"
#include "Arrays.h"
#include "Console.h"
#include "Criticals.h"
#include "Define.h"
#include "FileSystem.h"
#include "FalloutEngine.h"
#include "graphics.h"
#include "HeroAppearance.h"
#include "input.h"
#include "Inventory.h"
#include "Knockback.h"
#include "LoadGameHook.h"
#include "Logging.h"
#include "movies.h"
#include "PartyControl.h"
#include "perks.h"
#include "ScriptExtender.h"
#include "skills.h"
#include "sound.h"
#include "SuperSave.h"
#include "version.h"

#define MAX_GLOBAL_SIZE (MaxGlobalVars*12 + 4)

static DWORD InLoop = 0;
DWORD GainStatFix = 0;

DWORD GetCurrentLoops() {return InLoop;}

static const DWORD GainPerks[] = {
 0x4AF11F, 0x4AF181, 0x4AF19D, 0x4AF1BD, 0x4AF214, 0x4AF230, 0x4AF24B,
};

static void ModifyGainXXXPerks() {
 for (int i = 0; i < sizeof(GainPerks)/4; i++) {
  SafeWrite8(GainPerks[i], 0xEB);           // jmps
 }
}

static void RestoreGainXXXPerks() {
 for (int i = 0; i < sizeof(GainPerks)/4; i++) {
  SafeWrite8(GainPerks[i], 0x74);           // jz
 }
}

static void _stdcall ResetState(DWORD onLoad) {
 if (!onLoad) FileSystemReset();
 ClearGlobalScripts();
 ClearGlobals();
 ForceGraphicsRefresh(0);
 WipeSounds();
 if(GraphicsMode>3) graphics_OnGameLoad();
 Knockback_OnGameLoad();
 Skills_OnGameLoad();
 InLoop=0;
 GainStatFix=0;
 RestoreGainXXXPerks();
 PerksReset();
 InventoryReset();
 RegAnimCombatCheck(1);
 AfterAttackCleanup();
 PartyControlReset();
}

void GetSavePath(char* buf, int type) {
 int saveid = *(int*)_slot_cursor + 1 + LSPageOffset;//add SuperSave Page offset
 char buf2[6];
 //Fallout saving is independent of working directory
 struct sPath {
  char* path;
  int a;
  int b;
  sPath* next;
 };
 sPath* spath=*(sPath**)_paths;
 while(spath->a&&spath->next) spath=spath->next;

 //strcpy_s(buf, MAX_PATH, **(char***)_paths);
 strcpy_s(buf, MAX_PATH, spath->path);
 strcat_s(buf, MAX_PATH, "\\savegame\\slot");
 _itoa_s(saveid, buf2, 10);
 if(strlen(buf2)==1) strcat_s(buf, MAX_PATH, "0");
 strcat_s(buf, MAX_PATH, buf2);
 switch(type) {
  case 0: strcat_s(buf, MAX_PATH, "\\sfallgv.sav"); break;
  case 1: strcat_s(buf, MAX_PATH, "\\sfallfs.sav"); break;
 }
}

static char SaveSfallDataFailMsg[128];
static void _stdcall _SaveGame() {
 char buf[MAX_PATH];
 GetSavePath(buf, 0);

#ifdef TRACE
 dlog_f("Saving game: %s\r\n", DL_MAIN, buf);
#endif

 DWORD unused;
 DWORD unused2 = 0;
 HANDLE h = CreateFileA(buf, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
 if (h != INVALID_HANDLE_VALUE) {
  SaveGlobals(h);
  WriteFile(h, &unused2, 4, &unused, 0);
  WriteFile(h, &GainStatFix, 4, &unused, 0);
  PerksSave(h);
  SaveArrays(h);
 } else {
  dlogr("ERROR creating sfallgv!", DL_MAIN);
#ifdef TRACE
  dlog_f("sfallgv.sav: %d\r\n", DL_MAIN, GetLastError());
#endif
  __asm {
   mov  eax, offset SaveSfallDataFailMsg
   call display_print_
  }
  PlaySfx("IISXXXX1");
 }
 CloseHandle(h);                            // Что за фигня?
 GetSavePath(buf, 1);
 h = CreateFileA(buf, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
 if (h != INVALID_HANDLE_VALUE) {
  FileSystemSave(h);
#ifdef TRACE
 } else {
  dlog_f("sfallfs.sav: %d\r\n", DL_MAIN, GetLastError());
#endif
 }
 CloseHandle(h);                            // Что за фигня?
}

static DWORD SaveInCombatFix;
static char SaveFailMsg[128];
static void __declspec(naked) SaveGame_hook() {
 __asm {
  pushad
  test byte ptr ds:[_combat_state], 1       // В бою?
  jz   canSave                              // Нет
  cmp  IsControllingNPC, 0
  jne  skip
  cmp  SaveInCombatFix, 0
  je   canSave
  cmp  SaveInCombatFix, 2
  je   skip
  mov  eax, ds:[_obj_dude]
  mov  ebx, [eax+0x40]                      // curr_mp
  mov  edx, STAT_max_move_points
  push eax
  call stat_level_
  cmp  eax, ebx
  pop  eax
  jne  skip
  mov  edx, PERK_bonus_move
  call perk_level_
  shl  eax, 1
  cmp  eax, ds:[_combat_free_move]
  je   canSave
skip:
  popad
  mov  eax, offset SaveFailMsg              // "Cannot save at this time"
  call display_print_
  xor  eax, eax
  retn
canSave:
  popad
  or   InLoop, SAVEGAME
  push offset return
  push ebx
  push ecx
  push edx
  push esi
  push edi
  mov  esi, 0x47B891
  jmp  esi
return:
  and  InLoop, (-1^SAVEGAME)
  cmp  eax, 1
  jne  end
  pushad
  call _SaveGame
  popad
end:
  retn
 }
}

static void _stdcall LoadGame2_After() {
 char buf[MAX_PATH];
 GetSavePath(buf, 0);

#ifdef TRACE
 dlog_f("Loading save game: %s\r\n", DL_MAIN, buf);
#endif

 ClearGlobals();
 HANDLE h = CreateFileA(buf, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
 if (h != INVALID_HANDLE_VALUE) {
  DWORD size = 0;
  DWORD unused;
  LoadGlobals(h);
  ReadFile(h, (&unused), 4, &size, 0);
  ReadFile(h, &GainStatFix, 4, &size, 0);
  if (!size) GainStatFix = 0;
  else {
   PerksLoad(h);
   LoadArrays(h);
  }
  CloseHandle(h);
 } else {
  GainStatFix = 0;
  dlogr("Cannot read sfallgv.sav - assuming non-sfall save.", DL_MAIN);
 }

 if (GainStatFix) ModifyGainXXXPerks();
 else RestoreGainXXXPerks();

 LoadGlobalScripts();
 CritLoad();
 LoadHeroAppearance();
}

// should be called before savegame is loaded
static void __declspec(naked) LoadSlot_hook() {
 __asm {
  pushad
  push 1
  call ResetState
  popad
  push ebx
  push ecx
  push edx
  push esi
  push edi
  mov  ebx, 0x47DC6D
  jmp  ebx
 }
}

static void __declspec(naked) LoadGame_hook() {
 __asm {
  or   InLoop, LOADGAME
  push offset return
  push ebx
  push ecx
  push edx
  push esi
  push edi
  mov  esi, 0x47C645
  jmp  esi
return:
  and  InLoop, (-1^LOADGAME)
  cmp  eax, 1
  jne  end
  pushad
  call LoadGame2_After
  popad
end:
  retn
 }
}

static void NewGame2() {
 ResetState(0);

 dlogr("Starting new game", DL_MAIN);

 SetNewCharAppearanceGlobals();

 if (GetPrivateProfileInt("Misc", "GainStatPerkFix", 1, ini)) {
  ModifyGainXXXPerks();
  GainStatFix = 1;
 } else {
  RestoreGainXXXPerks();
  GainStatFix = 0;
 }

 if (GetPrivateProfileInt("Misc", "PipBoyAvailableAtGameStart", 0, ini)) {
  SafeWrite8(_gmovie_played_list + 0x3, 1);
 }

 if (GetPrivateProfileInt("Misc", "DisableHorrigan", 0, ini)) {
  *(DWORD*)0x672E04 = 1;
 }

 LoadGlobalScripts();
 CritLoad();
}

static void __declspec(naked) main_game_loop_call() {
 __asm {
  pushad
  call NewGame2
  popad
  jmp  main_game_loop_
 }
}

static void __declspec(naked) main_menu_loop_call() {
 __asm {
  pushad
  push 0
  call ResetState
  call LoadHeroAppearance
  popad
  jmp  main_menu_loop_
 }
}

static void __declspec(naked) wmWorldMapFunc_hook() {
 __asm {
  or   InLoop, WORLDMAP
  push offset return
  push ebx
  push ecx
  push edx
  push esi
  push edi
  mov  edx, 0x4BFE15
  jmp  edx
return:
  and  InLoop, (-1^WORLDMAP)
  retn
 }
}

static void __declspec(naked) combat_hook() {
 __asm {
  pushad
  call AICombatStart
  popad
  or   InLoop, COMBAT
  push offset return
  push ebx
  push ecx
  push edx
  push esi
  push edi
  mov  esi, 0x422D31
  jmp  esi
return:
  and  InLoop, (-1^COMBAT)
  pushad
  call AICombatEnd
  popad
  retn
 }
}

static void __declspec(naked) combat_input_hook() {
 __asm {
  or   InLoop, PCOMBAT
  push offset return
  push ebx
  push ecx
  push edx
  push esi
  mov  ecx, ds:[_obj_dude]
  mov  esi, 0x4227FE
  jmp  esi
return:
  and  InLoop, (-1^PCOMBAT)
  retn
 }
}

static void __declspec(naked) do_optionsFunc_hook() {
 __asm {
  or   InLoop, ESCMENU
  push offset return
  push ebx
  push ecx
  push edx
  push esi
  push edi
  mov  esi, 0x48FC55
  jmp  esi
return:
  and  InLoop, (-1^ESCMENU)
  retn
 }
}

static void __declspec(naked) do_prefscreen_hook() {
 __asm {
  or   InLoop, OPTIONS
  push offset return
  push ebx
  push edx
  xor  edx, edx
  dec  edx
  mov  eax, 0x49079F
  jmp  eax
return:
  and  InLoop, (-1^OPTIONS)
  retn
 }
}

static void __declspec(naked) game_help_hook() {
 __asm {
  or   InLoop, HELP
  push offset return
  push ebx
  push ecx
  push edx
  push esi
  push edi
  mov  eax, 0x443F79
  jmp  eax
return:
  and  InLoop, (-1^HELP)
  retn
 }
}

static void __declspec(naked) editor_design_hook() {
 __asm {
  or   InLoop, CHARSCREEN
  pushad
  call PerksEnterCharScreen
  popad
  push offset return
  push ebx
  push ecx
  push edx
  push esi
  push edi
  mov  esi, 0x431DFD
  jmp  esi
return:
  pushad
  test eax, eax
  jz   success
  call PerksCancelCharScreen
  jmp  end
success:
  call PerksAcceptCharScreen
end:
  popad
  and  InLoop, (-1^CHARSCREEN)
  retn
 }
}

static void __declspec(naked) gdProcess_hook() {
 __asm {
  or   InLoop, DIALOG
  push offset return
  push ebx
  push ecx
  push edx
  push esi
  push edi
  mov  edx, 0x4465C5
  jmp  edx
return:
  and  InLoop, (-1^DIALOG)
  retn
 }
}

static void __declspec(naked) pipboy_hook() {
 __asm {
  or   InLoop, PIPBOY
  push offset return
  push ebx
  push ecx
  push edx
  push esi
  xchg ecx, eax
  mov  eax, 0x49700A
  jmp  eax
return:
  and  InLoop, (-1^PIPBOY)
  retn
 }
}

static void __declspec(naked) skilldex_select_hook() {
 __asm {
  or   InLoop, SKILLDEX
  push offset return
  push edx
  xor  edx, edx
  dec  edx
  mov  eax, 0x4ABFD6
  jmp  eax
return:
  and  InLoop, (-1^SKILLDEX)
  retn
 }
}

static void __declspec(naked) handle_inventory_hook() {
 __asm {
  or   InLoop, INVENTORY
  push offset return
  push ebx
  push ecx
  push edx
  push esi
  push edi
  mov  eax, 0x46E7B5
  jmp  eax
return:
  and  InLoop, (-1^INVENTORY)
  retn
 }
}

static void __declspec(naked) automap_hook() {
 __asm {
  or   InLoop, AUTOMAP
  push offset return
  push ebx
  push ecx
  push esi
  push edi
  push ebp
  mov  esi, 0x41B8C1
  jmp  esi
return:
  and  InLoop, (-1^AUTOMAP)
  retn
 }
}

static void __declspec(naked) use_inventory_on_hook() {
 __asm {
  or   InLoop, INTFACEUSE
  push offset return
  push ebx
  push ecx
  push edx
  push esi
  push edi
  mov  esi, 0x4717E9
  jmp  esi
return:
  and  InLoop, (-1^INTFACEUSE)
  retn
 }
}

static void __declspec(naked) loot_container_hook() {
 __asm {
  or   InLoop, INTFACELOOT
  push offset return
  push ebx
  push ecx
  push esi
  push edi
  push ebp
  mov  ebx, 0x473909
  jmp  ebx
return:
  and  InLoop, (-1^INTFACELOOT)
  retn
 }
}

static void __declspec(naked) barter_inventory_hook() {
 __asm {
  or   InLoop, BARTER
  push [esp+4]
  push offset return
  push esi
  push edi
  push ebp
  sub  esp, 0x34
  mov  edi, 0x4757F6
  jmp  edi
return:
  and  InLoop, (-1^BARTER)
  retn 4
 }
}

void LoadGameHookInit() {
 GetPrivateProfileString("sfall", "SaveInCombat", "Cannot save at this time", SaveFailMsg, 128, translationIni);
 SaveInCombatFix = GetPrivateProfileInt("Misc", "SaveInCombatFix", 1, ini);
 if (SaveInCombatFix < 0 || SaveInCombatFix > 2) SaveInCombatFix = 0;

 GetPrivateProfileString("sfall", "SaveSfallDataFail", "ERROR saving extended savegame information! Check if other programs interfere with savegame files/folders and try again!", SaveSfallDataFailMsg, 128, translationIni);

 HookCall(0x480AB3, &main_game_loop_call);
 MakeCall(0x47DC68, &LoadSlot_hook, true);
 MakeCall(0x47C640, &LoadGame_hook, true);  // LOADGAME
 MakeCall(0x47B88C, &SaveGame_hook, true);  // SAVEGAME
 HookCall(0x480A28, &main_menu_loop_call);
 MakeCall(0x4BFE10, &wmWorldMapFunc_hook, true);// WORLDMAP
 MakeCall(0x422D2C, &combat_hook, true);    // COMBAT
 MakeCall(0x4227F4, &combat_input_hook, true);// PCOMBAT
 MakeCall(0x48FC50, &do_optionsFunc_hook, true);// ESCMENU
 MakeCall(0x490798, &do_prefscreen_hook, true);// OPTIONS
 MakeCall(0x443F74, &game_help_hook, true); // HELP
 MakeCall(0x431DF8, &editor_design_hook, true);// CHARSCREEN
 MakeCall(0x4465C0, &gdProcess_hook, true); // DIALOG
 MakeCall(0x497004, &pipboy_hook, true);    // PIPBOY
 MakeCall(0x4ABFD0, &skilldex_select_hook, true);// SKILLDEX
 MakeCall(0x46E7B0, &handle_inventory_hook, true);// INVENTORY
 MakeCall(0x41B8BC, &automap_hook, true);   // AUTOMAP
 MakeCall(0x4717E4, &use_inventory_on_hook, true);// INTFACEUSE
 MakeCall(0x473904, &loot_container_hook, true);// INTFACELOOT
 MakeCall(0x4757F0, &barter_inventory_hook, true);// BARTER
}
