/*
 *    sfall
 *    Copyright (C) 2009, 2010  The sfall team
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

static DWORD zone_number = 0;
static DWORD QuestsScrollButtonsX = 220;
static DWORD QuestsScrollButtonsY = 229;
static DWORD memFrms;

static void __declspec(naked) freeFrms() {
 __asm {
// eax = количество кнопок, edx = memFrms
  push ecx
  push ebx
  xchg ecx, eax
  mov  ebx, edx
loopFrms:
  mov  eax, [ebx+12]                        // eax = frm_ptr
  call art_ptr_unlock_
  add  ebx, 16
  loop loopFrms
  xchg edx, eax
  call mem_free_
  pop  ebx
  pop  ecx
  retn
 }
}

static void __declspec(naked) StartPipboy_hook() {
 __asm {
  mov  ds:[_pip_win], eax
  inc  eax                                  // Удачно создано окно?
  jz   end                                  // Нет
  dec  eax
  xchg ebp, eax                             // ebp = GNWID
  mov  eax, 6*16
  call mem_malloc_
  test eax, eax                             // Удачно выделили память?
  jz   deleteWin                            // Нет
  xor  esi, esi
  xchg edi, eax
  push edi
loopFrms:
  xor  ecx, ecx                             // ID1
  xor  ebx, ebx                             // ID2
  push ebx                                  // ID3
  lea  edx, [esi+49]                        // Index
  mov  eax, ObjType_Intrface                // ObjType
  call art_id_
  lea  edx, [edi+12]                        // *frm_ptr
  lea  ecx, [edi+8]                         // *Height
  lea  ebx, [edi+4]                         // *Width
  call art_lock_
  mov  [edi], eax                           // art_pic
  test eax, eax
  jz   fail
  inc  esi
  add  edi, 16
  cmp  esi, 6
  jb   loopFrms
  pop  edi
  mov  memFrms, edi
// creating first button
  mov  edx, QuestsScrollButtonsX            // Xpos
  mov  ebx, QuestsScrollButtonsY            // Ypos
  push edx
  push ebx
  xor  esi, esi
  push esi                                  // flags
  push esi
  dec  esi
  push [edi+(1*16)]                         // PicDown
  push [edi+(0*16)]                         // PicUp
  push 0x149                                // ButtUp (Page Up)
  push esi                                  // ButtDown
  push esi                                  // HovOff
  push esi                                  // HovOn
  mov  ecx, [edi+4+(0*16)]                  // Width
  push [edi+8+(0*16)]                       // Height
  mov  eax, ebp                             // GNWID
  call win_register_button_
  mov  ds:[_inven_scroll_up_bid], eax
  inc  eax
  jz   noUpButton
  dec  eax
  push eax
  mov  edx, [edi+(4*16)]
  mov  ecx, edx
  mov  ebx, edx
  call win_register_button_disable_
  pop  eax
  call win_disable_button_
noUpButton:
  pop  ebx                                  // Ypos
  pop  edx                                  // Xpos
// creating second button
  inc  esi
  push esi                                  // flags
  push esi
  dec  esi
  push [edi+(3*16)]                         // PicDown
  push [edi+(2*16)]                         // PicUp
  push 0x151                                // ButtUp (Page Down)
  push esi                                  // ButtDown
  push esi                                  // HovOff
  push esi                                  // HovOn
  mov  ecx, [edi+4+(2*16)]                  // Width
  mov  eax, [edi+8+(2*16)]                  // Height
  push eax                                  // Height
  add  ebx, eax                             // Ypos
  mov  eax, ebp                             // GNWID
  call win_register_button_
  mov  ds:[_inven_scroll_dn_bid], eax
  inc  eax
  jz   skip
  dec  eax
  push eax
  mov  edx, [edi+(5*16)]
  mov  ecx, edx
  mov  ebx, edx
  call win_register_button_disable_
  pop  eax
  call win_disable_button_
skip:
  xchg ebp, eax
  retn
fail:
  pop  edx
  xchg esi, eax
  call freeFrms
deleteWin:
  xchg ebp, eax
  call win_delete_
  xor  eax, eax
end:
  dec  eax
  retn
 }
}

static void __declspec(naked) EndPipboy_hook() {
 __asm {
  mov  edx, memFrms
  mov  eax, 6
  call freeFrms
  jmp  NixHotLines_
 }
}

static void __declspec(naked) pipboy_hook() {
 __asm {
  xor  eax, eax
  cmp  ds:[_stat_flag], al
  je   end
  cmp  zone_number, eax
  je   end
  mov  eax, ds:[_view_page]
  cmp  ebx, 0x149                           // Page Up?
  jne  notPgUp
  test eax, eax
  jz   endZone
  dec  eax
  jmp  click
endZone:
  mov  ebx, 0x210                           // Кнопка НАЗАД
notPgUp:
  cmp  ebx, 0x151                           // Page Down?
  jne  checkClickBug
  cmp  eax, ds:[_holopages]
  je   noKey
  inc  eax
click:
  mov  ds:[_view_page], eax
  call gsound_red_butt_press_
  mov  ebx, zone_number
  jmp  end
checkClickBug:
  cmp  ebx, 0x1F9
  jl   end
  cmp  ebx, 0x20F
  jg   end
noKey:
  xor  ebx, ebx
end:
  mov  edx, _mouse_y
  retn
 }
}

static void __declspec(naked) pipboy_hook1() {
 __asm {
  mov  edx, ds:[_crnt_func]
  test edx, edx
  jnz  end
  cmp  zone_number, edx
  mov  zone_number, ebx
  mov  ds:[_holopages], edx
  jne  end
  mov  eax, ds:[_inven_scroll_up_bid]
  inc  eax
  jz   end
  dec  eax
  call win_enable_button_
end:
  push 0x4971B8
  retn
 }
}

static void __declspec(naked) PipStatus_hook() {
 __asm {
  call _word_wrap_
  push eax
  test eax, eax
  jnz  end
  mov  ax, [esp+0x4A4]                      // Количество строк + 1
  dec  eax                                  // Количество строк
  shl  eax, 1                               // На каждую строку по две линии
  mov  ecx, ds:[_cursor_line]
  add  ecx, eax                             // Номер линии после вывода текста
  cmp  ecx, ds:[_bottom_line]
  jl   currentPage                          // Все строки вписываются
  mov  dword ptr ds:[_cursor_line], 3
  inc  dword ptr ds:[_holopages]
  mov  eax, 20
  sub  [esp+0x490], eax
  dec  dword ptr [esp+0x494]                // Номер текущего квеста
  sub  [esp+0x498], eax
  dec  dword ptr [esp+0x4A0]                // Номер квеста в списке квестов
  jmp  noWrite
currentPage:
  mov  eax, ds:[_view_page]
  cmp  eax, ds:[_holopages]
  je   end
  mov  ds:[_cursor_line], ecx
noWrite:
  mov  word ptr [esp+0x4A4], 1              // Количество строк
end:
  pop  eax
  retn
 }       
}

static void __declspec(naked) DownButton() {
 __asm {
  push eax
  push edx
  mov  eax, ds:[_inven_scroll_dn_bid]
  inc  eax
  jz   end
  dec  eax
  mov  edx, ds:[_view_page]
  cmp  edx, ds:[_holopages]
  je   disableButton
  call win_enable_button_
  jmp  end
disableButton:
  call win_disable_button_
end:
  pop  edx
  pop  eax
  retn
 }       
}

static void __declspec(naked) PipStatus_hook1() {
 __asm {
  call DownButton
  call pip_back_
  mov  eax, ds:[_holopages]
  test eax, eax
  jz   end
  push edx
  inc  eax
  push eax
  mov  ebx, 212                             // 'из'
  mov  edx, _pipmesg
  mov  eax, _pipboy_message_file
  call getmsg_
  push eax
  mov  eax, ds:[_view_page]
  inc  eax
  push eax
  push 0x50CD30                             // '%d %s %d'
  lea  eax, [esp+0x18]
  push eax
  call sprintf_
  add  esp, 0x14
  xor  ebx, ebx
  inc  ebx
  mov  ds:[_cursor_line], ebx
  mov  bl, ds:[_GreenColor]
  mov  edx, 0x20
  lea  eax, [esp+0x8]
  call pip_print_
  pop  edx
end:
  retn
 }       
}

static void __declspec(naked) ShowHoloDisk_hook() {
 __asm {
  call DownButton
  jmp  win_draw_
 }       
}

static void __declspec(naked) DisableButtons() {
 __asm {
  push eax
  xor  eax, eax
  mov  zone_number, eax
  mov  ds:[_holopages], eax
  mov  eax, ds:[_inven_scroll_up_bid]
  inc  eax
  jz   noUpButton
  dec  eax
  call win_disable_button_
noUpButton:
  mov  eax, ds:[_inven_scroll_dn_bid]
  inc  eax
  jz   noDownButton
  dec  eax
  call win_disable_button_
noDownButton:
  pop  eax
  retn
 }
}

static void __declspec(naked) PipFunc_hook() {
 __asm {
  call DisableButtons
  jmp  NixHotLines_
 }
}

static void __declspec(naked) PipAlarm_hook() {
 __asm {
  call DisableButtons
  jmp  critter_can_obj_dude_rest_
 }
}

void QuestListInit() {
 MakeCall(0x49740A, &StartPipboy_hook, false);
 HookCall(0x4978AD, &EndPipboy_hook);
 MakeCall(0x497088, &pipboy_hook, false);
 MakeCall(0x4971B2, &pipboy_hook1, true);
 HookCall(0x498186, &PipStatus_hook);
 HookCall(0x4982B0, &PipStatus_hook1);
 HookCall(0x498C31, &ShowHoloDisk_hook);
 HookCall(0x497BF6, &PipFunc_hook);         // PipStatus_
 HookCall(0x498D53, &PipFunc_hook);         // PipAutomaps_
 HookCall(0x499337, &PipFunc_hook);         // PipArchives_
 HookCall(0x499527, &PipAlarm_hook);        // PipAlarm_
 QuestsScrollButtonsX = GetPrivateProfileIntA("Misc", "QuestsScrollButtonsX", 220, ini);
 if (QuestsScrollButtonsX < 0 || QuestsScrollButtonsX > 618) QuestsScrollButtonsX = 220;
 QuestsScrollButtonsY = GetPrivateProfileIntA("Misc", "QuestsScrollButtonsY", 229, ini);
 if (QuestsScrollButtonsY < 0 || QuestsScrollButtonsY > 434) QuestsScrollButtonsY = 229;
}
