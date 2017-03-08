/*
 *    sfall
 *    Copyright (C) 2008, 2009, 2010  The sfall team
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

#include <string>
#include <vector>
#include "Define.h"
#include "FalloutEngine.h"
#include "HookScripts.h"
#include "Inventory.h"
#include "Logging.h"
#include "PartyControl.h"
#include "ScriptExtender.h"

#define MAXDEPTH (8)
static const int numHooks = HOOK_COUNT;

struct sHookScript {
 sScriptProgram prog;
 int callback;                              // proc number in script's proc table
 bool isGlobalScript;                       // false for hs_* scripts, true for gl* scripts
};

static std::vector<sHookScript> hooks[numHooks];

DWORD InitingHookScripts;

static DWORD args[16];                      // current hook arguments
static DWORD oldargs[8*MAXDEPTH];
static DWORD* argPtr;
static DWORD rets[16];                      // current hook return values

static DWORD firstArg = 0;
static DWORD callDepth;
static DWORD lastCount[MAXDEPTH];

static DWORD ArgCount; 
static DWORD cArg;                          // how many arguments were taken by current hook script
static DWORD cRet;                          // how many return values were set by current hook script
static DWORD cRetTmp;                       // how many return values were set by specific hook script (when using register_hook)

extern void item_add_mult_call();

#define hookbegin(a) __asm pushad __asm call BeginHook __asm popad __asm mov ArgCount, a
#define hookend __asm pushad __asm call EndHook __asm popad

static void _stdcall BeginHook() {
 if (callDepth <= MAXDEPTH) {
  if (callDepth) {
   lastCount[callDepth-1] = ArgCount;
   memcpy(&oldargs[8*(callDepth-1)], args, 8*sizeof(DWORD));
  }
  argPtr = args;
  for (DWORD i = 0; i < callDepth; i++) argPtr += lastCount[i];
 }
 callDepth++;
}

static void _stdcall EndHook() {
 callDepth--;
 if (callDepth && callDepth <= MAXDEPTH) {
  ArgCount = lastCount[callDepth-1];
  memcpy(args, &oldargs[8*(callDepth-1)], 8*sizeof(DWORD));
 }
}

static void _stdcall RunSpecificHookScript(sHookScript *hook) {
 cArg = 0;
 cRetTmp = 0;
 if (hook->callback != -1) RunScriptProcByNum(hook->prog.ptr, hook->callback);
 else RunScriptProc(&hook->prog, start);
}

static void _stdcall RunHookScript(DWORD hook) {
 if (hooks[hook].size()) {
#ifdef TRACE
  char buf[256];
  sprintf_s(buf, "Running hook %d, which has %0d entries attached", hook, hooks[hook].size());
  dlogr(buf, DL_HOOK);
#endif
  cRet = 0;
  for (int i = hooks[hook].size()-1; i >= 0; i--) RunSpecificHookScript(&hooks[hook][i]);
 } else {
  cArg = 0;
  cRet = 0;
 }
}

static void __declspec(naked) determine_to_hit_func_hook() {
 __asm {
  hookbegin(4)
  push [esp+8]                              // is_ranged
  push [esp+8]                              // hit_mode
  push offset return
  push esi
  push edi
  push ebp
  sub  esp, 0x2C
  mov  args[4], eax                         // source
  mov  args[8], ebx                         // target
  mov  args[12], ecx                        // body_part
  mov  edi, 0x4243AE
  jmp  edi
return:
  mov  args[0], eax
  pushad
  push HOOK_TOHIT
  call RunHookScript
  popad
  cmp  cRet, 0
  je   end
  mov  eax, rets[0]
end:
  hookend
  retn 8
 }
}

static void __declspec(naked) compute_attack_hook() {
 __asm {
  hookbegin(5)
  mov  args[0], eax                         // ROLL_*
  mov  ebx, [esi]                           // ebx = ctd.source
  mov  args[4], ebx
  mov  ebx, [esi+0x20]                      // ebx = ctd.target
  mov  args[8], ebx
  mov  ebx, [esi+0x28]                      // ebx = ctd.body_part
  mov  args[12], ebx
  mov  ebx, [esp+0x18+4]                    // ebx = chance_to_hit
  mov  args[16], ebx
  pushad
  push HOOK_AFTERHITROLL
  call RunHookScript
  popad
  mov  ebx, cRet
  dec  ebx
  jz   roll
  dec  ebx
  jz   bodypart
  dec  ebx
  jl   end
  mov  ebx, rets[8]
  mov  [esi+0x20], ebx                      // ctd.target
bodypart:
  mov  ebx, rets[4]
  mov  [esi+0x28], ebx                      // ctd.body_part
roll:
  mov  eax, rets[0]
end:
  hookend
  xchg ebx, eax
  cmp  ebx, ROLL_FAILURE
  retn
 }
}

static void __declspec(naked) item_w_mp_cost_hook() {
 __asm {
  hookbegin(4)
  push offset return
  push ecx
  push esi
  push edi
  push ebp
  sub  esp, 0xC
  mov  args[0], eax                         // source
  mov  args[4], edx                         // hit_mode
  mov  args[8], ebx                         // is_secondary
  mov  edi, 0x478B2B
  jmp  edi
return:
  mov  args[12], eax
  pushad
  push HOOK_CALCAPCOST
  call RunHookScript
  popad
  cmp  cRet, 0
  je   end
  mov  eax, rets[0]
end:
  hookend
  retn
 }
}

// this is for using non-weapon items, always 2 AP in vanilla
static void __declspec(naked) item_mp_cost_hook() {
 __asm {
  hookbegin(4)
  mov  args[0], ecx                         // source
  mov  args[4], edx                         // hit_mode (to determine hand)
  mov  args[8], ebx                         // is_secondary
  xor  eax, eax
  inc  eax
  inc  eax                                  // vanilla value
  mov  args[12], eax
  pushad
  push HOOK_CALCAPCOST
  call RunHookScript
  popad
  cmp  cRet, 0
  je   end
  mov  eax, rets[0]
end:
  hookend
  retn
 }
}

static void __declspec(naked) pick_death_call() {
 __asm {
  hookbegin(4)
  mov  args[24], ebx                        // оригинальный weapon
  push ebx
  test ebx, ebx
  jz   noWeapon
  mov  ebx, [ebx+0x64]
  inc  ebx
noWeapon:
  dec  ebx
  mov  args[0], ebx                         // weapon.pid или -1 если без оружия
  pop  ebx
  mov  args[4], eax                         // source
  mov  args[8], edx                         // target
  mov  args[12], ecx                        // amount
  mov  args[20], 0
  pushad
  push HOOK_DEATHANIM1
  call RunHookScript
  cmp  cRet, 0
  je   noNewPid
  sub  esp, 4
  mov  edx, rets[0]
  mov  args[0], edx
  mov  eax, esp
  call obj_pid_new_
  add  esp, 4
  inc  eax
  jz   noNewPid
  mov  args[20], eax
  mov  eax, [esp-4]
  mov  args[24], eax
noNewPid:
  popad
  push [esp+8]                              // hit_from_back
  push [esp+8]                              // animation
  mov  eax, args[4]                         // source
  mov  ebx, args[24]                        // weapon
  call pick_death_
  mov  args[16], eax                        // calculated animID
  mov  ArgCount, 5
  pushad
  push HOOK_DEATHANIM2
  call RunHookScript
  popad
  cmp  cRet, 0
  je   skip
  mov  eax, rets[0]
skip:
  cmp  args[20], 0
  je   end
  push eax
  mov  eax, args[24]
  xor  edx, edx
  call obj_erase_object_
  pop  eax
end:
  hookend
  retn 8
 }
}

static void __declspec(naked) check_death_call() {
 __asm {
  hookbegin(5)
  call check_death_                         // call original function
  mov  args[0], -1                          // weaponPid, -1
  mov  ebx, [esp+0x3C]
  mov  args[4], ebx                         // source
  mov  args[8], esi                         // target
  mov  ebx, [esp+0xC]
  mov  args[12], ebx                        // amount
  mov  args[16], eax                        // calculated animID
  pushad
  push HOOK_DEATHANIM2
  call RunHookScript
  popad
  cmp  cRet, 0
  je   end
  mov  eax, rets[0]
end:
  hookend
  retn
 }
}

static void __declspec(naked) compute_damage_hook() {
 __asm {
  hookbegin(11)
  push eax                                  // Сохраним указатель на ctd
  push offset return
  push ecx
  push esi
  push edi
  push ebp
  sub  esp, 0x34
  mov  args[32], ebx                        // DamageMultiplier
  mov  args[36], edx                        // rounds
  mov  esi, 0x4247BF
  jmp  esi
return:
  pop  edx                                  // Восстановим указатель на ctd
  mov  eax, [edx]                           // ctd.source
  mov  args[4], eax
  mov  ebx, [edx+0x14]                      // ctd.flagsSource
  mov  args[20], ebx
// Исправление отображения полученного дамага при мгновенной смерти
  test bl, 0x80                             // DAM_DEAD?
  jz   skipSource                           // Нет
//  test eax, eax
//  jz   skipSource
  call critter_get_hits_
  cmp  eax, [edx+0x10]
  jle  skipSource
  mov  [edx+0x10], eax                      // ctd.amountSource
skipSource:
  mov  eax, [edx+0x10]                      // ctd.amountSource
  mov  args[12], eax
  mov  eax, [edx+0x20]                      // ctd.target
  mov  args[0], eax
  mov  ebx, [edx+0x30]                      // ctd.flagsTarget
  mov  args[16], ebx
// Исправление отображения полученного дамага при мгновенной смерти
  test bl, 0x80                             // DAM_DEAD?
  jz   skipTarget                           // Нет
//  test eax, eax
//  jz   skipTarget
  call critter_get_hits_
  cmp  eax, [edx+0x2C]
  jle  skipTarget
  mov  [edx+0x2C], eax                      // ctd.amountTarget
skipTarget:
  mov  eax, [edx+0x2C]                      // ctd.amountTarget
  mov  args[8], eax
  mov  eax, [edx+0x8]                       // ctd.weapon
  mov  args[24], eax
  mov  eax, [edx+0x28]                      // ctd.body_part
  mov  args[28], eax
  mov  eax, [edx+0x34]                      // ctd.knockbackValue
  mov  args[40], eax
  pushad
  push HOOK_COMBATDAMAGE
  call RunHookScript
  popad
  mov  ebx, cRet
  dec  ebx
  jz   amountTarget
  dec  ebx
  jz   amountSource
  dec  ebx
  jz   flagsTarget
  dec  ebx
  jz   flagsSource
  dec  ebx
  jl   end
  mov  eax, rets[16]
  mov  [edx+0x34], eax                      // ctd.knockbackValue
flagsSource:
  mov  eax, rets[12]
  mov  [edx+0x14], eax                      // ctd.flagsSource
flagsTarget:
  mov  eax, rets[8]
  mov  [edx+0x30], eax                      // ctd.flagsTarget
amountSource:
  mov  eax, rets[4]
  mov  [edx+0x10], eax                      // ctd.amountSource
amountTarget:
  mov  eax, rets[0]
  mov  [edx+0x2C], eax                      // ctd.amountTarget
end:
  hookend
  retn
 }
}

static void __declspec(naked) critter_kill_hook() {
 __asm {
  hookbegin(1)
  push offset return
  push ecx
  push esi
  push edi
  push ebp
  sub  esp, 0x28
  mov  args[0], eax
  mov  esi, 0x42DA6B
  jmp  esi
return:
  pushad
  push HOOK_ONDEATH
  call RunHookScript
  call EndHook
  popad
  retn
 }
}

static void __declspec(naked) damage_object_hook() {
 __asm {
  hookbegin(1)
  mov  args[0], eax
  call partyMemberRemove_
  pushad
  push HOOK_ONDEATH
  call RunHookScript
  call EndHook
  popad
  retn
 }
}

static void __declspec(naked) ai_danger_source_hook() {
 __asm {
  hookbegin(5)
  mov  args[0], esi                         // source
  mov  edi, [eax]
  mov  args[4], edi
  mov  edi, [eax+4]
  mov  args[8], edi
  mov  edi, [eax+8]
  mov  args[12], edi
  mov  edi, [eax+12]
  mov  args[16], edi
  pushad
  push HOOK_FINDTARGET
  call RunHookScript
  popad
  cmp  cRet, 4
  jge  skip
  call qsort_
  jmp  end
skip:
  mov  edi, rets[0]
  mov  [eax], edi
  mov  edi, rets[4]
  mov  [eax+4], edi
  mov  edi, rets[8]
  mov  [eax+8], edi
  mov  edi, rets[12]
  mov  [eax+12], edi
end:
  hookend
  retn
 }
}

static void __declspec(naked) protinst_use_item_on_hook() {
 __asm {
  hookbegin(3)
  mov  args[0], edx                         // target
  mov  args[4], eax                         // source
  mov  args[8], ebx                         // item
  pushad
  push HOOK_USEOBJON
  call RunHookScript
  popad
  cmp  cRet, 0
  jne  skip
defaulthandler:
  push offset return
  push ecx
  push esi
  push edi
  push ebp
  sub  esp, 0x14
  mov  esi, 0x49C3D3
  jmp  esi
skip:
  cmp  rets[0], -1
  je   defaulthandler
  mov  eax, rets[0]
return:
  hookend
  retn
 }
}

void __declspec(naked) useobjon_item_d_take_drug_() {
 __asm {
  hookbegin(3)
  mov  args[0], eax                         // target
  mov  args[4], eax                         // source
  mov  args[8], edx                         // item
  pushad
  push HOOK_USEOBJON
  call RunHookScript
  popad
  cmp  cRet, 0
  je   defaulthandler
  cmp  rets[0], -1
  je   defaulthandler
  mov  eax, rets[0]
  cmp  eax, 1
  je   end
  xor  eax, eax
  dec  eax
  jmp  end
defaulthandler:
  call item_d_take_drug_
end:
  hookend
  retn
 }
}

static void __declspec(naked) protinst_use_item_hook() {
 __asm {
  hookbegin(2)
  mov  args[0], eax                         // source
  mov  args[4], edx                         // item
  pushad
  push HOOK_USEOBJ
  call RunHookScript
  popad
  cmp  cRet, 0
  jne  skip
defaulthandler:
  push offset return
  push ebx
  push ecx
  sub  esp, 0x10
  mov  ecx, 0x49BF3D
  jmp  ecx
skip:
  cmp  rets[0], -1
  je   defaulthandler
  mov  eax, rets[0]
//  test eax, eax
//  jnz  return
//  dec  eax
return:
  hookend
  retn
 }
}

static void __declspec(naked) item_remove_mult_hook() {
 __asm {
  hookbegin(4)
  push ecx
  mov  ecx, [esp+4]
  mov  args[0], eax                         // source
  mov  args[4], edx                         // item
  mov  args[8], ebx                         // count
  mov  args[12], ecx                        // return address
  pushad
  push HOOK_REMOVEINVENOBJ
  call RunHookScript
  call EndHook
  popad
  push esi
  push edi
  push ebp
  sub  esp, 0xC
  mov  ecx, 0x477497
  jmp  ecx                                  // item_remove_mult_
 }
}

static void __declspec(naked) barter_compute_value_hook() {
 __asm {
  hookbegin(6)
  push offset return
  push ebx
  push ecx
  push edi
  push ebp
  mov  ebp, esp
  mov  args[0], eax                         // source
  mov  args[4], edx                         // target
  mov  ecx, 0x474B32
  jmp  ecx
return:
  mov  args[8], eax                         // default value of the goods
  push eax
  mov  eax, ds:[_btable]
  mov  args[12], eax
  push eax
  call item_caps_total_
  mov  args[16], eax
  pop  eax
  call item_total_cost_
  mov  args[20], eax
  pop  eax
  pushad
  push HOOK_BARTERPRICE
  call RunHookScript
  popad
  cmp  cRet, 0
  je   end
  mov  eax, rets[0]
end:
  hookend
  retn
 }
}

static void __declspec(naked) critter_compute_ap_from_distance_hook() {
 __asm {
  hookbegin(3)
  push offset return
  push ebx
  mov  ebx, [eax+0x64]                      // pobj.pid
  shr  ebx, 0x18
  cmp  ebx, ObjType_Critter
  mov  args[0], eax                         // source
  mov  args[4], edx                         // distance
  mov  ebx, 0x42E636
  jmp  ebx
return:
  mov  args[8], eax
  pushad
  push HOOK_MOVECOST
  call RunHookScript
  popad
  cmp  cRet, 0
  je   end
  mov  eax, rets[0]
end:
  hookend
  retn
 }
}

static void __declspec(naked) obj_blocking_at_hook() {
 __asm {
  hookbegin(4)
  push offset return
  push ecx
  push esi
  push edi
  push ebp
  mov  ecx, eax
  mov  args[0], eax                         // source
  mov  args[4], edx                         // tile
  mov  args[8], ebx                         // elev
  mov  esi, 0x48B84E
  jmp  esi
return:
  mov  args[12], eax
  pushad
  push HOOK_HEXMOVEBLOCKING
  call RunHookScript
  popad
  cmp  cRet, 0
  je   end
  mov  eax, rets[0]
end:
  hookend
  retn
 }
}

static void __declspec(naked) obj_ai_blocking_at_hook() {
 __asm {
  hookbegin(4)
  push offset return
  push ecx
  push esi
  push edi
  push ebp
  mov  ecx, eax
  mov  args[0], eax                         // source
  mov  args[4], edx                         // tile
  mov  args[8], ebx                         // elev
  mov  esi, 0x48BA26
  jmp  esi
return:
  mov  args[12], eax
  pushad
  push HOOK_HEXAIBLOCKING
  call RunHookScript
  popad
  cmp  cRet, 0
  je   end
  mov  eax, rets[0]
end:
  hookend
  retn
 }
}

static void __declspec(naked) obj_shoot_blocking_at_hook() {
 __asm {
  hookbegin(4)
  push offset return
  push ecx
  push esi
  push edi
  push ebp
  mov  esi, eax
  mov  args[0], eax                         // source
  mov  args[4], edx                         // tile
  mov  args[8], ebx                         // elev
  mov  ecx, 0x48B936
  jmp  ecx
return:
  mov  args[12], eax
  pushad
  push HOOK_HEXSHOOTBLOCKING
  call RunHookScript
  popad
  cmp  cRet, 0
  je   end
  mov  eax, rets[0]
end:
  hookend
  retn
 }
}

static void __declspec(naked) obj_sight_blocking_at_hook() {
 __asm {
  hookbegin(4)
  push offset return
  push ecx
  mov  args[0], eax                         // source
  mov  args[4], edx                         // tile
  mov  args[8], ebx                         // elev
  xchg ecx, eax
  mov  edx, ds:[_objectTable][edx*4]
  mov  eax, 0x48BB92
  jmp  eax
return:
  mov  args[12], eax
  pushad
  push HOOK_HEXSIGHTBLOCKING
  call RunHookScript
  popad
  cmp  cRet, 0
  je   end
  mov  eax, rets[0]
end:
  hookend
  retn
 }
}

static void __declspec(naked) item_w_damage_hook() {
 __asm {
  hookbegin(6)
  mov  args[0], eax                         // min
  mov  args[4], edx                         // max
  mov  args[8], edi                         // weapon
  mov  args[12], ecx                        // source
  test edi, edi                             // Есть оружие в руке?
  jnz  skip                                 // Да
  add  esi, 8                               // Подправим hit_mode
skip:
  mov  args[16], esi                        // hit_mode
  mov  args[20], ebp                        // non-zero for weapon melee attack (Melee Damage|Урон х/ор.)
  pushad
  push HOOK_ITEMDAMAGE
  call RunHookScript
  popad
  cmp  cRet, 0
  je   runrandom
  mov  eax, rets[0]
  cmp  cRet, 1
  je   end
  mov  edx, rets[4]
runrandom:
  call roll_random_
end:
  hookend
  retn
 }
}

static void __declspec(naked) item_w_compute_ammo_cost_hook() {
 __asm {
  hookbegin(4)
  push offset return
  push ebx
  push esi
  push edi
  mov  args[0], eax                         // weapon
  test edx, edx
  jz   skip
  mov  ebx, [edx]
skip:
  mov  args[4], ebx                         // rounds in attack
  mov  ebx, 0x4790B1
  jmp  ebx
return:
  cmp  eax, -1
  je   end
  pushad
  mov  args[12], eax                        // type of hook (0)
  mov  eax, [edx]
  mov  args[8], eax                         // rounds as computed by game
  push HOOK_AMMOCOST
  call RunHookScript
  popad
  cmp  cRet, eax
  je   end
  mov  eax, rets[0]
  mov  [edx], eax                           // override result
  xor  eax, eax
end:
  hookend
  retn
 }
}

static void __declspec(naked) item_w_rounds_hook() {
 __asm {
  hookbegin(4)
  push offset return
  push edx
  sub  esp, 4
  mov  args[0], eax                         // weapon
  xor  edx, edx
  inc  edx
  mov  args[4], edx                         // rounds in attack
  inc  edx
  mov  args[12], edx                        // type of hook (2)
  test eax, eax
  mov  edx, 0x478D86
  jmp  edx
return:
  cmp  eax, -1
  je   end
  pushad
  mov  args[8], eax                         // rounds as computed by game
  push HOOK_AMMOCOST
  call RunHookScript
  popad
  cmp  cRet, 0
  je   end
  mov  eax, rets[0]                         // override result
end:
  hookend
  retn
 }
}

// Заглушка для отключения повторного вызова HOOK_AMMOCOST если это стрельба очередью
static void __declspec(naked) item_w_compute_ammo_cost_call() {
 __asm {
  mov  eax, [esp+0x1C+4]                    // animation
  cmp  eax, ANIM_fire_burst
  je   end
  cmp  eax, ANIM_fire_continuous
  je   end
  mov  eax, [esi+0x8]                       // ctd.weapon
  jmp  item_w_compute_ammo_cost_
end:
  xor  eax, eax                             // можно обойтись и без этого
  retn
 }
}

DWORD _stdcall KeyPressHook(DWORD dxKey, bool pressed, DWORD vKey) {
 DWORD result = 0;
 BeginHook();
 ArgCount = 3;
 args[0] = (DWORD)pressed;
 args[1] = dxKey;
 args[2] = vKey;
 RunHookScript(HOOK_KEYPRESS);
 if (cRet != 0) result = rets[0];
 EndHook();
 return result;
}

void _stdcall MouseClickHook(DWORD button, bool pressed) {
 BeginHook();
 ArgCount = 2;
 args[0] = (DWORD)pressed;
 args[1] = button;
 RunHookScript(HOOK_MOUSECLICK);
 EndHook();
}

static void __declspec(naked) skill_use_hook() {
 __asm {
  hookbegin(4)
  mov  args[0], eax                         // source
  mov  args[4], edx                         // target
  mov  args[8], ebx                         // skill
  mov  args[12], ecx                        // bonus
  pushad
  push HOOK_USESKILL
  call RunHookScript
  popad
  cmp  cRet, 0
  jne  skip
defaulthandler:
  push offset return
  push esi
  push edi
  push ebp
  sub  esp, 0xBC
  mov  esi, 0x4AAD11
  jmp  esi
skip:
  cmp  rets[0], -1
  je   defaulthandler
  mov  eax, rets[0]
return:
  hookend
  retn
 }
}

static void __declspec(naked) skill_check_stealing_hook() {
 __asm {
  hookbegin(4)
  mov  args[0], eax                         // source
  mov  args[4], edx                         // target
  mov  args[8], ebx                         // item
  mov  args[12], ecx                        // notsteal
  pushad
  push HOOK_STEAL
  call RunHookScript
  popad
  cmp  cRet, 0
  jne  skip
defaulthandler:
  push offset return
  push esi
  push edi
  push ebp
  sub  esp, 0x58
  mov  esi, 0x4ABBEA
  jmp  esi
skip:
  cmp  rets[0], -1
  je   defaulthandler
  mov  eax, rets[0]
return:
  hookend
  retn
 }
}

static void __declspec(naked) is_within_perception_hook() {
 __asm {
  hookbegin(3)
  push offset return
  push ebx
  push ecx
  push esi
  push edi
  push ebp
  mov  args[0], eax                         // source
  mov  args[4], edx                         // target
  mov  ecx, 0x42BA09
  jmp  ecx
return:
  mov  args[8], eax                         // check result
  pushad
  push HOOK_WITHINPERCEPTION
  call RunHookScript
  popad
  cmp  cRet, 0
  je   end
  mov  eax, rets[0]
end:
  hookend
  retn
 }
}

static void __declspec(naked) op_obj_can_see_obj_hook() { 
 __asm {
  call is_within_perception_
  cmp  eax, 2
  jne  end
  dec  eax
  mov  [esp+0x14], eax
  dec  eax
end:
  retn
 }
}

static void _declspec(naked) item_add_force_call() {
 __asm {
  cmp  dword ptr ds:[_curr_stack], 0
  je   force
// Была неудачная попытка поместить в сумку или зарядить оружие?
  jmp  item_add_mult_call
force:
  hookbegin(3)
  pushad
  mov  args[4], edx                         // item being moved
  xor  edx, edx
  mov  args[0], edx                         // target slot (0 - main backpack)
  mov  args[8], edx                         // no item being replaced here..
  push HOOK_INVENTORYMOVE
  call RunHookScript
  popad
  cmp  cRet, 0
  je   defaulthandler
  cmp  rets[0], -1
  je   defaulthandler
  xor  eax, eax
  dec  eax
  jmp  end
defaulthandler:
  call item_add_force_
end:
  hookend
  retn
 }
}

/*This hook is called every time an item is placed into either hand slot via inventory screen drag&drop
 If switch_hand_ function is not called, item is not placed anywhere (it remains in main inventory)*/
static void _declspec(naked) switch_hand_hook() {
 _asm {
  hookbegin(3)
  push eax
  call PartyControl_CanUseWeapon
  test eax, eax
  jnz  canUseWeapon
  call PartyControl_PrintWarning
  test eax, eax
canUseWeapon:
  pop  eax
  jz   return
  pushad
  mov  args[4], eax                         // item being moved
  call item_get_type_
  cmp  eax, item_type_ammo
  mov  eax, [edx]                           // itemReplaced
  mov  args[8], eax                         // other item
  jne  cont
  test eax, eax
  jz   cont
  call item_get_type_
  cmp  eax, item_type_weapon
  je   skip
cont:
  xor  eax, eax
  inc  eax
  cmp  edx, _i_lhand
  je   leftHand
  inc  eax
leftHand:
  mov  args[0], eax                         // target slot (1 - left hand, 2 - right hand)
  push HOOK_INVENTORYMOVE
  call RunHookScript
skip:
  popad
  cmp  cRet, 0
  je   defaulthandler
  cmp  rets[0], -1
  jne  return
defaulthandler:
  push offset return
  push esi
  push edi
  push ebp
  mov  ebp, eax
  mov  edi, 0x4714E5
  jmp  edi
return:
  hookend
  retn
 }
}

// This hack is called when an armor is dropped into the armor slot at inventory screen
static void _declspec(naked) inven_pickup_hook() {
 __asm {
  push eax
  call item_get_type_
  test eax, eax                             // Это item_type_armor?
  pop  eax
  jnz  skip                                 // Нет
  cmp  IsControllingNPC, 0
  je   cont
  call PartyControl_PrintWarning
skip:
  xor  eax, eax
  dec  eax
  retn
cont:
  hookbegin(3)
  mov  args[0], 3                           // target slot (3 - armor slot)
  mov  args[4], eax                         // item being moved
  mov  eax, ds:[_i_worn]
  mov  args[8], eax                         // other item
  pushad
  push HOOK_INVENTORYMOVE
  call RunHookScript
  popad
  xor  eax, eax
  cmp  cRet, eax
  je   end
  dec  eax
  cmp  rets[0], eax
  jne  end
  inc  eax
end:
  hookend
  retn
 }
}

static void _declspec(naked) drop_ammo_into_weapon_hook() {
 __asm {
  hookbegin(3)
  cmp  esi, -1
  je   end
  mov  args[0], 4                           // target slot (4 - weapon, when reloading it by dropping ammo)
  mov  eax, [esp+4]                         // ammo
  mov  args[4], eax                         // item being moved (ammo)
  mov  args[8], ebp                         // other item (weapon)
  pushad
  push HOOK_INVENTORYMOVE
  call RunHookScript
  popad
  xor  eax, eax
  cmp  cRet, eax
  je   skip
  dec  eax
  cmp  rets[0], eax
  je   skip
  xchg esi, eax
  jmp  end
skip:
  pop  eax
  mov  eax, 0x476588
  push eax
end:
  hookend
  retn
 }
}

void _declspec(naked) item_add_mult_call() {
 __asm {
  cmp  eax, ds:[_stack]                     // container == main backpack?
  jne  skip                                 // Нет
// Перемещение брони/вещи из руки (при открытой вложенной сумке) или из открытой вложенной сумки в основной рюкзак
  dec  dword ptr ds:[_curr_stack]
  call item_add_force_call
  inc  dword ptr ds:[_curr_stack]
  retn
skip:
  hookbegin(3)
  pushad
  mov  args[0], 5                           // target slot (5 - container)
  mov  args[4], edx                         // item being moved
  mov  args[8], eax                         // other item (container)
  push HOOK_INVENTORYMOVE
  call RunHookScript
  popad
  cmp  cRet, 0
  je   defaulthandler
  cmp  rets[0], -1
  je   defaulthandler
  xor  eax, eax
  dec  eax
  jmp  end
defaulthandler:
  call item_add_mult_
end:
  hookend
  retn
 }
}

static void _declspec(naked) invenWieldFunc_hook() {
 __asm {
  pushad
  xchg esi, eax                             // esi = source
  mov  eax, edx                             // eax = item
  call item_get_type_
  test eax, eax                             // item_type_armor?
  mov  eax, esi
  jnz  notArmor                             // Нет
  mov  ebx, 0x4000000                       // Worn
  call inven_worn_
  jmp  UnwieldOld
notArmor:
  test ebx, ebx
  jz   leftHand
  mov  ebx, 0x2000000                       // Right_Hand
  call inven_right_hand_
  jmp  UnwieldOld
leftHand:
  mov  ebx, 0x1000000                       // Left_Hand
  call inven_left_hand_
UnwieldOld:
  test eax, eax                             // Есть броня/вещь в руке?
  jz   skipUnwield                          // Нет
  xchg edx, eax                             // edx = снимаемая броня/вещь в руке
  xchg esi, eax                             // eax = source
  call correctFidForRemovedItem_
  test eax, eax                             // Удачно сняли?
skipUnwield:
  popad
  jnz  skipWield                            // Нет
  hookbegin(4)
  pushad
  mov  args[0], eax                         // source
  mov  args[4], edx                         // item
  mov  args[8], ebx                         // slot
  test ebx, ebx                             // INVEN_TYPE_RIGHT_HAND?
  jnz  skip                                 // Да
  inc  ebx
  mov  eax, edx
  call item_get_type_
  test eax, eax                             // item_type_armor?
  jz   skip                                 // Да
  inc  ebx
  mov  args[8], ebx                         // INVEN_TYPE_LEFT_HAND
  dec  ebx
skip:  
  mov  args[12], ebx                        // wield flag
  push HOOK_INVENWIELD
  call RunHookScript
  popad
  cmp  cRet, 0
  jne  end
defaulthandler:
  push offset return
  push esi
  push edi
  push ebp
  sub  esp, 0x1C
  mov  esi, 0x47276E
  jmp  esi
end:
  cmp  rets[0], -1
  je   defaulthandler
  mov  eax, rets[0]
  dec  eax
return:
  hookend
skipWield:
  retn
 }
}

// called when unwielding weapons
static void _declspec(naked) invenUnwieldFunc_hook() {
 __asm {
  hookbegin(4)
  pushad
  mov  args[0], eax                         // source
  test edx, edx                             // INVEN_TYPE_RIGHT_HAND?
  jnz  skip                                 // Да
  inc  edx
  inc  edx                                  // INVEN_TYPE_LEFT_HAND
skip:
  mov  args[8], edx                         // slot
  dec  edx
  jz   rightHand
  dec  edx
  call inven_left_hand_
  jmp  saveItem
rightHand:
  call inven_right_hand_
saveItem:
  mov  args[4], eax                         // item
  mov  args[12], edx                        // unwield flag
  push HOOK_INVENWIELD
  call RunHookScript
  popad
  cmp  cRet, 0
  jne  end
defaulthandler:
  push offset return
  push ecx
  push esi
  mov  esi, eax
  cmp  eax, ds:[_obj_dude]
  mov  ecx, 0x472A6E
  jmp  ecx
end:
  cmp  rets[0], -1
  je   defaulthandler
  mov  eax, rets[0]
  dec  eax
return:
  hookend
  retn
 }
}

static void _declspec(naked) correctFidForRemovedItem_hook() {
 __asm {
  hookbegin(4)
  pushad
  mov  args[0], eax                         // source
  mov  args[4], edx                         // item
  xor  eax, eax                             // INVEN_TYPE_WORN
  mov  args[12], eax                        // unwield flag
  test ebx, 0x3000000                       // Left_Hand or Right_Hand?
  jz   skip                                 // Нет, броня
  inc  eax                                  // INVEN_TYPE_RIGHT_HAND
  test ebx, 0x2000000                       // Right_Hand?
  jnz  skip                                 // Да
  inc  eax                                  // INVEN_TYPE_LEFT_HAND
skip:
  mov  args[8], eax                         // slot
  push HOOK_INVENWIELD
  call RunHookScript
  popad
  cmp  cRet, 0
  jne  end
defaulthandler:
  push offset return
  push ecx
  push esi
  push edi
  push ebp
  sub  esp, 0x14
  mov  esi, 0x4540A3
  jmp  esi
end:
  cmp  rets[0], -1
  je   defaulthandler
  mov  eax, rets[0]
  dec  eax
return:
  hookend
  retn
 }
}

DWORD _stdcall GetHSArgCount() {
 return ArgCount;
}

DWORD _stdcall GetHSArg() {
 if (cArg == ArgCount) return 0;
 else return args[cArg++];
}

void _stdcall SetHSArg(DWORD id, DWORD value) {
 if (id < ArgCount) args[id] = value;
}

DWORD* _stdcall GetHSArgs() {
 return args;
}

void _stdcall SetHSReturn(DWORD d) {
 if (cRetTmp < 8) rets[cRetTmp++] = d;
 if (cRetTmp > cRet) cRet = cRetTmp;
}

void _stdcall RegisterHook( DWORD script, DWORD id, DWORD procNum ) {
 if (id >= numHooks) return;
 for (std::vector<sHookScript>::iterator it = hooks[id].begin(); it != hooks[id].end(); ++it) {
  if (it->prog.ptr == script) {
   if (procNum == 0) hooks[id].erase(it); // unregister 
   return;
  }
 }
 sScriptProgram *prog = GetGlobalScriptProgram(script);
 if (prog) {
#ifdef TRACE
  dlog_f( "Global script %8x registered as hook id %d ", DL_HOOK, script, id);
#endif
  sHookScript hook;
  hook.prog = *prog;
  hook.callback = procNum;
  hook.isGlobalScript = true;
  hooks[id].push_back(hook);
 }
}

#define LoadHookScript(a,b) _LoadHookScript("data\\scripts\\hs_" a ".int", b)
static void _LoadHookScript(const char* path, int id) {
 if (id >= numHooks) return;
 WIN32_FIND_DATA file;
 HANDLE h;

 h = FindFirstFileA(path, &file);
 if(h != INVALID_HANDLE_VALUE) {
  sScriptProgram prog;
  dlog("Loading hook script: ", DL_HOOK);
  dlogr(path, DL_HOOK);
  char* fName = file.cFileName;
  fName[strlen(fName) - 4] = 0;
  LoadScriptProgram(prog, fName);
  FindClose(h);
  if (prog.ptr) {
   sHookScript hook;
   hook.prog = prog;
   hook.callback = -1;
   hook.isGlobalScript = false;
   hooks[id].push_back(hook);
   AddProgramToMap(prog);
  }
 }
}

static void HookScriptInit2() {
 dlogr("Initing hook scripts", DL_HOOK|DL_INIT);

 LoadHookScript("tohit", HOOK_TOHIT);
 MakeCall(0x4243A8, &determine_to_hit_func_hook, true);

 LoadHookScript("afterhitroll", HOOK_AFTERHITROLL);
 MakeCall(0x423893, &compute_attack_hook, false);

 LoadHookScript("calcapcost", HOOK_CALCAPCOST);
 MakeCall(0x478B24, &item_w_mp_cost_hook, true);
 MakeCall(0x478083, &item_mp_cost_hook, false);

 LoadHookScript("deathanim1", HOOK_DEATHANIM1);
 HookCall(0x4109DE, &pick_death_call);

 LoadHookScript("deathanim2", HOOK_DEATHANIM2);
 HookCall(0x410981, &check_death_call);
 HookCall(0x4109A1, &check_death_call);
 HookCall(0x4109BF, &check_death_call);

 LoadHookScript("combatdamage", HOOK_COMBATDAMAGE);
 MakeCall(0x4247B8, &compute_damage_hook, true);

 LoadHookScript("ondeath", HOOK_ONDEATH);
 MakeCall(0x42DA64, &critter_kill_hook, true);
 HookCall(0x425161, &damage_object_hook);

 LoadHookScript("findtarget", HOOK_FINDTARGET);
 HookCall(0x429143, &ai_danger_source_hook);

 LoadHookScript("useobjon", HOOK_USEOBJON);
 MakeCall(0x49C3CC, &protinst_use_item_on_hook, true);
 // the following hooks allows to catch drug use of AI and from action cursor
 HookCall(0x4285DF, &useobjon_item_d_take_drug_); // ai_check_drugs_
 HookCall(0x4286F8, &useobjon_item_d_take_drug_); // ai_check_drugs_
 HookCall(0x4287F8, &useobjon_item_d_take_drug_); // ai_check_drugs_
 HookCall(0x473573, &useobjon_item_d_take_drug_); // inven_action_cursor_

 LoadHookScript("useobj", HOOK_USEOBJ);
 MakeCall(0x49BF38, &protinst_use_item_hook, true);

 LoadHookScript("removeinvenobj", HOOK_REMOVEINVENOBJ);
 MakeCall(0x477490, &item_remove_mult_hook, true);

 LoadHookScript("barterprice", HOOK_BARTERPRICE);
 MakeCall(0x474B2C, &barter_compute_value_hook, true);

 LoadHookScript("movecost", HOOK_MOVECOST);
 MakeCall(0x42E62C, &critter_compute_ap_from_distance_hook, true);

 LoadHookScript("hexmoveblocking", HOOK_HEXMOVEBLOCKING);
 MakeCall(0x48B848, &obj_blocking_at_hook, true);

 LoadHookScript("hexaiblocking", HOOK_HEXAIBLOCKING);
 MakeCall(0x48BA20, &obj_ai_blocking_at_hook, true);

 LoadHookScript("hexshootblocking", HOOK_HEXSHOOTBLOCKING);
 MakeCall(0x48B930, &obj_shoot_blocking_at_hook, true);

 LoadHookScript("hexsightblocking", HOOK_HEXSIGHTBLOCKING);
 MakeCall(0x48BB88, &obj_sight_blocking_at_hook, true);

 LoadHookScript("itemdamage", HOOK_ITEMDAMAGE);
 HookCall(0x478560, &item_w_damage_hook);

 LoadHookScript("ammocost", HOOK_AMMOCOST);
 MakeCall(0x4790AC, &item_w_compute_ammo_cost_hook, true);
 MakeCall(0x478D80, &item_w_rounds_hook, true);
 HookCall(0x423A7C, &item_w_compute_ammo_cost_call);

 LoadHookScript("keypress", HOOK_KEYPRESS);
 LoadHookScript("mouseclick", HOOK_MOUSECLICK);

 LoadHookScript("useskill", HOOK_USESKILL);
 MakeCall(0x4AAD08, &skill_use_hook, true);

 LoadHookScript("steal", HOOK_STEAL);
 MakeCall(0x4ABBE4, &skill_check_stealing_hook, true);

 LoadHookScript("withinperception", HOOK_WITHINPERCEPTION);
 MakeCall(0x42BA04, &is_within_perception_hook, true);
 HookCall(0x456BA2, &op_obj_can_see_obj_hook);

 LoadHookScript("inventorymove", HOOK_INVENTORYMOVE);
 HookCall(0x471200, &item_add_force_call);
 MakeCall(0x4714E0, &switch_hand_hook, true);
 HookCall(0x47139C, &inven_pickup_hook);
 MakeCall(0x47657C, &drop_ammo_into_weapon_hook, false);
 HookCall(0x4764BC, &item_add_mult_call);

 LoadHookScript("invenwield", HOOK_INVENWIELD);
 MakeCall(0x472768, &invenWieldFunc_hook, true);
 MakeCall(0x472A64, &invenUnwieldFunc_hook, true);
 MakeCall(0x45409C, &correctFidForRemovedItem_hook, true);

 dlogr("Completed hook script init", DL_HOOK|DL_INIT);

}

void HookScriptClear() {
 for (int i = 0; i < numHooks; i++) hooks[i].clear();
}

void HookScriptInit() {
 isGlobalScriptLoading = 1; // this should allow to register global exported variables
 HookScriptInit2();
 InitingHookScripts = 1;
 for (int i = 0; i < numHooks; i++) {
  if (hooks[i].size()) InitScriptProgram(hooks[i][0].prog);// zero hook is always hs_*.int script because Hook scripts are loaded BEFORE global scripts
 }
 isGlobalScriptLoading = 0;
 InitingHookScripts = 0;
}

// run specific event procedure for all hook scripts
void _stdcall RunHookScriptsAtProc(DWORD procId) {
 for (int i = 0; i < numHooks; i++) {
  if (hooks[i].size() > 0 && !hooks[i][0].isGlobalScript) RunScriptProc(&hooks[i][0].prog, procId);
 }
}
