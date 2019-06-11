/*
	SG_WinService
	Secured Globe Generic Windows Service
	©2019 Secured Globe, Inc.

	version 1.0	June, 2019
*/
#pragma once

// Customizable values
#define NT_SERVICE_WIN			_T("SG_WinService.exe")			// Windows Svc Win
#define NT_SERVICE_DISPLAY_NAME _T("SG Win Service")			// Service display name
#define SERVICE_NAME			_T("SG_WinService")				// Service name
#define SERVICE_COMMAND_INSTALL _T("Install")					// The command line argument for installing the service
#define SERVICE_COMMAND_LUNCHER _T("ServiceIsLuncher")			// Luncher command for NT service
#define MAIN_CLASS_NAME			_T("SG_WinService_CLASS_NAME")	// Window class name for service client

#define MAIN_TIMER_ID			1
// Constant values
#define B_SLASH _T("\\")											// Backward slash '\'

// Messages
#define DEBUG_MESSAGE_OPENSCMANAGER_FAILED _T("OpenSCManager failed")
#define DEBUG_MESSAGE_OPENSERVICE_FAILED _T("OpenService failed")
#define DEBUG_MESSAGE_DELETESERVICE_FAILED _T("DeleteService failed")
#define DEBUG_MESSAGE_SERVICEDELETED_SUCCESS _T("Service deleted")
#define DEBUG_MESSAGE_SERVICEUNINSTALLED_SUCCESS L"Service Uninstalled"
#define DEBUG_MESSAGE_SETUP_OPEN_INF_FILE _T("SetupOpenInfFile(%s,NULL,INF_STYLE_WIN4,ErrorLine)")
#define DEBUG_MESSAGE_SETUP_INSTALL_FILE _T("SetupInstallFileEx(HInf,NULL,%s,%s,%s,SP_COPY_NEWER_OR_SAME,NULL,NULL,FileWasInUse)")
#define DEBUG_MESSAGE_CREATESERVICE_FAILED _T("CreateService failed")
#define DEBUG_MESSAGE_INSTALLSERVICE_SUCCESS _T("InstallService start")
#define DATE_FORMAT L"%Y%m%d%H%M%S"

