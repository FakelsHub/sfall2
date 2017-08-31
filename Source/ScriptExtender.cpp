/*
 * sfall
 * Copyright (C) 2008, 2009, 2010, 2011, 2012  The sfall team
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "main.h"

#include <hash_map>
#include <set>
#include <string>
#include "Arrays.h"
#include "BarBoxes.h"
#include "Console.h"
#include "Define.h"
#include "Explosions.h"
#include "FalloutEngine.h"
#include "HookScripts.h"
#include "input.h"
#include "LoadGameHook.h"
#include "Logging.h"
#include "ScriptExtender.h"
#include "version.h"

static void* opTable[maxOpcodes];

void _stdcall HandleMapUpdateForScripts(DWORD procId);

// variables for new opcodes
static DWORD opArgCount = 0;
static DWORD opArgs[5];
static DWORD opArgTypes[5];
static DWORD opRet;
static DWORD opRetType;

static void _stdcall SetOpReturn(DWORD value, DWORD type) {
 opRet = value;
 opRetType = type;
}

static void _stdcall SetOpReturn(int value) {
 SetOpReturn((DWORD)value, DATATYPE_INT);
}

static void _stdcall SetOpReturn(float value) {
 SetOpReturn(*(DWORD*)&value, DATATYPE_FLOAT);
}

static void _stdcall SetOpReturn(const char* value) {
 SetOpReturn((DWORD)value, DATATYPE_STR);
}

static bool _stdcall IsOpArgInt(int num) {
 return (opArgTypes[num] == DATATYPE_INT);
}

static bool _stdcall IsOpArgFloat(int num) {
 return (opArgTypes[num] == DATATYPE_FLOAT);
}

static bool _stdcall IsOpArgStr(int num) {
 return (opArgTypes[num] == DATATYPE_STR);
}

static int _stdcall GetOpArgInt(int num) {
 switch (opArgTypes[num]) {
 case DATATYPE_FLOAT:
  return (int)*(float*)&opArgs[num];
 case DATATYPE_INT:
  return (int)opArgs[num];
 default:
  return 0;
 }
}

static float _stdcall GetOpArgFloat(int num) {
 switch (opArgTypes[num]) {
 case DATATYPE_FLOAT:
  return *(float*)&opArgs[num];
 case DATATYPE_INT:
  return (float)(int)opArgs[num];
 default:
  return 0.0;
 }
}

static const char* _stdcall GetOpArgStr(int num) {
 return (opArgTypes[num] == DATATYPE_STR) 
  ? (const char*)opArgs[num]
  : "";
}

#include "ScriptOps\AsmMacros.h"
#include "ScriptOps\ScriptArrays.hpp"
#include "ScriptOps\ScriptUtils.hpp"

#include "ScriptOps\WorldmapOps.hpp"
#include "ScriptOps\PerksOp.hpp"
#include "ScriptOps\MemoryOp.hpp"
#include "ScriptOps\StatsOp.hpp"
#include "ScriptOps\InterfaceOp.hpp"
#include "ScriptOps\GraphicsOp.hpp"
#include "ScriptOps\FileSystemOps.hpp"
#include "ScriptOps\ObjectsOps.hpp"
#include "ScriptOps\AnimOps.hpp"
#include "ScriptOps\MiscOps.hpp"

typedef void (_stdcall *regOpcodeProc)(WORD opcode,void* ptr);

static int idle;

struct sGlobalScript {
 sScriptProgram prog;
 DWORD count;
 DWORD repeat;
 DWORD mode; //0 - local map loop, 1 - input loop, 2 - world map loop, 3 - local and world map loops

 sGlobalScript() {}
 sGlobalScript(sScriptProgram script) {
  prog=script;
  count=0;
  repeat=0;
  mode=0;
 }
};

struct sExportedVar {
 DWORD type; // in scripting engine terms, eg. VAR_TYPE_*
 DWORD val;  
 sExportedVar() : val(0), type(VAR_TYPE_INT) {}
};

static std::vector<DWORD> checkedScripts;
static std::vector<sGlobalScript> globalScripts;
// a map of all sfall programs (global and hook scripts) by thier scriptPtr
typedef stdext::hash_map<DWORD, sScriptProgram> SfallProgsMap;
static SfallProgsMap sfallProgsMap;
// a map scriptPtr => self_obj  to override self_obj for all script types using set_self
stdext::hash_map<DWORD, TGameObj*> selfOverrideMap;

typedef std::tr1::unordered_map<std::string, sExportedVar> ExportedVarsMap;
static ExportedVarsMap globalExportedVars;
DWORD isLoadingGlobalScript = 0;

stdext::hash_map<__int64, int> globalVars;
typedef stdext::hash_map<__int64, int> :: iterator glob_itr;
typedef stdext::hash_map<__int64, int> :: const_iterator glob_citr;
typedef std::pair<__int64, int> glob_pair;

DWORD AvailableGlobalScriptTypes=0;
bool isGameLoading;

TScript OverrideScriptStruct;

//eax contains the script pointer, edx contains the opcode*4

//To read a value, mov the script pointer to eax, call interpretPopShort_, eax now contains the value type
//mov the script pointer to eax, call interpretPopLong_, eax now contains the value

//To return a value, move it to edx, mov the script pointer to eax, call interpretPushLong_
//mov the value type to edx, mov the script pointer to eax, call interpretPushShort_

static void _stdcall SetGlobalScriptRepeat2(DWORD script, DWORD frames) {
 for(DWORD d=0;d<globalScripts.size();d++) {
  if(globalScripts[d].prog.ptr==script) {
   if(frames==0xFFFFFFFF) globalScripts[d].mode=!globalScripts[d].mode;
   else globalScripts[d].repeat=frames;
   break;
  }
 }
}
static void __declspec(naked) SetGlobalScriptRepeat() {
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
  push eax;
  push ecx;
  call SetGlobalScriptRepeat2;
end:
  pop edx;
  pop ecx;
  pop ebx;
  retn;
 }
}
static void _stdcall SetGlobalScriptType2(DWORD script, DWORD type) {
 if(type>3) return;
 for(DWORD d=0;d<globalScripts.size();d++) {
  if(globalScripts[d].prog.ptr==script) {
   globalScripts[d].mode=type;
   break;
  }
 }
}
static void __declspec(naked) SetGlobalScriptType() {
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
  push eax;
  push ecx;
  call SetGlobalScriptType2;
end:
  pop edx;
  pop ecx;
  pop ebx;
  retn;
 }
}
static void __declspec(naked) GetGlobalScriptTypes() {
 __asm {
  push ebx;
  push ecx;
  push edx;
  mov edx, AvailableGlobalScriptTypes;
  mov ecx, eax;
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
static void SetGlobalVarInternal(__int64 var, int val) {
 glob_itr itr=globalVars.find(var);
 if (itr==globalVars.end()) globalVars.insert(glob_pair(var, val));
 else {
  if (val == 0) globalVars.erase(itr); // applies for both float 0.0 and integer 0
  else itr->second=val;
 }
}
static void _stdcall SetGlobalVar2(const char* var, int val) {
 if(strlen(var)!=8) return;
 SetGlobalVarInternal(*(__int64*)var, val);
}
static void _stdcall SetGlobalVar2Int(DWORD var, int val) {
 SetGlobalVarInternal(var, val);
}
static void __declspec(naked) SetGlobalVar() {
 __asm {
  push ebx;
  push ecx;
  push edx;
  push edi;
  push esi;
  mov edi, eax;
  call interpretPopShort_
  mov eax, edi;
  call interpretPopLong_
  mov esi, eax;
  mov eax, edi;
  call interpretPopShort_
  mov edx, eax;
  mov eax, edi;
  call interpretPopLong_
  cmp dx, 0x9001;
  jz next;
  cmp dx, 0x9801;
  jz next;
  cmp dx, 0xc001;
  jnz end;
  push esi;
  push eax;
  call SetGlobalVar2Int;
  jmp end;
next:
  mov ebx, eax;
  mov eax, edi;
  call interpretGetString_
  push esi;
  push eax;
  call SetGlobalVar2;
end:
  pop esi;
  pop edi;
  pop edx;
  pop ecx;
  pop ebx;
  retn;
 }
}
static DWORD GetGlobalVarInternal(__int64 val) {
 glob_citr itr=globalVars.find(val);
 if(itr==globalVars.end()) return 0;
 else return itr->second;
}
static DWORD _stdcall GetGlobalVar2(const char* var) {
 if(strlen(var)!=8) return 0;
 return GetGlobalVarInternal(*(__int64*)var);
}
static DWORD _stdcall GetGlobalVar2Int(DWORD var) {
 return GetGlobalVarInternal(var);
}
static void __declspec(naked) GetGlobalVarInt() {
 __asm {
  push ebx;
  push ecx;
  push edx;
  push edi;
  push esi;
  xor edx, edx;
  mov edi, eax;
  call interpretPopShort_
  mov esi, eax;
  mov eax, edi;
  call interpretPopLong_
  cmp si, 0x9001;
  jz next;
  cmp si, 0x9801;
  jz next;
  cmp si, 0xc001;
  jnz end;
  push eax;
  call GetGlobalVar2Int;
  mov edx, eax;
  jmp end;
next:
  mov edx, esi;
  mov ebx, eax;
  mov eax, edi;
  call interpretGetString_
  push eax;
  call GetGlobalVar2;
  mov edx, eax;
end:
  mov eax, edi;
  call interpretPushLong_
  mov edx, 0xc001;
  mov eax, edi;
  call interpretPushShort_
  pop esi;
  pop edi;
  pop edx;
  pop ecx;
  pop ebx;
  retn;
 }
}
static void __declspec(naked) GetGlobalVarFloat() {
 __asm {
  push ebx;
  push ecx;
  push edx;
  push edi;
  push esi;
  xor edx, edx;
  mov edi, eax;
  call interpretPopShort_
  mov esi, eax;
  mov eax, edi;
  call interpretPopLong_
  cmp si, 0x9001;
  jz next;
  cmp si, 0x9801;
  jz next;
  cmp si, 0xc001;
  jnz end;
  push eax;
  call GetGlobalVar2Int;
  mov edx, eax;
  jmp end;
next:
  mov edx, esi;
  mov ebx, eax;
  mov eax, edi;
  call interpretGetString_
  push eax;
  call GetGlobalVar2;
  mov edx, eax;
end:
  mov eax, edi;
  call interpretPushLong_
  mov edx, 0xa001;
  mov eax, edi;
  call interpretPushShort_
  pop esi;
  pop edi;
  pop edx;
  pop ecx;
  pop ebx;
  retn;
 }
}

static void __declspec(naked) GetSfallArg() {
 __asm {
  pushad;
  push eax;
  call GetHSArg;
  pop ecx;
  mov edx, eax;
  mov eax, ecx;
  call interpretPushLong_
  mov eax, ecx;
  mov edx, 0xc001;
  call interpretPushShort_
  popad;
  retn;
 }
}
static DWORD _stdcall GetSfallArgs2() {
 DWORD argCount=GetHSArgCount();
 DWORD id=TempArray(argCount, 4);
 DWORD* args = GetHSArgs();
 for(DWORD i=0;i<argCount;i++) arrays[id].val[i].set(*(long*)&args[i]);
 return id;
}
static void __declspec(naked) GetSfallArgs() {
 __asm {
  pushad;
  push eax;
  call GetSfallArgs2;
  pop ecx;
  mov edx, eax;
  mov eax, ecx;
  call interpretPushLong_
  mov eax, ecx;
  mov edx, 0xc001;
  call interpretPushShort_
  popad;
  retn;
 }
}
static void __declspec(naked) SetSfallArg() {
 __asm {
  pushad;
  mov ecx, eax;
  call interpretPopShort_
  mov edi, eax;
  mov eax, ecx;
  call interpretPopLong_
  mov edx, eax;
  mov eax, ecx;
  call interpretPopShort_
  mov esi, eax;
  mov eax, ecx;
  call interpretPopLong_
  cmp di, 0xc001;
  jnz end;
  cmp si, 0xc001;
  jnz end;
  push edx;
  push eax;
  call SetHSArg;
end:
  popad;
  retn;
 }
}
static void __declspec(naked) SetSfallReturn() {
 __asm {
  push ebx;
  push ecx;
  push edx;
  mov ecx, eax;
  call interpretPopShort_
  mov edx, eax;
  mov eax, ecx;
  call interpretPopLong_
  cmp dx, 0xc001;
  jnz end;
  push eax;
  call SetHSReturn;
end:
  pop edx;
  pop ecx;
  pop ebx;
  retn;
 }
}
static void __declspec(naked) InitHook() {
 __asm {
  push ecx;
  push edx;
  mov ecx, eax;
  mov edx, InitingHookScripts;
  call interpretPushLong_
  mov eax, ecx;
  mov edx, 0xc001;
  call interpretPushShort_
  pop edx;
  pop ecx;
  retn;
 }
}

static void _stdcall set_self2(DWORD script, TGameObj* obj) {
 if (obj) selfOverrideMap[script] = obj;
 else {
  stdext::hash_map<DWORD, TGameObj*>::iterator it = selfOverrideMap.find(script);
  if (it != selfOverrideMap.end()) selfOverrideMap.erase(it);
 }
}

static void __declspec(naked) set_self() {
 __asm {
  pushad;
  mov ebp, eax;
  call interpretPopShort_
  mov edi, eax;
  mov eax, ebp;
  call interpretPopLong_
  cmp di, 0xc001;
  jnz end;
  push eax;
  push ebp;
  call set_self2;
end:
  popad;
  retn;
 }
}
static void __declspec(naked) register_hook() {
 __asm {
  pushad;
  mov ebp, eax;
  call interpretPopShort_
  mov edi, eax;
  mov eax, ebp;
  call interpretPopLong_
  cmp di, 0xc001;
  jnz end;
  push -1;
  push eax;
  push ebp;
  call RegisterHook;
end:
  popad;
  retn;
 }
}

static void __declspec(naked) register_hook_proc() {
 _OP_BEGIN(ebp)
 _GET_ARG_R32(ebp, ecx, esi)
 _GET_ARG_R32(ebp, ebx, edi)
 _CHECK_ARG_INT(cx, fail)
 _CHECK_ARG_INT(bx, fail)
 __asm {
  push esi;
  push edi;
  push ebp;
  call RegisterHook;
 fail:
 }
 _OP_END
}

static void __declspec(naked) sfall_ver() {
 __asm {
  call interpretPushLong_
  pop  eax
  mov  edx, VAR_TYPE_INT
  call interpretPushShort_
  pop  edx
  retn
 }
}

static void __declspec(naked) sfall_ver_major() {
 __asm {
  push edx
  push eax
  mov  edx, VERSION_MAJOR
  jmp  sfall_ver
 }
}

static void __declspec(naked) sfall_ver_minor() {
 __asm {
  push edx
  push eax
  mov  edx, VERSION_MINOR
  jmp  sfall_ver
 }
}

static void __declspec(naked) sfall_ver_build() {
 __asm {
  push edx
  push eax
  mov  edx, 509
  jmp  sfall_ver
 }
}

//END OF SCRIPT FUNCTIONS

DWORD _stdcall FindSidHook2(DWORD script) {
 stdext::hash_map<DWORD, TGameObj*>::iterator overrideIt = selfOverrideMap.find(script);
 if (overrideIt != selfOverrideMap.end()) {
  DWORD scriptId = overrideIt->second->scriptID;
  if (scriptId != -1) {
   selfOverrideMap.erase(overrideIt);
   return scriptId; // returns the real scriptId of object if it is scripted
  }
  OverrideScriptStruct.self_obj = overrideIt->second;
  OverrideScriptStruct.target_obj = overrideIt->second;
  selfOverrideMap.erase(overrideIt); // this reverts self_obj back to original value for next function calls
  return -2; // override struct
 }
 // this will allow to use functions like roll_vs_skill, etc without calling set_self (they don't really need self object)
 if (sfallProgsMap.find(script) != sfallProgsMap.end()) { 
  OverrideScriptStruct.target_obj = OverrideScriptStruct.self_obj = 0;
  return -2; // override struct
 }
 return -1; // change nothing
}

static void __declspec(naked) FindSidHook() {
 __asm {
  sub esp, 4;
  pushad;
  push eax;
  call FindSidHook2;
  mov [esp+32], eax; // to save value after popad
  popad;
  add esp, 4;
  cmp [esp-4], -1;
  jz end;
  cmp [esp-4], -2;
  jz override_script;
  mov eax, [esp-4];
  retn;
override_script:
  test edx, edx;
  jz end;
  lea eax, OverrideScriptStruct;
  mov [edx], eax;
  mov eax, -2;
  retn;
end:
  push ebx;
  push ecx;
  push edx;
  push esi;
  push ebp;
  push 0x4A3911
  retn
 }
}
static void __declspec(naked) ScrPtrHook() {
 __asm {
  cmp eax, -2;
  jnz end;
  xor eax, eax;
  retn;
end:
  push ebx;
  push ecx;
  push esi;
  push edi;
  push ebp;
  push 0x4A5E39
  retn
 }
}

static void __declspec(naked) AfterCombatAttackHook() {
 __asm {
  pushad;
  call AfterAttackCleanup;
  popad;
  xor  eax, eax
  inc  eax
  push 0x4230DA
  retn
 }
}
static void __declspec(naked) ExecMapScriptsHook() {
 __asm {
  sub esp, 32;
  mov [esp+12], eax;
  pushad;
  push eax; // int procId
  call HandleMapUpdateForScripts;
  popad;
  push 0x4A67F9                             // jump back
  retn
 }
}
static DWORD __stdcall GetGlobalExportedVarPtr(const char* name) {
 std::string str(name);
 ExportedVarsMap::iterator it = globalExportedVars.find(str);
 //dlog_f("\n Trying to find exported var %s... ", DL_MAIN, name);
 if (it != globalExportedVars.end()) {
  sExportedVar *ptr = &it->second;
  return (DWORD)ptr;
 }
 return 0;
}
static DWORD __stdcall CreateGlobalExportedVar(DWORD scr, const char* name) {
 //dlog_f("\nTrying to export variable %s (%d)\r\n", DL_MAIN, name, isLoadingGlobalScript);
 std::string str(name);
 globalExportedVars[str] = sExportedVar(); // add new
 return 1;
}
/* 
 when fetching/storing into exported variables, first search in our own hash table instead, then (if not found), resume normal search

 reason for this: game frees all scripts and exported variables from memory when you exit map
 so if you create exported variable in sfall global script, it will work until you exit map, after that you will get crashes

 with this you can still use variables exported from global scripts even between map changes (per global scripts logic)
*/
static void __declspec(naked) Export_FetchOrStore_FindVar_Hook() {
 __asm {
  push eax;
  push edx;
  push ecx;
  push ebx;
  push edx // char* varName
  call GetGlobalExportedVarPtr;
  test eax, eax
  jz proceedNormal;
  pop ebx;
  pop ecx;
  pop edx;
  pop edx;
  sub eax, 0x24; // adjust pointer for the code
  retn;

 proceedNormal:
  pop ebx;
  pop ecx;
  pop edx;
  pop eax;
  call findVar_
  retn
 }
}
static const DWORD Export_Export_FindVar_back1 = 0x4414CD;
static const DWORD Export_Export_FindVar_back2 = 0x4414AC;
static void __declspec(naked) Export_Export_FindVar_Hook() {
 __asm {
  pushad;
  mov eax, isLoadingGlobalScript;
  test eax, eax;
  jz proceedNormal;
  push edx; // char* - var name
  push ebp; // script ptr
  call CreateGlobalExportedVar;
  popad;
  jmp Export_Export_FindVar_back2; // if sfall exported var, jump to the end of function

proceedNormal:
  popad;
  call findVar_  // else - proceed normal
  jmp Export_Export_FindVar_back1;
 }
}

// this hook prevents sfall scripts from being removed after switching to another map, since normal script engine re-loads completely
static void _stdcall FreeProgramHook2(DWORD progPtr) {
 if (isGameLoading || (sfallProgsMap.find(progPtr) == sfallProgsMap.end())) { // only delete non-sfall scripts or when actually loading the game
  __asm {
   mov eax, progPtr;
   call interpretFreeProgram_
  }
 }
}

static void __declspec(naked) FreeProgramHook() {
 __asm {
  pushad;
  push eax;
  call FreeProgramHook2;
  popad;
  retn;
 }
}

static void __declspec(naked) combat_begin_hook() {
 __asm {
  push eax
  call scr_set_ext_param_
  pop  eax                                  // pobj.sid
  mov  edx, combat_is_starting_p_proc
  jmp  exec_script_proc_
 }
}

static void __declspec(naked) combat_over_hook() {
 __asm {
  push eax
  call scr_set_ext_param_
  pop  eax                                  // pobj.sid
  mov  edx, combat_is_over_p_proc
  jmp  exec_script_proc_
 }
}

void ScriptExtenderSetup() {

 dlogr("Adding additional opcodes", DL_SCRIPT);
 dlogr("Unsafe opcodes enabled", DL_SCRIPT);

 SafeWrite32(0x46E370, maxOpcodes);         // Maximum number of allowed opcodes
 SafeWrite32(0x46CE34, (DWORD)opTable);     // cmp check to make sure opcode exists
 SafeWrite32(0x46CE6C, (DWORD)opTable);     // call that actually jumps to the opcode
 SafeWrite32(0x46E390, (DWORD)opTable);     // mov that writes to the opcode

 opTable[0x156]=ReadByte;
 opTable[0x157]=ReadShort;
 opTable[0x158]=ReadInt;
 opTable[0x159]=ReadString;
 opTable[0x15a]=SetPCBaseStat;
 opTable[0x15b]=SetPCExtraStat;
 opTable[0x15c]=GetPCBaseStat;
 opTable[0x15d]=GetPCExtraStat;
 opTable[0x15e]=SetCritterBaseStat;
 opTable[0x15f]=SetCritterExtraStat;
 opTable[0x160]=GetCritterBaseStat;
 opTable[0x161]=GetCritterExtraStat;
 opTable[0x162]=funcTapKey;
 opTable[0x163]=GetYear;
 opTable[0x164]=GameLoaded;
 opTable[0x165]=GraphicsFuncsAvailable;
 opTable[0x166]=funcLoadShader;
 opTable[0x167]=funcFreeShader;
 opTable[0x168]=funcActivateShader;
 opTable[0x169]=funcDeactivateShader;
 opTable[0x16a]=SetGlobalScriptRepeat;
 opTable[0x16b]=InputFuncsAvailable;
 opTable[0x16c]=KeyPressed;
 opTable[0x16d]=funcSetShaderInt;
 opTable[0x16e]=funcSetShaderFloat;
 opTable[0x16f]=funcSetShaderVector;
 opTable[0x170]=in_world_map;
 opTable[0x171]=ForceEncounter;
 opTable[0x172]=SetWorldMapPos;
 opTable[0x173]=GetWorldMapXPos;
 opTable[0x174]=GetWorldMapYPos;
 opTable[0x175]=SetDMModel;
 opTable[0x176]=SetDFModel;
 opTable[0x177]=SetMoviePath;
 for(int i = 0x178; i < 0x189; i++) opTable[i] = funcSetPerkValue;
 opTable[0x189]=funcSetPerkName;
 opTable[0x18a]=funcSetPerkDesc;
 opTable[0x18b]=SetPipBoyAvailable;
 if (usingExtraKillTypes) {
  opTable[0x18c]=GetKillCounter2;
  opTable[0x18d]=ModKillCounter2;
 } else {
  opTable[0x18c]=GetKillCounter;
  opTable[0x18d]=ModKillCounter;
 }
 opTable[0x18e]=GetPerkOwed;
 opTable[0x18f]=SetPerkOwed;
 opTable[0x190]=GetPerkAvailable;
 opTable[0x191]=GetCritterAP;
 opTable[0x192]=SetCritterAP;
 opTable[0x193]=GetActiveHand;
 opTable[0x194]=ToggleActiveHand;
 opTable[0x195]=SetWeaponKnockback;
 opTable[0x196]=SetTargetKnockback;
 opTable[0x197]=SetAttackerKnockback;
 opTable[0x198]=RemoveWeaponKnockback;
 opTable[0x199]=RemoveTargetKnockback;
 opTable[0x19a]=RemoveAttackerKnockback;
 opTable[0x19b]=SetGlobalScriptType;
 opTable[0x19c]=GetGlobalScriptTypes;
 opTable[0x19d]=SetGlobalVar;
 opTable[0x19e]=GetGlobalVarInt;
 opTable[0x19f]=GetGlobalVarFloat;
 opTable[0x1a0]=fSetPickpocketMax;
 opTable[0x1a1]=fSetHitChanceMax;
 opTable[0x1a2]=fSetSkillMax;
 opTable[0x1a3]=EaxAvailable;
 //opTable[0x1a4]=SetEaxEnvironmentFunc;
 opTable[0x1a5]=IncNPCLevel;
 opTable[0x1a6]=GetViewportX;
 opTable[0x1a7]=GetViewportY;
 opTable[0x1a8]=SetViewportX;
 opTable[0x1a9]=SetViewportY;
 opTable[0x1aa]=SetXpMod;
 opTable[0x1ab]=SetPerkLevelMod;
 opTable[0x1ac]=GetIniSetting;
 opTable[0x1ad]=funcGetShaderVersion;
 opTable[0x1ae]=funcSetShaderMode;
 opTable[0x1af]=get_game_mode;
 opTable[0x1b0]=funcForceGraphicsRefresh;
 opTable[0x1b1]=funcGetShaderTexture;
 opTable[0x1b2]=funcSetShaderTexture;
 opTable[0x1b3]=funcGetTickCount;
 opTable[0x1b4]=SetStatMax;
 opTable[0x1b5]=SetStatMin;
 opTable[0x1b6]=SetCarTown;
 opTable[0x1b7]=fSetPCStatMax;
 opTable[0x1b8]=fSetPCStatMin;
 opTable[0x1b9]=fSetNPCStatMax;
 opTable[0x1ba]=fSetNPCStatMin;
 opTable[0x1bb]=fSetFakePerk;
 opTable[0x1bc]=fSetFakeTrait;
 opTable[0x1bd]=fSetSelectablePerk;
 opTable[0x1be]=fSetPerkboxTitle;
 opTable[0x1bf]=fIgnoreDefaultPerks;
 opTable[0x1c0]=fRestoreDefaultPerks;
 opTable[0x1c1]=fHasFakePerk;
 opTable[0x1c2]=fHasFakeTrait;
 opTable[0x1c3]=fAddPerkMode;
 opTable[0x1c4]=fClearSelectablePerks;
 opTable[0x1c5]=SetCritterHitChance;
 opTable[0x1c6]=SetBaseHitChance;
 opTable[0x1c7]=SetCritterSkillMod;
 opTable[0x1c8]=SetBaseSkillMod;
 opTable[0x1c9]=SetCritterPickpocket;
 opTable[0x1ca]=SetBasePickpocket;
 opTable[0x1cb]=SetPyromaniacMod;
 opTable[0x1cc]=fApplyHeaveHoFix;
 opTable[0x1cd]=SetSwiftLearnerMod;
 opTable[0x1ce]=SetLevelHPMod;
 opTable[0x1cf]=WriteByte;
 opTable[0x1d0]=WriteShort;
 opTable[0x1d1]=WriteInt;
 for (int i = 0x1d2; i < 0x1dc; i++) opTable[i] = CallOffset;
 opTable[0x1dc]=ShowIfaceTag;
 opTable[0x1dd]=HideIfaceTag;
 opTable[0x1de]=IsIfaceTagActive;
 opTable[0x1df]=GetBodypartHitModifier;
 opTable[0x1e0]=SetBodypartHitModifier;
 opTable[0x1e1]=funcSetCriticalTable;
 opTable[0x1e2]=funcGetCriticalTable;
 opTable[0x1e3]=funcResetCriticalTable;
 opTable[0x1e4]=GetSfallArg;
 opTable[0x1e5]=SetSfallReturn;
 opTable[0x1e6]=SetApAcBonus;
 opTable[0x1e7]=GetApAcBonus;
 opTable[0x1e8]=SetApAcEBonus;
 opTable[0x1e9]=GetApAcEBonus;
 opTable[0x1ea]=InitHook;
 opTable[0x1eb]=GetIniString;
 opTable[0x1ec]=funcSqrt;
 opTable[0x1ed]=funcAbs;
 opTable[0x1ee]=funcSin;
 opTable[0x1ef]=funcCos;
 opTable[0x1f0]=funcTan;
 opTable[0x1f1]=funcATan;
 opTable[0x1f2]=SetPalette;
 opTable[0x1f3]=RemoveScript;
 opTable[0x1f4]=SetScript;
 opTable[0x1f5]=GetScript;
 opTable[0x1f6]=NBCreateChar;
 opTable[0x1f7]=fs_create;
 opTable[0x1f8]=fs_copy;
 opTable[0x1f9]=fs_find;
 opTable[0x1fa]=fs_write_byte;
 opTable[0x1fb]=fs_write_short;
 opTable[0x1fc]=fs_write_int;
 opTable[0x1fd]=fs_write_int;
 opTable[0x1fe]=fs_write_string;
 opTable[0x1ff]=fs_delete;
 opTable[0x200]=fs_size;
 opTable[0x201]=fs_pos;
 opTable[0x202]=fs_seek;
 opTable[0x203]=fs_resize;
 opTable[0x204]=get_proto_data;
 opTable[0x205]=set_proto_data;
 opTable[0x206]=set_self;
 opTable[0x207]=register_hook;
 opTable[0x208]=fs_write_bstring;
 opTable[0x209]=fs_read_byte;
 opTable[0x20a]=fs_read_short;
 opTable[0x20b]=fs_read_int;
 opTable[0x20c]=fs_read_float;
 opTable[0x20d]=list_begin;
 opTable[0x20e]=list_next;
 opTable[0x20f]=list_end;
 opTable[0x210]=sfall_ver_major;
 opTable[0x211]=sfall_ver_minor;
 opTable[0x212]=sfall_ver_build;
 opTable[0x213]=funcHeroSelectWin;
 opTable[0x214]=funcSetHeroRace;
 opTable[0x215]=funcSetHeroStyle;
 opTable[0x216]=set_critter_burst_disable;
 opTable[0x217]=get_weapon_ammo_pid;
 opTable[0x218]=set_weapon_ammo_pid;
 opTable[0x219]=get_weapon_ammo_count;
 opTable[0x21a]=set_weapon_ammo_count;
 opTable[0x21b]=WriteString;
 opTable[0x21c]=get_mouse_x;
 opTable[0x21d]=get_mouse_y;
 opTable[0x21e]=get_mouse_buttons;
 opTable[0x21f]=get_window_under_mouse;
 opTable[0x220]=get_screen_width;
 opTable[0x221]=get_screen_height;
 opTable[0x222] = (void*)map_disable_bk_processes_;// Stop game
 opTable[0x223] = (void*)map_enable_bk_processes_;// Resume the game when it is stopped
 opTable[0x224]=create_message_window;
 opTable[0x225]=remove_trait;
 opTable[0x226]=get_light_level;
 opTable[0x227]=refresh_pc_art;
 opTable[0x228]=get_attack_type;
 opTable[0x229]=ForceEncounterWithFlags;
 opTable[0x22a]=set_map_time_multi;
 opTable[0x22b]=play_sfall_sound;
 opTable[0x22c]=stop_sfall_sound;
 opTable[0x22d]=create_array;
 opTable[0x22e]=set_array;
 opTable[0x22f]=get_array;
 opTable[0x230]=free_array;
 opTable[0x231]=len_array;
 opTable[0x232]=resize_array;
 opTable[0x233]=temp_array;
 opTable[0x234]=fix_array;
 opTable[0x235]=string_split;
 opTable[0x236]=list_as_array;
 opTable[0x237]=str_to_int;
 opTable[0x238]=str_to_flt;
 opTable[0x239]=scan_array;
 opTable[0x23a]=get_tile_pid;
 opTable[0x23b]=modified_ini;
 opTable[0x23c]=GetSfallArgs;
 opTable[0x23d]=SetSfallArg;
 opTable[0x23e]=force_aimed_shots;
 opTable[0x23f]=disable_aimed_shots;
 opTable[0x240]=mark_movie_played;
 opTable[0x241]=get_npc_level;
 opTable[0x242]=set_critter_skill_points;
 opTable[0x243]=get_critter_skill_points;
 opTable[0x244]=set_available_skill_points;
 opTable[0x245]=get_available_skill_points;
 opTable[0x246]=mod_skill_points_per_level;
 opTable[0x247]=set_perk_freq;
 opTable[0x248]=get_last_attacker;
 opTable[0x249]=get_last_target;
 opTable[0x24a]=block_combat;
 opTable[0x24b]=tile_under_cursor;
 opTable[0x24c]=gdialog_get_barter_mod;
 opTable[0x24d]=set_inven_ap_cost;
 opTable[0x24e]=op_substr;
 opTable[0x24f]=op_strlen;
 opTable[0x250]=op_sprintf;
 opTable[0x251]=op_ord;
 // opTable[0x252]=  RESERVED
 opTable[0x253]=op_typeof;
 opTable[0x254]=op_save_array;
 opTable[0x255]=op_load_array;
 opTable[0x256]=op_get_array_key;
 opTable[0x257]=op_stack_array;
 // opTable[0x258]= RESERVED for arrays
 // opTable[0x259]= RESERVED for arrays
 opTable[0x25a]=op_reg_anim_destroy;
 opTable[0x25b]=op_reg_anim_animate_and_hide;
 opTable[0x25c]=op_reg_anim_combat_check;
 opTable[0x25d]=op_reg_anim_light;
 opTable[0x25e]=op_reg_anim_change_fid;
 opTable[0x25f]=op_reg_anim_take_out;
 opTable[0x260]=op_reg_anim_turn_towards;
 opTable[0x261]=op_explosions_metarule;
 opTable[0x262]=register_hook_proc;
 opTable[0x263]=funcPow;
 opTable[0x264]=funcLog;
 opTable[0x265]=funcExp;
 opTable[0x266]=funcCeil;
 opTable[0x267]=funcRound;
 // opTable[0x268]= RESERVED
 // opTable[0x269]= RESERVED
 // opTable[0x26a]=op_game_ui_redraw;
 opTable[0x26b]=op_message_str_game;
 opTable[0x26c]=op_sneak_success;
 opTable[0x26d]=op_tile_light;
 opTable[0x26e]=op_make_straight_path;
 opTable[0x26f]=op_obj_blocking_at;
 opTable[0x270]=op_tile_get_objects;
 opTable[0x271]=op_get_party_members;
 opTable[0x272]=op_make_path;
 opTable[0x273]=op_create_spatial;
 opTable[0x274]=op_art_exists;
 opTable[0x275]=op_obj_is_carrying_obj;

 memset(&OverrideScriptStruct, 0, 0xE0);

 idle = GetPrivateProfileIntA("Misc", "ProcessorIdle", -1, ini);

 arraysBehavior = GetPrivateProfileIntA("Misc", "arraysBehavior", 1, ini);
 if (arraysBehavior > 0) {
  arraysBehavior = 1; // only 1 and 0 allowed at this time
  dlogr("New arrays behavior enabled.", DL_SCRIPT);
 } else dlogr("Arrays in backward-compatiblity mode.", DL_SCRIPT);

 MakeCall(0x4A390C, &FindSidHook, true);
 MakeCall(0x4A5E34, &ScrPtrHook, true);

 MakeCall(0x4230D5, &AfterCombatAttackHook, true);
 MakeCall(0x4A67F2, &ExecMapScriptsHook, true);

 // this patch makes it possible to export variables from sfall global scripts
 MakeCall(0x4414C8, &Export_Export_FindVar_Hook, true);
 HookCall(0x441285, &Export_FetchOrStore_FindVar_Hook); // store
 HookCall(0x4413D9, &Export_FetchOrStore_FindVar_Hook); // fetch

 HookCall(0x46E141, FreeProgramHook);

 HookCall(0x421B72, &combat_begin_hook);
 HookCall(0x421FC1, &combat_over_hook);

}

DWORD GetScriptProcByName(DWORD scriptPtr, const char* procName) {
 DWORD result;
 __asm {
  mov edx, procName;
  mov eax, scriptPtr;
  call interpretFindProcedure_
  mov result, eax;
 }
 return result;
}

// loads script from .int file into a sScriptProgram struct, filling script pointer and proc lookup table
void LoadScriptProgram(sScriptProgram &prog, const char* fileName) {
 DWORD scriptPtr;
 __asm {
  mov eax, fileName;
  call loadProgram_
  mov scriptPtr, eax;
 }
 if (scriptPtr) {
  const char** procTable = (const char **)_procTableStrs;
  prog.ptr = scriptPtr;
  // fill lookup table
  for (int i=0; i<=SCRIPT_PROC_MAX; i++) {
   prog.procLookup[i] = GetScriptProcByName(prog.ptr, procTable[i]);
  }
  prog.initialized = 0;
 } else {
  prog.ptr = NULL;
 }
}

void InitScriptProgram(sScriptProgram &prog) {
 DWORD progPtr = prog.ptr;
 if (prog.initialized == 0) {
  __asm {
   mov eax, progPtr;
   call runProgram_
   mov edx, -1;
   mov eax, progPtr;
   call interpret_
  }
  prog.initialized = 1;
 }
}

void AddProgramToMap(sScriptProgram &prog) {
 sfallProgsMap[prog.ptr] = prog;
}

sScriptProgram* GetGlobalScriptProgram(DWORD scriptPtr) {
 for (std::vector<sGlobalScript>::iterator it = globalScripts.begin(); it != globalScripts.end(); it++) {
  if (it->prog.ptr == scriptPtr) return &it->prog;
 }
 return NULL;
}

bool _stdcall isGameScript(const char* filename) {
 for (DWORD i = 0; i < *(DWORD*)_maxScriptNum; i++) {
  if (strcmp(filename, (char*)(*(DWORD*)_scriptListInfo + i*20)) == 0) return true;
 }
 return false;
}

// this runs after the game was loaded/started
void LoadGlobalScripts() {
 isGameLoading = false;
 HookScriptInit();
 dlogr("Loading global scripts", DL_SCRIPT|DL_INIT);

 char* name = "scripts\\gl*.int";
 DWORD count, *filenames;
 __asm {
  xor  ecx, ecx
  xor  ebx, ebx
  lea  edx, filenames
  mov  eax, name
  call db_get_file_list_
  mov  count, eax
 }

 sScriptProgram prog;
 for (DWORD i = 0; i < count; i++) {
  name = _strlwr((char*)filenames[i]);
  name[strlen(name) - 4] = 0;
  if (!isGameScript(name)) {
   dlog(">", DL_SCRIPT);
   dlog(name, DL_SCRIPT);
   isLoadingGlobalScript = 1;
   LoadScriptProgram(prog, name);
   if (prog.ptr) {
    dlogr(", success!", DL_SCRIPT);
    DWORD idx;
    sGlobalScript gscript = sGlobalScript(prog);
    idx = globalScripts.size();
    globalScripts.push_back(gscript);
    AddProgramToMap(prog);
    // initialize script (start proc will be executed for the first time) -- this needs to be after script is added to "globalScripts" array
    InitScriptProgram(prog);
   } else dlogr(", error!", DL_SCRIPT);
   isLoadingGlobalScript = 0;
  }
 }
 __asm {
  xor  edx, edx
  lea  eax, filenames
  call db_free_file_list_
 }
 dlogr("Finished loading global scripts", DL_SCRIPT|DL_INIT); 
}

static DWORD _stdcall ScriptHasLoaded(DWORD script) {
 for(DWORD d=0;d<checkedScripts.size();d++) {
  if(checkedScripts[d]==script) return 0;
 }
 checkedScripts.push_back(script);
 return 1;
}

// this runs before actually loading/starting the game
void ClearGlobalScripts() {
 isGameLoading = true;
 checkedScripts.clear();
 sfallProgsMap.clear();
 globalScripts.clear();
 selfOverrideMap.clear();
 globalExportedVars.clear();
 HookScriptClear();

 //Reset some settable game values back to the defaults
 //Pyromaniac bonus
 SafeWrite8(0x424AB6, 5);
 //xp mod
 SafeWrite8(0x004AFAB8, 0x53);
 SafeWrite32(0x004AFAB9, 0x55575651);
 //Perk level mod
 SafeWrite32(0x00496880, 0x00019078);
 //HP bonus
 SafeWrite8(0x4AFBC1, 2);
 //Stat ranges
 StatsReset();

 //Bodypart hit chances
 *((DWORD*)0x510954) = GetPrivateProfileIntA("Misc", "BodyHit_Head",      0xFFFFFFD8, ini);
 *((DWORD*)0x510958) = GetPrivateProfileIntA("Misc", "BodyHit_Left_Arm",  0xFFFFFFE2, ini);
 *((DWORD*)0x51095C) = GetPrivateProfileIntA("Misc", "BodyHit_Right_Arm", 0xFFFFFFE2, ini);
 *((DWORD*)0x510960) = GetPrivateProfileIntA("Misc", "BodyHit_Torso",     0x00000000, ini);
 *((DWORD*)0x510964) = GetPrivateProfileIntA("Misc", "BodyHit_Right_Leg", 0xFFFFFFEC, ini);
 *((DWORD*)0x510968) = GetPrivateProfileIntA("Misc", "BodyHit_Left_Leg",  0xFFFFFFEC, ini);
 *((DWORD*)0x51096C) = GetPrivateProfileIntA("Misc", "BodyHit_Eyes",      0xFFFFFFC4, ini);
 *((DWORD*)0x510970) = GetPrivateProfileIntA("Misc", "BodyHit_Groin",     0xFFFFFFE2, ini);
 *((DWORD*)0x510974) = GetPrivateProfileIntA("Misc", "BodyHit_Torso",     0x00000000, ini);

 //skillpoints per level mod
 SafeWrite8(0x43C27a, 5);
}
void RunScriptProcByNum(DWORD sptr, DWORD procNum) {
 __asm {
  mov edx, procNum;
  mov eax, sptr;
  call executeProcedure_
 }
}
void RunScriptProc(sScriptProgram* prog, const char* procName) {
 DWORD sptr=prog->ptr;
 DWORD procNum = GetScriptProcByName(sptr, procName);
 if (procNum != -1) {
  RunScriptProcByNum(sptr, procNum);
 }
}
void RunScriptProc(sScriptProgram* prog, DWORD procId) {
 if (procId > 0 && procId <= SCRIPT_PROC_MAX) {
  DWORD sptr = prog->ptr;
  DWORD procNum = prog->procLookup[procId];
  if (procNum != -1) {
   RunScriptProcByNum(sptr, procNum);
  }
 }
}

static void RunScript(sGlobalScript* script) {
 script->count=0;
 RunScriptProc(&script->prog, start); // run "start"
}

/**
 Do some clearing after each frame:
 - delete all temp arrays
 - reset reg_anim_* combatstate checks
*/
static void ResetStateAfterFrame() {
 if (tempArrays.size()) {
  for (std::set<DWORD>::iterator it = tempArrays.begin(); it != tempArrays.end(); ++it) 
   FreeArray(*it);
  tempArrays.clear();
 }
 RegAnimCombatCheck(1);
}

/**
 Do some cleaning after each combat attack action
*/
void AfterAttackCleanup() {
 ResetExplosionSettings();
}

void RunGlobalScripts1() {
 if(idle>-1) Sleep(idle);
 for(DWORD d=0;d<globalScripts.size();d++) {
  if(!globalScripts[d].repeat||(globalScripts[d].mode!=0&&globalScripts[d].mode!=3)) continue;
  if(++globalScripts[d].count>=globalScripts[d].repeat) {
   RunScript(&globalScripts[d]);
  }
 }
 ResetStateAfterFrame();
}

void RunGlobalScripts2() {
 if(idle>-1) Sleep(idle);
 for(DWORD d=0;d<globalScripts.size();d++) {
  if(!globalScripts[d].repeat||globalScripts[d].mode!=1) continue;
  if(++globalScripts[d].count>=globalScripts[d].repeat) {
   RunScript(&globalScripts[d]);
  }
 }
 ResetStateAfterFrame();
}

void RunGlobalScripts3() {
 if(idle>-1) Sleep(idle);
 for(DWORD d=0;d<globalScripts.size();d++) {
  if(!globalScripts[d].repeat||(globalScripts[d].mode!=2&&globalScripts[d].mode!=3)) continue;
  if(++globalScripts[d].count>=globalScripts[d].repeat) {
   RunScript(&globalScripts[d]);
  }
 }
 ResetStateAfterFrame();
}

void _stdcall HandleMapUpdateForScripts(DWORD procId) {
 if (procId == map_enter_p_proc) {
  // map changed, all game objects were destroyed and scripts detached, need to re-insert global scripts into the game
  for (SfallProgsMap::iterator it = sfallProgsMap.begin(); it != sfallProgsMap.end(); it++) {
   DWORD progPtr = it->second.ptr;
   __asm {
    mov eax, progPtr;
    call runProgram_
   }
  }
 }
 RunGlobalScriptsAtProc(procId); // gl* scripts of types 0 and 3
 RunHookScriptsAtProc(procId); // all hs_ scripts
}

// run all global scripts of types 0 and 3 at specific procedure (if exist)
void _stdcall RunGlobalScriptsAtProc(DWORD procId) {
 for (DWORD d = 0; d < globalScripts.size(); d++) {
  if (globalScripts[d].mode != 0 && globalScripts[d].mode != 3) continue;
  RunScriptProc(&globalScripts[d].prog, procId);
 }
}

void LoadGlobals(HANDLE h) {
 DWORD count, unused;
 ReadFile(h, &count, 4, &unused, 0);
 if(unused!=4) return;
 sGlobalVar var;
 for(DWORD i=0;i<count;i++) {
  ReadFile(h, &var, sizeof(sGlobalVar), &unused, 0);
  globalVars.insert(glob_pair(var.id, var.val));
 }
}

void SaveGlobals(HANDLE h) {
 DWORD size;
 DWORD count = globalVars.size();
 WriteFile(h, &count, 4, &size, 0);
 sGlobalVar var;
 glob_citr itr = globalVars.begin();
 while (itr != globalVars.end()) {
  var.id = itr->first;
  var.val = itr->second;
  WriteFile(h, &var, sizeof(sGlobalVar), &size, 0);
  itr++;
 }
}

void ClearGlobals() {
 globalVars.clear();
 for (array_itr it = arrays.begin(); it != arrays.end(); ++it) 
  it->second.clear();
 arrays.clear();
 savedArrays.clear();
}
int GetNumGlobals() { return globalVars.size(); }
void GetGlobals(sGlobalVar* globals) {
 glob_citr itr=globalVars.begin();
 int i=0;
 while(itr!=globalVars.end()) {
  globals[i].id=itr->first;
  globals[i++].val=itr->second;
  itr++;
 }
}
void SetGlobals(sGlobalVar* globals) {
 glob_itr itr=globalVars.begin();
 int i=0;
 while(itr!=globalVars.end()) {
  itr->second=globals[i++].val;
  itr++;
 }
}

//fuctions to load and save appearance globals
void _stdcall SetAppearanceGlobals(int race, int style) {
 SetGlobalVar2("HAp_Race", race);
 SetGlobalVar2("HApStyle", style);
}

void _stdcall GetAppearanceGlobals(int *race, int *style) {
 *race=GetGlobalVar2("HAp_Race");
 *style=GetGlobalVar2("HApStyle");
}
