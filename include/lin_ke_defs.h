#ifndef __LIN_KE_DEFS__
#define __LIN_KE_DEFS__

#include <string.h>

/*!\brief define data types like in windows
*/


typedef  unsigned char     BYTE  ;
typedef  BYTE*             LPBYTE;

typedef  unsigned short    WORD  ;
typedef  WORD*             LPWORD;

typedef unsigned int       DWORD  ;
typedef DWORD*             LPDWORD;

typedef unsigned long long QWORD  ;
typedef QWORD*             LPQWORD;

typedef void *             LPVOID ;
typedef DWORD              BOOL;
typedef long               LONG;

typedef char               __int8;
typedef short              __int16;
typedef long               __int32;
typedef long int           __int64;
typedef unsigned char      __uint8;
typedef unsigned short     __uint16;
typedef unsigned long      __uint32;
typedef unsigned long int  __uint64;
typedef int HANDLE;




/*! stub for compatibility
*/
#define WINAPI
#define winapi
#define __fastcall
#define EXPORT
#define IMPORT
#define KERTL_CLASS class
#define DECLSPEC_NOVTABLE
#define TRUE 1
#define FALSE 0
#define ANYSIZE_ARRAY 1
#define KERTL_ARRAY_COUNT(x) (sizeof(x)/sizeof(*x))
#define NULL_PTR ((LPVOID)0)


#define FillMemory(ptr,size,value) memset(ptr,value,size)
#define ZeroMemory(ptr,size)       memset(ptr,0,size)
#define GlobalAlloc(x,sz) malloc(sz)
#define GlobalFree(ptr)   free(ptr)
#define FILETIME QWORD
#define LPFILETIME LPQWORD

#define LOBYTE(x) (x&0xFF)
#define HIBYTE(x) ((x>>8)&0xFF)
#define LOWORD(x) (x&0xFFFF)
#define HIWORD(x) ((x>>16)&0xFFFF)
#define LODWORD(x) (x&0xFFFFFFFF)
#define HIDWORD(x) ((x>>32)&0xFFFFFFFF)

#define MSEC_NS100(x) (10000*(QWORD)(x))
#define NS100_MSEC(x) ((x)/10000)


#ifdef __cplusplus

#ifndef _NO_NAMECPACE
namespace KeRTL
{
#endif

 template<class T>
 inline T  __fastcall MIN(T a,T b)
 {return (a < b )? a: b;};

 template<class T>
 inline T  __fastcall MAX(T one,T two)
 {return (one > two ) ? one:two;};

 template<class T>
 inline T  __fastcall ABS(T val)
 {return (val >= T(0) ) ? val: T(0)-val;};

WORD           WINAPI calc_kpk  (void * Buffer,unsigned short len,unsigned short Del);

#ifndef _NO_NAMECPACE
}
#endif

#endif

#endif
