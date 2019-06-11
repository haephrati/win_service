/*
	SG_WinService
	Secured Globe Generic Windows Service
	©2019 Secured Globe, Inc.

	version 1.0	June, 2019
*/
#pragma once

#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>


#define WINVER			0x0600
#define _WIN32_WINNT	0x0600
#define _WIN32_IE		0x0700
#define GDIPVER			0x0110
#define WIN32_LEAN_AND_MEAN

#include <atlbase.h>
#include <atltime.h>
#include <atlstr.h>
#include <WinInet.h>
#include <wtsapi32.h>
#include <Shlwapi.h>
#include <string>
#include <tlhelp32.h>
#include <Userenv.h>
#include <fstream>
#include <Setupapi.h>
#include <shellapi.h>
#include "SG_WinService.h"



// Common Standard Headers
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <new>
#include <queue>
#include <string>
#include <sstream>
#include <atlbase.h>
#include <atlstr.h>
// Common Windows Headers
#include <Windows.h>
#include <tchar.h>
#include <tlhelp32.h>
#include <userenv.h>
#include <wtsApi32.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <dshow.h>
#include <wincred.h>
#include <WinCrypt.h>
#include <WinInet.h>
#include <LMCons.h>
#include <Psapi.h>