#include "main.h"

#include "FalloutEngine.h"
#include "SafeWrite.h"

static const DWORD ItemName[] = {
 0x4282B6, 0x429045, 0x42AB91, 0x42AB91, 0x42ACCE, 0x42ADDF, 0x42B1AF,
 0x42B1E3, 0x42B211, 0x42B303, 0x42B3D3, 0x42B7E5, 0x458B12, 0x4943F0,
 0x494566, 0x494870, 0x494A27, 0x4956B1, 0x495BE6, 0x4C14AF,
};

static void __declspec(naked) win_debug_hook() {
 __asm {
  call debug_log_
  xor  eax, eax
  cmp  ds:[_GNW_win_init_flag], eax
  push 0x4DC320
  retn
 }
}

void DebugModeInit() {

 int tmp = GetPrivateProfileIntA("Debugging", "DebugMode", 0, ini);
 if (tmp && *((DWORD*)0x444A60) == 0xFFFE76FC && *((DWORD*)0x444A64) == 0x0F01FE83) {
  dlog("Applying DebugMode patch.", DL_INIT);
  MakeCall(0x4425E6, (void*)debug_register_env_, false);
  int tmp2 = 0x50F90C;                      // "log"
  if (tmp&1) {
   if (tmp&2) {
    SafeWrite16(0x4C6E75, 0x66EB);          // jmps 0x4C6EDD
    SafeWrite8(0x4C6EF2, 0xEB);
    SafeWrite8(0x4C7034, 0x0);
    MakeCall(0x4DC319, &win_debug_hook, true);
   } else tmp2 = 0x50F928;                  // "gnw"
  }
  SafeWrite8(0x4C6D9B, 0xB8);               // mov  eax, offset ???
  SafeWrite32(0x4C6D9C, tmp2);

  for (int i = 0; i < sizeof(ItemName)/4; i++) HookCall(ItemName[i], (void*)item_name_);

  SafeWriteStr(0x50108C, "\n%s minHp = %d; curHp = %d\n");
  SafeWriteStr(0x501174, "%s: FLEEING: OOR -> min_to_hit !\n");
  SafeWriteStr(0x5012D4, "%s is using %s packet with a %d chance to taunt\n");
  SafeWriteStr(0x501268, ">>>NOTE: %s had extra AP''s to use!<<<\n");

  dlogr(" Done", DL_INIT);
 } else BlockCall(0x4425E6);                // dlogr("Patching out ereg call.", DL_INIT);

}
