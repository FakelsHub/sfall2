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

#include "main.h"

#include <stdio.h>
#include "FalloutEngine.h"
#include "Logging.h"
#include "version.h"

static DWORD InCredits = 0;
static DWORD CreditsLine = 0;

static const char* ExtraLines[] = {
 "#" VERSION_NAME " " VERSION_STRING,
 "",
 "sfall2 is free software, licensed under the GPL",
 "Copyright 2008-2016  The sfall team",
 "",
 "@Author",
 "Timeslip",
 "",
 "@Contributors",
 "ravachol",
 "Noid",
 "Glovz",
 "Dream",
 "Ray",
 "Kanhef",
 "KLIMaka",
 "Mash",
 "Helios",
 "Haenlomal",
 "NVShacker",
 "NovaRain",
 "JimTheDinosaur",
 "phobos2077",
 "Tehnokrat",
 "Crafty",
 "Slider2k",
 "Vennor",
 "",
 "@Additional thanks to",
 "Nirran",
 "killap",
 "MIB88",
 "Rain man",
 "Continuum",
 "Fakeman",
 "Drobovik",
 "Lexx",
 "Sduibek",
 "burn",
 "Foxx",
 "Pyran",
 "Ethereal",
 "Anyone who has used sfall in their own mods",
 "The bug reporters and feature requesters",
 ""
 "",
 "",
 "#FALLOUT 2",
 ""
};
static DWORD ExtraLineCount = sizeof(ExtraLines)/4;

static void __declspec(naked) credits_hook() {
 __asm {
  mov  CreditsLine, 0
  inc  InCredits
  call credits_
  dec  InCredits
  retn
 }
}

static DWORD _stdcall CreditsNextLine(char* buf, DWORD* font, DWORD* colour) {
 if(!InCredits||CreditsLine>=ExtraLineCount) return 0;
 const char* line=ExtraLines[CreditsLine++];
 if(strlen(line)) {
  if(line[0]=='#') {
   line++;
   *font=*(DWORD*)_name_font;
   *colour=*(BYTE*)0x6A7F01;
  } else if(line[0]=='@') {
   line++;
   *font=*(DWORD*)_title_font;
   *colour=*(DWORD*)_title_color;
  } else {
   *font=*(DWORD*)_name_font;
   *colour=*(DWORD*)_name_color;
  }
 }
 strcpy_s(buf, 256, line);
 return 1;
}

static void __declspec(naked) credits_get_next_line_hook() {
 __asm {
  pushad
  push ebx
  push edx
  push eax
  call CreditsNextLine
  test eax, eax
  popad
  jz   fail
  xor  eax, eax
  inc  eax
  retn
fail:
  jmp credits_get_next_line_
 }
}

void CreditsInit() {
 if (!GetPrivateProfileIntA("Debugging", "NoCredits", 0, ini)) {
  dlogr("Applying credits patch", DL_INIT);
  HookCall(0x480C49, &credits_hook);
  HookCall(0x43F881, &credits_hook);
  HookCall(0x42CB49, &credits_get_next_line_hook);
 }
}
