/*
 *    sfall
 *    Copyright (C) 2008, 2009, 2010  The sfall team
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

#include "FalloutEngine.h"

bool usingExtraKillTypes = false;

static const DWORD ExtraKillTypesCount[] = {
 0x42D980,                                  // critter_kill_name_
 0x42D990,                                  // critter_kill_name_
 0x42D9C0,                                  // critter_kill_info_
 0x42D9D0,                                  // critter_kill_info_
 0x4344E4,                                  // ListKills_
// 0x43A154,                                  // Save_as_ASCII_ (Не сработает - количество навыков)
// 0x43A1FF,                                  // Save_as_ASCII_ (равно количеству убитых типов)
};

static void __declspec(naked) critter_kill_count_hook() {
 __asm {
  cmp  eax, -1
  je   skip
  cmp  eax, 38
  jl   end
skip:
  xor  eax, eax
  retn
end:
  push ebx
  lea  ebx, ds:[_pc_kill_counts+eax*2]
  xor  eax, eax
  mov  ax, [ebx]
  pop  ebx
  retn
 }
}

static void __declspec(naked) critter_kill_count_inc_hook() {
 __asm {
  cmp  eax, -1
  je   skip
  cmp  eax, 38
  jl   end
skip:
  xor  eax, eax
  dec  eax
  retn
end:
  push ebx
  lea  ebx, ds:[_pc_kill_counts+eax*2]
  inc  word ptr [ebx]
  xor  eax, eax
  pop  ebx
  retn
 }
}

void KillCounterInit() {
 usingExtraKillTypes = true;

 //Overwrite the function that reads the kill counter with my own
 MakeCall(0x42D8A8, &critter_kill_count_hook, true);

 //Overwrite the function that increments the kill counter with my own
 HookCall(0x425144, &critter_kill_count_inc_hook);

 for (int i = 0; i < sizeof(ExtraKillTypesCount)/4; i++) {
  SafeWrite8(ExtraKillTypesCount[i], 38);
 }

 SafeWrite32(0x42D9DD, 1488);
}
