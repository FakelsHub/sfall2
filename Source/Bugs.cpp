#include "main.h"

#include "Bugs.h"
#include "Define.h"
#include "FalloutEngine.h"
#include "LoadGameHook.h"
#include "PartyControl.h"

DWORD WeightOnBody = 0;
DWORD SizeOnBody = 0;

static void __declspec(naked) determine_to_hit_func_hook() {
 __asm {
  call stat_level_                          // Perception|Восприятие
  cmp  edi, ds:[_obj_dude]
  jne  end
  xchg ecx, eax
  mov  eax, edi                             // _obj_dude
  mov  edx, PERK_sharpshooter
  call perk_level_
  shl  eax, 1
  add  eax, ecx
end:
  retn
 }
}

static void __declspec(naked) pipboy_hook() {
 __asm {
  cmp  ebx, 0x210                           // Кнопка НАЗАД?
  je   end
  cmp  byte ptr ds:[_holo_flag], 0
  jne  end
  xor  ebx, ebx                             // Нет человека - нет проблемы (c) :-p
end:
  mov  eax, ds:[_crnt_func]
  retn
 }
}

static void __declspec(naked) PipAlarm_hook() {
 __asm {
  mov  ds:[_crnt_func], eax
  mov  eax, 0x400
  call PipStatus_
  mov  eax, 0x50CC04                        // 'iisxxxx1'
  retn
 }
}

static void __declspec(naked) scr_save_hook() {
 __asm {
  mov  ecx, 16
  cmp  [esp+0xEC+4], ecx                    // number_of_scripts
  jg   skip
  mov  ecx, [esp+0xEC+4]
  cmp  ecx, 0
  jg   skip
  xor  eax, eax
  retn
skip:
  sub  [esp+0xEC+4], ecx                    // number_of_scripts
  push dword ptr [ebp+0xE00]                // num
  mov  [ebp+0xE00], ecx                     // num
  xor  ecx, ecx
  xchg [ebp+0xE04], ecx                     // NextBlock
  call scr_write_ScriptNode_
  xchg [ebp+0xE04], ecx                     // NextBlock
  pop  dword ptr [ebp+0xE00]                // num
  retn
 }
}

static void __declspec(naked) protinst_default_use_item_hook() {
 __asm {
  mov  eax, [edx+0x64]                      // eax = target pid
  cmp  eax, PID_DRIVABLE_CAR
  je   itsCar
  cmp  eax, PID_CAR_TRUNK
  jne  noCar
itsCar:
  mov  eax, ebx
  call obj_use_power_on_car_
  cmp  eax, -1
  jne  skip
noCar:
  push 0x49C38B
  retn                                      // "Это ничего не даст."
skip:
  test eax, eax
  jnz  end
  dec  eax
end:
  push 0x49C3C5
  retn
 }
}

static void __declspec(naked) obj_use_power_on_car_hook() {
 __asm {
  xor  eax, eax
  cmp  ebx, 596                             // "Аккумулятор полностью заряжен."?
  je   skip                                 // Да
  inc  eax                                  // "Вы заряжаете автомобильный аккумулятор."
skip:
  retn
 }
}

static void __declspec(naked) item_d_check_addict_hook() {
 __asm {
  xor  edx, edx
  inc  edx
  inc  edx                                  // type = зависимость
  inc  eax                                  // Есть drug_pid?
  jz   skip                                 // Нет
  dec  eax
  xchg ebx, eax                             // ebx = drug_pid
  mov  eax, esi                             // eax = source
  call queue_find_first_
loopQueue:
  test eax, eax                             // Есть что в списке?
  jz   end                                  // Нет
  cmp  ebx, [eax+0x4]                       // drug_pid == queue_addict.drug_pid?
  je   end                                  // Есть конкретная зависимость
  mov  eax, esi                             // eax = source
  xor  edx, edx
  inc  edx
  inc  edx                                  // type = зависимость
  call queue_find_next_
  jmp  loopQueue
skip:
  mov  eax, ds:[_obj_dude]
  call queue_find_first_
end:
  push 0x47A6A1
  retn
 }
}

static void __declspec(naked) remove_jet_addict() {
 __asm {
  cmp  eax, ds:[_wd_obj]
  jne  end
  cmp  dword ptr [edx+0x4], PID_JET         // queue_addict.drug_pid == PID_JET?
  jne  end
  xor  eax, eax
  inc  eax                                  // Удалить из очереди
  retn
end:
  xor  eax, eax                             // Не трогать
  retn
 }
}

static void __declspec(naked) item_d_take_drug_hook() {
 __asm {
  cmp  dword ptr [eax], 0                   // queue_addict.init
  jne  skip                                 // Зависимость ещё не активна
  mov  edx, PERK_add_jet
  mov  eax, esi
  call perform_withdrawal_end_
skip:
  mov  ds:[_wd_obj], esi
  xor  eax, eax
  inc  eax
  inc  eax                                  // type = зависимость
  mov  edx, offset remove_jet_addict
  call queue_clear_type_
  push 0x479FD1
  retn
 }
}

static void __declspec(naked) item_wd_process_hook() {
 __asm {
  cmp  esi, PERK_add_jet
  jne  end
  xor  edx, edx                             // edx=init
  mov  ecx, [ebx+0x4]                       // ecx=drug_pid
  push ecx
  mov  ecx, esi                             // ecx=perk
  mov  ebx, 10080                           // ebx=время в минутах (10080 минут = 168 часов = 7 дней)
  call insert_withdrawal_
  pop  eax                                  // Уничтожаем адрес возврата
  xor  eax, eax
  pop  esi
  pop  ecx
  pop  ebx
end:
  retn
 }
}

static void __declspec(naked) item_d_load_hook() {
 __asm {
  sub  esp, 4
  mov  [ebp], edi                           // edi->queue_drug
  mov  ecx, 7
  mov  esi, _drugInfoList+12
loopDrug:
  cmp  dword ptr [esi+8], 0                 // drugInfoList.numeffects
  je   nextDrug
  mov  edx, esp
  mov  eax, [esi]                           // drugInfoList.pid
  call proto_ptr_
  mov  edx, [esp]
  mov  eax, [edx+0x24]                      // drug.stat0
  cmp  eax, [edi+0x4]                       // drug.stat0 == queue_drug.stat0?
  jne  nextDrug                             // Нет
  mov  eax, [edx+0x28]                      // drug.stat1
  cmp  eax, [edi+0x8]                       // drug.stat1 == queue_drug.stat1?
  jne  nextDrug                             // Нет
  mov  eax, [edx+0x2C]                      // drug.stat2
  cmp  eax, [edi+0xC]                       // drug.stat2 == queue_drug.stat2?
  je   foundPid                             // Да
nextDrug:
  add  esi, 12
  loop loopDrug
foundPid:
  jecxz end
  mov  eax, [esi]                           // drugInfoList.pid
  mov  [edi], eax                           // queue_drug.drug_pid
end:
  xor  eax, eax
  add  esp, 4
  retn
 }
}

static void __declspec(naked) queue_clear_type_hook() {
 __asm {
  mov  ebx, [esi]
  jmp  mem_free_
 }
}

static void __declspec(naked) partyMemberCopyLevelInfo_hook() {
 __asm {
nextArmor:
  mov  eax, esi
  call inven_worn_
  test eax, eax
  jz   noArmor
  and  byte ptr [eax+0x27], 0xFB            // Сбрасываем флаг одетой брони
  jmp  nextArmor
noArmor:
  mov  eax, esi
  jmp  stat_level_
 }
}

static void __declspec(naked) partyMemberIncLevels_hook() {
 __asm {
  push eax
  call partyMemberCopyLevelInfo_
  pop  ebx
  cmp  eax, -1
  je   end
  pushad
  mov  edx, ebx
  xchg edx, eax                             // eax=source, edx=type
  call queue_remove_this_
  mov  ecx, 8
  mov  edi, _drugInfoList
  mov  esi, ebx
loopAddict:
  mov  eax, [edi]                           // eax = drug pid
  call item_d_check_addict_
  test eax, eax                             // Есть зависимость?
  jz   noAddict                             // Нет
  cmp  dword ptr [eax], 0                   // queue_addict.init
  jne  noAddict                             // Зависимость ещё не активна
  mov  edx, [eax+0x8]                       // queue_addict.perk
  mov  eax, ebx
  call perk_add_effect_
noAddict:
  add  edi, 12
  loop loopAddict
  popad
end:
  retn
 }
}

static void __declspec(naked) gdProcessUpdate_hook() {
 __asm {
  add  eax, esi
  cmp  eax, ds:[_optionRect + 12]           // _optionRect.offy
  jge  skip
  inc  eax
  inc  eax
  push 0x44702D
  retn
skip:
  push 0x4470DB
  retn
 }
}

static void __declspec(naked) invenWieldFunc_hook() {
 __asm {
  mov  edx, esi
  xor  ebx, ebx
  inc  ebx
  push ebx
  mov  cl, [edi+0x27]
  and  cl, 0x3
  xchg edx, eax                             // eax = source, edx = item
  call item_remove_mult_
  xchg ebx, eax
nextWeapon:
  mov  eax, esi
  test cl, 0x2                              // Правая рука?
  jz   leftHand                             // Нет
  call inven_right_hand_
  jmp  removeFlag
leftHand:
  call inven_left_hand_
removeFlag:
  test eax, eax
  jz   noWeapon
  and  byte ptr [eax+0x27], 0xFC            // Сбрасываем флаг оружия в руке
  jmp  nextWeapon
noWeapon:
  or   [edi+0x27], cl                       // Устанавливаем флаг оружия в руке
  inc  ebx
  pop  ebx
  jz   skip
  mov  eax, esi
  mov  edx, edi
  call item_add_force_
skip:
  mov  eax, edi
  jmp  item_get_type_
 }
}

static void __declspec(naked) loot_container_hook() {
 __asm {
  mov  eax, [esp+0x114+0x4]
  mov  SizeOnBody, eax
  test eax, eax
  jz   noLeftWeapon
  push eax
  call item_size_
  mov  SizeOnBody, eax
  pop  eax
  call item_weight_
noLeftWeapon:
  mov  WeightOnBody, eax
  mov  eax, [esp+0x118+0x4]
  test eax, eax
  jz   noRightWeapon
  push eax
  call item_size_
  add  SizeOnBody, eax
  pop  eax
  call item_weight_
  add  WeightOnBody, eax
noRightWeapon:
  mov  eax, [esp+0x11C+0x4]
  test eax, eax
  jz   noArmor
  push eax
  call item_size_
  add  SizeOnBody, eax
  pop  eax
  call item_weight_
  add  WeightOnBody, eax
noArmor:
  mov  eax, [esp+0xF8+0x4]
  push eax
  call item_c_curr_size_
  add  SizeOnBody, eax
  pop  eax
  call item_total_weight_
  add  WeightOnBody, eax
  xor  eax, eax
  inc  eax
  inc  eax
  retn
 }
}

static void __declspec(naked) barter_inventory_hook() {
 __asm {
  mov  eax, [esp+0x20+0x4]
  mov  SizeOnBody, eax
  test eax, eax
  jz   noArmor
  push eax
  call item_size_
  mov  SizeOnBody, eax
  pop  eax
  call item_weight_
noArmor:
  mov  WeightOnBody, eax
  mov  eax, [esp+0x1C+0x4]
  test eax, eax
  jnz  haveWeapon
  cmp  ds:[_dialog_target_is_party], eax
  jne  end                                  // это собутыльник
  mov  eax, [esp+0x18+0x4]
  test eax, eax
  jz   end
haveWeapon:
  push eax
  call item_size_
  add  SizeOnBody, eax
  pop  eax
  call item_weight_
  add  WeightOnBody, eax
end:
  mov  eax, [esp+0x10+0x4]
  push eax
  call item_c_curr_size_
  add  SizeOnBody, eax
  pop  eax
  call item_total_weight_
  add  WeightOnBody, eax
  mov  eax, ds:[_inven_dude]
  retn
 }
}

static void __declspec(naked) barter_attempt_transaction_hook() {
 __asm {
  call stat_level_                          // eax = Макс. груз
  sub  eax, WeightOnBody                    // Учитываем вес одетой на цели брони и оружия
  retn
 }
}

static void __declspec(naked) exit_inventory_hook() {
 __asm {
  mov  ds:[_i_lhand], eax
  mov  WeightOnBody, eax
  mov  SizeOnBody, eax
  mov  ds:[_curr_stack], eax
  mov  ds:[_target_stack], eax
  and  InLoop, (-1^0x39000)                 // INVENTORY + INTFACEUSE + INTFACELOOT + BARTER
  retn
 }
}

static void __declspec(naked) inven_pickup_hook() {
 __asm {
  mov  edx, ds:[_pud]
  mov  edx, [edx]                           // itemsCount
  dec  edx
  sub  edx, eax
  lea  edx, ds:0[edx*8]
  push 0x470EC9
  retn
 }
}

static void __declspec(naked) inven_pickup_hook1() {
 __asm {
  test eax, eax
  jz   end
  mov  eax, ds:[_i_wid]
  call GNW_find_
  mov  ecx, [eax+0x8+0x4]                   // ecx = _i_wid.rect.y
  mov  eax, [eax+0x8+0x0]                   // eax = _i_wid.rect.x
  add  eax, 44                              // x_start
  mov  ebx, 64
  add  ebx, eax                             // x_end
  xor  edx, edx
next:
  push eax
  push edx
  push ecx
  push ebx
  imul edx, edx, 48
  add  edx, 35
  add  edx, ecx                             // y_start
  mov  ecx, edx
  add  ecx, 48                              // y_end
  call mouse_click_in_
  pop  ebx
  pop  ecx
  pop  edx
  test eax, eax
  pop  eax
  jnz  found
  inc  edx
  cmp  edx, 6
  jb   next
end:
  push 0x47125C
  retn
found:
  mov  ebx, 0x4711DF
  add  edx, [esp+0x40]                      // inventory_offset
  mov  eax, ds:[_pud]
  mov  ecx, [eax]                           // itemsCount
  jecxz skip
  dec  ecx
  cmp  edx, ecx
  ja   skip
  sub  ecx, edx
  mov  edx, ecx
  mov  ebx, 0x471181
skip:
  jmp  ebx
 }
}

static void __declspec(naked) drop_ammo_into_weapon_hook() {
 __asm {
  dec  esi
  test esi, esi                             // Одна коробка патронов?
  jz   skip                                 // Да
  xor  esi, esi
// Лишняя проверка на from_slot, но пусть будет
  mov  edx, [esp+0x24+4]                    // from_slot
  cmp  edx, 1006                            // Руки?
  jge  skip                                 // Да
  lea  edx, [eax+0x2C]                      // Inventory
  mov  ecx, [edx]                           // itemsCount
  jecxz skip                                // инвентарь пустой (ещё лишняя проверка, но пусть будет)
  mov  edx, [edx+8]                         // FirstItem
nextItem:
  cmp  ebp, [edx]                           // Наше оружие?
  je   foundItem                            // Да
  add  edx, 8                               // К следующему
  loop nextItem
  jmp  skip                                 // Нашего оружия нет в инвентаре
foundItem:
  cmp  dword ptr [edx+4], 1                 // Оружие в единственном экземпляре?
  jg   skip                                 // Нет
  mov  edx, [esp+0x24+4]                    // from_slot
  lea  edx, [edx-1000]
  add  edx, [esp+0x40+4+0x24+4]             // edx=порядковый номер слота с патронами
  cmp  ecx, edx                             // Оружие после патронов?
  jg   skip                                 // Да
  inc  esi                                  // Нет, нужно менять from_slot
skip:
  mov  edx, ebp
  call item_remove_mult_
  test eax, eax                             // Удалили оружие из инвентаря?
  jnz  end                                  // Нет
  sub  [esp+0x24+4], esi                    // Да, корректируем from_slot
end:
  retn
 }
}

static void __declspec(naked) PipStatus_hook() {
 __asm {
  call AddHotLines_
  xor  eax, eax
  mov  ds:[_hot_line_count], eax
  retn
 }
}

static void __declspec(naked) perform_withdrawal_start_hook() {
 __asm {
  test eax, eax
  jz   end
  jmp  display_print_
end:
  retn
 }
}

static void __declspec(naked) op_wield_obj_critter_hook() {
 __asm {
  xor  edx, edx                             // Старой брони нет, поскольку она была снята в invenWieldFunc_hook (HookScripts.cpp) с последующим вызовом adjust_ac_
  call adjust_ac_
  xor  eax, eax                             // не анимировать
  jmp  intface_update_ac_
 }
}

//checks if an attacked object is a critter before attempting dodge animation
static void __declspec(naked) action_melee_hook() {
 __asm {
  mov  edx, 0x4113DC
  mov  ebx, [eax+0x20]                      // pobj.fid
  and  ebx, 0x0F000000
  sar  ebx, 0x18
  cmp  ebx, ObjType_Critter                 // check if object FID type flag is set to critter
  jne  end                                  // if object not a critter skip dodge animation
  test byte ptr [eax+0x44], 0x3             // (DAM_KNOCKED_OUT or DAM_KNOCKED_DOWN)?
  jnz  end
  mov  edx, 0x4113FE
end:
  jmp  edx
 }
}

static void __declspec(naked) action_ranged_hook() {
 __asm {
  mov  edx, 0x411B6D
  mov  ebx, [eax+0x20]                      // pobj.fid
  and  ebx, 0x0F000000
  sar  ebx, 0x18
  cmp  ebx, ObjType_Critter                 // check if object FID type flag is set to critter
  jne  end                                  // if object not a critter skip dodge animation
  test byte ptr [eax+0x44], 0x3             // (DAM_KNOCKED_OUT or DAM_KNOCKED_DOWN)?
  jnz  end
  mov  edx, 0x411BD2
end:
  jmp  edx
 }
}

static DWORD XPWithSwiftLearner;
static void __declspec(naked) statPCAddExperienceCheckPMs_hook() {
 __asm {
  mov  XPWithSwiftLearner, edi
  mov  eax, ds:[_Experience_]
  retn
 }
}

static void __declspec(naked) combat_give_exps_hook() {
 __asm {
  call stat_pc_add_experience_
  mov  ebx, XPWithSwiftLearner
  retn
 }
}

static void __declspec(naked) loot_container_hook1() {
 __asm {
  mov  edx, [esp+0x138]
  xchg edx, eax
  call stat_pc_add_experience_
  dec  edx
  jnz  skip
  push XPWithSwiftLearner
  mov  ebx, [esp+0xE8]
  push ebx
  lea  eax, [esp+0x8]
  push eax
  call sprintf_
  add  esp, 0xC
  mov  eax, esp
  call display_print_
skip:
  push 0x4745E3
  retn
 }
}

static void __declspec(naked) wmRndEncounterOccurred_hook() {
 __asm {
  call stat_pc_add_experience_
  cmp  ecx, 110
  jnb  skip
  push XPWithSwiftLearner
  push edx
  lea  eax, [esp+0x8+0x4]
  push eax
  call sprintf_
  add  esp, 0xC
  lea  eax, [esp+0x4]
  call display_print_
skip:
  retn
 }
}

static void __declspec(naked) set_new_results_hook() {
 __asm {
  test ah, 0x1                              // DAM_KNOCKED_OUT?
  jz   end                                  // Нет
  mov  eax, esi
  xor  edx, edx
  inc  edx                                  // type = отключка
  jmp  queue_remove_this_                   // Удаляем отключку из очереди (если отключка там есть)
end:
  pop  eax                                  // Уничтожаем адрес возврата
  push 0x424FC6
  retn
 }
}

static void __declspec(naked) critter_wake_clear_hook() {
 __asm {
  jne  end                                  // Это не персонаж
  mov  dl, [esi+0x44]
  test dl, 0x80                             // DAM_DEAD?
  jnz  end                                  // Это трупик
  and  dl, 0xFE                             // Сбрасываем DAM_KNOCKED_OUT
  or   dl, 0x2                              // Устанавливаем DAM_KNOCKED_DOWN
  mov  [esi+0x44], dl
end:
  xor  eax, eax
  inc  eax
  pop  esi
  pop  ecx
  pop  ebx
  retn
 }
}

static void __declspec(naked) obj_load_func_hook() {
 __asm {
  test byte ptr [eax+0x25], 0x4             // Temp_
  jnz  end
  mov  edi, [eax+0x64]
  shr  edi, 0x18
  cmp  edi, ObjType_Critter
  jne  skip
  test byte ptr [eax+0x44], 0x2             // DAM_KNOCKED_DOWN?
  jz   clear                                // Нет
  pushad
  xor  ecx, ecx
  inc  ecx
  xor  ebx, ebx
  xor  edx, edx
  xchg edx, eax
  call queue_add_
  popad
clear:
  and  word ptr [eax+0x44], 0x7FFD          // not (DAM_LOSE_TURN or DAM_KNOCKED_DOWN)
skip:
  push 0x488F14
  retn
end:
  push 0x488EF9
  retn
 }
}

static void __declspec(naked) partyMemberPrepLoadInstance_hook() {
 __asm {
  and  word ptr [eax+0x44], 0x7FFD          // not (DAM_LOSE_TURN or DAM_KNOCKED_DOWN)
  jmp  dude_stand_
 }
}

static void __declspec(naked) combat_ctd_init_hook() {
 __asm {
  mov  [esi+0x24], eax                      // ctd.targetTile
  mov  eax, [ebx+0x54]                      // pobj.who_hit_me
  inc  eax
  jnz  end
  mov  [ebx+0x54], eax                      // pobj.who_hit_me
end:
  push 0x422F11
  retn
 }
}

static void __declspec(naked) obj_save_hook() {
 __asm {
  inc  eax
  jz   end
  dec  eax
  mov  edx, [esp+0x1C]                      // combat_data
  mov  eax, [eax+0x68]                      // pobj.who_hit_me.cid
  test byte ptr ds:[_combat_state], 1       // В бою?
  jz   clear                                // Нет
  cmp  dword ptr [edx], 0                   // В бою?
  jne  skip                                 // Да
clear:
  xor  eax, eax
  dec  eax
skip:
  mov  [edx+0x18], eax                      // combat_data.who_hit_me
end:
  push 0x489422
  retn
 }
}

static void __declspec(naked) action_explode_hook() {
 __asm {
  mov  edx, destroy_p_proc
  mov  eax, [esi+0x78]                      // pobj.sid
  call exec_script_proc_
  xor  edx, edx
  dec  edx
  retn
 }
}

static void __declspec(naked) action_explode_hook1() {
 __asm {
  push esi
  mov  esi, [esi+0x40]                      // ctd.target#
  call action_explode_hook
  pop  esi
  retn
 }
}

static void __declspec(naked) ai_danger_source_hook() {
 __asm {
  xor  ecx, ecx
  test byte ptr [ebx+0x25], 0x8             // is target multihex?
  mov  ebx, [ebx+0x4]                       // ebx = pobj.tile_num (target's tilenum)
  jz   end                                  // skip if not multihex
  inc  ebx                                  // otherwise, increase tilenum by 1
end:
  retn
 }
}

static void __declspec(naked) use_inventory_on_hook() {
 __asm {
  inc  ecx
  mov  edx, [eax]                           // Inventory.inv_size
  sub  edx, ecx
  jge  end
  mov  edx, [eax]                           // Inventory.inv_size
end:
  retn
 }
}

static void __declspec(naked) op_obj_can_see_obj_hook() {
 __asm {
  mov  ebx, 0x456BDC
  mov  eax, [esp+4]                         // source
  mov  ecx, [eax+0x28]                      // source.elev
  cmp  ecx, [edx+0x28]                      // target.elev
  jne  end
  cmp  dword ptr [eax+0x4], -1              // source.tile_num
  je   end
  cmp  dword ptr [edx+0x4], -1              // target.tile_num
  je   end
  mov  ebx, 0x456BA2
end:
  jmp  ebx
 }
}

static void __declspec(naked) op_obj_can_hear_obj_hook() {
 __asm {
  mov  edi, 0x458414
  mov  eax, [esp]                           // target
  test eax, eax
  jz   end
  mov  edx, [esp+4]                         // source
  test edx, edx
  jz   end
  mov  edi, 0x4583E6
end:
  jmp  edi
 }
}

static void __declspec(naked) switch_hand_hook() {
 __asm {
  mov  eax, ds:[_inven_dude]
  push eax
  mov  [edi], ebp
  inc  ecx
  jz   skip
  xor  ebx, ebx
  inc  ebx
  mov  edx, ebp
  call item_remove_mult_
skip:
  pop  edx
  mov  eax, ebp
  call item_get_type_
  dec  eax                                  // item_type_container?
  jnz  end                                  // Нет
  mov  [ebp+0x7C], edx                      // iobj.owner = _inven_dude
end:
  pop  ebp
  pop  edi
  pop  esi
  retn
 }
}

static void __declspec(naked) inven_item_wearing() {
 __asm {
  mov  esi, ds:[_inven_dude]
  xchg ebx, eax                             // ebx = source
  mov  eax, [esi+0x20]
  and  eax, 0xF000000
  sar  eax, 0x18
  test eax, eax                             // Это ObjType_Item?
  jnz  skip                                 // Нет
  mov  eax, esi
  call item_get_type_
  dec  eax                                  // Сумка/Рюкзак?
  jnz  skip                                 // Нет
  mov  eax, esi
  call obj_top_environment_
  test eax, eax                             // Есть владелец?
  jz   skip                                 // Нет
  mov  ecx, [eax+0x20]
  and  ecx, 0xF000000
  sar  ecx, 0x18
  cmp  ecx, ObjType_Critter                 // Это персонаж?
  jne  skip                                 // Нет
  cmp  eax, ebx                             // владелец сумки == source?
  je   end                                  // Да
skip:
  xchg ebx, eax
  cmp  eax, esi
end:
  retn
 }
}

static void __declspec(naked) combat_display_hook() {
 __asm {
  mov  ebx, 0x42536B
  je   end                                  // Это ObjType_Critter
  cmp  dword ptr [ecx+0x78], -1             // У цели есть скрипт?
  jne  end                                  // Да
  mov  ebx, 0x425413
end:
  jmp  ebx
 }
}

static void __declspec(naked) apply_damage_hook() {
 __asm {
  xchg edx, eax
  test [esi+0x15], dl                       // ctd.flags2Source & DAM_HIT_?
  jz   end                                  // Нет
  inc  ebx
end:
  retn
 }
}

static void __declspec(naked) inven_action_cursor_hook() {
 __asm {
  cmp  dword ptr [esp+0x44+0x4], item_type_container
  jne  end
  cmp  eax, ds:[_stack]
  je   end
  cmp  eax, ds:[_target_stack]
end:
  retn
 }
}

static void __declspec(naked) combat_hook() {
 __asm {
  mov  eax, [ecx+eax]                       // eax = source
  test eax, eax
  jz   end
  push eax
  mov  edx, STAT_max_move_points
  call stat_level_
  mov  edx, ds:[_gcsd]
  test edx, edx
  jz   skip
  add  eax, [edx+0x8]                       // gcsd.free_move
skip:
  pop  edx
  xchg edx, eax                             // eax = source, edx = Макс. очки действия
  mov  [eax+0x40], edx                      // pobj.curr_mp
  test byte ptr ds:[_combat_state], 1       // В бою?
  jz   end                                  // Нет
  mov  edx, [eax+0x68]                      // pobj.cid
  cmp  edx, -1
  je   end
  push eax
  mov  eax, ds:[_aiInfoList]
  shl  edx, 4
  mov  dword ptr [edx+eax+0xC], 0           // aiInfo.lastMove
  pop  eax
end:
  mov  edx, edi                             // dude_turn
  retn
 }
}

static void __declspec(naked) wmTeleportToArea_hook() {
 __asm {
  cmp  ebx, ds:[_WorldMapCurrArea]
  je   end
  mov  ds:[_WorldMapCurrArea], ebx
  sub  eax, edx
  add  eax, ds:[_wmAreaInfoList]
  mov  edx, [eax+0x30]                      // wmAreaInfoList.world_posy
  mov  ds:[_world_ypos], edx
  mov  edx, [eax+0x2C]                      // wmAreaInfoList.world_posx
  mov  ds:[_world_xpos], edx
end:
  xor  eax, eax
  mov  ds:[_target_xpos], eax
  mov  ds:[_target_ypos], eax
  mov  ds:[_In_WorldMap], eax
  push 0x4C5A77
  retn
 }
}

static void __declspec(naked) combat_display_hook1() {
 __asm {
  mov  ecx, [eax+0x20]                      // pobj.fid
  and  ecx, 0x0F000000
  sar  ecx, 0x18
  cmp  ecx, ObjType_Critter
  jne  skip
  jmp  stat_level_
skip:
  xor  eax, eax
  retn
 }
}

void BugsInit() {

 dlog("Applying sharpshooter patch.", DL_INIT);
// http://www.nma-fallout.com/showthread.php?178390-FO2-Engine-Tweaks&p=4050162&viewfull=1#post4050162
// by Slider2k
 HookCall(0x4244AB, &determine_to_hit_func_hook);
 SafeWrite8(0x424527, 0xEB);
 dlogr(" Done", DL_INIT);

// Исправление багов кликабельности в пипбое
 MakeCall(0x4971C7, &pipboy_hook, false);
 MakeCall(0x499530, &PipAlarm_hook, false);

// Исправление "Too Many Items Bug"
 HookCall(0x4A596A, &scr_save_hook);
 HookCall(0x4A59C1, &scr_save_hook);

// Исправление "заправки" не_автомобиля и использования топливных элементов даже когда бак полный
 if (GetPrivateProfileIntA("Misc", "CarChargingFix", 1, ini)) MakeCall(0x49C36D, &protinst_default_use_item_hook, true);
 MakeCall(0x49BE70, &obj_use_power_on_car_hook, false);

// Исправление проверки наркотической зависимости
 MakeCall(0x47A644, &item_d_check_addict_hook, true);

// Исправления снятия зависимости к винту у нпс
 MakeCall(0x479FC5, &item_d_take_drug_hook, true);
 MakeCall(0x47A3A4, &item_wd_process_hook, false);

// Исправление прокачки статов через употребление наркотиков после записи-загрузки
 MakeCall(0x47A243, &item_d_load_hook, false);

// Исправление краша при использовании стимпаков на жертве с последующим выходом с карты
 HookCall(0x4A27E7, &queue_clear_type_hook);

// Мерзкий баг! Если в инвентаре сопартийца есть броня такая же как на нём одета, то при
// получении уровня он теряет КБ равный значению получаемому от этой брони
 HookCall(0x495F3B, &partyMemberCopyLevelInfo_hook);

// Исправление неправильных статов при получении уровня сопартийцем когда он под наркотиками
 HookCall(0x495D5C, &partyMemberIncLevels_hook);

// 9 опций в диалоговом окне
 MakeCall(0x44701C, &gdProcessUpdate_hook, true);

// Исправление "Unlimited Ammo bug"
 HookCall(0x472957, &invenWieldFunc_hook);

// Исправление отображения отрицательных значений в окне навыков ("S")
 SafeWrite8(0x4AC377, 0x7F);                // jg

// Исправление ошибки неучёта веса одетых вещей
 MakeCall(0x473B4E, &loot_container_hook, false);
 MakeCall(0x4758B0, &barter_inventory_hook, false);
 HookCall(0x474CB8, &barter_attempt_transaction_hook);
 MakeCall(0x46FC8A, &exit_inventory_hook, false);

// Ширина текста 64, а не 80 
 SafeWrite8(0x475541, 64);
 SafeWrite8(0x475789, 64);

// Исправление ошибки обратного порядка в инвентаре игрока
 MakeCall(0x470EC2, &inven_pickup_hook, true);

// Исправление ошибки в инвентаре игрока связанной с IFACE_BAR_MODE=1 из f2_res.ini, ну
// и ошибки обратного порядка
 MakeCall(0x47114A, &inven_pickup_hook1, true);

// Исправление ошибки использования только одной пачки патронов когда оружие находится перед патронами
 HookCall(0x476598, &drop_ammo_into_weapon_hook);

 dlog("Applying black skilldex patch.", DL_INIT);
 HookCall(0x497D0F, &PipStatus_hook);
 dlogr(" Done", DL_INIT);

 dlog("Applying withdrawal perk description crash fix. ", DL_INIT);
 HookCall(0x47A501, &perform_withdrawal_start_hook);
 dlogr(" Done", DL_INIT);

 dlog("Applying Jet Antidote fix.", DL_INIT);
 MakeCall(0x47A013, (void*)0x47A168, true);
 dlogr(" Done", DL_INIT);

 dlog("Applying shiv patch.", DL_INIT);
 SafeWrite8(0x477B2B, 0xEB);
 dlogr(" Done", DL_INIT);

 dlog("Applying wield_obj_critter fix.", DL_INIT);
 SafeWrite8(0x456912, 0x1E);
 HookCall(0x45697F, &op_wield_obj_critter_hook);
 dlogr(" Done", DL_INIT);

 dlog("Applying Dodgy Door Fix.", DL_INIT);
 MakeCall(0x4113D6, &action_melee_hook, true);
 MakeCall(0x411BCC, &action_ranged_hook, true);
 dlogr(" Done", DL_INIT);

// При выводе количества полученных очков опыта учитывать перк 'Прилежный ученик'
 MakeCall(0x4AFAEF, &statPCAddExperienceCheckPMs_hook, false);
 HookCall(0x4221E2, &combat_give_exps_hook);
 MakeCall(0x4745AE, &loot_container_hook1, true);
 SafeWrite16(0x4C0AB1, 0x23EB);             // jmps 0x4C0AD6
 HookCall(0x4C0AEB, &wmRndEncounterOccurred_hook);

// Исправление "NPC turns into a container"
 MakeCall(0x424F8E, &set_new_results_hook, false);
 MakeCall(0x42E46E, &critter_wake_clear_hook, true);
 MakeCall(0x488EF3, &obj_load_func_hook, true);
 HookCall(0x4949B2, &partyMemberPrepLoadInstance_hook);

// Fix explosives bugs
 MakeCall(0x422F05, &combat_ctd_init_hook, true);
 MakeCall(0x489413, &obj_save_hook, true);
 MakeCall(0x4130C3, &action_explode_hook, false);
 MakeCall(0x4130E5, &action_explode_hook1, false);

 dlog("Applying MultiHex Pathing Fix.", DL_INIT);
 MakeCall(0x42901F, &ai_danger_source_hook, false);
 MakeCall(0x429170, &ai_danger_source_hook, false);
 dlogr(" Done", DL_INIT);

 dlog("Applying imported procedure patch. ", DL_INIT);// http://teamx.ru/site_arc/smf/index.php-topic=398.0.htm
 SafeWrite32(0x46B35A, 0x1C24A489);         // Исправление проблем с временным стеком
 SafeWrite8(0x46DBF1, 0xEB);                // Отключение предупреждений
 SafeWrite8(0x46DDC4, 0xEB);                // Отключение предупреждений
 SafeWrite8(0x4415CC, 0x00);                // предотвращение слета при повторном экспорте
 dlogr(" Done", DL_INIT);

 dlog("Applying NPCLevelFix.", DL_INIT);
 HookCall(0x495BC9, (void*)0x495E51);
 dlogr(" Done", DL_INIT);

// Исправление краша при клике по пустому месту в инвентаре при курсорном использовании предмета из рюкзака
 MakeCall(0x471A94, &use_inventory_on_hook, false);

#ifdef TRACE
// Исправление видимости предмета только в активной руке игрока для op_critter_inven_obj_
 SafeWrite8(0x458FF6, 0xEB);
 SafeWrite8(0x459023, 0xEB);
#endif

// Исправление op_obj_can_see_obj_ и op_obj_can_hear_obj_
 MakeCall(0x456B63, &op_obj_can_see_obj_hook, true);
 MakeCall(0x4583D3, &op_obj_can_hear_obj_hook, true);

// Исправление пропадания предметов в инвентаре при попытке поместить их во вложенную сумку если игрок перегружен
 HookCall(0x4764FC, (void*)item_add_force_);
// Восстановление принадлежности сумки игроку после помещения её в руку
 MakeCall(0x4715DB, &switch_hand_hook, true);

// Исправление неучёта одетых вещей на игроке при открытой вложенной сумке
 MakeCall(0x471B7F, &inven_item_wearing, false);// inven_right_hand_
 SafeWrite8(0x471B84, 0x90);                // nop
 MakeCall(0x471BCB, &inven_item_wearing, false);// inven_left_hand_
 SafeWrite8(0x471BD0, 0x90);                // nop
 MakeCall(0x471C17, &inven_item_wearing, false);// inven_worn_
 SafeWrite8(0x471C1C, 0x90);                // nop

// Исправление вывода в окно монитора сообщения при попадании в случайную скриптовую цель если она не является персонажем
 MakeCall(0x425365, &combat_display_hook, true);
// Исправление вызова damage_p_proc при промахах если цель не_персонаж
 MakeCall(0x424CD2, &apply_damage_hook, false);

// Исправление краша при попытке открыть сумку/рюкзак в окне обмена/торговли
 MakeCall(0x473191, &inven_action_cursor_hook, false);

// Исправление неправильной инициализации очков действия в начале каждого хода
 BlockCall(0x422E02);
 MakeCall(0x422E1B, &combat_hook, false);

// 
 MakeCall(0x4C5A41, &wmTeleportToArea_hook, true);

// Исправление бага заcтывания убитых персонажей при использовании kill_critter_type
 SafeWrite16(0x457E22, 0xDB31);
 SafeWrite32(0x457C99, 0x30BE0075);

// Временный костыль, ошибка в sfall, нужно поискать причину
 HookCall(0x42530A, &combat_display_hook1);

}
