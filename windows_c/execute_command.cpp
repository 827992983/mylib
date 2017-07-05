#include "execute_command.h"

int ExecuteCommand(char *cmd){
	STARTUPINFO si = { sizeof(si) };
	PROCESS_INFORMATION pi;

	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = TRUE; //TRUE表示显示创建的进程的窗口
	int nLen = strlen(cmd)+1;
	int nwLen = MultiByteToWideChar(CP_ACP, 0, cmd, nLen, NULL, 0);
	TCHAR cmdline[1024];
	MultiByteToWideChar(CP_ACP, 0, cmd, nLen, cmdline, nwLen);
	BOOL bRet = ::CreateProcess (
		NULL,
		cmdline,
		NULL,
		NULL,
		FALSE,
		CREATE_NEW_CONSOLE,
		NULL,
		NULL,   
		&si,
		&pi);

	DWORD error = GetLastError();
	if(error != 0){
		return -1;
	}

	if(bRet)   
	{
		::CloseHandle (pi.hThread);
		::CloseHandle (pi.hProcess);
	}else{
		return -2;
	}
	return 0;
}
