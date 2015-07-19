#include "main.h"

#include "Bugs.h"
#include "Define.h"
#include "FalloutEngine.h"

DWORD WeightOnBody = 0;

static void __declspec(naked) determine_to_hit_func_hook() {
 __asm {
  call stat_level_                          // Perception|����������
  cmp  edi, dword ptr ds:[_obj_dude]
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
  cmp  ebx, 0x210                           // ������ �����?
  je   end
  cmp  byte ptr ds:[_holo_flag], 0
  jne  end
  xor  ebx, ebx                             // ��� �������� - ��� �������� (c) :-p
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
  cmp  dword ptr [esp+0xEC+4], ecx          // number_of_scripts
  jg   skip
  mov  ecx, dword ptr [esp+0xEC+4]
  cmp  ecx, 0
  jg   skip
  xor  eax, eax
  retn
skip:
  sub  dword ptr [esp+0xEC+4], ecx          // number_of_scripts
  push dword ptr [ebp+0xE00]                // num
  mov  dword ptr [ebp+0xE00], ecx           // num
  xor  ecx, ecx
  xchg dword ptr [ebp+0xE04], ecx           // NextBlock
  call scr_write_ScriptNode_
  xchg dword ptr [ebp+0xE04], ecx           // NextBlock
  pop  dword ptr [ebp+0xE00]                // num
  retn
 }
}

static void __declspec(naked) protinst_default_use_item_hook() {
 __asm {
  mov  eax, dword ptr [edx+0x64]            // eax = target pid
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
  mov  eax, 0x49C38B
  jmp  eax                                  // "��� ������ �� ����."
skip:
  test eax, eax
  jnz  end
  dec  eax
end:
  mov  esi, 0x49C3C5
  jmp  esi
 }
}

static void __declspec(naked) obj_use_power_on_car_hook() {
 __asm {
  xor  eax, eax
  cmp  ebx, 596                             // "����������� ��������� �������."?
  je   skip                                 // ��
  inc  eax                                  // "�� ��������� ������������� �����������."
skip:
  retn
 }
}

static void __declspec(naked) item_d_check_addict_hook() {
 __asm {
  mov  edx, 2                               // type = �����������
  cmp  eax, -1                              // ���� drug_pid?
  je   skip                                 // ���
  xchg ebx, eax                             // ebx = drug_pid
  mov  eax, esi                             // eax = who
  call queue_find_first_
loopQueue:
  test eax, eax                             // ���� ��� � ������?
  jz   end                                  // ���
  cmp  ebx, dword ptr [eax+0x4]             // drug_pid == queue_addict.drug_pid?
  je   end                                  // ���� ���������� �����������
  mov  eax, esi                             // eax = who
  mov  edx, 2                               // type = �����������
  call queue_find_next_
  jmp  loopQueue
skip:
  mov  eax, dword ptr ds:[_obj_dude]
  call queue_find_first_
end:
  mov  esi, 0x47A6A1
  jmp  esi
 }
}

static void __declspec(naked) remove_jet_addict() {
 __asm {
  cmp  eax, dword ptr ds:[_wd_obj]
  jne  end
  cmp  dword ptr [edx+0x4], PID_JET         // queue_addict.drug_pid == PID_JET?
  jne  end
  xor  eax, eax
  inc  eax                                  // ������� �� �������
  retn
end:
  xor  eax, eax                             // �� �������
  retn
 }
}

static void __declspec(naked) item_d_take_drug_hook() {
 __asm {
  cmp  dword ptr [eax], 0                   // queue_addict.init
  jne  skip                                 // ����������� ��� �� �������
  mov  edx, PERK_add_jet
  mov  eax, esi
  call perform_withdrawal_end_
skip:
  mov  dword ptr ds:[_wd_obj], esi
  mov  eax, 2                               // type = �����������
  mov  edx, offset remove_jet_addict
  call queue_clear_type_
  mov  eax, 0x479FD1
  jmp  eax
 }
}

static void __declspec(naked) item_wd_process_hook() {
 __asm {
  cmp  esi, PERK_add_jet
  je   itsJet
  retn
itsJet:
  pop  edx                                  // ���������� ����� ��������
  xor  edx, edx                             // edx=init
  mov  ecx, dword ptr [ebx+0x4]             // ecx=drug_pid
  push ecx
  mov  ecx, esi                             // ecx=perk
  mov  ebx, 2880                            // ebx=time
  call insert_withdrawal_
  mov  eax, 0x47A3FB
  jmp  eax
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
  jne  nextDrug                             // ���
  mov  eax, [edx+0x28]                      // drug.stat1
  cmp  eax, [edi+0x8]                       // drug.stat1 == queue_drug.stat1?
  jne  nextDrug                             // ���
  mov  eax, [edx+0x2C]                      // drug.stat2
  cmp  eax, [edi+0xC]                       // drug.stat2 == queue_drug.stat2?
  je   foundPid                             // ��
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
  and  byte ptr [eax+0x27], 0xFB            // ���������� ���� ������ �����
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
  mov  dword ptr ds:[_critterClearObj], ebx
  mov  edx, critterClearObjDrugs_
  call queue_clear_type_
  mov  ecx, 8
  mov  edi, _drugInfoList
  mov  esi, ebx
loopAddict:
  mov  eax, dword ptr [edi]                 // eax = drug pid
  call item_d_check_addict_
  test eax, eax                             // ���� �����������?
  jz   noAddict                             // ���
  cmp  dword ptr [eax], 0                   // queue_addict.init
  jne  noAddict                             // ����������� ��� �� �������
  mov  edx, dword ptr [eax+0x8]             // queue_addict.perk
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
  cmp  eax, dword ptr ds:[_optionRect + 12] // _optionRect.offy
  jge  skip
  add  eax, 2
  mov  esi, 0x44702D
  jmp  esi
skip:
  mov  esi, 0x4470DB
  jmp  esi
 }
}

static void __declspec(naked) invenWieldFunc_hook() {
 __asm {
  pushad
  mov  edx, esi
  xor  ebx, ebx
  inc  ebx
  push ebx
  mov  cl, byte ptr [edi+0x27]
  and  cl, 0x3
  xchg edx, eax                             // eax=who, edx=item
  call item_remove_mult_
nextWeapon:
  mov  eax, esi
  test cl, 0x2                              // ������ ����?
  jz   leftHand                             // ���
  call inven_right_hand_
  jmp  removeFlag
leftHand:
  call inven_left_hand_
removeFlag:
  test eax, eax
  jz   noWeapon
  and  byte ptr [eax+0x27], 0xFC            // ���������� ���� ������ � ����
  jmp  nextWeapon
noWeapon:
  or   byte ptr [edi+0x27], cl              // ������������� ���� ������ � ����
  xchg esi, eax
  mov  edx, edi
  pop  ebx
  call item_add_force_
  popad
  jmp  item_get_type_
 }
}

static void __declspec(naked) loot_container_hook() {
 __asm {
  mov  eax, [esp+0x114+0x4]
  test eax, eax
  jz   noLeftWeapon
  call item_weight_
noLeftWeapon:
  mov  WeightOnBody, eax
  mov  eax, [esp+0x118+0x4]
  test eax, eax
  jz   noRightWeapon
  call item_weight_
noRightWeapon:
  add  WeightOnBody, eax
  mov  eax, [esp+0x11C+0x4]
  test eax, eax
  jz   noArmor
  call item_weight_
noArmor:
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
  test eax, eax
  jz   noArmor
  call item_weight_
noArmor:
  mov  WeightOnBody, eax
  mov  eax, [esp+0x1C+0x4]
  test eax, eax
  jnz  haveWeapon
  cmp  dword ptr ds:[_dialog_target_is_party], eax
  jne  end                                  // ��� �����������
  mov  eax, [esp+0x18+0x4]
  test eax, eax
  jz   end
haveWeapon:
  call item_weight_
  add  WeightOnBody, eax
end:
  mov  ebx, PID_JESSE_CONTAINER
  retn
 }
}

static void __declspec(naked) barter_attempt_transaction_hook() {
 __asm {
  call stat_level_                          // eax = ����. ����
  sub  eax, WeightOnBody                    // ��������� ��� ������ �� ���� ����� � ������
  retn
 }
}

static DWORD Looting = 0;
static void __declspec(naked) move_inventory_hook() {
 __asm {
  inc  Looting
  call move_inventory_
  dec  Looting
  retn
 }
}

static void __declspec(naked) item_add_mult_hook() {
 __asm {
  call stat_level_                          // eax = ����. ����
  cmp  Looting, 0
  je   end
  sub  eax, WeightOnBody                    // ��������� ��� ������ �� ���� ����� � ������
end:
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
  mov  eax, 0x470EC9
  jmp  eax
 }
}

static DWORD inven_pickup_loop=-1;
static const DWORD inven_pickup_hook1_Loop = 0x471145;
static void __declspec(naked) inven_pickup_hook1() {
 __asm {
  cmp  inven_pickup_loop, -1
  jne  inLoop
  test eax, eax
  jnz  startLoop
  mov  eax, 0x47125C
  jmp  eax
startLoop:
  xor  edx, edx
  mov  inven_pickup_loop, edx
nextLoop:
  mov  eax, 124                             // x_start
  mov  ebx, 188                             // x_end
  add  edx, 35                              // y_start
  mov  ecx, edx
  add  ecx, 48                              // y_end
  jmp  inven_pickup_hook1_Loop
inLoop:
  test eax, eax
  mov  eax, inven_pickup_loop
  jnz  foundRect
  inc  eax
  mov  inven_pickup_loop, eax
  imul edx, eax, 48
  jmp  nextLoop
foundRect:
  mov  inven_pickup_loop, -1
  mov  edx, [esp+0x40]                      // inventory_offset
  add  edx, eax
  mov  eax, ds:[_pud]
  push eax
  mov  eax, [eax]                           // itemsCount
  test eax, eax
  jz   skip
  dec  eax
  cmp  edx, eax
  jle  inRange
skip:
  pop  eax
  mov  ebx, 0x4711DF
  jmp  ebx
inRange:
  xchg edx, eax
  sub  edx, eax
  pop  eax
  mov  ecx, 0x471181
  jmp  ecx
 }
}

static void __declspec(naked) drop_ammo_into_weapon_hook() {
 __asm {
  dec  esi
  test esi, esi                             // ���� ������� ��������?
  jz   skip                                 // ��
  xor  esi, esi
// ������ �������� �� from_slot, �� ����� �����
  mov  edx, [esp+0x24+4]                    // from_slot
  cmp  edx, 1006                            // ����?
  jge  skip                                 // ��
  lea  edx, [eax+0x2C]                      // Inventory
  mov  ecx, [edx]                           // itemsCount
  jcxz skip                                 // ��������� ������ (��� ������ ��������, �� ����� �����)
  mov  edx, [edx+8]                         // FirstItem
nextItem:
  cmp  ebp, [edx]                           // ���� ������?
  je   foundItem                            // ��
  add  edx, 8                               // � ����������
  loop nextItem
  jmp  skip                                 // ������ ������ ��� � ���������
foundItem:
  cmp  dword ptr [edx+4], 1                 // ������ � ������������ ����������?
  jg   skip                                 // ���
  mov  edx, [esp+0x24+4]                    // from_slot
  lea  edx, [edx-1000]
  add  edx, [esp+0x40+4+0x24+4]             // edx=���������� ����� ����� � ���������
  cmp  ecx, edx                             // ������ ����� ��������?
  jg   skip                                 // ��
  inc  esi                                  // ���, ����� ������ from_slot
skip:
  mov  edx, ebp
  call item_remove_mult_
  test eax, eax                             // ������� ������ �� ���������?
  jnz  end                                  // ���
  sub  [esp+0x24+4], esi                    // ��, ������������ from_slot
end:
  retn
 }
}

static void __declspec(naked) PipStatus_hook() {
 __asm {
  call AddHotLines_
  xor  eax, eax
  mov  dword ptr ds:[_hot_line_count], eax
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

static void __declspec(naked) item_d_take_drug_hook1() {
 __asm {
  mov  eax, 0x47A168
  jmp  eax
 }
}

static void __declspec(naked) op_wield_obj_critter_hook() {
 __asm {
  call adjust_ac_
  xor  ecx, ecx                             // todo: ��������� ��� �� �� ��� �����
  jmp  intface_update_ac_
 }
}

//checks if an attacked object is a critter before attempting dodge animation
static void __declspec(naked) action_melee_hook() {
 __asm {
  mov  eax, [ebp+0x20]                      // (original code) objStruct ptr
  mov  ebx, [eax+0x20]                      // objStruct->FID
  and  ebx, 0x0F000000
  sar  ebx, 0x18
  cmp  ebx, ObjType_Critter                 // check if object FID type flag is set to critter
  jne  end                                  // if object not a critter leave jump condition flags
                                            // set to skip dodge animation
  test byte ptr [eax+0x44], 3               // (original code) DAM_KNOCKED_OUT or DAM_KNOCKED_DOWN
end:
  mov  ebx, 0x4113DA
  jmp  ebx
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
  cmp  edx, 1
  jne  skip
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
  mov  ebx, 0x4745E3
  jmp  ebx
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
  mov  ecx, 0x424FC6
  test ah, 0x3                              // DAM_KNOCKED_OUT or DAM_KNOCKED_DOWN
  jz   end                                  // � �������� � �� �����
  xor  edx, edx
  inc  edx                                  // type = ��������
  test ah, 0x1                              // DAM_KNOCKED_OUT
  jnz  knocked_out                          // ��� ��������
  mov  eax, esi
  call queue_find_
  test eax, eax                             // ���� �������� � �������?
  jnz  end                                  // ��
  mov  ecx, 0x424FAD
  xchg edx, eax
  jmp  ecx
knocked_out:
  mov  dword ptr ds:[_critterClearObj], esi
  mov  eax, critterClearObjDrugs_
  xchg edx, eax
  call queue_clear_type_
  mov  ecx, 0x424F93
end:
  jmp  ecx
 }
}

static void __declspec(naked) critter_wake_clear_hook() {
 __asm {
  test dl, 0x80                             // DAM_DEAD
  jnz  skip                                 // ��� ������
  push eax
  mov  eax, esi
  call isPartyMember_
  test eax, eax                             // ��� ���������?
  pop  eax
  jnz  end                                  // ��
  and  dl, 0xFE                             // ���������� DAM_KNOCKED_OUT
  or   dl, 0x2                              // ������������� DAM_KNOCKED_DOWN
  mov  [esi+0x44], dl
skip:
  pop  eax                                  // ���������� ����� ��������
  xor  eax, eax
  inc  eax
  pop  esi
  pop  ecx
  pop  ebx
end:
  retn
 }
}

static void __declspec(naked) critter_wake_clear_hook1() {
 __asm {
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
  mov  edi, 0x488EF9
  test byte ptr [eax+0x25], 0x4             // Temp_
  jnz  end
  mov  edi, [eax+0x64]
  shr  edi, 0x18
  cmp  edi, ObjType_Critter
  jne  skip
  test byte ptr [eax+0x44], 0x2             // DAM_KNOCKED_DOWN
  jz   skip
  pushad
  xor  ecx, ecx
  inc  ecx
  xor  ebx, ebx
  xor  edx, edx
  xchg edx, eax
  call queue_add_
  popad
skip:
  mov  edi, 0x488F14
end:
  jmp  edi
 }
}

void BugsInit() {

 dlog("Applying sharpshooter patch.", DL_INIT);
// http://www.nma-fallout.com/showthread.php?178390-FO2-Engine-Tweaks&p=4050162&viewfull=1#post4050162
// by Slider2k
 HookCall(0x4244AB, &determine_to_hit_func_hook);
 SafeWrite8(0x424527, 0xEB);
 dlogr(" Done", DL_INIT);

// ����������� ����� �������������� � ������
 MakeCall(0x4971C7, &pipboy_hook, false);
 MakeCall(0x499530, &PipAlarm_hook, false);

// ����������� "Too Many Items Bug"
 HookCall(0x4A596A, &scr_save_hook);
 HookCall(0x4A59C1, &scr_save_hook);

// ����������� "��������" ��_���������� � ������������� ��������� ��������� ���� ����� ��� ������
 MakeCall(0x49C36D, &protinst_default_use_item_hook, true);
 MakeCall(0x49BE70, &obj_use_power_on_car_hook, false);

// ����������� �������� ������������� �����������
 MakeCall(0x47A644, &item_d_check_addict_hook, true);

// ����������� ������ ����������� � ����� � ���
 MakeCall(0x479FC5, &item_d_take_drug_hook, true);
 MakeCall(0x47A3A4, &item_wd_process_hook, false);

// ����������� �������� ������ ����� ������������ ���������� ����� ������-��������
 MakeCall(0x47A243, &item_d_load_hook, false);

// ����������� ����� ��� ������������� ��������� �� ������ � ����������� ������� � �����
 HookCall(0x4A27E7, &queue_clear_type_hook);

// ������� ���! ���� � ��������� ���������� ���� ����� ����� �� ��� �� �� �����, �� ���
// ��������� ������ �� ������ �� ������ �������� ����������� �� ���� �����
 HookCall(0x495F3B, &partyMemberCopyLevelInfo_hook);

// ����������� ������������ ������ ��� ��������� ������ ����������� ����� �� ��� �����������
 HookCall(0x495D5C, &partyMemberIncLevels_hook);

// 9 ����� � ���������� ����
 MakeCall(0x44701C, &gdProcessUpdate_hook, true);

// ����������� "Unlimited Ammo bug"
 HookCall(0x472957, &invenWieldFunc_hook);

// ����������� ����������� ������������� �������� � ���� ������� ("S")
 SafeWrite8(0x4AC377, 0x7F);                // jg

// ����������� ������ ������� ���� ������ �����
 MakeCall(0x473B4E, &loot_container_hook, false);
 MakeCall(0x47588A, &barter_inventory_hook, false);
 HookCall(0x474CB8, &barter_attempt_transaction_hook);
 HookCall(0x4742AD, &move_inventory_hook);
 HookCall(0x4771B5, &item_add_mult_hook);

// ������ ������ 64, � �� 80 
 SafeWrite8(0x475541, 64);
 SafeWrite8(0x475789, 64);

// ����������� ������ ��������� ������� � ��������� ������
 MakeCall(0x470EC2, &inven_pickup_hook, true);

// ����������� ������ � ��������� ������ ��������� � IFACE_BAR_MODE=1 �� f2_res.ini, ��
// � ������ ��������� �������
 MakeCall(0x47114A, &inven_pickup_hook1, true);

// ����������� ������ ������������� ������ ����� ����� �������� ����� ������ ��������� �����
// ���������
 HookCall(0x476598, &drop_ammo_into_weapon_hook);

 dlog("Applying black skilldex patch.", DL_INIT);
 HookCall(0x497D0F, &PipStatus_hook);
 dlogr(" Done", DL_INIT);

 dlog("Applying withdrawal perk description crash fix. ", DL_INIT);
 HookCall(0x47A501, &perform_withdrawal_start_hook);
 dlogr(" Done", DL_INIT);

 dlog("Applying Jet Antidote fix.", DL_INIT);
 MakeCall(0x47A013, &item_d_take_drug_hook1, true);
 dlogr(" Done", DL_INIT);

 dlog("Applying shiv patch. ", DL_INIT);
 SafeWrite8(0x477B2B, 0xEB);
 dlogr(" Done", DL_INIT);

 dlog("Applying wield_obj_critter fix.", DL_INIT);
 SafeWrite8(0x456912, 0x1E);
 HookCall(0x45697F, &op_wield_obj_critter_hook);
 dlogr(" Done", DL_INIT);

 dlog("Applying Dodgy Door Fix.", DL_INIT);
 MakeCall(0x4113D3, &action_melee_hook, true);
 dlogr(" Done", DL_INIT);

// ��� ������ ���������� ���������� ����� ����� ��������� ���� '��������� ������'
 MakeCall(0x4AFAEF, &statPCAddExperienceCheckPMs_hook, false);
 HookCall(0x4221E2, &combat_give_exps_hook);
 MakeCall(0x4745AE, &loot_container_hook1, true);
 SafeWrite16(0x4C0AB1, 0x23EB);             // jmps 0x4C0AD6
 HookCall(0x4C0AEB, &wmRndEncounterOccurred_hook);

// ����������� "NPC turns into a container"
 MakeCall(0x424F8E, &set_new_results_hook, true);
 SafeWrite16(0x42E44F, 0x03EB);             // jmps 0x42E454
 MakeCall(0x42E476, &critter_wake_clear_hook, false);
 MakeCall(0x42E4BA, &critter_wake_clear_hook1, true);
 MakeCall(0x488EF3, &obj_load_func_hook, true);

}
