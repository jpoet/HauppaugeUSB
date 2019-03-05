#ifndef __WINWRAP_H_
#define __WINWRAP_H_

#include <stdint.h>
#include "wrapper.h"

#define VOID void
#undef BOOL
#define BOOL bool
#define BOOLEAN bool
#define BYTE uint8_t
#undef CHAR
#define CHAR char
#define WCHAR int16_t
#define SHORT int16_t
#define WORD uint16_t
#define LONG int32_t
#define DWORD uint32_t

#define PVOID void*
#define PCHAR CHAR*
#define PWCHAR WCHAR*
#define LPDWORD DWORD*
#define PDWORD DWORD*
#define ULONG_PTR unsigned long*

#define NTSTATUS int

#define STATUS_SUCCESS 0
#define STATUS_UNSUCCESSFUL -1

#define NT_SUCCESS(V) ((V) == STATUS_SUCCESS)

#define IN
#define OUT

#define HANDLE void*

typedef struct _GUID {
  DWORD Data1;
  WORD  Data2;
  WORD  Data3;
  BYTE  Data4[8];
} GUID;

#ifndef TRUE
#ifdef __cplusplus
        #define TRUE true
        #define FALSE false
#else
        #define TRUE 1
        #define FALSE 0
#endif
#endif

#endif
