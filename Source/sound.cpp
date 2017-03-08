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

}
