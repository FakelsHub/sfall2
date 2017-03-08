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
#include "Sound.h"
#include "SuperSave.h"
#include "version.h"

#define MAX_GLOBAL_SIZE (MaxGlobalVars*12 + 4)

DWORD InLoop = 0;

DWORD GetCurrentLoops() {return InLoop;}

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
 PerksReset();
 InventoryReset();
 RegAnimCombatCheck(1);
 AfterAttackCleanup();
 PartyControlReset();
}

void GetSavePath(char* buf, char* ftype) {
 sprintf(buf, "%s\\savegame\\slot%.2d\\sfall%s.sav", *(char**)_patches, *(DWORD*)_slot_cursor + 1 + LSPageOffset, ftype);
}

static char SaveSfallDataFailMsg[128];
static void _stdcall _SaveGame() {
 char buf[MAX_PATH];
 GetSavePath(buf, "gv");

#ifdef TRACE
 dlog_f("Saving game: %s\r\n", DL_MAIN, buf);
#endif

 DWORD size, unused = 0;
 HANDLE h = CreateFileA(buf, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
 if (h != INVALID_HANDLE_VALUE) {
  SaveGlobals(h);
  WriteFile(h, &unused, 4, &size, 0);
  unused++;
  WriteFile(h, &unused, 4, &size, 0);
  PerksSave(h);
  SaveArrays(h);
  CloseHandle(h);
 } else {
  dlogr("ERROR creating sfallgv!", DL_MAIN);
#ifdef TRACE
  dlog_f("sfallgv.sav: %d\r\n", DL_MAIN, GetLastError());
#endif
  __asm {
   mov  eax, offset SaveSfallDataFailMsg
   call display_print_
   mov  eax, 0x50CC04                       // 'iisxxxx1'
   call gsound_play_sfx_file_
  }
 }
 GetSavePath(buf, "fs");
 h = CreateFileA(buf, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
 if (h != INVALID_HANDLE_VALUE) {
  FileSystemSave(h);
  CloseHandle(h);
#ifdef TRACE
 } else {
  dlog_f("sfallfs.sav: %d\r\n", DL_MAIN, GetLastError());
#endif
 }
}

static DWORD SaveInCombatFix;
static char SaveFailMsg[128];
static void __declspec(naked) SaveGame_hook() {
 __asm {
  pushad
  test byte ptr ds:[_combat_state], 1       // В бою?
  jz   canSave                              // Нет
  xor  eax, eax
  cmp  IsControllingNPC, eax
  jne  skip
  cmp  SaveInCombatFix, eax
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
  push 0x47B891
  retn
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
 GetSavePath(buf, "gv");

#ifdef TRACE
 dlog_f("Loading save game: %s\r\n", DL_MAIN, buf);
#endif

 ClearGlobals();
 HANDLE h = CreateFileA(buf, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
 if (h != INVALID_HANDLE_VALUE) {
  DWORD size, unused[2];
  LoadGlobals(h);
  ReadFile(h, &unused, 8, &size, 0);
  if (size == 8) {
   PerksLoad(h);
   LoadArrays(h);
  }
  CloseHandle(h);
 } else {
  dlogr("Cannot read sfallgv.sav - assuming non-sfall save.", DL_MAIN);
#ifdef TRACE
  dlog_f("sfallgv.sav: %d\r\n", DL_MAIN, GetLastError());
#endif
 }

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
  push 0x47DC6D
  retn
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
  push 0x47C645
  retn
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

 LoadGlobalScripts();
 CritLoad();
}

static bool DisableHorrigan = false;
static void __declspec(naked) main_game_loop_call() {
 __asm {
  pushad
  call NewGame2
  mov  al, DisableHorrigan
  mov  ds:[_Meet_Frank_Horrigan], al
  popad
  jmp  main_game_loop_
 }
}

static bool PipBoyAvailableAtGameStart = false;
static void __declspec(naked) main_menu_loop_call() {
 __asm {
  pushad
  push 0
  call ResetState
  mov  al, PipBoyAvailableAtGameStart
  mov  ds:[_gmovie_played_list+3], al
  call LoadHeroAppearance                   // Зачем?
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
  push 0x4BFE15
  retn
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
  push 0x422D31
  retn
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
  push 0x4227FE
  retn
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
  push 0x48FC55
  retn
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
  push 0x49079F
  retn
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
  push 0x443F79
  retn
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
  push 0x431DFD
  retn
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
  push 0x4465C5
  retn
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
  push 0x49700A
  retn
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
  push 0x4ABFD6
  retn
return:
  and  InLoop, (-1^SKILLDEX)
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
  push 0x41B8C1
  retn
return:
  and  InLoop, (-1^AUTOMAP)
  retn
 }
}

static void __declspec(naked) setup_inventory_hook() {
 __asm {
  mov  esi, 6
  test eax, eax
  jnz  notZero
  or   InLoop, INVENTORY
  retn
notZero:
  dec  eax
  jnz  notOne
  or   InLoop, INTFACEUSE
  retn
notOne:
  dec  eax
  jnz  notTwo
  or   InLoop, INTFACELOOT
  retn
notTwo:
  or   InLoop, BARTER
  retn
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
 MakeCall(0x41B8BC, &automap_hook, true);   // AUTOMAP
 MakeCall(0x46EC9D, &setup_inventory_hook, false);// INVENTORY + INTFACEUSE + INTFACELOOT + BARTER

 PipBoyAvailableAtGameStart = GetPrivateProfileIntA("Misc", "PipBoyAvailableAtGameStart", 0, ini) != 0;
 DisableHorrigan = GetPrivateProfileIntA("Misc", "DisableHorrigan", 0, ini) != 0;

}
