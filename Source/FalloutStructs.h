/*
* sfall
* Copyright (C) 2008-2016 The sfall team
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*/
#pragma once


/******************************************************************************/
/* FALLOUT2.EXE structs, function offsets and wrappers should be placed here  */
/******************************************************************************/


struct TGameObj;

/*   26 */
#pragma pack(push, 1)
struct TInvenRec
{
 TGameObj *object;
 int count;
};
#pragma pack(pop)

/*    8 */
#pragma pack(push, 1)
struct TGameObj
{
 int ID;
 int tile;
 char gap_8[2];
 char field_A;
 char gap_B[17];
 int rotation;
 int artFID;
 char gap_24[4];
 int elevation;
 int invenCount;
 int field_30;
 TInvenRec *invenTablePtr;
 char gap_38[4];
 int itemCharges;
 int movePoints;
 char gap_44[16];
 int lastTarget;
 char gap_58[12];
 int pid;
 char gap_68[16];
 int scriptID;
 char gap_7C[4];
 int script_index;
 char gap_84[7];
 char field_0;
};
#pragma pack(pop)

/*   22 */
#pragma pack(push, 1)
struct TScript
{
 int script_id;
 DWORD next;
 int elevation_and_tile;
 int spatial_radius;
 DWORD flags;
 int script_index;
 int program_ptr;
 int self_obj_id;
 DWORD local_var_offset;
 DWORD num_local_vars;
 int scr_return;
 DWORD action;
 int fixed_param;
 TGameObj *self_obj;
 TGameObj *source_obj;
 TGameObj *target_obj;
 DWORD action_num;
 int script_overrides;
 DWORD unknown;
 DWORD how_much;
 DWORD not_used;
 int procedure_table[28];
 DWORD unused[7];
};
#pragma pack(pop)

/*   25 */
#pragma pack(push, 1)
struct TProgram
{
 char gap_0[4];
 int *codeStackPtr;
 char gap_8[8];
 int *codePtr;
 int field_14;
 char gap_18[4];
 int *dStackPtr;
 int *aStackPtr;
 int *dStackOffs;
 int *aStackOffs;
 char gap_2C[4];
 int *stringRefPtr;
 char gap_34[4];
 int *procTablePtr;
};
#pragma pack(pop)
