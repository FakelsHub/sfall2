/*
 *    sfall
 *    Copyright (C) 2009, 2010  Mash (Matt Wells, mashw at bigpond dot net dot au)
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

#include <stdio.h>
#include "Define.h"
#include "FalloutEngine.h"
#include "HeroAppearance.h"
#include "LoadGameHook.h"
#include "PartyControl.h"
#include "ScriptExtender.h"

static char RaceText[8];
static char StyleText[8];

static const char* RaceName = "AppRace";
static const char* StyleName = "AppStyle";
static const char* AppearanceFmt = "Appearance\\h%cR%02dS%02d";

static bool raceExist = false;
static bool styleExist = false;

// holds Appearance values to restore after global reset in NewGame2 fuction in LoadGameHooks.cpp
static DWORD CurrentAppearance[2] = {0, 0};

static DWORD critterListCount = 0, critterArraySize = 0;// Critter art list size

static const DWORD write32[27][2] = {
// {0x43647C, 592+3},                         // skill slider thingy (xpos)
// {0x4364FA, 614+3},                         // skill slider thingy (plus button xpos)
// {0x436567, 614+3},                         // skill slider thingy (minus button xpos)
 {0x43332F, 522+48},                        // x счётчика
 {0x43338D, 522+48},                        // x счётчика
 {0x43628A, 522+48},                        // x счётчика
 {0x4362F2, 522+48},                        // x счётчика
 {0x43631E, 522+48},                        // x счётчика
 {0x43B5B2, 522+48},                        // x счётчика
 {0x433678, 347+80},                        // x для кнопок выбора навыков
 {0x43A71E, 223-77},                        // ширина информационного окна #117 (530)
 {0x43A72A, 376+77},                        // x для информационного окна #117 (530)
 {0x4361C4, 370+77},                        // x для восстановления окна с навыками (to)
 {0x4361D9, 270-77},                        // ширина для окна с навыками
 {0x4361DE, 370+77},                        // x для восстановления окна с навыками (from)
 {0x43641C, 573+3},                         // x для значения навыка в процентах
 {0x43A74C, 223-77},                        // ширина информационного окна с навыками (531)
 {0x43A75B, 370+77},                        // x для информационного окна с навыками (531)
 {0x43A77D, 592+3-347-80},                  // ширина информационного окна с счётчиком (532)
 {0x43A789, 347+80},                        // x для информационного окна с счётчиком (532)
// x и ширина строки #138 (ОСНОВН.)
 {0x433370, 0x02485605},                    // add  eax, 640*233+470 (0x24856)
 {0x433374, 0x0064BB00},                    // mov  ebx, 100 (0x64)
// x и ширина строки #117 (НАВЫКИ)
 {0x43621E, 0x000E4905},                    // add  eax, 640*5+380+77 (0xE49)
 {0x436222, 0x0092BB00},                    // mov  ebx, 146 (0x92)
// x и ширина строки #112 (ДОП. ОЧКИ)
 {0x436260, 0x02484005},                    // add  eax, 640*233+448 (0x24840)
 {0x436264, 0x007ABB00},                    // mov  ebx, 122 (0x7A)
// x и ширина строки #138 (ОСНОВН.)
 {0x4362BC, 0x02485605},                    // add  eax, 640*233+470 (0x24856)
 {0x4362C0, 0x0064BB00},                    // mov  ebx, 100 (0x64)
// x и ширина навыков
 {0x4363D9, 0x904DC283},                    // add  edx, 77 (0x4D)
 {0x4363DF, 0x000077BB}                     // mov  ebx, 573+3-380-77 (0x77)
};

static const DWORD write16[10][2] = {
 {0x419521, 0x003B},                        // jmp  0x419560
 {0x433368, 0xA192},                        // xchg edx, eax; mov  eax, ds:[_win_buf]
 {0x433378, 0x0000},
 {0x436216, 0xA192},                        // xchg edx, eax; mov  eax, ds:[_win_buf]
 {0x436226, 0x0000},
 {0x436258, 0xA192},                        // xchg edx, eax; mov  eax, ds:[_win_buf]
 {0x436268, 0x0000},
 {0x4362B4, 0xA192},                        // xchg edx, eax; mov  eax, ds:[_win_buf]
 {0x4362C4, 0x0000},
 {0x4363E3, 0x9200}                         // xchg edx, eax
};

/*static const DWORD write8[2][2] = {
 {0x4365C8, 100},                           // Чтобы обрабатывались 98 и 99 _info_line в DrawInfoWin_
 {0x43ACD5, 160}                            // Максимальная ширина описания в DrawCard_
};*/

static void __declspec(naked) makeDataPath() {
// ecx = Race, ebx = Style, esi = filename, edi = filename_dat
 __asm {
  push edx
  mov  eax, ds:[_obj_dude]                  // hero state structure
//  test eax, eax
//  jz   skip
  mov  edx, STAT_gender                     // sex stat ref
  call stat_level_                          // get Player stat val
//skip:
  mov  edx, 'M'
  dec  eax                                  // male=0, female=1
  jnz  end
  mov  edx, 'F'
end:
  push ebx                                  // Style
  push ecx                                  // Race
  push edx                                  // Sex
  push AppearanceFmt                        // 'Appearance\\h%cR%02dS%02d'
  push esi                                  // buf
  call sprintf_
  push 0x50243A                             // '.dat'
  push esi
  push 0x5016A0                             // '%s%s'
  push edi                                  // buf
  call sprintf_
  add  esp, 9*4
  pop  edx
  retn
 }
}

static void __declspec(naked) xaddpath() {
// eax = *filename
 __asm {
  push esi
  push edx
  push ecx
  push ebx
  xchg esi, eax
  mov  eax, 16                              // size
  mov  ebx, eax                             // size
  call nmalloc_
  test eax, eax
  jz   end
  xor  edx, edx                             // val
  call memset_
  xchg ecx, eax
  mov  eax, esi
  call strdup_
  test eax, eax
  jnz  skip
  xchg ecx, eax
  call nfree_
  xchg ecx, eax
  jmp  end
skip:
  mov  [ecx], eax                           // database.path
  xchg esi, eax
  call dbase_open_
  test eax, eax                             // Открыли dat-файл?
  jz   notDat                               // Нет
  mov  [ecx+0x4], eax                       // database.dat
  xor  eax, eax
  inc  eax
  mov  [ecx+0x8], eax                       // database.is_dat
  inc  eax
notDat:
  mov  edx, ds:[_paths]
  mov  ds:[_paths], ecx
  mov  [ecx+0xC], edx                       // database.next
  dec  eax
end:
// eax: 0 = фейл, но память освобождать не надо; 1 = всё хорошо; -1 = не смогли открыть dat-файл, нужно освободить память
  pop  ebx
  pop  ecx
  pop  edx
  pop  esi
  retn
 }
}

static void __declspec(naked) LoadHeroDat() {
// eax = Race, ebx = Style
 __asm {
  push edi
  push esi
  push edx
  push ecx
  push ebx
  sub  esp, 260+260                         // [0]filename, [260]filename_dat
  xchg ecx, eax                             // ecx = Race
  call art_flush_
  lea  esi, [esp+0]                         // filename
  lea  edi, [esp+260]                       // filename_dat
// unload previous Dats
  mov  eax, ds:[_master_db_handle]
  test eax, eax
  jz   noHero
  mov  eax, [eax]                           // database.path
  call xremovepath_
  dec  eax
  mov  ds:[_master_db_handle], eax
noHero:
  mov  eax, ds:[_critter_db_handle]
  test eax, eax
  jz   noRace
  mov  eax, [eax]                           // database.path
  call xremovepath_
  dec  eax
  mov  ds:[_critter_db_handle], eax
noRace:
  call makeDataPath
  xor  edx, edx                             // mode = Существование файла (3 = Доступность для чтения)
  mov  eax, edi
  call access_
  test eax, eax                             // Есть dat-файл?
  jnz  noDatFile                            // Нет
  mov  eax, edi
  call xaddpath
  dec  eax                                  // Открыли dat-файл?
  jz   goodDat                              // Да
  inc  eax                                  // Ничего не добавили?
  jz   noDatFile                            // Да
  mov  eax, edi
  call xremovepath_
noDatFile:
  xor  edx, edx                             // mode = Существование файла (3 = Доступность для чтения)
  mov  eax, esi
  call access_
  test eax, eax                             // Есть каталог?
  jnz  shortFail                            // Нет
  mov  eax, esi
  call xaddpath
  inc  eax                                  // Удачно добавили путь?
shortFail:
  jnz  fail
goodDat:
  mov  eax, ds:[_paths]
  mov  ds:[_master_db_handle], eax          // set path for selected appearance
  test ebx, ebx
  jz   skip
  xor  ebx, ebx                             // Style
  call makeDataPath
  xor  edx, edx                             // mode = Существование файла (3 = Доступность для чтения)
  mov  eax, edi
  call access_
  test eax, eax                             // Есть dat-файл?
  jnz  noStyleDatFile                       // Нет
  mov  eax, edi
  call xaddpath
  dec  eax                                  // Открыли dat-файл?
  jz   goodStyleDat                         // Да
  inc  eax                                  // Ничего не добавили?
  jz   noStyleDatFile                       // Да
  mov  eax, edi
  call xremovepath_
noStyleDatFile:
  xor  edx, edx                             // mode = Существование файла (3 = Доступность для чтения)
  mov  eax, esi
  call access_
  test eax, eax                             // Есть каталог?
  jnz  end                                  // Нет
  mov  eax, esi
  call xaddpath
  inc  eax                                  // Удачно добавили путь?
  jnz  skip
goodStyleDat:
  mov  eax, ds:[_paths]                     // _critter_db_handle -> _master_db_handle -> _paths
  mov  ds:[_critter_db_handle], eax         // eax = _critter_db_handle
  mov  edx, [eax+0xC]                       // edx = _master_db_handle
  mov  ecx, [edx+0xC]                       // ecx = _paths
  mov  [eax+0xC], ecx                       // _critter_db_handle -> _paths
  mov  [edx+0xC], eax                       // _master_db_handle -> _critter_db_handle
  mov  ds:[_paths], edx                     // _master_db_handle -> _critter_db_handle -> _paths
skip:
  xor  eax, eax
  jmp  end
fail:
  xor  eax, eax
  dec  eax
end:
  add  esp, 260+260
  pop  ebx
  pop  ecx
  pop  edx
  pop  esi
  pop  edi
  retn
 }
}

static void __declspec(naked) SetHeroArt() {
// eax: 0 = normal range, 1 = hero range
 __asm {
  push edx
  push ecx
  push ebx
  xchg ecx, eax
  mov  eax, ds:[_obj_dude]                  // hero state struct
  mov  ebx, critterListCount
  mov  edx, [eax+0x20]                      // get hero FrmID
  and  edx, 0xFFF                           // Index
  cmp  edx, ebx                             // check if critter LST index is in Hero range
  ja   IsHero
  jecxz end
  add  [eax+0x20], ebx                      // shift index up into hero range
  jmp  end
IsHero:
  dec  ecx
  jecxz end
  sub  [eax+0x20], ebx                      // shift index down into normal critter range
end:
  pop  ebx
  pop  ecx
  pop  edx
  retn
 }
}

static void __declspec(naked) DrawPC() {
 __asm {
  push edx
  push ebx
  push eax
  sub  esp, 16
  mov  ebx, esp
  mov  eax, ds:[_obj_dude]
  mov  edx, [eax+0x20]
  call obj_change_fid_
  xchg ebx, eax
  mov  edx, ds:[_map_elevation]
  call tile_refresh_rect_
  add  esp, 16
  pop  eax
  pop  ebx
  pop  edx
  retn
 }
}

static void __declspec(naked) display_body() {
// eax = GNWID, ebx = x, ecx = y, edx = noWearFlag
 __asm {
  push edi
  push esi
  push ebp
  sub  esp, 16+4+4+4+4                      // [0]rect, [16]frm_ptr, [20]from, [24]width, [28]height
  xchg ebp, eax                             // ebp = GNWID
  mov  eax, ds:[_ticker]
  call elapsed_time_
  cmp  eax, cs:[0x47066B]                   // inventory rotation speed
  jb   end
  mov  edi, ds:[_curr_rot]
  inc  edi
  cmp  edi, 6
  jb   setRotation
  xor  edi, edi
setRotation:
  mov  ds:[_curr_rot], edi
  mov  [esp], ebx                           // rect.x
  mov  [esp+4], ecx                         // rect.y
  add  ebx, 59
  add  ecx, 99
  mov  [esp+8], ebx                         // rect.offx
  mov  [esp+12], ecx                        // rect.offy
  mov  eax, ds:[_art_vault_guy_num]         // current base dude model
  test edx, edx
  jnz  noWear
  mov  eax, ds:[_obj_dude]
  mov  eax, [eax+0x20]                      // pobj.fid (pointer to current armored hero critter FrmId)
  and  eax, 0xFFF                           // Index
  inc  edx
noWear:
  xchg edx, eax                             // eax = ObjType_Critter, edx = Index
  push edi                                  // ID3
  xor  ebx, ebx                             // ID2
  xor  ecx, ecx                             // ID1
  call art_id_
  lea  edx, [esp+16]
  call art_ptr_lock_
  test eax, eax
  jz   setTime
  xor  edx, edx                             // frame
  mov  ebx, edi                             // rotation
  mov  ecx, eax                             // frm_ptr
  call art_frame_data_
  mov  [esp+20], eax                        // from
  xor  edx, edx                             // frame
  mov  ebx, edi                             // rotation
  mov  eax, ecx                             // frm_ptr
  call art_frame_width_
  mov  esi, eax                             // from_width
  mov  ebx, 60
  cmp  eax, ebx
  jbe  skipWidth
  xchg ebx, eax
skipWidth:
  mov  [esp+24], eax                        // width
  xor  edx, edx                             // frame
  mov  ebx, edi                             // rotation
  mov  eax, ecx                             // frm_ptr
  call art_frame_length_
  mov  ebx, 100
  cmp  eax, ebx
  jbe  skipHeight
  xchg ebx, eax
skipHeight:
  mov  [esp+28], eax                        // height
  mov  eax, ebp                             // GNWID
  call win_get_buf_
  xchg edi, eax                             // edi = toSurface
  xchg ebp, eax                             // GNWID
  push eax
  call win_width_
  mov  ecx, eax                             // ToWidth
  push ecx
  mov  ebx, 100                             // Height
  push ebx
  movzx edx, byte ptr ds:[_DARK_GREY_Color]
  push edx                                  // ColorIndex
  imul eax, [esp+4+(4*4)]                   // eax = ToWidth * rect.y
  add  eax, [esp+0+(4*4)]                   // eax = ToWidth * rect.y + rect.x
  add  eax, edi                             // ToSurface
  mov  edx, 60                              // Width
  call buf_fill_
  pop  ecx
  pop  eax                                  // ToWidth
  mov  edx, [esp+28+(1*4)]                  // height
  mov  ebx, [esp+24+(1*4)]                  // width
  push edx                                  // height
  push ebx                                  // width
  push esi                                  // from_width
  push dword ptr [esp+20+(4*4)]             // from
  push eax                                  // to_width
  sub  ecx, edx                             // y = win_height - height
  shr  ecx, 1                               // y = y/2
  add  ecx, [esp+4+(6*4)]                   // y = rect.y + y
  imul eax, ecx                             // to_width * y
  add  edi, eax
  mov  eax, 60
  sub  eax, ebx                             // x = win_width - width
  shr  eax, 1                               // x = x/2
  add  eax, [esp+0+(6*4)]                   // x = rect.x + x
  add  edi, eax                             // edi = to_width * y + x
  push edi                                  // to
  call transSrcCopy_
  add  esp, 6*4
  pop  eax                                  // eax = GNWID
  mov  edx, esp                             // edx = *WinRect
  call win_draw_rect_
  mov  eax, [esp+16]
  call art_ptr_unlock_
setTime:
  call get_time_
  mov  ds:[_ticker], eax
end:
  add  esp, 16+4+4+4+4
  pop  ebp
  pop  esi
  pop  edi
  retn
 }
}

static void __declspec(naked) DrawCard() {
// eax = GNWID, ebx = x, ecx = y, edx = Style, esi = fromSurface
 __asm {
  push edi
  push ebp
  sub  esp, 4+4+4+4+4+8+260                 // [0]x, [4]y, [8]GNWID, [12]filename, [16]frm_ptr, [20]msgfile, [28]fullname
  mov  [esp+0], ebx                         // x
  mov  [esp+4], ecx                         // y
  mov  [esp+8], eax                         // GNWID
  mov  ebp, eax
  call win_get_buf_
  xchg edi, eax                             // edi = toSurface
  mov  eax, StyleName
  dec  edx
  jz   notRace
  mov  eax, RaceName
notRace:
  mov  [esp+12], eax                        // filename
  xchg ebp, eax                             // GNWID
  call win_width_
  mov  ebp, eax                             // ebp = to_width
  sub  ecx, 8
  sub  ebx, 8
  imul eax, ecx                             // eax = to_width * y
  add  eax, ebx                             // eax = to_width * y + x
  add  esi, eax
  add  eax, edi
  push 186                                  // height
  push 293                                  // width
  push ebp                                  // from_width
  push esi                                  // from
  push ebp                                  // to_width
  push eax                                  // to
  call srcCopy_
  add  esp, 6*4
  lea  edx, [esp+28]                        // fullname
  mov  ecx, _str
  push 0x5002FF                             // '.frm'
  push dword ptr [esp+12+4]
  push 0x5016A0                             // '%s%s'
  push ecx                                  // buf
  call sprintf_
  add  esp, 4*4
  push ecx
  push _art_skilldex+4                      // _art_skilldex.name
  push 0x5003A8                             // 'art\%s\%s'
  push edx                                  // buf
  call sprintf_
  add  esp, 4*4
  lea  eax, [esp+16]                        // frm_ptr
  xchg edx, eax                             // eax = fullname, edx = frm_ptr
  call load_frame_
  test eax, eax
  jnz  skipFRM
  mov  eax, [esp+16]                        // frm_ptr
  xor  edx, edx                             // frame
  xor  ebx, ebx                             // rotation
  mov  ecx, eax                             // frm_ptr
  call art_frame_data_
  xchg esi, eax
  xor  edx, edx                             // frame
  xor  ebx, ebx                             // rotation
  mov  eax, ecx                             // frm_ptr
  call art_frame_width_
  xor  edx, edx                             // frame
  xor  ebx, ebx                             // rotation
  xchg ecx, eax                             // frm_ptr
  call art_frame_length_
  push eax                                  // height
  mov  ebx, [esp+0+(1*4)]                   // x
  add  ebx, 139
  mov  eax, [esp+4+(1*4)]                   // y
  add  eax, 42
  imul eax, ebp                             // eax = to_width * y
  add  eax, ebx                             // eax = to_width * y + x
  add  eax, edi
  lea  ebx, [ecx-5]                         // из-за Олимп 2207
  push ebx                                  // width
//  push ecx                                  // width
  push ecx                                  // from_width
  push esi                                  // from
  push ebp                                  // to_width
  push eax                                  // to
  call srcCopy_
  add  esp, 6*4
  mov  eax, [esp+16]                        // frm_ptr
  call mem_free_
skipFRM:
  lea  esi, [esp+20]                        // msgfile
  xor  eax, eax
  mov  [esi], eax
  mov  [esi+4], eax
  lea  edx, [esp+28]                        // fullname
  mov  ecx, _str
  mov  ebx, 0x5016A0                        // '%s%s'
  push 0x50D174                             // '.msg'
  push dword ptr [esp+12+4]
  push ebx                                  // '%s%s'
  push ecx                                  // buf
  call sprintf_
  add  esp, 4*4
  push ecx
  push dword ptr ds:[_msg_path]
  push ebx                                  // '%s%s'
  push edx                                  // buf
  call sprintf_
  add  esp, 4*4
  mov  eax, esi                             // msgfile
  call message_load_
  dec  eax
  jnz  end
  mov  ebx, 100                             // number
  mov  edx, _mesg                           // msgdata
  push edx
  mov  eax, esi                             // msgfile
  call getmsg_
  mov  ds:[_folder_card_title], eax
  inc  ebx                                  // number
  pop  edx                                  // msgdata
  xchg esi, eax                             // msgfile
  call getmsg_
  mov  ds:[_folder_card_desc], eax
  push dword ptr ds:[_curr_font_num]        // store current font
  mov  eax, 102
  call text_font_                           // set font for title
  movzx eax, byte ptr ds:[_colorTable]
  push eax                                  // Color
  push eax                                  // Color
  push eax                                  // ColorIndex
  mov  eax, [esp+4+(4*4)]                   // y
  add  eax, 5
  imul eax, ebp                             // eax = to_width * y
  add  eax, 3
  add  eax, [esp+0+(4*4)]                   // x
  mov  edx, esi                             // DisplayText
  mov  ecx, ebp                             // ToWidth
  mov  ebx, 272                             // TxtWidth
  add  eax, edi                             // eax = ToSurface
  call ds:[_text_to_buf]
  call ds:[_text_height]
  xchg esi, eax
  mov  eax, [esp+8+(3*4)]                   // GNWID
  mov  ebx, [esp+4+(3*4)]                   // y
  add  ebx, 5
  add  ebx, esi                             // y_start
  mov  edx, [esp+0+(3*4)]                   // x
  add  edx, 3                               // x_start
  lea  ecx, [edx+265]                       // x_end
  push ebx
  call win_line_
  mov  eax, [esp+8+(2*4)]                   // GNWID
  mov  ebx, [esp+4+(2*4)]                   // y
  add  ebx, 6
  add  ebx, esi                             // y_start
  mov  edx, [esp+0+(2*4)]                   // x
  add  edx, 3                               // x_start
  lea  ecx, [edx+265]                       // x_end
  push ebx
  call win_line_
  mov  eax, 101
  call text_font_                           // set font for info
  call ds:[_text_height]
  inc  eax
  mov  [esp+12+(1*4)], eax                  // Style
  add  [esp+4+(1*4)], 48                    // y
  mov  edx, 160                             // maxWidth
  lea  ecx, [esp+16+(1*4)]                  // count
  lea  ebx, [esp+28+(1*4)]                  // buf
  mov  eax, ds:[_folder_card_desc]          // text
  call _word_wrap_
  test eax, eax
  jnz  skip
  cmp  word ptr [esp+16+(1*4)], 12
  jbe  noLimit
  mov  word ptr [esp+16+(1*4)], 12
noLimit:
  xor  esi, esi
  xor  ecx, ecx
  inc  ecx
loopText:
  mov  eax, [esp+4+(1*4)]                   // y
  imul eax, ebp                             // eax = to_width * y
  add  eax, 3
  add  eax, [esp+0+(1*4)]                   // eax = to_width * y + x
  lea  ebx, [edi+eax]                       // ToSurface
  xor  edx, edx
  mov  dx, [esp+esi+28+2+(1*4)]
  add  edx, ds:[_folder_card_desc]
  xor  eax, eax
  xchg [edx], al
  push edx
  push eax
  xor  edx, edx
  mov  dx, [esp+esi+28+(3*4)]
  add  edx, ds:[_folder_card_desc]          // DisplayText
  push ecx
  movzx eax, byte ptr ds:[_colorTable]
  push eax                                  // ColorIndex
  mov  ecx, ebp                             // ToWidth
  mov  eax, 160                             // TxtWidth
  xchg ebx, eax                             // eax = ToSurface, ebx = TxtWidth
  call ds:[_text_to_buf]
  pop  ecx
  pop  eax
  pop  edx
  mov  [edx], al
  mov  eax, [esp+12+(1*4)]                  // font_height
  add  [esp+4+(1*4)], eax                   // y
  add  esi, 2
  inc  ecx
  cmp  cx, [esp+16+(1*4)]
  jl   loopText
skip:
  xor  ecx, ecx
  mov  esi, ds:[_folder_card_title]
  mov  edi, _old_str1
  mov  edx, esi
  mov  eax, edi
  call strcmp_
  test eax, eax
  jz   skipSound
  cmp  ds:[_frstc_draw1], ecx
  je   skipSound
  mov  eax, 0x501890                        // 'isdxxxx1'
  call gsound_play_sfx_file_
skipSound:
  xchg edi, eax                             // destination
  mov  edx, esi                             // source
  mov  ebx, 48                              // num
  call strncpy_
  inc  ecx
  mov  ds:[_frstc_draw1], ecx
  dec  ecx
  dec  ecx
  mov  ds:[_old_fid1], ecx
  pop  eax
  call text_font_                           // restore previous font
  lea  eax, [esp+20]                        // msgfile
  call message_exit_
end:
  add  esp, 4+4+4+4+4+8+260
  pop  ebp
  pop  edi
  retn
 }
}

static void __declspec(naked) CreateButtons() {
// eax = GNWID, ebx = x, ecx = y, edx = toSurface, esi = LeftButtUp, edi = RightButtUp
 __asm {
  push ebp
  sub  esp, 4+4+4+4+4+4                     // [0]x, [4]y, [8]toSurface, [12]GNWID, [16]LeftButtUp, [20]RightButtUp
  mov  [esp+0], ebx                         // x
  mov  [esp+4], ecx                         // y
  mov  [esp+8], edx                         // toSurface
  mov  [esp+12], eax                        // GNWID
  mov  [esp+16], esi                        // LeftButtUp
  mov  [esp+20], edi                        // RightButtUp
  call win_width_
  xchg ebp, eax                             // ebp = to_width
  mov  edi, edx                             // toSurface
  xchg ecx, eax                             // eax = y
  imul eax, ebp                             // eax = to_width * y
  add  eax, ebx                             // eax = to_width * y + x
  add  edi, eax                             // to
  mov  esi, ds:[_grphbmp+30*4]              // art_pic (DONEBOX.FRM)
  mov  edx, ds:[_GInfo+30*8]                // from_width
  mov  ecx, ds:[_GInfo+30*8+4]              // from_height
  mov  ebx, 9
// button backround left
  push ecx                                  // height
  push ebx                                  // width
  push edx                                  // from_width
  push esi                                  // from
  push ebp                                  // to_width
  push edi                                  // to
  call srcCopy_
  add  esp, 6*4
// button backround center
  push ecx                                  // height
  push 50                                   // width
  push edx                                  // from_width
  lea  eax, [esi+37]
  push eax                                  // from
  push ebp                                  // to_width
  lea  eax, [edi+9]
  push eax                                  // to
  call srcCopy_
  add  esp, 6*4
// button backround right
  push ecx                                  // height
  push ebx                                  // width
  push edx                                  // from_width
  sub  edx, ebx
  lea  eax, [esi+edx]
  push eax                                  // from
  push ebp                                  // to_width
  lea  eax, [edi+9+50]
  push eax                                  // to
  call srcCopy_
  add  esp, 6*4
  xor  esi, esi
  mov  edi, 0x20
  mov  ebp, [esp+12]                        // GNWID
  mov  ebx, [esp+4]                         // y
  inc  ebx
  inc  ebx
  mov  edx, [esp+0]                         // x
  push edx
  push ebx
  push edi                                  // flags
  push esi
  push dword ptr ds:[_grphbmp+37*4]         // PicDown (sld.frm)
  push dword ptr ds:[_grphbmp+36*4]         // PicUp (slu.frm)
  dec  esi
  push dword ptr [esp+16+(6*4)]             // ButtUp
  push esi                                  // ButtDown
  push esi                                  // HovOff
  push esi                                  // HovOn
  mov  eax, ds:[_GInfo+36*8+4]              // Height
  push eax                                  // Height
  mov  ecx, ds:[_GInfo+36*8]                // Width
  add  edx, 31
  sub  edx, ecx                             // Xpos
  mov  eax, ebp                             // GNWID
  call win_register_button_
  inc  esi
  pop  ebx                                  // Ypos
  pop  edx                                  // x
  push edi                                  // flags
  push esi
  push dword ptr ds:[_grphbmp+39*4]         // PicDown (srd.frm)
  push dword ptr ds:[_grphbmp+38*4]         // PicUp (sru.frm)
  dec  esi
  push dword ptr [esp+20+(4*4)]             // ButtUp
  push esi                                  // ButtDown
  push esi                                  // HovOff
  push esi                                  // HovOn
  mov  eax, ds:[_GInfo+38*8+4]              // Height
  push eax                                  // Height
  mov  ecx, ds:[_GInfo+38*8]                // Width
  add  edx, 37                              // Xpos
  xchg ebp, eax                             // GNWID
  call win_register_button_
  add  esp, 4+4+4+4+4+4
  pop  ebp
  retn
 }
}

static const DWORD pic_id[9] = {
 10,                                        // SEXOFF.FRM (Character editor)
 30,                                        // DONEBOX.FRM (Character editor)
 36,                                        // slu.frm (Left arrow up)
 37,                                        // sld.frm (Left arrow down)
 38,                                        // sru.frm (Right arrow up)
 39,                                        // srd.frm (Right arrow down)
 41,                                        // OPBASE.FRM (Character editor)
 42,                                        // OPBTNOFF.FRM (Character editor)
 43,                                        // OPBTNON.FRM (Character editor)
};

static void __declspec(naked) CreateHeroWindow() {
// eax = GNWID, ebx = x, ecx = y, edx = toSurface
 __asm {
  push edi
  push esi
  push ebp
  sub  esp, 4+4+4+4                         // [0]x, [4]y, [8]toSurface, [12]GNWID
  mov  [esp+0], ebx                         // x
  mov  [esp+4], ecx                         // y
  mov  [esp+8], edx                         // toSurface
  mov  [esp+12], eax                        // GNWID
  call win_width_
  xchg ebp, eax                             // ebp = to_width
  xor  esi, esi
loopInit:
  mov  edi, pic_id[esi*4]
  xor  ecx, ecx                             // ID1
  xor  ebx, ebx                             // ID2
  push ebx                                  // ID3
  mov  edx, ds:[_grph_id][edi*4]            // Index
  mov  eax, ObjType_Intrface                // ObjType
  call art_id_
  lea  edx, ds:[_grph_key][edi*4]           // *frm_ptr
  lea  ecx, ds:[_GInfo][edi*8+4]            // *Height
  lea  ebx, ds:[_GInfo][edi*8]              // *Width
  call art_lock_
  mov  ds:[_grphbmp][edi*4], eax            // art_pic
  test eax, eax
  jz   failInit
  inc  esi
  cmp  esi, 9
  jb   loopInit
  je   goodInit
failInit:
  mov  ecx, esi
  xor  esi, esi
loopFail:
  mov  eax, pic_id[esi*4]
  mov  eax, ds:[_grph_key][eax*4]           // eax = frm_ptr
  call art_ptr_unlock_
  inc  esi
  loop loopFail
  xchg ecx, eax
  dec  eax
  jmp  end
goodInit:
  mov  edi, [esp+8]                         // toSurface
  mov  eax, [esp+4]                         // eax = y
  add  eax, 32                              // eax = y + 32
  imul eax, ebp                             // eax = to_width * (y + 32)
  add  eax, [esp]                           // eax = to_width * (y + 32) + x
  add  eax, edi
  mov  esi, ds:[_grphbmp+41*4]              // art_pic (OPBASE.FRM)
  mov  ecx, 98                              // height
  mov  ebx, 40                              // width
  mov  edx, ds:[_GInfo+41*8]                // from_width
// Верхний левый фон
  push ecx                                  // height
  push ebx                                  // width
  push edx                                  // from_width
  push esi                                  // from
  push ebp                                  // to_width
  push eax                                  // to
  call srcCopy_
  add  esp, 6*4
  push ecx                                  // height
  push ebx                                  // width
  push edx                                  // from_width
  push ecx                                  // height
  push ebx                                  // width
  push edx                                  // from_width
// Верхний правый фон
  push ecx                                  // height
  push ebx                                  // width
  push edx                                  // from_width
  add  eax, ebx                             // eax = to_width * (y + 32) + x + 40
  lea  ebx, [esi+edx-40]
  push ebx                                  // from
  push ebp                                  // to_width
  push eax                                  // to
  call srcCopy_
  add  esp, 6*4
// Нижний левый фон
  mov  eax, ds:[_GInfo+41*8+4]              // eax = from_height
  sub  eax, ecx                             // eax = from_height - 98
  imul eax, edx                             // eax = from_width * (from_height - 98)
  add  esi, eax
  mov  eax, [esp+4+(6*4)]                   // eax = y
  add  eax, 32+98                           // eax = y + 130
  imul eax, ebp                             // eax = to_width * (y + 130)
  add  eax, [esp+0+(6*4)]                   // eax = to_width * (y + 130) + x
  add  eax, edi
  push esi                                  // from
  push ebp                                  // to_width
  push eax                                  // to
  call srcCopy_
  add  esp, 6*4
// Нижний правый фон
  pop  edx                                  // from_width
  push edx                                  // from_width
  lea  esi, [esi+edx-40]                    // esi = from_width * (from_height - 98) + from_width - 40
  push esi                                  // from
  push ebp                                  // to_width
  add  eax, 40                              // eax = to_width * (y + 130) + x + 40
  push eax                                  // to
  call srcCopy_
  add  esp, 6*4
  mov  eax, [esp+4]                         // eax = y
  imul eax, ebp                             // eax = to_width * (y + 0)
  add  eax, [esp+0]                         // eax = to_width * (y + 0) + x
  lea  eax, [eax+edi+40]                    // eax = to_width * (y + 0) + x + 40
  mov  esi, ds:[_grphbmp+10*4]              // art_pic (SEXOFF.FRM)
  mov  ecx, ds:[_GInfo+10*8+4]              // Height
  mov  ebx, ds:[_GInfo+10*8+0]              // Width
  mov  edx, ebx
  shr  ebx, 1                               // width/2
  sub  eax, ebx                             // eax = to_width * (y + 0) + x + 40 - width/2
  push ecx                                  // Height
  push edx                                  // Width
  push ecx                                  // height
  push edx                                  // width
  push edx                                  // from_width
  push esi                                  // from (SEXOFF.FRM)
  push ebp                                  // to_width
  push ecx                                  // height
  push edx                                  // width
  push edx                                  // from_width
  push esi                                  // from (SEXOFF.FRM)
  push ebp                                  // to_width
// race button
  push eax                                  // to
  call srcCopy_
  add  esp, 6*4
  mov  eax, [esp+4+(7*4)]                   // eax = y
  add  eax, 228                             // eax = y + 228
  imul eax, ebp                             // eax = to_width * (y + 228)
  add  eax, [esp+0+(7*4)]                   // eax = to_width * (y + 228) + x
  lea  eax, [eax+edi+40]                    // eax = to_width * (y + 228) + x + 40
  sub  eax, ebx                             // eax = to_width * (y + 228) + x + 40 - width/2
// style button
  push eax                                  // to
  call srcCopy_
  add  esp, 6*4
  pop  ecx                                  // Width
  pop  edx                                  // Height
  push edx                                  // Height
  push ecx                                  // Width
  xor  esi, esi
  push esi                                  // flags
  push esi                                  // Unk
  push esi                                  // PicDown
  push esi                                  // PicUp
  dec  esi
  push esi                                  // ButtUp
  push 572                                  // ButtDown
  push esi                                  // HovOff
  push esi                                  // HovOn
  push edx                                  // Height
  mov  esi, [esp+0+(11*4)]                  // x
  add  esi, 40                              // esi = x + 40
  sub  esi, ebx                             // esi = x + 40 - width/2
  mov  edx, esi                             // Xpos
  mov  ebx, [esp+4+(11*4)]                  // Ypos
  mov  eax, [esp+12+(11*4)]                 // GNWID
  call win_register_button_
  pop  ecx                                  // Width
  pop  ebx                                  // Height
  mov  edx, esi                             // Xpos
  xor  esi, esi
  push esi                                  // flags
  push esi                                  // Unk
  push esi                                  // PicDown
  push esi                                  // PicUp
  dec  esi
  push esi                                  // ButtUp
  push 573                                  // ButtDown
  push esi                                  // HovOff
  push esi                                  // HovOn
  push ebx                                  // Height
  mov  ebx, [esp+4+(9*4)]                   // y
  add  ebx, 228                             // Ypos
  mov  eax, [esp+12+(9*4)]                  // GNWID
  call win_register_button_
  movzx eax, byte ptr ds:[_DARK_GREY_Color]
  push eax                                  // ColorIndex
  mov  eax, [esp+4+4]                       // eax = y
  add  eax, 79                              // eax = y + 79
  imul eax, ebp                             // eax = to_width * (y + 79)
  add  eax, [esp+0+4]                       // eax = to_width * (y + 79) + x
  lea  eax, [eax+edi+9]                     // eax = to_width * (y + 79) + x + 9
  mov  edx, 62                              // Width
  mov  ecx, ebp                             // ToWidth
  mov  ebx, 102                             // Height
  call buf_fill_
  movzx eax, byte ptr ds:[_LIGHT_GREY_Color]
  push eax                                  // Color
  mov  eax, [esp+4+4]                       // y
  lea  ecx, [eax+78]                        // Ypos
  lea  eax, [ecx+103]
  push eax                                  // Ypos_size
  mov  eax, [esp+0+8]                       // x
  lea  ebx, [eax+8]                         // Xpos
  lea  eax, [ebx+63]
  push eax                                  // Xpos_size
  mov  edx, ebp                             // ToWidth
  mov  eax, edi                             // toSurface
  call draw_box_
  push dword ptr ds:[_curr_font_num]
  mov  eax, 103
  call text_font_
  mov  ecx, ds:[_GInfo+10*8+4]              // Height
  call ds:[_text_height]
  sub  ecx, eax
  shr  ecx, 1
  inc  ecx
  mov  ebx, ds:[_GInfo+10*8+0]              // TxtWidth
  mov  eax, [esp+4+4]                       // eax = y
  add  eax, ecx                             // eax = y + 0
  imul eax, ebp                             // eax = to_width * (y + 0)
  add  eax, [esp+0+4]                       // eax = to_width * (y + 0) + x
  lea  esi, [eax+edi+40]                    // eax = to_width * (y + 0) + x + 40
  movzx eax, byte ptr ds:[_PeanutButter]
  push eax                                  // ColorIndex
  push ebx                                  // TxtWidth
  push ecx
  push eax                                  // ColorIndex
  mov  eax, offset RaceText
  mov  edx, eax                             // *DisplayText
  call ds:[_text_width]
  shr  eax, 1                               // TxtWidth/2
  mov  ecx, ebp                             // ToWidth
  sub  esi, eax
  xchg esi, eax                             // eax = ToSurface
  call ds:[_text_to_buf]
  pop  eax
  pop  ebx                                  // TxtWidth
  add  eax, [esp+4+8]                       // eax = y
  add  eax, 228                             // eax = y + 228
  imul eax, ebp                             // eax = to_width * (y + 228)
  add  eax, [esp+0+8]                       // eax = to_width * (y + 228) + x
  lea  esi, [eax+edi+40]                    // eax = to_width * (y + 228) + x + 40
  mov  eax, offset StyleText
  mov  edx, eax                             // *DisplayText
  call ds:[_text_width]
  shr  eax, 1                               // TxtWidth/2
  mov  ecx, ebp                             // ToWidth
  sub  esi, eax
  xchg esi, eax                             // eax = ToSurface
  call ds:[_text_to_buf]
  pop  eax
  call text_font_
  xor  ebx, ebx
  cmp  IsControllingNPC, ebx                // Контроль персонажей?
  jne  skipButtons                          // Да
  mov  eax, [esp+12]                        // GNWID
  cmp  eax, ds:[_edit_win]
  jne  createButtons
  cmp  ds:[_glblmode], ebx                  // Редактирование персонажа?
  je   skipButtons                          // Да
createButtons:
  mov  edx, [esp+8]                         // toSurface
  mov  ecx, [esp+4]                         // y
  mov  ebx, [esp+0]                         // x
  add  ebx, 6
  cmp  byte ptr raceExist, 0
  je   skip
  push edx
  push ecx
  push ebx
  push eax
  mov  edi, 576                             // RightButtUp
  mov  esi, 574                             // LeftButtUp
  add  ecx, 49
  call CreateButtons                        // race selection buttons
  pop  eax
  pop  ebx
  pop  ecx
  pop  edx
skip:
  cmp  byte ptr styleExist, 0
  je   skipButtons
  mov  edi, 577                             // RightButtUp
  mov  esi, 575                             // LeftButtUp
  add  ecx, 187
  call CreateButtons                        // style selection buttons
skipButtons:
  xor  eax, eax
end:
  add  esp, 4+4+4+4
  pop  ebp
  pop  esi
  pop  edi
  retn
 }
}

static void __declspec(naked) DestroyHeroWindow() {
 __asm {
  push esi
  push ecx
  push eax
  mov  ecx, 9
  xor  esi, esi
loopUnlock:
  mov  eax, pic_id[esi*4]
  mov  eax, ds:[_grph_key][eax*4]           // eax = frm_ptr
  call art_ptr_unlock_
  inc  esi
  loop loopUnlock
  pop  eax
  pop  ecx
  pop  esi
  retn
 }
}

static void __declspec(naked) art_init_hook() {
 __asm {
  push edi
  push esi
// edi = _art_critters.mem, ebp = _art_critters.count, esi = _art_critters.name
  mov  eax, [ebp]                           // eax = _art_critters.count
  mov  ecx, eax
  imul ebx, eax, 13                         // ebx = _art_critters.count * 13
  shl  eax, 1                               // double critter list count to add room for hero art
  mov  [ebp], eax
  mov  eax, ebx
  shl  eax, 1                               // (_art_critters.count * 13) * 2
  call mem_malloc_
  test eax, eax                             // Удачно выделили память?
  jnz  skip                                 // Да
  mov  [ebp], ecx                           // Восстановим _art_critters.count
  mov  raceExist, al
  mov  styleExist, al
  xchg ecx, eax
  jmp  end
skip:
  mov  critterListCount, ecx
  mov  critterArraySize, ebx
  mov  edx, [edi]                           // source
  push edx
  call memmove_                             // eax = destination, edx = source, ebx = num
  mov  [edi], eax                           // _art_critters.mem указывает на выделенный блок памяти
  lea  edi, [eax+ebx]                       // edi = *HeroList (set start of hero critter list after regular critter list)
  xchg esi, eax                             // esi = *CritList
  pop  eax
  call mem_free_                            // eax = mem
  xor  edx, edx                             // value
  mov  eax, edi
  call memset_                              // eax = destination, edx = value, ebx = num
loopCopy:                                   // copy critter name list to hero name list 
  mov  byte ptr [edi], '_'                  // insert a '_' char at the front of new hero critt names (fallout wont load the same name twice)
  lea  eax, [edi+1]                         // destination
  mov  edx, esi                             // source
  mov  ebx, 11                              // num
  call strncpy_
  add  esi, 13
  add  edi, 13
  loop loopCopy
end:
  inc  ecx
  pop  esi
  pop  edi
  retn
 }
}

// adjust base hero art
static void __declspec(naked) art_init_hook1() {
 __asm {
  mov  eax, critterListCount
  mov  ecx, 5
  mov  esi, _art_vault_guy_num
loopArt:
  add  [esi], eax
  add  esi, 4
  loop loopArt
  retn
 }
}

// insert main path structure in front when not loading
static void __declspec(naked) xfopen_hook() {
 __asm {
  mov  ecx, ds:[_paths]
  cmp  byte ptr [esi], 'r'
  je   isReading
  mov  eax, ds:[_critter_db_handle]
  test eax, eax
  jnz  skip
  mov  eax, ds:[_master_db_handle]
  test eax, eax
  jz   isReading
skip:
  mov  ecx, [eax+0xC]                       // database.next = _paths
isReading:
  push 0x4DEEEB
  retn
 }
}

static void __declspec(naked) art_get_name_hook() {
 __asm {
  mov  eax, _art_name
  mov  ecx, critterArraySize
  jecxz end
  cmp  esi, ecx                             // check if loading hero art
  jbe  end
  push eax
  sub  esp, 4
  mov  edx, esp
  call db_dir_entry_
  add  esp, 4
  inc  eax
  pop  eax
  jnz  end
// if file not found load regular critter art instead
  sub  esi, ecx
  pop  eax                                  // drop func ret address
  push 0x4194E2
end:
  retn
 }
}

//adjust armor art if num below hero art range
static void __declspec(naked) adjust_fid_hook() {
 __asm {
  call art_id_
  mov  edx, eax
  and  edx, 0xFFF
  cmp  edx, critterListCount                // check if critter art in PC range
  ja   end
  add  eax, critterListCount                // shift critter art index up into hero range
end:
  push 0x4717D6
  retn
 }
}

static void __declspec(naked) editor_design_hook() {
 __asm {
  mov  ds:[_frame_time], eax
  xor  edx, edx                             // noWearFlag
  mov  eax, ds:[_edit_win]                  // GNWID
  mov  ebx, 341                             // x
  mov  ecx, 80                              // y
  jmp  display_body
 }
}

static void __declspec(naked) checkHeroButtons() {
// eax = button, ebp = _info_line
 __asm {
  push ebp
  push edx
  push ecx
  push ebx
//  mov  ebp, ds:[_info_line]
  lea  ecx, [ebp-98]                        // 98 (Race)
  jecxz switchSetButton
  dec  ecx                                  // 99 (Style)
  jnz  afterSetButton
  inc  ecx
switchSetButton:
  cmp  eax, 0x14B                           // button left?
  je   buttonSetLeftRight
  cmp  eax, 0x14D                           // button right?
  jne  afterSetButton
buttonSetLeftRight:
  lea  eax, [eax+0xF3]
  jecxz afterSetButton
  inc  eax
afterSetButton:
  mov  ebx, CurrentAppearance[4]            // styleVal
  mov  ecx, CurrentAppearance[0]            // raceVal
  xor  edx, edx                             // drawFlag
  mov  ebp, 98
  sub  eax, 572
  jz   redraw                               // race button pushed (572)
  inc  ebp
  dec  eax
  jz   redraw                               // style button pushed (573)
  cmp  IsControllingNPC, edx                // Контроль персонажей?
  jne  afterSwitchButton                    // Да
  cmp  ds:[_glblmode], edx                  // Редактирование персонажа?
  je   afterSwitchButton                    // Да
  dec  eax
  jz   raceLeftButton                       // race left button pushed (574)
  dec  eax
  jz   styleLeftButton                      // style left button pushed (575)
  dec  eax
  jz   raceRightButton                      // race right button pushed (576)
  dec  eax
  jnz  afterSwitchButton
// style right button pushed (577)
  cmp  styleExist, dl
  je   afterSwitchButton
  inc  ebx
  mov  eax, ecx
  call LoadHeroDat
  inc  eax
  jnz  redraw
  dec  ebx
  jmp  loadPrevious
styleLeftButton:
  cmp  styleExist, dl
  je   afterSwitchButton
  test ebx, ebx
  jz   firstStyle
  dec  ebx
firstStyle:
  mov  eax, ecx
  call LoadHeroDat
  inc  eax
  jnz  redraw
  xchg ebx, eax
  jmp  loadPrevious
raceRightButton:
  cmp  raceExist, dl
  je   afterSwitchButton
  dec  ebp
  xor  ebx, ebx
  inc  ecx
  mov  eax, ecx
  call LoadHeroDat
  inc  eax
  jnz  redraw
  dec  ecx
  jmp  loadPrevious
raceLeftButton:
  cmp  raceExist, dl
  je   afterSwitchButton
  dec  ebp
  xor  ebx, ebx                             // todo: решить - сбрасывать ли стиль если раса уже первая
  jecxz firstRace
  dec  ecx
firstRace:
  mov  eax, ecx
  call LoadHeroDat
  inc  eax
  jnz  redraw
  xchg ecx, eax
loadPrevious:
  mov  eax, ecx
  call LoadHeroDat
redraw:
  inc  edx
  mov  ds:[_info_line], ebp
afterSwitchButton:
  mov  CurrentAppearance[4], ebx            // styleVal
  mov  CurrentAppearance[0], ecx            // raceVal
  xchg edx, eax
  pop  ebx
  pop  ecx
  pop  edx
  pop  ebp
  retn
 }
}

static void __declspec(naked) editor_design_Tab_hook() {
// ebp = _info_line
 __asm {
  push 0x4326C0
  cmp  ebp, 78
  ja   skip
  mov  ebp, 98
  retn
skip:
/*  cmp  ebp, 98
  ja   end
  inc  ebp
  retn
end:*/
  xor  ebp, ebp
  retn
 }
}                

static void __declspec(naked) editor_design_Up_hook() {
// ebp = _info_line
 __asm {
  cmp  ebp, 82
  je   skip
  cmp  ebp, 98
  jne  end
  sub  ebp, 14
skip:
  add  ebp, 16
end:
  dec  ebp
  push 0x4327A9
  retn
 }
}                

static void __declspec(naked) editor_design_Down_hook() {
// ebp = _info_line
 __asm {
  cmp  ebp, 97
  je   skip
  cmp  ebp, 99
  jne  end
  add  ebp, 14
skip:
  sub  ebp, 16
end:
  inc  ebp
  push 0x4328B4
  retn
 }
}                

static void __declspec(naked) editor_design_Other_hook() {
// edx = button, ebp = _info_line
 __asm {
  xchg edx, eax
  call checkHeroButtons
  dec  eax
  jnz  end
  pop  eax                                  // Уничтожаем адрес возврата
  push 0x4328CC
end:
  mov  eax, ds:[_edit_win]
  retn
 }
}                

static void __declspec(naked) SexWindow_call() {
 __asm {
  mov  edx, STAT_gender
  mov  eax, ds:[_obj_dude]
  push eax
  push edx
  call stat_level_
  pop  edx
  xchg ecx, eax
  call SexWindow_                           // call gender selection window
  pop  eax
  call stat_level_
  cmp  ecx, eax                             // check if gender has been changed
  je   end
  movzx ecx, byte ptr ds:[_gmovie_played_list+3]//check if wearing vault suit
  jecxz skip
  inc  eax
  inc  eax
skip:
  shl  eax, 2
  mov  eax, ds:[_art_vault_person_nums][eax]// Порядковый номер имени файла в _art_critters для базовой модели
  mov  ds:[_art_vault_guy_num], eax         // current base dude model
  xor  eax, eax
  mov  CurrentAppearance[4], eax            // reset style and race to defaults
  mov  CurrentAppearance[0], eax
  call RefreshPCArt
  pop  eax                                  // Уничтожаем адрес возврата
  push 0x431E85
end:
  retn
 }
}

// Отрисовка описания
static void __declspec(naked) DrawInfoWin_hook() {
 __asm {
  cmp  ecx, 81
  je   end
  sub  ecx, 98                              // Check If Race or Style selected to redraw info note
  jecxz skip
  dec  ecx
  jnz  end
  inc  ecx
skip:
  mov  edx, ecx
  mov  eax, ds:[_edit_win]
  mov  ebx, 345
  mov  ecx, 267
  mov  esi, ds:[_bckgnd]
  call DrawCard
  pop  eax                                  // Уничтожаем адрес возврата
  push 0x436C3C
end:
  retn
 }
}

static void __declspec(naked) CharEditStart_hook() {
 __asm {
  mov  ds:[_edit_win], eax
  inc  eax                                  // Удачно создано окно?
  jz   skip                                 // Нет
  dec  eax
  xchg ebp, eax                             // ebp = GNWID
  mov  eax, 640*480
  mov  ebx, eax
  call mem_malloc_
  test eax, eax                             // Удачно выделили память?
  jz   fail                                 // Нет
  push ebp
  mov  ebp, 640
  mov  edx, ds:[_bckgnd]
  mov  ds:[_bckgnd], eax
// Копируем оригинал
  mov  esi, edx
  call memmove_                             // eax = destination, edx = source, ebx = num
  xchg edi, eax
// shift behind button rail
  push 258                                  // height
  push 52                                   // width
  push ebp                                  // from_width
  lea  eax, [esi+640*0+331]
  push eax                                  // from
  push ebp                                  // to_width
  lea  eax, [edi+640*0+411]
  push eax                                  // to
  call srcCopy_
  add  esp, 6*4
// copy a blank part of the Tag Skill Bar hiding the old counter
  mov  ecx, 27
  push ecx                                  // height
  push 113                                  // width
  push ebp                                  // from_width
  lea  eax, [esi+640*227+381]
  push eax                                  // from
  push ebp                                  // to_width
  lea  eax, [edi+640*227+454]
  push eax                                  // to
  call srcCopy_
  add  esp, 6*4
// copy Tag Skill Counter background to the right
  push ecx                                  // height
  push 33                                   // width
  push ebp                                  // from_width
  lea  eax, [esi+640*227+519]
  push eax                                  // from
  push ebp                                  // to_width
  lea  eax, [edi+640*227+567]
  push eax                                  // to
  call srcCopy_
  add  esp, 6*4
// Сдвиг верхнего описания (из-за Олимп 2207)
  push 19                                   // height
  push 155                                  // width
  push ebp                                  // from_width
  lea  eax, [esi+640*0+413]
  push eax                                  // from
  push ebp                                  // to_width
  lea  eax, [edi+640*0+450]
  push eax                                  // to
  call srcCopy_
  add  esp, 6*4
  pop  ebp                                  // GNWID
  mov  ebx, 331                             // x
  xor  ecx, ecx                             // y
  mov  edx, edi                             // toSurface
  mov  eax, ebp                             // GNWID
  call CreateHeroWindow
  inc  eax
  mov  eax, ebp
  jnz  end
  xchg edi, eax
  call mem_free_
fail:
  xchg ebp, eax
  call win_delete_
  xor  eax, eax
skip:
  dec  eax
end:
  retn
 }
}

static void __declspec(naked) CharEditEnd_hook() {
 __asm {
  call art_ptr_unlock_
  call DestroyHeroWindow
  mov  eax, ds:[_bckgnd]
  jmp  mem_free_
 }
}

// Reset Appearance when selecting "Create Character" from the New Char screen
static void __declspec(naked) select_character_hook() {
 __asm {
  dec  eax
  mov  CurrentAppearance[4], eax            // reset style and race to defaults
  mov  CurrentAppearance[0], eax
  call RefreshPCArt
  inc  eax
  jmp  editor_design_
 }
}

// Load Appearance data from GCD file
static void __declspec(naked) pc_load_data_hook() {
 __asm {
  xor  ebx, ebx
  inc  ebx
  inc  ebx
  mov  edx, offset CurrentAppearance
  call db_freadIntCount_
  inc  eax
  jnz  skip
  mov  CurrentAppearance[4], eax            // Style
  mov  CurrentAppearance[0], eax            // Race
skip:
  call RefreshPCArt
  xchg esi, eax
  jmp  db_fclose_
 }
}

// Save Appearance data to GCD file
static void __declspec(naked) pc_save_data_hook() {
 __asm {
  xor  ebx, ebx
  inc  ebx
  inc  ebx
  mov  edx, offset CurrentAppearance
  call db_fwriteIntCount_
  xchg esi, eax
  jmp  db_fclose_
 }
}

//Adjust PC SFX Name
static void __declspec(naked) gsound_load_sound_hook() {
 __asm {
//Skip Underscore char at the start of PC App Name
  cmp  byte ptr [ebx], 0x5F                 // check if Name begins with an '_' character
  jne  end
  inc  ebx                                  // shift address to next char
end:
  jmp  gsound_get_sound_ready_for_effect_
 }
}

//return hero art val to normal before saving
static void __declspec(naked) obj_save_dude_call() {
 __asm {
  push eax
  xor  eax, eax                             // set hero FrmID LST index to normal range before saving
  call SetHeroArt
  pop  eax
  call obj_save_dude_                       // save current hero state structure fuction
  push eax
  xor  eax, eax
  inc  eax                                  // return hero FrmID LST index back to hero art range after saving hero state structure
  call SetHeroArt
  pop  eax
  retn
 }
}

static void __declspec(naked) combat_get_loc_name_hook() {
 __asm {
  cmp  eax, critterListCount                // check if critter art in PC range
  jbe  end
  sub  eax, critterListCount                // shift critter art index down out of hero range
end:
  jmp  art_alias_num_
 }
}

void __declspec(naked) HeroSelectWindow() {
// eax = RaceStyleFlag
 __asm {
  pushad
  test dword ptr InLoop, WORLDMAP+ESCMENU+SAVEGAME+LOADGAME+OPTIONS+HELP+CHARSCREEN+PIPBOY+AUTOMAP+SKILLDEX+INTFACELOOT+BARTER+HEROWIN
  jnz  end
  xchg ecx, eax
  mov  eax, ds:[_intfaceEnabled]
  dec  eax
  jnz  end
  cmp  critterListCount, eax
  je   end
  cmp  IsControllingNPC, eax                // Контроль персонажей?
  jne  end                                  // Да
  or   dword ptr InLoop, HEROWIN            // Чтобы исключить повторный вызов
  movzx eax, byte ptr styleExist
  push eax
  movzx eax, byte ptr raceExist
  push eax
  jecxz disableStyle
  mov  raceExist, ah
  jmp  checkRun
disableStyle:
  mov  styleExist, ah
checkRun:
  call map_disable_bk_processes_
  push eax
  test eax, eax
  jz   skip_gmouse_3d_is_on
  call gmouse_3d_is_on_
skip_gmouse_3d_is_on:
  push eax
  dec  eax
  jnz  skip_gmouse_3d_off
  call gmouse_3d_off_
skip_gmouse_3d_off:
  mov  eax, ds:[_mouse_is_hidden]
  push eax
  dec  eax
  jnz  skip_mouse_show
  call mouse_show_
skip_mouse_show:
  push dword ptr ds:[_gmouse_current_cursor]// get current mouse pic ref
  push dword ptr ds:[_info_line]
  push dword ptr ds:[_glblmode]
  push dword ptr ds:[_curr_font_num]        // store current font
  xor  eax, eax
  inc  eax
  call gmouse_set_cursor_                   // set mouse pic
  sub  esp, 4+4+4+4+8+16+4+4+4+4            // [0]WinMem, [4]frm_ptr, [8]Width, [12]Height, [16]msgfile, [24]msgdata, [40]UpDone, [44]UpCancel, [48]DownDone, [52]DownCancel
  mov  eax, (80+314)*260
  call mem_malloc_
  test eax, eax                             // Удачно выделили память?
  jz   restore                              // Нет
  mov  [esp+0], eax
  xor  edi, edi
  xchg edi, eax
  mov  ds:[_frstc_draw1], eax
  inc  eax
  mov  ds:[_glblmode], eax
  lea  eax, [ecx+98]
  mov  ds:[_info_line], eax
  mov  esi, 314
  mov  ebp, 80+314
  xor  ecx, ecx                             // ID1
  xor  ebx, ebx                             // ID2
  push ebx                                  // ID3
  mov  edx, 86                              // Index (PERKWIN.FRM)
  mov  eax, ObjType_Intrface                // ObjType
  call art_id_
  lea  edx, [esp+4]                         // *frm_ptr
  lea  ecx, [esp+12]                        // *Height
  lea  ebx, [esp+8]                         // *Width
  call art_lock_
  test eax, eax
  jz   freeBuffer
  push dword ptr [esp+12]                   // height
  push esi                                  // width
  mov  edx, [esp+8+(2*4)]                   // from_width
  push edx
  sub  edx, esi
  add  eax, edx
  push eax                                  // from
  push ebp                                  // to_width
  lea  eax, [edi+80]
  push eax                                  // to
  call srcCopy_
  add  esp, 6*4
  mov  eax, [esp+4]                         // eax = frm_ptr
  call art_ptr_unlock_
  xor  ecx, ecx                             // ID1
  xor  ebx, ebx                             // ID2
  push ebx                                  // ID3
  mov  edx, 362                             // Index (WMINFCE3.FRM)
  mov  eax, ObjType_Intrface                // ObjType
  call art_id_
  lea  edx, [esp+4]                         // *frm_ptr
  lea  ecx, [esp+12]                        // *Height
  lea  ebx, [esp+8]                         // *Width
  call art_lock_
  test eax, eax
  jz   freeBuffer
  mov  ebx, [esp+12]                        // Height
  mov  edx, [esp+8]                         // Width
  push 15                                   // height
  push esi                                  // width
  push edx                                  // from_width
  imul edx, edx, 12                         // edx = from_width * y
  lea  eax, [eax+edx+20]                    // eax = from_width * y + x
  push eax                                  // from
  push ebp                                  // to_width
  imul eax, ebp, 230                        // eax = to_width * y
  lea  eax, [edi+eax+80]
  push eax                                  // to
  call srcCopy_
  add  esp, 6*4
  mov  eax, [esp+4]                         // eax = frm_ptr
  call art_ptr_unlock_
  xor  ecx, ecx                             // ID1
  xor  ebx, ebx                             // ID2
  push ebx                                  // ID3
  mov  edx, 359                             // Index (WMINFCE0.FRM)
  mov  eax, ObjType_Intrface                // ObjType
  call art_id_
  lea  edx, [esp+4]                         // *frm_ptr
  lea  ecx, [esp+12]                        // *Height
  lea  ebx, [esp+8]                         // *Width
  call art_lock_
  test eax, eax
  jz   freeBuffer
  mov  edx, [esp+8]                         // Width
  push 15                                   // height
  push esi                                  // width
  push edx                                  // from_width
  imul edx, edx, 5                          // edx = from_width * y
  lea  eax, [eax+edx+20]                    // eax = from_width * y + x
  push eax                                  // from
  push ebp                                  // to_width
  imul eax, ebp, 245                        // eax = to_width * y
  lea  eax, [edi+eax+80]
  push eax                                  // to
  call srcCopy_
  add  esp, 6*4
  mov  eax, [esp+4]                         // eax = frm_ptr
  call art_ptr_unlock_
  mov  eax, ds:[_scr_size+12]               // _scr_size.offy
  sub  eax, ds:[_scr_size+4]                // _scr_size.y
  sub  eax, 259
  shr  eax, 1
  inc  eax
  xchg edx, eax                             // edx = y
  mov  eax, ds:[_scr_size+8]                // _scr_size.offx
  sub  eax, ds:[_scr_size]                  // _scr_size.x
  inc  eax                                  // win_width
  sub  eax, ebp
  shr  eax, 1
  inc  eax                                  // eax = x
  push 0x1C                                 // flags (MoveOnTop + Hidden + Exclusive)
  mov  ecx, 260                             // height
  mov  ebx, ebp                             // width
  push 0x100                                // color
  call win_add_
  mov  [esp+4], eax
  inc  eax
  jz   freeBuffer
  dec  eax
  mov  esi, eax
  call win_get_buf_
  xor  ebx, ebx                             // x
  xor  ecx, ecx                             // y
  mov  edx, edi                             // toSurface
  xchg esi, eax                             // eax = GNWID, esi = buffer
  call CreateHeroWindow
  inc  eax
  jz   deleteWin
  mov  ebx, (80+314)*260
  mov  edx, edi
  mov  eax, esi
  call memmove_                             // eax = destination, edx = source, ebx = num
  push _editor_msg
  push dword ptr ds:[_msg_path]
  push 0x5016A0                             // '%s%s'
  mov  edx, _str
  push edx
  call sprintf_
  add  esp, 4*4
  lea  eax, [esp+16]                        // msgfile
  xor  ecx, ecx
  mov  [eax], ecx
  mov  [eax+4], ecx
  call message_load_
  dec  eax
  jnz  deleteWin
  inc  ecx
  inc  ecx
  lea  esi, [esp+40]
  mov  ebx, ds:[_GInfo+42*8]                // Width
  imul ebx, ds:[_GInfo+42*8+4]              // Height
  mov  edx, ds:[_grphbmp+42*4]              // OPBTNOFF.FRM
loopUp:
  mov  eax, ebx
  call mem_malloc_
  mov  [esi], eax
  test eax, eax
  jz   deleteMemUp
  push edx
  call memmove_                             // eax = destination, edx = source, ebx = num
  pop  edx
  add  esi, 4
  loop loopUp
  inc  ecx
  inc  ecx
  mov  edx, ds:[_grphbmp+43*4]              // OPBTNON.FRM
loopDown:
  mov  eax, ebx
  call mem_malloc_
  mov  [esi], eax
  test eax, eax
  jz   deleteMemDown
  push edx
  call memmove_                             // eax = destination, edx = source, ebx = num
  pop  edx
  add  esi, 4
  loop loopDown
  mov  eax, 103
  call text_font_
  mov  edi, ds:[_GInfo+42*8+4]              // Height
  call ds:[_text_height]
  sub  edi, eax
  shr  edi, 1
  inc  edi                                  // edi = y
  imul edi, ds:[_GInfo+42*8]                // edi = to_width * y
  mov  ebx, 100                             // number (DONE)
  lea  edx, [esp+24]                        // msgdata
  lea  eax, [esp+16]                        // msgfile
  call getmsg_
  mov  edx, eax                             // edx = DisplayText
  call ds:[_text_width]
  xchg ebx, eax                             // ebx = TxtWidth
  mov  eax, ds:[_GInfo+42*8]
  mov  ecx, eax                             // ToWidth
  sub  eax, ebx
  shr  eax, 1                               // eax = x = (ToWidth - TxtWidth) / 2
  movzx ebp, byte ptr ds:[_PeanutButter]
  push ebp                                  // ColorIndex
  push eax
  push edx
  push ecx
  push ebx
  push ebp                                  // ColorIndex
  add  eax, [esp+40+(6*4)]                  // [40]UpDone
  add  eax, edi                             // eax = x + buffer + to_width * y
  call ds:[_text_to_buf]
  pop  ebx                                  // TxtWidth
  pop  ecx                                  // ToWidth
  pop  edx                                  // DisplayText
  pop  eax                                  // x
  add  eax, [esp+48+(1*4)]                  // [48]DownDone
  add  eax, edi                             // eax = x + buffer + to_width * y
  call ds:[_text_to_buf]
  mov  ebx, 102                             // number (CANCEL)
  lea  edx, [esp+24]                        // msgdata
  lea  eax, [esp+16]                        // msgfile
  call getmsg_
  mov  edx, eax                             // edx = DisplayText
  call ds:[_text_width]
  xchg ebx, eax                             // ebx = TxtWidth
  mov  eax, ds:[_GInfo+42*8]
  mov  ecx, eax                             // ToWidth
  sub  eax, ebx
  shr  eax, 1                               // eax = x = (ToWidth - TxtWidth) / 2
  push ebp                                  // ColorIndex
  push eax
  push edx
  push ecx
  push ebx
  push ebp
  add  eax, [esp+44+(6*4)]                  // [44]UpCancel
  add  eax, edi                             // eax = x + buffer + to_width * y
  call ds:[_text_to_buf]
  pop  ebx
  pop  ecx
  pop  edx
  pop  eax
  add  eax, [esp+52+4]                      // [52]DownCancel
  add  eax, edi                             // eax = x + buffer + to_width * y
  call ds:[_text_to_buf]
  xor  esi, esi
  mov  edi, 0x20
  mov  eax, ds:[_GInfo+42*8+4]              // Height
  mov  ebp, ds:[_GInfo+42*8]                // Width
  push edi                                  // flags
  push esi                                  // Unk
  push dword ptr [esp+52+(2*4)]             // PicDown (DownDone)
  push dword ptr [esp+44+(3*4)]             // PicUp (UpDone)
  dec  esi
  push 0x1F6                                // ButtUp (Esc)
  push esi                                  // ButtDown
  push esi                                  // HovOff
  push esi                                  // HovOn
  push eax                                  // Height
  inc  esi
  push edi                                  // flags
  push esi                                  // Unk
  push dword ptr [esp+48+(11*4)]            // PicDown (DownDone)
  push dword ptr [esp+40+(12*4)]            // PicUp (UpDone)
  dec  esi
  push 0x1F4                                // ButtUp (Enter)
  push esi                                  // ButtDown
  push esi                                  // HovOff
  push esi                                  // HovOn
  push eax                                  // Height
  mov  esi, 260
  sub  esi, eax                             // YPos = 260 - Height
  mov  edi, 314/2
  sub  edi, ebp
  shr  edi, 1
  lea  edx, [edi+80+0]                      // XPos
  mov  ecx, ebp                             // Width
  mov  ebx, esi                             // YPos
  mov  eax, [esp+4+(18*4)]                  // GNWID
  call win_register_button_
  lea  edx, [edi+80+314/2]                  // XPos
  mov  ecx, ebp                             // Width
  mov  ebx, esi                             // YPos
  mov  eax, [esp+4+(9*4)]                   // GNWID
  mov  ebp, eax
  call win_register_button_
  mov  eax, ds:[_info_line]
  xchg ebp, eax
  call win_show_
  mov  eax, CurrentAppearance[0]
  mov  [esp+8], eax
  mov  eax, CurrentAppearance[4]
  mov  [esp+12], eax
  xor  eax, eax
  inc  eax
loopButton:
  mov  ds:[_info_line], ebp
  dec  eax
  mov  eax, [esp+4]                         // GNWID
  jnz  skipRedraw
  lea  edx, [ebp-98]                        // Check If Race or Style selected to redraw info note
  mov  ebx, 96                              // x
  mov  ecx, 24                              // y
  mov  esi, [esp+0]
  push eax
  call DrawCard
  pop  eax
  push eax
  call win_draw_
  pop  eax
skipRedraw:
  xor  edx, edx
  inc  edx                                  // noWearFlag
  mov  ebx, 10                              // x
  mov  ecx, 80                              // y
  call display_body
  call get_input_
  inc  eax
  jnz  haveKey
  cmp  ds:[_game_user_wants_to_quit], eax
  jne  restoreAppearance
haveKey:
  dec  eax
  cmp  eax, 0x1F4                           // button = done
  je   saveAppearance
  cmp  eax, 0xD                             // button = return
  je   saveAppearance
  cmp  eax, 'd'                             // button = done
  je   saveAppearance
  cmp  eax, 'D'                             // button = done
  je   saveAppearance
  cmp  eax, 0x1F6                           // button = cancel
  je   restoreAppearance
  cmp  eax, 0x1B                            // button = esc
  je   restoreAppearance
  cmp  eax, 'c'                             // button = cancel
  je   restoreAppearance
  cmp  eax, 'C'                             // button = cancel
  je   restoreAppearance
//  cmp  eax, 0x9                             // tab button pushed
//  je   itsChange
  cmp  eax, 0x148                           // button up?
  je   itsChange
  cmp  eax, 0x150                           // button down?
  je   itsChange
  call checkHeroButtons
  mov  ebp, ds:[_info_line]
  jmp  loopButton
itsChange:
  xor  eax, eax
  inc  eax
  cmp  ebp, 98
  je   itsRace
  dec  ebp
  dec  ebp
itsRace:
  inc  ebp
  jmp  loopButton
restoreAppearance:
  mov  eax, [esp+8]
  mov  CurrentAppearance[0], eax
  mov  eax, [esp+12]
  mov  CurrentAppearance[4], eax
saveAppearance:
  mov  ebx, CurrentAppearance[4]
  mov  eax, CurrentAppearance[0]
  push ebx                                  // Style
  push eax                                  // Race
  call LoadHeroDat
  call SetAppearanceGlobals
  call DrawPC
  lea  eax, [esp+16]                        // msgfile
  call message_exit_
  dec  eax
  call DestroyHeroWindow
  xor  ecx, ecx
deleteMemDown:
  inc  eax
  inc  eax
deleteMemUp:
  inc  eax
  inc  eax
  sub  eax, ecx
  xchg ecx, eax
  jecxz deleteWin
  lea  esi, [esp+40]
freeMem:
  mov  eax, [esi]
  call mem_free_
  add  esi, 4
  loop freeMem
deleteWin:
  mov  eax, [esp+4]
  call win_delete_
freeBuffer:
  mov  eax, [esp+0]
  call mem_free_
restore:
  add  esp, 4+4+4+4+8+16+4+4+4+4
  pop  eax                                  // _curr_font_num
  call text_font_
  pop  dword ptr ds:[_glblmode]
  pop  dword ptr ds:[_info_line]
  pop  eax                                  // _gmouse_current_cursor
  call gmouse_set_cursor_                   // set mouse pic
  test dword ptr InLoop, INVENTORY+INTFACEUSE
  jz   notInventory
  xor  eax, eax
  call inven_set_mouse_
notInventory:
  pop  ecx
  jecxz skip_mouse_hide
  call mouse_hide_
skip_mouse_hide:
  pop  ecx
  jecxz skip_gmouse_3d_on
  call gmouse_3d_on_
skip_gmouse_3d_on:
  pop  ecx
  jecxz skipbk
  call map_enable_bk_processes_
skipbk:
  pop  eax
  mov  raceExist, al
  pop  eax
  mov  styleExist, al
  and  dword ptr InLoop, (-1^HEROWIN)
end:
  popad
  retn
 }
}

void __declspec(naked) SetHeroStyle() {
// eax = newStyleVal
 __asm {
  pushad
  movzx ecx, byte ptr styleExist
  jecxz end
  cmp  IsControllingNPC, ecx                // Контроль персонажей?
  je   end                                  // Да
  mov  edx, CurrentAppearance[4]            // edx = Style
  cmp  eax, edx
  je   end
  mov  ecx, CurrentAppearance[0]            // ecx = Race
  mov  ebx, ecx
  xchg ebx, eax                             // eax = Race, ebx = Style
  call LoadHeroDat
  inc  eax
  jnz  setAppearance
// if new style cannot be set
  test ebx, ebx
  jnz  skip
  jecxz setAppearance                       // ignore(???) error if appearance = default (Race=0. Style=0)
skip:
  mov  ebx, edx                             // ebx = Style
  mov  eax, ecx                             // eax = Race
  call LoadHeroDat                          // reload original style
setAppearance:
  mov  CurrentAppearance[4], ebx
  push ebx                                  // Style
  push ecx                                  // Race
  call SetAppearanceGlobals
  call DrawPC
end:
  popad
  retn
 }
}

void __declspec(naked) SetHeroRace() {
// eax = newRaceVal
 __asm {
  pushad
  movzx ecx, byte ptr raceExist
  jecxz end
  cmp  IsControllingNPC, ecx                // Контроль персонажей?
  je   end                                  // Да
  mov  edx, CurrentAppearance[0]
  cmp  eax, edx
  je   end
  mov  ecx, eax                             // ecx = newRaceVal
  xor  ebx, ebx                             // Style
  call LoadHeroDat
  inc  eax
  jnz  setAppearance
// if new race fails with style at 0
  jecxz setAppearance                       // ignore(???) error if appearance = default (Race=0. Style=0)
  mov  ebx, CurrentAppearance[4]            // ebx = Style
  mov  ecx, edx
  xchg edx, eax                             // eax = Race
  call LoadHeroDat                          // reload original race & style
setAppearance:
  mov  CurrentAppearance[4], ebx
  mov  CurrentAppearance[0], ecx
  push ebx                                  // Style
  push ecx                                  // Race
  call SetAppearanceGlobals                 // store new globals
  call DrawPC
end:
  popad
  retn
 }
}

void __declspec(naked) LoadHeroAppearance() {
 __asm {
  pushad
  mov  ecx, critterListCount
  jecxz end
  push offset CurrentAppearance[4]
  push offset CurrentAppearance[0]
  call GetAppearanceGlobals
  mov  ebx, CurrentAppearance[4]
  mov  eax, CurrentAppearance[0]
  call LoadHeroDat
  xor  eax, eax
  inc  eax
  call SetHeroArt
  call DrawPC
end:
  popad
  retn
 }
}

void __declspec(naked) SetNewCharAppearanceGlobals() {
 __asm {
  pushad
  mov  ecx, critterListCount
  jecxz end
  mov  ecx, CurrentAppearance[4]            // Style
  mov  eax, CurrentAppearance[0]            // Race
  test eax, eax
  jnz  skip
  jecxz end
skip:
  push ecx
  push eax
  call SetAppearanceGlobals
end:
  popad
  retn
 }
}

//scan inventory items for armor and weapons currently being worn or wielded and setup matching FrmID for PC
void __declspec(naked) RefreshPCArt() {
 __asm{
  pushad
  xor  eax, eax
  cmp  IsControllingNPC, eax                // Контроль персонажей?
  jne  end                                  // Да
  call proto_dude_update_gender_            // refresh PC base model art
  push dword ptr ds:[_inven_dude]
  push dword ptr ds:[_i_lhand]
  push dword ptr ds:[_i_rhand]
  push dword ptr ds:[_i_worn]
  mov  eax, ds:[_obj_dude]                  // PC state struct
  mov  ds:[_inven_dude], eax                // inventory temp pointer to PC state struct
  mov  edx, eax
  call inven_left_hand_
  mov  ds:[_i_lhand], eax
  mov  eax, edx
  call inven_right_hand_
  mov  ds:[_i_rhand], eax
  mov  eax, edx
  call inven_worn_
  mov  ds:[_i_worn], eax
  call adjust_fid_                          // inventory function - setup pc FrmID and store at address _i_fid
// copy new FrmID to hero state struct
  mov  [edx+0x20], eax                      // pobj.fid
  pop  dword ptr ds:[_i_worn]
  pop  dword ptr ds:[_i_rhand]
  pop  dword ptr ds:[_i_lhand]
  pop  dword ptr ds:[_inven_dude]
  mov  ecx, critterListCount
  jecxz end
  mov  ebx, CurrentAppearance[4]
  mov  eax, CurrentAppearance[0]
  mov  edx, eax
  call LoadHeroDat
  inc  eax
  jnz  skip
// if load fails
  xor  ebx, ebx                             // set style to default
  mov  eax, edx
  call LoadHeroDat
  inc  eax
  jnz  skip
// if race fails with style at default
  xor  edx, edx                             // set race to default
  call LoadHeroDat
skip:
  mov  CurrentAppearance[4], ebx
  mov  CurrentAppearance[0], edx
  push ebx
  push edx
  call SetAppearanceGlobals                 // store new globals
  call DrawPC
end:
  popad
  retn
 }
}

static void __declspec(naked) DrawCard_hook() {
 __asm {
  mov  esi, ecx
  call _word_wrap_
  test eax, eax
  jnz  end
  cmp  word ptr [esi], 12
  jbe  end
  mov  word ptr [esi], 12
end:
  retn
 }
}

void EnableHeroAppearanceMod() {
 int i;

 if (GetPrivateProfileIntA("Misc", "EnableHeroAppearanceMod", 0, ini)) {
  dlog("Setting up Appearance Char Screen buttons. ", DL_INIT);

// check if Data exists for styles male or female
  if (GetFileAttributes("Appearance\\hmR00S00") != INVALID_FILE_ATTRIBUTES ||
      GetFileAttributes("Appearance\\hfR00S00") != INVALID_FILE_ATTRIBUTES ||
      GetFileAttributes("Appearance\\hmR00S00.dat") != INVALID_FILE_ATTRIBUTES ||
      GetFileAttributes("Appearance\\hfR00S00.dat") != INVALID_FILE_ATTRIBUTES) styleExist = true;

// check if Data exists for other races male or female
  if (GetFileAttributes("Appearance\\hmR01S00") != INVALID_FILE_ATTRIBUTES ||
      GetFileAttributes("Appearance\\hfR01S00") != INVALID_FILE_ATTRIBUTES ||
      GetFileAttributes("Appearance\\hmR01S00.dat") != INVALID_FILE_ATTRIBUTES ||
      GetFileAttributes("Appearance\\hfR01S00.dat") != INVALID_FILE_ATTRIBUTES) raceExist = true;

// Get alternate text from ini if available
  GetPrivateProfileString("AppearanceMod", "RaceText", "Race", RaceText, 8, translationIni);
  GetPrivateProfileString("AppearanceMod", "StyleText", "Style", StyleText, 8, translationIni);

// Double size of critter art index creating a new area for hero art and add new hero critter names at end of critter list
  HookCall(0x4189E4, &art_init_hook);

// Shift base hero critter art offset up into hero section
  HookCall(0x418CA2, &art_init_hook1);

// Insert main path structure in front when not loading
  MakeCall(0x4DEEE5, &xfopen_hook, true);

// Check if new hero art exists otherwise use regular art
  MakeCall(0x419560, &art_get_name_hook, false);

// Adjust hero art index offset when changing armor
  MakeCall(0x4717D1, &adjust_fid_hook, true);
  
// Check if new Appearance char scrn button pushed
  MakeCall(0x431E98, &editor_design_hook, false);
  MakeCall(0x4326B6, &editor_design_Tab_hook, true);
  MakeCall(0x43279C, &editor_design_Up_hook, true);
  MakeCall(0x4328A7, &editor_design_Down_hook, true);
  MakeCall(0x43297C, &editor_design_Other_hook, false);

// Check if sex has changed and reset char appearance
  HookCall(0x4322E8, &SexWindow_call);

// Для отрисовки описания
  MakeCall(0x436BFF, &DrawInfoWin_hook, false);

// Mod char scrn background and add new app mod graphics
  MakeCall(0x432DED, &CharEditStart_hook, false);

// Destroy mem after use
  HookCall(0x433B23, &CharEditEnd_hook);

// make room for char view window
  for (i = 0; i < 27; i++) SafeWrite32(write32[i][0], write32[i][1]);
  for (i = 0; i < 10; i++) SafeWrite16(write16[i][0], (WORD)write16[i][1]);
//  for (i = 0; i < 2; i++) SafeWrite8(write8[i][0], (BYTE)write8[i][1]);
  SafeWrite8(0x4365C8, 100);                // Чтобы обрабатывались 98 и 99 _info_line в DrawInfoWin_

// Reset Appearance when selecting "Create Character" from the New Char screen
  HookCall(0x4A740A, &select_character_hook);

// Adjust PC SFX Name
  HookCall(0x45114C, &gsound_load_sound_hook);

// Return hero art index offset back to normal before saving
  SafeWrite32(0x519400, (DWORD)&obj_save_dude_call);

// Fixes missing console critical hit messages when PC is attacked.
  HookCall(0x42613A, &combat_get_loc_name_hook);

// Force Criticals For Testing
//  SafeWrite16(0x423A8D, 0x08EB);            // jmps 0x423A97

  dlogr(" Done", DL_INIT);
 }

// Load Appearance data from GCD file
 HookCall(0x42DF5F, &pc_load_data_hook);

// Save Appearance data to GCD file
 HookCall(0x42E163, &pc_save_data_hook);

 HookCall(0x43ACFC, &DrawCard_hook);

}
