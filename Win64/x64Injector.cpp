// x64Injector.cpp : Defines the entry point for the console application.
//
// #ifndef _WIN64
// #define _WIN64
// #endif


#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <string>
#include <Tlhelp32.h>
#include <Shlwapi.h>
#pragma comment( lib, "Shlwapi.lib" )

bool PrintError(DWORD dwError, std::wstring FormCall)
{

	int errCode = dwError;
	LPWSTR errString = NULL;

	int size = FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER |FORMAT_MESSAGE_FROM_SYSTEM, 0, errCode, 0,(LPWSTR)&errString, 0, 0 );
	wprintf( L"%s Returned Error code %d:  %s\n",FormCall.c_str(), errCode, errString) ;
	LocalFree( errString ) ;
	return false;

}

void RemoveFileName(wchar_t* path)
{
	wchar_t* pos = (wchar_t*)(path + wcslen( path ));
	while(pos >= path && *pos != '\\') --pos;
	pos[1] = 0;
}

void StripFileExtention(wchar_t *szPath)
{
	wchar_t* szCurPosition = (wchar_t*)(szPath + wcslen( szPath ));	// End of String
	while(szCurPosition >= szPath && *szCurPosition != '.')		// Work Back Until "\"
		--szCurPosition;
	szCurPosition[1] = 0;										// Got It!
}

BOOL GetProcessOf(wchar_t exename[], PROCESSENTRY32W *process)
{
	HANDLE handle ;
	process->dwSize = sizeof(PROCESSENTRY32W);
	handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if(Process32First(handle, process))
	{
		do
		{
			if(wcscmp(process->szExeFile, exename) == 0)
			{
				CloseHandle(handle);
				return true;
			}
		}while(Process32Next(handle, process));
	}
	CloseHandle(handle);
	return false;
}

wchar_t *GetDirectoryFile(wchar_t *filename)
{
	wchar_t dlldir[MAX_PATH];
	GetModuleFileName(0, dlldir, MAX_PATH);
	for(size_t i = wcslen(dlldir); i > 0; i--)
	{ 
		if(dlldir[i] == '\\') 
		{ 
			dlldir[i+1] = 0; 
			break; 
		} 
	}
	static wchar_t path[320];
	wcscpy_s(path, dlldir);
	wcscat_s(path, filename);
	return path;
}
bool load(wchar_t *key, wchar_t *buffer, int len)
{
	return (bool)GetPrivateProfileString(L"Injector Config", key, 0, buffer, len, GetDirectoryFile(L"Settings.cfg"));
}
//======================================================================================
void save(wchar_t *key, wchar_t *buffer)
{
	WritePrivateProfileString(L"Injector Config", key, buffer, GetDirectoryFile(L"Settings.cfg"));
}
bool fileExists(const wchar_t filename[]) 
{
	WIN32_FIND_DATA finddata;
	HANDLE handle = FindFirstFile(filename,&finddata);

	bool ret = (handle!=INVALID_HANDLE_VALUE);

	if(ret)
		FindClose(handle);
	return ret;
} 


int _tmain(int argc, _TCHAR* argv[])
{
	PROCESSENTRY32W pe32;
	
	int iTotalSize = NULL;
	int iModuleSize = NULL;
	int iStructSize = NULL;
	int iFunctionSize = NULL;
	HANDLE hToken = NULL;
	DWORD err;


	wchar_t fullPath[MAX_PATH]= L"";
	wchar_t szFileName[MAX_PATH]= L"";
	wchar_t szProcessName[MAX_PATH]= L"";
	wchar_t szWait[MAX_PATH]=L"";
	wchar_t szFullProcessPath[MAX_PATH]=L"";
	wchar_t szCmdLine[512]=L"";

	if(GetModuleFileName(GetModuleHandle(NULL),fullPath,MAX_PATH)==0)
	{
		err = GetLastError();
		PrintError(err,L"GetModuleFileName failed\n");
		getchar();
		return 0;
	}
	wcscpy_s(szFileName,fullPath);

	PathStripPath(szFileName);
	StripFileExtention(szFileName);
	wcscat_s(szFileName,L"dll");

	RemoveFileName(fullPath);

	if(!fileExists(GetDirectoryFile(L"Settings.cfg")))
	{
		save(L"exe", L"AC2-Win64-Shipping.exe");
		save(L"wait", L"10");
		save(L"dll", szFileName);
	}
	
	load(L"exe", szProcessName, sizeof(szProcessName));
	load(L"wait", szWait, sizeof(szWait));
	load(L"dll", szFileName, sizeof(szFileName));
	load(L"path",szFullProcessPath,sizeof(szFullProcessPath));
	load(L"cmdline",szCmdLine,sizeof(szCmdLine));

	wcscat_s(fullPath,szFileName);

	if (!fileExists(fullPath))
	{
		DWORD err = GetLastError();
		PrintError(err, L"fileExists");
		getchar();
		return 0;
	}

	wprintf(L"Starting: %s\nWith: %s\n",szFullProcessPath ,szCmdLine);
	if (wcslen(szFullProcessPath) > 0)
		ShellExecute(NULL,L"open",szFullProcessPath, szCmdLine,NULL,SW_SHOW );

	wprintf(L"Injecting: %s\nInto: %s\n",fullPath,szProcessName);

	while(!GetProcessOf(szProcessName, &pe32))
	{
		Sleep(10);
	}
	wprintf(L"Process Found %s....Waiting...\r\n\r\n", szProcessName);
	Sleep(_wtoi(szWait));
	HANDLE_PTR hRemoteProcess = (HANDLE_PTR)OpenProcess(PROCESS_ALL_ACCESS, FALSE, pe32.th32ProcessID);
	wprintf(L"HANDLE_PTR hRemoteProcess: 0x%llX\n",hRemoteProcess);
	
	if ((HANDLE)hRemoteProcess == INVALID_HANDLE_VALUE)
	{
		DWORD err = GetLastError();
		PrintError(err, L"OpenProcess");
		getchar();
		return 0;
	}

	DWORD_PTR lpBase = (DWORD_PTR )VirtualAllocEx((HANDLE)hRemoteProcess, NULL, MAX_PATH, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!lpBase)
	{
		DWORD err = GetLastError();
		PrintError(err, L"VirtualAllocEx");
		getchar();
		return 0;
	}

	wprintf(L"VirtualAllocEx: 0x%llX\n",lpBase);
	
	if (!WriteProcessMemory((HANDLE)hRemoteProcess, (LPVOID)lpBase, fullPath, sizeof(fullPath) * 2 , NULL ))
	{
		DWORD err = GetLastError();
		PrintError(err, L"WriteProcessMemory");
		VirtualFreeEx((HANDLE)hRemoteProcess,(LPVOID)lpBase,NULL,MEM_RELEASE);
		getchar();
		return 0;
	}

	DWORD_PTR dwThreadID;
	
	HANDLE_PTR handle =(HANDLE_PTR)CreateRemoteThread((HANDLE)hRemoteProcess, NULL, NULL, (LPTHREAD_START_ROUTINE)LoadLibraryW, (LPVOID)lpBase, NULL, (LPDWORD)&dwThreadID);

	WaitForSingleObject((HANDLE)handle,INFINITE);
	VirtualFreeEx((HANDLE)hRemoteProcess,(LPVOID)lpBase,NULL,MEM_RELEASE);
 	CloseHandle((HANDLE)hRemoteProcess);
 	CloseHandle((HANDLE)handle);

	return 0;
}

