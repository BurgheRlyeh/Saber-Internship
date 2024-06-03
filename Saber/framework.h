// header.h : include file for standard system include files,
// or project specific include files
//

#pragma once

#include "targetver.h"

// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN

// Windows Header Files
#include <windows.h>

#include <shellapi.h> // For CommandLineToArgvW

// To avoid conflicts and use only min/max defined in <algorithm>
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif



// C RunTime Header Files
//#include <stdlib.h>
//#include <malloc.h>
//#include <memory.h>
//#include <tchar.h>
