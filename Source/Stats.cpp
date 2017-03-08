/*
 *    sfall
 *    Copyright (C) 2008-2016  The sfall team
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

#include <math.h>
#include <stdio.h>
#include "Define.h"
#include "FalloutEngine.h"
#include "Stats.h"

static const DWORD AgeMin[] = {
 0x51D860, 0x4373C9, 0x4373F3, 0x43754C,
};

static const DWORD AgeMax[] = {
 0x43731B, 0x437352, 0x437536,
};

struct Stat {
 int min;
 int max;
};

struct StatFormula {
 int base;
 int min;
};

static Stat StatPC[STAT_max_stat];
static Stat StatNPC[STAT_max_stat];

DWORD StandardApAcBonus = 4;
DWORD ExtraApAcBonus = 4;

static DWORD xpTable[99];

static void __declspec(naked) stat_level_hook() {
 __asm {
  shl  esi, 3                               // esi*8
  cmp  ebx, ds:[_obj_dude]
  je   player
  mov  ebx, StatNPC[esi].min
  cmp  ecx, ebx
  jl   change
  mov  ebx, StatNPC[esi].max
  cmp  ecx, ebx
  jle  end
  jmp  change
player:
  mov  ebx, StatPC[esi].min
  cmp  ecx, ebx
  jl   change
  mov  ebx, StatPC[esi].max
  cmp  ecx, ebx
  jle  end
change:
  mov  ecx, ebx
end:
  mov  ebx, 0x4AF3D5
  jmp  ebx
 }
}

static void __declspec(naked) stat_set_base_hook() {
 __asm {
  mov  eax, 0x4AF59C
  cmp  esi, ds:[_obj_dude]
  je   player
  cmp  ebx, StatNPC[ecx*8].min
  jge  skip
failMin:
  mov  eax, 0x4AF57E
  jmp  eax
skip:
  cmp  ebx, StatNPC[ecx*8].max
  jg   failMax
  jmp  eax
player:
  cmp  ebx, StatPC[ecx*8].min
  jl   failMin
  cmp  ebx, StatPC[ecx*8].max
  jle  end
failMax:
  mov  eax, 0x4AF591
end:
  jmp  eax
 }
}

void _stdcall SetPCStatMin(int stat, int value) {
 if (stat >= STAT_st && stat < STAT_max_stat) StatPC[stat].min = value;
}

void _stdcall SetPCStatMax(int stat, int value) {
 if (stat >= STAT_st && stat < STAT_max_stat) StatPC[stat].max = value;
}

void _stdcall SetNPCStatMin(int stat, int value) {
 if (stat >= STAT_st && stat < STAT_max_stat) StatNPC[stat].min = value;
}

void _stdcall SetNPCStatMax(int stat, int value) {
 if (stat >= STAT_st && stat < STAT_max_stat) StatNPC[stat].max = value;
}

void StatsReset() {
 for (int stat = STAT_st; stat < STAT_max_stat; stat++) {
  StatPC[stat].min = StatNPC[stat].min = *(DWORD*)(_stat_data + 12 + stat*24);
  StatPC[stat].max = StatNPC[stat].max = *(DWORD*)(_stat_data + 16 + stat*24);
 }
 StandardApAcBonus = 4;
 ExtraApAcBonus = 4;
}

static void __declspec(naked) stat_level_hook1() {
 __asm {
  mov  edx, [ebx+0x40]                      // pobj.curr_mp
  test edi, edi
  jz   skip
  mov  eax, ExtraApAcBonus
  push edx
  imul eax, edx
  pop  edx
  xchg edi, eax
skip:
  mov  eax, StandardApAcBonus
  imul eax, edx
  add  eax, edi
  shr  eax, 2                               // eax/4
  mov  edi, 0x4AF0A4
  jmp  edi
 }
}

static void __declspec(naked) statPcMinExpForLevel_hook() {
 __asm {
  dec  eax
  mov  eax, [xpTable+eax*4]
  retn
 }
}

static StatFormula StatFormulas[STAT_max_derived+1];
static int StatShifts[STAT_max_derived+1][STAT_lu+1];
static double StatMulti[STAT_max_derived+1][STAT_lu+1];

static void _stdcall StatRecalcDerived(int* proto, DWORD* critter) {
 int basestats[STAT_lu+1];
 for (int stat = STAT_st; stat <= STAT_lu; stat++) {
  basestats[stat] = stat_level(critter, stat);
 }
 for (int i = STAT_max_hit_points; i <= STAT_max_derived; i++) {
  if (i >= STAT_dmg_thresh && i <= STAT_dmg_resist_explosion) continue;
  double sum = 0;
  for (int j = STAT_st; j <= STAT_lu; j++) {
   sum += (basestats[j] + StatShifts[i][j]) * StatMulti[i][j];
  }
  proto[i+9] = StatFormulas[i].base + (int)floor(sum);
  if (proto[i+9] < StatFormulas[i].min) proto[i+9] = StatFormulas[i].min;
 }
}

static void __declspec(naked) stat_recalc_derived_hook() {
 __asm {
  pushad
  sub  esp, 4
  mov  edx, esp
  push eax
  push edx
  mov  eax, [eax+0x64]                      // eax = pid
  call proto_ptr_
  pop  edx
  push [edx]
  call StatRecalcDerived
  add  esp, 4
  popad
  retn
 }
}

void StatsInit() {

// Минимальный возраст игрока
 for (int i = 0; i < sizeof(AgeMin)/4; i++) SafeWrite8(AgeMin[i], 8);

// Максимальный возраст игрока
 for (int i = 0; i < sizeof(AgeMax)/4; i++) SafeWrite8(AgeMax[i], 60);

 int tmp = GetPrivateProfileIntA("Misc", "CarryWeightLimit", -1, ini);
 if (tmp > 0) SafeWrite32(0x51D66C, tmp);

 StatsReset();

 MakeCall(0x4AF3AF, &stat_level_hook, true);
 MakeCall(0x4AF56A, &stat_set_base_hook, true);

 MakeCall(0x4AF09C, &stat_level_hook1, true);

 char table[2048];
 if (GetPrivateProfileStringA("Misc", "XPTable", "", table, 2048, ini) > 0) {
  DWORD level = 0;
  char *xpStr = strtok(table, ",");
  while (xpStr && level < 99) {
   xpTable[level++] = atoi(xpStr);
   xpStr = strtok(0, ",");
  }
  for (int i = level; i < 99; i++) xpTable[i] = -1;
  SafeWrite8(0x4AFB1B, (BYTE)(level+1));
  MakeCall(0x4AF9A8, &statPcMinExpForLevel_hook, true);  
 }

 if (GetPrivateProfileStringA("Misc", "DerivedStats", "", table, 2048, ini) > 0) {
  memset(StatFormulas, 0, sizeof(StatFormulas));
  memset(StatShifts, 0, sizeof(StatShifts));
  memset(StatMulti, 0, sizeof(StatMulti));

// STAT_st + STAT_en * 2 + 15
  StatFormulas[STAT_max_hit_points].base = 15;// max hp
  StatMulti[STAT_max_hit_points][STAT_st] = 1;
  StatMulti[STAT_max_hit_points][STAT_en] = 2;

// STAT_ag / 2 + 5
  StatFormulas[STAT_max_move_points].base = 5;// max ap
  StatMulti[STAT_max_move_points][STAT_ag] = 0.5;

// STAT_ag
  StatMulti[STAT_ac][STAT_ag] = 1;          // ac

// STAT_st - 5
  StatFormulas[STAT_melee_dmg].min = 1;     // melee damage
  StatShifts[STAT_melee_dmg][STAT_st] = -5;
  StatMulti[STAT_melee_dmg][STAT_st] = 1;

// STAT_st * 25 + 25
  StatFormulas[STAT_carry_amt].base = 25;   // carry weight
  StatMulti[STAT_carry_amt][STAT_st] = 25;

// STAT_pe * 2
  StatMulti[STAT_sequence][STAT_pe] = 2;    // sequence

// STAT_en / 3
  StatFormulas[STAT_heal_rate].min = 1;     // heal rate
  StatMulti[STAT_heal_rate][STAT_en] = 1.0/3.0;

// STAT_lu
  StatMulti[STAT_crit_chance][STAT_lu] = 1; // critical chance

// STAT_en * 2
  StatMulti[STAT_rad_resist][STAT_en] = 2;  // rad resist

// STAT_en * 5
  StatMulti[STAT_poison_resist][STAT_en] = 5;// poison resist

  char key[6], buf2[256], buf3[256];
  strcpy(buf3, table);
  sprintf(table, ".\\%s", buf3);
  for (int i = STAT_max_hit_points; i <= STAT_max_derived; i++) {
   if (i >= STAT_dmg_thresh && i <= STAT_dmg_resist_explosion) continue;
   _itoa(i, key, 10);
   StatFormulas[i].base = GetPrivateProfileInt(key, "base", StatFormulas[i].base, table);
   StatFormulas[i].min = GetPrivateProfileInt(key, "min", StatFormulas[i].min, table);
   for (int j = STAT_st; j <= STAT_lu; j++) {
    sprintf(buf2, "shift%d", j);
    StatShifts[i][j] = GetPrivateProfileInt(key, buf2, StatShifts[i][j], table);
    sprintf(buf2, "multi%d", j);
    _gcvt(StatMulti[i][j], 16, buf3);
    GetPrivateProfileStringA(key, buf2, buf3, buf2, 256, table);
    StatMulti[i][j] = atof(buf2);
   }
  }
  MakeCall(0x4AF6FC, &stat_recalc_derived_hook, true);
 }
}
