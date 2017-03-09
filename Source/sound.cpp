#include "main.h"

#include "FalloutEngine.h"

static char attackerSnd[8];
static char targetSnd[8];

static void __declspec(naked) combatai_msg_hook() {
 __asm {
  mov  edi, [esp+0xC]
  pushad
  cmp  eax, _target_str
  jne  attacker
  lea  eax, targetSnd
  jmp  end
attacker:
  lea  eax, attackerSnd
end:
  mov  edx, edi
  mov  ebx, 8
  call strncpy_
  popad
  jmp  strncpy_
 }
}

static void __declspec(naked) ai_print_msg_hook() {
 __asm {
  pushad
  cmp  edx, _target_str
  jne  attacker
  lea  eax, targetSnd
  jmp  end
attacker:
  lea  eax, attackerSnd
end:
  mov  ebx, [eax]
  test bl, bl
  jz   skip
  call gsound_play_sfx_file_
skip:
  popad
  jmp  text_object_create_
 }
}

static void __declspec(naked) gsnd_build_weapon_sfx_name_hook() {
 __asm {
  xor  edx, edx
  inc  edx
  inc  edx
  inc  eax
  call roll_random_
  mov  esi, eax
  push eax
  mov  al, cl
  push eax
  push ebx
  mov  al, [esp+0x1C]
  push eax
  mov  al, [esp+0x1C]
  push eax
  push 0x503DE0                             // 'W%c%c%1d%cXX%1d'
  mov  edx, _sfx_file_name
  push edx
  call sprintf_
  add  esp, 7*4
  xchg edx, eax
  call strupr_
  push 0x503908                             // '.ACM'
  push eax                                  // _sfx_file_name
  push 0x5035BC                             // 'sound\sfx\'
  push 0x503C98                             // '%s%s%s'
  mov  edx, _str
  push edx
  call sprintf_
  mov  eax, esp
  xchg edx, eax
  call db_dir_entry_
  add  esp, 5*4
  inc  eax
  jnz  skip
  inc  eax
  xchg esi, eax
skip:
  push esi
  mov  al, cl
  push eax
  push 0x451863
  retn
 }
}

void SoundInit() {
 int tmp = GetPrivateProfileIntA("Sound", "NumSoundBuffers", 0, ini);
 if (tmp > 0 && tmp <= 127) SafeWrite8(0x451129, (BYTE)tmp);

 if (GetPrivateProfileIntA("Sound", "AllowSoundForFloats", 0, ini)) {
  HookCall(0x42B7C7, &combatai_msg_hook);
  HookCall(0x42B849, &ai_print_msg_hook);
 }

 //Yes, I did leave this in on purpose. Will be of use to anyone trying to add in the sound effects
 if (GetPrivateProfileIntA("Sound", "Test_ForceFloats", 0, ini)) {
  SafeWrite8(0x42B772, 0xEB);
 }

 MakeCall(0x45185E, &gsnd_build_weapon_sfx_name_hook, true);

}
