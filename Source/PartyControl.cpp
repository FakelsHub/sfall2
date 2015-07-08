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

#include "Define.h"
#include "FalloutEngine.h"
#include "PartyControl.h"
#include <vector>

DWORD IsControllingNPC = 0;
static DWORD HiddenArmor = 0;

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
static DWORD real_dude;
static DWORD real_hand;
static DWORD real_trait;
static DWORD real_trait2;
static DWORD real_itemButtonItems[(6*4)*2];
static DWORD real_perkLevelDataList[PERK_count];
static DWORD real_drug_gvar[6];
static DWORD real_jet_gvar;
static DWORD real_tag_skill[4];

struct sMessage {
 DWORD number;
 DWORD flags;
 char* audio;
 char* message;
};

static void __declspec(naked) isRealParty() {
 __asm {
  push edx
  push ecx
  push ebx
  mov  ecx, dword ptr ds:[0x519D9C]         // _partyMemberMaxCount
  dec  ecx
  jecxz skip                                // ���쪮 ��ப
  mov  edx, dword ptr ds:[0x519DA0]         // _partyMemberPidList
  mov  ebx, dword ptr [eax+0x64]            // pid
loopPid:
  add  edx, 4
  cmp  ebx, dword ptr [edx]
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

static void __declspec(naked) CanUseWeapon() {
 __asm {
  push edi
  push esi
  push edx
  push ecx
  push ebx
  mov  edi, eax
  call item_get_type_
  cmp  eax, 3                               // �� item_type_weapon?
  jne  canUse                               // ���
  mov  eax, edi                             // eax=item
  mov  edx, 2                               // hit_right_weapon_primary
  call item_w_anim_code_
  xchg ecx, eax                             // ecx=ID1=Weapon code
  xchg edi, eax                             // eax=item
  call item_w_anim_weap_
  xchg ebx, eax                             // ebx=ID2=Animation code
  mov  esi, dword ptr ds:[0x519058]         // _inven_dude
  mov  edx, dword ptr [esi+0x20]            // fid
  and  edx, 0xFFF                           // edx=Index
  mov  eax, dword ptr [esi+0x1C]            // cur_rot
  inc  eax
  push eax                                  // ID3=Direction code
  mov  eax, 1                               // ObjType_Critter
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
  if (*it == pid) {
   return true;
  }
 }
 return false;
}

// save "real" dude state
static void __declspec(naked) SaveDudeState() {
 __asm {
  push edi
  push esi
  push edx
  push ecx
  push ebx
  mov  esi, 0x56D75C                        // _pc_name
  mov  edi, offset real_pc_name
  mov  ecx, 32/4
  rep  movsd
  mov  eax, ebx
  call critter_name_
  call critter_pc_set_name_
  mov  eax, dword ptr ds:[0x5707B4]         // _last_level
  mov  real_last_level, eax
  mov  eax, dword ptr ds:[0x6681B0]         // Level
  mov  real_Level, eax
  mov  eax, ebx
  call isRealParty
  test eax, eax
  jz   saveLevel
  call partyMemberGetCurLevel_
saveLevel:
  mov  dword ptr ds:[0x6681B0], eax         // Level
  mov  dword ptr ds:[0x5707B4], eax         // _last_level
  mov  eax, dword ptr ds:[0x6681B4]         // Experience
  mov  real_Experience, eax
  movzx eax, byte ptr ds:[0x570A29]         // _free_perk
  mov  real_free_perk, eax
  mov  eax, dword ptr ds:[0x6681AC]         // _curr_pc_stat
  mov  real_unspent_skill_points, eax
  mov  eax, dword ptr ds:[0x519578]         // _map_elevation
  mov  real_map_elevation, eax
  mov  eax, dword ptr ds:[0x56D77C]         // _sneak_working
  mov  real_sneak_working, eax
  mov  eax, dword ptr ds:[_obj_dude]        // _obj_dude
  mov  real_dude, eax
  mov  eax, dword ptr ds:[0x518F78]         // _itemCurrentItem
  mov  real_hand, eax
  mov  edx, offset real_trait2
  mov  eax, offset real_trait
  call trait_get_
  mov  esi, 0x5970F8                        // _itemButtonItems
  mov  edi, offset real_itemButtonItems
  mov  ecx, (6*4)*2
  rep  movsd
  mov  esi, dword ptr ds:[0x51C120]         // _perkLevelDataList
  push esi
  mov  edi, offset real_perkLevelDataList
  mov  ecx, PERK_count
  push ecx
  rep  movsd
  pop  ecx
  pop  edi
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
  mov  byte ptr ds:[0x570A29], al           // _free_perk
  mov  dword ptr ds:[0x6681AC], eax         // _curr_pc_stat
  mov  dword ptr ds:[0x56D77C], eax         // _sneak_working
  mov  eax, dword ptr [ebx+0x64]
  mov  dword ptr ds:[0x51905C], eax         // _inven_pid
  mov  dword ptr ds:[_obj_dude], ebx        // _obj_dude
  mov  dword ptr ds:[0x519058], ebx         // _inven_dude
  mov  esi, dword ptr ds:[0x5186C0]         // _game_global_vars
  add  esi, 21*4                            // esi->GVAR_NUKA_COLA_ADDICT
  push esi
  mov  edi, offset real_drug_gvar
  mov  ecx, 6
  push ecx
  rep  movsd
  pop  ecx
  pop  edi
  mov  eax, dword ptr [edi+296*4-21*4]      // eax = GVAR_ADDICT_JET
  mov  real_jet_gvar, eax
  push edi
  mov  edx, 0x5191CC                        // _drugInfoList
  mov  esi, ebx                             // _obj_dude
loopDrug:
  mov  eax, dword ptr [edx]                 // eax = drug_pid
  call item_d_check_addict_
  test eax, eax                             // ���� ����ᨬ����?
  jz   noAddict                             // ���
  xor  eax, eax
  inc  eax
noAddict:
  mov  dword ptr [edi], eax
  add  edx, 12
  add  edi, 4
  loop loopDrug
  test eax, eax                             // ���� ����ᨬ���� � �������� (����)?
  jnz  skipBooze                            // ��
  mov  eax, dword ptr [edx]                 // PID_BOOZE
  call item_d_check_addict_
  mov  dword ptr [edi-4], eax               // GVAR_ALCOHOL_ADDICT
skipBooze:
  mov  eax, dword ptr [edx+12]              // PID_JET
  call item_d_check_addict_
  test eax, eax
  jz   noJetAddict
  xor  eax, eax
  inc  eax
noJetAddict:
  pop  edi
  mov  dword ptr [edi+296*4-21*4], eax      // GVAR_ADDICT_JET
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
  mov  edi, 0x668070                        // _tag_skill
  pop  ecx
  xor  eax, eax
  dec  eax
  rep  stosd
  mov  edx, eax
  call trait_set_
  mov  dword ptr ds:[0x6681B4], eax         // Experience
// get active hand by weapon anim code
  mov  edx, dword ptr [ebx+0x20]            // fid
  and  edx, 0x0F000
  sar  edx, 0xC                             // edx = current weapon anim code as seen in hands
  xor  ecx, ecx                             // ����� �㪠
  mov  eax, ebx
  call inven_right_hand_
  test eax, eax                             // ���� ���� � �ࠢ�� �㪥?
  jz   setActiveHand                        // ���
  push eax
  call item_get_type_
  cmp  eax, 3                               // �� item_type_weapon?
  pop  eax
  jne  setActiveHand                        // ���
  call item_w_anim_code_
  cmp  eax, edx                             // ������� ����������?
  jne  setActiveHand                        // ���
  inc  ecx                                  // �ࠢ�� �㪠
setActiveHand:
  mov  dword ptr ds:[0x518F78], ecx         // _itemCurrentItem
  mov  eax, ebx
  call inven_right_hand_
  test eax, eax                             // ���� ���� � �ࠢ�� �㪥?
  jz   noRightHand                          // ���
  push eax
  call CanUseWeapon
  test eax, eax
  pop  eax
  jnz  noRightHand
  and  byte ptr [eax+0x27], 0xFD            // ����뢠�� 䫠� ��� � �ࠢ�� �㪥
noRightHand:
  xchg ebx, eax
  call inven_left_hand_
  test eax, eax                             // ���� ���� � ����� �㪥?
  jz   noLeftHand                           // ���
  push eax
  call CanUseWeapon
  test eax, eax
  pop  eax
  jnz  noLeftHand
  and  byte ptr [eax+0x27], 0xFE            // ����뢠�� 䫠� ��� � ����� �㪥
noLeftHand:
  pop  ebx
  pop  ecx
  pop  edx
  pop  esi
  pop  edi
  retn
 }
}

// restore dude state
static void __declspec(naked) RestoreDudeState() {
 __asm {
  push edi
  push esi
  push edx
  push ecx
  push eax
  mov  eax, offset real_pc_name
  call critter_pc_set_name_
  mov  eax, real_last_level
  mov  dword ptr ds:[0x5707B4], eax         // _last_level
  mov  eax, real_Level
  mov  dword ptr ds:[0x6681B0], eax         // Level
  mov  eax, real_Experience
  mov  dword ptr ds:[0x6681B4], eax         // Experience
  mov  eax, real_free_perk
  mov  byte ptr ds:[0x570A29], al           // _free_perk
  mov  eax, real_unspent_skill_points
  mov  dword ptr ds:[0x6681AC], eax         // _curr_pc_stat
  mov  eax, real_map_elevation
  mov  dword ptr ds:[0x519578], eax         // _map_elevation
  mov  eax, real_sneak_working
  mov  dword ptr ds:[0x56D77C], eax         // _sneak_working
  mov  eax, real_hand
  mov  dword ptr ds:[0x518F78], eax         // _itemCurrentItem
  mov  edx, real_trait2
  mov  eax, real_trait
  call trait_set_
  mov  esi, offset real_itemButtonItems
  mov  edi, 0x5970F8                        // _itemButtonItems
  mov  ecx, (6*4)*2
  rep  movsd
  mov  esi, offset real_drug_gvar
  mov  edi, dword ptr ds:[0x5186C0]         // _game_global_vars
  mov  eax, real_jet_gvar
  mov  dword ptr [edi+296*4], eax           // GVAR_ADDICT_JET
  add  edi, 21*4                            // esi->GVAR_NUKA_COLA_ADDICT
  mov  ecx, 6
  rep  movsd
  mov  edx, 4
  mov  eax, offset real_tag_skill
  call skill_set_tags_
  mov  eax, dword ptr ds:[_obj_dude]        // _obj_dude
  mov  ecx, real_dude
  mov  dword ptr ds:[_obj_dude], ecx        // _obj_dude
  mov  dword ptr ds:[0x519058], ecx         // _inven_dude
  mov  esi, dword ptr [ecx+0x64]
  mov  dword ptr ds:[0x51905C], esi         // _inven_pid
  mov  ecx, PERK_count
  push ecx
  mov  esi, dword ptr ds:[0x51C120]         // _perkLevelDataList
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
  pop  eax
  pop  ecx
  pop  edx
  pop  esi
  pop  edi
  retn
 }
}

sMessage cantdo = {675, 0, 0, 0};       // '� �� ���� �⮣� ᤥ����.'
static void __declspec(naked) PrintWarning() {
 __asm {
  push edx
  lea  edx, cantdo
  mov  eax, 0x6647FC                        // _proto_main_msg_file
  call message_search_
  cmp  eax, 1
  jne  end
  mov  eax, cantdo.message
  call display_print_
end:
  xor  eax, eax
  pop  edx
  retn
 }
}

// 1 skip handler, -1 don't skip
int __stdcall PartyControl_SwitchHandHook(TGameObj* weapon) {
 DWORD result;
 __asm {
  push eax
  cmp  IsControllingNPC, 0
  je   dontSkipHandler
  mov  eax, weapon
  call CanUseWeapon
  test eax, eax
  jnz  dontSkipHandler
  call PrintWarning
  inc  eax
  jmp  end
dontSkipHandler:
  xor  eax, eax
  dec  eax
end:  
  mov  result, eax
  pop  eax
 }
 return result;
}

static void _declspec(naked) CombatWrapper_v2() {
 __asm {
  pushad
  cmp  eax, dword ptr ds:[_obj_dude]        // _obj_dude
  jne  skip
  xor  edx, edx
  cmp  dword ptr ds:[0x510940], edx         // _combatNumTurns
  je   skipControl                          // �� ���� 室
  mov  eax, dword ptr [eax+0x4]             // tile_num
  add  edx, 2
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
  mov  eax, dword ptr [ebx+0x4]             // tile_num
  mov  edx, 2
  call tile_scroll_to_
  xchg ebx, eax                             // eax = npc
  call combat_turn_
  xchg ecx, eax
  cmp  IsControllingNPC, 0                  // if game was loaded during turn, PartyControlReset()
  je   skipRestore                          // was called and already restored state
  call RestoreDudeState
  call intface_redraw_
skipRestore:
  test ecx, ecx                             // ��ଠ�쭮� �����襭�� 室�?
  popad
  jz   end                                  // ��
// ��室/����㧪�/�����/ᬥ���
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
  test eax, eax
  jnz  end
  mov  dword ptr ds:[0x56D394], eax         // _list_com
  mov  ecx, dword ptr [esp+0x4]
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

static void _declspec(naked) inven_pickup_hook() {
 __asm {
  call item_get_type_
  test eax, eax                             // �� item_type_armor?
  jnz  end                                  // ���
  cmp  IsControllingNPC, eax
  je   end
  call PrintWarning
  dec  eax
end:
  retn
 }
}

static void _declspec(naked) handle_inventory_hook() {
 __asm {
  mov  edx, eax                             // edx = _inven_dude
  call inven_worn_
  test eax, eax
  jz   end
  cmp  IsControllingNPC, 0
  je   end
  mov  HiddenArmor, eax
  pushad
  push edx
  mov  ebx, 1
  xchg edx, eax
  call item_remove_mult_
  pop  edx
nextArmor:
  mov  eax, edx
  call inven_worn_
  test eax, eax
  jz   noArmor
  and  byte ptr [eax+0x27], 0xFB            // ����뢠�� 䫠� ���⮩ �஭�
  jmp  nextArmor
noArmor:
  popad
end:
  retn
 }
}

static void _declspec(naked) handle_inventory_hook1() {
 __asm {
  cmp  IsControllingNPC, 0
  je   end
  pushad
  mov  edx, HiddenArmor
  test edx, edx
  jz   skip
  or   byte ptr [edx+0x27], 4               // ��⠭�������� 䫠� ���⮩ �஭�
  mov  ebx, 1
  call item_add_force_
  xor  edx, edx
skip:
  mov  HiddenArmor, edx
  popad
end:
  jmp  inven_worn_
 }
}

static void _declspec(naked) display_stats_hook() {
 __asm {
  call item_total_weight_
  xchg edx, eax                             // edx = ��� ��饩
  xor  eax, eax
  cmp  IsControllingNPC, eax                // ����஫��㥬� ���ᮭ��?
  je   end                                  // ���
  mov  eax, HiddenArmor
  test eax, eax                             // � ���� ���� �஭�?
  jz   end                                  // ���
  call item_weight_
end:
  add  eax, edx
  retn
 }
}

static const DWORD combat_input_hook_End = 0x4228A8;
static void _declspec(naked) combat_input_hook() {
 __asm {
  cmp  ebx, 0x20                            // Space (����砭�� 室�)?
  jne  skip
space:
  pop  ebx
  mov  ebx, 0x20                            // Space (����砭�� 室�)
  jmp  combat_input_hook_End
skip:
  cmp  IsControllingNPC, 0
  je   end
  cmp  ebx, 0xD                             // Enter (�����襭�� ���)?
  je   space                                // ��
end:  
  retn
 }
}

static const DWORD action_skill_use_hook_End = 0x4124F9;
static void __declspec(naked) action_skill_use_hook() {
 __asm {
  cmp  eax, 8                               // SKILL_SNEAK
  jne  end
  xor  eax, eax
  cmp  IsControllingNPC, eax
  jne  skip
  retn
skip:
  call PrintWarning
end:
  pop  eax                                  // ����⮦��� ���� ������
  jmp  action_skill_use_hook_End
 }
}

static void __declspec(naked) action_use_skill_on_hook() {
 __asm {
  cmp  IsControllingNPC, eax
  je   end
  call PrintWarning
  retn
end:
  jmp  pc_flag_toggle_
 }
}

void PartyControlInit() {
 Mode = GetPrivateProfileIntA("Misc", "ControlCombat", 0, ini);
 if (Mode == 1 || Mode == 2) {
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
  HookCall(0x47139C, &inven_pickup_hook);
  HookCall(0x46E8BA, &handle_inventory_hook);
  HookCall(0x46EC0A, &handle_inventory_hook1);
  SafeWrite32(0x471E48, 152);               // ��ਭ� ⥪�� 152, � �� 80 
  HookCall(0x4725F0, &display_stats_hook);
  MakeCall(0x422879, &combat_input_hook, false);
  MakeCall(0x4124E0, &action_skill_use_hook, false);
  HookCall(0x41279A, &action_use_skill_on_hook);
 } else dlog(" Disabled.", DL_INIT);
}

void __stdcall PartyControlReset() {
 __asm {
  cmp  IsControllingNPC, 0
  je   end
  call RestoreDudeState
end:
 }
}