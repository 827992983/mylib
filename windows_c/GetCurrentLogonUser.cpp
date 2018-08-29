// GetCurrentLogonUser.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <windows.h>
#include <TlHelp32.h>

#pragma comment(lib, "Psapi.lib")

DWORD GetProcessIDFromName(char *name)
{
	HANDLE snapshot;
	PROCESSENTRY32 processinfo;
	processinfo.dwSize = sizeof(processinfo);
	snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == NULL)
		return FALSE;

	BOOL status = Process32First(snapshot, &processinfo);
	while (status)
	{
		if (_stricmp(name, processinfo.szExeFile) == 0)
			return processinfo.th32ProcessID;
		status = Process32Next(snapshot, &processinfo);
	}
	return -1;
}

/*
  Get_LogUser 函数说明
  参数 lpUserName 为用来输出登录用户名的内存，以字符串方式
        nNameLen   为内存大小
		返回值：如果windows有用户登录了，会返回true，并且lpUserName 不为空字符串，否则返回false，并且lpUserName为空字符串

  补充说明：
  这个函数和GetUserName不同处在于，GetUserName不支持服务，这个函数支持服务模式。如果在服务模式下，系统开机就会自启动的服务，通过GetUserName
    获得的用户永远是SYSTEM，即使这期间用户进行了登录，也不会返回正确的用户信息，而该函数会执行正确的结果。


  如果您有兴趣，可以让该函数每隔几秒执行一次，并且写入到文件里，来观看执行结果，就会发现当系统【启动中】但是还没有执行用户登录时，
  会返回false，一旦用户登录了，就会返回当前登录的用户名。
  
  本程序代码来源于网络，本人只做了简单的修改，再次发布于网络，因为这个问题困扰了我一天时间来搞定，希望能帮助需要他的人。
	


*/
bool Get_LogUser(char *lpUserName, DWORD nNameLen)
{
	DWORD dwProcessID = GetProcessIDFromName("explorer.exe");
	if (dwProcessID == 0)
		return false;

	BOOL fResult = FALSE;
	HANDLE hProc = NULL;
	HANDLE hToken = NULL;
	TOKEN_USER *pTokenUser = NULL;

	__try
	{
		// Open the process with PROCESS_QUERY_INFORMATION access
		hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwProcessID);
		if (hProc == NULL)
		{
			return false;
		}
		fResult = OpenProcessToken(hProc, TOKEN_QUERY, &hToken);
		if (!fResult)
		{
			return false;
		}

		DWORD dwNeedLen = 0;
		fResult = GetTokenInformation(hToken, TokenUser, NULL, 0, &dwNeedLen);
		if (dwNeedLen > 0)
		{
			pTokenUser = (TOKEN_USER*)new BYTE[dwNeedLen];
			fResult = GetTokenInformation(hToken, TokenUser, pTokenUser, dwNeedLen, &dwNeedLen);
			if (!fResult)
			{
				return false;
			}
		}
		else
		{
			return false;
		}

		SID_NAME_USE sn;
		TCHAR szDomainName[MAX_PATH];
		DWORD dwDmLen = MAX_PATH;

		fResult = LookupAccountSid(NULL, pTokenUser->User.Sid, lpUserName, &nNameLen,
			szDomainName, &dwDmLen, &sn);
	}
	__finally
	{
		if (hProc)
			::CloseHandle(hProc);
		if (hToken)
			::CloseHandle(hToken);
		if (pTokenUser)
			delete[](char*)pTokenUser;
	}
	return true;
}

int _tmain(int argc, _TCHAR* argv[])
{
	char szCurUser[1024];
	DWORD dwCurUserBufSize = sizeof(szCurUser);
	bool bRet = Get_LogUser(szCurUser, dwCurUserBufSize);
	if (bRet)
	{
		printf("Current user is:%s\n", szCurUser);
	}
	else
	{
		printf("Function failed!\n");
	}
	getchar();
	return 0;
}

