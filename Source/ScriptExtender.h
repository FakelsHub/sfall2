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

#pragma once
#include "main.h"

#define DATATYPE_NONE  (0)
#define DATATYPE_INT   (1)
#define DATATYPE_FLOAT (2)
#define DATATYPE_STR   (3)

struct sGlobalVar {
 __int64 id;
 int val;
};

#define SCRIPT_PROC_MAX  (27)
typedef struct {
 DWORD ptr;
 DWORD procLookup[SCRIPT_PROC_MAX+1];
 char initialized;
} sScriptProgram;

void ScriptExtenderSetup();
void LoadGlobalScripts();
void ClearGlobalScripts();

void RunGlobalScripts1();
void RunGlobalScripts2();
void RunGlobalScripts3();
void _stdcall RunGlobalScriptsAtProc(DWORD procId);
void AfterAttackCleanup();

void LoadGlobals(HANDLE h);
void SaveGlobals(HANDLE h);
void ClearGlobals();

int GetNumGlobals();
void GetGlobals(sGlobalVar* globals);
void SetGlobals(sGlobalVar* globals);

extern DWORD AvailableGlobalScriptTypes;

void _stdcall SetAppearanceGlobals(int race, int style);
void _stdcall GetAppearanceGlobals(int *race, int *style);

void _stdcall RegAnimCombatCheck(DWORD newValue);

DWORD _stdcall ScriptHasLoaded(DWORD script);
// loads script from .int file into scripting engine, fill scriptPtr and proc table
void LoadScriptProgram(sScriptProgram &prog, const char* fileName);
// init program after load, needs to be called once
void InitScriptProgram(sScriptProgram &prog);
// execute script proc by internal proc number (from script's proc table, basically a sequential number of a procedure as defined in code, starting from 1)
void RunScriptProcByNum(DWORD sptr, DWORD procNum);
// execute script by specific proc name
void RunScriptProc(sScriptProgram* prog, const char* procName);
// execute script proc by procId from define.h
void RunScriptProc(sScriptProgram* prog, DWORD procId);

void AddProgramToMap(sScriptProgram &prog);
sScriptProgram* GetGlobalScriptProgram(DWORD scriptPtr);

char* _stdcall mysubstr(char* str, int pos, int length);
// variables
static char reg_anim_combat_check = 1;
extern DWORD isGlobalScriptLoading;
