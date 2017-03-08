/*
 *    sfall
 *    Copyright (C) 2011  Timeslip
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

#include <stdio.h>
#include "Bugs.h"
#include "Define.h"
#include "FalloutEngine.h"
#include "HookScripts.h"
#include "input.h"
#include "Inventory.h"
#include "LoadGameHook.h"
#include "PartyControl.h"

bool UseScrollWheel = true;

static DWORD InvSizeLimitMode = 0;
static int InvSizeLimit = 0;
static DWORD ReloadWeaponKey = 0;

static const int PartyMax = 15;
static int npcCount = 0;
static char iniArmor[MAX_PATH];

struct npcArmor {
 int Pid;
 int Default;
 int Leather;
 int Power;
 int Advanced;
 int Metal;
 int Cured;
 int Combat;
 int Robe;
};

static npcArmor armors[PartyMax];
static char MsgBuf[32];

static void __declspec(naked) critter_max_size() {
 __asm {
  push edx
  mov  edx, InvSizeLimitMode
  dec  edx
  jz   PlayerOnly
  dec  edx
  jz   PartyAndPlayer
  dec  edx
  jz   run                                  // Все персонажи
fail:
  xor  eax, eax
  dec  eax
  jmp  end
PartyAndPlayer:
  push eax
  call isPartyMember_
  test eax, eax                             // Собутыльник?
  pop  eax
  jnz  run                                  // Да
PlayerOnly:
  cmp  eax, ds:[_obj_dude]
  jne  fail
run:
  mov  edx, STAT_unused
  call stat_level_
end:
  pop  edx
  retn
 }
}

static void __declspec(naked) critter_curr_size() {
 __asm {
  push esi
  push edi
  push edx
  mov  edi, eax
  call item_c_curr_size_
  xchg esi, eax
  mov  eax, [edi+0x20]                      // pobj.fid
  and  eax, 0xF000000
  sar  eax, 0x18
  cmp  eax, ObjType_Critter
  jne  end
  mov  eax, edi
  call inven_right_hand_
  mov  edx, eax
  call item_size_
  add  esi, eax
  mov  eax, edi
  call inven_left_hand_
  cmp  edx, eax
  je   skip
  call item_size_
  add  esi, eax
skip:
  xchg edi, eax
  call inven_worn_
  call item_size_
  add  esi, eax
end:
  xchg esi, eax
  pop  edx
  pop  edi
  pop  esi
  retn
 }
}

static void __declspec(naked) stat_level_hook() {
 __asm {
  mov  edx, 0x4AF0D0
  cmp  esi, STAT_max_move_points
  je   end
  cmp  esi, STAT_unused
  jne  skip
  mov  edx, InvSizeLimitMode
  dec  edx
  jz   PlayerOnly
  dec  edx
  jz   PartyAndPlayer
  dec  edx
  jz   run                                  // Все персонажи
  jmp  skip
PartyAndPlayer:
  push eax
  mov  eax, ebx
  call isPartyMember_
  test eax, eax                             // Собутыльник?
  pop  eax
  jnz  run                                  // Да
PlayerOnly:
  cmp  ebx, edi                             // source == _obj_dude?
  jne  skip
run:
  xchg edx, eax                             // edx = bonus
  mov  eax, InvSizeLimit
  cmp  edx, eax                             // bonus == InvSizeLimit?
  je   skip                                 // Да
  push ebx
  push eax
  sub  ecx, edx                             // Вычитаем старый бонус
  add  ecx, eax                             // Добавляем новый бонус
  xchg ebx, eax                             // eax = source, ebx = InvSizeLimit
  mov  edx, esi
  call stat_set_bonus_
  pop  eax
  pop  ebx
skip:
  mov  edx, 0x4AF0FC
end:
  jmp  edx
 }
}

static void __declspec(naked) op_set_critter_stat_hook() {
 __asm {
  cmp  dword ptr [esp+4+4], STAT_unused
  jne  end
  pop  ebx                                  // Уничтожаем адрес возврата
  mov  ebx, 0x455D8A
  push ebx
end:
  mov  ebx, 3
  retn
 }
}

static void __declspec(naked) stat_set_base_hook() {
 __asm {
  cmp  ecx, STAT_unused
  jne  end
  pop  eax                                  // Уничтожаем адрес возврата
  mov  eax, 0x4AF559
  push eax
end:
  xor  eax, eax
  dec  eax
  retn
 }
}

static char InvenFmt[20]="%d/%d  %d/%d";
static void __declspec(naked) display_stats_hook() {
 __asm {
  mov  ecx, ds:[_curr_stack]
  mov  eax, ds:[_stack][ecx*4]
  push eax
  jecxz itsCritter
  call item_c_max_size_
  pop  edx
  xchg edx, eax                             // eax=source, edx=макс. размер сумки
  push eax
  call item_c_curr_size_
  sub  edx, eax                             // edx=свободный размер сумки
  pop  eax
  push edx
  call obj_top_environment_
  push eax
  mov  edx, STAT_carry_amt
  call stat_level_                          // Макс. груз
  pop  edx
  xchg edx, eax                             // edx=Макс. груз, eax=source
  call item_total_weight_
  sub  edx, eax
  mov  eax, HiddenArmor
  call item_weight_
  sub  edx, eax                             // edx=свободный вес
  mov  edi, edx
  jmp  print
itsCritter:
  pop  edi
  call critter_max_size
  inc  eax                                  // Включён CritterInvSizeLimitMode?
  jnz  SizeLimitMode                        // Да
  mov  eax, edi                             // source
  mov  edx, STAT_carry_amt
  call stat_level_                          // Макс. груз
  push eax
  xchg edi, eax                             // eax=source, edi=Макс. груз
  call item_total_weight_
  xchg edx, eax                             // edx=вес вещей
  mov  eax, HiddenArmor
  call item_weight_
  add  edx, eax
  sub  edi, edx
print:
  push edx
  mov  edx, [esp+0x9C]
  push edx
  push 0x509EF4                             // "%s %d/%d"
  lea  eax, [esp+0x10]
  push eax
  call sprintf_
  add  esp, 5*4
  movzx ebx, byte ptr ds:[_GreenColor]
  cmp  edi, 0                               // Перегрузка по весу?
  jge  noOverloaded                         // Нет
  mov  bl, ds:[_RedColor]
noOverloaded:
  mov  edx, 0x472626
  jmp  edx
SizeLimitMode:
  dec  eax
  push eax                                  // макс.размер
  xchg ecx, eax                             // ecx = макс.размер
  mov  eax, edi                             // source
  call critter_curr_size
  xchg edx, eax                             // edx = размер вещей
  mov  eax, HiddenArmor
  call item_size_
  add  eax, edx
  push eax                                  // общий размер
  sub  ecx, eax                             // ecx = свободный размер
  mov  edx, STAT_carry_amt
  mov  eax, edi                             // source
  call stat_level_
  push eax                                  // Макс.груз
  xchg ebx, eax                             // ebx = Макс.груз
  mov  eax, edi                             // source
  call item_total_weight_
  xchg edx, eax                             // edx = вес вещей
  mov  eax, HiddenArmor
  call item_weight_
  add  eax, edx
  push eax                                  // общий вес
  sub  ebx, eax                             // ebx = свободный вес
  mov  eax, offset InvenFmt
  push eax
  mov  eax, offset MsgBuf
  push eax
  call sprintf_
  add  esp, 6*4
  movzx eax, byte ptr ds:[_GreenColor]
  cmp  ebx, 0                               // Перегрузка по весу?
  jl   Red                                  // Да
  cmp  ecx, 0                               // Перегрузка по размеру?
  jge  noRed                                // Нет
Red:
  mov  al, ds:[_RedColor]
noRed:
  push eax                                  // ColorIndex
  mov  edi, offset MsgBuf
  push edi
  xor  edx, edx
nextChar:
  xor  eax, eax
  mov  al, [edi]
  call dword ptr ds:[_text_char_width]
  inc  eax
  add  edx, eax
  inc  edi
  cmp  word ptr [edi-1], '  '
  jne  nextChar
  mov  ebx, 150                             // TxtWidth
  mov  ecx, 499                             // ToWidth
  lea  eax, [esi+ebp+75]                    // ToSurface
  sub  eax, edx
  pop  edx                                  // DisplayText
  mov  edi, 0x472677
  jmp  edi
 }
}

static void __declspec(naked) critterIsOverloaded_hook() {
 __asm {
  call item_total_weight_
  cmp  edx, eax                             // максимальный вес < общего веса?
  jl   end                                  // Да
  mov  eax, ebx                             // source
  call critter_max_size
  inc  eax
  jz   end
  dec  eax
  xchg edx, eax                             // edx = макс.размер
  mov  eax, ebx                             // source
  call item_c_curr_size_
end:
  retn
 }
}

static void __declspec(naked) item_add_mult_hook() {
 __asm {
  mov  edi, 0x4772A6
  mov  eax, ecx
  call critter_max_size
  inc  eax
  jz   end
  dec  eax
  cmp  Looting, 0
  je   skip
  sub  eax, SizeOnBody                      // Учитываем размер одетой на цели брони и оружия
skip:
  push eax                                  // макс.размер
  mov  eax, esi
  call item_size_
  xchg edx, eax
  imul edx, ebx                             // edx = размер вещи * количество
  mov  eax, ecx
  call critter_curr_size
  add  eax, edx
  pop  edx
  cmp  eax, edx                             // общий размер <= макс.размера?
  jle  end                                  // Да
  mov  edi, 0x4771C2                        // С вещами на выход
end:
  jmp  edi
 }
}

static void __declspec(naked) barter_attempt_transaction_peon() {
 __asm {
  call item_total_weight_
  cmp  eax, edx                             // добавляемый вес > свободного веса?
  jg   end                                  // Да
  mov  eax, edi                             // source
  call critter_max_size
  inc  eax
  jz   end
  dec  eax
  xchg edx, eax
  mov  eax, edi                             // source
  call critter_curr_size
  sub  edx, eax
  mov  eax, ecx                             // _barterer_table_obj
  call critter_curr_size
end:
  retn
 }
}

static void __declspec(naked) barter_attempt_transaction_barterer() {
 __asm {
  call item_total_weight_
  cmp  eax, edx                             // добавляемый вес > свободного веса?
  jg   end                                  // Да
  mov  eax, ebx                             // source
  call critter_max_size
  inc  eax
  jz   end
  dec  eax
  sub  eax, SizeOnBody                      // Учитываем размер одетой на цели брони и оружия
  xchg edx, eax
  mov  eax, ebx                             // source
  call critter_curr_size
  sub  edx, eax
  mov  eax, esi                             // _peon_table_obj
  call critter_curr_size
end:
  retn
 }
}

static void __declspec(naked) loot_container_hook() {
 __asm {
  call item_total_weight_
  cmp  eax, edx                             // добавляемый вес > свободного веса?
  jg   end                                  // Да
  mov  eax, [esp+0x134+4]                   // source
  call critter_max_size
  inc  eax
  jz   end
  dec  eax
  xchg edx, eax
  mov  eax, [esp+0x134+4]                   // source
  call critter_curr_size
  sub  edx, eax
  mov  eax, ebp
  call critter_curr_size
end:
  retn
 }
}

static void __declspec(naked) gdControlUpdateInfo_hook() {
 __asm {
  push eax                                  // _dialog_target
  call stat_level_
  xchg ecx, eax                             // ecx = Макс. груз
  pop  eax
  push eax
  call critter_max_size
  xchg edi, eax                             // edi = макс.размер
  pop  eax
  inc  edi
  jz   end
  dec  edi
  push edi                                  // макс.размер
  call item_c_curr_size_
  push eax                                  // общий размер
  push ecx                                  // Макс. груз
  push ebx                                  // Общий вес
  mov  eax, offset InvenFmt
  push eax
  lea  eax, [esp+0xC+12]
  push eax
  call sprintf_
  add  esp, 6*4
  pop  edx                                  // Уничтожаем адрес возврата
  mov  edx, 0x449151
  push edx
  xor  edx, edx
end:
  xchg ecx, eax
  retn
 }
}

static char ItemSizeMsg[32];
static void __declspec(naked) inven_obj_examine_func_hook() {
 __asm {
  xchg esi, eax
  call item_size_
  test eax, eax
  jz   end
  push eax                                  // Размер вещи
  mov  eax, offset ItemSizeMsg
  push eax
  mov  edx, offset MsgBuf
  push edx
  call sprintf_
  add  esp, 3*4
  xchg edx, eax
  call inven_display_msg_
end:
  xchg esi, eax
  jmp  text_font_
 }
}

static char SuperStimMsg[128];
static void __declspec(naked) protinst_use_item_on_hook() {
 __asm {
  mov  ebx, [ebx+0x64]                      // ebx = item pid
  cmp  ebx, PID_SUPER_STIMPAK
  jne  end
  mov  edx, [edi+0x64]                      // edx = target pid
  shr  edx, 0x18
  cmp  edx, ObjType_Critter
  jne  end
  mov  eax, edi                             // eax = target
  mov  edx, STAT_max_hit_points
  call stat_level_
  cmp  eax, [edi+0x58]                      // max_hp == curr_hp?
  jne  end                                  // Нет
  pop  eax                                  // Уничтожаем адрес возврата
  mov  eax, offset SuperStimMsg
  mov  esi, 0x49C478
  jmp  esi
end:
  xor  edx, edx
  retn
 }
}

static int invenapcost;
static char invenapqpreduction;
void _stdcall SetInvenApCost(int a) {invenapcost = a;}
static void __declspec(naked) handle_inventory_hook() {
 _asm {
  movzx ebx, byte ptr invenapqpreduction
  mul  bl
  mov  edx, invenapcost
  mov  ebx, 0x46E812
  jmp  ebx
 }
}

// adds check for weapons which require more than 1 ammo for single shot (super cattle prod & mega power fist), or burst
static void __declspec(naked) combat_check_bad_shot_hook() {
 __asm {
  push edx
  push ebx
  call item_w_cur_ammo_
  cmp  esi, ds:[_obj_dude]
  jne  end
  xchg ebx, eax                             // ebx = current ammo
  mov  eax, ecx                             // weapon
  mov  edx, edi                             // hit_mode
  call item_w_anim_weap_
  cmp  eax, ANIM_fire_burst
  je   itsBurst
  cmp  eax, ANIM_fire_continuous
  je   itsBurst
  sub  esp, 4
  xor  eax, eax
  inc  eax
  mov  [esp], eax
  mov  eax, ecx                             // weapon
  mov  edx, esp
  call item_w_compute_ammo_cost_
  mov  eax, [esp]
  add  esp, 4
  jmp  skip
itsBurst:
  mov  eax, ecx                             // weapon
  call item_w_rounds_
skip:
  xchg edx, eax
  xor  eax, eax
  cmp  edx, ebx
  ja   end                                  // this will force "Out of ammo"
  inc  eax                                  // this will force success
end:
  pop  ebx
  pop  edx
  retn
 }
}

static void __declspec(naked) ReloadActiveHand() {
 __asm {
// esi=-1 если не перезарядили неактивную руку или смещение неактивной руки
  push ebx
  push ecx
  push edx
  mov  eax, ds:[_itemCurrentItem]
  imul ebx, eax, 24
  mov  eax, ds:[_obj_dude]
  xor  ecx, ecx
reloadItem:
  push eax
  mov  edx, ds:[_itemButtonItems][ebx]
  call item_w_try_reload_
  test eax, eax
  pop  eax
  jnz  endReloadItem
  inc  ecx
  jmp  reloadItem
endReloadItem:
  cmp  dword ptr ds:[_itemButtonItems + 0x10][ebx], 5// mode
  jne  skip_toggle_item_state
  call intface_toggle_item_state_
skip_toggle_item_state:
  test ecx, ecx
  jnz  useActiveHand
  xchg esi, ebx
useActiveHand:
  push ebx
  xor  esi, esi
  dec  esi
  mov  ebx, esi
  xor  eax, eax
  mov  edx, ebx
  call intface_update_items_
  pop  eax
  cmp  eax, esi
  je   end
  mov  ebx, 2
  xor  ecx, ecx
  mov  edx, ds:[_itemButtonItems][eax]
  mov  eax, 0x460B85
  jmp  eax
end:
  pop  edx
  pop  ecx
  pop  ebx
  retn
 }
}

static void __declspec(naked) ReloadWeaponHotKey() {
 __asm {
  call gmouse_is_scrolling_
  test eax, eax
  jnz  end
  pushad
  xchg ebx, eax
  push ReloadWeaponKey
  call KeyDown
  test eax, eax
  jnz  ourKey
  popad
  retn
ourKey:
  cmp  dword ptr ds:[_intfaceEnabled], ebx
  je   endReload
  xor  esi, esi
  dec  esi
  cmp  dword ptr ds:[_interfaceWindow], esi
  je   endReload
  mov  edx, ds:[_itemCurrentItem]
  imul eax, edx, 24
  cmp  byte ptr ds:[_itemButtonItems + 0x5][eax], bl// itsWeapon
  jne  itsWeapon                            // Да
  call intface_use_item_
  jmp  endReload
itsWeapon:
  test byte ptr ds:[_combat_state], 1       // В бою?
  jnz  inCombat                             // Да
  call ReloadActiveHand
  jmp  endReload
inCombat:
//  xor  ebx, ebx                             // is_secondary
  add  edx, hit_left_weapon_reload          // edx = 6/7 - перезарядка оружия в левой/правой руке
  mov  eax, ds:[_obj_dude]
  push eax
  call item_mp_cost_
  xchg ecx, eax
  pop  eax                                  // _obj_dude
  mov  edx, [eax+0x40]                      // movePoints
  cmp  ecx, edx
  jg   endReload
  push eax
  call ReloadActiveHand
  test eax, eax
  pop  eax                                  // _obj_dude
  jnz  endReload
  sub  edx, ecx
  mov  [eax+0x40], edx                      // movePoints
  xchg edx, eax
  mov  edx, ds:[_combat_free_move]
  call intface_update_move_points_
endReload:
  popad
  inc  eax
end:
  retn
 }
}

static void __declspec(naked) AutoReloadWeapon() {
 __asm {
  call scr_exec_map_update_scripts_
  pushad
  cmp  dword ptr ds:[_game_user_wants_to_quit], 0
  jnz  end
  mov  eax, ds:[_obj_dude]
  call critter_is_dead_                     // Дополнительная проверка не помешает
  test eax, eax
  jnz  end
  cmp  dword ptr ds:[_intfaceEnabled], eax
  je   end
  xor  esi, esi
  dec  esi
  cmp  dword ptr ds:[_interfaceWindow], esi
  je   end
  inc  eax
  mov  ebx, ds:[_itemCurrentItem]
  push ebx
  sub  eax, ebx
  mov  ds:[_itemCurrentItem], eax           // Устанавливаем неактивную руку
  imul ebx, eax, 24
  mov  eax, ds:[_obj_dude]
  xor  ecx, ecx
reloadOffhand:
  push eax
  mov  edx, ds:[_itemButtonItems][ebx]
  call item_w_try_reload_
  test eax, eax
  pop  eax
  jnz  endReloadOffhand
  inc  ecx
  jmp  reloadOffhand
endReloadOffhand:
  cmp  dword ptr ds:[_itemButtonItems + 0x10][ebx], 5// mode
  jne  skip_toggle_item_state
  call intface_toggle_item_state_
skip_toggle_item_state:
  test ecx, ecx
  jnz  useOffhand
  xchg ebx, esi
useOffhand:
  xchg esi, ebx                             // esi=-1 если не перезарядили или смещению неактивной руки
  pop  eax
  mov  ds:[_itemCurrentItem], eax           // Восстанавливаем активную руку
  call ReloadActiveHand
end:
  popad
  retn
 }
}

static int pobj;
static int _stdcall ChangeArmorFid(DWORD* item, DWORD* npc) {
 int itm_pid = 0, npc_pid = PID_Player;
 if (item) itm_pid=item[0x64/4]&0xffffff;
 if (npc) npc_pid=npc[0x64/4];
 pobj=0;
 for (int k=0; k<npcCount; k++) {
  if (armors[k].Pid==npc_pid) {
   pobj=armors[k].Default;
   switch (itm_pid) {
    case PID_LEATHER_ARMOR: case PID_LEATHER_ARMOR_MK_II:
     if (armors[k].Leather!=0) pobj=armors[k].Leather;
     break;
    case PID_POWERED_ARMOR: case PID_HARDENED_POWER_ARMOR:
     if (armors[k].Power!=0) pobj=armors[k].Power;
     break;
    case PID_ADVANCED_POWER_ARMOR: case PID_ADVANCED_POWER_ARMOR_MK2:
     if (armors[k].Advanced!=0) pobj=armors[k].Advanced;
     break;
    case PID_METAL_ARMOR: case PID_TESLA_ARMOR: case PID_METAL_ARMOR_MK_II:
     if (armors[k].Metal!=0) pobj=armors[k].Metal;
     break;
    case PID_LEATHER_JACKET: case PID_CURED_LEATHER_ARMOR:
     if (armors[k].Cured!=0) pobj=armors[k].Cured;
     break;
    case PID_COMBAT_ARMOR: case PID_BROTHERHOOD_COMBAT_ARMOR: case PID_COMBAT_ARMOR_MK_II:
     if (armors[k].Combat!=0) pobj=armors[k].Combat;
     break;
    case PID_PURPLE_ROBE: case PID_BRIDGEKEEPERS_ROBE:
     if (armors[k].Robe!=0) pobj=armors[k].Robe;
     break;
   }
   __asm {
    mov  ebx, npc
    mov  eax, ebx
    call inven_right_hand_
    push eax                                // Сохраним указатель на оружие, если оно конечно есть
    test eax, eax
    jz   noWeapon                           // Оружия в руках нет
    mov  eax, ebx
    xor  edx, edx
    inc  edx                                // правая рука (INVEN_TYPE_RIGHT_HAND)
    call inven_unwield_                     // Разоружимся
noWeapon:
    push pobj                               // pobj = fid брони
    mov  edx, offset pobj
    mov  eax, npc_pid
    call proto_ptr_
    mov  edx, pobj
    mov  eax, [edx+0x8]
    mov  pobj, eax                          // pobj = fid из pro-файла
    pop  edx
    test edx, edx                           // А был ли мальчик?
    jnz  haveFid                            // Да
    mov  edx, eax                           // Нет, используем fid из pro-файла
haveFid:
    and  edx, 0x0FFF                        // edx = индекс в LST-файле
    mov  ebx, [ebx+0x20]                    // fid
    mov  eax, ebx
    and  eax, 0x70000000
    sar  eax, 0x1C
    push eax                                // ID3 (Direction code)
    and  ebx, 0x0FF0000
    sar  ebx, 0x10                          // ebx = ID2 (Animation code)
    xor  ecx, ecx                           // ecx = ID1 (Weapon code = None)
    mov  eax, ObjType_Critter               // eax = тип объекта
    call art_id_
    mov  edx, eax
    call art_exists_
    test eax, eax
    jnz  canUse                             // Такое изображение можно использовать
    mov  edx, pobj                          // Используем изображение из pro-файла
canUse:
    mov  pobj, edx                          // Сохраним fid для выхода
    xor  ebx, ebx
    mov  ecx, npc
    mov  eax, ecx
    call obj_change_fid_                    // Меняем вид
    pop  edx                                // Восстановим указатель на оружие, если оно конечно есть
    test edx, edx                           // Оружие было в руках?
    jz   end                                // Нет
    xor  ebx, ebx
    inc  ebx                                // правая рука (INVEN_TYPE_RIGHT_HAND)
    mov  eax, ecx                           // *npc
    call inven_wield_                       // Одеваем оружие
    mov  edx, [ecx+0x20]                    // fid
    mov  pobj, edx                          // Сохраним fid уже с оружием для correctFidForRemovedItem_
end:
   }
   break;
  }
 }
 return pobj;                               // Возвращаем fid
}

static void __declspec(naked) correctFidForRemovedItem_hook() {
 __asm {
  call adjust_ac_
nextArmor:
  mov  eax, esi
  call inven_worn_
  test eax, eax
  jz   noArmor
  and  byte ptr [eax+0x27], 0xFB            // Сбрасываем флаг одетой брони
  jmp  nextArmor
noArmor:
  cmp  edi, -1                              // Это игрок?
  jnz  end                                  // Да
  cmp  npcCount, 0
  je   end
  push esi                                  // указатель на нпс
  push eax                                  // новой брони нет
  call ChangeArmorFid
  test eax, eax
  jz   end
  mov  edi, eax
end:
  retn
 }
}

static void __declspec(naked) ControlWeapon_hook() {
 __asm {
  mov  edx, ds:[_dialog_target]
  mov  eax, edx
  call inven_right_hand_
  test eax, eax
  mov  eax, edx                             // _dialog_target
  jnz  haveWeapon
  mov  edx, 0x4494FA
  jmp  edx
haveWeapon:
  xor  edx, edx
  inc  edx                                  // правая рука (INVEN_TYPE_RIGHT_HAND)
  call inven_unwield_
  mov  edx, 0x44952E
  jmp  edx
 }
}

static void __declspec(naked) ControlArmor_hook() {
 __asm {
  mov  ecx, eax                             // _dialog_target
  call inven_worn_
  test eax, eax
  jnz  haveArmor
  xchg ecx, eax                             // _dialog_target
  jmp  ai_search_inven_armor_
haveArmor:
  mov  ebx, 0x4000000                       // Worn
  xchg edx, eax                             // edx = указатель на снимаемую броню
  xchg ecx, eax                             // eax = _dialog_target
  call correctFidForRemovedItem_
  xor  eax, eax
  retn
 }
}

static void __declspec(naked) fontHeight() {
 __asm {
  push ebx
  mov  ebx, ds:[_curr_font_num]
  mov  eax, 101
  call text_font_
  call dword ptr ds:[_text_height]
  xchg ebx, eax
  call text_font_
  xchg ebx, eax
  pop  ebx
  retn
 }
}

static void __declspec(naked) printFreeMaxWeightSize() {
 __asm {
// ebx = source, ecx = ToWidth, edi = posOffset, esi = extraWeight, ebp = extraSize
  mov  eax, ds:[_curr_font_num]
  push eax
  mov  eax, 101
  call text_font_
  mov  eax, ds:[_i_wid]
  call win_get_buf_                         // eax=ToSurface
  add  edi, eax                             // ToSurface+posOffset (Ypos*ToWidth+Xpos)
  mov  eax, [ebx+0x20]
  and  eax, 0xF000000
  sar  eax, 0x18
  test eax, eax                             // Это ObjType_Item?
  jz   itsItem                              // Да
  cmp  eax, ObjType_Critter
  jne  noWeight                             // Нет
  mov  eax, ebx
  mov  edx, STAT_carry_amt
  call stat_level_
  xchg ebx, eax                             // ebx = макс. вес груза, eax = source
  sub  ebx, esi                             // ebx = макс. вес груза - extraWeight
  push eax
  call critter_max_size
  xchg edx, eax                             // edx = макс. размер груза
  pop  eax
  inc  edx
  jnz  WeightAndSize
  call item_total_weight_                   // eax = общий вес груза
  jmp  printFree
WeightAndSize:
  dec  edx
  sub  edx, ebp                             // edx = макс. размер груза - extraSize
  push eax
inContainer:
  call critter_curr_size
  sub  edx, eax                             // edx = свободный размер
  pop  eax
  push edx
  call item_total_weight_
  sub  ebx, eax                             // eax = свободный вес
  push ebx
  push 0x503180                             // '%d/%d'
  mov  esi, offset MsgBuf
  push esi
  call sprintf_
  add  esp, 4*4
  movzx eax, byte ptr ds:[_GreenColor]
  cmp  ebx, 0                               // Перегрузка по весу?
  jl   Red                                  // Да
  cmp  edx, 0                               // Перегрузка по размеру?
  jge  noRed                                // Нет
Red:
  mov  al, ds:[_RedColor]
noRed:
  push eax
  push esi
  xor  edx, edx
nextChar:
  movzx eax, byte ptr [esi]
  call ds:[_text_char_width]
  inc  eax
  add  edx, eax
  inc  esi
  cmp  byte ptr [esi-1], '/'
  jne  nextChar
  sub  edi, edx
  pop  edx                                  // DisplayText
  jmp  print
itsItem:
  mov  eax, ebx
  call item_get_type_
  cmp  eax, item_type_container
  jne  noWeight                             // Нет
  mov  eax, ebx
  call obj_top_environment_
  test eax, eax                             // Есть владелец?
  jz   noOwner                              // Нет
  mov  edx, [eax+0x20]
  and  edx, 0xF000000
  sar  edx, 0x18
  cmp  edx, ObjType_Critter                 // Это персонаж?
  jne  noOwner                              // Нет
  push eax
  mov  edx, STAT_carry_amt
  call stat_level_
  xchg ebx, eax                             // ebx = макс. вес груза у персонажа, eax = source
  sub  ebx, esi                             // ebx = макс. вес груза - extraWeight
  push eax
  call item_c_max_size_
  xchg edx, eax                             // edx = макс. размер груза сумки
  pop  eax
  jmp  inContainer
noOwner:
  mov  eax, ebx
  call item_c_max_size_                     // eax = макс. размер груза
  xchg ebx, eax                             // ebx = макс. размер груза, eax = source
  call item_c_curr_size_
printFree:
  sub  ebx, eax                             // ebx = свободный вес/размер
  push ebx
  push 0x503190                             // '%d'
  mov  edx, offset MsgBuf
  push edx
  call sprintf_
  add  esp, 3*4
  movzx eax, byte ptr ds:[_GreenColor]
  cmp  ebx, 0                               // Перегрузка по весу?
  jge  skipColor                            // Нет
  mov  al, ds:[_RedColor]
skipColor:
  push eax
  mov  eax, edx
  call ds:[_text_width]
  shr  eax, 1
  inc  eax
  sub  edi, eax
print:
  xchg edi, eax                             // ToSurface+posOffset (Ypos*ToWidth+Xpos)
  mov  ebx, 64                              // TxtWidth
  call ds:[_text_to_buf]
noWeight:
  pop  eax
  jmp  text_font_
 }
}

static void __declspec(naked) display_inventory_hook() {
 __asm {
  call fontHeight
  inc  eax
  add  [esp+0x8], eax                       // height = height + text_height + 1
  call buf_to_buf_
  add  esp, 0x18
  mov  eax, [esp+0x4]
  call art_ptr_unlock_
  pushad
  mov  eax, ds:[_curr_stack]
  mov  ebx, ds:[_stack][eax*4]
  mov  ecx, 537
  mov  edi, 325*537+180+32                  // Xpos=180, Ypos=325, max text width/2=32
  xor  esi, esi
  xor  ebp, ebp
  call printFreeMaxWeightSize
  popad
  mov  ecx, 0x4700C5
  jmp  ecx
 }
}

static void __declspec(naked) display_target_inventory_hook() {
 __asm {
  call fontHeight
  inc  eax
  add  [esp+0x8], eax                       // height = height + text_height + 1
  call buf_to_buf_
  add  esp, 0x18
  mov  eax, [esp]
  call art_ptr_unlock_
  pushad
  mov  eax, ds:[_target_curr_stack]
  mov  ebx, ds:[_target_stack][eax*4]
  mov  ecx, 537
  mov  edi, 325*537+301+32                  // Xpos=301, Ypos=325, max text width/2=32
  mov  esi, WeightOnBody                    // Учитываем вес одетой на цели брони и оружия
  mov  ebp, SizeOnBody                      // Учитываем размер одетой на цели брони и оружия
  call printFreeMaxWeightSize
  popad
  mov  eax, 0x470468
  jmp  eax
 }
}

static void __declspec(naked) display_table_inventories_hook() {
 __asm {
  call win_get_buf_
  mov  edi, ds:[_btable]
  mov  [esp+0x8C+4], edi
  mov  edi, ds:[_ptable]
  mov  [esp+0x94+4], edi
  retn
 }
}

static void __declspec(naked) display_table_inventories_hook1() {
 __asm {
  add  dword ptr [esp+8], 20
  sub  dword ptr [esp+16], 20*480
  call buf_to_buf_
  add  esp, 0x18
  pushad
  mov  eax, ds:[_btable]
  push eax
  call item_total_weight_                   // eax = вес вещей цели в окне бартера
  xchg esi, eax
  pop  eax
  call critter_curr_size                    // eax = размер вещей цели в окне бартера
  xchg ebp, eax
  mov  eax, ds:[_curr_stack]
  mov  ebx, ds:[_stack][eax*4]
  mov  ecx, 480
  mov  edi, 10*480+169+32                   // Xpos=169, Ypos=10, max text width/2=32
  call printFreeMaxWeightSize
  popad
  mov  eax, 0x47548C
  jmp  eax
 }
}

// Рисуем участок окна
static void __declspec(naked) display_table_inventories_hook2() {
 __asm {
  mov  dword ptr [edx+4], 4                 // WinRect.y_start = 4
  jmp  win_draw_rect_
 }
}

static void __declspec(naked) display_table_inventories_hook3() {
 __asm {
  add  dword ptr [esp+8], 20
  sub  dword ptr [esp+16], 20*480
  call buf_to_buf_
  add  esp, 0x18
#ifndef TRACE
  cmp  dword ptr ds:[_dialog_target_is_party], 0
  je   end                                  // это торговля
#endif
  pushad
  mov  eax, ds:[_ptable]
  push eax
  call item_total_weight_                   // eax = вес вещей игрока в окне бартера
  xchg esi, eax
  add  esi, WeightOnBody                    // Учитываем вес одетой на цели брони и оружия
  pop  eax
  call critter_curr_size
  add  eax, SizeOnBody                      // Учитываем размер одетой на цели брони и оружия
  xchg ebp, eax
  mov  eax, ds:[_target_curr_stack]
  mov  ebx, ds:[_target_stack][eax*4]
  mov  ecx, 480
  mov  edi, 10*480+254+32                   // Xpos=254, Ypos=10, max text width/2=32
  call printFreeMaxWeightSize
  popad
#ifndef TRACE
end:
#endif
  mov  edx, 0x475612
  jmp  edx
 }
}

static void __declspec(naked) barter_inventory_hook() {
 __asm {
  call container_exit_
  xor  ecx, ecx
  dec  ecx
  mov  ebx, [esp+0x30+0x4]                  // target
  mov  eax, edi                             // owner
  mov  edx, ebp                             // source
  jmp  display_table_inventories_
 }
}

static void __declspec(naked) gdProcess_hook() {
 __asm {
  mov  eax, ds:[_dialog_target]
  cmp  dword ptr [eax+0x64], PID_Marcus
  jz   noArmor
  mov  edx, eax
  call ai_search_inven_armor_
  test eax, eax
  jz   noArmor
  xor  ebx, ebx
  xchg edx, eax                             // edx=указатель на объект (броню), eax=_dialog_target
  call inven_wield_
noArmor:
  jmp  gdialog_barter_cleanup_tables_
 }
}

static void __declspec(naked) invenWieldFunc_hook() {
 __asm {
  call adjust_ac_
  push esi                                  // указатель на нпс
  push edi                                  // указатель на объект (броню)
  call ChangeArmorFid
  retn
 }
}

static void __declspec(naked) SetDefaultAmmo() {
 __asm {
  push eax
  push ebx
  push edx
  xchg edx, eax
  mov  ebx, eax
  call item_get_type_
  cmp  eax, item_type_weapon
  jne  end                                  // Нет
  cmp  dword ptr [ebx+0x3C], 0              // Есть патроны в оружии?
  jne  end                                  // Да
  sub  esp, 4
  mov  edx, esp
  mov  eax, [ebx+0x64]                      // eax = pid оружия
  call proto_ptr_
  mov  edx, [esp]
  mov  eax, [edx+0x5C]                      // eax = идентификатор прототипа патронов по умолчанию
  mov  [ebx+0x40], eax                      // прототип используемых патронов
  add  esp, 4
end:
  pop  edx
  pop  ebx
  pop  eax
  retn
 }
}

static void __declspec(naked) inven_action_cursor_hook() {
 __asm {
  mov  edx, [esp+0x1C]
  call SetDefaultAmmo
  cmp  dword ptr [esp+0x18], 0
  mov  eax, 0x4736CB
  jmp  eax
 }
}

static void __declspec(naked) item_add_force_call() {
 __asm {
  call SetDefaultAmmo
  jmp  item_add_force_
 }
}

static void __declspec(naked) inven_pickup_hook2() {
 __asm {
  mov  eax, ds:[_i_wid]
  call GNW_find_
  mov  ebx, [eax+0x8+0x0]                   // ebx = _i_wid.rect.x
  mov  ecx, [eax+0x8+0x4]                   // ecx = _i_wid.rect.y
  mov  eax, 176
  add  eax, ebx                             // x_start
  add  ebx, 176+60                          // x_end
  mov  edx, 37
  add  edx, ecx                             // y_start
  add  ecx, 37+100                          // y_end
  call mouse_click_in_
  test eax, eax
  jz   end
  mov  edx, ds:[_curr_stack]
  test edx, edx
  jnz  end
  cmp  edi, 1006                            // Руки?
  jae  skip                                 // Да
  mov  edx, [esp+0x18+0x4]                  // item
  mov  eax, edx
  call item_get_type_
  cmp  eax, item_type_drug
  jne  skip
  mov  eax, ds:[_stack]
  push eax
  push edx
  call useobjon_item_d_take_drug_           // item_d_take_drug_
  pop  edx
  pop  ebx
  cmp  eax, 1
  jne  notUsed
  xchg ebx, eax
  push edx
  push eax
  call item_remove_mult_
  pop  eax
  xor  ecx, ecx
  mov  ebx, [eax+0x28]
  mov  edx, [eax+0x4]
  pop  eax
  push eax
  call obj_connect_
  pop  eax
  call obj_destroy_
notUsed:
  mov  eax, 1
  call intface_update_hit_points_
skip:
  xor  eax, eax
end:
  retn
 }
}

static void __declspec(naked) make_drop_button() {
 __asm {
  mov  ebx, [esp+0x28+0x4]
  mov  eax, [ebx+0x20]
  and  eax, 0xF000000
  sar  eax, 0x18
  test eax, eax                             // Это ObjType_Item?
  jnz  skip                                 // Нет
  xchg ebx, eax
  call item_get_type_
  cmp  eax, item_type_container
  je   goodTarget                           // Да
  jmp  noButton
skip:
  cmp  eax, ObjType_Critter
  jne  noButton                             // Нет
  xchg ebx, eax
  call critter_body_type_
  test eax, eax                             // Это Body_Type_Biped?
  jnz  noButton                             // Нет
goodTarget:
  push ebp
  mov  edx, 255                             // DROPN.FRM (Action menu drop normal)
  mov  eax, ObjType_Intrface
  xor  ecx, ecx
  xor  ebx, ebx
  call art_id_
  mov  ecx, 0x59E7E4
  xor  ebx, ebx
  xor  edx, edx
  call art_ptr_lock_data_
  test eax, eax
  jz   noButton
  xchg esi, eax
  push ebp
  mov  edx, 254                             // DROPH.FRM (Action menu drop highlighted )
  mov  eax, ObjType_Intrface
  xor  ecx, ecx
  xor  ebx, ebx
  call art_id_
  mov  ecx, 0x59E7E8
  xor  ebx, ebx
  xor  edx, edx
  call art_ptr_lock_data_
  test eax, eax
  jz   noButton
  push ebp                                  // ButType
  push ebp
  push eax                                  // PicDown
  push esi                                  // PicUp
  xor  ecx, ecx
  dec  ecx
  push ecx                                  // ButtUp
  mov  edx, 68                              // Xpos
  push edx                                  // ButtDown
  push ecx                                  // HovOff
  push ecx                                  // HovOn
  mov  ecx, 40                              // Width
  push ecx                                  // Height
  mov  ebx, 204                             // Ypos
  mov  eax, ds:[_i_wid]                     // WinRef
  call win_register_button_
noButton:
  mov  edx, 436
  retn
 }
}

static char OverloadedDrop[48];
static void __declspec(naked) drop_all() {
 __asm {
  cmp  esi, 'a'
  jne  skip
  mov  ebx, 0x4740DB
  jmp  ebx
skip:
  cmp  esi, 'D'
  je   dropKey
  cmp  esi, 'd'
  je   dropKey
  mov  ebx, 0x4741B2
  jmp  ebx
dropKey:
  pushad
  cmp  dword ptr ds:[_gIsSteal], 0
  jne  end
  mov  esi, ebp
  mov  ecx, [esp+0x134+0x20]
  mov  eax, ecx
  call critter_body_type_
  test eax, eax                             // Это Body_Type_Biped?
  jnz  end                                  // Нет
  mov  eax, [esi+0x20]
  and  eax, 0xF000000
  sar  eax, 0x18
  test eax, eax                             // Это ObjType_Item?
  jz   itsItem                              // Да
  cmp  eax, ObjType_Critter
  jne  end                                  // Нет
  mov  eax, esi
  call critter_body_type_
  test eax, eax                             // Это Body_Type_Biped?
  jnz  end                                  // Нет
itsCritter:
  mov  edx, STAT_carry_amt
  mov  eax, esi
  call stat_level_
  xchg edx, eax                             // edx = макс. вес груза цели
  sub  edx, WeightOnBody                    // Учитываем вес одетой на цели брони и оружия
  mov  eax, esi
  call item_total_weight_                   // eax = общий вес груза цели
  sub  edx, eax
  mov  eax, ecx
  call item_total_weight_
  xchg edi, eax                             // edi = общий вес груза игрока
  mov  eax, ds:[_i_rhand]
  call item_weight_
  sub  edi, eax
  mov  eax, ds:[_i_lhand]
  call item_weight_
  sub  edi, eax
  mov  eax, ds:[_i_worn]
  call item_weight_
  sub  edi, eax
  xchg edi, eax                             // eax = общий вес груза игрока
  cmp  eax, edx
  jg   compareSizeWeight
  mov  eax, esi
  call critter_max_size
  inc  eax
  jz   cont
  dec  eax
  xchg edx, eax
  sub  edx, SizeOnBody                      // Учитываем размер одетой на цели брони и оружия
  mov  eax, esi                             // source
  call critter_curr_size
  sub  edx, eax
  mov  eax, ecx
  call item_c_curr_size_
  jmp  compareSizeWeight
itsItem:
  mov  eax, esi
  call item_get_type_
  cmp  eax, item_type_container             // Это сумка/рюкзак?
  jne  end                                  // Нет
  mov  eax, esi
  call obj_top_environment_
  test eax, eax                             // Есть владелец?
  jz   noOwner                              // Нет
  xchg esi, eax                             // esi = владелец сумки
  mov  eax, [esi+0x20]
  and  eax, 0xF000000
  sar  eax, 0x18
  cmp  eax, ObjType_Critter                 // Это персонаж?
  je   itsCritter                           // Нет
noOwner:
  mov  eax, esi
  call item_c_max_size_
  xchg edx, eax
  mov  eax, esi
  call item_c_curr_size_
  sub  edx, eax
  mov  eax, ecx
  call item_c_curr_size_
compareSizeWeight:
  cmp  eax, edx
  jg   cantDrop
cont:
  mov  eax, 0x503E14                        // 'ib1p1xx1'
  call gsound_play_sfx_file_
  mov  edx, ebp
  mov  eax, ecx
  call item_move_all_
  xor  eax, eax
  mov  ds:[_stack_offset], eax
  mov  ds:[_target_stack_offset], eax
  inc  eax
  mov  ds:[_curr_stack], eax
  inc  eax
  push eax                                  // mode
  xchg ecx, eax                             // ecx = mode
  mov  eax, ds:[_target_curr_stack]
  mov  eax, ds:[_target_stack_offset][eax*4]// inventory_offset
  xor  edx, edx
  dec  edx                                  // visible_offset
  mov  ebx, ds:[_target_pud]                // target_inventory
  call display_target_inventory_
  pop  edx                                  // 2 (mode)
  mov  eax, 2500
  call container_exit_
  jmp  end
cantDrop:
  mov  eax, 0x503E14                        // 'ib1p1xx1'
  call gsound_play_sfx_file_
  xor  eax, eax
  push eax
  mov  al, ds:[0x6AB718]                    // color
  push eax
  xor  ebx, ebx
  push ebx
  push eax
  mov  ecx, 169
  push 117
  xor  edx, edx
  lea  eax, OverloadedDrop
  call dialog_out_
end:
  popad
  mov  ebx, 0x474435
  jmp  ebx
 }
}

static void __declspec(naked) loot_container_hook1() {
 __asm {
  xor  eax, eax
  mov  ds:[_stack_offset], eax
  mov  ds:[_target_stack_offset], eax
  inc  eax
  mov  ds:[_target_curr_stack], eax
  mov  eax, 2501
  mov  edx, ecx                             // edx = mode
  jmp  container_exit_
 }
}

static void __declspec(naked) loot_container_hook2() {
 __asm {
  cmp  esi, 0x150                           // source_down
  je   scroll
  cmp  esi, 0x148                           // source_up
  jne  end
scroll:
  push edx
  push ecx
  push ebx
  mov  eax, ds:[_i_wid]
  call GNW_find_
  mov  ebx, [eax+0x8+0x0]                   // ebx = _i_wid.rect.x
  mov  ecx, [eax+0x8+0x4]                   // ecx = _i_wid.rect.y
  mov  eax, 297
  add  eax, ebx                             // x_start
  add  ebx, 297+64                          // x_end
  mov  edx, 37
  add  edx, ecx                             // y_start
  add  ecx, 37+6*48                         // y_end
  call mouse_click_in_
  pop  ebx
  pop  ecx
  pop  edx
  test eax, eax
  jz   end
  cmp  esi, 0x150                           // source_down
  je   targetDown
  mov  esi, 0x18D                           // target_up
  jmp  end
targetDown:
  mov  esi, 0x191                           // target_down
end:
  mov  eax, ds:[_curr_stack]
  retn
 }
}

static void __declspec(naked) barter_inventory_hook1() {
 __asm {
  push edx
  push ecx
  push ebx
  xchg esi, eax
  cmp  esi, 0x150                           // source_down
  je   scroll
  cmp  esi, 0x148                           // source_up
  jne  end
scroll:
  mov  eax, ds:[_i_wid]
  call GNW_find_
  mov  ebx, [eax+0x8+0x0]                   // ebx = _i_wid.rect.x
  mov  ecx, [eax+0x8+0x4]                   // ecx = _i_wid.rect.y
  push ebx
  push ecx
  mov  eax, 395
  add  eax, ebx                             // x_start
  add  ebx, 395+64                          // x_end
  mov  edx, 35
  add  edx, ecx                             // y_start
  add  ecx, 35+3*48                         // y_end
  call mouse_click_in_
  pop  ecx
  pop  ebx
  test eax, eax
  jz   notTargetScroll
  cmp  esi, 0x150                           // source_down
  je   targetDown
  mov  esi, 0x18D                           // target_up
  jmp  end
targetDown:
  mov  esi, 0x191                           // target_down
  jmp  end
notTargetScroll:
  push ebx
  push ecx
  mov  eax, 250
  add  eax, ebx                             // x_start
  add  ebx, 250+64                          // x_end
  mov  edx, 20
  add  edx, ecx                             // y_start
  add  ecx, 20+3*48                         // y_end
  call mouse_click_in_
  pop  ecx
  pop  ebx
  test eax, eax
  jz   notTargetBarter
  cmp  esi, 0x150                           // source_down
  je   barterTargetDown
  mov  esi, 0x184                           // target_barter_up
  jmp  end
barterTargetDown:
  mov  esi, 0x176                           // target_barter_down
  jmp  end
notTargetBarter:
  mov  eax, 165
  add  eax, ebx                             // x_start
  add  ebx, 165+64                          // x_end
  mov  edx, 20
  add  edx, ecx                             // y_start
  add  ecx, 20+3*48                         // y_end
  call mouse_click_in_
  test eax, eax
  jz   end
  cmp  esi, 0x150                           // source_down
  je   barterSourceDown
  mov  esi, 0x149                           // source_barter_up
  jmp  end
barterSourceDown:
  mov  esi, 0x151                           // source_barter_down
end:
  pop  ebx
  pop  ecx
  pop  edx
  mov  eax, esi
  cmp  eax, 0x11
  retn
 }
}

static void __declspec(naked) handle_inventory_hook1() {
 __asm {
  call container_exit_
  jmp  display_stats_
 }
}

void InventoryInit() {
 // Теперь STAT_unused при включённом InvSizeLimitMode является STAT_size для заданных персонажей
 MakeCall(0x4AF0CB, &stat_level_hook, true);
 // Чтобы можно было менять STAT_unused не только у игрока
 MakeCall(0x455D65, &op_set_critter_stat_hook, false);
 // Чтобы можно было менять STAT_unused
 MakeCall(0x4AF54E, &stat_set_base_hook, false);

 // Показывать не только вес вещей/общий вес игрока, но и вес в открытой вложенной сумке
 MakeCall(0x4725C8, &display_stats_hook, true);

 InvSizeLimitMode = GetPrivateProfileInt("Misc", "CritterInvSizeLimitMode", 0, ini);
 if (InvSizeLimitMode >= 1 && InvSizeLimitMode <= 7) {
  if (InvSizeLimitMode >= 4) {
   InvSizeLimitMode -= 4;
   SafeWrite8(0x477EB3, 0xEB);
  }
  if (InvSizeLimitMode) {
   InvSizeLimit = GetPrivateProfileInt("Misc", "CritterInvSizeLimit", 50, ini);

   HookCall(0x42E67E, &critterIsOverloaded_hook);

   //Check item_add_mult_ (picking stuff from the floor, etc.)
   HookCall(0x4771BD, &item_add_mult_hook);

   //Check capacity of player and barteree when bartering
   HookCall(0x474C73, &barter_attempt_transaction_peon);
   HookCall(0x474CCA, &barter_attempt_transaction_barterer);

   // Кнопка "Take all"
   HookCall(0x47410B, &loot_container_hook);

   // Показывать в окне настроек сопартийца в поле "Несёт" размеры
   HookCall(0x449136, &gdControlUpdateInfo_hook);

   //Display item weight when examening
   GetPrivateProfileString("sfall", "ItemSizeMsg", "It occupies %d unit(s).", ItemSizeMsg, 32, translationIni);
   HookCall(0x473007, &inven_obj_examine_func_hook);
  }
 } else InvSizeLimitMode = 0;

 if (GetPrivateProfileInt("Misc", "SuperStimExploitFix", 0, ini)) {
  GetPrivateProfileString("sfall", "SuperStimExploitMsg", "You cannot use a super stim on someone who is not injured!", SuperStimMsg, 128, translationIni);
  MakeCall(0x49C3DE, &protinst_use_item_on_hook, false);
 }

 invenapcost = GetPrivateProfileInt("Misc", "InventoryApCost", 4, ini);
 invenapqpreduction = GetPrivateProfileInt("Misc", "QuickPocketsApCostReduction", 2, ini);
 MakeCall(0x46E80B, &handle_inventory_hook, true);

 if (GetPrivateProfileInt("Misc", "CheckWeaponAmmoCost", 0, ini)) {
  HookCall(0x4266E9, &combat_check_bad_shot_hook);
 }

 ReloadWeaponKey = GetPrivateProfileIntA("Input", "ReloadWeaponKey", 0, ini);
 if (ReloadWeaponKey) HookCall(0x442DFF, &ReloadWeaponHotKey);

 if (GetPrivateProfileInt("Misc", "AutoReloadWeapon", 0, ini)) {
  HookCall(0x422E8A, &AutoReloadWeapon);
 }

 HookCall(0x45419B, &correctFidForRemovedItem_hook);
 MakeCall(0x4494F5, &ControlWeapon_hook, true);
 HookCall(0x449570, &ControlArmor_hook);

 if (GetPrivateProfileInt("Misc", "FreeWeight", 0, ini)) {
  MakeCall(0x47002D, &display_inventory_hook, true);
  MakeCall(0x4703E9, &display_target_inventory_hook, true);

  HookCall(0x475363, &display_table_inventories_hook);

  SafeWrite16(0x4753D5, 0xD231);
  MakeCall(0x47541F, &display_table_inventories_hook1, true);
  HookCall(0x47558A, &display_table_inventories_hook2);

  SafeWrite16(0x4755BF, 0xFF31);
  MakeCall(0x47560A, &display_table_inventories_hook3, true);
  HookCall(0x4757D2, &display_table_inventories_hook2);

  HookCall(0x475D93, &barter_inventory_hook);
 }

 if (GetPrivateProfileInt("Misc", "EquipArmor", 0, ini)) {
  HookCall(0x4466CC, &gdProcess_hook);
 }

 char buf[MAX_PATH-3];
 int i, count;
 memset(armors,0,sizeof(npcArmor)*PartyMax);
 GetPrivateProfileString("Misc", "ArmorFile", "", buf, MAX_PATH, ini);
 if (strlen(buf) > 0) {
  sprintf(iniArmor, ".\\%s", buf);
  count = GetPrivateProfileIntA("Main", "Count", 0, iniArmor);
  char section[4];
  for (i = 1; i <= count; i++) {
   _itoa_s(i, section, 10);
   armors[npcCount].Pid = GetPrivateProfileIntA(section, "PID", 0, iniArmor);
   armors[npcCount].Default = GetPrivateProfileIntA(section, "Default", 0, iniArmor);
   armors[npcCount].Leather = GetPrivateProfileIntA(section, "Leather", 0, iniArmor);
   armors[npcCount].Power = GetPrivateProfileIntA(section, "Power", 0, iniArmor);
   armors[npcCount].Advanced = GetPrivateProfileIntA(section, "Advanced", 0, iniArmor);
   armors[npcCount].Metal = GetPrivateProfileIntA(section, "Metal", 0, iniArmor);
   armors[npcCount].Cured = GetPrivateProfileIntA(section, "Cured", 0, iniArmor);
   armors[npcCount].Combat = GetPrivateProfileIntA(section, "Combat", 0, iniArmor);
   armors[npcCount].Robe = GetPrivateProfileIntA(section, "Robe", 0, iniArmor);
   npcCount++;
  }
  if (npcCount > 0) HookCall(0x472833, &invenWieldFunc_hook);
 }

 if (GetPrivateProfileIntA("Misc", "StackEmptyWeapons", 0, ini)) {
  MakeCall(0x4736C6, &inven_action_cursor_hook, true);
  HookCall(0x4772AA, &item_add_force_call);
 }

// Не вызывать окошко выбора количества при перетаскивании патронов в оружие
 int ReloadReserve = GetPrivateProfileIntA("Misc", "ReloadReserve", 1, ini);
 if (ReloadReserve >= 0) {
  SafeWrite32(0x47655F, ReloadReserve);      // mov  eax, ReloadReserve
  SafeWrite32(0x476563, 0x097EC139);         // cmp  ecx, eax; jle  0x476570
  SafeWrite16(0x476567, 0xC129);             // sub  ecx, eax
  SafeWrite8(0x476569, 0x91);                // xchg ecx, eax
 };

// Использование химии из инвентаря на картинке игрока
 HookCall(0x471457, &inven_pickup_hook2);

// Кнопка "Положить всё"
 MakeCall(0x46FA7A, &make_drop_button, false);
 MakeCall(0x473EFC, &drop_all, true);
 HookCall(0x47418D, &loot_container_hook1);
 GetPrivateProfileString("sfall", "OverloadedDrop", "Sorry, there is no space left.", OverloadedDrop, 48, translationIni);

 UseScrollWheel = GetPrivateProfileIntA("Input", "UseScrollWheel", 1, ini) != 0;
 if (UseScrollWheel) {
  MakeCall(0x473E66, &loot_container_hook2, false);
  MakeCall(0x4759F1, &barter_inventory_hook1, false);
 };

// Обновление данных игрока (в частности веса вещей) при закрытии вложенной сумки
 HookCall(0x46EB1E, &handle_inventory_hook1);

}

void InventoryReset() {
 invenapcost = GetPrivateProfileInt("Misc", "InventoryApCost", 4, ini);
}
