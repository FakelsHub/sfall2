/*
 *    sfall
 *    Copyright (C) 2012  The sfall team
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

#include <hash_map>
#include "AI.h"
#include "Define.h"
#include "FalloutEngine.h"
#include "SafeWrite.h"

typedef stdext::hash_map<DWORD, DWORD> :: const_iterator iter;

static stdext::hash_map<DWORD,DWORD> targets;
static stdext::hash_map<DWORD,DWORD> sources;

static int AI_Called_Freq_Div;

DWORD _stdcall AIGetLastAttacker(DWORD target) {
 iter itr = sources.find(target);
 if (itr == sources.end()) return 0;
 else return itr->second;
}

DWORD _stdcall AIGetLastTarget(DWORD source) {
 iter itr = targets.find(source);
 if (itr == targets.end()) return 0;
 else return itr->second;
}

static void _stdcall CombatAttackHook(DWORD source, DWORD target) {
 sources[target] = source;
 targets[source] = target;
}

static void __declspec(naked) combat_attack_hook() {
 _asm {
  pushad
  push edx
  push eax
  call CombatAttackHook
  popad
  jmp  combat_attack_
 }
}

DWORD CombatDisabled = 0;
static char CombatBlockedMessage[128];

static void __declspec(naked) intface_use_item_hook() {
 __asm {
  xor  eax, eax
  cmp  CombatDisabled, eax
  je   end
  mov  eax, offset CombatBlockedMessage
  call display_print_
  pop  eax                                  // Уничтожаем адрес возврата
  push 0x45F6D7
end:
  mov  eax, 20
  retn
 }
}

static void __declspec(naked) game_handle_input_hook() {
 __asm {
  mov  eax, ds:[_intfaceEnabled]
  test eax, eax
  jz   end
  cmp  CombatDisabled, eax
  jne  end
  mov  eax, offset CombatBlockedMessage
  call display_print_
  xor  eax, eax
end:
  retn
 }
}

static void __declspec(naked) ai_called_shot_hook() {
 __asm {
  xchg edx, eax                             // eax = cap.called_freq
  cdq
  div  dword ptr AI_Called_Freq_Div
  xchg edx, eax
  xor  eax, eax
  inc  eax
  cmp  eax, edx
  jbe  end
  mov  edx, eax
end:
  retn
 }
}

static void __declspec(naked) action_push_critter() {
// eax, esi = source, edx = target
 __asm {
  push ebp
  push edi
  push esi
  push ecx
  push ebx
  cmp  eax, edx
  je   fail
  mov  edi, edx                             // edi = target
  xchg edx, eax
  call critter_is_active_
  dec  eax
  jnz  fail
  mov  eax, edi
  xchg edx, eax
  call action_can_talk_to_
  test eax, eax
  jnz  fail
  test byte ptr ds:[_combat_state], 1       // В бою?
  jz   skip                                 // Нет
  mov  eax, [esi+0x50]                      // eax = source.team_num
  mov  ecx, [edi+0x54]                      // edx = target.who_hit_me
  cmp  eax, [edi+0x50]                      // source.team_num == target.team_num?
  jne  notTeam
  cmp  esi, ecx                             // source стрелял в братюню?
  je   fail                                 // Да, обидка
notTeam:
  jecxz skip                                // target никто не обижал
  cmp  eax, [ecx+0x50]                      // source.team_num == target.who_hit_me.team_num?
  je   fail                                 // кто-то из группы поддержки source обидел target
skip:
  mov  eax, [edi+0x78]
  inc  eax                                  // У target есть скрипт?
  jz   noScript                             // Нет
  dec  eax
  push eax
  sub  esp, 4
  mov  edx, esp
  call scr_ptr_
  mov  ecx, [esp]                           // ecx = Script
  add  esp, 4
  inc  eax
  pop  eax
  jz   noScript
  mov  [ecx+0x38], esi                      // Script.source_obj
  mov  [ecx+0x3C], edi                      // Script.target_obj
  mov  edx, push_p_proc
  call exec_script_proc_
  inc  eax
  jz   noScript
  cmp  [ecx+0x44], eax                      // Script.override
  je   fail
noScript:  
  mov  edx, [edi+0x4]                       // target_tile
  mov  eax, [esi+0x4]                       // source_tile
  call tile_dir_
  xchg ebp, eax
  mov  ecx, 5
loopRot:
  lea  eax, [ebp+1]
  mov  ebx, 6
  cdq
//  mov  edx, eax
//  sar  edx, 0x1F
  div  ebx                                  // rotation
  xor  ebx, ebx
  inc  ebx                                  // distance
  mov  eax, [edi+0x4]                       // tile
  call tile_num_in_direction_
  mov  esi, eax
  xchg edx, eax                             // tile
  mov  ebx, [edi+0x28]                      // elev
  mov  eax, edi                             // source
  call obj_blocking_at_
  test eax, eax
  jz   canMove
  inc  ebp
  loop loopRot
fail:
  xor  eax, eax
  dec  eax
  jmp  end
canMove:
  xor  ecx, ecx
  dec  ecx
  test byte ptr ds:[_combat_state], 1       // В бою?
  jz   moveTarget                           // Нет
  mov  ecx, [edi+0x40]                      // ecx = target.curr_mp
moveTarget:
  xor  eax, eax
  inc  eax
  inc  eax                                  // RB_RESERVED
  call register_begin_
  mov  edx, esi                             // tile_num
  mov  eax, edi                             // source
  call register_object_turn_towards_
  push eax
  mov  edx, esi                             // tile_num
  mov  eax, edi                             // source
  mov  ebx, [edi+0x28]                      // elev
  call register_object_move_to_tile_
  call register_end_
end:
  pop  ebx
  pop  ecx
  pop  esi
  pop  edi
  pop  ebp
  retn
 }
}

static const char* PushFmt = "\n%s (->%s) move %s from %d to %d";
static void __declspec(naked) ai_move_steps_closer_hook() {
// esi = source, edx = target
 __asm {
  jz   fail                                 // Нет объекта
  mov  eax, [edx+0x64]
  shr  eax, 0x18
  dec  eax                                  // ObjType_Critter?
  jnz  fail                                 // Нет
  push edx
  mov  eax, esi                             // eax = source, edx = target
  call obj_dist_
  pop  edx
  dec  eax                                  // Расстояние == 1?
  jnz  end                                  // Нет
  call register_end_
  mov  ebx, [edx+0x4]                       // target.tile_num
  push edx
  mov  eax, esi                             // eax = source, edx = target
  call action_push_critter
  pop  edx
  inc  eax                                  // Сдвинули?
  jz   skip                                 // Нет
  call combat_turn_run_
  push dword ptr [edx+0x4]                  // target.tile_num
  push ebx
  xchg edx, eax
  call item_name_
  push eax
  mov  eax, edi
  call item_name_
  push eax
  mov  eax, esi
  call item_name_
  push eax
  push PushFmt
  call debug_printf_
  add  esp, 6*4
  pop  eax                                  // Уничтожаем адрес возврата
  push 0x42A089
skip:
  push eax
  xor  eax, eax
  inc  eax
  inc  eax
  call register_begin_
  pop  ecx
  jecxz end
fail:
  xor  eax, eax
  retn
end:
  mov  eax, 0x1000000
  retn
 }
}

static void __declspec(naked) isGoodTarget() {
// eax = target, ebp = *target4+4
 __asm {
  test byte ptr [eax+0x44], 0x80            // target.results & DAM_DEAD?
  jnz  skip                                 // Да, target трупик
  cmp  [ebp-16], eax                        // *target1 == target?
  je   skip                                 // Да
  cmp  [ebp-12], eax                        // *target2 == target?
  je   skip                                 // Да
  cmp  [ebp-8], eax                         // *target3 == target?
  je   skip                                 // Да
  cmp  [ebp-4], eax                         // *target4 == target?
  jne  end                                  // Нет
skip:
  xor  eax, eax
end:
  retn
 }
}

static void __declspec(naked) ai_find_attackers() {
// eax,esi = source, edx = *target2
 __asm {
  push ebp
  push edi
  push esi
  mov  ebx, 4
  sub  edx, ebx
  mov  edi, edx                             // edi = *target1
  lea  ebp, [ecx+ebx]                       // ebp = *target4+4
  mov  edx, ds:[_curr_crit_num]
  test edx, edx                             // Есть персонажи в списке?
  jz   end                                  // Нет
  mov  ds:[_combat_obj], eax
  mov  eax, ds:[_curr_crit_list]
  mov  ecx, compare_nearer_
  call qsort_
  mov  ebx, [esi+0x50]                      // ebx = source.team_num
  xor  edx, edx
  mov  eax, edi
  cmp  [edi], edx                           // target1 занят?
  je   clearTargets                         // Нет
  add  edi, 4
  mov  eax, edi
clearTargets:
  mov  [edi], edx
  add  edi, 4
  cmp  edi, ebp
  jne  clearTargets
  xchg edi, eax
// eax = target, ebx = source.team_num, ecx = target.who_hit_me, edx = crit_count, esi = source, edi = *target#, ebp = *target4+4
loopCritters:
  mov  eax, ds:[_curr_crit_list]
  mov  eax, [eax+edx*4]                     // eax = target
  cmp  ebx, [eax+0x50]                      // source.team_num = target.team_num?
  je   nextCritter                          // Да, это собутыльник
  mov  ecx, [eax+0x54]                      // ecx = target.who_hit_me
  jecxz nextCritter                         // У target нет обидчика
  cmp  ebx, [ecx+0x50]                      // source.team_num = target.who_hit_me.team_num?
  jne  nextCritter                          // Кто-то левый обидел target
  call isGoodTarget
  test eax, eax
  jz   nextCritter
  mov  [edi], eax
  add  edi, 4
  cmp  edi, ebp                             // Использованы все слоты для целей?
  je   end                                  // Да
nextCritter:
  inc  edx
  cmp  edx, ds:[_curr_crit_num]             // Все персонажи из списка?
  jl   loopCritters                         // Нет
  xor  edx, edx
loopTeam:
  mov  eax, ds:[_curr_crit_list]
  mov  eax, [eax+edx*4]                     // eax = target
  call isGoodTarget
  test eax, eax                             // Цель можно использовать?
  jz   nextTeam                             // Нет
  lea  esi, [ebp-16]                        // esi = *target1
  mov  ebx, [eax+0x50]                      // ebx = target.team_num
loopTargets:
  mov  ecx, [esi]                           // ecx = target#
  jecxz nextTarget                          // Слот не занят
  cmp  ebx, [ecx+0x50]                      // target.team_num == target#.team_num?
  je   foundTarget                          // Да, нашли новую цель
nextTarget:
  add  esi, 4
  cmp  esi, ebp                             // Проверены все слоты целей?
  jne  loopTargets                          // Нет
  jmp  nextTeam
foundTarget:
  mov  [edi], eax
  add  edi, 4
  cmp  edi, ebp                             // Использованы все слоты для целей?
  je   end                                  // Да
nextTeam:
  inc  edx
  cmp  edx, ds:[_curr_crit_num]             // Все персонажи из списка?
  jl   loopTeam                             // Нет
end:
  xor  eax, eax
  pop  esi
  pop  edi
  pop  ebp
  retn
 }
}

static void __declspec(naked) is_within_perception_hook() {
 __asm {
  xchg edx, eax                             // edx = source, eax = target
  test eax, eax                             // Есть цель?
  jz   end                                  // Нет
  mov  ecx, edx                             // ecx = source
  xchg ebx, eax                             // ebx = target
  xor  eax, eax
  inc  eax                                  // STAT_pe
  xchg edx, eax                             // eax = source, edx = STAT_pe
  call stat_level_
  mov  ebp, eax                             // ebp = восприятие source
  test byte ptr ds:[_combat_state], 1       // В бою?
  jz   notCombat                            // Нет
  shl  eax, 1                               // eax = восприятие * 2
notCombat:
  xchg esi, eax                             // esi = видимость (на данном этапе это слышимость)
  test byte ptr [ecx+0x44], 0x40            // source.results & DAM_BLIND?
  jnz  cantSee                              // Да, слепой
  mov  edx, ebx
  mov  eax, ecx
  call can_see_
  dec  eax                                  // Перед лицом?
  jnz  cantSee                              // Нет
  push ecx
  push ebx
  push obj_sight_blocking_at_
  push eax
  xchg ecx, eax                             // eax = source, ecx = *sad_rotation_ptr (0)
  mov  ebx, [ebx+0x4]                       // ebx = target.tile_num
  mov  edx, [eax+0x4]                       // edx = source.tile_num
  call make_path_func_                      // Проверка прямой видимости
  pop  ebx
  pop  ecx
  test eax, eax                             // Есть преграды?
  jz   cantSee                              // Да
  lea  esi, [ebp+ebp*4]                     // esi = восприятие * 5
  test byte ptr [ebx+0x26], 0x2             // target.flags3 & TransGlass_?
  jz   cantSee                              // Нет
  shr  esi, 1                               // esi = видимость / 2
cantSee:
  cmp  ebx, ds:[_obj_dude]                  // Цель == ГГ?
  jne  checkDistance                        // Нет
  call is_pc_sneak_working_
  test eax, eax                             // Работает скрытность?
  jz   noSneakWorking                       // Нет
  shr  esi, 2                               // esi = видимость / 4
  mov  edx, SKILL_SNEAK
  mov  eax, ebx
  call skill_level_
  cmp  eax, 120
  jle  checkDistance
  dec  esi
  jmp  checkDistance
noSneakWorking:
  call is_pc_flag_
  test eax, eax                             // Включён режим скрытности?
  jz   checkDistance                        // Нет
  inc  eax
  inc  eax                                  // eax = 3
  xchg esi, eax
  shl  eax, 1                               // eax = видимость * 2
  cdq
  div  esi                                  // eax = видимость * 2 / 3
  xchg esi, eax
checkDistance:
  mov  edx, ecx
  xchg ebx, eax
  call obj_dist_
  xor  edi, edi
  xchg edi, eax                             // edi = расстояние
  cmp  edi, esi                             // Расстояние больше видимости?
  jg   end                                  // Да
  inc  eax
end:
  pop  ebp
  pop  edi
  pop  esi
  pop  ecx
  pop  ebx
  retn
 }
}

static void __declspec(naked) op_obj_can_hear_obj_hook() {
 __asm {
  mov  esi, eax                             // esi = source
  mov  ebx, [esi+0x44]
  or   byte ptr [esi+0x44], 0x40            // source.results & DAM_BLIND
  call is_within_perception_
  mov  [esi+0x44], ebx
  retn
 }
}

void AIInit() {
 HookCall(0x426A95, combat_attack_hook);
 HookCall(0x42A796, combat_attack_hook);
 MakeCall(0x45F6AF, &intface_use_item_hook, false);
 HookCall(0x4432A6, &game_handle_input_hook);
 GetPrivateProfileString("sfall", "BlockedCombat", "You cannot enter combat at this time.", CombatBlockedMessage, 128, translationIni);

 AI_Called_Freq_Div = GetPrivateProfileIntA("Misc", "AI_Called_Freq_Div", 0, ini);
 if (AI_Called_Freq_Div > 1) MakeCall(0x42A6BA, &ai_called_shot_hook, false);

 if (GetPrivateProfileIntA("Misc", "SmarterAI", 0, ini)) {
//  SafeWrite8(0x428F6E, 0xEB);
//  MakeCall(0x42A0C9, &ai_move_steps_closer_hook, false);
  HookCall(0x4290D3, &ai_find_attackers);
 }

// Дополнительные проверки на слепоту и прямую видимость
 if (GetPrivateProfileIntA("Misc", "CanSeeAndHearFix", 1, ini)) {
  MakeCall(0x42BA09, &is_within_perception_hook, true);
  HookCall(0x458403, &op_obj_can_hear_obj_hook);
 }

}

void _stdcall AICombatStart() {
 targets.clear();
 sources.clear();
}

void _stdcall AICombatEnd() {
 targets.clear();
 sources.clear();
}
