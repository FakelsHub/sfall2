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

#include <math.h>
#include <stdio.h>

#include "AI.h"
#include "AmmoMod.h"
#include "AnimationsAtOnceLimit.h"
#include "BarBoxes.h"
#include "Books.h"
#include "Bugs.h"
#include "BurstMods.h"
#include "console.h"
#include "Credits.h"
#include "Criticals.h"
#include "CRC.h"
#include "Define.h"
#include "Elevators.h"
#include "Explosions.h"
#include "FalloutEngine.h"
#include "FileSystem.h"
#include "Graphics.h"
#include "HeroAppearance.h"
#include "Inventory.h"
#include "KillCounter.h"
#include "knockback.h"
#include "LoadGameHook.h"
#include "Logging.h"
#include "MainMenu.h"
#include "movies.h"
#include "PartyControl.h"
#include "perks.h"
#include "Premade.h"
#include "QuestList.h"
#include "Reputations.h"
#include "ScriptExtender.h"
#include "skills.h"
#include "sound.h"
#include "stats.h"
#include "SuperSave.h"
#include "Tiles.h"
#include "timer.h"
#include "version.h"

char ini[65];
char translationIni[65];

static char mapName[65];
static char configName[65];
static char patchName[65];
static char versionString[65];
static char windowName[65];

static char smModelName[65];
char dmModelName[65];
static char sfModelName[65];
char dfModelName[65];

static const char* musicOverridePath = "data\\sound\\music\\";

bool npcautolevel;

DWORD MotionSensorFlags;

static int* scriptDialog;

static const DWORD FastShotFixF1[] = {
 0x478BB8, 0x478BC7, 0x478BD6, 0x478BEA, 0x478BF9, 0x478C08, 0x478C2F,
};

static const DWORD script_dialog_msgs[] = {
 0x4A50C2, 0x4A5169, 0x4A52FA, 0x4A5302, 0x4A6B86, 0x4A6BE0, 0x4A6C37,
};

static const DWORD EncounterTableSize[] = {
 0x4BD1A3, 0x4BD1D9, 0x4BD270, 0x4BD604, 0x4BDA14, 0x4BDA44, 0x4BE707,
 0x4C0815, 0x4C0D4A, 0x4C0FD4,
};

static const DWORD AgeMin[] = {
 0x51D860, 0x4373C9, 0x4373F3, 0x43754C,
};

static const DWORD AgeMax[] = {
 0x43731B, 0x437352, 0x437536,
};

//GetTickCount calls
static const DWORD offsetsA[] = {
 0x4C8D34, 0x4C9375, 0x4C9384, 0x4C93C0, 0x4C93E8, 0x4C9D2E, 0x4FE01E,
};

//Delayed GetTickCount calls
static const DWORD offsetsB[] = {
 0x4FDF64,
};

//timeGetTime calls
static const DWORD offsetsC[] = {
 0x4A3179, 0x4A325D, 0x4F482B, 0x4F4E53, 0x4F5542, 0x4F56CC, 0x4F59C6,
 0x4FE036,
};

static DWORD AddrGetTickCount;
static DWORD AddrGetLocalTime;

static DWORD ViewportX;
static DWORD ViewportY;

static const DWORD WalkDistance[] = {
 0x411FF0, 0x4121C4, 0x412475, 0x412906,
};

static const DWORD PutAwayWeapon[] = {
 0x411EA2, 0x412046, 0x41224A, 0x4606A5, 0x472996,
};

static const DWORD TimedRest[] = {
 0x499B0C, 0x499B4B, 0x499BE0, 0x499CE9, 0x499D2C, 0x499DF2,
};

static void __declspec(naked) game_time_date_hook() {
 __asm {
  test edi, edi
  jz   end
  mov  ecx, ds:[_pc_proto+0x134]            // _pc_proto.bonus_age
  add  esi, ecx
  mov  [edi], esi
end:
  mov  esi, 0x4A33BE
  jmp  esi
 }
}

static void __declspec(naked) TimerReset() {
 __asm {
  push edx
  mov  eax, ds:[_fallout_game_time]
  mov  ebx, 315360000
  xor  edx, edx
  div  ebx
  cmp  eax, 13                              // Количество прошедших лет < 13?
  jb   end                                  // Да
  imul ebx, ebx, 13                         // ebx = 315360000 * 13 = 4099680000 = 0xF45C2700
  sub  ds:[_fallout_game_time], ebx
  add  dword ptr ds:[_pc_proto+0x134], 13   // _pc_proto.bonus_age
  mov  eax, ds:[_queue]
loopQueue:
  test eax, eax
  jz   end
  cmp  [eax], ebx
  jnb  skip
  mov  [eax], ebx
skip:
  sub  [eax], ebx                           // queue.time
  mov  eax, [eax+0x10]                      // queue.next_queue
  jmp  loopQueue
end:
  mov  ebx, ds:[_game_user_wants_to_quit]
  pop  edx
  retn
 }
}

static void __declspec(naked) script_chk_timed_events_hook() {
 __asm {
  inc  dword ptr ds:[_fallout_game_time]
  call TimerReset
  mov  ecx, 0x4A3DFB
  jmp  ecx
 }
}

static void __declspec(naked) TimedRest_hook() {
 __asm {
  call set_game_time_
  push edx
  push ebx
  push eax
  call TimerReset
  pop  eax
  mov  edx, ds:[_fallout_game_time]
  cmp  edx, eax
  je   end
  mov  ebx, eax
  push edx
  sub  eax, [esp+0x5C]                      // current_minutes
  sub  edx, eax
  mov  [esp+0x5C], edx
  pop  edx
  sub  ebx, [esp+0x48]                      // current_hours
  sub  edx, ebx
  mov  [esp+0x48], edx
  pop  ebx
  pop  edx
  sub  edx, 315360000 * 13
  retn
end:
  pop  ebx
  pop  edx
  retn
 }
}

static void __declspec(naked) DebugMode() {
 __asm {
  call config_set_value_
  jmp  debug_register_env_
 }
}

static double TickFrac = 0;
static double MapMulti = 1;
static double MapMulti2 = 1;
void _stdcall SetMapMulti(float d) { MapMulti2=d; }
static DWORD _stdcall PathfinderFix2(DWORD perkLevel, DWORD ticks) {
 double d = MapMulti * MapMulti2;
 switch (perkLevel) {
  case 1: d*=0.75; break;
  case 2: d*=0.50; break;
  case 3: d*=0.25; break;
 }
 d = ((double)ticks)*d + TickFrac;
 TickFrac = modf(d, &d);
 return (DWORD)d;
}

static __declspec(naked) void PathfinderFix() {
 __asm {
  push eax
  mov  eax, ds:[_obj_dude]
  mov  edx, PERK_pathfinder
  call perk_level_
  push eax
  call PathfinderFix2
  jmp  inc_game_time_
 }
}

static double FadeMulti;
static __declspec(naked) void palette_fade_to_hook() {
 __asm {
  pushf
  push ebx
  fild [esp]
  fmul FadeMulti
  fistp [esp]
  pop  ebx
  popf
  jmp  fadeSystemPalette_
 }
}

static int mapSlotsScrollMax=27 * (17 - 7);

static void __declspec(naked) wmInterfaceScrollTabsStart_hook() {
 __asm {
  xchg ebx, eax
  mov  eax, ds:[0x672F10]
  test ebx, ebx
  jl   up
  cmp  eax, mapSlotsScrollMax
  jne  end
skip:
  pop  eax                                  // Уничтожаем адрес возврата
  pop  edi
  pop  esi
  pop  edx
  pop  ecx
  pop  ebx
  retn
up:
  test eax, eax
  jz   skip
end:
  pop  eax                                  // Уничтожаем адрес возврата
  push ebp
  xor  edx, edx
  jmp  eax
 }
}

static DWORD wp_delay = 0;
static DWORD wp_ticks;
static void __declspec(naked) worldmap_patch() {
 __asm {
  pushad
  call RunGlobalScripts3
  mov  ecx, wp_delay
  jecxz skip                                // world map speed patch is disabled
tck:
  mov  eax, wp_ticks
  call elapsed_time_
  cmp  eax, ecx
  jl   tck
  call get_time_
  mov  wp_ticks, eax
skip:
  popad
  jmp  get_input_
 }
}

static DWORD WorldMapEncounterRate;
static void __declspec(naked) wmWorldMapFunc_hook() {
 __asm {
  inc  dword ptr ds:[_wmLastRndTime]
  jmp  wmPartyWalkingStep_
 }
}

static void __declspec(naked) wmRndEncounterOccurred_hook() {
 __asm {
  xor  ecx, ecx
  cmp  edx, WorldMapEncounterRate
  retn
 }
}

static void __declspec(naked) apply_damage_hook() {
 __asm {
  xor  edx, edx
  inc  edx                                  // COMBAT_SUBTYPE_WEAPON_USED
  test byte ptr ds:[esi+0x15], dl           // ctd.flags2Source & DAM_HIT_?
  jz   end                                  // Нет
  inc  edx                                  // COMBAT_SUBTYPE_HIT_SUCCEEDED
end:
  retn
 }
}

static void __declspec(naked) wmWorldMap_reset_hook() {
 __asm {
  call wmWorldMapLoadTempData_
  mov  eax, ViewportX
  mov  ds:[_wmWorldOffsetX], eax
  mov  eax, ViewportY
  mov  ds:[_wmWorldOffsetY], eax
  retn
 }
}

HANDLE _stdcall FakeFindFirstFile(const char* str, WIN32_FIND_DATAA* data) {
 HANDLE h=FindFirstFileA(str,data);
 if(h==INVALID_HANDLE_VALUE) return h;
 while(strlen(data->cFileName)>12) {
  int i=FindNextFileA(h, data);
  if(i==0) {
   FindClose(h);
   return INVALID_HANDLE_VALUE;
  }
 }
 return h;
}
int _stdcall FakeFindNextFile(HANDLE h, WIN32_FIND_DATAA* data) {
 int i=FindNextFileA(h, data);
 while(strlen(data->cFileName)>12&&i) {
  i=FindNextFileA(h, data);
 }
 return i;
}

static void __declspec(naked) art_get_code_hook() {
 __asm {
  jge  good
  pop  eax                                  // Уничтожаем адрес возврата
  xor  eax, eax
  dec  eax
  add  esp, 4
  retn
good:
  push eax
  mov  eax, 16
  cmp  edx, 11
  je   end
  cmp  edx, 15
  jne  skip
  inc  eax
end:
  xchg edx, eax
  mov  [esp+8], edx
skip:
  pop  eax
  cmp  edx, 18
  retn
 }
}

static char KarmaGainMsg[128];
static char KarmaLossMsg[128];
static void _stdcall SetKarma(int value) {
 char buf[128];
 if (value > 0) {
  sprintf_s(buf, KarmaGainMsg, value);
 } else {
  sprintf_s(buf, KarmaLossMsg, -value);
 }
 __asm {
  lea  eax, buf
  call display_print_
 }
}

static void __declspec(naked) op_set_global_var_hook() {
 __asm {
  test eax, eax
  jnz  end
  pushad
  call game_get_global_var_
  sub  edx, eax
  test edx, edx
  jz   skip
  push edx
  call SetKarma
skip:
  popad
end:
  jmp  game_set_global_var_
 }
}

static void __declspec(naked) intface_item_reload_hook() {
 __asm {
  pushad
  mov  eax, ds:[_obj_dude]
  push eax
  call register_clear_
  xor  eax, eax
  inc  eax
  call register_begin_
  xor  edx, edx
  xor  ebx, ebx
  dec  ebx
  pop  eax                                  // _obj_dude
  call register_object_animate_
  call register_end_
  popad
  jmp  gsound_play_sfx_file_
 }
}

static void __declspec(naked) obj_shoot_blocking_at_hook() {
 __asm {
  je   itsCritter
  cmp  edx, ObjType_Wall
  retn
itsCritter:
  xchg edi, eax
  call critter_is_dead_                     // check if it's a dead critter
  xchg edi, eax
  test edi, edi
  retn
 }
}

// same logic as above, for different loop
static void __declspec(naked) obj_shoot_blocking_at_hook1() {
 __asm {
  je   itsCritter
  cmp  eax, ObjType_Wall
  retn
itsCritter:
  mov  eax, [edx]
  call critter_is_dead_
  test eax, eax
  retn
 }
}

static DWORD RetryCombatMinAP;
static void __declspec(naked) combat_turn_hook() {
 __asm {
  xor  eax, eax
retry:
  xchg ecx, eax
  mov  eax, esi
  push edx
  call combat_ai_
  pop  edx
process:
  cmp  dword ptr ds:[_combat_turn_running], 0
  jle  next
  call process_bk_
  jmp  process
next:
  mov  eax, [esi+0x40]                      // curr_mp
  cmp  eax, RetryCombatMinAP
  jl   end
  cmp  eax, ecx
  jne  retry
end:
  retn
 }
}

static DWORD SpeedInterfaceCounterAnims;
static void __declspec(naked) intface_rotate_numbers_hook() {
 __asm {
  push edi
  push ebp
  sub  esp, 0x54
  mov  edi, SpeedInterfaceCounterAnims
// ebx=old value, ecx=new value
  cmp  ebx, ecx
  je   end
  mov  ebx, ecx
  jg   decrease
  test edi, edi
  jnz  end
  dec  ebx
  jmp  end
decrease:
  cmp  ecx, 0
  jl   negative
  test edi, edi
  jnz  end
  inc  ebx
  jmp  end
negative:
  xor  ebx, ebx
end:
  mov  esi, 0x460BA6
  jmp  esi
 }
}

static DWORD KarmaFrmCount;
static DWORD* KarmaFrms;
static int* KarmaPoints;
static DWORD _stdcall DrawCardHook2() {
 int rep=**(int**)_game_global_vars;
 for(DWORD i=0;i<KarmaFrmCount-1;i++) {
  if(rep < KarmaPoints[i]) return KarmaFrms[i];
 }
 return KarmaFrms[KarmaFrmCount-1];
}
static void __declspec(naked) DrawCardHook() {
 __asm {
  cmp dword ptr ds:[_info_line], 10;
  jne skip;
  cmp eax, 0x30;
  jne skip;
  push ecx;
  push edx;
  call DrawCardHook2;
  pop edx;
  pop ecx;
skip:
  jmp DrawCard_
 }
}

static void __declspec(naked) action_use_skill_on_hook() {
 __asm {
  cmp  eax, ds:[_obj_dude]
  jne  end
  mov  eax, KILL_TYPE_robot
  retn
end:
  jmp  critter_kill_count_type_
 }
}

static void __declspec(naked) partyMember_init_hook() {
 __asm {
  imul eax, edx, 204                        // necessary memory = number of NPC records in party.txt * record size
  push eax
  call mem_malloc_                          // malloc the necessary memory
  pop  ebx
  mov  edx, 0x493D16
  jmp  edx                                  // call memset to set all malloc'ed memory to 0
 }
}

static void __declspec(naked) partyMemberGetAIOptions_hook() {
 __asm {
  imul edx, edx, 204
  mov  eax, ds:[_partyMemberAIOptions]      // get starting offset of internal NPC table
  mov  esi, 0x49423A
  jmp  esi                                  // eax+edx = offset of specific NPC record
 }
}

static void __declspec(naked) item_w_called_shot_hook() {
 __asm {
  mov  edx, 0x478E7F
  test eax, eax                             // does player have Fast Shot trait?
  jz   end                                  // skip ahead if no
  push edx
  mov  edx, ecx                             // hit_mode
  mov  eax, ebx                             // source
  call item_w_range_                        // get weapon's range
  pop  edx
  cmp  eax, 2                               // is weapon range less than or equal 2 (i.e. melee/unarmed attack)?
  jle  end                                  // skip ahead if yes
  mov  edx, 0x478E9B                        // otherwise, disallow called shot attempt
end:
  jmp  edx
 }
}

static void __declspec(naked) automap_hook() {
 __asm {
  mov  edx, PID_MOTION_SENSOR
  jmp  inven_pid_is_carried_ptr_
 }
}

static void __declspec(naked) op_obj_can_see_obj_hook() {
 __asm {
  push esi
  push edi
  push obj_shoot_blocking_at_               // arg3 check hex objects func pointer
  mov  esi, 0x20                            // arg2 flags, 0x20 = check shootthru
  mov  edi, 0x4163B7
  jmp  edi
 }
}

static byte XltTable[94];
static byte XltKey = 4;                     // 4 = Scroll Lock, 2 = Caps Lock, 1 = Num Lock
static void __declspec(naked) get_input_str_hook() {
 __asm {
  push ecx
  mov  cl, XltKey
  test byte ptr ds:[_kb_lock_flags], cl
  jz   end
  mov  ecx, offset XltTable
  and  eax, 0xFF
  mov  al, [ecx+eax-0x20]
end:
  mov  byte ptr [esp+esi+4], al
  mov  eax, 0x433F43
  jmp  eax
 }
}

static void __declspec(naked) get_input_str2_hook() {
 __asm {
  push edx
  mov  dl, XltKey
  test byte ptr ds:[_kb_lock_flags], dl
  jz   end
  mov  edx, offset XltTable
  and  eax, 0xFF
  mov  al, [edx+eax-0x20]
end:
  mov  byte ptr [esp+edi+4], al
  mov  eax, 0x47F369
  jmp  eax
 }
}

static void __declspec(naked) kb_next_ascii_English_US_hook() {
 __asm {
  mov  dh, [eax]
  cmp  dh, 0x1A                             // DIK_LBRACKET
  je   end
  cmp  dh, 0x1B                             // DIK_RBRACKET
  je   end
  cmp  dh, 0x27                             // DIK_SEMICOLON
  je   end
  cmp  dh, 0x28                             // DIK_APOSTROPHE
  je   end
  cmp  dh, 0x33                             // DIK_COMMA
  je   end
  cmp  dh, 0x34                             // DIK_PERIOD
  je   end
  cmp  dh, 0x30                             // DIK_B
end:
  mov  eax, 0x4CC35D
  jmp  eax
 }
}

static void __declspec(naked) pipboy_hook() {
 __asm {
  call get_input_
  cmp  eax, '1'
  jne  notOne
  mov  eax, 0x1F4
  jmp  click
notOne:
  cmp  eax, '2'
  jne  notTwo
  mov  eax, 0x1F8
  jmp  click
notTwo:
  cmp  eax, '3'
  jne  notThree
  mov  eax, 0x1F5
  jmp  click
notThree:
  cmp  eax, '4'
  jne  notFour
  mov  eax, 0x1F6
click:
  push eax
  mov  eax, 0x50CC50                        // 'ib1p1xx1'
  call gsound_play_sfx_file_
  pop  eax
notFour:
  retn
 }
}

static void __declspec(naked) FirstTurnAndNoEnemy() {
 __asm {
  xor  eax, eax
  test byte ptr ds:[_combat_state], 1
  jz   end                                  // Не в бою
  cmp  dword ptr ds:[_combatNumTurns], eax
  jne  end                                  // Это не первый ход
  call combat_should_end_
  test eax, eax                             // Враги есть?
  jz   end                                  // Да
  pushad
  mov  ecx, ds:[_list_total]
  mov  edx, ds:[_obj_dude]
  mov  edx, [edx+0x50]                      // team_num группы поддержки игрока
  mov  edi, ds:[_combat_list]
loopCritter:
  mov  eax, [edi]                           // eax = персонаж
  mov  ebx, [eax+0x50]                      // team_num группы поддержки персонажа
  cmp  edx, ebx                             // Братюня?
  je   nextCritter                          // Да
  mov  eax, [eax+0x54]                      // who_hit_me
  test eax, eax                             // В персонажа стреляли?
  jz   nextCritter                          // Нет
  cmp  edx, [eax+0x50]                      // Стреляли из группы поддержки игрока?
  jne  nextCritter                          // Нет
  popad
  dec  eax                                  // Разборки!!!
  retn
nextCritter:
  add  edi, 4                               // К следующему персонажу в списке
  loop loopCritter                          // Перебираем весь список
  popad
end:
  retn
 }
}

sMessage cantdothat = {675, 0, 0, 0};       // 'Я не могу этого сделать.'
static void __declspec(naked) FirstTurnCheckDist() {
 __asm {
  push eax
  push edx
  call obj_dist_
  cmp  eax, 1                               // Расстояние до объекта больше 1?
  pop  edx
  pop  eax
  jle  end                                  // Нет
  lea  edx, cantdothat
  mov  eax, _proto_main_msg_file
  call message_search_
  cmp  eax, 1
  jne  skip
  mov  eax, cantdothat.message
  call display_print_
skip:
  pop  eax                                  // Уничтожаем адрес возврата
  xor  eax, eax
  dec  eax
end:
  retn
 }
}

static void __declspec(naked) check_move_hook() {
 __asm {
  call FirstTurnAndNoEnemy
  test eax, eax                             // Это первый ход в бою и врагов нет?
  jnz  skip                                 // Да
  cmp  dword ptr [ecx], -1
  je   end
  retn
skip:
  xor  esi, esi
  dec  esi
end:
  pop  eax                                  // Уничтожаем адрес возврата
  mov  eax, 0x4180A7
  jmp  eax
 }
}

static void __declspec(naked) gmouse_bk_process_hook() {
 __asm {
  xchg edi, eax
  call FirstTurnAndNoEnemy
  test eax, eax                             // Это первый ход в бою и врагов нет?
  jnz  end                                  // Да
  xchg edi, eax
  cmp  eax, dword ptr [edx+0x40]
  jg   end
  retn
end:
  pop  eax                                  // Уничтожаем адрес возврата
  mov  eax, 0x44B8C5
  jmp  eax
 }
}

static void __declspec(naked) FakeCombatFix1() {
 __asm {
  push eax                                  // _obj_dude
  call FirstTurnAndNoEnemy
  test eax, eax                             // Это первый ход в бою и врагов нет?
  pop  eax
  jz   end                                  // Нет
  call FirstTurnCheckDist
end:
  jmp  action_get_an_object_
 }
}

static void __declspec(naked) FakeCombatFix2() {
 __asm {
  push eax                                  // _obj_dude
  call FirstTurnAndNoEnemy
  test eax, eax                             // Это первый ход в бою и врагов нет?
  pop  eax
  jz   end                                  // Нет
  call FirstTurnCheckDist
end:
  jmp  action_loot_container_
 }
}

static void __declspec(naked) FakeCombatFix3() {
 __asm {
  cmp  ds:[_obj_dude], eax
  jne  end
  push eax
  call FirstTurnAndNoEnemy
  test eax, eax                             // Это первый ход в бою и врагов нет?
  pop  eax
  jz   end                                  // Нет
  call FirstTurnCheckDist
end:
  jmp  action_use_an_item_on_object_
 }
}

static void __declspec(naked) wmTownMapFunc_hook() {
 __asm {
  cmp  edx, 0x31
  jl   end
  cmp  edx, ecx
  jge  end
  push edx
  sub  edx, 0x31
  lea  eax, ds:0[edx*8]
  sub  eax, edx
  pop  edx
  cmp  dword ptr [edi+eax*4+0x0], 0         // Visited
  je   end
  cmp  dword ptr [edi+eax*4+0x4], -1        // Xpos
  je   end
  cmp  dword ptr [edi+eax*4+0x8], -1        // Ypos
  je   end
  retn
end:
  pop  eax                                  // Уничтожаем адрес возврата
  mov  eax, 0x4C4976
  jmp  eax
 }
}

FILETIME ftCurr, ftPrev;
static void _stdcall _GetFileTime(char* filename) {
 char fname[65];
 sprintf_s(fname, "%s%s", "data\\", filename);
 HANDLE hFile = CreateFile(fname, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
 if (hFile != INVALID_HANDLE_VALUE) {
  GetFileTime(hFile, NULL, NULL, &ftCurr);
  CloseHandle(hFile);
 } else {
  ftCurr.dwHighDateTime = 0;
  ftCurr.dwLowDateTime = 0;
 };
}

static const char* commentFmt="%02d/%02d/%d  %02d:%02d:%02d";
static void _stdcall createComment(char* bufstr) {
 SYSTEMTIME stUTC, stLocal;
 char buf[30];
 GetSystemTime(&stUTC);
 SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
 sprintf_s(buf, commentFmt, stLocal.wDay, stLocal.wMonth, stLocal.wYear, stLocal.wHour, stLocal.wMinute, stLocal.wSecond);
 strcpy(bufstr, buf);
}

static DWORD AutoQuickSave = 0;
static void __declspec(naked) SaveGame_hook() {
 __asm {
  pushad
  mov  ecx, ds:[_slot_cursor]
  mov  dword ptr ds:[_flptr], eax
  test eax, eax
  jz   end                                  // Это пустой слот, можно записывать
  call db_fclose_
  push ecx
  push edi
  call _GetFileTime
  pop  ecx
  mov  edx, ftCurr.dwHighDateTime
  mov  ebx, ftCurr.dwLowDateTime
  jecxz nextSlot                            // Это первый слот
  cmp  edx, ftPrev.dwHighDateTime
  ja   nextSlot                             // Текущий слот записан позже предыдущего
  jb   end                                  // Текущий слот записан раньше предыдущего
  cmp  ebx, ftPrev.dwLowDateTime
  jbe  end
nextSlot:
  mov  ftPrev.dwHighDateTime, edx
  mov  ftPrev.dwLowDateTime, ebx
  inc  ecx
  cmp  ecx, AutoQuickSave                   // Последний слот+1?
  ja   firstSlot                            // Да
  mov  ds:[_slot_cursor], ecx
  popad
  mov  eax, 0x47B929
  jmp  eax
firstSlot:
  xor  ecx, ecx
end:
  mov  ds:[_slot_cursor], ecx
  mov  eax, ecx
  shl  eax, 4
  add  eax, ecx
  shl  eax, 3
  add  eax, _LSData+0x3D                    // eax->_LSData[_slot_cursor].Comment
  push eax
  call createComment
  popad
  xor  ebx, ebx
  inc  ebx
  mov  ds:[_quick_done], ebx
  mov  ebx, 0x47B9A4
  jmp  ebx
 }
}

static char LvlMsg[6];
static char extraFmt[16]="%d/%d  (%s: %d)";
static void __declspec(naked) partyMemberCurLevel() {
 __asm {
  mov  ebx, eax                             // _dialog_target
  sub  esp, 4
  mov  edx, esp
  call partyMemberGetAIOptions_
  add  esp, 4
  inc  eax
  test eax, eax
  jz   skip
  mov  eax, ebx                             // _dialog_target
  call partyMemberGetCurLevel_
skip:
  push eax
  mov  eax, offset LvlMsg
  push eax
  mov  edx, STAT_max_hit_points
  mov  eax, ebx                             // _dialog_target
  call stat_level_
  push eax
  mov  edx, STAT_current_hp
  mov  eax, ebx                             // eax=_dialog_target
  call stat_level_
  push eax
  mov  eax, offset extraFmt
  push eax
  lea  eax, [esp+5*4]
  push eax
  call sprintf_
  add  esp, 6*4
  xchg ebx, eax                             // eax=_dialog_target
  mov  edx, 2                               // type = зависимость
  call queue_find_first_
  test eax, eax
  mov  al, ds:[_GreenColor]
  jz   end
  mov  al, ds:[_RedColor]
end:
  and  eax, 0xFF
  mov  ebx, 0x44909D
  jmp  ebx
 }
}

static char AcMsg[6];
static void __declspec(naked) partyMemberAC() {
 __asm {
  mov  ecx, eax                             // _dialog_target
  mov  edx, STAT_ac
  call stat_level_
  push eax
  mov  eax, offset AcMsg
  push eax
  xchg ecx, eax                             // eax=_dialog_target
  mov  edx, STAT_max_move_points
  call stat_level_
  push eax
  push ebx
  mov  eax, offset extraFmt
  push eax
  lea  eax, [esp+5*4]
  push eax
  call sprintf_
  add  esp, 6*4
  xor  eax, eax
  mov  edx, 0x44923D
  jmp  edx
 }
}

static void __declspec(naked) print_with_linebreak() {
 __asm {
  push esi
  push ecx
  test eax, eax                             // А есть строка?
  jz   end                                  // Нет
  mov  esi, eax
  xor  ecx, ecx
loopString:
  cmp  byte ptr [esi], 0                    // Конец строки
  je   printLine                            // Да
  cmp  byte ptr [esi], 0x5C                 // Возможно перевод строки? '\'
  jne  nextChar                             // Нет
  cmp  byte ptr [esi+1], 0x6E               // Точно перевод строки? 'n'
  jne  nextChar                             // Нет
  inc  ecx
  mov  byte ptr [esi], 0
printLine:
  call edi
  jecxz end
  dec  ecx
  mov  byte ptr [esi], 0x5C
  inc  esi
  mov  eax, esi
  inc  eax
nextChar:
  inc  esi
  jmp  loopString
end:
  pop  ecx
  pop  esi
  retn
 }
}

static void __declspec(naked) display_print_with_linebreak() {
 __asm {
  push edi
  mov  edi, display_print_
  call print_with_linebreak
  pop  edi
  retn
 }
}

static void __declspec(naked) inven_display_msg_with_linebreak() {
 __asm {
  push edi
  mov  edi, inven_display_msg_
  call print_with_linebreak
  pop  edi
  retn
 }
}

static char EncounterMsg[128];
static void __declspec(naked) wmSetupRandomEncounter_hook() {
 __asm {
  push eax
  push edi
  push 0x500B64                             // '%s %s'
  lea  eax, EncounterMsg
  push eax
  xchg edi, eax
  call sprintf_
  add  esp, 4*4
  xchg edi, eax
  jmp  display_print_
 }
}

static int drugExploit = 0;
static void __declspec(naked) protinst_use_item_hook() {
 __asm {
  dec  drugExploit
  call obj_use_book_
  inc  drugExploit
  retn
 }
}

static void __declspec(naked) UpdateLevel_hook() {
 __asm {
  inc  drugExploit
  call perks_dialog_
  dec  drugExploit
  retn
 }
}

static void __declspec(naked) skill_level_hook() {
 __asm {
  dec  drugExploit
  call skill_level_
  inc  drugExploit
  retn
 }
}

static void __declspec(naked) SliderBtn_hook() {
 __asm {
  dec  drugExploit
  call skill_inc_point_
  inc  drugExploit
  retn
 }
}

static void __declspec(naked) SliderBtn_hook1() {
 __asm {
  dec  drugExploit
  call skill_dec_point_
  inc  drugExploit
  retn
 }
}

static void __declspec(naked) stat_level_hook1() {
 __asm {
  call stat_get_bonus_
  cmp  esi, STAT_lu                         // Проверяем только силу-удачу
  ja   end
//  test eax, eax                             // А есть хоть какой [+/-]бонус?
//  jz   end                                  // Нет
  cmp  drugExploit, 0                       // Вызов из нужных мест?
  jl   checkPenalty                         // Проверка чтения книг/скилла
  jg   noBonus                              // Получение перков
  retn
checkPenalty:
  cmp  eax, 1                               // Положительный эффект?
  jge  end                                  // Да - учитываем его
noBonus:
  xor  eax, eax                             // Не учитываем эффект от наркотиков/радиации/etc
end:
  retn
 }
}

static void __declspec(naked) barter_attempt_transaction_hook() {
 __asm {
  cmp  dword ptr [eax+0x64], PID_ACTIVE_GEIGER_COUNTER
  je   found
  cmp  dword ptr [eax+0x64], PID_ACTIVE_STEALTH_BOY
  je   found
  mov  eax, 0x474D34
  jmp  eax
found:
  call item_m_turn_off_
  mov  eax, 0x474D17
  jmp  eax                                  // А есть ли ещё включённые предметы среди продаваемых?
 }
}

static void __declspec(naked) item_m_turn_off_hook() {
 __asm {
  and  byte ptr [eax+0x25], 0xDF            // Сбросим флаг использованного предмета
  jmp  queue_remove_this_
 }
}

static void __declspec(naked) register_object_take_out_hook() {
 __asm {
  push ecx
  push eax
  mov  ecx, edx                             // ID1
  mov  edx, [eax+0x1C]                      // cur_rot
  inc  edx
  push edx                                  // ID3
  xor  ebx, ebx                             // ID2
  mov  edx, [eax+0x20]                      // fid
  and  edx, 0xFFF                           // Index
  xor  eax, eax
  inc  eax                                  // ObjType
  call art_id_
  xor  ebx, ebx
  dec  ebx
  xchg edx, eax
  pop  eax
  call register_object_change_fid_
  pop  ecx
  xor  eax, eax
  retn
 }
}

static void DllMain2() {
 int tmp;
 //SafeWrite8(0x4B15E8, 0xc3);
 //SafeWrite8(0x4B30C4, 0xc3); //this is the one I need to override for bigger tiles
 dlogr("In DllMain2", DL_MAIN);

 //BlockCall(0x4123BC);

 if (GetPrivateProfileIntA("Speed", "Enable", 0, ini)) {
  dlog("Applying speed patch.", DL_INIT);
  AddrGetTickCount = (DWORD)&FakeGetTickCount;
  AddrGetLocalTime = (DWORD)&FakeGetLocalTime;

  for(int i=0;i<sizeof(offsetsA)/4;i++) {
   SafeWrite32(offsetsA[i], (DWORD)&AddrGetTickCount);
  }
  dlog(".", DL_INIT);
  for(int i=0;i<sizeof(offsetsB)/4;i++) {
   SafeWrite32(offsetsB[i], (DWORD)&AddrGetTickCount);
  }
  dlog(".", DL_INIT);
  for(int i=0;i<sizeof(offsetsC)/4;i++) {
   SafeWrite32(offsetsC[i], (DWORD)&AddrGetTickCount);
  }
  dlog(".", DL_INIT);

  SafeWrite32(0x4FDF58, (DWORD)&AddrGetLocalTime);
  TimerInit();
  dlogr(" Done", DL_INIT);
 }

 dlog("Applying input patch.", DL_INIT);
 SafeWriteStr(0x50FB70, "ddraw.dll");
 AvailableGlobalScriptTypes|=1;
 dlogr(" Done", DL_INIT);

 GraphicsMode = GetPrivateProfileIntA("Graphics", "Mode", 0, ini);
 if (GraphicsMode != 4 && GraphicsMode != 5) GraphicsMode = 0;
 if (GraphicsMode == 4 || GraphicsMode == 5) {
  dlog("Applying dx9 graphics patch.", DL_INIT);
  HMODULE h = LoadLibraryEx("d3dx9_43.dll", 0, LOAD_LIBRARY_AS_DATAFILE);
  if (!h) {
   MessageBoxA(0, "You have selected graphics mode 4 or 5, but d3dx9_43.dll is missing\nSwitch back to mode 0, or install an up to date version of DirectX", "Error", 0);
   ExitProcess(-1);
  } else {
   FreeLibrary(h);
  }
  SafeWrite8(0x0050FB6B, '2');
  dlogr(" Done", DL_INIT);
 }

 tmp = GetPrivateProfileIntA("Graphics", "FadeMultiplier", 100, ini);
 if (tmp != 100) {
  dlog("Applying fade patch.", DL_INIT);
  HookCall(0x493B16, &palette_fade_to_hook);
  FadeMulti = ((double)tmp)/100.0;
  dlogr(" Done", DL_INIT);
 }

 AmmoModInit();
 MoviesInit();

 mapName[64] = 0;
 if (GetPrivateProfileString("Misc", "StartingMap", "", mapName, 64, ini)) {
  dlog("Applying starting map patch.", DL_INIT);
  SafeWrite32(0x480AAA, (DWORD)&mapName);
  dlogr(" Done", DL_INIT);
 }

 versionString[64] = 0;
 if (GetPrivateProfileString("Misc", "VersionString", "", versionString, 64, ini)) {
  dlog("Applying version string patch.", DL_INIT);
  SafeWrite32(0x4B4588, (DWORD)&versionString);
  dlogr(" Done", DL_INIT);
 }

 configName[64] = 0;
 if (GetPrivateProfileString("Misc", "ConfigFile", "", configName, 64, ini)) {
  dlog("Applying config file patch.", DL_INIT);
  SafeWrite32(0x444BA5, (DWORD)&configName);
  SafeWrite32(0x444BCA, (DWORD)&configName);
  dlogr(" Done", DL_INIT);
 }

 patchName[64] = 0;
 if (GetPrivateProfileString("Misc", "PatchFile", "", patchName, 64, ini)) {
  dlog("Applying patch file patch.", DL_INIT);
  SafeWrite32(0x444323, (DWORD)&patchName);
  dlogr(" Done", DL_INIT);
 }

 smModelName[64] = 0;
 if (GetPrivateProfileString("Misc", "MaleStartModel", "", smModelName, 64, ini)) {
  dlog("Applying male start model patch.", DL_INIT);
  SafeWrite32(0x418B88, (DWORD)&smModelName);
  dlogr(" Done", DL_INIT);
 }

 sfModelName[64] = 0;
 if (GetPrivateProfileString("Misc", "FemaleStartModel", "", sfModelName, 64, ini)) {
  dlog("Applying female start model patch.", DL_INIT);
  SafeWrite32(0x418BAB, (DWORD)&sfModelName);
  dlogr(" Done", DL_INIT);
 }

 dmModelName[64] = 0;
 if (GetPrivateProfileString("Misc", "MaleDefaultModel", "", dmModelName, 64, ini)) {
  dlog("Applying male model patch.", DL_INIT);
  SafeWrite32(0x418B50, (DWORD)&dmModelName);
  dlogr(" Done", DL_INIT);
 }

 dfModelName[64] = 0;
 if (GetPrivateProfileString("Misc", "FemaleDefaultModel", "", dfModelName, 64, ini)) {
  dlog("Applying female model patch.", DL_INIT);
  SafeWrite32(0x418B6D, (DWORD)&dfModelName);
  dlogr(" Done", DL_INIT);
 }

 tmp = GetPrivateProfileInt("Misc", "StartYear", -1, ini);
 if (tmp >= 0) {
  dlog("Applying starting year patch.", DL_INIT);
  SafeWrite32(0x4A336C, tmp);
  dlogr(" Done", DL_INIT);
 }

 tmp = GetPrivateProfileInt("Misc", "StartMonth", -1, ini);
 if (tmp >= 1 && tmp <= 12) {
  dlog("Applying starting month patch.", DL_INIT);
  SafeWrite32(0x4A3382, tmp);
  dlogr(" Done", DL_INIT);
 }

 tmp = GetPrivateProfileInt("Misc", "StartDay", -1, ini);
 if (tmp >= 1 && tmp <= 31) {
  dlog("Applying starting day patch.", DL_INIT);
  SafeWrite8(0x4A3356, (BYTE)tmp);
  dlogr(" Done", DL_INIT);
 }

 tmp = GetPrivateProfileInt("Misc", "LocalMapXLimit", 0, ini);
 if (tmp) {
  dlog("Applying local map x limit patch.", DL_INIT);
  SafeWrite32(0x4B13B9, tmp);
  dlogr(" Done", DL_INIT);
 }

 tmp = GetPrivateProfileInt("Misc", "LocalMapYLimit", 0, ini);
 if (tmp) {
  dlog("Applying local map y limit patch.", DL_INIT);
  SafeWrite32(0x4B13C7, tmp);
  dlogr(" Done", DL_INIT);
 }

 tmp = GetPrivateProfileInt("Misc", "StartXPos", -1, ini);
 if (tmp != -1) {
  dlog("Applying starting x position patch.", DL_INIT);
  SafeWrite32(0x4BC990, tmp);
  SafeWrite32(0x4BCC08, tmp);
  dlogr(" Done", DL_INIT);
 }

 tmp = GetPrivateProfileInt("Misc", "StartYPos", -1, ini);
 if (tmp != -1) {
  dlog("Applying starting y position patch.", DL_INIT);
  SafeWrite32(0x4BC995, tmp);
  SafeWrite32(0x4BCC0D, tmp);
  dlogr(" Done", DL_INIT);
 }

 ViewportX = GetPrivateProfileInt("Misc", "ViewXPos", -1, ini);
 if (ViewportX != -1) {
  dlog("Applying starting x view patch.", DL_INIT);
  SafeWrite32(_wmWorldOffsetX, ViewportX);
  dlogr(" Done", DL_INIT);
 }

 ViewportY = GetPrivateProfileInt("Misc", "ViewYPos", -1, ini);
 if (ViewportY != -1) {
  dlog("Applying starting y view patch.", DL_INIT);
  SafeWrite32(_wmWorldOffsetY, ViewportY);
  dlogr(" Done", DL_INIT);
 }

 if (ViewportX != -1 || ViewportY != -1) HookCall(0x4BCF07, &wmWorldMap_reset_hook);

 dlog("Applying pathfinder patch.", DL_INIT);
 SafeWrite8(0x4C1FF7, 0xC0);               // sub eax, eax
 HookCall(0x4C1C78, &PathfinderFix);
 MapMulti = (double)GetPrivateProfileIntA("Misc", "WorldMapTimeMod", 100, ini)/100.0;
 dlogr(" Done", DL_INIT);

 if (*((DWORD*)0x4BFE5D) == 0x008D16E8) {
  HookCall(0x4BFE5D, &worldmap_patch);
  if (*((WORD*)0x4CAFB9) == 0x0000 && GetPrivateProfileIntA("Misc", "WorldMapFPSPatch", 0, ini)) {
   dlog("Applying world map fps patch.", DL_INIT);
   wp_delay = GetPrivateProfileIntA("Misc", "WorldMapDelay2", 66, ini);
   if (wp_delay == 0) wp_delay++;
   wp_ticks = GetTickCount();
   dlogr(" Done", DL_INIT);
  }
 }

 if (GetPrivateProfileIntA("Misc", "WorldMapEncounterFix", 0, ini)) {
  dlog("Applying world map encounter patch.", DL_INIT);
  WorldMapEncounterRate = GetPrivateProfileIntA("Misc", "WorldMapEncounterRate", 5, ini);
  SafeWrite32(0x4C232D, 0x01EBC031);        // xor eax, eax; jmps 0x4C2332
  HookCall(0x4BFEE0, &wmWorldMapFunc_hook);
  MakeCall(0x4C0667, &wmRndEncounterOccurred_hook, false);
  dlogr(" Done", DL_INIT);
 }

 if (GetPrivateProfileIntA("Misc", "DialogueFix", 1, ini)) {
  dlog("Applying dialogue patch.", DL_INIT);
  SafeWrite8(0x446848, 0x31);
  dlogr(" Done", DL_INIT);
 }

 if (GetPrivateProfileIntA("Misc", "ExtraKillTypes", 0, ini)) {
  dlog("Applying extra kill types patch.", DL_INIT);
  KillCounterInit();
  dlogr(" Done", DL_INIT);
 }

 dlog("Applying script extender patch.", DL_INIT);
 StatsInit();
 dlog(".", DL_INIT);
 ScriptExtenderSetup();
 dlog(".", DL_INIT);
 LoadGameHookInit();
 dlog(".", DL_INIT);
 PerksInit();
 dlog(".", DL_INIT);
 KnockbackInit();
 dlog(".", DL_INIT);
 SkillsInit();
 dlog(".", DL_INIT);

 //Ray's combat_p_proc fix - в действительности это не фикс, а расширение функционала combat_p_proc
 //Однако, чтобы получить профит от этого нужно править скрипты
 SafeWrite8(0x424DC7, 0x00);
 MakeCall(0x424DD9, &apply_damage_hook, false);
// SafeWrite8(0x424EA1, 0x00);
// MakeCall(0x424EB3, &apply_damage_hook, false);

 dlogr(" Done", DL_INIT);

 dlog("Applying world map cities list patch.", DL_INIT);
 MakeCall(0x4C21A1, &wmInterfaceScrollTabsStart_hook, false);
 dlogr(" Done", DL_INIT);

 dlog("Applying cities limit patch.", DL_INIT);
 if (*((BYTE*)0x4BF3BB) == 0x74) {
  SafeWrite8(0x4BF3BB, 0xEB);              // jmps
 }
 dlogr(" Done", DL_INIT);

 tmp = GetPrivateProfileIntA("Misc", "WorldMapSlots", 0, ini);
 if (tmp >= 7 && tmp < 128) {
  dlog("Applying world map slots patch.", DL_INIT);
  mapSlotsScrollMax = (tmp - 7) * 27;
  if (tmp < 25) tmp = 230 - (tmp - 17) * 27;
  else {
   tmp = 2 + 27 * (tmp - 26);
   SafeWrite8(0x4C21FC, 0xC2);
  }
  SafeWrite32(0x4C21FD, tmp);
  dlogr(" Done", DL_INIT);
 }

 tmp = GetPrivateProfileIntA("Misc", "TimeLimit", 13, ini);
 if (tmp < 13 && tmp > -4) {
  dlog("Applying time limit patch.", DL_INIT);
  if (tmp < -1) {
   MakeCall(0x4A33B8, &game_time_date_hook, true);
  }
  if (tmp < 0) {
   SafeWrite32(0x51D864, 99);
   HookCall(0x4A34F9, &TimerReset);         // inc_game_time_
   HookCall(0x4A3551, &TimerReset);         // inc_game_time_in_seconds_
   MakeCall(0x4A3DF5, &script_chk_timed_events_hook, true);
   for (int i = 0; i < sizeof(TimedRest)/4; i++) {
    HookCall(TimedRest[i], &TimedRest_hook);
   }
  } else {
   SafeWrite8(0x4A34EC, (BYTE)tmp);
   SafeWrite8(0x4A3544, (BYTE)tmp);
  }
  dlogr(" Done", DL_INIT);
 }

 tmp = GetPrivateProfileIntA("Debugging", "DebugMode", 0, ini);
 if (tmp && *((DWORD*)0x444A60) == 0xFFFE76FC && *((DWORD*)0x444A64) == 0x0F01FE83) {
  dlog("Applying DebugMode patch.", DL_INIT);
  HookCall(0x444A5F, &DebugMode);
  SafeWrite8(0x4C6D9B, 0xB8);               // mov  eax, offset ???
  if (tmp == 1) tmp = 0x50F928;             // "gnw"
  else tmp = 0x50F90C;                      // "log"
  SafeWrite32(0x4C6D9C, tmp);
  dlogr(" Done", DL_INIT);
 }

 npcautolevel = GetPrivateProfileIntA("Misc", "NPCAutoLevel", 0, ini) != 0;
 if (npcautolevel) {
  dlog("Applying npc autolevel patch.", DL_INIT);
  SafeWrite8(0x495CFB, 0xEB);           // jmps 0x495D28 (skip random check)
  dlogr(" Done", DL_INIT);
 }

 if (GetPrivateProfileIntA("Misc", "SingleCore", 1, ini)) {
  dlog("Applying single core patch.", DL_INIT);
  HANDLE process = GetCurrentProcess();
  SetProcessAffinityMask(process, 1);
  CloseHandle(process);
  dlogr(" Done", DL_INIT);
 }

 if (GetPrivateProfileIntA("Misc", "OverrideArtCacheSize", 0, ini)) {
  dlog("Applying override art cache size patch.", DL_INIT);
  SafeWrite8(0x41886A, 0x0);
  SafeWrite32(0x418872, 256);
  dlogr(" Done", DL_INIT);
 }

 char elevPath[MAX_PATH];
 GetPrivateProfileString("Misc", "ElevatorsFile", "", elevPath, MAX_PATH, ini);
 if (strlen(elevPath) > 0) {
  dlog("Applying elevator patch.", DL_INIT);
  ElevatorsInit(elevPath);
  dlogr(" Done", DL_INIT);
 }

 if (GetPrivateProfileIntA("Misc", "UseFileSystemOverride", 0, ini)) {
  FileSystemInit();
 }

 dlog("Applying print to file patch.", DL_INIT);
 SafeWrite32(0x6C0364, (DWORD)&FakeFindFirstFile);
 SafeWrite32(0x6C0368, (DWORD)&FakeFindNextFile);
 dlogr(" Done", DL_INIT);

 if (GetPrivateProfileIntA("Misc", "AdditionalWeaponAnims", 0, ini)) {
  dlog("Applying additional weapon animations patch.", DL_INIT);
  MakeCall(0x41931C, &art_get_code_hook, false);
  dlogr(" Done", DL_INIT);
 }

#ifdef TRACE
 if (GetPrivateProfileIntA("Debugging", "DontDeleteProtos", 0, ini)) {
  dlog("Applying permanent protos patch.", DL_INIT);
  SafeWrite8(0x48007E, 0xEB);
  dlogr(" Done", DL_INIT);
 }
#endif

 CritInit();

 tmp = GetPrivateProfileInt("Misc", "NumberPatchLoop", -1, ini);
 if (tmp > -1) {
  dlog("Applying load multiple patches patch. ", DL_INIT);
  SafeWrite8(0x444366, 0xEB);               // Disable check
  SafeWrite32(0x444357, tmp);               // New loop count
  SafeWrite8(0x444354, 0x90);               // Change step from 2 to 1
  dlogr(" Done", DL_INIT);
 }

 if (GetPrivateProfileInt("Misc", "DisplayKarmaChanges", 0, ini)) {
  dlog("Applying display karma changes patch. ", DL_INIT);
  GetPrivateProfileString("sfall", "KarmaGain", "You gained %d karma.", KarmaGainMsg, 128, translationIni);
  GetPrivateProfileString("sfall", "KarmaLoss", "You lost %d karma.", KarmaLossMsg, 128, translationIni);
  HookCall(0x455A6D, &op_set_global_var_hook);
  dlogr(" Done", DL_INIT);
 }

 if (GetPrivateProfileInt("Misc", "AlwaysReloadMsgs", 0, ini)) {
  dlog("Applying always reload messages patch. ", DL_INIT);
  SafeWrite16(0x4A6B83, 0x0CEB);            // jmps 0x4A6B91
  dlogr(" Done", DL_INIT);
 }

 if (GetPrivateProfileInt("Misc", "PlayIdleAnimOnReload", 0, ini)) {
  dlog("Applying idle anim on reload patch. ", DL_INIT);
  HookCall(0x460B8C, &intface_item_reload_hook);
  dlogr(" Done", DL_INIT);
 }

 if (GetPrivateProfileInt("Misc", "CorpseLineOfFireFix", 0, ini)) {
  dlog("Applying corpse line of fire patch. ", DL_INIT);
  MakeCall(0x48B98D, &obj_shoot_blocking_at_hook, false);
  MakeCall(0x48B9FD, &obj_shoot_blocking_at_hook1, false);
  dlogr(" Done", DL_INIT);
 }

 if (GetPrivateProfileIntA("Misc", "EnableHeroAppearanceMod", 0, ini)) {
  dlog("Setting up Appearance Char Screen buttons. ", DL_INIT);
  EnableHeroAppearanceMod();
  dlogr(" Done", DL_INIT);
 }

 if (GetPrivateProfileIntA("Misc", "SkipOpeningMovies", 0, ini)) {
  dlog("Blocking opening movies. ", DL_INIT);
  SafeWrite16(0x4809C7, 0x1CEB);            // jmps 0x4809E5
  dlogr(" Done", DL_INIT);
 }

 RetryCombatMinAP = GetPrivateProfileIntA("Misc", "NPCsTryToSpendExtraAP", 0, ini);
 if (RetryCombatMinAP > 0) {
  dlog("Applying retry combat patch. ", DL_INIT);
  HookCall(0x422B94, &combat_turn_hook);
  dlogr(" Done", DL_INIT);
 }

 dlog("Checking for changed skilldex images. ", DL_INIT);
 tmp = GetPrivateProfileIntA("Misc", "Lockpick", 293, ini);
 if (tmp != 293) SafeWrite32(0x518D54, tmp);
 tmp = GetPrivateProfileIntA("Misc", "Steal", 293, ini);
 if (tmp != 293) SafeWrite32(0x518D58, tmp);
 tmp = GetPrivateProfileIntA("Misc", "Traps", 293, ini);
 if (tmp != 293) SafeWrite32(0x518D5C, tmp);
 tmp = GetPrivateProfileIntA("Misc", "FirstAid", 293, ini);
 if (tmp != 293) SafeWrite32(0x518D4C, tmp);
 tmp = GetPrivateProfileIntA("Misc", "Doctor", 293, ini);
 if (tmp != 293) SafeWrite32(0x518D50, tmp);
 tmp = GetPrivateProfileIntA("Misc", "Science", 293, ini);
 if (tmp != 293) SafeWrite32(0x518D60, tmp);
 tmp = GetPrivateProfileIntA("Misc", "Repair", 293, ini);
 if (tmp != 293) SafeWrite32(0x518D64, tmp);
 dlogr(" Done", DL_INIT);

 if (GetPrivateProfileIntA("Misc", "RemoveWindowRounding", 0, ini)) {
  SafeWrite16(0x4B8090, 0x04EB);            // jmps 0x4B8096
 }

 dlogr("Running TilesInit().", DL_INIT);
 TilesInit();

 CreditsInit();

 char xltcodes[512];
 if (GetPrivateProfileStringA("Misc", "XltTable", "", xltcodes, 512, ini) > 0) {
  char *xltcode;
  int count = 0;
  xltcode = strtok(xltcodes, ",");
  while (xltcode) {
   int _xltcode = atoi(xltcode);
   if (_xltcode<32 || _xltcode>255) break;
   XltTable[count] = byte(_xltcode);
   if (count == 93) break;
   count++;
   xltcode = strtok(0, ",");
  }
  if (count == 93) {
   XltKey = GetPrivateProfileIntA("Misc", "XltKey", 4, ini);
   if (XltKey!=4 && XltKey!=2 && XltKey!=1) XltKey=4;
   MakeCall(0x433F3E, &get_input_str_hook, true);
   SafeWrite8(0x433ED6, 0x7D);
   MakeCall(0x47F364, &get_input_str2_hook, true);
   SafeWrite8(0x47F2F7, 0x7D);
   MakeCall(0x4CC358, &kb_next_ascii_English_US_hook, true);
  }
 }

 HookCall(0x497075, &pipboy_hook);
 if (GetPrivateProfileIntA("Misc", "UseScrollingQuestsList", 0, ini)) {
  dlog("Applying quests list patch ", DL_INIT);
  QuestListInit();
  dlogr(" Done", DL_INIT);
 }

 dlog("Applying premade characters patch", DL_INIT);
 PremadeInit();

 dlogr("Running SoundInit().", DL_INIT);
 SoundInit();

 dlogr("Running ReputationsInit().", DL_INIT);
 ReputationsInit();

 dlogr("Running ConsoleInit().", DL_INIT);
 ConsoleInit();

 if (GetPrivateProfileIntA("Misc", "ExtraSaveSlots", 0, ini)) {
  dlog("Running EnableSuperSaving()", DL_INIT);
  EnableSuperSaving();
  dlogr(" Done", DL_INIT);
 }

 SpeedInterfaceCounterAnims = GetPrivateProfileIntA("Misc", "SpeedInterfaceCounterAnims", 0, ini);
 if (SpeedInterfaceCounterAnims == 1 || SpeedInterfaceCounterAnims == 2) {
  SpeedInterfaceCounterAnims--;
  dlog("Applying SpeedInterfaceCounterAnims patch.", DL_INIT);
  MakeCall(0x460BA1, &intface_rotate_numbers_hook, true);
  dlogr(" Done", DL_INIT);
 }

 KarmaFrmCount=GetPrivateProfileIntA("Misc", "KarmaFRMsCount", 0, ini);
 if(KarmaFrmCount) {
  KarmaFrms=new DWORD[KarmaFrmCount];
  KarmaPoints=new int[KarmaFrmCount-1];
  dlog("Applying karma frm patch.", DL_INIT);
  char buf[512];
  GetPrivateProfileStringA("Misc", "KarmaFRMs", "", buf, 512, ini);
  char *ptr=buf, *ptr2;
  for(DWORD i=0;i<KarmaFrmCount-1;i++) {
   ptr2=strchr(ptr, ',');
   *ptr2='\0';
   KarmaFrms[i]=atoi(ptr);
   ptr=ptr2+1;
  }
  KarmaFrms[KarmaFrmCount-1]=atoi(ptr);
  GetPrivateProfileStringA("Misc", "KarmaPoints", "", buf, 512, ini);
  ptr=buf;
  for(DWORD i=0;i<KarmaFrmCount-2;i++) {
   ptr2=strchr(ptr, ',');
   *ptr2='\0';
   KarmaPoints[i]=atoi(ptr);
   ptr=ptr2+1;
  }
  KarmaPoints[KarmaFrmCount-2]=atoi(ptr);
  HookCall(0x4367A9, DrawCardHook);
  dlogr(" Done", DL_INIT);
 }

 switch (GetPrivateProfileIntA("Misc", "ScienceOnCritters", 0, ini)) {
  case 1: HookCall(0x41276E, &action_use_skill_on_hook); break;
  case 2: SafeWrite8(0x41276A, 0xEB); break;// jmps
 }

 tmp = GetPrivateProfileIntA("Misc", "SpeedInventoryPCRotation", 166, ini);
 if (tmp >= 0 && tmp <= 1000) {
  dlog("Applying SpeedInventoryPCRotation patch.", DL_INIT);
  SafeWrite32(0x47066B, tmp);
  dlogr(" Done", DL_INIT);
 }

 dlogr("Running BarBoxesInit().", DL_INIT);
 BarBoxesInit();

 dlogr("Patching out ereg call.", DL_INIT);
 BlockCall(0x4425E6);

 AnimationsLimit = GetPrivateProfileIntA("Misc", "AnimationsAtOnceLimit", 32, ini);
 if (AnimationsLimit > 32) {
  dlog("Applying AnimationsAtOnceLimit patch.", DL_INIT);
  AnimationsAtOnceInit();
  dlogr(" Done", DL_INIT);
 }

 if (GetPrivateProfileIntA("Misc", "RemoveCriticalTimelimits", 0, ini)) {
  dlog("Removing critical time limits.", DL_INIT);
  SafeWrite8(0x424118, 0xEB);
  SafeWrite8(0x4A3053, 0x0);
  SafeWrite8(0x4A3094, 0x0);
  dlogr(" Done", DL_INIT);
 }

 switch (GetPrivateProfileIntA("Sound", "OverrideMusicDir", 0, ini)) {
  case 2:                                   // отсутствие break - хитрый трюк :)
   SafeWrite32(0x518E78, (DWORD)musicOverridePath);
   SafeWrite32(0x518E7C, (DWORD)musicOverridePath);
  case 1:
   SafeWrite32(0x4449C2, (DWORD)musicOverridePath);
   SafeWrite32(0x4449DB, (DWORD)musicOverridePath);
   break;
 }

 if (GetPrivateProfileIntA("Misc", "NPCStage6Fix", 0, ini)) {
  dlog("Applying NPC Stage 6 Fix.", DL_INIT);
  MakeCall(0x493CE9, &partyMember_init_hook, true);
  SafeWrite8(0x494063, 6);                  // loop should look for a potential 6th stage
  SafeWrite8(0x4940BB, 204);                // move pointer by 204 bytes instead of 200
  MakeCall(0x494224, &partyMemberGetAIOptions_hook, true);
  dlogr(" Done", DL_INIT);
 }

 switch (GetPrivateProfileIntA("Misc", "FastShotFix", 1, ini)) {
  case 1:
   dlog("Applying Fast Shot Trait Fix.", DL_INIT);
   MakeCall(0x478E75, &item_w_called_shot_hook, true);
   dlogr(" Done", DL_INIT);
   break;
  case 2:
   dlog("Applying Fast Shot Trait Fix. (Fallout 1 version)", DL_INIT);
   SafeWrite8(0x478CA0, 0x0);
   for (int i = 0; i < sizeof(FastShotFixF1)/4; i++) {
    HookCall(FastShotFixF1[i], (void*)0x478C7D);
   }
   dlogr(" Done", DL_INIT);
   break;
 }

 if (GetPrivateProfileIntA("Misc", "BoostScriptDialogLimit", 0, ini)) {
  const int scriptDialogCount = 10000;
  dlog("Applying script dialog limit patch.", DL_INIT);
  scriptDialog = new int[scriptDialogCount*2];// Because the msg structure is 8 bytes, not 4.
  SafeWrite32(0x4A50E3, scriptDialogCount); // scr_init_
  SafeWrite32(0x4A519F, scriptDialogCount); // scr_game_init_
  SafeWrite32(0x4A534F, scriptDialogCount*8);// scr_message_free_
  for (int i = 0; i < sizeof(script_dialog_msgs)/4; i++) {
   SafeWrite32(script_dialog_msgs[i], (DWORD)scriptDialog);
  }
  dlogr(" Done", DL_INIT);
 }

 dlog("Running InventoryInit.", DL_INIT);
 InventoryInit();
 dlogr(" Done", DL_INIT);

 MotionSensorFlags = GetPrivateProfileIntA("Misc", "MotionScannerFlags", 1, ini);
 if (MotionSensorFlags != 0) {
  dlog("Applying MotionScannerFlags patch.", DL_INIT);
  if (MotionSensorFlags&1) HookCall(0x41BBEE, &automap_hook);
  if (MotionSensorFlags&2) {
   SafeWrite8(0x41BC25, 0x0);
   BlockCall(0x41BC3C);
  }
  dlogr(" Done", DL_INIT);
 }

 tmp = GetPrivateProfileIntA("Misc", "EncounterTableSize", 40, ini);
 if (tmp > 40 && tmp <= 127) {
  dlog("Applying EncounterTableSize patch.", DL_INIT);
  SafeWrite8(0x4BDB17, (BYTE)tmp);
  tmp = (tmp + 1) * 180 + 0x50;
  for (int i = 0; i < sizeof(EncounterTableSize)/4; i++) {
   SafeWrite32(EncounterTableSize[i], tmp);
  }
  dlogr(" Done", DL_INIT);
 }

 dlog("Initing main menu patches.", DL_INIT);
 MainMenuInit();
 dlogr(" Done", DL_INIT);

 if(GetPrivateProfileIntA("Misc", "DisablePipboyAlarm", 0, ini)) {
  SafeWrite8(0x499518, 0xC3);               // retn
 }

 dlog("Initing AI patches.", DL_INIT);
 AIInit();
 dlogr(" Done", DL_INIT);

 dlog("Initing AI control.", DL_INIT);
 PartyControlInit();
 dlogr(" Done", DL_INIT);

 if (GetPrivateProfileIntA("Misc", "ObjCanSeeObj_ShootThru_Fix", 0, ini)) {
  dlog("Applying ObjCanSeeObj ShootThru Fix.", DL_INIT);
  HookCall(0x456BC6, &op_obj_can_see_obj_hook);
  dlogr(" Done", DL_INIT);
 }

 // phobos2077:
 ComputeSprayModInit();
 ExplosionLightingInit();
 tmp = SimplePatch<DWORD>(0x4A2873, "Misc", "Dynamite_DmgMax", 50, 0, 9999);
 SimplePatch<DWORD>(0x4A2878, "Misc", "Dynamite_DmgMin", 30, 0, tmp);
 tmp = SimplePatch<DWORD>(0x4A287F, "Misc", "PlasticExplosive_DmgMax", 80, 0, 9999);
 SimplePatch<DWORD>(0x4A2884, "Misc", "PlasticExplosive_DmgMin", 40, 0, tmp);
 BooksInit();
 DWORD addrs[2] = {0x45F9DE, 0x45FB33};
 SimplePatch<WORD>(addrs, 2, "Misc", "CombatPanelAnimDelay", 1000, 0, 65535);
 addrs[0] = 0x447DF4; addrs[1] = 0x447EB6;
 SimplePatch<BYTE>(addrs, 2, "Misc", "DialogPanelAnimDelay", 33, 0, 255);

// Минимальный возраст игрока
 for (int i = 0; i < sizeof(AgeMin)/4; i++) {
  SafeWrite8(AgeMin[i], 8);
 }

// Максимальный возраст игрока
 for (int i = 0; i < sizeof(AgeMax)/4; i++) {
  SafeWrite8(AgeMax[i], 60);
 }

 windowName[64] = 0;
 if (GetPrivateProfileString("Misc", "WindowName", "", windowName, 64, ini)) {
  dlog("Applying window name patch.", DL_INIT);
  SafeWrite32(0x480CCB, (DWORD)&windowName);
  dlogr(" Done", DL_INIT);
 }

// Уменьшение дистанции переключения на ходьбу при клике на предмет
 for (int i = 0; i < sizeof(WalkDistance)/4; i++) {
  SafeWrite8(WalkDistance[i], 1);
 }

// fix "Pressing A to enter combat before anything else happens, thus getting infinite free running"
 if (GetPrivateProfileIntA("Misc", "FakeCombatFix", 0, ini)) {
  MakeCall(0x41803A, &check_move_hook, false);
  MakeCall(0x44B8A9, &gmouse_bk_process_hook, false);
  HookCall(0x44C130, &FakeCombatFix1);      // action_get_an_object_
  HookCall(0x44C7B0, &FakeCombatFix1);      // action_get_an_object_
  HookCall(0x44C1D9, &FakeCombatFix2);      // action_loot_container_
  HookCall(0x44C79C, &FakeCombatFix2);      // action_loot_container_
  HookCall(0x412117, &FakeCombatFix3);      // action_use_an_object_
  HookCall(0x44C33B, &FakeCombatFix3);      // gmouse_handle_event_
  HookCall(0x471AC8, &FakeCombatFix3);      // use_inventory_on_
 }

 if (GetPrivateProfileIntA("Misc", "DisableHotkeysForUnknownAreasInCity", 0, ini)) {
  MakeCall(0x4C4945, &wmTownMapFunc_hook, false);
 }

 if (GetPrivateProfileIntA("Misc", "EnableMusicInDialogue", 0, ini)) {
  SafeWrite8(0x44525B, 0x00);
//  BlockCall(0x450627);
 }

 AutoQuickSave = GetPrivateProfileIntA("Misc", "AutoQuickSave", 0, ini);
 if (AutoQuickSave >= 1 && AutoQuickSave <= 10) {
  AutoQuickSave--;
  SafeWrite16(0x47B91C, 0xC031);            // xor  eax, eax
  SafeWrite32(0x47B91E, 0x5193B8A3);        // mov  ds:_slot_cursor, eax
  SafeWrite16(0x47B923, 0x04EB);            // jmp  0x47B929
  MakeCall(0x47B984, &SaveGame_hook, true);
 }

 if (GetPrivateProfileIntA("Misc", "DontTurnOffSneakIfYouRun", 0, ini)) {
  SafeWrite8(0x418135, 0xEB);
 }

// Показывать в окне настроек сопартийца в поле "Здоровье" в скобках уровень
 GetPrivateProfileString("sfall", "LvlMsg", "Lvl", LvlMsg, 6, translationIni);
 MakeCall(0x44906E, &partyMemberCurLevel, true);

// Показывать в окне настроек сопартийца в поле "ОД" в скобках класс брони
 GetPrivateProfileString("sfall", "ACMsg", "AC", AcMsg, 6, translationIni);
 MakeCall(0x449222, &partyMemberAC, true);

// Можно использовать управляющий символ новой строки (\n) в описании объектов из pro_*.msg
 SafeWrite32(0x46ED87, (DWORD)&display_print_with_linebreak);
 SafeWrite32(0x49AD7A, (DWORD)&display_print_with_linebreak);
 SafeWrite32(0x472F9A, (DWORD)&inven_display_msg_with_linebreak);

// При описании энкаунтера выводить одну строку вместо двух в информационном окне
 SafeWrite32(0x4C1011, 0xF702EB97);         // xchg edi, eax; jmps 0x4C1016
 HookCall(0x4C1042, &wmSetupRandomEncounter_hook);

// Force the party members to play the idle animation when reloading their weapon
// SafeWrite16(0x421F2E, 0xEA89);             // mov  edx, ebp

 if (GetPrivateProfileIntA("Misc", "DrugExploitFix", 0, ini)) {
  HookCall(0x49BF5B, &protinst_use_item_hook);
  HookCall(0x43C342, &UpdateLevel_hook);
  HookCall(0x43A8A1, &skill_level_hook);    // SavePlayer_
  HookCall(0x43B3FC, &SliderBtn_hook);
  HookCall(0x43B463, &skill_level_hook);    // SliderBtn_
  HookCall(0x43B47C, &SliderBtn_hook1);
  HookCall(0x4AEFC3, &stat_level_hook1);
 }

 dlog("Running BugsInit.", DL_INIT);
 BugsInit();
 dlogr(" Done", DL_INIT);

// Исправление невозможности продажи ранее использованных "Счетчик Гейгера"/"Невидимка"
 if (GetPrivateProfileIntA("Misc", "CanSellUsedGeiger", 0, ini)) {
  SafeWrite8(0x478115, 0xBA);
  SafeWrite8(0x478138, 0xBA);
  MakeCall(0x474D22, &barter_attempt_transaction_hook, true);
  HookCall(0x4798B1, &item_m_turn_off_hook);
 }

 if (GetPrivateProfileIntA("Misc", "InstanWeaponEquip", 0, ini)) {
// Пропускать анимацию убирания оружия
  for (int i = 0; i < sizeof(PutAwayWeapon)/4; i++) {
   SafeWrite8(PutAwayWeapon[i], 0xEB);      // jmps
  }
  BlockCall(0x472AD5);                      //
  BlockCall(0x472AE0);                      // invenUnwieldFunc_
  BlockCall(0x472AF0);                      //

  MakeCall(0x415238, &register_object_take_out_hook, true);
 }

 tmp = GetPrivateProfileIntA("Misc", "PipboyTimeAnimDelay", -1, ini);
 if (tmp >= 0 && tmp <= 127) {
  dlog("Applying PipboyTimeAnimDelay patch.", DL_INIT);
  SafeWrite8(0x499B99, (BYTE)tmp);
  SafeWrite8(0x499DA8, (BYTE)tmp);
  dlogr(" Done", DL_INIT);
 }

 tmp = GetPrivateProfileIntA("Misc", "CarryWeightLimit", -1, ini);
 if (tmp > 0) {
  SafeWrite32(0x51D66C, tmp);
 }

 dlogr("Leave DllMain2", DL_MAIN);
}

static void _stdcall OnExit() {
 ConsoleExit();
 AnimationsAtOnceExit();
 HeroAppearanceModExit();
 //SoundExit();
}

static void __declspec(naked) OnExitFunc() {
 __asm {
  pushad
  call OnExit
  popad
  jmp  DOSCmdLineDestroy_
 }
}

static void CompatModeCheck(HKEY root, const char* filepath, int extra) {
 HKEY key;
 char buf[MAX_PATH];
 DWORD size=MAX_PATH;
 DWORD type;
 if(!(type=RegOpenKeyEx(root, "Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers", 0, extra|STANDARD_RIGHTS_READ|KEY_QUERY_VALUE, &key))) {
  if(!RegQueryValueEx(key, filepath, 0, &type, (BYTE*)buf, &size)) {
   if(size&&(type==REG_EXPAND_SZ||type==REG_MULTI_SZ||type==REG_SZ)) {
    if(strstr(buf, "256COLOR")||strstr(buf, "640X480")||strstr(buf, "WIN")) {
     RegCloseKey(key);
     /*if(!RegOpenKeyEx(root, "Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers", 0, extra|KEY_READ|KEY_WRITE, &key)) {
      if((type=RegDeleteValueA(key, filepath))==ERROR_SUCCESS) {
       MessageBoxA(0, "Fallout was set to run in compatibility mode.\n"
        "Please restart fallout to ensure it runs correctly.", "Error", 0);
       RegCloseKey(key);
       ExitProcess(-1);
      } else {
       //FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, type, 0, buf, 260, 0);
       //MessageBoxA(0, buf, "", 0);
      }
     }*/

     MessageBoxA(0, "Fallout appears to be running in compatibility mode.\n" //, and sfall was not able to disable it.\n"
      "Please check the compatibility tab of fallout2.exe, and ensure that the following settings are unchecked.\n"
      "Run this program in compatibility mode for..., run in 256 colours, and run in 640x480 resolution.\n"
      "If these options are disabled, click the 'change settings for all users' button and see if that enables them.", "Error", 0);
     //RegCloseKey(key);
     ExitProcess(-1);
    }
   }
  }
  RegCloseKey(key);
 } else {
  //FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, type, 0, buf, 260, 0);
  //MessageBoxA(0, buf, "", 0);
 }
}

bool _stdcall DllMain(HANDLE hDllHandle, DWORD dwReason, LPVOID  lpreserved) {
 if(dwReason==DLL_PROCESS_ATTACH) {
#ifdef TRACE
  LoggingInit();
#endif

  HookCall(0x4DE7D2, &OnExitFunc);

  char filepath[MAX_PATH];
  GetModuleFileName(0, filepath, MAX_PATH);

  CRC(filepath);

#ifdef TRACE
  if(!GetPrivateProfileIntA("Debugging", "SkipCompatModeCheck", 0, ".\\ddraw.ini")) {
#else
  if(1) {
#endif
   int is64bit;
   typedef int (_stdcall *chk64bitproc)(HANDLE, int*);
   HMODULE h=LoadLibrary("Kernel32.dll");
   chk64bitproc proc = (chk64bitproc)GetProcAddress(h, "IsWow64Process");
   if(proc) proc(GetCurrentProcess(), &is64bit);
   else is64bit=0;
   FreeLibrary(h);

   CompatModeCheck(HKEY_CURRENT_USER, filepath, is64bit?KEY_WOW64_64KEY:0);
   CompatModeCheck(HKEY_LOCAL_MACHINE, filepath, is64bit?KEY_WOW64_64KEY:0);
  }


  bool cmdlineexists=false;
  char* cmdline=GetCommandLineA();
  if(GetPrivateProfileIntA("Main", "UseCommandLine", 0, ".\\ddraw.ini")) {
   while(cmdline[0]==' ') cmdline++;
   bool InQuote=false;
   int count=-1;

   while(true) {
    count++;
    if(cmdline[count]==0) break;;
    if(cmdline[count]==' '&&!InQuote) break;
    if(cmdline[count]=='"') {
     InQuote=!InQuote;
     if(!InQuote) break;
    }
   }
   if(cmdline[count]!=0) {
    count++;
    while(cmdline[count]==' ') count++;
    cmdline=&cmdline[count];
    cmdlineexists=true;
   }
  }

  if(cmdlineexists&&strlen(cmdline)) {
   strcpy_s(ini, ".\\");
   strcat_s(ini, cmdline);
   HANDLE h = CreateFileA(cmdline, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
   if(h!=INVALID_HANDLE_VALUE) CloseHandle(h);
   else {
    MessageBox(0, "You gave a command line argument to fallout, but it couldn't be matched to a file\n" \
     "Using default ddraw.ini instead", "Warning", MB_TASKMODAL);
    strcpy_s(ini, ".\\ddraw.ini");
   }
  } else strcpy_s(ini, ".\\ddraw.ini");

  GetPrivateProfileStringA("Main", "TranslationsINI", "./Translations.ini", translationIni, 65, ini);

  DllMain2();
 }
 return true;
}
