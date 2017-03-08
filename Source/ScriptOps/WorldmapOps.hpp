/*
 *    sfall
 *    Copyright (C) 2008, 2009, 2010, 2012  The sfall team
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

#pragma once

#include "main.h"

#include "ScriptExtender.h"

static DWORD EncounteredHorrigan;

static void _stdcall ForceEncounter4() {
 *(DWORD*)0x00672E04=EncounteredHorrigan;
 SafeWrite32(0x004C070E, 0x95);
 SafeWrite32(0x004C0718, 0x95);
 SafeWrite32(0x004C06D1, 0x2E043D83);
 SafeWrite32(0x004C071D, 0xFFFC2413);
 SafeWrite8(0x4C0706, 0x75);
}

static void __declspec(naked) ForceEncounter3() {
 __asm {
  push eax;
  push ebx;
  push ecx;
  push edx;
  mov eax, [esp+0x10];
  sub eax, 5;
  mov [esp+0x10], eax;
  call ForceEncounter4;
  pop edx;
  pop ecx;
  pop ebx;
  pop eax;
  retn;
 }
}

static void _stdcall ForceEncounter2(DWORD mapID, DWORD flags) {
 EncounteredHorrigan=*(DWORD*)0x00672E04;
 SafeWrite32(0x004C070E, mapID);
 SafeWrite32(0x004C0718, mapID);
 SafeWrite32(0x004C06D1, 0x18EBD231); //xor edx, edx / jmp 0x18
 SafeWrite32(0x004C071D, ((DWORD)&ForceEncounter3) - 0x004C0721);
 if(flags&1) SafeWrite8(0x4C0706, 0xeb);
}

static void __declspec(naked) ForceEncounter() {
 __asm {
  push ebx;
  push ecx;
  push edx;
  mov ecx, eax;
  call interpretPopShort_
  mov edx, eax;
  mov eax, ecx;
  call interpretPopLong_
  cmp dx, 0xC001;
  jnz end;
  push 0;
  push eax;
  call ForceEncounter2;
end:
  pop edx;
  pop ecx;
  pop ebx;
  retn;
 }
}

static void __declspec(naked) ForceEncounterWithFlags() {
 __asm {
  pushad
  mov ecx, eax;
  call interpretPopShort_
  mov edx, eax;
  mov eax, ecx;
  call interpretPopLong_
  mov ebx, eax;
  mov eax, ecx;
  call interpretPopShort_
  mov edi, eax;
  mov eax, ecx;
  call interpretPopLong_
  cmp dx, 0xC001;
  jnz end;
  cmp di, 0xc001;
  jnz end;
  push ebx;
  push eax;
  call ForceEncounter2;
end:
  popad
  retn;
 }
}

// world_map_functions
static void __declspec(naked) in_world_map() {
 __asm {
  push edx
  xor  edx, edx
  test InLoop, 0x1
  jz   skip
  inc  edx
skip:
  push eax
  call interpretPushLong_
  mov  edx, VAR_TYPE_INT
  pop  eax
  call interpretPushShort_
  pop  edx
  retn
 }
}

static void __declspec(naked) get_game_mode() {
 __asm {
  push edx
  mov  edx, InLoop
  push eax
  call interpretPushLong_
  mov  edx, VAR_TYPE_INT
  pop  eax
  call interpretPushShort_
  pop  edx
  retn
 }
}

static void __declspec(naked) GetWorldMapXPos() {
 __asm {
  push ebx;
  push ecx;
  push edx;
  mov ecx, eax;
  mov edx, ds:[_world_xpos]
  call interpretPushLong_
  mov edx, 0xc001;
  mov eax, ecx;
  call interpretPushShort_
  pop edx;
  pop ecx;
  pop ebx;
  retn;
 }
}

static void __declspec(naked) GetWorldMapYPos() {
 __asm {
  push ebx;
  push ecx;
  push edx;
  mov ecx, eax;
  mov edx, ds:[_world_ypos]
  call interpretPushLong_
  mov edx, 0xc001;
  mov eax, ecx;
  call interpretPushShort_
  pop edx;
  pop ecx;
  pop ebx;
  retn;
 }
}

static void __declspec(naked) SetWorldMapPos() {
 __asm {
  push ebx;
  push ecx;
  push edx;
  push edi;
  push esi;
  mov ecx, eax;
  call interpretPopShort_
  mov esi, eax;
  mov eax, ecx;
  call interpretPopLong_
  mov edi, eax;
  mov eax, ecx;
  call interpretPopShort_
  mov edx, eax;
  mov eax, ecx;
  call interpretPopLong_
  cmp dx, 0xC001;
  jnz end;
  cmp si, 0xC001;
  jnz end;
  mov ds:[_world_xpos], eax
  mov ds:[_world_ypos], edi
end:
  pop esi;
  pop edi;
  pop edx;
  pop ecx;
  pop ebx;
  retn;
 }
}
