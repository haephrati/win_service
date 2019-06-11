/*
	SG_WinService
	Secured Globe Generic Windows Service
	©2019 Secured Globe, Inc.

	version 1.0	June, 2019
*/
#include "stdafx.h"
#include "SG_WinService.h"
#include "Utils.h"

#define UNINSTALL_HOTKEY_ID							301				// Code for uninstall hotkey
#define SERVICE_CONTROL_UNINSTALL					130				// User defined service comman code

SERVICE_STATUS          serviceStatus;
SERVICE_STATUS_HANDLE   hServiceStatus;
HWND hWnd = NULL;
HANDLE hPrevAppProcess = NULL;
HANDLE ghSvcStopEvent = NULL;

void ReportServiceStatus(DWORD, DWORD, DWORD);
void WINAPI InstallService();
void WINAPI ServiceMain(DWORD dwArgCount, LPTSTR lpszArgValues[]);
DWORD WINAPI CtrlHandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID pEventData, LPVOID pUserData);
DWORD WINAPI AppMainFunction();
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void WINAPI UninstallService(bool showMBox);
void WINAPI Run(DWORD dwTargetSessionId, int desktop, LPTSTR lpszCmdLine);



enum Desktop
{
	User = 1, Winlogon
};

// Background thread function that tries to delete SG_WinService.exe a few times.
unsigned int __stdcall DeleteFileThread(void *param)
{
	const wchar_t *path = reinterpret_cast<const wchar_t*>(param);
	wprintf(L"Trying to delete file %s", path);

	::Sleep(300);	// Wait for 0.3s before the first attempt.

	for (int i = 0; i < 5; ++i)
	{
		if (DeleteFileW(path))
			return 0;

		::Sleep(1000); // Wait for 1.0s before the second, thrid... attempt.
	}

	return 1;
}


LONG GetStringRegKey(HKEY hKey, const std::wstring &strValueName, std::wstring &strValue, const std::wstring &strDefaultValue)
{
	strValue = strDefaultValue;
	TCHAR szBuffer[MAX_PATH];
	DWORD dwBufferSize = sizeof(szBuffer);
	ULONG nError;
	nError = RegQueryValueEx(hKey, strValueName.c_str(), 0, NULL, (LPBYTE)szBuffer, &dwBufferSize);
	if (ERROR_SUCCESS == nError)
	{
		strValue = szBuffer;
		if (strValue.front() == _T('"') && strValue.back() == _T('"')) {
			strValue.erase(0, 1); // erase the first character
			strValue.erase(strValue.size() - 1); // erase the last character
		}
	}
	return nError;
}

/*!
Check if the service is already installed based on version numbers of SG_WinService

\param szDirPath
On input the directory of SG_WinService, on output the directory if an update is required

\return
- true if the service is already installed and is not an older version
- false otherwise
*/
bool serviceAlreadyInstalledAndUpdated(TCHAR *szDirPath, size_t length)
{
	bool ret = false;

	bool serviceInstalled = false;
	SC_HANDLE scm_handle = OpenSCManager(0, 0, SC_MANAGER_CONNECT);
	if (scm_handle)
	{
		SC_HANDLE service_handle = OpenService(scm_handle, SERVICE_NAME, SERVICE_INTERROGATE);
		if (service_handle != NULL)
		{
			//MessageBox(NULL, _T("Service already Installed\n"), SERVICE_NAME, MB_ICONSTOP);
			serviceInstalled = true;
			CloseServiceHandle(service_handle);
		}
		else
		{
			wprintf(_T("OpenService failed - service not installed"));
		}
		CloseServiceHandle(scm_handle);
	}
	else
	{
		wprintf(_T("OpenSCManager failed - service not installed"));
	}

	std::wstring strValueOfImagePath;
	bool serviceInstalledIsOlder = false;
	if (serviceInstalled == true)
	{
		wprintf(_T("service already installed"));

		unsigned int svcFileVer[4];
		unsigned int exeFileVer[4];

		HKEY hKey;
		int n;
		CString KeyName;
		BOOL iswow64 = FALSE;
		IsWow64Process(GetCurrentProcess(), &iswow64);
		KeyName = (CString)(L"SYSTEM\\CurrentControlSet\\Services\\") + (CString)(SERVICE_NAME);
		if (iswow64)
		{
			n = RegOpenKeyEx(HKEY_LOCAL_MACHINE, KeyName.GetBuffer(), 0, KEY_READ | KEY_WOW64_64KEY, &hKey);
		}
		else
		{
			n = RegOpenKeyEx(HKEY_LOCAL_MACHINE, KeyName.GetBuffer(), 0, KEY_READ, &hKey);
		}
		if (n == ERROR_SUCCESS)
		{
			wprintf(_T("registry key HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\%s opened"), SERVICE_NAME);
			if (GetStringRegKey(hKey, L"ImagePath", strValueOfImagePath, L"") == ERROR_SUCCESS)
			{
				RegCloseKey(hKey);
				wprintf(_T("registry value ImagePath readed : %s"), strValueOfImagePath.c_str());
				DWORD  verHandle = NULL;
				DWORD verSize;
				//  = GetFileVersionInfoSize(strValueOfImagePath.c_str(), &verHandle);
				if ((verSize = GetFileVersionInfoSize(strValueOfImagePath.c_str(), NULL)) != 0)
					//if (verSize > 0) 
				{
					wprintf(_T("file version size readed"));

					BYTE *verData = new BYTE[verSize];
					if (GetFileVersionInfo(strValueOfImagePath.c_str(), verHandle, verSize, verData))
					{
						wprintf(_T("file version readed"));

						VS_FIXEDFILEINFO *pFileInfo = NULL;
						UINT puLenFileInfo = 0;
						if (VerQueryValue(verData, L"\\", (LPVOID*)&pFileInfo, &puLenFileInfo))
						{
							wprintf(_T("file version data readed"));

							svcFileVer[0] = (pFileInfo->dwFileVersionMS >> 16) & 0xffff;
							svcFileVer[1] = (pFileInfo->dwFileVersionMS >> 0) & 0xffff;
							svcFileVer[2] = (pFileInfo->dwFileVersionLS >> 16) & 0xffff;
							svcFileVer[3] = (pFileInfo->dwFileVersionLS >> 0) & 0xffff;

							WCHAR exePath[MAX_PATH];
							if (GetModuleFileName(NULL, exePath, MAX_PATH) != 0)
							{
								wprintf(_T("%s filename readed"), SERVICE_NAME);

								verHandle = NULL;
								verSize = GetFileVersionInfoSize(exePath, &verHandle);
								if (verSize > 0)
								{
									if (GetFileVersionInfo(exePath, verHandle, verSize, verData))
									{
										wprintf(_T("%s file version size readed"), SERVICE_NAME);

										if (VerQueryValue(verData, L"\\", (LPVOID*)&pFileInfo, &puLenFileInfo))
										{
											wprintf(_T("%s file version data readed"), SERVICE_NAME);


											exeFileVer[0] = (pFileInfo->dwFileVersionMS >> 16) & 0xffff;
											exeFileVer[1] = (pFileInfo->dwFileVersionMS >> 0) & 0xffff;
											exeFileVer[2] = (pFileInfo->dwFileVersionLS >> 16) & 0xffff;
											exeFileVer[3] = (pFileInfo->dwFileVersionLS >> 0) & 0xffff;


											TCHAR verStr[MAX_PATH];
											_stprintf_s(verStr, _T("Installed file version %d %d %d %d"), svcFileVer[0], svcFileVer[1], svcFileVer[2], svcFileVer[3]);
											wprintf(verStr);
											_stprintf_s(verStr, _T("%s file version %d %d %d %d"), SERVICE_NAME, exeFileVer[0], exeFileVer[1], exeFileVer[2], exeFileVer[3]);
											wprintf(verStr);

											if (svcFileVer[0] < exeFileVer[0])
											{
												serviceInstalledIsOlder = true;
											}
											else if (svcFileVer[0] == exeFileVer[0] && svcFileVer[1] < exeFileVer[1])
											{
												serviceInstalledIsOlder = true;
											}
											else if (svcFileVer[0] == exeFileVer[0] && svcFileVer[1] == exeFileVer[1] && svcFileVer[2] < exeFileVer[2])
											{
												serviceInstalledIsOlder = true;
											}
											else if (svcFileVer[0] == exeFileVer[0] && svcFileVer[1] == exeFileVer[1] && svcFileVer[2] == exeFileVer[2] && svcFileVer[3] < exeFileVer[3])
											{
												serviceInstalledIsOlder = true;
											}
										}
									}
								}
							}
						}
					}
					if (verData != NULL)
					{
						delete[] verData;
						verData = NULL;
					}
				}
				else
				{
					wprintf(_T("file version size invalid"));
					DWORD   dwLastError = ::GetLastError();
					TCHAR   lpBuffer[256] = _T("?");
					if (dwLastError != 0)    // Don't want to see a "operation done successfully" error ;-)
						::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,                 // It´s a system error
							NULL,                                      // No string to be formatted needed
							dwLastError,                               // Hey Windows: Please explain this error!
							MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  // Do it in the standard language
							lpBuffer,              // Put the message here
							255,                     // Number of bytes to store the message
							NULL);
					TCHAR errNum[500];
					wsprintf(errNum, L"%d", dwLastError);
				}
			}
		}
		else
		{
			wprintf(L"Error opening registry key '%s' error = %d however Service seems to be installed", KeyName, n);
			serviceInstalled = false;
		}

	}
	if (serviceInstalled == true && serviceInstalledIsOlder == true)
	{
		// Service installed but older. Remove the old service, instead of uninstall the service will be stopped, its executable
		// replaced and the the service will be restarted.

		wprintf(_T("service installed is older"));

		SC_HANDLE hSCManager;
		SC_HANDLE hService;
		hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
		if (NULL != hSCManager) {
			wprintf(_T("SCM opened..."));
			hService = OpenService(hSCManager, SERVICE_NAME, SERVICE_USER_DEFINED_CONTROL);// SERVICE_USER_DEFINED_CONTROL | SERVICE_QUERY_STATUS);
			if (hService != NULL) {
				wprintf(_T("Service opened..."));
				SERVICE_STATUS sStat;
				sStat.dwCurrentState = SERVICE_RUNNING;
				if (ControlService(hService, SERVICE_CONTROL_UNINSTALL, &sStat) == TRUE) {
					wprintf(_T("uninstall command sent..."));

					CloseServiceHandle(hService);
					Sleep(1000);
					hService = OpenService(hSCManager, SERVICE_NAME, SERVICE_USER_DEFINED_CONTROL | SERVICE_QUERY_STATUS);

					if (hService != NULL) {
						DWORD dwBytesNeeded;
						DWORD dwStartTime = GetTickCount();
						DWORD dwTimeout = 30000; // 30-second time-out

						while (sStat.dwCurrentState != SERVICE_STOPPED) {
							Sleep(sStat.dwWaitHint);
							if (!QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&sStat, sizeof(SERVICE_STATUS_PROCESS), &dwBytesNeeded)) {
								wprintf(_T("QueryServiceStatusEx failed"));
								return 0;
							}
							if (sStat.dwCurrentState == SERVICE_STOPPED) {
								wprintf(_T("Service stopped..."));
								break;
							}
							if (GetTickCount() - dwStartTime > dwTimeout)
							{
								wprintf(_T("Command timeout"));
								return 0;
							}
						}
						if (sStat.dwCurrentState == SERVICE_STOPPED) {
							// Service has been stopped wait until the exe becomes writable
							while (true) {
								std::ofstream outputFile(strValueOfImagePath.c_str(), std::ios::binary);
								if (outputFile.good()) {
									outputFile.close();
									break;
								}
								Sleep(1000);
							}
						}
						//MessageBox(NULL, _T("Service stopped"), SERVICE_NAME, MB_OK);
					}
				}
				CloseServiceHandle(hService);
			}
			CloseServiceHandle(hSCManager);
		}

		// The new service version has to be installed in the same directory of the older one, so change the installation path
		if (szDirPath != NULL && strValueOfImagePath.empty() == false) {
			_tcscpy_s(szDirPath, length, strValueOfImagePath.c_str());
			PathRemoveFileSpec(szDirPath);
		}
		ret = false;
	}
	else if (serviceInstalled == true && serviceInstalledIsOlder == false) {
		// Service installed and updated
		ret = true;
	}
	else {
		// Service not installed
		ret = false;
	}

	return ret;
}

/*!
Service main routine.

SG_WinService.exe Install				-	Service installation
SG_WinService.exe ServiceIsLuncher		-	Start the client
*/

HANDLE hConsole;
// 
int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpszCmdLine, int nCmdShow) 
{	
	LPWSTR command = L"";
	{
		int argc;
		wchar_t **argv = CommandLineToArgvW(::GetCommandLineW(), &argc);
		if (argc > 0)
			command = argv[argc - 1];
	}
	AllocConsole();
	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	int fontsize_x = utils::getfontsize();
	CString Intro = L"Secured Globe Generic Windows Service\n(c)2019 Secured Globe, Inc.\n\nversion 1.0	June, 2019\n\n";
	CString Instructions = L"\nPress ALT+SHIFT+U to uninstall the service";
	utils::setcolor(LOG_COLOR_YELLOW,0);
	utils::fontsize(50, 50);
	wprintf(L"SG_WinService");
	utils::setcolor(LOG_COLOR_DARKBLUE, 0);
	utils::fontsize(30, 30);
	wprintf(L"%s", Intro);
	utils::setcolor(LOG_COLOR_MAGENTA, 0);
	wprintf(L"%s", Instructions);
	utils::setcolor(LOG_COLOR_WHITE, 0);
	utils::fontsize(fontsize_x, fontsize_x);
	wprintf(L"SG_WinService: command = '%s'\n\n", command);

	if (::wcsstr(command, SERVICE_COMMAND_INSTALL) != NULL)
	{
		// Obtaining the full path of the service and adding the special service name quotations
		TCHAR szPath[MAX_PATH] = { 0 };
		GetModuleFileName(NULL, szPath, MAX_PATH);

		wprintf(L"Option 1 - Install");

		bool serviceAlreadyInstalled = serviceAlreadyInstalledAndUpdated(szPath, ARRAYSIZE(szPath));
		if (serviceAlreadyInstalled)
		{
			wprintf(L"Service already installed");

		}
		else
		{
			// If the /Install param contains in addition the invoker file name+path delete this file
			HANDLE hThread = NULL;

			wchar_t *path = ::wcschr(command, L'#');
			if (path)
			{
				path++;

				unsigned int threadID;
				hThread = reinterpret_cast<HANDLE>(
					_beginthreadex(NULL, 0, &DeleteFileThread, path, 0, &threadID));
			}

			InstallService();

			if (hThread)
			{
				// The background thread lasts around 5.3 secs at most.
				::WaitForSingleObject(hThread, INFINITE);
				::CloseHandle(hThread);
			}
		}
	}
	else if (::wcsstr(command, SERVICE_COMMAND_LUNCHER) != NULL)
	{
		wprintf(L"Service Launched");

		AppMainFunction();
	}
	else // called with no args
	{
		wprintf(L"Option 4 - No args");

		SERVICE_TABLE_ENTRY serviceTableEntry[] = 
		{ 
			{ 
				SERVICE_NAME,ServiceMain 
			}, 
			{ 
				NULL, NULL 
			} 
		};
		StartServiceCtrlDispatcher(serviceTableEntry);
	}

	wprintf(L"SG_WinService ended");

	return 0;
}





/*!
Service uninstall

\param showMBox
If true open the "Service Uninstalled" message box 
*/
void WINAPI UninstallService(bool showMBox) 
{
	MessageBox(NULL, L"UNINSTALL_HOTKEY_ID", L"WndProc", MB_OK);

	SC_HANDLE hSCManager = NULL;
	SC_HANDLE hService = NULL;

	hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (NULL == hSCManager) 
	{
		wprintf(DEBUG_MESSAGE_OPENSCMANAGER_FAILED);
		return;
	}

	hService = OpenService(hSCManager, SERVICE_NAME, DELETE | SERVICE_STOP);
	if (hService == NULL) 
	{
		CloseServiceHandle(hSCManager);
		wprintf(DEBUG_MESSAGE_OPENSERVICE_FAILED);
		return;
	}


	// Deleting the service
	if (!DeleteService(hService)) 
	{
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCManager);
		wprintf(DEBUG_MESSAGE_DELETESERVICE_FAILED);
		return;
	}
	wprintf(DEBUG_MESSAGE_SERVICEDELETED_SUCCESS);

	// Stopping the service  
	serviceStatus = { 0 };
	ControlService(hService, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS)&serviceStatus);

	CloseServiceHandle(hService);
	CloseServiceHandle(hSCManager);

	exit(0);

}



DWORD GetServiceProcessID(SC_HANDLE hService)
{
    SERVICE_STATUS_PROCESS  serviceStatus;
    DWORD                   dwBytesNeeded;

    DWORD result = ::QueryServiceStatusEx(hService
                                         ,SC_STATUS_PROCESS_INFO
                                         ,reinterpret_cast<unsigned char*>(&serviceStatus)
                                         ,sizeof(SERVICE_STATUS_PROCESS)
                                         ,&dwBytesNeeded);

    return (result != 0) ? serviceStatus.dwProcessId : 0;
}

/*!
Service installation
*/
void WINAPI InstallService()
{
	wprintf(DEBUG_MESSAGE_INSTALLSERVICE_SUCCESS);


	// Obtaining the full path of the service and adding the special service name quotations
	TCHAR szServicePath[MAX_PATH] = { _T("\"") };
	TCHAR szPath[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, szPath, MAX_PATH);
	lstrcat(szServicePath, szPath);
	lstrcat(szServicePath, _T("\""));

	SC_HANDLE hSCManager = NULL;
	SC_HANDLE hService = NULL;

	hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

	if (NULL == hSCManager)
	{
		wprintf(DEBUG_MESSAGE_OPENSCMANAGER_FAILED);
		return;
	}

	hService = CreateService(hSCManager, SERVICE_NAME, NT_SERVICE_DISPLAY_NAME, SERVICE_ALL_ACCESS | SERVICE_USER_DEFINED_CONTROL | READ_CONTROL
		| WRITE_DAC | WRITE_OWNER, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
		szServicePath, NULL, NULL, _T("TermService\0"), NULL, _T(""));

	if (hService == NULL)
	{
		// Service already installed, just open it
		wprintf(_T("CreateService failed"));
		hService = OpenService(hSCManager, SERVICE_NAME, SERVICE_ALL_ACCESS);
		if (hService == NULL) 
		{
			wprintf(_T("OpenService failed"));
			CloseServiceHandle(hSCManager);
			return;
		}
		return;
	}

	

	// Starting the service
	wprintf(_T("start service..."));
	if(0 == StartService(hService, 0, NULL))
	{
        wprintf(_T("StartService failed. Error: %d"), ::GetLastError());
	}
    else
    {
        wprintf(_T("StartService success. Started service Process ID: %d"), GetServiceProcessID(hService));
    }

	CloseServiceHandle(hService);
	CloseServiceHandle(hSCManager);
}


/*!
*/
void ReportServiceStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
	wprintf(L"Service Status %d", dwCurrentState);
	static DWORD dwCheckPoint = 1;

	serviceStatus.dwCurrentState = dwCurrentState;
	serviceStatus.dwWin32ExitCode = dwWin32ExitCode;
	serviceStatus.dwWaitHint = dwWaitHint;

	if (dwCurrentState == SERVICE_START_PENDING)
		serviceStatus.dwControlsAccepted = 0;
	else
		serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SESSIONCHANGE;
		//serviceStatus.dwControlsAccepted =	SERVICE_ACCEPT_PAUSE_CONTINUE | SERVICE_ACCEPT_PARAMCHANGE | SERVICE_ACCEPT_NETBINDCHANGE | SERVICE_ACCEPT_HARDWAREPROFILECHANGE | SERVICE_ACCEPT_POWEREVENT | 
		//									SERVICE_ACCEPT_PRESHUTDOWN | SERVICE_ACCEPT_TIMECHANGE | SERVICE_ACCEPT_TRIGGEREVENT |SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SESSIONCHANGE;

	if ((dwCurrentState == SERVICE_RUNNING) || (dwCurrentState == SERVICE_STOPPED))
		serviceStatus.dwCheckPoint = 0;
	else
		serviceStatus.dwCheckPoint = dwCheckPoint++;

	SetServiceStatus(hServiceStatus, &serviceStatus);
}


/*!
*/
void WINAPI ServiceMain(DWORD dwArgCount, LPTSTR lpszArgValues[]) 
{
	AllocConsole();
	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	wprintf(_T("ServiceMain start"));

	ZeroMemory(&serviceStatus, sizeof(SERVICE_STATUS));
	serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	hServiceStatus = RegisterServiceCtrlHandlerEx(SERVICE_NAME, CtrlHandlerEx, NULL);

	ReportServiceStatus(SERVICE_START_PENDING, NO_ERROR, 0);

	// Service stopped event for sync between client and service
	ghSvcStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (ghSvcStopEvent == NULL) 
	{
		ReportServiceStatus(SERVICE_STOPPED, NO_ERROR, 0);
		return;
	}

	wprintf(_T("launch client ..."));
	DWORD dwActiveSession = WTSGetActiveConsoleSessionId();
	Run(dwActiveSession, User, SERVICE_COMMAND_LUNCHER);

	ReportServiceStatus(SERVICE_RUNNING, NO_ERROR, 0);

	while (1)
	{
		// Check whether to stop the service.
		WaitForSingleObject(ghSvcStopEvent, INFINITE);
		CloseHandle(ghSvcStopEvent);
		ReportServiceStatus(SERVICE_STOPPED, NO_ERROR, 0);
		wprintf(_T("ServiceMain end"));
		return;
	}
}

void WINAPI Run(DWORD dwTargetSessionId, int desktop, LPTSTR lpszCmdLine)
{
	wprintf(_T("Run client start"));

	if (hPrevAppProcess != NULL)
	{
		TerminateProcess(hPrevAppProcess, 0);
		WaitForSingleObject(hPrevAppProcess, INFINITE);
	}


	HANDLE hToken = 0;
	WTS_SESSION_INFO *si;
	DWORD cnt = 0;
	WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &si, &cnt);
	for (int i = 0; i < (int)cnt; i++)
	{
		if (si[i].SessionId == 0)
		{
			wprintf(_T("Trying session id %i (%s) user admin token (sessionId = 0) - continuing"), si[i].SessionId, si[i].pWinStationName);
			continue;
		}
		wprintf(_T("Trying session id %i (%s) user admin token"), si[i].SessionId, si[i].pWinStationName);
		HANDLE userToken;
		if (WTSQueryUserToken(si[i].SessionId, &userToken))
		{
			wprintf(L"Success\n");
			TOKEN_LINKED_TOKEN *admin;
			DWORD dwSize,dwResult;
			// The data area passed to a system call is too small
			 // Call GetTokenInformation to get the buffer size.

			if (!GetTokenInformation(hToken, TokenLinkedToken, NULL, dwSize, &dwSize))
			{
				dwResult = GetLastError();
				if (dwResult != ERROR_INSUFFICIENT_BUFFER) 
				{
					wprintf(L"GetTokenInformation Error %u\n", dwResult);
					break;
				}
			}

			// Allocate the buffer.

			admin = (TOKEN_LINKED_TOKEN *)GlobalAlloc(GPTR, dwSize);

			// Call GetTokenInformation again to get the group information.

			if (!GetTokenInformation(hToken, TokenLinkedToken, admin,
				dwSize, &dwSize))
			{
				wprintf(L"GetTokenInformation Error %u\n", GetLastError());
				break;
			}
			else
			{ 
				wprintf(_T("GetTokenInformation success"));
				hToken = admin->LinkedToken;
				break;
			}
			CloseHandle(userToken);
		}
		else
		{
			DWORD error = GetLastError();
			{
				LPVOID lpMsgBuf;
				DWORD bufLen = FormatMessage(
					FORMAT_MESSAGE_ALLOCATE_BUFFER |
					FORMAT_MESSAGE_FROM_SYSTEM |
					FORMAT_MESSAGE_IGNORE_INSERTS,
					NULL,
					error,
					MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
					(LPTSTR)&lpMsgBuf,
					0, NULL);
				wprintf(L"Error '%s'\n", lpMsgBuf);
			}
		}
	}
	if (hToken == 0)
	{
		HANDLE systemToken;
		OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &systemToken);
		DuplicateTokenEx(systemToken, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenPrimary, &hToken);
		CloseHandle(systemToken);
		int i;
		for (i = 0; i < (int)cnt; i++)
		{
			if (si[i].SessionId == 0)continue;
			if (SetTokenInformation(hToken, TokenSessionId, &si[i].SessionId, sizeof(DWORD)))
			{
				wprintf(_T("Success using system token with set user session id %i"), si[i].SessionId);
				break;
			}
		}
		if (i == cnt)
			wprintf(_T("No success to get user admin token nor system token with set user session id"));
	}
	WTSFreeMemory(si);


	STARTUPINFO startupInfo = {};
	startupInfo.cb = sizeof(STARTUPINFO);

	//if (desktop == Winlogon)
		//startupInfo.lpDesktop = _T("winsta0\\winlogon");
	//else
	startupInfo.lpDesktop = _T("winsta0\\default");

	LPVOID pEnv = NULL;
	CreateEnvironmentBlock(&pEnv, hToken, TRUE);

	PROCESS_INFORMATION processInfo = {};
	PROCESS_INFORMATION processInfo32 = {};

	TCHAR szCurModule[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, szCurModule, MAX_PATH);

	BOOL bRes = FALSE;

	{
		std::wstring commandLine;
		commandLine.reserve(1024);

		commandLine += L"\"";
		commandLine += szCurModule;
		commandLine += L"\" \"";
		commandLine += SERVICE_COMMAND_LUNCHER;
		commandLine += L"\"";

		wprintf(_T("launch SG_WinService with CreateProcessAsUser ...  %s"), commandLine.c_str());

		bRes = CreateProcessAsUserW(hToken, NULL, &commandLine[0], NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS |
			CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_CONSOLE | CREATE_DEFAULT_ERROR_MODE, pEnv,
			NULL, &startupInfo, &processInfo);

		if (bRes == FALSE) {
			DWORD   dwLastError = ::GetLastError();
			TCHAR   lpBuffer[256] = _T("?");
			if (dwLastError != 0)    // Don't want to see a "operation done successfully" error ;-)
				::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,                 // It´s a system error
					NULL,                                      // No string to be formatted needed
					dwLastError,                               // Hey Windows: Please explain this error!
					MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  // Do it in the standard language
					lpBuffer,              // Put the message here
					255,                     // Number of bytes to store the message
					NULL);
			wprintf(_T("CreateProcessAsUser(SG_WinService) failed - Error : %s (%i)"), lpBuffer, dwLastError);
		}
		else
		{
			wprintf(_T("CreateProcessAsUser(SG_WinService) success. New process ID: %i"), processInfo.dwProcessId);
		}
	}
}

/*!
*/
DWORD WINAPI CtrlHandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID pEventData, LPVOID pUserData) 
{
	switch (dwControl) 
	{
		case SERVICE_CONTROL_SHUTDOWN:
		case SERVICE_CONTROL_STOP:
			{
				if (dwControl == SERVICE_CONTROL_SHUTDOWN)
					wprintf(_T("SERVICE_CONTROL_SHUTDOWN"));
				else
					wprintf(_T("SERVICE_CONTROL_STOP"));
				// Service stopped by the user
				ReportServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
				//if (hPrevAppProcess != NULL) {
				//	TerminateProcess(hPrevAppProcess, 0);
				//	WaitForSingleObject(hPrevAppProcess, INFINITE);
				//	hPrevAppProcess = NULL;
				//}
				SetEvent(ghSvcStopEvent);
				ReportServiceStatus(serviceStatus.dwCurrentState, NO_ERROR, 0);
			}
			break;
		case SERVICE_CONTROL_UNINSTALL:
			{

				// Service update message, the service has to be stopped before its update
				ReportServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
				if (hPrevAppProcess != NULL) 
				{
					TerminateProcess(hPrevAppProcess, 0);
					WaitForSingleObject(hPrevAppProcess, INFINITE);
					hPrevAppProcess = NULL;
				}
				SetEvent(ghSvcStopEvent);
				ReportServiceStatus(serviceStatus.dwCurrentState, NO_ERROR, 0);
			}
			break;
		case SERVICE_CONTROL_PAUSE:
			{
				wprintf(_T("SERVICE_CONTROL_PAUSE"));
			}
			break;
		case SERVICE_CONTROL_CONTINUE:
			{
				wprintf(_T("SERVICE_CONTROL_CONTINUE"));
			}
			break;
		case SERVICE_CONTROL_INTERROGATE:
			{
				wprintf(_T("SERVICE_CONTROL_INTERROGATE"));
			}
			break;
		case SERVICE_CONTROL_PARAMCHANGE:
			{
				wprintf(_T("SERVICE_CONTROL_PARAMCHANGE"));
			}
			break;
		case SERVICE_CONTROL_NETBINDADD:
			{
				wprintf(_T("SERVICE_CONTROL_NETBINDADD"));
			}
			break;
		case SERVICE_CONTROL_NETBINDREMOVE:
			{
				wprintf(_T("SERVICE_CONTROL_NETBINDREMOVE"));
			}
			break;
		case SERVICE_CONTROL_NETBINDENABLE:
			{
				wprintf(_T("SERVICE_CONTROL_NETBINDENABLE"));
			}
			break;
		case SERVICE_CONTROL_NETBINDDISABLE:
			{
				wprintf(_T("SERVICE_CONTROL_NETBINDDISABLE"));
			}
			break;
		case SERVICE_CONTROL_DEVICEEVENT:
			{
				wprintf(_T("SERVICE_CONTROL_DEVICEEVENT"));
			}
			break;
		case SERVICE_CONTROL_HARDWAREPROFILECHANGE:
			{
				wprintf(_T("SERVICE_CONTROL_HARDWAREPROFILECHANGE"));
			}
			break;
		case SERVICE_CONTROL_POWEREVENT:
			{
				wprintf(_T("SERVICE_CONTROL_POWEREVENT"));
			}
			break;
		case SERVICE_CONTROL_SESSIONCHANGE:
			{
				wprintf(_T("SERVICE_CONTROL_SESSIONCHANGE"));
				switch (dwEventType) {
					case WTS_CONSOLE_CONNECT:
						{
							wprintf(_T("eventType = WTS_CONSOLE_CONNECT"));
						}
						break;
					case WTS_CONSOLE_DISCONNECT:
						{
							wprintf(_T("eventType = WTS_CONSOLE_DISCONNECT"));
						}
						break;
					case WTS_REMOTE_CONNECT:
						{
							wprintf(_T("eventType = WTS_REMOTE_CONNECT"));
						}
						break;
					case WTS_REMOTE_DISCONNECT:
						{
							wprintf(_T("eventType = WTS_REMOTE_DISCONNECT"));
						}
						break;
					case WTS_SESSION_LOGON:
						{
							// User logon, startup the client app
							wprintf(_T("eventType = WTS_SESSION_LOGON"));
						}
						break;
					case WTS_SESSION_LOGOFF:
						{
							// User logoff, the client app has been closed, reset hPrevAppProcess
							wprintf(_T("eventType = WTS_SESSION_LOGOFF"));
							hPrevAppProcess = NULL;
						}
						break;
					case WTS_SESSION_LOCK:
						{
							wprintf(_T("eventType = WTS_SESSION_LOCK"));
						}
						break;
					case WTS_SESSION_UNLOCK:
						{
							wprintf(_T("eventType = WTS_SESSION_UNLOCK"));
						}
						break;
					case WTS_SESSION_REMOTE_CONTROL:
						{
							wprintf(_T("eventType = WTS_SESSION_REMOTE_CONTROL"));
						}
						break;
					case WTS_SESSION_CREATE:
						{
							wprintf(_T("eventType = WTS_SESSION_CREATE"));
						}
						break;
					case WTS_SESSION_TERMINATE:
						{
							wprintf(_T("eventType = WTS_SESSION_TERMINATE"));
						}
						break;
				}
			}
			break;
		case SERVICE_CONTROL_PRESHUTDOWN:
			{
				wprintf(_T("SERVICE_CONTROL_PRESHUTDOWN"));
			}
			break;
		case SERVICE_CONTROL_TIMECHANGE:
			{
				wprintf(_T("SERVICE_CONTROL_TIMECHANGE"));
			}
			break;
		case SERVICE_CONTROL_TRIGGEREVENT:
			{
				wprintf(_T("SERVICE_CONTROL_TRIGGEREVENT"));
			}
			break;
		default:
			return ERROR_CALL_NOT_IMPLEMENTED;
	}

	ReportServiceStatus(serviceStatus.dwCurrentState, NO_ERROR, 0);
	return NO_ERROR;
}


void MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex = { sizeof(wcex) };
	wcex.style =			CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc =		WndProc;
	wcex.hInstance =		hInstance;
	wcex.hIcon =			NULL;
	wcex.hCursor =			NULL;
	wcex.hbrBackground =	NULL;
	wcex.lpszMenuName =		NULL;
	wcex.lpszClassName =	MAIN_CLASS_NAME;
	RegisterClassEx(&wcex);
}

/*!
Service client main application
*/
DWORD WINAPI AppMainFunction() 
{
	wprintf(_T("AppMainFunction start"));

	HINSTANCE hInstance = GetModuleHandle(NULL);


	// Setting the working directory to the currect
	// application folder rather than System32
	TCHAR szDirPath[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, szDirPath, MAX_PATH);
	PathRemoveFileSpec(szDirPath);
	SetCurrentDirectory(szDirPath);

	// Registering the main window class
	MyRegisterClass(hInstance);

	// Creating the main window
	hWnd = CreateWindow(MAIN_CLASS_NAME, _T(""), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0,
		CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

	if (hWnd)
	{

		SetTimer(hWnd, MAIN_TIMER_ID, 10000, NULL);

		// Registering our main window to receive session notifications
		WTSRegisterSessionNotification(hWnd, NOTIFY_FOR_ALL_SESSIONS);
		RegisterHotKey(hWnd, UNINSTALL_HOTKEY_ID, MOD_ALT | MOD_SHIFT, L'U');
	}
	else
		wprintf(L"CreateWindow failed");


	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	hWnd = NULL;
	


	wprintf(_T("AppMainFunction end"));
	return (DWORD)msg.wParam;
}

/*!
Service client message loop
*/
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	//int wmId, wmEvent;
	std::wstring drive;
	static HDEVNOTIFY hDeviceNotify;
	wprintf(L"Main event loop");
	switch (message)
	{
		case WM_TIMER:
		{
			wprintf(_T("Timer ..."));

		}
		break;
		case WM_HOTKEY:
		{
			if ((WORD)wParam == UNINSTALL_HOTKEY_ID) 
			{
				wprintf(_T("uninstall service with Alt+Shift+U from SG_WinService ..."));

				// Terminating the monitor application
				WTSUnRegisterSessionNotification(hWnd);
				DestroyWindow(hWnd);
				UninstallService(true);
			}
		}
		break;
		default: return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}



