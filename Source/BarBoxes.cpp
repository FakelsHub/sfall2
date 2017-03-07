/*
 *    sfall
 *    Copyright (C) 2010  The sfall team
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

#include "BarBoxes.h"
#include "FalloutEngine.h"

struct sBox {
 DWORD msg;
 DWORD colour;
 void* mem;
};

static sBox boxes[10];
static DWORD boxesEnabled[5];

static void __declspec(naked) refresh_box_bar_win_hook() {
 __asm {
  xor  ebx, ebx
loopBoxes:
  mov  eax, boxesEnabled[ebx*4]
  test eax, eax
  jz   skip
  lea  eax, [ebx+5]
  call add_bar_box_
  add  esi, eax
skip:
  inc  ebx
  cmp  ebx, 4
  jne  loopBoxes
  cmp  esi, 1
  jg   end
  pop  eax                                  // ���������� ����� ��������
  mov  eax, 0x4615BE
  push eax
end:
  retn
 }
}

void BarBoxesInit() {
 SafeWrite32(0x461266, (DWORD)boxes + 8);
 SafeWrite32(0x4612AC, (DWORD)boxes + 8);
 SafeWrite32(0x4612FE, (DWORD)boxes + 4);
 SafeWrite32(0x46133C, (DWORD)boxes + 0);
 SafeWrite32(0x461374, (DWORD)boxes + 8);
 SafeWrite32(0x4613E8, (DWORD)boxes + 8);

 SafeWrite32(0x461479, (DWORD)boxes + 8);
 SafeWrite32(0x46148C, (DWORD)boxes + 8);
 SafeWrite32(0x4616BB, (DWORD)boxes + 8);

 memset(boxes, 0, 12*10);
 memset(boxesEnabled, 0, 5*4);
 memcpy(boxes, (void*)0x518FE8, 12*5);
 
 for (int i = 5; i < 10; i++) boxes[i].msg = 0x69 + i - 5;

 SafeWrite8(0x46127C, 10);
 SafeWrite8(0x46140B, 10);
 SafeWrite8(0x461495, 0x78);

 MakeCall(0x4615A3, &refresh_box_bar_win_hook, false);
 char buf[6];
 GetPrivateProfileString("Misc", "BoxBarColours", "", buf, 6, ini);
 if (strlen(buf) == 5) {
  for (int i = 0; i < 5; i++) if (buf[i] == '1') boxes[i+5].colour = 1;
 }
}

int _stdcall GetBox(int i) { 
 if (i < 5 || i > 9) return 0;
 return boxesEnabled[i-5];
}

void _stdcall AddBox(int i) { 
 if (i < 5 || i > 9) return;
 boxesEnabled[i-5] = 1;
}

void _stdcall RemoveBox(int i) { 
 if (i < 5 || i > 9) return;
 boxesEnabled[i-5] = 0;
}
