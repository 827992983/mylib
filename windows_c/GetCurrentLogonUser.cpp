// GetCurrentLogonUser.cpp : �������̨Ӧ�ó������ڵ㡣
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
  Get_LogUser ����˵��
  ���� lpUserName Ϊ���������¼�û������ڴ棬���ַ�����ʽ
        nNameLen   Ϊ�ڴ��С
		����ֵ�����windows���û���¼�ˣ��᷵��true������lpUserName ��Ϊ���ַ��������򷵻�false������lpUserNameΪ���ַ���

  ����˵����
  ���������GetUserName��ͬ�����ڣ�GetUserName��֧�ַ����������֧�ַ���ģʽ������ڷ���ģʽ�£�ϵͳ�����ͻ��������ķ���ͨ��GetUserName
    ��õ��û���Զ��SYSTEM����ʹ���ڼ��û������˵�¼��Ҳ���᷵����ȷ���û���Ϣ�����ú�����ִ����ȷ�Ľ����


  ���������Ȥ�������øú���ÿ������ִ��һ�Σ�����д�뵽�ļ�����ۿ�ִ�н�����ͻᷢ�ֵ�ϵͳ�������С����ǻ�û��ִ���û���¼ʱ��
  �᷵��false��һ���û���¼�ˣ��ͻ᷵�ص�ǰ��¼���û�����
  
  �����������Դ�����磬����ֻ���˼򵥵��޸ģ��ٴη��������磬��Ϊ���������������һ��ʱ�����㶨��ϣ���ܰ�����Ҫ�����ˡ�
	


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

