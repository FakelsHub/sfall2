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
#include "numbers.h"
#include "ScriptExtender.h"
#include "version.h"

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

static DWORD TurnHighlightContainers = 0;
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
DWORD isGlobalScriptLoading = 0;

stdext::hash_map<__int64, int> globalVars;
typedef stdext::hash_map<__int64, int> :: iterator glob_itr;
typedef stdext::hash_map<__int64, int> :: const_iterator glob_citr;
typedef std::pair<__int64, int> glob_pair;

static void* opcodes[0x300];
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
  mov  edx, VERSION_BUILD
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
  mov  ecx, 0x4A3911
  jmp  ecx
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
  mov  ecx, 0x4A5E39
  jmp  ecx
 }
}

static void __declspec(naked) obj_outline_all_items_on() {
 __asm {
  mov  eax, ds:[_map_elevation]
  call obj_find_first_at_
loopObject:
  test eax, eax
  jz   end
  cmp  eax, ds:[_outlined_object]
  je   nextObject
  xchg ecx, eax
  mov  eax, [ecx+0x20]
  and  eax, 0xF000000
  sar  eax, 0x18
  test eax, eax                             // ��� ObjType_Item?
  jnz  nextObject                           // ���
  cmp  [ecx+0x7C], eax                      // ����-�� �����������?
  jnz  nextObject                           // ��
  test [ecx+0x74], eax                      // ��� ��������������?
  jnz  nextObject                           // ��
  mov  edx, 0x10                            // �����
  test [ecx+0x25], dl                       // ���������� NoHighlight_ (��� ���������)?
  jz   NoHighlight                          // ���
  cmp  TurnHighlightContainers, eax         // ������������ ����������?
  je   nextObject                           // ���
  mov  edx, 0x4                             // ������-�����
NoHighlight:
  mov  [ecx+0x74], edx
nextObject:
  call obj_find_next_at_
  jmp  loopObject
end:
  jmp  tile_refresh_display_
 }
}

static void __declspec(naked) obj_outline_all_items_off() {
 __asm {
  mov  eax, ds:[_map_elevation]
  call obj_find_first_at_
loopObject:
  test eax, eax
  jz   end
  cmp  eax, ds:[_outlined_object]
  je   nextObject
  xchg ecx, eax
  mov  eax, [ecx+0x20]
  and  eax, 0xF000000
  sar  eax, 0x18
  test eax, eax                             // ��� ObjType_Item?
  jnz  nextObject                           // ���
  cmp  [ecx+0x7C], eax                      // ����-�� �����������?
  jnz  nextObject                           // ��
  mov  [ecx+0x74], eax
nextObject:
  call obj_find_next_at_
  jmp  loopObject
end:
  jmp  tile_refresh_display_
 }
}

static DWORD toggleHighlightsKey;
static char HighlightFail1[128];
static char HighlightFail2[128];
static void __declspec(naked) get_input_hook() {
 __asm {
  call get_input_
  pushad
  mov  eax, toggleHighlightsKey
  test eax, eax
  jz   end
  push eax
  call KeyDown
  mov  ebx, ds:[_objItemOutlineState]
  test eax, eax
  jz   notOurKey
  test ebx, ebx
  jnz  end
  test MotionSensorFlags, 4                 // Sensor is required to use the item highlight feature
  jnz  checkSensor
outlineOn:
  call obj_outline_all_items_on
  jmp  stateOn
checkSensor:
  mov  eax, ds:[_obj_dude]
  mov  edx, PID_MOTION_SENSOR
  call inven_pid_is_carried_ptr_
  test eax, eax
  jz   noSensor
  test MotionSensorFlags, 2                 // Sensor doesn't require charges
  jnz  outlineOn
  call item_m_dec_charges_                  // Returns -1 if the item has no charges
  inc  eax
  test eax, eax
  jnz  outlineOn
  mov  eax, offset HighlightFail2           // "Your motion sensor is out of charge."
  jmp  printFail
noSensor:
  mov  eax, offset HighlightFail1           // "You aren't carrying a motion sensor."
printFail:
  call display_print_
  inc  ebx
stateOn:
  inc  ebx
  jmp  setState
notOurKey:
  cmp  ebx, 1
  jne  stateOff
  call obj_outline_all_items_off
stateOff:
  xor  ebx, ebx  
setState:
  mov  ds:[_objItemOutlineState], ebx
end:
  call RunGlobalScripts1
  popad
  retn
 }
}

static void __declspec(naked) AfterCombatAttackHook() {
 __asm {
  pushad;
  call AfterAttackCleanup;
  popad;
  mov eax, 1;
  mov edx, 0x4230DA;
  jmp edx;
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
  mov ebx, 0x4A67F9; // jump back
  jmp ebx;
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
 //dlog_f("\nTrying to export variable %s (%d)\r\n", DL_MAIN, name, isGlobalScriptLoading);
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
  mov eax, isGlobalScriptLoading;
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

static void __declspec(naked) gmouse_bk_process_hook() {
 __asm {
  test eax, eax
  jz   end
  test byte ptr [eax+0x25], 0x10            // NoHighlight_
  jnz  end
  mov  dword ptr [eax+0x74], 0
end:
  mov  edx, 0x40
  jmp  obj_outline_object_
 }
}

static void __declspec(naked) obj_remove_outline_hook() {
 __asm {
  call obj_remove_outline_
  test eax, eax
  jnz  end
  cmp  dword ptr ds:[_objItemOutlineState], 1
  jne  end
  mov  ds:[_outlined_object], eax
  call obj_outline_all_items_on
end:
  retn
 }
}

void ScriptExtenderSetup() {
#ifdef TRACE
 bool AllowUnsafeScripting=GetPrivateProfileIntA("Debugging", "AllowUnsafeScripting", 0, ini)!=0;
#else
 const bool AllowUnsafeScripting=false;
#endif
 toggleHighlightsKey = GetPrivateProfileIntA("Input", "ToggleItemHighlightsKey", 0, ini);
 if (toggleHighlightsKey) {
  HookCall(0x44B9BA, &gmouse_bk_process_hook);
  HookCall(0x44BD1C, &obj_remove_outline_hook);
  HookCall(0x44E559, &obj_remove_outline_hook);
  TurnHighlightContainers = GetPrivateProfileIntA("Input", "TurnHighlightContainers", 0, ini);
  GetPrivateProfileStringA("Sfall", "HighlightFail1", "You aren't carrying a motion sensor.", HighlightFail1, 128, translationIni);
  GetPrivateProfileStringA("Sfall", "HighlightFail2", "Your motion sensor is out of charge.", HighlightFail2, 128, translationIni);
 }

 idle = GetPrivateProfileIntA("Misc", "ProcessorIdle", -1, ini);
 modifiedIni=GetPrivateProfileIntA("Main", "ModifiedIni", 0, ini);

 dlogr("Adding additional opcodes", DL_SCRIPT);
 if(AllowUnsafeScripting) dlogr("  Unsafe opcodes enabled", DL_SCRIPT);
 else dlogr("  Unsafe opcodes disabled", DL_SCRIPT);

 arraysBehavior = GetPrivateProfileIntA("Misc", "arraysBehavior", 1, ini);
 if (arraysBehavior > 0) {
  arraysBehavior = 1; // only 1 and 0 allowed at this time
  dlogr("New arrays behavior enabled.", DL_SCRIPT);
 } else dlogr("Arrays in backward-compatiblity mode.", DL_SCRIPT);

 SafeWrite32(0x46E370, 0x300); //Maximum number of allowed opcodes
 SafeWrite32(0x46ce34, (DWORD)opcodes); //cmp check to make sure opcode exists
 SafeWrite32(0x46ce6c, (DWORD)opcodes); //call that actually jumps to the opcode
 SafeWrite32(0x46e390, (DWORD)opcodes); //mov that writes to the opcode

 HookCall(0x480E7B, &get_input_hook);       //hook the main game loop
 HookCall(0x422845, &get_input_hook);       //hook the combat loop

 MakeCall(0x4A390C, &FindSidHook, true);
 MakeCall(0x4A5E34, &ScrPtrHook, true);
 memset(&OverrideScriptStruct, 0, sizeof(TScript));

 MakeCall(0x4230D5, &AfterCombatAttackHook, true);
 MakeCall(0x4A67F2, &ExecMapScriptsHook, true);

 // this patch makes it possible to export variables from sfall global scripts
 MakeCall(0x4414C8, &Export_Export_FindVar_Hook, true);
 HookCall(0x441285, &Export_FetchOrStore_FindVar_Hook); // store
 HookCall(0x4413D9, &Export_FetchOrStore_FindVar_Hook); // fetch

 // fix vanilla negate operator on float values
 MakeCall(0x46AB63, &NegateFixHook, true);
 // fix incorrect int-to-float conversion
 // op_mult:
 SafeWrite16(0x46A3F4, 0x04DB); // replace operator to "fild 32bit"
 SafeWrite16(0x46A3A8, 0x04DB);
 // op_div:
 SafeWrite16(0x46A566, 0x04DB);
 SafeWrite16(0x46A4E7, 0x04DB);

 HookCall(0x46E141, FreeProgramHook);

 if(AllowUnsafeScripting) {
  opcodes[0x156]=ReadByte;
  opcodes[0x157]=ReadShort;
  opcodes[0x158]=ReadInt;
  opcodes[0x159]=ReadString;
 }
 opcodes[0x15a]=SetPCBaseStat;
 opcodes[0x15b]=SetPCExtraStat;
 opcodes[0x15c]=GetPCBaseStat;
 opcodes[0x15d]=GetPCExtraStat;
 opcodes[0x15e]=SetCritterBaseStat;
 opcodes[0x15f]=SetCritterExtraStat;
 opcodes[0x160]=GetCritterBaseStat;
 opcodes[0x161]=GetCritterExtraStat;
 opcodes[0x162]=funcTapKey;
 opcodes[0x163]=GetYear;
 opcodes[0x164]=GameLoaded;
 opcodes[0x165]=GraphicsFuncsAvailable;
 opcodes[0x166]=funcLoadShader;
 opcodes[0x167]=funcFreeShader;
 opcodes[0x168]=funcActivateShader;
 opcodes[0x169]=funcDeactivateShader;
 opcodes[0x16a]=SetGlobalScriptRepeat;
 opcodes[0x16b]=InputFuncsAvailable;
 opcodes[0x16c]=KeyPressed;
 opcodes[0x16d]=funcSetShaderInt;
 opcodes[0x16e]=funcSetShaderFloat;
 opcodes[0x16f]=funcSetShaderVector;
 opcodes[0x170]=in_world_map;
 opcodes[0x171]=ForceEncounter;
 opcodes[0x172]=SetWorldMapPos;
 opcodes[0x173]=GetWorldMapXPos;
 opcodes[0x174]=GetWorldMapYPos;
 opcodes[0x175]=SetDMModel;
 opcodes[0x176]=SetDFModel;
 opcodes[0x177]=SetMoviePath;
 for(int i=0x178;i<0x189;i++)
  opcodes[i]=funcSetPerkValue;
 opcodes[0x189]=funcSetPerkName;
 opcodes[0x18a]=funcSetPerkDesc;
 opcodes[0x18b]=SetPipBoyAvailable;
 if (usingExtraKillTypes) {
  opcodes[0x18c]=GetKillCounter2;
  opcodes[0x18d]=ModKillCounter2;
 } else {
  opcodes[0x18c]=GetKillCounter;
  opcodes[0x18d]=ModKillCounter;
 }
 opcodes[0x18e]=GetPerkOwed;
 opcodes[0x18f]=SetPerkOwed;
 opcodes[0x190]=GetPerkAvailable;
 opcodes[0x191]=GetCritterAP;
 opcodes[0x192]=SetCritterAP;
 opcodes[0x193]=GetActiveHand;
 opcodes[0x194]=ToggleActiveHand;
 opcodes[0x195]=SetWeaponKnockback;
 opcodes[0x196]=SetTargetKnockback;
 opcodes[0x197]=SetAttackerKnockback;
 opcodes[0x198]=RemoveWeaponKnockback;
 opcodes[0x199]=RemoveTargetKnockback;
 opcodes[0x19a]=RemoveAttackerKnockback;
 opcodes[0x19b]=SetGlobalScriptType;
 opcodes[0x19c]=GetGlobalScriptTypes;
 opcodes[0x19d]=SetGlobalVar;
 opcodes[0x19e]=GetGlobalVarInt;
 opcodes[0x19f]=GetGlobalVarFloat;
 opcodes[0x1a0]=fSetPickpocketMax;
 opcodes[0x1a1]=fSetHitChanceMax;
 opcodes[0x1a2]=fSetSkillMax;
 opcodes[0x1a3]=EaxAvailable;
 //opcodes[0x1a4]=SetEaxEnvironmentFunc;
 opcodes[0x1a5]=IncNPCLevel;
 opcodes[0x1a6]=GetViewportX;
 opcodes[0x1a7]=GetViewportY;
 opcodes[0x1a8]=SetViewportX;
 opcodes[0x1a9]=SetViewportY;
 opcodes[0x1aa]=SetXpMod;
 opcodes[0x1ab]=SetPerkLevelMod;
 opcodes[0x1ac]=GetIniSetting;
 opcodes[0x1ad]=funcGetShaderVersion;
 opcodes[0x1ae]=funcSetShaderMode;
 opcodes[0x1af]=get_game_mode;
 opcodes[0x1b0]=funcForceGraphicsRefresh;
 opcodes[0x1b1]=funcGetShaderTexture;
 opcodes[0x1b2]=funcSetShaderTexture;
 opcodes[0x1b3]=funcGetTickCount;
 opcodes[0x1b4]=SetStatMax;
 opcodes[0x1b5]=SetStatMin;
 opcodes[0x1b6]=SetCarTown;
 opcodes[0x1b7]=fSetPCStatMax;
 opcodes[0x1b8]=fSetPCStatMin;
 opcodes[0x1b9]=fSetNPCStatMax;
 opcodes[0x1ba]=fSetNPCStatMin;
 opcodes[0x1bb]=fSetFakePerk;
 opcodes[0x1bc]=fSetFakeTrait;
 opcodes[0x1bd]=fSetSelectablePerk;
 opcodes[0x1be]=fSetPerkboxTitle;
 opcodes[0x1bf]=fIgnoreDefaultPerks;
 opcodes[0x1c0]=fRestoreDefaultPerks;
 opcodes[0x1c1]=fHasFakePerk;
 opcodes[0x1c2]=fHasFakeTrait;
 opcodes[0x1c3]=fAddPerkMode;
 opcodes[0x1c4]=fClearSelectablePerks;
 opcodes[0x1c5]=SetCritterHitChance;
 opcodes[0x1c6]=SetBaseHitChance;
 opcodes[0x1c7]=SetCritterSkillMod;
 opcodes[0x1c8]=SetBaseSkillMod;
 opcodes[0x1c9]=SetCritterPickpocket;
 opcodes[0x1ca]=SetBasePickpocket;
 opcodes[0x1cb]=SetPyromaniacMod;
 opcodes[0x1cc]=fApplyHeaveHoFix;
 opcodes[0x1cd]=SetSwiftLearnerMod;
 opcodes[0x1ce]=SetLevelHPMod;
 if(AllowUnsafeScripting) {
  opcodes[0x1cf]=WriteByte;
  opcodes[0x1d0]=WriteShort;
  opcodes[0x1d1]=WriteInt;
  for(int i=0x1d2;i<0x1dc;i++)
   opcodes[i]=CallOffset;
 }
 opcodes[0x1dc]=ShowIfaceTag;
 opcodes[0x1dd]=HideIfaceTag;
 opcodes[0x1de]=IsIfaceTagActive;
 opcodes[0x1df]=GetBodypartHitModifier;
 opcodes[0x1e0]=SetBodypartHitModifier;
 opcodes[0x1e1]=funcSetCriticalTable;
 opcodes[0x1e2]=funcGetCriticalTable;
 opcodes[0x1e3]=funcResetCriticalTable;
 opcodes[0x1e4]=GetSfallArg;
 opcodes[0x1e5]=SetSfallReturn;
 opcodes[0x1e6]=SetApAcBonus;
 opcodes[0x1e7]=GetApAcBonus;
 opcodes[0x1e8]=SetApAcEBonus;
 opcodes[0x1e9]=GetApAcEBonus;
 opcodes[0x1ea]=InitHook;
 opcodes[0x1eb]=GetIniString;
 opcodes[0x1ec]=funcSqrt;
 opcodes[0x1ed]=funcAbs;
 opcodes[0x1ee]=funcSin;
 opcodes[0x1ef]=funcCos;
 opcodes[0x1f0]=funcTan;
 opcodes[0x1f1]=funcATan;
 opcodes[0x1f2]=SetPalette;
 opcodes[0x1f3]=RemoveScript;
 opcodes[0x1f4]=SetScript;
 opcodes[0x1f5]=GetScript;
 opcodes[0x1f6]=NBCreateChar;
 opcodes[0x1f7]=fs_create;
 opcodes[0x1f8]=fs_copy;
 opcodes[0x1f9]=fs_find;
 opcodes[0x1fa]=fs_write_byte;
 opcodes[0x1fb]=fs_write_short;
 opcodes[0x1fc]=fs_write_int;
 opcodes[0x1fd]=fs_write_int;
 opcodes[0x1fe]=fs_write_string;
 opcodes[0x1ff]=fs_delete;
 opcodes[0x200]=fs_size;
 opcodes[0x201]=fs_pos;
 opcodes[0x202]=fs_seek;
 opcodes[0x203]=fs_resize;
 opcodes[0x204]=get_proto_data;
 opcodes[0x205]=set_proto_data;
 opcodes[0x206]=set_self;
 opcodes[0x207]=register_hook;
 opcodes[0x208]=fs_write_bstring;
 opcodes[0x209]=fs_read_byte;
 opcodes[0x20a]=fs_read_short;
 opcodes[0x20b]=fs_read_int;
 opcodes[0x20c]=fs_read_float;
 opcodes[0x20d]=list_begin;
 opcodes[0x20e]=list_next;
 opcodes[0x20f]=list_end;
 opcodes[0x210]=sfall_ver_major;
 opcodes[0x211]=sfall_ver_minor;
 opcodes[0x212]=sfall_ver_build;
 opcodes[0x213]=funcHeroSelectWin;
 opcodes[0x214]=funcSetHeroRace;
 opcodes[0x215]=funcSetHeroStyle;
 opcodes[0x216]=set_critter_burst_disable;
 opcodes[0x217]=get_weapon_ammo_pid;
 opcodes[0x218]=set_weapon_ammo_pid;
 opcodes[0x219]=get_weapon_ammo_count;
 opcodes[0x21a]=set_weapon_ammo_count;
 if(AllowUnsafeScripting) {
  opcodes[0x21b]=WriteString;
 }
 opcodes[0x21c]=get_mouse_x;
 opcodes[0x21d]=get_mouse_y;
 opcodes[0x21e]=get_mouse_buttons;
 opcodes[0x21f]=get_window_under_mouse;
 opcodes[0x220]=get_screen_width;
 opcodes[0x221]=get_screen_height;
 opcodes[0x222] = (void*)map_disable_bk_processes_;// Stop game
 opcodes[0x223] = (void*)map_enable_bk_processes_; // Resume the game when it is stopped
 opcodes[0x224]=create_message_window;
 opcodes[0x225]=remove_trait;
 opcodes[0x226]=get_light_level;
 opcodes[0x227]=refresh_pc_art;
 opcodes[0x228]=get_attack_type;
 opcodes[0x229]=ForceEncounterWithFlags;
 opcodes[0x22a]=set_map_time_multi;
 opcodes[0x22b]=play_sfall_sound;
 opcodes[0x22c]=stop_sfall_sound;
 opcodes[0x22d]=create_array;
 opcodes[0x22e]=set_array;
 opcodes[0x22f]=get_array;
 opcodes[0x230]=free_array;
 opcodes[0x231]=len_array;
 opcodes[0x232]=resize_array;
 opcodes[0x233]=temp_array;
 opcodes[0x234]=fix_array;
 opcodes[0x235]=string_split;
 opcodes[0x236]=list_as_array;
 opcodes[0x237]=str_to_int;
 opcodes[0x238]=str_to_flt;
 opcodes[0x239]=scan_array;
 opcodes[0x23a]=get_tile_pid;
 opcodes[0x23b]=modified_ini;
 opcodes[0x23c]=GetSfallArgs;
 opcodes[0x23d]=SetSfallArg;
 opcodes[0x23e]=force_aimed_shots;
 opcodes[0x23f]=disable_aimed_shots;
 opcodes[0x240]=mark_movie_played;
 opcodes[0x241]=get_npc_level;
 opcodes[0x242]=set_critter_skill_points;
 opcodes[0x243]=get_critter_skill_points;
 opcodes[0x244]=set_available_skill_points;
 opcodes[0x245]=get_available_skill_points;
 opcodes[0x246]=mod_skill_points_per_level;
 opcodes[0x247]=set_perk_freq;
 opcodes[0x248]=get_last_attacker;
 opcodes[0x249]=get_last_target;
 opcodes[0x24a]=block_combat;
 opcodes[0x24b]=tile_under_cursor;
 opcodes[0x24c]=gdialog_get_barter_mod;
 opcodes[0x24d]=set_inven_ap_cost;
 opcodes[0x24e]=op_substr;
 opcodes[0x24f]=op_strlen;
 opcodes[0x250]=op_sprintf;
 opcodes[0x251]=op_ord;
 // opcodes[0x252]=  RESERVED
 opcodes[0x253]=op_typeof;
 opcodes[0x254]=op_save_array;
 opcodes[0x255]=op_load_array;
 opcodes[0x256]=op_get_array_key;
 opcodes[0x257]=op_stack_array;
 // opcodes[0x258]= RESERVED for arrays
 // opcodes[0x259]= RESERVED for arrays
 opcodes[0x25a]=op_reg_anim_destroy;
 opcodes[0x25b]=op_reg_anim_animate_and_hide;
 opcodes[0x25c]=op_reg_anim_combat_check;
 opcodes[0x25d]=op_reg_anim_light;
 opcodes[0x25e]=op_reg_anim_change_fid;
 opcodes[0x25f]=op_reg_anim_take_out;
 opcodes[0x260]=op_reg_anim_turn_towards;
 opcodes[0x261]=op_explosions_metarule;
 opcodes[0x262]=register_hook_proc;
 opcodes[0x263]=funcPow;
 opcodes[0x264]=funcLog;
 opcodes[0x265]=funcExp;
 opcodes[0x266]=funcCeil;
 opcodes[0x267]=funcRound;
 // opcodes[0x268]= RESERVED
 // opcodes[0x269]= RESERVED
 // opcodes[0x26a]=op_game_ui_redraw;
 opcodes[0x26b]=op_message_str_game;
 opcodes[0x26c]=op_sneak_success;
 opcodes[0x26d]=op_tile_light;
 opcodes[0x26e]=op_make_straight_path;
 opcodes[0x26f]=op_obj_blocking_at;
 opcodes[0x270]=op_tile_get_objects;
 opcodes[0x271]=op_get_party_members;
 opcodes[0x272]=op_make_path;
 opcodes[0x273]=op_create_spatial;
 opcodes[0x274]=op_art_exists;
 opcodes[0x275]=op_obj_is_carrying_obj;
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

// this runs after the game was loaded/started
void LoadGlobalScripts() {
 isGameLoading = false;
 HookScriptInit();
 dlogr("Loading global scripts", DL_SCRIPT);
 WIN32_FIND_DATA file;
 HANDLE h=FindFirstFileA("data\\scripts\\gl*.int", &file);
 if (h != INVALID_HANDLE_VALUE) {
  char* fName = file.cFileName;
  // TODO: refactor script programs
  sScriptProgram prog;
  DWORD found = 1;
  while (found) {
   fName[strlen(fName) - 4] = 0;
   dlogr(fName, DL_SCRIPT);
   isGlobalScriptLoading = 1;
   LoadScriptProgram(prog, fName);
   if (prog.ptr) {
    DWORD idx;
    sGlobalScript gscript = sGlobalScript(prog);
    idx = globalScripts.size();
    globalScripts.push_back(gscript);
    AddProgramToMap(prog);
    // initialize script (start proc will be executed for the first time) -- this needs to be after script is added to "globalScripts" array
    InitScriptProgram(prog);
   }
   isGlobalScriptLoading = 0;
   found = FindNextFileA(h, &file);
  }
  FindClose(h);
 }
 dlogr("Finished loading global scripts", DL_SCRIPT); 
 //ButtonsReload();
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
 *((DWORD*)0x510974) = GetPrivateProfileIntA("Misc", "BodyHit_Uncalled",  0x00000000, ini);

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

static void RunGlobalScripts1() {
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
 DWORD count, unused;
 count=globalVars.size();
 WriteFile(h, &count, 4, &unused, 0);
 sGlobalVar var;
 glob_citr itr=globalVars.begin();
 while(itr!=globalVars.end()) {
  var.id=itr->first;
  var.val=itr->second;
  WriteFile(h, &var, sizeof(sGlobalVar), &unused, 0);
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
void SetAppearanceGlobals(int race, int style) {
 SetGlobalVar2("HAp_Race", race);
 SetGlobalVar2("HApStyle", style);
}

void GetAppearanceGlobals(int *race, int *style) {
 *race=GetGlobalVar2("HAp_Race");
 *style=GetGlobalVar2("HApStyle");
}
