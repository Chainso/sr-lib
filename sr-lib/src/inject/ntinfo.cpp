// Credits to https://github.com/makemek/cheatengine-threadstack-finder

#include <iostream>
#include <Windows.h>
#include <Psapi.h>

#include "inject/ntinfo.h"

typedef LONG NTSTATUS;
typedef DWORD KPRIORITY;
typedef WORD UWORD;

typedef struct _CLIENT_ID
{
	PVOID UniqueProcess;
	PVOID UniqueThread;
} CLIENT_ID, * PCLIENT_ID;

typedef struct _THREAD_BASIC_INFORMATION
{
	NTSTATUS                ExitStatus;
	PVOID                   TebBaseAddress;
	CLIENT_ID               ClientId;
	KAFFINITY               AffinityMask;
	KPRIORITY               Priority;
	KPRIORITY               BasePriority;
} THREAD_BASIC_INFORMATION, * PTHREAD_BASIC_INFORMATION;

enum THREADINFOCLASS
{
	ThreadBasicInformation,
};

void* GetThreadStackTopAddress_x86(HANDLE hProcess, HANDLE hThread)
{

	LPCWSTR moduleName = L"ntdll.dll";

	bool loadedManually = false;
	HMODULE module = GetModuleHandle(moduleName);

	if (!module)
	{
		module = LoadLibrary(moduleName);
		loadedManually = true;
	}

	NTSTATUS(__stdcall * NtQueryInformationThread)(HANDLE ThreadHandle, THREADINFOCLASS ThreadInformationClass, PVOID ThreadInformation, ULONG ThreadInformationLength, PULONG ReturnLength);
	NtQueryInformationThread = reinterpret_cast<decltype(NtQueryInformationThread)>(GetProcAddress(module, "NtQueryInformationThread"));

	if (NtQueryInformationThread)
	{
		NT_TIB tib = { 0 };
		THREAD_BASIC_INFORMATION tbi = { 0 };

		NTSTATUS status = NtQueryInformationThread(hThread, ThreadBasicInformation, &tbi, sizeof(tbi), nullptr);
		if (status >= 0)
		{
			ReadProcessMemory(hProcess, tbi.TebBaseAddress, &tib, sizeof(tbi), nullptr);

			if (loadedManually)
			{
				FreeLibrary(module);
			}
			return tib.StackBase;
		}
	}


	if (loadedManually)
	{
		FreeLibrary(module);
	}

	return nullptr;
}

DWORD GetThreadStartAddress(HANDLE processHandle, HANDLE hThread)
{
	/* rewritten from https://github.com/cheat-engine/cheat-engine/blob/master/Cheat%20Engine/CEFuncProc.pas#L3080 */
	DWORD used = 0, ret = 0;
	DWORD stacktop = 0, result = 0;

	MODULEINFO mi;

	GetModuleInformation(processHandle, GetModuleHandle(L"kernel32.dll"), &mi, sizeof(mi));
	stacktop = (DWORD)GetThreadStackTopAddress_x86(processHandle, hThread);

	/* The stub below has the same result as calling GetThreadStackTopAddress_x86()
	change line 54 in ntinfo.cpp to return tbi.TebBaseAddress
	Then use this stub
	*/
	//LPCVOID tebBaseAddress = GetThreadStackTopAddress_x86(processHandle, hThread);
	//if (tebBaseAddress)
	//	ReadProcessMemory(processHandle, (LPCVOID)((DWORD)tebBaseAddress + 4), &stacktop, 4, NULL);

	/* rewritten from 32 bit stub (line3141)
	Result: fail -- can't get GetThreadContext()
	*/
	//CONTEXT context;
	//LDT_ENTRY ldtentry;
	//GetModuleInformation(processHandle, LoadLibrary("kernel32.dll"), &mi, sizeof(mi));
	//
	//if (GetThreadContext(processHandle, &context)) {
	//	
	//	if (GetThreadSelectorEntry(hThread, context.SegFs, &ldtentry)) {
	//		ReadProcessMemory(processHandle,
	//			(LPCVOID)( (DWORD*)(ldtentry.BaseLow + ldtentry.HighWord.Bytes.BaseMid << ldtentry.HighWord.Bytes.BaseHi << 24) + 4),
	//			&stacktop,
	//			4,
	//			NULL);
	//	}
	//}

	if (stacktop)
	{
		//find the stack entry pointing to the function that calls "ExitXXXXXThread"
		//Fun thing to note: It's the first entry that points to a address in kernel32

		DWORD* buf32 = new DWORD[4096];

		if (ReadProcessMemory(processHandle, (LPCVOID)(stacktop - 4096), buf32, 4096, NULL))
		{
			for (int i = 4096 / 4 - 1; i >= 0; --i)
			{
				if (buf32[i] >= (DWORD)mi.lpBaseOfDll && buf32[i] <= (DWORD)mi.lpBaseOfDll + mi.SizeOfImage)
				{
					result = stacktop - 4096 + i * 4;
					break;
				}

			}
		}

		delete[] buf32;
	}

	return result;
}