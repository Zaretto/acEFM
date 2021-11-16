// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"

#include <string>

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
    
	if (AllocConsole()) {
		freopen("CONOUT$", "w", stdout);
		SetConsoleTitle((L"DCS EFM debug console"));
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED);
	} else {
		MessageBox(0,L"FAIL_console",L"Error",0);
	}
  
	break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
