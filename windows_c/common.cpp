#include "common.h"
#include "logger.h"

void Char2Wchar(const char *chr, wchar_t *wchar, int size)
{
	MultiByteToWideChar( CP_ACP, 0, chr, strlen(chr)+1, wchar, size/sizeof(wchar[0]) );
}

void Wchar2Char(const wchar_t *wchar, char *chr, int length)
{
	WideCharToMultiByte( CP_ACP, 0, wchar, -1, chr, length, NULL, NULL );  
}

void Utf8ToWchar(const char *utf8, wchar_t *wchar, int len)
{
	MultiByteToWideChar( CP_UTF8, 0,(char *) utf8, -1, wchar, len ); 
}

void Unicode2Wchar(const char * str, wchar_t *result)
{
	wchar_t rst[1024] = {0};
	bool escape = false;
	int len = strlen(str);
	int intHex;
	char tmp[5];
	int size = 0;
	memset(tmp, 0, 5);
	for (int i = 0; i < len; i++)
	{
		char c = str[i];
		switch (c)
		{
		case '//':
		case '%':
		case '\\':
			escape = true;
			break;
		case 'u':
		case 'U':
			if (escape)
			{
				memcpy(tmp, str+i+1, 4);
				sscanf(tmp, "%x", &intHex); //把16进制字符转换为数字
				rst[size++] = intHex;
				i+=4;
				escape=false;
			}else{
				rst[size++] = c;
			}
			break;
		default:
			rst[size++] = c;
			break;
		}
	}
	wcscpy(result, rst);
	return;
}

int ExecuteCommandAsUser(HANDLE token, char *cmd)
{
	STARTUPINFO si = { sizeof(si) };
	PROCESS_INFORMATION pi;

	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = TRUE; //TRUE表示显示创建的进程的窗口
	BOOL bRet = ::CreateProcessAsUser (
		token,
		NULL,
		cmd,
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
		LOG_ERROR("GetLastError=%d\n", error);
	}

	if(bRet)   
	{
		::CloseHandle (pi.hThread);
		::CloseHandle (pi.hProcess);
	}else{
		LOG_ERROR("CreateProcessAsUser with error!");
		return -2;
	}
	return 0;
}

char * trim(char *str)
{
	register char *s, *t;

	if (str == NULL)
	{
		return NULL;
	}

	for (s = str; isspace (*s); s++)
		;

	if (*s == 0)
		return s;

	t = s + strlen (s) - 1;
	while (t > s && isspace (*t))
		t--;
	*++t = '\0';

	return s;
}

char *ReplaceChar(char *str, char _old, char _new)
{
	char *result = (char *)malloc(sizeof(char) * (strlen(str)+1));
	int len = strlen(str);
	strcpy(result, str);
	while(len-- > 0){
		if(str[len] == _old){
			result[len] = _new;
		}
	}
	return result;
}

char *ReplaceStr(char *src, const char *sub, const char *dst)
{
	int flag = 0;
	int length = 0;
	int srcLen, subLen, dstLen;

	if(src ==NULL || sub == NULL || dst == NULL)
	{
		return NULL;
	}

	srcLen = strlen(src);
	subLen = strlen(sub);
	dstLen = strlen(dst);

	if(srcLen == 0 || subLen == 0)
	{
		return NULL;
	}

	char *result = NULL;
	char *psrc = src;
	while(psrc != NULL){
		char *pos = strstr(psrc, sub);
		if(pos != NULL){
			result = (char *)realloc(result, length + (pos-psrc) + dstLen);
			memcpy(result+length, psrc, pos-psrc);
			length += pos-psrc;
			memcpy(result + length, dst, dstLen);
			length += dstLen;
			result[length] = '\0';
		}else{
			result = (char *)realloc(result, length + srcLen-(psrc-src));
			memcpy(result + length, psrc, srcLen-(psrc-src));
			length += srcLen-(psrc-src);
			result[length] = '\0';
			break;
		}
		psrc = pos+subLen;
	}

	return result;
}

unsigned long CheckFileSize(const char *path)
{  
	unsigned long filesize = -1;  
	FILE *fp;  
	fp = ::fopen(path, "r");  
	if(fp == NULL)  
		return filesize;  
	fseek(fp, 0L, SEEK_END);  
	filesize = ftell(fp);  
	fclose(fp);  
	return filesize;
}