/*
 *    sfall
 *    Copyright (C) 2009, 2010  Mash (Matt Wells, mashw at bigpond dot net dot au)
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
#include "FalloutEngine.h"
#include "SuperSave.h"

DWORD LSPageOffset = 0;

static bool ExtraSaveSlots = false;

static const char* filename = "%s\\savegame\\slotdat.ini";

static const DWORD CorrectSlotCursor[] = {
 0x47B929, 0x47D8DB, 0x47D9B0, 0x47DA34, 0x47DABF, 0x47DB58, 0x47DBE9,
 0x47DC9C, 0x47EC77, 0x47F5AB, 0x47F694, 0x47F6EB, 0x47F7FB, 0x47F892,
 0x47FB86, 0x47FC3A, 0x47FCF2, 0x480117, 0x4801CF, 0x480234, 0x480310,
 0x4803F3, 0x48049F, 0x480512, 0x4805F2, 0x480767, 0x4807E6, 0x480839,
 0x4808D3,
};

static void _stdcall LoadPageOffsets() {
 char LoadPath[MAX_PATH];
 sprintf(LoadPath, filename, *(char**)_patches);
 *(DWORD*)_slot_cursor = GetPrivateProfileIntA("Position", "ListNum", 0, LoadPath);
 if (*(DWORD*)_slot_cursor < 0 || *(DWORD*)_slot_cursor > 9) *(DWORD*)_slot_cursor = 0;
 if (ExtraSaveSlots) {
  LSPageOffset = GetPrivateProfileIntA("Position", "PageOffset", 0, LoadPath);
  if (LSPageOffset < 0 || LSPageOffset > 9990) LSPageOffset = 0;
 } else LSPageOffset = 0;
}

static void __declspec(naked) game_init_hook() {
 __asm {
  pushad
  call LoadPageOffsets                      // load last slot position values from file
  popad
  jmp  KillOldMaps_
 }
}

static void _stdcall SavePageOffsets() {
 char SavePath[MAX_PATH], buffer[6];
 sprintf(SavePath, filename, *(char**)_patches);
 _itoa_s(*(DWORD*)_slot_cursor, buffer, 10);
 WritePrivateProfileString("Position", "ListNum", buffer, SavePath);
 if (ExtraSaveSlots) {
  _itoa_s(LSPageOffset, buffer, 10);
  WritePrivateProfileString("Position", "PageOffset", buffer, SavePath);
 }
}

static void __declspec(naked) LSGameEnd_hook() {
 __asm {
  pushad
  call SavePageOffsets                      // save last slot position values to file
  popad
  jmp  win_delete_
 }
}

static void __declspec(naked) LSGameStart_hook() {
 __asm {
  call text_font_
  xor  esi, esi                             // esi = 0
  xor  edi, edi
  dec  edi                                  // edi = -1
  mov  ebx, 58
  push ebx                                  // Ypos
  push ebx                                  // Ypos
  push ebx                                  // Ypos
  push ebx                                  // Ypos
// left button -100
  mov  eax, 0x50CF0C                        // '<<'
  mov  edx, eax
  call ds:[_text_width]
  push eax
  push esi                                  // flags
  push edx                                  // '<<'
  push edi                                  // ButtUp
  push 0x149                                // ButtDown (Page Up)
  push edi                                  // HovOff
  mov  ecx, edi                             // HovOn
  mov  edx, 55+15                           // Xpos
  mov  eax, ds:[_lsgwin]                    // eax = WinRef
  mov  ebp, eax                             // ebp = WinRef
  call win_register_text_button_
  pop  edx                                  // ширина
  pop  ebx                                  // Ypos
// left button -10
  push esi                                  // flags
  push 0x50EDB4                             // '<'
  push edi                                  // ButtUp
  push 0x14B                                // ButtDown (left button)
  push edi                                  // HovOff
  mov  ecx, edi                             // HovOn
  add  edx, 55+15+15+16                     // Xpos
  mov  eax, ebp                             // WinRef
  call win_register_text_button_
  pop  ebx                                  // Ypos
// right button +100
  mov  eax, 0x50CF10                        // '>>'
  mov  edx, eax
  call ds:[_text_width]
  push eax
  push esi                                  // flags
  push edx                                  // '>>'
  push edi                                  // ButtUp
  push 0x151                                // ButtDown (Page Down)
  push edi                                  // HovOff
  mov  ecx, edi                             // HovOn
  mov  edx, 55+230-15-16                    // Xpos
  sub  edx, eax
  mov  eax, ebp                             // WinRef
  call win_register_text_button_
  pop  edx                                  // ширина
  pop  ebx                                  // Ypos
// right button +10
  push esi                                  // flags
  mov  eax, 0x50EDB8                        // '>'
  mov  esi, eax
  call ds:[_text_width]
  add  eax, edx
  push esi                                  // '>'
  push edi                                  // ButtUp
  push 0x14D                                // ButtDown (right button)
  push edi                                  // HovOff
  mov  ecx, edi                             // HovOn
  mov  edx, 55+230-15-15-16-16              // Xpos
  sub  edx, eax
  mov  eax, ebp                             // WinRef
  call win_register_text_button_
  pop  ebx                                  // Ypos
// Set Number button
  xor  esi, esi
  push esi                                  // flags
  push esi
  push esi                                  // PicDown
  push esi                                  // PicUp
  push edi                                  // ButtUp
  push 'p'                                  // ButtDown
  push edi                                  // HovOff
  push edi                                  // HovOn
  push 16                                   // Height
  mov  ecx, 50                              // Width
  mov  edx, 145                             // Xpos
  xchg ebp, eax                             // WinRef
  call win_register_button_
  retn
 }
}

static char FmtOffset[7]="[ %d ]";
static void __declspec(naked) ShowSlotList_hook() {
 __asm {
  push edx
  push ecx
  push eax
  push ecx                                  // MaxWidth
  push eax                                  // _lsgbuf
  push ebx                                  // ColorIndex
  add  eax, 38545                           // ToSurface (640*60+145)
  mov  edx, 50                              // Width
  mov  ebx, 12                              // Height
  call buf_fill_
  mov  eax, LSPageOffset
  mov  ebx, 10
  cdq
  div  ebx
  inc  eax
  push eax
  push offset FmtOffset                     // '[ %d ]'
  mov  edx, _str
  push edx
  call sprintf_
  add  esp, 3*4
  pop  edi                                  // edi = _lsgbuf
  pop  ecx                                  // MaxWidth
  movzx eax, byte ptr ds:[_DARK_GREEN_Color]
  push eax
  mov  eax, edx                             // *DisplayText
  call ds:[_text_width]
  mov  ebx, eax                             // TxtWidth
  shr  eax, 1                               // TxtWidth/2
  add  edi, 39850                           // 640*62+170
  sub  edi, eax
  xchg edi, eax                             // eax = ToSurface
  call ds:[_text_to_buf]
  pop  eax
  pop  ecx
  pop  edx
  add  eax, 55735                           // 640*87+55
  retn
 }
}

static void __declspec(naked) SaveLoadGame_hook() {
 __asm {
  cmp  eax, 'p'                             // 'p' button pressed?
  je   setNumber
  cmp  eax, 'P'                             // 'P' button pressed?
  je   setNumber
  mov  edx, 10
  cmp  eax, 0x14B                           // left button?
  je   subPage
  cmp  eax, 0x14D                           // right button?
  je   addPage
  mov  edx, 100
  cmp  eax, 0x149                           // Page Up?
  je   subPage
  cmp  eax, 0x151                           // Page Down?
  je   addPage
  xor  edx, edx
  cmp  eax, 0x148
  retn
addPage:
  add  LSPageOffset, edx
  jmp  end
subPage:
  sub  LSPageOffset, edx
  jmp  end
setNumber:
  pushad
  sub  esp, 16
  mov  edx, esp
  mov  ebx, 656                             // 'Э-э...'
  mov  eax, _proto_main_msg_file
  call getmsg_
  xchg ecx, eax
  mov  ebx, esp                             // *buf
  mov  eax, ds:[_lsgwin]
  call GNW_find_
  mov  edx, [eax+0x8+0x4]                   // _lsgwin.rect.y
  add  edx, 87
  push edx                                  // y
  mov  edx, [eax+0x8+0x0]                   // _lsgwin.rect.x
  add  edx, 240
  push edx                                  // x
  xor  edx, edx
  inc  edx                                  // min
  mov  eax, 1000                            // max
  push ecx                                  // *msg
  mov  ecx, edx                             // notClear
  xchg ebx, eax                             // eax = *buf, ebx = max
  call win_get_num_i_
  cmp  eax, -1
  je   skipNumber
  mov  eax, [esp]
  dec  eax
  imul eax, eax, 10
  mov  LSPageOffset, eax
skipNumber:
  add  esp, 16
  popad
end:
  mov  edx, 9990
  mov  eax, LSPageOffset
  cmp  eax, edx
  jg   setPage
  xor  edx, edx
  cmp  eax, edx
  jl   setPage
  xchg edx, eax
setPage:
  mov  LSPageOffset, edx
  call gsound_red_butt_press_
  call GetSlotList_                         // reset page save list func
  pop  edx
  add  edx, 26                              // set return to button pressed code
  jmp  edx
 }
}

// add page num offset when reading and writing various save data files
static void __declspec(naked) Correct_slot_cursor() {
 __asm {
  mov  eax, ds:[_slot_cursor]               // list position 0-9
  add  eax, LSPageOffset                    // add page num offset
  retn
 }
}

// getting info for the 10 currently displayed save slots from save.dats
static void __declspec(naked) GetSlotList_hook() {
 __asm {
  push 0x50A514                             // "SAVE.DAT"
  mov  eax, LSPageOffset
  add  eax, ebx
  inc  eax
  push eax
  mov  eax, 0x47E5EA
  jmp  eax
 }
}

// printing current 10 slot numbers
static void __declspec(naked) ShowSlotList_hook1() {
 __asm {
  add  eax, LSPageOffset                    // add page num offset
  mov  edx, _lsgmesg
  retn
 }
}

//--------------------------------------------------------------------------
void EnableSuperSaving() {
 ExtraSaveSlots = GetPrivateProfileIntA("Misc", "ExtraSaveSlots", 0, ini) != 0;
 HookCall(0x442995, &game_init_hook);       // load saved page and list positions from file
 HookCall(0x47D82D, &LSGameEnd_hook);       // save current page and list positions to file on load/save scrn exit
 if (ExtraSaveSlots) {
  dlog("Running EnableSuperSaving()", DL_INIT);
  HookCall(0x47D812, &LSGameStart_hook);    // save/load button setup func
  MakeCall(0x47E704, &ShowSlotList_hook, false);// Draw button text
  MakeCall(0x47BD49, &SaveLoadGame_hook, false);// check save buttons
  MakeCall(0x47CB1C, &SaveLoadGame_hook, false);// check load buttons
  // Add page offset to Load/Save folder number
  for (int i = 0; i < sizeof(CorrectSlotCursor)/4; i++) MakeCall(CorrectSlotCursor[i], &Correct_slot_cursor, false);
  MakeCall(0x47E5E1, &GetSlotList_hook, true);
  MakeCall(0x47E74F, &ShowSlotList_hook1, false);
  dlogr(" Done", DL_INIT);
 }
}
