#include "stdafx.h"
#include "Utils.h"
namespace utils
{
	int getfontsize()
	{
		HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
		PCONSOLE_FONT_INFOEX lpConsoleCurrentFontEx = new CONSOLE_FONT_INFOEX();
		lpConsoleCurrentFontEx->cbSize = sizeof(CONSOLE_FONT_INFOEX);
		GetCurrentConsoleFontEx(hStdOut, 0, lpConsoleCurrentFontEx);
		return lpConsoleCurrentFontEx->dwFontSize.Y;
	}
	void fontsize(int a, int b)
	{
		HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
		PCONSOLE_FONT_INFOEX lpConsoleCurrentFontEx = new CONSOLE_FONT_INFOEX();
		lpConsoleCurrentFontEx->cbSize = sizeof(CONSOLE_FONT_INFOEX);
		GetCurrentConsoleFontEx(hStdOut, 0, lpConsoleCurrentFontEx);
		lpConsoleCurrentFontEx->dwFontSize.X = a;
		lpConsoleCurrentFontEx->dwFontSize.Y = b;
		SetCurrentConsoleFontEx(hStdOut, 0, lpConsoleCurrentFontEx);
	}
	void setcolor(int textcol, int backcol)
	{
		if ((textcol % 16) == (backcol % 16))textcol++;
		textcol %= 16; backcol %= 16;
		unsigned short wAttributes = ((unsigned)backcol << 4) | (unsigned)textcol;
		HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleTextAttribute(hStdOut, wAttributes);
	}


}