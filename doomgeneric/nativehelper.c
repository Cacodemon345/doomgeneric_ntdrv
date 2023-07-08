// This file is only there to forward some of the print functions to ntdll.dll.
#include <stdlib.h>
#include <stdarg.h>
#include <memory.h>

extern int __cdecl _vsnprintf(char* buf, size_t buflen, const char* fmt, va_list arg);
int __cdecl snprintf(char* buf, size_t buflen, const char* fmt, ...)
{
	int res;
	va_list args;
	va_start(args, fmt);
	res = _vsnprintf(buf, buflen, fmt, args);
	va_end(args);
	return res;
}

int __cdecl vsnprintf(char* buf, size_t buflen, const char* fmt, va_list arg)
{
	return _vsnprintf(buf, buflen, fmt, arg);
}

extern int __cdecl sscanf(const char* buffer, const char* format, ... );

unsigned char M_StrToInt(const char *str, int *result)
{
    return sscanf(str, " 0x%x", result) == 1
        || sscanf(str, " 0X%x", result) == 1
        || sscanf(str, " 0%o", result) == 1
        || sscanf(str, " %d", result) == 1;
}


extern int nt_free(void* ptr);
extern void* nt_malloc(size_t size);

char* nt_strdup(char* str)
{
	int len = 0;
	char* strptr = str;
	while (*strptr++ != 0)
	{
		len++;
	}
	len++;
	strptr = nt_malloc(len);
	if (!strptr) return NULL;
	memcpy(strptr, str, len);
	return strptr;
}