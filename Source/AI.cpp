/*
 *    sfall
 *    Copyright (C) 2012  The sfall team
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

#include <hash_map>
#include "AI.h"
#include "FalloutEngine.h"
#include "SafeWrite.h"

typedef stdext::hash_map<DWORD, DWORD> :: const_iterator iter;

static stdext::hash_map<DWORD,DWORD> targets;
static stdext::hash_map<DWORD,DWORD> sources;

DWORD _stdcall AIGetLastAttacker(DWORD target) {
 iter itr = sources.find(target);
 if (itr == sources.end()) return 0;
 else return itr->second;
}

DWORD _stdcall AIGetLastTarget(DWORD source) {
 iter itr = targets.find(source);
 if (itr == targets.end()) return 0;
 else return itr->second;
}

static void _stdcall CombatAttackHook(DWORD source, DWORD target) {
 sources[target] = source;
 targets[source] = target;
}

static void __declspec(naked) combat_attack_hook() {
 _asm {
  pushad
  push edx
  push eax
  call CombatAttackHook
  popad
  jmp  combat_attack_
 }
}

DWORD CombatDisabled = 0;
static char CombatBlockedMessage[128];

static void __declspec(naked) intface_use_item_hook() {
 __asm {
  xor  eax, eax
  cmp  CombatDisabled, eax
  je   end
  mov  eax, offset CombatBlockedMessage
  call display_print_
  pop  eax                                  // ”ничтожаем адрес возврата
  push 0x45F6D7
end:
  mov  eax, 20
  retn
 }
}

static void __declspec(naked) game_handle_input_hook() {
 __asm {
  mov  eax, ds:[_intfaceEnabled]
  test eax, eax
  jz   end
  cmp  CombatDisabled, eax
  jne  end
  mov  eax, offset CombatBlockedMessage
  call display_print_
  xor  eax, eax
end:
  retn
 }
}

void AIInit() {
 HookCall(0x426A95, combat_attack_hook);
 HookCall(0x42A796, combat_attack_hook);
 MakeCall(0x45F6AF, &intface_use_item_hook, false);
 HookCall(0x4432A6, &game_handle_input_hook);
 GetPrivateProfileString("sfall", "BlockedCombat", "You cannot enter combat at this time.", CombatBlockedMessage, 128, translationIni);
}

void _stdcall AICombatStart() {
 targets.clear();
 sources.clear();
}

void _stdcall AICombatEnd() {
 targets.clear();
 sources.clear();
}
