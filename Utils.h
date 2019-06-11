#pragma once

#include <vector>
#include <string>
#include <Shlobj.h>
#define SERVICE_INSTALLTION_PATH L"SG_Server"
extern HANDLE hConsole;
#define LOG_COLOR_DARKBLUE 9
#define LOG_COLOR_DARKGREEN 2
#define LOG_COLOR_WHITE 7
#define LOG_COLOR_GREEN 10
#define LOG_COLOR_YELLOW 14 
#define LOG_COLOR_MAGENTA 13
#define LOG_COLOR_CIAN 11

namespace utils
{
	int getfontsize();
	void fontsize(int a, int b);
	void setcolor(int textcol, int backcol);
}
