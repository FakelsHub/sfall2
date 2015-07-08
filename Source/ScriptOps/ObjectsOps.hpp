/*
 *    sfall
 *    Copyright (C) 2008, 2009, 2010, 2012  The sfall team
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
#include "Inventory.h"
#include "ScriptExtender.h"

//script control functions

static const DWORD scr_remove=0x4A61D4;
static void __declspec(naked) RemoveScript() {
	__asm {
		push ebx;
		push ecx;
		push edx;
		mov ecx, eax;
		call GetArgType;
		mov edx, eax;
		mov eax, ecx;
		call GetArg;
		cmp dx, 0xc001;
		jnz end;
		test eax, eax;
		jz end;
		mov edx, eax;
		mov eax, [eax+0x78];
		cmp eax, 0xffffffff;
		jz end;
		call scr_remove;
		mov dword ptr [edx+0x78], 0xffffffff;
end:
		pop edx;
		pop ecx;
		pop ebx;
		retn;
	}
}

static const DWORD obj_new_sid_inst=0x49AAC0;
static const DWORD exec_script_proc=0x4A4810;
//static const DWORD obj_new_sid_inst=0x49A9B4;
static void __declspec(naked) SetScript() {
	__asm {
		pushad;
		mov ecx, eax;
		call GetArgType;
		mov edx, eax;
		mov eax, ecx;
		call GetArg;
		mov ebx, eax;
		mov eax, ecx;
		call GetArgType;
		mov edi, eax;
		mov eax, ecx;
		call GetArg;
		cmp dx, 0xc001;
		jnz end;
		cmp di, 0xc001;
		jnz end;
		test eax, eax;
		jz end;
		mov esi, [eax+0x78];
		cmp esi, 0xffffffff;
		jz newscript
		push eax;
		mov eax, esi;
		call scr_remove;
		pop eax;
		mov dword ptr [eax+0x78], 0xffffffff;
newscript:
		mov esi, 1;
		test ebx, 0x80000000;
		jz execMapEnter;
		xor esi, esi;
		xor ebx, 0x80000000;
execMapEnter:
		mov ecx, eax;
		mov edx, 3; // script_type_item
		mov edi, [eax+0x64];
		shr edi, 24;
		cmp edi, 1;
		jnz notCritter;
		inc edx; // 4 - "critter" type script
notCritter:
		dec ebx;
		call obj_new_sid_inst;
		mov eax, [ecx+0x78];
		mov edx, 1; // start
		call exec_script_proc;
		cmp esi, 1; // run map enter?
		jnz end;
		mov eax, [ecx+0x78];
		mov edx, 0xf; // map_enter_p_proc
		call exec_script_proc;
end:
		popad;
		retn;
	}
}

static const DWORD scr_new_=0x4A5F28; // eax - script index from scripts lst, edx - type (0 - system, 1 - spatials, 2 - time, 3 - items, 4 - critters)
static const DWORD scr_ptr_=0x4A5E34; // eax - scriptId, edx - **TScript (where to store script pointer)

static void _stdcall op_create_spatial2() {
	DWORD scriptIndex = GetOpArgInt(0),
		  tile = GetOpArgInt(1),
		  elevation = GetOpArgInt(2),
		  radius = GetOpArgInt(3), 
		  scriptId, tmp, objectPtr,
		  scriptPtr;
	__asm {
		lea eax, scriptId;
		mov edx, 1;
		call scr_new_;
		mov tmp, eax;
	}
	if (tmp == -1)
		return;
	__asm {
		mov eax, scriptId;
		lea edx, scriptPtr;
		call scr_ptr_;
		mov tmp, eax;
	}
	if (tmp == -1)
		return;
	// fill spatial script properties:
	*(DWORD*)(scriptPtr + 0x14) = scriptIndex - 1;
	*(DWORD*)(scriptPtr + 0x8) = (elevation << 29) & 0xE0000000 | tile;
	*(DWORD*)(scriptPtr + 0xC) = radius;
	// this will load appropriate script program and link it to the script instance we just created:
	__asm {
		mov eax, scriptId;
		mov edx, 1; // start_p_proc
		call exec_script_proc_; 
		mov eax, scriptPtr;
		mov eax, [eax + 0x18]; // program pointer
		call scr_find_obj_from_program_;
		mov objectPtr, eax;
	}
	SetOpReturn((int)objectPtr);
}

static void __declspec(naked) op_create_spatial() {
	_WRAP_OPCODE(4, op_create_spatial2)
}
static void __declspec(naked) op_spatial_radius() {
	_OP_BEGIN(ebp)
	_GET_ARG_R32(ebp, ecx, eax)
	_CHECK_ARG_INT(cx, fail)
	__asm {
		test eax, eax;
		js fail;
		mov eax, [eax+0xC];
fail:
	}
	_RET_VAL_INT(ebp)
	_OP_END
}

static void __declspec(naked) GetScript() {
	__asm {
		pushad;
		mov ecx, eax;
		mov ecx, eax;
		call GetArgType;
		mov edx, eax;
		mov eax, ecx;
		call GetArg;
		cmp dx, 0xc001;
		jnz fail;
		test eax, eax;
		jz fail;
		mov edx, [eax+0x80];
		cmp edx, -1;
		jz fail;
		inc edx;
		jmp end;
fail:
		xor edx, edx;
		dec edx;
end:
		mov eax, ecx;
		call SetResult;
		mov eax, ecx;
		mov edx, 0xc001;
		call SetResultType;
		popad;
		retn;
	}
}

static void __declspec(naked) set_critter_burst_disable() {
	__asm {
		pushad;
		mov ebp, eax;
		call GetArgType;
		mov edi, eax;
		mov eax, ebp;
		call GetArg;
		mov ecx, eax;
		mov eax, ebp;
		call GetArgType;
		mov esi, eax;
		mov eax, ebp;
		call GetArg;
		cmp di, 0xc001;
		jnz end;
		cmp si, 0xc001;
		jnz end;
		push ecx;
		push eax;
		call SetNoBurstMode;
end:
		popad;
		retn;
	}
}
static void __declspec(naked) get_weapon_ammo_pid() {
	__asm {
		pushad;
		mov ebp, eax;
		call GetArgType;
		mov edi, eax;
		mov eax, ebp;
		call GetArg;
		cmp di, 0xc001;
		jnz fail;
		test eax, eax;
		jz fail;
		mov edx, [eax+0x40];
		jmp end;
fail:
		xor edx, edx;
		dec edx;
end:
		mov eax, ebp;
		call SetResult;
		mov eax, ebp;
		mov edx, 0xc001;
		call SetResultType;
		popad;
		retn;
	}
}
static void __declspec(naked) set_weapon_ammo_pid() {
	__asm {
		pushad;
		mov ebp, eax;
		call GetArgType;
		mov edi, eax;
		mov eax, ebp;
		call GetArg;
		mov ecx, eax;
		mov eax, ebp;
		call GetArgType;
		mov esi, eax;
		mov eax, ebp;
		call GetArg;
		cmp di, 0xc001;
		jnz end;
		cmp si, 0xc001;
		jnz end;
		test eax, eax;
		jz end;
		mov [eax+0x40], ecx;
end:
		popad;
		retn;
	}
}
static void __declspec(naked) get_weapon_ammo_count() {
	__asm {
		pushad;
		mov ebp, eax;
		call GetArgType;
		mov edi, eax;
		mov eax, ebp;
		call GetArg;
		cmp di, 0xc001;
		jnz fail;
		test eax, eax;
		jz fail;
		mov edx, [eax+0x3c];
		jmp end;
fail:
		xor edx, edx;
		dec edx;
end:
		mov eax, ebp;
		call SetResult;
		mov eax, ebp;
		mov edx, 0xc001;
		call SetResultType;
		popad;
		retn;
	}
}
static void __declspec(naked) set_weapon_ammo_count() {
	__asm {
		pushad;
		mov ebp, eax;
		call GetArgType;
		mov edi, eax;
		mov eax, ebp;
		call GetArg;
		mov ecx, eax;
		mov eax, ebp;
		call GetArgType;
		mov esi, eax;
		mov eax, ebp;
		call GetArg;
		cmp di, 0xc001;
		jnz end;
		cmp si, 0xc001;
		jnz end;
		test eax, eax;
		jz end;
		mov [eax+0x3c], ecx;
end:
		popad;
		retn;
	}
}

static const DWORD obj_blocking_at_ = 0x48B848; // <eax>(int aExcludeObject<eax> /* can be 0 */, signed int aTile<edx>, int aElevation<ebx>)
static const DWORD obj_shoot_blocking_at_ = 0x48B930; // 
static const DWORD obj_ai_blocking_at_ = 0x48BA20; // 
static const DWORD obj_scroll_blocking_at_ = 0x48BB44; // 
static const DWORD obj_sight_blocking_at = 0x48BB88; // 
static const DWORD make_straight_path_func_ = 0x4163C8; // (TGameObj *aObj<eax>, int aTileFrom<edx>, int a3<ecx>, signed int aTileTo<ebx>, TGameObj **aObjResult, int a5, int (*a6)(void))
// (int aObjFrom<eax>, int aTileFrom<edx>, char* aPathPtr<ecx>, int aTileTo<ebx>, int a5, int (__fastcall *a6)(_DWORD, _DWORD)) 
// - path is saved in ecx as a sequence of tile directions (0..5) to move on each step,
// - returns path length
static const DWORD make_path_func_ = 0x415EFC; 

static DWORD _stdcall obj_blocking_at_wrapper(DWORD obj, DWORD tile, DWORD elevation, DWORD func) {
	DWORD retval;
	__asm {
		mov eax, obj;
		mov edx, tile;
		mov ebx, elevation;
		call func;
		mov retval, eax;
	}
	return retval;
}

static DWORD _stdcall make_straight_path_func_wrapper(DWORD obj, DWORD tileFrom, DWORD a3, DWORD tileTo, DWORD* result, DWORD a6, DWORD func) {
	DWORD retval;
	__asm {
		mov eax, obj;
		mov edx, tileFrom;
		mov ecx, a3;
		mov ebx, tileTo;
		push func;
		push a6;
		push result;
		call make_straight_path_func_;
		mov retval, eax;
	}
	return retval;
}

#define BLOCKING_TYPE_BLOCK		(0)
#define BLOCKING_TYPE_SHOOT		(1)
#define BLOCKING_TYPE_AI		(2)
#define BLOCKING_TYPE_SIGHT		(3)
#define BLOCKING_TYPE_SCROLL	(4)

static DWORD getBlockingFunc(DWORD type) {
	switch (type) {
		case BLOCKING_TYPE_BLOCK: default: 
			return obj_blocking_at_;
		case BLOCKING_TYPE_SHOOT: 
			return obj_shoot_blocking_at_;
		case BLOCKING_TYPE_AI: 
			return obj_ai_blocking_at_;
		case BLOCKING_TYPE_SIGHT: 
			return obj_sight_blocking_at;
		//case 4: 
		//	return obj_scroll_blocking_at_;
			
	}
}

static void _stdcall op_make_straight_path2() {
	DWORD objFrom = GetOpArgInt(0),
		  tileTo = GetOpArgInt(1),
		  type = GetOpArgInt(2),
		  resultObj, arg6;
	arg6 = (type == BLOCKING_TYPE_SHOOT) ? 32 : 0;
	make_straight_path_func_wrapper(objFrom, *(DWORD*)(objFrom + 4), 0, tileTo, &resultObj, arg6, getBlockingFunc(type));
	SetOpReturn(resultObj, DATATYPE_INT);
}

static void __declspec(naked) op_make_straight_path() {
	_WRAP_OPCODE(3, op_make_straight_path2)
}

static void _stdcall op_make_path2() {
	DWORD objFrom = GetOpArgInt(0),
		tileFrom = 0,
		tileTo = GetOpArgInt(1),
		type = GetOpArgInt(2),
		func = getBlockingFunc(type),
		arr;
	long pathLength, a5 = 1;
	if (!objFrom) {
		SetOpReturn(0, DATATYPE_INT);
		return;
	}
	tileFrom = *(DWORD*)(objFrom + 4);
	char pathData[800];
	char* pathDataPtr = pathData;
	__asm {
		mov eax, objFrom;
		mov edx, tileFrom;
		mov ecx, pathDataPtr;
		mov ebx, tileTo;
		push func;
		push a5;
		call make_path_func_;
		mov pathLength, eax;
	}
	arr = TempArray(pathLength, 0);
	for (int i=0; i < pathLength; i++) {
		arrays[arr].val[i].set((long)pathData[i]);
	}
	SetOpReturn(arr, DATATYPE_INT);
}

static void __declspec(naked) op_make_path() {
	_WRAP_OPCODE(3, op_make_path2)
}


static void _stdcall op_obj_blocking_at2() {
	DWORD tile = GetOpArgInt(0),
		  elevation = GetOpArgInt(1),
		  type = GetOpArgInt(2), 
		  resultObj;
	resultObj = obj_blocking_at_wrapper(0, tile, elevation, getBlockingFunc(type));
	if (resultObj && type == BLOCKING_TYPE_SHOOT && (*(DWORD*)(resultObj + 39) & 0x80)) { // don't know what this flag means, copy-pasted from the engine code
		// this check was added because the engine always does exactly this when using shoot blocking checks
		resultObj = 0;
	}
	SetOpReturn(resultObj, DATATYPE_INT);
}

static void __declspec(naked) op_obj_blocking_at() {
	_WRAP_OPCODE(3, op_obj_blocking_at2)
}

static void _stdcall op_tile_get_objects2() {
	DWORD tile = GetOpArgInt(0),
		elevation = GetOpArgInt(1), 
		obj;
	DWORD arrayId = TempArray(0, 4);
	__asm {
		mov eax, elevation;
		mov edx, tile;
		call obj_find_first_at_tile_;
		mov obj, eax;
	}
	while (obj) {
		arrays[arrayId].push_back((long)obj);
		__asm {
			call obj_find_next_at_tile_;
			mov obj, eax;
		}
	}
	SetOpReturn(arrayId, DATATYPE_INT);
}

static void __declspec(naked) op_tile_get_objects() {
	_WRAP_OPCODE(2, op_tile_get_objects2)
}

static const DWORD partyMemberListPtr = 0x519DA8; // each struct - 4 integers, first integer - objPtr
static const DWORD partyMemberCount = 0x519DAC;

static void _stdcall op_get_party_members2() {
	DWORD obj, mode = GetOpArgInt(0), isDead;
	int i, actualCount = *(DWORD*)partyMemberCount;
	DWORD arrayId = TempArray(0, 4);
	DWORD* partyMemberList = *(DWORD**)partyMemberListPtr;
	for (i = 0; i < actualCount; i++) {
		obj = partyMemberList[i*4];
		if (mode == 0) { // mode 0 will act just like op_party_member_count in fallout2
			if ((*(DWORD*)(obj + 100) >> 24) != 1)  // obj type != critter
				continue;
			__asm {
				mov eax, obj;
				call critter_is_dead_;
				mov isDead, eax;
			}
			if (isDead)
				continue;
			if (*(DWORD*)(obj + 36) & 1) // no idea..
				continue;
		}
		arrays[arrayId].push_back((long)obj);
	}
	SetOpReturn(arrayId, DATATYPE_INT);
}

static void __declspec(naked) op_get_party_members() {
	_WRAP_OPCODE(1, op_get_party_members2)
}


static void __declspec(naked) op_art_exists() {
	_OP_BEGIN(ebp)
	_GET_ARG_R32(ebp, ecx, eax)
	_CHECK_ARG_INT(cx, fail)
	__asm {
		call art_exists_;
		jmp end;
fail:
		xor eax, eax;
end:
	}
	_RET_VAL_INT(ebp)
	_OP_END
}

static void _stdcall op_obj_is_carrying_obj2() {
	DWORD num = 0;
	if (IsOpArgInt(0) && IsOpArgInt(1)) {
		TGameObj *invenObj = (TGameObj*)GetOpArgInt(0),
				 *itemObj = (TGameObj*)GetOpArgInt(1);
		if (invenObj && itemObj) {
			for (int i = 0; i < invenObj->invenCount; i++) {
				if (invenObj->invenTablePtr[i].object == itemObj) {
					num = invenObj->invenTablePtr[i].count;
					break;
				}
			}
		}
	}
	SetOpReturn(num, DATATYPE_INT);
}

static void __declspec(naked) op_obj_is_carrying_obj() {
	_WRAP_OPCODE(2, op_obj_is_carrying_obj2)
}