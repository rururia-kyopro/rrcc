#ifndef __STDDEF_INC
#define __STDDEF_INC
typedef unsigned long int size_t;
typedef long int ptrdiff_t;
typedef int wchar_t;
#define NULL 0
#define offsetof(t, m) ((size_t)(char*)&(((t*)0)->m))
#endif
