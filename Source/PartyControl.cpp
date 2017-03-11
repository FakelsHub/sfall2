/*
 *    sfall
 *    Copyright (C) 2013  The sfall team
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

#include <vector>
#include "BarBoxes.h"
#include "Define.h"
#include "FalloutEngine.h"
#include "Inventory.h"
#include "PartyControl.h"

DWORD IsControllingNPC = 0;
DWORD HiddenArmor = 0;
DWORD DelayedExperience = 0;

static DWORD Mode;
static std::vector<WORD> Chars;

static char real_pc_name[32];
static DWORD real_last_level;
static DWORD real_Level;
static DWORD real_Experience;
static DWORD real_free_perk;
static DWORD real_unspent_skill_points;
static DWORD real_map_elevation;
static DWORD real_sneak_working;
static DWORD real_sneak_queue_time;
static DWORD real_dude;
static DWORD real_hand;
static DWORD real_trait;
static DWORD real_trait2;
static DWORD real_itemButtonItems[6*2];
static DWORD real_perkLevelDataList[PERK_count];
static DWORD real_drug_gvar[6];
static DWORD real_jet_gvar;
static DWORD real_tag_skill[4];
static DWORD real_bbox_sneak;
static DWORD party_PERK_bonus_awareness;
static DWORD party_PERK_gecko_skinning;

static BYTE *NameBox;
extern sBBox bboxes[10];

static void __declspec(naked) isRealParty() {
 __asm {
  push edx
  push ecx
  push ebx
  mov  ecx, ds:[_partyMemberMaxCount]
  dec  ecx
  jecxz skip                                // Только игрок
  mov  edx, ds:[_partyMemberPidList]
  mov  ebx, [eax+0x64]                      // pid
loopPid:
  add  edx, 4
  cmp  ebx, [edx]
  je   end
  loop loopPid
skip:
  xor  eax, eax
end:
  pop  ebx
  pop  ecx
  pop  edx
  retn
 }
}

void __declspec(naked) PartyControl_CanUseWeapon() {
 __asm {
  push edi
  push esi
  push edx
  push ecx
  push ebx
  mov  edi, eax
  call item_get_type_
  cmp  eax, item_type_weapon
  jne  canUse                               // Нет
  dec  eax
  dec  eax
  cmp  IsControllingNPC, eax                // Контроль персонажей?
  jne  end                                  // Нет
  inc  eax                                  // hit_right_weapon_primary
  xchg ecx, eax
  mov  eax, edi                             // eax=item
  call item_w_anim_code_
  xchg ecx, eax                             // ecx=ID1=Weapon code
  xchg edx, eax
  xchg edi, eax                             // eax=item
  call item_w_anim_weap_
  xchg ebx, eax                             // ebx=ID2=Animation code
  mov  esi, ds:[_inven_dude]
  mov  edx, [esi+0x20]                      // fid
  and  edx, 0xFFF                           // edx=Index
  mov  eax, [esi+0x1C]                      // cur_rot
  inc  eax
  push eax                                  // ID3=Direction code
  mov  eax, ObjType_Critter
  call art_id_
  call art_exists_
  test eax, eax
  jz   end
canUse:
  xor  eax, eax
  inc  eax
end:
  pop  ebx
  pop  ecx
  pop  edx
  pop  esi
  pop  edi
  retn
 }
}

static bool _stdcall IsInPidList(DWORD* npc) {
 if (Chars.size() == 0) return true;
 int pid = npc[0x64/4] & 0xFFFFFF;
 for (std::vector<WORD>::iterator it = Chars.begin(); it != Chars.end(); it++) {
  if (*it == pid) return true;
 }
 return false;
}

// save "real" dude state
void __declspec(naked) SaveDudeState() {
 __asm {
  pushad
  mov  esi, _pc_name
  mov  edi, offset real_pc_name
  mov  ecx, 32/4
  rep  movsd
  mov  eax, ebx
  push ebx
  call critter_name_
  push eax
  call critter_pc_set_name_
  mov  eax, [bboxes+12+8]
  mov  real_bbox_sneak, eax
  mov  eax, NameBox
  mov  [bboxes+12+8], eax
  mov  ebx, 2730
  xor  edx, edx
  call memset_                              // eax = destination, edx = value, ebx = num
  xchg edi, eax                             // edi = Buffer
  pop  edx                                  // edx = DisplayText
  push dword ptr ds:[_curr_font_num]
  mov  eax, 103
  call text_font_
  mov  esi, 21
  call ds:[_text_height]
  sub  esi, eax
  shr  esi, 1
  inc  esi                                  // esi = y
  mov  ecx, 130                             // ecx = ToWidth
  imul esi, ecx                             // esi = ToWidth * y
  lea  esi, [esi+67]                        // esi = ToWidth * y + 67
  lea  ebx, [ecx-4]
  mov  eax, edx
  call ds:[_text_width]
  cmp  eax, ebx
  jbe  goodWidth
  xchg ebx, eax
goodWidth:
  mov  ebx, eax                             // ebx = TxtWidth
  shr  eax, 1                               // TxtWidth/2
  sub  esi, eax
  movzx eax, byte ptr ds:[_BlueColor]
  push eax                                  // ColorIndex
  lea  eax, [edi+esi]                       // eax = Buffer
  call ds:[_text_to_buf]
  pop  eax
  call text_font_
  movzx eax, byte ptr ds:[_GreenColor]
  push eax                                  // Color
  push 19
  mov  edx, 129
  push edx
  inc  edx
  xor  ecx, ecx
  mov  ebx, 3
  xchg edi, eax                             // toSurface
  call draw_box_
  pop  ebx
  mov  eax, ds:[_last_level]
  mov  real_last_level, eax
  mov  eax, ds:[_Level_]
  mov  real_Level, eax
  mov  eax, ebx
  call isRealParty
  test eax, eax
  jz   saveLevel
  call partyMemberGetCurLevel_
saveLevel:
  mov  ds:[_Level_], eax
  mov  ds:[_last_level], eax
  mov  eax, ds:[_Experience_]
  mov  real_Experience, eax
  movzx eax, byte ptr ds:[_free_perk]
  mov  real_free_perk, eax
  mov  eax, ds:[_curr_pc_stat]
  mov  real_unspent_skill_points, eax
  mov  eax, ds:[_map_elevation]
  mov  real_map_elevation, eax
  mov  eax, ds:[_sneak_working]
  mov  real_sneak_working, eax
  mov  eax, ds:[_obj_dude]
  push eax
  mov  edx, 10
  call queue_find_first_
  test edx, edx
  jz   noSneak
  mov  edx, [edx]
  mov  eax, ds:[_fallout_game_time]
  sub  edx, eax
  pop  eax
  push eax
  push edx
  mov  edx, 10
  call queue_remove_this_
  pop  edx
noSneak:
  mov  real_sneak_queue_time, edx
  pop  eax
  mov  real_dude, eax
  mov  eax, ds:[_itemCurrentItem]
  mov  real_hand, eax
  mov  edx, offset real_trait2
  mov  eax, offset real_trait
  call trait_get_
  mov  esi, _itemButtonItems
  mov  edi, offset real_itemButtonItems
  mov  ecx, 6*2
  rep  movsd
  mov  esi, ds:[_perkLevelDataList]
  push esi
  mov  edi, offset real_perkLevelDataList
  mov  ecx, PERK_count
  push ecx
  rep  movsd
  pop  ecx
  pop  edi
  push dword ptr [edi]                      // PERK_bonus_awareness
  push dword ptr [edi+73*4]                 // PERK_gecko_skinning
  push edi
  mov  eax, ebx
  call isRealParty
  test eax, eax
  jz   clearPerks
  call perkGetLevelData_
  xchg esi, eax
  rep  movsd
  xor  eax, eax
  jmp  skipPerks
clearPerks:
  rep  stosd
skipPerks:
  mov  ds:[_free_perk], al
  mov  ds:[_curr_pc_stat], eax
  mov  ds:[_sneak_working], eax
  pop  edi
  pop  eax
  xchg [edi+73*4], eax                      // PERK_gecko_skinning
  mov  party_PERK_gecko_skinning, eax
  pop  eax
  xchg [edi], eax                           // PERK_bonus_awareness
  mov  party_PERK_bonus_awareness, eax
  mov  eax, [ebx+0x64]
  mov  ds:[_inven_pid], eax
  mov  ds:[_obj_dude], ebx
  mov  ds:[_inven_dude], ebx
  mov  ds:[_stack], ebx
  mov  esi, ds:[_game_global_vars]
  add  esi, 21*4                            // esi->GVAR_NUKA_COLA_ADDICT
  push esi
  mov  edi, offset real_drug_gvar
  mov  ecx, 6
  push ecx
  rep  movsd
  pop  ecx
  pop  edi
  mov  eax, [edi+296*4-21*4]                // eax = GVAR_ADDICT_JET
  mov  real_jet_gvar, eax
  push edi
  mov  edx, _drugInfoList
  mov  esi, ebx                             // _obj_dude
loopDrug:
  mov  eax, [edx]                           // eax = drug_pid
  call item_d_check_addict_
  test eax, eax                             // Есть зависимость?
  jz   noAddict                             // Нет
  xor  eax, eax
  inc  eax
noAddict:
  mov  [edi], eax
  add  edx, 12
  add  edi, 4
  loop loopDrug
  test eax, eax                             // Есть зависимость к алкоголю (пиво)?
  jnz  skipBooze                            // Да
  mov  eax, [edx]                           // PID_BOOZE
  call item_d_check_addict_
  mov  [edi-4], eax                         // GVAR_ALCOHOL_ADDICT
skipBooze:
  mov  eax, [edx+12]                        // PID_JET
  call item_d_check_addict_
  test eax, eax
  jz   noJetAddict
  xor  eax, eax
  inc  eax
noJetAddict:
  pop  edi
  mov  [edi+296*4-21*4], eax                // GVAR_ADDICT_JET
  xor  eax, eax
  dec  eax
  call item_d_check_addict_
  mov  edx, 4
  test eax, eax
  mov  eax, edx
  jz   unsetAddict
  call pc_flag_on_
  jmp  skip
unsetAddict:
  call pc_flag_off_
skip:
  push edx
  mov  eax, offset real_tag_skill
  call skill_get_tags_
  mov  edi, _tag_skill
  pop  ecx
  xor  eax, eax
  dec  eax
  rep  stosd
  mov  edx, eax
  call trait_set_
  mov  ds:[_Experience_], eax
// get active hand by weapon anim code
  mov  edx, [ebx+0x20]                      // fid
  and  edx, 0x0F000
  sar  edx, 0xC                             // edx = current weapon anim code as seen in hands
  xor  ecx, ecx                             // Левая рука
  mov  eax, ebx
  call inven_right_hand_
  test eax, eax                             // Есть вещь в правой руке?
  jz   checkAnim                            // Нет
  push eax
  call item_get_type_
  cmp  eax, item_type_weapon
  pop  eax
  jne  setActiveHand                        // Нет
  call item_w_anim_code_
checkAnim:
  cmp  eax, edx                             // Анимация одинаковая?
  jne  setActiveHand                        // Нет
  inc  ecx                                  // Правая рука
setActiveHand:
  mov  ds:[_itemCurrentItem], ecx
  mov  eax, ebx
  call inven_right_hand_
  test eax, eax                             // Есть вещь в правой руке?
  jz   noRightHand                          // Нет
  push eax
  call PartyControl_CanUseWeapon
  test eax, eax
  pop  eax
  jnz  noRightHand
  and  byte ptr [eax+0x27], 0xFD            // Сбрасываем флаг вещи в правой руке
noRightHand:
  xchg ebx, eax
  call inven_left_hand_
  test eax, eax                             // Есть вещь в левой руке?
  jz   noLeftHand                           // Нет
  push eax
  call PartyControl_CanUseWeapon
  test eax, eax
  pop  eax
  jnz  noLeftHand
  and  byte ptr [eax+0x27], 0xFE            // Сбрасываем флаг вещи в левой руке
noLeftHand:
  popad
  retn
 }
}

// restore dude state
void __declspec(naked) RestoreDudeState() {
 __asm {
  pushad
  mov  eax, offset real_pc_name
  call critter_pc_set_name_
  mov  eax, real_last_level
  mov  ds:[_last_level], eax
  mov  eax, real_Level
  mov  ds:[_Level_], eax
  mov  eax, real_Experience
  mov  ds:[_Experience_], eax
  mov  eax, real_free_perk
  mov  ds:[_free_perk], al
  mov  eax, real_unspent_skill_points
  mov  ds:[_curr_pc_stat], eax
  mov  eax, real_map_elevation
  mov  ds:[_map_elevation], eax
  mov  eax, real_sneak_working
  mov  ds:[_sneak_working], eax
  mov  eax, real_bbox_sneak
  mov  [bboxes+12+8], eax

/*  mov  eax, ds:[_itemCurrentItem]
  imul eax, eax, 24
  mov  edx, ds:[_itemButtonItems+0x10][eax] // mode
  dec  edx
  mov  eax, ds:[_perkLevelDataList]
  mov  [eax+36*4], edx                      // PERK_friendly_foe*/

  mov  eax, real_hand
  mov  ds:[_itemCurrentItem], eax
  mov  edx, real_trait2
  mov  eax, real_trait
  call trait_set_
  mov  esi, offset real_itemButtonItems
  mov  edi, _itemButtonItems
  mov  ecx, 6*2
  rep  movsd
  mov  esi, offset real_drug_gvar
  mov  edi, ds:[_game_global_vars]
  mov  eax, real_jet_gvar
  mov  [edi+296*4], eax                     // GVAR_ADDICT_JET
  add  edi, 21*4                            // esi->GVAR_NUKA_COLA_ADDICT
  mov  ecx, 6
  rep  movsd
  mov  edx, 4
  mov  eax, offset real_tag_skill
  call skill_set_tags_
  mov  eax, ds:[_obj_dude]
  mov  ecx, real_dude
  push ecx
  mov  ds:[_obj_dude], ecx
  mov  ds:[_inven_dude], ecx
  mov  ds:[_stack], ecx
  mov  esi, [ecx+0x64]
  mov  ds:[_inven_pid], esi
  mov  esi, ds:[_perkLevelDataList]
  mov  ecx, party_PERK_bonus_awareness
  mov  [esi], ecx                           // PERK_bonus_awareness
  mov  ecx, party_PERK_gecko_skinning
  mov  [esi+73*4], ecx                      // PERK_gecko_skinning
  mov  ecx, PERK_count
  push ecx
  push esi
  call isRealParty
  test eax, eax
  jz   skipPerks
  call perkGetLevelData_
  xchg edi, eax
  rep  movsd
skipPerks:
  pop  edi
  pop  ecx
  mov  esi, offset real_perkLevelDataList
  rep  movsd
  xor  eax, eax
  mov  IsControllingNPC, eax
  dec  eax
  call item_d_check_addict_
  test eax, eax
  mov  eax, 4
  jz   unsetAddict
  call pc_flag_on_
  jmp  skip
unsetAddict:
  call pc_flag_off_
skip:
  pop  edx
  mov  eax, real_sneak_queue_time
  test eax, eax
  jz   noSneak
  mov  ecx, 10
  xor  ebx, ebx
  call queue_add_
noSneak:
  mov  eax, DelayedExperience
  test eax, eax
  jz   end
  call stat_pc_add_experience_
  mov  DelayedExperience, eax
end:
  popad
  retn
 }
}

void __declspec(naked) PartyControl_PrintWarning() {
 __asm {
  push edx
  push ebx
  sub  esp, 16
  mov  edx, esp
  mov  ebx, 675                             // 'I can't do that.'/'Я не могу этого сделать.'
  mov  eax, _proto_main_msg_file
  call getmsg_
  call display_print_
  xor  eax, eax
  add  esp, 16
  pop  ebx
  pop  edx
  retn
 }
}

static void _declspec(naked) CombatWrapper_v2() {
 __asm {
  pushad
  cmp  eax, ds:[_obj_dude]
  jne  skip
  xor  edx, edx
  cmp  ds:[_combatNumTurns], edx
  je   skipControl                          // Это первый ход
  mov  eax, [eax+0x4]                       // tile_num
  add  edx, 3
  call tile_scroll_to_
  jmp  skipControl
skip:
  push eax
  call IsInPidList
  and  eax, 0xFF
  test eax, eax
  jz   skipControl
  cmp  Mode, eax                            // control all critters?
  je   npcControl
  popad
  pushad
  call isPartyMember_
  test eax, eax
  jnz  npcControl
skipControl:
  popad
  jmp  combat_turn_
npcControl:
  mov  IsControllingNPC, eax
  popad
  pushad
  xchg ebx, eax                             // ebx = npc
  call SaveDudeState
  call intface_redraw_

/*  mov  eax, ds:[_itemCurrentItem]
  imul edx, eax, 24
  mov  eax, ds:[_perkLevelDataList]
  mov  eax, [eax+36*4]                      // PERK_friendly_foe
  inc  eax
  mov  ds:[_itemButtonItems+0x10][edx], eax // mode
  call intface_redraw_items_*/

  mov  eax, [ebx+0x4]                       // tile_num
  mov  edx, 3
  call tile_scroll_to_
  xchg ebx, eax                             // eax = npc
  xor  edx, edx
  push edx
  call combat_turn_
  pop  ecx
  xchg ecx, eax
  cmp  IsControllingNPC, eax                // if game was loaded during turn, RestoreDudeState()
  je   skipRestore                          // was called and already restored state
  call RestoreDudeState
  call intface_redraw_
skipRestore:
  test ecx, ecx                             // Нормальное завершение хода?
  popad
  jz   end                                  // Да
// выход/загрузка/побег/смерть
  test byte ptr [eax+0x44], 0x80            // DAM_DEAD
  jnz  end
  xor  eax, eax
  dec  eax
  retn
end:
  xor  eax, eax
  retn
 }
}

// hack to exit from this function safely when you load game during NPC turn
static void _declspec(naked) combat_add_noncoms_hook() {
 __asm {
  call CombatWrapper_v2
  inc  eax
  jnz  end
  mov  ds:[_list_com], eax
  mov  ecx, [esp+0x4]
end:
  retn
 }
}

static void _declspec(naked) PrintLevelWin_hook() {
 __asm {
  xor  edi, edi
  cmp  IsControllingNPC, edi
  je   end
  dec  edi
  xchg edi, eax
end:
  mov  edi, eax
  cmp  eax, -1
  retn
 }
}

static void _declspec(naked) Save_as_ASCII_hook() {
 __asm {
  xor  ebx, ebx
  cmp  IsControllingNPC, ebx
  je   end
  dec  ebx
  xchg ebx, eax
end:
  mov  ebx, 649
  retn
 }
}

static void _declspec(naked) handle_inventory_hook() {
 __asm {
  xor  ebx, ebx
  mov  edx, eax                             // edx = _inven_dude
  call inven_worn_
  test eax, eax
  jz   end
  cmp  IsControllingNPC, ebx
  je   end
  and  byte ptr [eax+0x27], 0xFB            // Сбрасываем флаг одетой брони
  push eax
  push edx
  inc  ebx
  xchg edx, eax                             // eax = source, edx = armor
  call item_remove_mult_
  pop  edx                                  // edx = source
  pop  ebx                                  // ebx = armor
  inc  eax                                  // Удалили?
  jnz  skip                                 // Да
// Не смогли удалить, поэтому снимем броню с учётом уменьшения КБ
  push edx
  push eax
  xchg ebx, eax                             // ebx = newarmor, eax = oldarmor
  xchg edx, eax                             // edx = oldarmor, eax = source
  call correctFidForRemovedItem_
  pop  ebx
  pop  edx
skip:
  xchg ebx, eax                             // eax = armor
  mov  HiddenArmor, eax
end:
  retn
 }
}

static void _declspec(naked) handle_inventory_hook1() {
 __asm {
  xor  edx, edx
  cmp  IsControllingNPC, edx
  je   end
  push eax
  mov  edx, HiddenArmor
  test edx, edx
  jz   skip
  or   byte ptr [edx+0x27], 4               // Устанавливаем флаг одетой брони
  xor  ebx, ebx
  inc  ebx
  call item_add_force_
  xor  edx, edx
skip:
  mov  HiddenArmor, edx
  pop  eax
end:
  jmp  inven_worn_
 }
}

static void _declspec(naked) combat_input_hook() {
 __asm {
  cmp  ebx, 0x20                            // Space (окончание хода)?
  jne  skip
space:
  pop  ebx                                  // Уничтожаем адрес возврата
  mov  ebx, 0x20                            // Space (окончание хода)
  push 0x4228A8
  retn
skip:
  cmp  dword ptr IsControllingNPC, 0
  je   end
  cmp  ebx, 0xD                             // Enter (завершение боя)?
  je   space                                // Да
end:  
  retn
 }
}

static void __declspec(naked) action_skill_use_hook() {
 __asm {
  cmp  eax, SKILL_SNEAK
  jne  skip
  xor  eax, eax
  cmp  IsControllingNPC, eax
  je   end
  call PartyControl_PrintWarning
skip:
  pop  eax                                  // Уничтожаем адрес возврата
  xor  eax, eax
  dec  eax
end:
  retn
 }
}

static void __declspec(naked) action_use_skill_on_hook() {
 __asm {
  cmp  IsControllingNPC, eax
  je   end
  jmp  PartyControl_PrintWarning
end:
  jmp  pc_flag_toggle_
 }
}

static void __declspec(naked) damage_object_hook() {
 __asm {
  push ecx
  xor  ecx, ecx
  cmp  IsControllingNPC, ecx
  je   skip
  cmp  edx, ds:[_obj_dude]
  jne  skip
  mov  ecx, edx
  call RestoreDudeState
  push eax
  call intface_redraw_
  pop  eax
skip:
  push eax
  call scr_set_objs_
  pop  eax
  mov  edx, destroy_p_proc
  call exec_script_proc_
  jecxz end
  inc  ebx
  mov  IsControllingNPC, ebx
  mov  ebx, ecx
  call SaveDudeState
  call intface_redraw_

/*  mov  eax, ds:[_itemCurrentItem]
  imul edx, eax, 24
  mov  eax, ds:[_perkLevelDataList]
  mov  eax, [eax+36*4]                      // PERK_friendly_foe
  inc  eax
  mov  ds:[_itemButtonItems+0x10][edx], eax // mode
  call intface_redraw_items_*/

end:
  pop  ecx
  pop  eax                                  // Уничтожаем адрес возврата
  push 0x4250E9
  retn
 }
}

static void __declspec(naked) op_give_exp_points_hook() {
 __asm {
  xor  eax, eax
  cmp  IsControllingNPC, eax
  je   skip
  add  DelayedExperience, esi
  retn
skip:
  xchg esi, eax
  jmp  stat_pc_add_experience_
 }
}

static void __declspec(naked) adjust_fid_hook() {
 __asm {
  mov  edx, ds:[_i_worn]
  xor  eax, eax
  cmp  IsControllingNPC, eax
  je   end
  mov  eax, HiddenArmor
  test eax, eax
  jz   skip
  xchg edx, eax
skip:
  push edx
  push ecx
  push dword ptr ds:[_inven_dude]           // указатель на нпс
  push edx                                  // указатель на объект (броню)
  call ChangeArmorFid
  pop  ecx
  pop  edx
  test eax, eax
  jz   end
  xor  edx, edx
  and  eax, 0xFFF
  cmp  eax, esi
  je   end
  xchg esi, eax
end:
  push 0x471739
  retn
 }
}

static void __declspec(naked) partyMemberCopyLevelInfo_hook() {
 __asm {
  call inven_left_hand_
  test eax, eax
  jz   end
  and  byte ptr [eax+0x27], 0xFC            // Сбрасываем флаг оружия в руке
  xor  eax, eax
end:
  retn
 }
}

static void __declspec(naked) refresh_box_bar_win_hook() {
 __asm {
  cmp  IsControllingNPC, eax
  jne  end
  jmp  is_pc_flag_
end:
  inc  eax
  retn
 }
}

void PartyControlInit() {
 Mode = GetPrivateProfileIntA("Misc", "ControlCombat", 0, ini);
 if (Mode == 1 || Mode == 2) {
  NameBox = new BYTE[2730];
  char pidbuf[512];
  pidbuf[511]=0;
  if (GetPrivateProfileStringA("Misc", "ControlCombatPIDList", "", pidbuf, 511, ini)) {
   char* ptr = pidbuf;
   char* comma;
   while (true) {
    comma = strchr(ptr, ',');
    if (!comma) 
     break;
    *comma = 0;
    if (strlen(ptr) > 0)
     Chars.push_back((WORD)strtoul(ptr, 0, 0));
    ptr = comma + 1;
   }
   if (strlen(ptr) > 0)
    Chars.push_back((WORD)strtoul(ptr, 0, 0));
  }
  dlog_f(" Mode %d, Chars read: %d.", DL_INIT, Mode, Chars.size());
  HookCall(0x422354, &combat_add_noncoms_hook);
  HookCall(0x422E20, &CombatWrapper_v2);
  MakeCall(0x434AAC, &PrintLevelWin_hook, false);
  MakeCall(0x43964E, &Save_as_ASCII_hook, false);
  HookCall(0x46E8BA, &handle_inventory_hook);
  HookCall(0x46EC0A, &handle_inventory_hook1);
  SafeWrite32(0x471E48, 152);               // Ширина текста 152, а не 80 
  MakeCall(0x422879, &combat_input_hook, false);
  MakeCall(0x4124E0, &action_skill_use_hook, false);
  HookCall(0x41279A, &action_use_skill_on_hook);
  HookCall(0x4250D7, &damage_object_hook);
  HookCall(0x454218, &op_give_exp_points_hook);
  MakeCall(0x471733, &adjust_fid_hook, true);
  HookCall(0x495F00, &partyMemberCopyLevelInfo_hook);
  HookCall(0x461528, &refresh_box_bar_win_hook);
 } else dlog(" Disabled.", DL_INIT);
}

void PartyControlExit() {
 if (Mode == 1 || Mode == 2) delete[] NameBox;
}
