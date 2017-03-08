#include "Defines.h"
#include "Input.h"
#include "Numbers.h"
#include "SafeWrite.h"

static void RunNumbers(DWORD load) {
 if(!load) {
 }
 exit(0);
}

static void _declspec(naked) NewGameHook() {
 _asm {
  pushad;
  push DIK_LSHIFT;
  call KeyDown;
  test eax, eax;
  jz fail;
  popad;
  xor eax, eax;
  push eax;
  call RunNumbers();
fail:
  popad;
  jmp main_menu_hide_
 }
}

static void _declspec(naked) LoadGameHook() {
 _asm {
  pushad;
  push DIK_LSHIFT;
  call KeyDown;
  test eax, eax;
  jz fail;
  popad;
  push 1;
  call RunNumbers();
fail:
  popad;
  mov  ecx, 480
  retn
 }
}

NumbersInit() {
 HookCall(0x480A81, &NewGameHook);
 MakeCall(0x480AF9, &LoadGameHook, false);
}
