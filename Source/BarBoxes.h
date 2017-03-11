#pragma once

struct sBBox {
 DWORD msgnumber;
 DWORD colour;
 void* mem;
};

void BarBoxesInit();
int _stdcall GetBox(int i);
void _stdcall AddBox(int i);
void _stdcall RemoveBox(int i);
