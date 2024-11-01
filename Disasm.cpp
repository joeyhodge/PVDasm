      
/*
     8888888b.                  888     888 d8b                        
     888   Y88b                 888     888 Y8P                        
     888    888                 888     888                            
     888   d88P 888d888  .d88b. Y88b   d88P 888  .d88b.  888  888  888 
     8888888P"  888P"   d88""88b Y88b d88P  888 d8P  Y8b 888  888  888 
     888        888     888  888  Y88o88P   888 88888888 888  888  888 
     888        888     Y88..88P   Y888P    888 Y8b.     Y88b 888 d88P 
     888        888      "Y88P"     Y8P     888  "Y8888   "Y8888888P"  

Disassembler
             PE Editor & Dissasembler & File Identifier
             ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 Written by Shany Golan.
 In January, 2003.
 I have investigated P.E / Opcodes file(s) format as thoroughly as possible,
 But I cannot claim that I am an expert yet, so some of its information
 May give you wrong results.

 Language used: Visual C++ 6.0
 Date of creation: July 06, 2002

 Date of first release: unknown ??, 2003

 You can contact me: e-mail address: Shanytc@yahoo.com
                            
 Copyright (C) 2011. By Shany Golan.

 Permission is granted to make and distribute verbatim copies of this
 Program provided the copyright notice and this permission notice are
 Preserved on all copies.


 File: Disasm.cpp (main)
   This program was written by Shany Golan, Student at :
                    Ruppin, department of computer science and engineering University.
*/

// ================================================================
// ==========================  INCLUDES  ==========================
// ================================================================

#include "Disasm.h"
#include "functions.h"
#include "assert.h"
#include "CDisasmData.h"
#include "Chip8.h"
#include "Wizard.h"
#include <vector>
#include <map>
#include <algorithm>
 
// External Data
#ifndef _EXTERN_DATA_
#define _EXTERN_DATA_

using namespace std;  // Use Microsoft's STL Template Implemation

// ================================================================
// ========================  TYPEDEF  =============================
// ================================================================

typedef vector<CDisasmData>				DisasmDataArray;
typedef DisasmDataArray::iterator		ItrDisasm;
typedef vector<int>						DisasmImportsArray,IntArray;
typedef vector<CODE_BRANCH>				CodeBranch;
typedef vector<BRANCH_DATA>				BranchData;
typedef vector<FUNCTION_INFORMATION>	FunctionInfo;
typedef FunctionInfo::iterator			FunctionInfoItr;
typedef multimap<const int, int>		XRef,MapTree;
typedef pair<const int const, int>		XrefAdd,MapTreeAdd;
typedef XRef::iterator					ItrXref;
typedef MapTree::iterator				TreeMapItr;
typedef MapTree::reverse_iterator		rTreeMapItr;
typedef IntArray::reverse_iterator		rIntArray;
typedef DisasmImportsArray::iterator	ItrImports;

// ================================================================
// =====================  EXTERNAL VARIABLES  =====================
// ================================================================

// Extern Variables from FileMap.cpp

extern DisasmDataArray		DisasmDataLines; 
extern DisasmImportsArray	ImportsLines;
extern DisasmImportsArray	StrRefLines;
extern XRef					XrefData;
extern CodeBranch			DisasmCodeFlow;
extern BranchData			BranchTrace;
extern bool					FilesInMemory;
extern bool					LoadedPe,LoadedPe64,LoadedNe;
extern bool					DisassemblerReady;
extern char					Buffer[128];
extern HWND					hWndTB;
extern DataMapTree			DataSection;
extern CodeSectionArray		CodeSections;
extern FunctionInfo			fFunctionInfo;

// ================================================================
// ==========================  DEFINES  ===========================
// ================================================================

#define AddNew(key,data) XrefData.insert(XrefAdd(key,data));
#define AddNewData(key,data) DataAddersses.insert(MapTreeAdd(key,data));
#define AddFunction(Function) fFunctionInfo.insert(fFunctionInfo.end(),Function);

#endif

#define USE_MMX

#ifdef USE_MMX
	#ifndef _M_X64
		#define StringLen(str) xlstrlen(str) // new lstrlen with mmx function
	#else
		#define StringLen(str) lstrlen(str) // old c library strlen
	#endif
#else
	#define StringLen(str) lstrlen(str) // old c library strlen
#endif

// ================================================================
// ======================  GLOBAL VARIABLES  ======================
// ================================================================

DISASM_OPTIONS			disop;			// Disasm options struct
HWND					mainhWnd;		// main program's hWnd
HWND					CurrentWin;
DWORD					DisasmThreadId;	// Disasm's TreadID
DWORD_PTR				RangeFromEP;
HANDLE					hDisasmThread=0;// Disasm's Thread handler
IMAGE_SECTION_HEADER*	SectionHeader;
IMAGE_DOS_HEADER*		DosHeader;
IMAGE_NT_HEADERS*		NTheader;
IMAGE_NT_HEADERS64*		NTheader64;
MapTree					DataAddersses;	// Sets all range of valid Code
bool					CallApi;
bool					CallAddrApi;
bool					JumpApi,DirectJmp;
bool					PushString;
bool					DisasmIsWorking=false;
bool					FunctionsEntryPoint;
bool					LoadFirstPass=TRUE;
DWORD_PTR				Address;

// ================================================================
// ========================	CONST VARIABLES	=======================
// ================================================================

// Size of registers
const char* RegSize[4] = {"Qword","Dword","Word","Byte"};
// Segments
const char* Segs[8]	= {"ES","CS","SS","DS","FS","GS","SEG?","SEG?"};
char*	DisasmPtr;
char*	LoadedFileName;
bool	PE_File=false;
// 8/16/32 Registers
const char *Regs[3][9] = {
	{"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"},
	{"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"},
	{"eax","ecx","edx","ebx","esp","ebp","esi","edi"} 
};

// ================================================================
// ========================  DIALOGS  =============================
// ================================================================

BOOL CALLBACK DisasmDlgProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch(Message)
	{
		case WM_INITDIALOG:
		{
			// create A copy
			CurrentWin = hWnd;

			OutDebug(mainhWnd,"Loading Disassembly Option Dialog...");
			SelectLastItem(GetDlgItem(mainhWnd,IDC_LIST)); // Selects the Last Item
            
			// set all content to Zero
			memset(&disop,0,sizeof(disop));

			// caption of loaded file (full path)
			SetDlgItemText(hWnd,IDC_LOADED_FILE,LoadedFileName);
			// add list box options
			
			#ifdef BINARY_FORMAT
				SendDlgItemMessage(hWnd,IDC_FILE_FORMAT,LB_ADDSTRING,(WPARAM)0,(LPARAM)BINARY_FORMAT);
		    #endif
			
			SendDlgItemMessage(hWnd,IDC_FILE_FORMAT,LB_SETCURSEL,(WPARAM)0,(LPARAM)0);
			// add combo box options

			// check if file has a valid pe header and add options for it 
			// in the listBox
			if(PE_File==TRUE){	
				#ifdef x86_PC
					if(LoadedPe==TRUE){
						SendDlgItemMessage(hWnd,IDC_CPUS,CB_ADDSTRING,(WPARAM)0,(LPARAM)x86_PC);
						SendDlgItemMessage(hWnd,IDC_CPUS,CB_SETITEMDATA,(WPARAM)0,(LPARAM)x86);
					}
                #endif

				#ifdef x86_PC_16Bit
					if(LoadedNe==TRUE){
						SendDlgItemMessage(hWnd,IDC_CPUS,CB_ADDSTRING,(WPARAM)0,(LPARAM)x86_PC_16Bit);
						SendDlgItemMessage(hWnd,IDC_CPUS,CB_SETITEMDATA,(WPARAM)0,(LPARAM)x86_16Bit);
					}
				#endif

				#ifdef x87_PC
					if(LoadedPe64==TRUE){
						SendDlgItemMessage(hWnd,IDC_CPUS,CB_ADDSTRING,(WPARAM)0,(LPARAM)x87_PC);
						SendDlgItemMessage(hWnd,IDC_CPUS,CB_SETITEMDATA,(WPARAM)0,(LPARAM)x87);
					}
				#endif
					
				disop.DecodePE=1;

				// if defined PE format to decode
				#ifdef PE_FORMAT
					if(LoadedPe==TRUE){
						SendDlgItemMessage(hWnd,IDC_FILE_FORMAT,LB_ADDSTRING,(WPARAM)0,(LPARAM)PE_FORMAT);
					}
				#endif

				#ifdef PE64_FORMAT
					if(LoadedPe64==TRUE){
						SendDlgItemMessage(hWnd,IDC_FILE_FORMAT,LB_ADDSTRING,(WPARAM)0,(LPARAM)PE64_FORMAT);
					}
				#endif

				#ifdef NE_FORMAT
					if(LoadedNe==TRUE){
						SendDlgItemMessage(hWnd,IDC_FILE_FORMAT,LB_ADDSTRING,(WPARAM)0,(LPARAM)NE_FORMAT);
					}
				#endif

				// if defined MZ format to decode
			    #ifdef MZ_FORMAT
					SendDlgItemMessage(hWnd,IDC_FILE_FORMAT,LB_ADDSTRING,(WPARAM)0,(LPARAM)MZ_FORMAT);
				#endif

				SendDlgItemMessage(hWnd,IDC_FILE_FORMAT,LB_SETCURSEL,(WPARAM)2,(LPARAM)0);
			}
            else{
                #ifdef CPU_chip8
                    SendDlgItemMessage(hWnd,IDC_CPUS,CB_ADDSTRING,(WPARAM)1,(LPARAM)CPU_chip8);
                    SendDlgItemMessage(hWnd,IDC_CPUS,CB_SETITEMDATA,(WPARAM)0,(LPARAM)chip8);
                #endif
            }
            
            SendDlgItemMessage(hWnd,IDC_CPUS,CB_SETCURSEL,(WPARAM)0,(LPARAM)0);

			// Show default cpu for exe files.
			SetFocus(GetDlgItem(hWnd,IDC_CPUS));
			SendDlgItemMessage(hWnd,IDC_SHOW_SECTIONS,BM_SETCHECK,(WPARAM)BST_UNCHECKED,(LPARAM)0);
			SendDlgItemMessage(hWnd,IDC_UPPERCASE_DISASM,BM_SETCHECK,(WPARAM)BST_CHECKED,(LPARAM)0);
			SendDlgItemMessage(hWnd,IDC_SHOW_ADDR,BM_SETCHECK,(WPARAM)BST_CHECKED,(LPARAM)0);
			SendDlgItemMessage(hWnd,IDC_SHOW_REMARKS,BM_SETCHECK,(WPARAM)BST_CHECKED,(LPARAM)0);
			SendDlgItemMessage(hWnd,IDC_SHOW_OPCODES,BM_SETCHECK,(WPARAM)BST_CHECKED,(LPARAM)0);
			SendDlgItemMessage(hWnd,IDC_SHOW_ASSEMBLY,BM_SETCHECK,(WPARAM)BST_CHECKED,(LPARAM)0);
			SendDlgItemMessage(hWnd,IDC_FORCE_DISASM,BM_SETCHECK,(WPARAM)BST_CHECKED,(LPARAM)0);
            SendDlgItemMessage(hWnd,IDC_FIRST_PASS,BM_SETCHECK,(WPARAM)BST_CHECKED,(LPARAM)0);
			SendDlgItemMessage(hWnd,IDC_SIGNATURES,BM_SETCHECK,(WPARAM)BST_CHECKED,(LPARAM)0);
            MaskEditControl(GetDlgItem(hWnd,IDC_BYTE_EP),"0123456789\b",TRUE);
            SendDlgItemMessage(hWnd,IDC_BYTE_EP, EM_SETLIMITTEXT,(WPARAM)2,0);

            // SETUP & ATTACH BUDDY
            SendDlgItemMessage(hWnd,IDC_INC_BYTES,UDM_SETRANGE,(WPARAM)0,(LPARAM)MAKELONG(55,0));
            SendDlgItemMessage(hWnd,IDC_INC_BYTES,UDM_SETPOS,(WPARAM)0,(LPARAM)1);
			SendDlgItemMessage((HWND)GetDlgItem(hWnd,IDC_BYTE_EP),IDC_BYTE_EP,UDM_SETBUDDY,(WPARAM)(HWND)GetDlgItem(hWnd,IDC_INC_BYTES),(LPARAM)0);
             
            if(LoadedPe==TRUE){
                disop.CPU=x86;        // default: decode x86 Cpu
                EnableWindow(GetDlgItem(hWnd,IDC_SET_CPU),FALSE);
            }
            else if(LoadedPe64==TRUE){
				disop.CPU=x87;
				EnableWindow(GetDlgItem(hWnd,IDC_SET_CPU),FALSE);
			}
			else if(LoadedNe==TRUE){
				disop.CPU=x86_16Bit;
				EnableWindow(GetDlgItem(hWnd,IDC_SET_CPU),FALSE);
			}
			else{
                EnableWindow(GetDlgItem(hWnd,IDC_ADVANCED),FALSE);
                EnableWindow(GetDlgItem(hWnd,IDC_FORCE_DISASM),FALSE);
                EnableWindow(GetDlgItem(hWnd,IDC_FORCE_EP),FALSE);
                EnableWindow(GetDlgItem(hWnd,IDC_BYTE_EP),FALSE);
			}

			if(LoadFirstPass==FALSE){
				EnableWindow(GetDlgItem(hWnd,IDC_FIRST_PASS),FALSE);
			}
			else{
				EnableWindow(GetDlgItem(hWnd,IDC_FIRST_PASS),TRUE);
			}
            
			SetFocus(GetDlgItem(hWnd,IDC_DISASM_FILE));
			UpdateWindow(hWnd);	// Update the window            
		}
		break;

		case WM_PAINT:{
			return false; //no paint
		}
		break;

		case WM_COMMAND:
		{
			switch(LOWORD(wParam)) // what we press on?
			{
                case IDC_ADVANCED:{
					DialogBox(GetModuleHandle(NULL),MAKEINTRESOURCE(IDD_ADVANCED_OPTIONS), hWnd, (DLGPROC)AdvancedOptionsDlgProc);
				}
                break;

				case IDC_CPUS:{
					switch(HIWORD(wParam)){
						case CBN_SELCHANGE:{
							EnableWindow(GetDlgItem(hWnd,IDC_SET_CPU),TRUE); 
						}
						break;
					}
				}
				break;

				case IDC_SET_CPU:{
					DWORD_PTR Index;
					Index=SendDlgItemMessage(hWnd,IDC_CPUS,CB_GETCURSEL,0,0);
					disop.CPU=SendDlgItemMessage(hWnd,IDC_CPUS,CB_GETITEMDATA,(WPARAM)Index,(LPARAM)0);

					EnableWindow(GetDlgItem(hWnd,IDC_SET_CPU),FALSE);
				}
				break;

				case IDC_DATA_CREATE_SEGMENTS:{
					DialogBox(GetModuleHandle(NULL),MAKEINTRESOURCE(IDD_DATA_SEGMENTS), hWnd, (DLGPROC)DataSegmentsDlgProc);
				}
				break;

				case IDC_FDATA_CREATE_EP:{
					DialogBox(GetModuleHandle(NULL),MAKEINTRESOURCE(IDD_FUNCTIONS_ENTRYPOINTS), hWnd, (DLGPROC)FunctionsEPSegmentsDlgProc);
				}
				break;

				case IDC_LOAD_MAP:{
					try{
						LoadMapFile(hWnd); // Load the map
					}
					catch(...){}
				}
				break;

				case IDC_DISASM_FILE:{	
					if(FilesInMemory==true /*&& LoadedPe==true*/){
						// Free Disasm data from memory
						DisasmDataLines.clear();
						DisasmDataArray(DisasmDataLines).swap(DisasmDataLines);

						// Free Imports data from memory
						ImportsLines.clear();
						DisasmImportsArray(ImportsLines).swap(ImportsLines);

						// Free String ref data from memory
						StrRefLines.clear();
						DisasmImportsArray(StrRefLines).swap(StrRefLines);

						// Free The Data
						DisasmCodeFlow.clear();
						CodeBranch(DisasmCodeFlow).swap(DisasmCodeFlow);

						// Clear Tracing List
						BranchTrace.clear();
						BranchData(BranchTrace).swap(BranchTrace);

						// Clear Xref List
						XrefData.clear();
						XRef(XrefData).swap(XrefData);

						// clear wizard data vars
						DataSection.clear();
						DataMapTree(DataSection).swap(DataSection);

						// clear wizard code information
						CodeSections.clear();
						CodeSectionArray(CodeSections).swap(CodeSections);

						// Clear Function Information
						///fFunctionInfo.clear();
						//FunctionInfo(fFunctionInfo).swap(fFunctionInfo);
					}

					SetDlgItemText(mainhWnd,IDC_MESSAGE1,"");
					SetDlgItemText(mainhWnd,IDC_MESSAGE3,"");

					// Get Position
					RangeFromEP=SendDlgItemMessage(hWnd,IDC_INC_BYTES,UDM_GETPOS,(WPARAM)0,(LPARAM)0); 
					// get user options from main disasm options dialog
					GetDisasmOptions(hWnd,&disop);
					// Show Disassembly window
					ShowWindow(GetDlgItem(mainhWnd,IDC_DISASM),SW_SHOW);
					// Start decoding using a thread worker

					// Enable Stop Disassembly menu item
					HMENU hMenu=GetMenu(mainhWnd);
					EnableMenuItem(hMenu,IDC_STOP_DISASM,MF_ENABLED);
					EnableMenuItem(hMenu,IDC_PAUSE_DISASM,MF_ENABLED);

					SendMessage(hWndTB, TB_ENABLEBUTTON, ID_EP_SHOW, (LPARAM)FALSE);
					SendMessage(hWndTB, TB_ENABLEBUTTON, ID_CODE_SHOW, (LPARAM)FALSE);
					SendMessage(hWndTB, TB_ENABLEBUTTON, ID_ADDR_SHOW, (LPARAM)FALSE);

					// Must Select a CPU Category.
					if(IsWindowEnabled(GetDlgItem(hWnd,IDC_SET_CPU))){
						MessageBox(hWnd,"Please Specify a Cpu!","Notice",MB_OK);
						return FALSE;
					}

					// What CPU has been selected
					switch(disop.CPU){
						case x86:{
							hDisasmThread=CreateThread(0,0,(LPTHREAD_START_ROUTINE)Disassembler,hWnd,0,&DisasmThreadId);
						}
						break;

						case x86_16Bit:{
							OutDebug(mainhWnd,"Disassembly of 16Bit NE Executable not supported.");
							/*hDisasmThread=CreateThread(0,0,(LPTHREAD_START_ROUTINE)Disassembler,hWnd,0,&DisasmThreadId);*/
						}
						break;

						case x87:{
							OutDebug(mainhWnd,"Disassembly of 64Bit Instructions is not supported.");
							/*hDisasmThread=CreateThread(0,0,(LPTHREAD_START_ROUTINE)Disassembler,hWnd,0,&DisasmThreadId);*/
						}
						break;

						case chip8:{
							hDisasmThread=CreateThread(0,0,(LPTHREAD_START_ROUTINE)Chip8,hWnd,0,&DisasmThreadId);
						}
						break;
					}
                    
					EndDialog(hWnd,0);	// Close Dialog
					SetFocus(GetDlgItem(mainhWnd,IDC_DISASM));	// seet focus to disasm listview
				}
				break;

				case IDC_FORCE_EP:{
					static bool Notice=false;
					if((SendDlgItemMessage(hWnd,IDC_FORCE_EP,BM_GETCHECK,(WPARAM)BST_CHECKED,(LPARAM)0))==BST_CHECKED){
						if(Notice==false){
							MessageBox(hWnd,"This Option will Disable Disassembly Before EntryPoint","Notice",MB_OK);
						}
						SetDlgItemText(hWnd,IDC_FORCE_DISASM,"[X] Force Disasm Before EP");
						Notice=true;
					}
					else{
						SetDlgItemText(hWnd,IDC_FORCE_DISASM,"Force Disasm Before EP");
					}
				}
				break;
                
				// User cancel Disasm Options
				case IDC_CANCEL:{
					OutDebug(mainhWnd,"Disassembly Option Dialog Cancled...");
					SelectLastItem(GetDlgItem(mainhWnd,IDC_LIST)); // Selects the Last Item
					EndDialog(hWnd,0);
				}
				break;
			}
		}
		break;

		case WM_CLOSE:{
			EndDialog(hWnd,0); // kill dialog
		}
		break;
	}
	return 0;
}

BOOL CALLBACK AdvancedOptionsDlgProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch(Message)
	{        
		case WM_PAINT:{
			return false;	// no paint
		}
		break;

		case WM_INITDIALOG:{
			ShowWindow(hWnd,SW_SHOWNORMAL);
		}
		break;
        
		case WM_COMMAND:
		{
            switch(LOWORD(wParam)){ // what we press on?
				// User cancel Disasm Options
				case IDC_ADVANCE_CANCEL:{
					EndDialog(hWnd,0); 
				}
				break;

				case IDC_SAVE_OPTIONS:{
					if((SendDlgItemMessage(hWnd,IDC_SHOW_DBG_IMP,BM_GETCHECK,(WPARAM)BST_CHECKED,(LPARAM)0))==BST_CHECKED){
						disop.ShowResolvedApis=1;
					}
					else{
						disop.ShowResolvedApis=0;
					}

					if((SendDlgItemMessage(hWnd,IDC_AUTO_DB,BM_GETCHECK,(WPARAM)BST_CHECKED,(LPARAM)0))==BST_CHECKED){
						disop.AutoDB=1;
					}
					else{
						disop.AutoDB=0;
					}

					EndDialog(hWnd,0);
				}
				break;
			}
		}
		break;
        
		case WM_CLOSE:{
			EndDialog(hWnd,0); // destroy dialog
		}
		break;
	}
	return 0;
}

BOOL CALLBACK FunctionsEPSegmentsDlgProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch(Message)
	{
		case WM_PAINT:{
			return false;	// no paint
		}
		break;

		case WM_NOTIFY:{
			switch(LOWORD(wParam)){
				case IDC_FDATA_LIST:{
					if(((LPNMHDR)lParam)->code == NM_DBLCLK){
						LV_ITEM LvItem;
						HWND hList = GetDlgItem(hWnd,IDC_FDATA_LIST);
						DWORD_PTR iItem=0;
						char Text[256];

						memset(&LvItem,0,sizeof(LvItem));
						iItem=SendMessage(hList,LVM_GETNEXTITEM,-1,LVNI_FOCUSED);
						if(iItem==-1){
							return false;
						}
						LvItem.mask=LVIF_TEXT;
						LvItem.iSubItem=0;
						LvItem.pszText=Text;
						LvItem.cchTextMax=256;
						LvItem.iItem=(DWORD)iItem;
						SendMessage(hList,LVM_GETITEMTEXT, iItem, (LPARAM)&LvItem);
						SetDlgItemText(hWnd,IDC_START_FADDR,LvItem.pszText);
						LvItem.iSubItem=1;
						SendMessage(hList,LVM_GETITEMTEXT, iItem, (LPARAM)&LvItem);
						SetDlgItemText(hWnd,IDC_END_FADDR,LvItem.pszText);
						LvItem.iSubItem=2;
						SendMessage(hList,LVM_GETITEMTEXT, iItem, (LPARAM)&LvItem);
						SetDlgItemText(hWnd,IDC_FUNCTION_NAME,LvItem.pszText);
					}
				}
				break;
			}
		}
		break;

		case WM_INITDIALOG:
		{
			LV_COLUMN LvCol;
			LVITEM  LvItem;
			HWND hList = GetDlgItem(hWnd,IDC_FDATA_LIST);
			DWORD_PTR data,i,j;
			char Text[128];

			UpdateWindow(hWnd);
			ShowWindow(hWnd,SW_SHOWNORMAL);
			SendMessage(hList,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT);
			memset(&LvCol,0,sizeof(LvCol));
			LvCol.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM;
			LvCol.pszText="Start Address";
			LvCol.cx=0x90;
			SendMessage(hList,LVM_INSERTCOLUMN,0,(LPARAM)&LvCol);
			LvCol.pszText="End Address";
			LvCol.cx=0x90;
			SendMessage(hList,LVM_INSERTCOLUMN,1,(LPARAM)&LvCol);
			LvCol.pszText="Function Name";
			LvCol.cx=0x90;
			SendMessage(hList,LVM_INSERTCOLUMN,2,(LPARAM)&LvCol);

			MaskEditControl(GetDlgItem(hWnd,IDC_START_FADDR),	"0123456789abcdefABCDEF\b",TRUE);
			MaskEditControl(GetDlgItem(hWnd,IDC_END_FADDR),		"0123456789abcdefABCDEF\b",TRUE);
			MaskEditControl(GetDlgItem(hWnd,IDC_FUNCTION_NAME),	"0123456789abcdefghijklmnopqrstuvwxyz_ABCDEFGHIJKLMNOPQRSTUVWXYZ\b",TRUE);
			SendDlgItemMessage(hWnd,IDC_START_FADDR,	EM_SETLIMITTEXT,	(WPARAM)8,0);
			SendDlgItemMessage(hWnd,IDC_END_FADDR,		EM_SETLIMITTEXT,	(WPARAM)8,0);
			SendDlgItemMessage(hWnd,IDC_FUNCTION_NAME,	EM_SETLIMITTEXT,	(WPARAM)50,0);

			memset(&LvItem,0,sizeof(LvItem));
			LvItem.cchTextMax = 256;
			
			for(j=0,i=0;i<fFunctionInfo.size();i++,j++){
				data = fFunctionInfo[i].FunctionStart;
				wsprintf(Text,"%08X",data);
				LvItem.mask=LVIF_TEXT|LVIF_PARAM|LVIF_STATE; // Styles
				LvItem.iItem=(DWORD)j;
				LvItem.iSubItem=0;
				LvItem.pszText = Text;
				LvItem.lParam = i; // Save Position
				SendMessage(hList,LVM_INSERTITEM,0,(LPARAM)&LvItem);
				LvItem.mask=LVIF_TEXT;
				data = fFunctionInfo[i].FunctionEnd;
				wsprintf(Text,"%08X",data);
				LvItem.iItem=(DWORD)j;
				LvItem.iSubItem=1;
				LvItem.pszText=Text;
				LvItem.cchTextMax = 256;
				SendMessage(hList,LVM_SETITEM,0,(LPARAM)&LvItem);
				LvItem.iItem=(DWORD)j;
				LvItem.iSubItem=2;
				LvItem.cchTextMax = 256;
				strcpy_s(LvItem.pszText,StringLen(fFunctionInfo[i].FunctionName)+1,fFunctionInfo[i].FunctionName);
				SendMessage(hList,LVM_SETITEM,0,(LPARAM)&LvItem);
			}
		}
		break;

		case WM_COMMAND:
		{
			switch(LOWORD(wParam)){ // what we press on?
				case IDC_ADD_NEW_F:{
					FUNCTION_INFORMATION fFunc;
					LVITEM  LvItem;
					HWND hList = GetDlgItem(hWnd,IDC_FDATA_LIST);
					DWORD dwStart=0,dwEnd=0;
					bool Found=FALSE;
					char cStart[10]="",cEnd[10]="";
					char FunctionName[50]="";

					GetDlgItemText(hWnd,IDC_START_FADDR,cStart,(LPARAM)256);
					GetDlgItemText(hWnd,IDC_END_FADDR,cEnd,(LPARAM)256);
					GetDlgItemText(hWnd,IDC_FUNCTION_NAME,FunctionName,(LPARAM)256);
					
					if(StringLen(cStart)!=8 || StringLen(cEnd)!=8){
						MessageBox(hWnd,"Start or End Address has invalid length!","Notice",MB_OK);
						return false;
					}

					dwStart = StringToDword(cStart);
					dwEnd = StringToDword(cEnd);

					if(dwEnd<=dwStart){
						MessageBox(hWnd,"End Address can't be <= to Start Address","Notice",MB_OK);
						return false;
					}

					for(DWORD_PTR i=0;i<fFunctionInfo.size();i++){
						if(fFunctionInfo[i].FunctionStart==dwStart){
							Found=TRUE;
							break;
						}
					}

					if(Found==FALSE){
						// Key is not found in the list, add new
						fFunc.FunctionStart=dwStart;
						fFunc.FunctionEnd=dwEnd;
						strcpy_s(fFunc.FunctionName,StringLen(FunctionName)+1,FunctionName);
						AddFunction(fFunc);
					}
					else{
						return false;	// Found key, Don't add
					}

					memset(&LvItem,0,sizeof(LvItem));
					//LvItem.cchTextMax = 256;
					int index=fFunctionInfo.size();
					if(index!=0){
						index--;
					}
					LvItem.mask=LVIF_TEXT|LVIF_PARAM|LVIF_STATE; // Styles;
					LvItem.iItem=index;
					LvItem.iSubItem=0;
					LvItem.pszText = cStart;
					LvItem.lParam = index; // Save Position
					SendMessage(hList,LVM_INSERTITEM,0,(LPARAM)&LvItem);
					LvItem.mask=LVIF_TEXT;
					LvItem.iItem=index;
					LvItem.iSubItem=1;
					LvItem.pszText=cEnd;
					SendMessage(hList,LVM_SETITEM,0,(LPARAM)&LvItem);
					LvItem.mask=LVIF_TEXT;
					LvItem.iItem=index;
					LvItem.iSubItem=2;
					LvItem.pszText=FunctionName;
					SendMessage(hList,LVM_SETITEM,0,(LPARAM)&LvItem);
					SelectItem(hList,index);
				}
				break;

				case IDC_GOTO_ENTRY_F:{
					LVITEM  LvItem;
					DWORD_PTR iSelected=0;
					HWND hList = GetDlgItem(hWnd,IDC_FDATA_LIST);
					HWND hDasm = GetDlgItem(mainhWnd,IDC_DISASM);
					char Text[256]="";

					iSelected=SendMessage(hList,LVM_GETNEXTITEM,-1,LVNI_FOCUSED);
					if(iSelected==-1){
						MessageBox(hWnd,"No Address Selected","Notice",MB_OK);
						return false;
					}

					memset(&LvItem,0,sizeof(LvItem));
					LvItem.mask=LVIF_TEXT|LVIF_PARAM;
					LvItem.iSubItem=0;
					LvItem.pszText=Text;
					LvItem.cchTextMax=256;
					LvItem.iItem=(DWORD)iSelected;

					SendMessage(hList,LVM_GETITEMTEXT, iSelected, (LPARAM)&LvItem);
					iSelected = SearchItemText(hDasm,Text); // search item
					if(iSelected!=-1){ // Found index
						SelectItem(hDasm,iSelected); // select and scroll to item
						SetFocus(hDasm);
					}
				}
				break;
				
				case IDC_UPDATE_FDATA:{
					LVITEM  LvItem;
					DWORD_PTR iSelected=0;
					HWND hList = GetDlgItem(hWnd,IDC_FDATA_LIST);
					HWND hDasm = GetDlgItem(mainhWnd,IDC_DISASM);
					DWORD dwData;
					char Data[50],Text[128];
					iSelected=SendMessage(hList,LVM_GETNEXTITEM,-1,LVNI_FOCUSED);
					if(iSelected==-1){
					MessageBox(hWnd,"Nothing to Delete...","Notice",MB_OK);
						return false;
					}

					memset(&LvItem,0,sizeof(LvItem));
					GetWindowText(GetDlgItem(hWnd,IDC_START_FADDR),Data,10);
					GetWindowText(GetDlgItem(hWnd,IDC_END_FADDR),Text,10);

					if( StringLen(Data)!=8 || StringLen(Text)!=8 ){
						MessageBox(hWnd,"Start or End Address has invalid length!","Notice",MB_OK);
						return false;
					}
					
					LvItem.mask=LVIF_TEXT;
					LvItem.iItem=(DWORD)iSelected;
					LvItem.iSubItem=0;
					LvItem.pszText=Data;
					SendMessage(hList,LVM_SETITEM,0,(LPARAM)&LvItem);
					dwData=StringToDword(Data);
					fFunctionInfo[iSelected].FunctionStart=dwData;
					//GetWindowText(GetDlgItem(hWnd,IDC_END_FADDR),Data,10);
					LvItem.mask=LVIF_TEXT;
					LvItem.iItem=(DWORD)iSelected;
					LvItem.iSubItem=1;
					LvItem.pszText=Text;
					SendMessage(hList,LVM_SETITEM,0,(LPARAM)&LvItem);
					dwData=StringToDword(Text);
					fFunctionInfo[iSelected].FunctionEnd=dwData;
					GetWindowText(GetDlgItem(hWnd,IDC_FUNCTION_NAME),Data,50);
					LvItem.mask=LVIF_TEXT;
					LvItem.iItem=(DWORD)iSelected;
					LvItem.iSubItem=2;
					LvItem.pszText=Data;
					SendMessage(hList,LVM_SETITEM,0,(LPARAM)&LvItem);
					lstrcpy(fFunctionInfo[iSelected].FunctionName,Data);

					// Update Disasm Data
					memset(&LvItem,0,sizeof(LvItem));
					LvItem.mask=LVIF_TEXT|LVIF_PARAM;
					LvItem.iSubItem=0;
					LvItem.pszText=Text;
					LvItem.cchTextMax=256;
					LvItem.iItem=(DWORD)iSelected;
					SendMessage(hList,LVM_GETITEMTEXT, iSelected, (LPARAM)&LvItem);
					iSelected = SearchItemText(hDasm,Text); // search item
					
					// Update live the proc entry name on the disasm window
					if(iSelected!=-1){ // found index
						if(StringLen(Data)!=0){
							wsprintf(Text,"; ====== %s Proc ======",Data);
							DisasmDataLines[iSelected-1].SetMnemonic(Text);
							RedrawWindow(hDasm,NULL,NULL,TRUE);
						}
					}

					SetFocus(hList);
					SelectItem(hList,iSelected);
				}
				break;

				case IDC_DELETE_FDATA:{
					LVITEM  LvItem;
					DWORD_PTR iSelected=0;
					HWND hList = GetDlgItem(hWnd,IDC_FDATA_LIST);
					DWORD Data;
					bool Found=FALSE;
					char Text[256]="";

					iSelected=SendMessage(hList,LVM_GETNEXTITEM,-1,LVNI_FOCUSED);
					if(iSelected==-1){
						MessageBox(hWnd,"Nothing to Delete...","Notice",MB_OK);
						return false;
					}
                    
					memset(&LvItem,0,sizeof(LvItem));
					LvItem.mask=LVIF_TEXT|LVIF_PARAM;
					LvItem.iSubItem=0;
					LvItem.pszText=Text;
					LvItem.cchTextMax=256;
					LvItem.iItem=(DWORD)iSelected;

					SendMessage(hList,LVM_GETITEMTEXT, iSelected, (LPARAM)&LvItem);
					Data = StringToDword(Text);
					SendMessage(hList,LVM_DELETEITEM,(WPARAM)iSelected,0);
                    
					FunctionInfoItr itr=fFunctionInfo.begin();
					for(;itr!=fFunctionInfo.end();itr++){
						if( (*itr).FunctionStart==Data ){
							Found=TRUE;
							break;
						}
					}
					
					if(Found==TRUE){
						fFunctionInfo.erase(itr);
					}
				}
				break;

				case IDC_EXIT_FDATA:{
					SendMessage(hWnd,WM_CLOSE,0,0);
				}
				break;
			}
		}
		break;

		case WM_CLOSE:{
			EndDialog(hWnd,0); // destroy dialog
		}
		break;
	}

	return 0;
}

BOOL CALLBACK DataSegmentsDlgProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	static int Index=0;

	switch(Message)
	{
		case WM_PAINT:{
			return false;	// no paint
		}
		break;

		case WM_NOTIFY:{
			switch(LOWORD(wParam)){
				case IDC_DATA_LIST:{
					if(((LPNMHDR)lParam)->code == NM_DBLCLK){
						LV_ITEM LvItem;
						HWND hList = GetDlgItem(hWnd,IDC_DATA_LIST);
						DWORD_PTR iItem=0;
						char Text[10];

						memset(&LvItem,0,sizeof(LvItem));
						iItem=SendMessage(hList,LVM_GETNEXTITEM,-1,LVNI_FOCUSED);
						if(iItem==-1){
							return false;
						}
						LvItem.mask=LVIF_TEXT;
						LvItem.iSubItem=0;
						LvItem.pszText=Text;
						LvItem.cchTextMax=256;
						LvItem.iItem=(DWORD)iItem;   
						SendMessage(hList,LVM_GETITEMTEXT, iItem, (LPARAM)&LvItem);
						SetDlgItemText(hWnd,IDC_START_ADDR,LvItem.pszText);
						LvItem.iSubItem=1;
						SendMessage(hList,LVM_GETITEMTEXT, iItem, (LPARAM)&LvItem);
						SetDlgItemText(hWnd,IDC_END_ADDR,LvItem.pszText);
					}
				}
				break;
			}
		}
		break;

		case WM_INITDIALOG:{
			LV_COLUMN LvCol;
			LVITEM  LvItem;
			HWND hList = GetDlgItem(hWnd,IDC_DATA_LIST);
			DWORD data,i=0;
			char Text[9];
			
			ShowWindow(hWnd,SW_SHOWNORMAL);
			Index=0; // Reinitialize Index Counter on dialog init
			SendMessage(hList,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT);
			memset(&LvCol,0,sizeof(LvCol));
			LvCol.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM;
			LvCol.pszText="Start Address";
			LvCol.cx=0x90;
			SendMessage(hList,LVM_INSERTCOLUMN,0,(LPARAM)&LvCol);
			LvCol.pszText="End Address";
			LvCol.cx=0x90;
			SendMessage(hList,LVM_INSERTCOLUMN,1,(LPARAM)&LvCol);

			MaskEditControl(GetDlgItem(hWnd,IDC_START_ADDR), "0123456789abcdefABCDEF\b",TRUE);
			MaskEditControl(GetDlgItem(hWnd,IDC_END_ADDR),   "0123456789abcdefABCDEF\b",TRUE);
			SendDlgItemMessage(hWnd,IDC_START_ADDR,     EM_SETLIMITTEXT,	(WPARAM)8,0);
			SendDlgItemMessage(hWnd,IDC_END_ADDR,       EM_SETLIMITTEXT,	(WPARAM)8,0);

			memset(&LvItem,0,sizeof(LvItem));
			LvItem.cchTextMax = 256;
            
			for(TreeMapItr itr=DataAddersses.begin();itr!=DataAddersses.end();itr++,i++,Index++){
				data = (*itr).first;
				wsprintf(Text,"%08X",data);
				LvItem.mask=LVIF_TEXT|LVIF_PARAM|LVIF_STATE; // Styles
				LvItem.iItem=i;
				LvItem.iSubItem=0;
				LvItem.pszText = Text;
				LvItem.lParam = i; // Save Position
				SendMessage(hList,LVM_INSERTITEM,0,(LPARAM)&LvItem);
				LvItem.mask=LVIF_TEXT;
				data = (*itr).second;
				wsprintf(Text,"%08X",data);
				LvItem.iItem=i;
				LvItem.iSubItem=1;
				LvItem.pszText=Text;
				SendMessage(hList,LVM_SETITEM,0,(LPARAM)&LvItem);
			}
		}
		break;
        
		case WM_COMMAND:{
			switch(LOWORD(wParam)) // What did we press on?
			{
				case IDC_ADD_NEW:{
					LVITEM  LvItem;
					HWND hList = GetDlgItem(hWnd,IDC_DATA_LIST);
					DWORD dwStart=0,dwEnd=0;
					char cStart[10],cEnd[10];

					GetDlgItemText(hWnd,IDC_START_ADDR,cStart,(LPARAM)256);
					GetDlgItemText(hWnd,IDC_END_ADDR,cEnd,(LPARAM)256);

					if(StringLen(cStart)!=8 || StringLen(cEnd)!=8){
						return false;
					}

					dwStart = StringToDword(cStart);
					dwEnd = StringToDword(cEnd);

					if(dwEnd<dwStart){
						MessageBox(hWnd,"End Address can't be < to Start Address","Notice",MB_OK);
						return false;
					}

					TreeMapItr itr=DataAddersses.find(dwStart);
					if(itr==DataAddersses.end()){
						// Key is not found in the list, add new
						AddNewData(dwStart,dwEnd);
					}
					else{
						return false; // Found key, Don't add
					}

					memset(&LvItem,0,sizeof(LvItem));
					LvItem.cchTextMax = 256;
					LvItem.mask=LVIF_TEXT|LVIF_PARAM|LVIF_STATE; // Styles
					LvItem.iItem=Index;
					LvItem.iSubItem=0;
					LvItem.pszText = cStart;
					LvItem.lParam = Index; // Save Position
					SendMessage(hList,LVM_INSERTITEM,0,(LPARAM)&LvItem);
					LvItem.mask=LVIF_TEXT;
					LvItem.iSubItem=1;
					LvItem.pszText=cEnd;
					SendMessage(hList,LVM_SETITEM,0,(LPARAM)&LvItem);
					SelectItem(hList,Index);
					Index++;
				}
				break;

				case IDC_DELETE_DATA:{
					DWORD_PTR iSelected=0;
					HWND hList = GetDlgItem(hWnd,IDC_DATA_LIST);
					LVITEM  LvItem;
					DWORD Data;
					char Text[256]="";

					iSelected=SendMessage(hList,LVM_GETNEXTITEM,-1,LVNI_FOCUSED);
					if(iSelected==-1){
						MessageBox(hWnd,"Nothing to Delete...","Notice",MB_OK);
						return false;
					}

					memset(&LvItem,0,sizeof(LvItem));
					LvItem.mask=LVIF_TEXT|LVIF_PARAM;
					LvItem.iSubItem=0;
					LvItem.pszText=Text;
					LvItem.cchTextMax=256;
					LvItem.iItem=(DWORD)iSelected;
					SendMessage(hList,LVM_GETITEMTEXT, iSelected, (LPARAM)&LvItem);
					Data = StringToDword(Text);
					SendMessage(hList,LVM_DELETEITEM,(WPARAM)iSelected,0);
					DataAddersses.erase(Data);
					Index--;
				}
				break;

				case IDC_EXIT_DATA:{
					SendMessage(hWnd,WM_CLOSE,0,0);
				}
				break;
			}
		}
		break;
        
		case WM_CLOSE:{
			EndDialog(hWnd,0); // destroy dialog
		}
		break;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////
void IntializeDisasm(bool PeLoaded,bool PeLoaded64,bool NeLoaded,HWND myhWnd,char *File,char *FileName)
{
	// First Time intializations, check PE version (32Bit/64Bit)
	if(PeLoaded==TRUE){
		PE_File=PeLoaded;
	}
	else{
		if(PeLoaded64==TRUE){
			PE_File=PeLoaded64;
		}
		else{
			if(NeLoaded==TRUE){
				PE_File=NeLoaded;
			}
		}
	}
	mainhWnd=myhWnd;
	DisasmPtr=File;
	LoadedFileName=FileName;
}

void LoadMapFile(HWND hWnd)
{
	FUNCTION_INFORMATION fFunc;
	OPENFILENAME ofn;
	DWORD hFileSize,temp,i,dwStart,dwEnd;
	HANDLE hFile;
	char szFileName[128]="",*FileDataPtr,*ptr,dWord[8],dWord2[8],FuncName[128];

	// Intialize struct
	ZeroMemory(&ofn, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME); // Set Size of Struct
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = "Map Files (*.map)\0*.map\0";
	ofn.lpstrFile = szFileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
	ofn.lpstrDefExt = "map";
	
	// we selected a file ?
	if(GetOpenFileName(&ofn)!=0){
		hFile=CreateFile(szFileName,
		GENERIC_READ|GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL );

		if(hFile==INVALID_HANDLE_VALUE){
           return;
		}
		hFileSize=GetFileSize(hFile,NULL);
		if(hFileSize==0xFFFFFFFF){
			return;
		}
		FileDataPtr=new char[hFileSize];
		ReadFile(hFile,(char*)FileDataPtr,hFileSize,&temp,NULL);
		ptr=strstr(FileDataPtr,"[FUNCTIONS]");

		if(ptr!=NULL){
			ptr+=strlen("[FUNCTIONS]")+2;

			while(*ptr!='T'){
				FuncName[0]=NULL;
				i=0;
				while(*ptr!=':'){
					dWord[i]=*ptr;
					ptr++;
					i++;
				}   
             
				dwStart=StringToDword(dWord);
				ptr++;
             
				i=0;
				while(*ptr!=':'){
					dWord2[i]=*ptr;
					ptr++;
					i++;
				}
				ptr++;

				dwEnd=StringToDword(dWord2);

				i=0;
				while(*ptr!=0x0D){
					FuncName[i]=*ptr;
					ptr++;
					i++;
				}
				FuncName[i]=NULL;

				if(StringLen(FuncName)>50){
					FuncName[50]=NULL;
				}

				fFunc.FunctionStart=dwStart;
				fFunc.FunctionEnd=dwEnd;
				strcpy_s(fFunc.FunctionName,StringLen(FuncName)+1,FuncName);
				AddFunction(fFunc);
				ptr+=2;
			}
		}

		ptr=strstr(FileDataPtr,"[DATA]");

		if(ptr!=NULL){
			ptr+=strlen("[DATA]")+2;

			while(*ptr!='T'){
				i=0;
				while(*ptr!=':'){
					dWord[i]=*ptr;
					ptr++;
					i++;
				}
             
				dwStart=StringToDword(dWord);
				ptr++;
				i=0;
				while(*ptr!=0x0D){
					dWord2[i]=*ptr;
					ptr++;
					i++;
				}

				dwEnd=StringToDword(dWord2);

				TreeMapItr itr=DataAddersses.find(dwStart);
				if(itr!=DataAddersses.end()) {
					if((*itr).first==dwStart && (*itr).second==dwEnd){
						ptr+=2;
						continue;
					}
				}

				AddNewData(dwStart,dwEnd);
				ptr+=2;
			}
		}

		delete FileDataPtr;
		CloseHandle(hFile);
		SendDlgItemMessage(hWnd,IDC_FIRST_PASS,BM_SETCHECK,(WPARAM)BST_UNCHECKED,(LPARAM)0);
	}
}


//=================================================================
//================== DISASSEMBLY ENGINE ===========================
//=================================================================

void Decode(DISASSEMBLY *Disasm,char *Opcode,DWORD_PTR *Index)
{
	/*
		This function is the Main decoding rutine.
		The function gets 3 params:
		1. DISASSEMBLY struct pointer
		2. Opcode pointer, point to the linear address to decode
		3. Index pointer, this is the distance from the beginning<>end of the linear

		The function First searches for Prefixes + Repeated prefixes,
		This is the first step to do in any disasm engine.
		Prefixes determine behavior of instruction and the way they
		Are decoded.
		Once prefixes has been found, we changes params (such as default r/m size, Lock..)

		The function than searched for the byte to be decoded, the actual
		Mnemonic referenced in CPU form (Opcode),
		I have not used big table - time prob -, although it is highly recommended! (hopefully in future)
	*/

	// initializations
    DWORD_PTR dwMem=0,dwOp=0;
    DWORD_PTR	i=*Index,RegRepeat=0,LockRepeat=0,SegRepeat=0,RepRepeat=0,AddrRepeat=0; // Repeated Prefixes
	int   RM=REG32,SEG=SEG_DS,ADDRM=REG32;	// default modes	
	int   PrefixesSize=0,PrefixesRSize=0;	// PrefixesSize = all Prefixes(no rep), PrefixesRsize (with Rep Prefix)
	WORD  wMem=0,wOp=0;
    bool  RegPrefix=0,LockPrefix=0,SegPrefix=0,RepPrefix=0,AddrPrefix=0;  // default size of Prefixes
    BYTE  Bit_D=0, Bit_W=0;		// bit d/w for R/M
	BYTE  Op=(BYTE)Opcode[i];	// current opcode
    char  menemonic[256]="";
    char  RSize[10]="Dword";	// default size of menemonic

    //======================================================//
    //					PERFIX DECODER						//
    //======================================================//
    // we first assume there are prefixes!
	// if we skip this, our decoding might be currupted.

	while( // check only RegPreifix/LockProfix/SegPrefixes/RepPrefix/AddrPerfix
		   (Op==0x66) || (Op==0x0F0)|| (Op==0x2E) || (Op==0x36) ||
		   (Op==0x3E) || (Op==0x26) || (Op==0x64) || (Op==0x65) ||
		   (Op==0xF2) || (Op==0xF3) || (Op==0x67)
		 ) 
	{
		switch(Op){
			case 0x66:{ // reg prefix, change default size, dword->word
				RM=REG16; // 66 prefix, change default size
				RegPrefix=1; 
				BYTE temp;
				wsprintf(RSize,"%s",RegSize[2]); // change default size of menemonic to 'Word'
				//LockPrefix=0;
				lstrcat(Disasm->Opcode,"66:");
				i++;
				++(*Index);
				Op=(BYTE)Opcode[i];
				temp=(BYTE)Opcode[i+1];
				RegRepeat++;
				if(RegRepeat>1){
					strcpy_s(Disasm->Opcode,"66:");
					strcpy_s(Disasm->Remarks,";Prefix DataSize:");
					Disasm->OpcodeSize=1;
					Disasm->PrefixSize=0;
					(*Index)-=RegRepeat;
					return;
				}
			}
			break;

			case 0x67:{ // Addr prefix, change default Reg size, (EDI->DI) and more!
				ADDRM=REG16; // 67 prefix, change default size, in this case: Memory Reg Size
				AddrPrefix=1; 
				BYTE temp;
				lstrcat(Disasm->Opcode,"67:");
				i++;
				++(*Index);
				Op=(BYTE)Opcode[i];
				temp=(BYTE)Opcode[i+1];
				AddrRepeat++;
				if(AddrRepeat>1){
					strcpy_s(Disasm->Opcode,"67:");
					strcpy_s(Disasm->Remarks,";Prefix AddrSize:");
					Disasm->OpcodeSize=1;
					Disasm->PrefixSize=0;
					(*Index)-=AddrRepeat;
					return;
				}
			}
			break;

			case 0x0F0:{ // LockPrefix, Add bus lock menemonic opcode in front of every menemonic
				//BYTE temp;
				LockPrefix=1;
				//RegPrefix=0; 
				lstrcat(Disasm->Opcode,"F0:");
				strcpy_s(Disasm->Assembly,"lock ");
				i++;
				++(*Index);
				Op=(BYTE)Opcode[i];
				//temp=(BYTE)Opcode[i+1];
				LockRepeat++;
				if(LockRepeat>1){
					strcpy_s(Disasm->Assembly,"");
					strcpy_s(Disasm->Opcode,"F0:");
					strcpy_s(Disasm->Remarks,";Prefix LOCK:");
					Disasm->OpcodeSize=1;
					Disasm->PrefixSize=0;
					(*Index)-=LockRepeat;
					return;
				}
			}
			break;

			case 0xF2: case 0xF3:{ // RepPrefix (only string instruction!!)
                BYTE NextOp=(BYTE)Opcode[i+1];	// Next followed opcode
                BYTE NextOp2=(BYTE)Opcode[i+2];
				wsprintf(menemonic,"%02X:",Op);
				lstrcat(Disasm->Opcode,menemonic);
				if(!(NextOp==0x0F && NextOp2==0xD6)){	//	[0xF3/0xF2]0FD6xxxx doesn't have repne/repe
					RepPrefix=1;
					switch(Op){
						case 0xF2:wsprintf(menemonic,"repne ");break;
						case 0xF3:wsprintf(menemonic,"repe ");break;
					}
					lstrcat(Disasm->Assembly,menemonic);
				}
				i++;
				++(*Index);
				Op=(BYTE)Opcode[i];
				RepRepeat++;

				// REPE/REPNE Prefixes affect only string operations:
				// MOVS/LODS/SCAS/CMPS/STOS/CMPSS.CMPPS..etc (NewSet of Instructions)
				if(!( 
                      (Op>=0xA4 && Op<=0xA7) ||
                      (Op>=0xAA && Op<=0xAF) ||
                      (NextOp==0x0F && NextOp2==0x2A) ||
                      (NextOp==0x0F && NextOp2==0x10) ||
                      (NextOp==0x0F && NextOp2==0x11) ||
                      (NextOp==0x0F && NextOp2==0x2C) ||
                      (NextOp==0x0F && NextOp2==0x2D) ||
                      (NextOp==0x0F && NextOp2==0x51) ||
                      (NextOp==0x0F && NextOp2==0x52) ||
                      (NextOp==0x0F && NextOp2==0x53) ||
                      (NextOp==0x0F && NextOp2==0x58) ||
                      (NextOp==0x0F && NextOp2==0x59) ||
                      (NextOp==0x0F && NextOp2==0x5C) ||
                      (NextOp==0x0F && NextOp2==0x5D) ||
                      (NextOp==0x0F && NextOp2==0x5E) ||
                      (NextOp==0x0F && NextOp2==0x5F) ||
                      (NextOp==0x0F && NextOp2==0xC2) ||
					  (NextOp==0x0F && NextOp2==0xD6)
                    )
                  )
				{
					strcpy_s(Disasm->Assembly,"");
					strcpy_s(Disasm->Remarks,";Prefix REP:");
					Disasm->OpcodeSize=1;
					Disasm->PrefixSize=0;
					(*Index)-=RepRepeat;
					return;
				}else{ // Double check (especially with MMX)
					Disasm->PrefixSize=1;
				}
			}
			break;

			case 0x2E: case 0x36: // Segment Prefixes
			case 0x3E: case 0x26: // Segment Prefixes
			case 0x64: case 0x65: // Segment Prefixes
			{
				BYTE temp;
				switch(Op){
					// Change Default Segment
					case 0x2E: SEG = SEG_CS; break; // Segment CS
					case 0x36: SEG = SEG_SS; break; // Segment SS
					case 0x3E: SEG = SEG_DS; break; // Segment DS
					case 0x26: SEG = SEG_ES; break; // Segment ES
					case 0x64: SEG = SEG_FS; break; // Segment FS
					case 0x65: SEG = SEG_GS; break; // Segment GS
				}

				SegPrefix=1;
				wsprintf(menemonic,"%02X:",Op);
				lstrcat(Disasm->Opcode,menemonic);
				i++;
				++(*Index);
				Op=(BYTE)Opcode[i];
				temp=(BYTE)Opcode[i-2];
				SegRepeat++;

				// Check if SegPrefix is repeating
				if(SegRepeat>1){
					BYTE opc=(BYTE)Opcode[i-1];
					if(  temp==0x2E || temp==0x36 ||
					     temp==0x3E || temp==0x26 ||
                         temp==0x64 || temp==0x65 ||
                         temp==0x66 || temp==0xF0 ||
                         temp==0x67
					  )
					{
						// Check if last byte was an seg prefix and show it
						if(temp==0x66 || temp==0xF0 || temp==0x67){
                           opc=(BYTE)Opcode[i-3];
                           SegRepeat++;
                        }
						else{
                           opc=(BYTE)Opcode[i-2];
						}

						switch(opc){
							// Change Segment, accurding to last segPrefix (if repeated)
							case 0x2E: SEG = SEG_CS; break; // Segment CS
							case 0x36: SEG = SEG_SS; break; // Segment SS
							case 0x3E: SEG = SEG_DS; break; // Segment DS
							case 0x26: SEG = SEG_ES; break; // Segment ES
							case 0x64: SEG = SEG_FS; break; // Segment FS
							case 0x65: SEG = SEG_GS; break; // Segment GS
						}
					
                        strcpy_s(Disasm->Assembly,"");
                        wsprintf(menemonic,"%02X:",opc);
                        strcpy_s(Disasm->Opcode,menemonic);
                        wsprintf(menemonic,"Prefix %s:",Segs[SEG]);
                        strcpy_s(Disasm->Remarks,StringLen(menemonic)+1,menemonic);
                        Disasm->OpcodeSize=0;
                        Disasm->PrefixSize=1;
                        (*Index)-=SegRepeat;
                    }
					return;
				}
			}
			break;
			
		    default:{
				// reset prefixes/repeats to default
				LockRepeat=0;
				RegRepeat=0;
				SegRepeat=0;
				RegPrefix=0;
				LockPrefix=0;
				SegPrefix=0;
				strcpy_s(RSize,StringLen(RegSize[1])+1,RegSize[1]); // Default size
			}
			break;
		}
	}
    
	//==========================================//
	//				DECODER CORE				//
	//==========================================//
	// Calculate Prefixes Sizes
	PrefixesSize  = RegPrefix+LockPrefix+SegPrefix+AddrPrefix; // No RepPrefix
	PrefixesRSize = PrefixesSize+RepPrefix; // Special Case (Rep Prefix is being used -> String menemonics only)

    switch(Op){ // Find & Decode Big Set Opcodes
		case 0x00: case 0x01: case 0x02: case 0x03: // ADD  XX/XXX, XX/XXX
		case 0x08: case 0x09: case 0x0A: case 0x0B: // OR   XX/XXX, XX/XXX
		case 0x10: case 0x11: case 0x12: case 0x13: // ADC  XX/XXX, XX/XXX
		case 0x18: case 0x19: case 0x1A: case 0x1B: // SBB  XX/XXX, XX/XXX
		case 0x20: case 0x21: case 0x22: case 0x23: // AND  XX/XXX, XX/XXX
		case 0x28: case 0x29: case 0x2A: case 0x2B: // SUB  XX/XXX, XX/XXX
		case 0x30: case 0x31: case 0x32: case 0x33: // XOR  XX/XXX, XX/XXX
		case 0x38: case 0x39: case 0x3A: case 0x3B: // CMP  XX/XXX, XX/XXX
        case 0x88: case 0x89: case 0x8A: case 0x8B: // MOV  XX/XXX, XX/XXX
        case 0x8C: case 0x8E:						// MOV  XX/XXX, XX/XXX
		case 0x62: case 0x63:						// BOUND / ARPL XX/XXX, XX/XXX
        case 0x69:									// IMUL RM,IIM32 (DWORD)
        case 0x6B:									// IMUL <reg>,<RM>
		case 0x80: case 0x81: case 0x82: case 0x83: // MIXED Instructions
		case 0x84: case 0x85:						// TEST
		case 0x86: case 0x87:						// XCHG
        case 0x8D:									// LEA 
        case 0x8F:									// POP
        case 0xC0: case 0xC1:						// MIXED Instructions
        case 0xC4: case 0xC5:						// LES / LDS REG,MEM
        case 0xC6: case 0xC7:						// MOV [MEM],IIM8/16/32
        case 0xD0: case 0xD1: case 0xD2: case 0xD3: // MIXED Bitwise Instructions
        case 0xD8: case 0xD9: case 0xDA: case 0xDB: // FPU Instructions
        case 0xDC: case 0xDD: case 0xDE: case 0xDF: // FPU Instructions
        case 0xF6: case 0xF7: case 0xFE: case 0xFF: // MIX Instructions
        {
			char mene[10]="";
			if(((BYTE)Opcode[i+1] & 0xC0)==0xC0){ // Check Opcode Range
				GetInstruction(Op,mene);	// Get instruction from Opcode Byte
				Bit_D=(Op&0x02)>>1;			// Get bit d (direction)
				Bit_W=(Op&0x01);			// Get bit w (full/partial reg size)
				
				// Check Special Cases for alone Opcodes
				switch(Op){
                    case 0x63:{ Bit_D=0;Bit_W=1; }				break;
                    case 0x62:{ Bit_D=1;Bit_W=1; }				break;
                    case 0x86:{ Bit_D=0;Bit_W=0; }				break;
                    case 0x87:{ Bit_D=0;Bit_W=1; }				break;
                    case 0x80: case 0x82: { Bit_D=0;Bit_W=0; }	break;
                    case 0x81: case 0x83: { Bit_D=0;Bit_W=1; }	break;
                    case 0x8C:{ Bit_D=0;Bit_W=0;}				break;
                    case 0x8E:{ Bit_D=1;Bit_W=0;}				break;
                    case 0xC4: case 0xC5: { Bit_D=1;Bit_W=1; }	break;
				}

				Mod_11_RM(Bit_D,Bit_W,&Opcode,&Disasm,mene,RegPrefix,Op,&Index); // Decode with bits
				Disasm->PrefixSize=PrefixesSize; // PrefixSize (if prefix present)
				break;
			}
			
		 // operand doesn't have byte(s) extension in addressing mode
		 //	if((BYTE)Opcode[i+1]>=0x00 && (BYTE)Opcode[i+1]<=0xBF)
		 //	{
		 //     char mene[10]="";
				GetInstruction(Op,mene);	// Get instruction from Opcode Byte
				Bit_D=(Op&0x02)>>1;			// Get bit d (direction)
				Bit_W=(Op&0x01);			// Get bit w (full/partial reg size)
				Mod_RM_SIB(&Disasm,&Opcode,i,AddrPrefix,SEG,&Index,Bit_D,Bit_W,mene,Op,RegPrefix,SegPrefix,AddrPrefix);
				Disasm->PrefixSize=PrefixesSize;
		 //		break;
		 //	}
		}
		break;

		case 0x04:case 0x0C:case 0x14: // INSTRUCTION AL,XX
		case 0x1C:case 0x24:case 0x2C: // INSTRUCTION AL,XX
        case 0x34:case 0x3C:case 0xA8: // INSTRUCTION AL,XX
		case 0xE4:
        {
			char mene[10]="";
			GetInstruction(Op,mene); // Get instruction for a specified Byte
            wsprintf(menemonic,"%s al, %02Xh",mene,(BYTE)Opcode[i+1]);
            lstrcat(Disasm->Assembly,menemonic);
            strcpy_s(Disasm->Remarks,"");
            wsprintf(menemonic,"%02X%02X",Op,(BYTE)*(Opcode+i+1));
            lstrcat(Disasm->Opcode,menemonic);
            Disasm->OpcodeSize=2;
			Disasm->PrefixSize=PrefixesSize;
            ++(*Index);
        }
        break;

        case 0x05:case 0x0D:case 0x15: // INSTRUCTION EAX/AX,XXXXXXXX
        case 0x1D:case 0x25:case 0x2D: // INSTRUCTION EAX/AX,XXXXXXXX
		case 0x35:case 0x3D:case 0xA9: // INSTRUCTION EAX/AX,XXXXXXXX
        {
			char mene[10]="";
			GetInstruction(Op,mene); // Get instruction for a specified Byte

			if(RegPrefix==0){ // no prefix
				// read 4 bytes into EAX
				SwapDword((BYTE*)(Opcode+i+1),&dwOp,&dwMem);
                wsprintf(menemonic,"%s %s, %08Xh",mene,Regs[REG32][0],dwMem);
				lstrcat(Disasm->Assembly,menemonic);
				wsprintf(menemonic,"%02X %08X",Op,dwOp);
				lstrcat(Disasm->Opcode,menemonic);
				Disasm->OpcodeSize=5;
				Disasm->PrefixSize=PrefixesSize;
				(*Index)+=4;
				break;
			}
			//else if(RegPrefix==1) // RegPrefix is being used
			//{
				// read 2 bytes into AX (REG16)
				SwapWord((BYTE*)(Opcode+i+1),&wOp,&wMem);
                wsprintf(menemonic,"%s %s, %04Xh",mene,Regs[REG16][0],wMem);
				lstrcat(Disasm->Assembly,menemonic);
				wsprintf(menemonic,"%02X %04X",Op,wOp);
				lstrcat(Disasm->Opcode,menemonic);
				Disasm->OpcodeSize=3;
				Disasm->PrefixSize=PrefixesSize;
				(*Index)+=2;
            //}
        }
        break;

        case 0x06: // PUSH ES
        {
            lstrcat(Disasm->Assembly,"push es");
            strcpy_s(Disasm->Remarks,";Push ES register to the stack");
			lstrcat(Disasm->Opcode,"06");
			Disasm->PrefixSize=PrefixesSize;
        }
        break;

		case 0x07: // POP ES
        {
            lstrcat(Disasm->Assembly,"pop es");
            strcpy_s(Disasm->Remarks,";Pop top stack to ES");
			lstrcat(Disasm->Opcode,"07");
			Disasm->PrefixSize=PrefixesSize;
        }
        break;
        
        case 0x0E: // PUSH CS
        {
			lstrcat(Disasm->Assembly,"push cs");
			strcpy_s(Disasm->Remarks,";Push CS register to the stack");
			lstrcat(Disasm->Opcode,"0E");
			Disasm->PrefixSize=PrefixesSize;
        }
        break;

        //========================================================//
        //=== DECODE NEW INSTRUCTION SET (MMX/3DNow!/SSE/SSE2) ===//
        //========================================================//
        case 0x0F:
        {
          char Instruction[128],m_bytes[128];
          int RetVal;
          BYTE Code=(BYTE)Opcode[i+1];

          lstrcat(Disasm->Opcode,"0F");
          RetVal=GetNewInstruction(Code,Instruction,RegPrefix,Opcode,i); // Check Instruction Type

          switch(RetVal) // check if we need to decode instruction
          {
            case 0:
            {
                // Decode SIB + ModRM
                if((BYTE)Opcode[i+2]>=0x00 && (BYTE)Opcode[i+2]<=0xBF)
                {
                    (*Index)++;
                    i=*Index;
                    Bit_D=(Op&0x02)>>1;		// Get bit d (direction)
				    Bit_W=(Op&0x01);		// Get bit w (full/partial reg size)
                    Mod_RM_SIB_EX(&Disasm,&Opcode,i,AddrPrefix,SEG,&Index,Code,RegPrefix,SegPrefix,AddrPrefix,Bit_D,Bit_W,RepPrefix);
                    Disasm->PrefixSize=PrefixesSize;
                    Disasm->OpcodeSize++; // 0F extra Byte
                    break;
			    }
              //else
              //{
                  //if(((BYTE)Opcode[i+2] & 0xC0)==0xC0)
                  //{
                        Bit_D=(Op&0x02)>>1;		// Get bit d (direction)
                        Bit_W=(Op&0x01);		// Get bit w (full/partial reg size)
                        (*Index)++;
                        i=*Index;
                        Mod_11_RM_EX(Bit_D,Bit_W,&Opcode,&Disasm,RegPrefix,Code,&Index,RepPrefix); // Decode with bits
                        Disasm->PrefixSize=PrefixesSize;
                        Disasm->OpcodeSize++; // 0F extra Byte
                  //}
                  //break;
              //}
            }
            break; // big set instructions
            
            case 1: // 1 byte instructions set
            {
                lstrcat(Disasm->Assembly,Instruction);
                wsprintf(Instruction,"%02X",Code);
                lstrcat(Disasm->Opcode,Instruction);
                Disasm->OpcodeSize=2;
                Disasm->PrefixSize=PrefixesSize;
                (*Index)++;
            }
            break;
            
            case 2: // JUMP NEAR (JXX)
            {
                SwapDword((BYTE*)(Opcode+i+2),&dwOp,&dwMem);
                dwMem+=Disasm->Address+PrefixesSize+6; // calculate dest addr
                Address=dwMem;
                wsprintf(m_bytes,"%08X",dwMem);
                lstrcat(Disasm->Assembly,Instruction);
                wsprintf(m_bytes,"%08X",dwOp);
                wsprintf(Instruction,"%02X ",Code);
                lstrcat(Disasm->Opcode,Instruction);
                lstrcat(Disasm->Opcode,m_bytes);
                Disasm->OpcodeSize=6;
                Disasm->PrefixSize=PrefixesSize;
                Disasm->CodeFlow.Jump = TRUE;
                Disasm->CodeFlow.BranchSize=NEAR_JUMP;
                (*Index)+=5;
            }
            break; 
            
            case 3:
            {
                if(((BYTE)Opcode[i+2]&0xC0)==0xC0)
                {
                    Bit_D=(Op&0x02)>>1;		// Get bit d (direction)
                    Bit_W=(Op&0x01);		// Get bit w (full/partial reg size)
                    (*Index)++;
                    i=*Index;
                    Mod_11_RM_EX(Bit_D,Bit_W,&Opcode,&Disasm,RegPrefix,Code,&Index,RepPrefix); // Decode with bits
                    Disasm->PrefixSize=PrefixesSize;
                    Disasm->OpcodeSize++; // 0F as extra Byte
					break;
                }
              //else
              //{
                    lstrcat(Disasm->Assembly,Instruction);
                    wsprintf(Instruction,"%02X",Code);
                    lstrcat(Disasm->Opcode,Instruction);
                    Disasm->OpcodeSize=2;
                    Disasm->PrefixSize=PrefixesSize;
                    (*Index)++;
              //}
            }
            break;

			case 4:
			{
				// List of opcodes that are not valid in two byes 0x0F map.
				if( !(
					( (BYTE)Opcode[i+1]>=0x04 && (BYTE)Opcode[i+1]<=0x05 ) ||
					( (BYTE)Opcode[i+1]==0x07 ) || ( (BYTE)Opcode[i+1]==0x0A ) ||
					( (BYTE)Opcode[i+1]>=0x0C && (BYTE)Opcode[i+1]<=0x0F ) ||
					( (BYTE)Opcode[i+1]>=0x19 && (BYTE)Opcode[i+1]<=0x1F ) ||
					( (BYTE)Opcode[i+1]==0x25 ) || ( (BYTE)Opcode[i+1]==0x27 ) ||
					( (BYTE)Opcode[i+1]>=0x36 && (BYTE)Opcode[i+1]<=0x3B ) ||
					( (BYTE)Opcode[i+1]>=0x3D && (BYTE)Opcode[i+1]<=0x3F ) ||
					( (BYTE)Opcode[i+1]>=0x78 && (BYTE)Opcode[i+1]<=0x7D ) ||
					( (BYTE)Opcode[i+1]>=0xA6 && (BYTE)Opcode[i+1]<=0xA7 ) ||
					( (BYTE)Opcode[i+1]==0xB8 ) || ( (BYTE)Opcode[i+1]==0xF0 ) || 
					( (BYTE)Opcode[i+1]==0xFF )

					) 
				)
				{
					(*Index)++;
					i=*Index;
					// FIX BIT D/W for proper disassembling
					switch((BYTE)Opcode[i-2]){
						case 0xF3:case 0xF2:{	Bit_D=0;	}	break;	//	->
						case 0x66:{				Bit_D=1;	}	break;	//	<-
						default:Bit_D=(Op&0x02)>>1;	// Get bit d (direction)
					}
					Bit_W=(Op&0x01);         // Get bit w (full/partial reg size)
					Mod_RM_SIB_EX(&Disasm,&Opcode,i,AddrPrefix,SEG,&Index,Code,RegPrefix,SegPrefix,AddrPrefix,Bit_D,Bit_W,RepPrefix);
					Disasm->PrefixSize=PrefixesSize;
					Disasm->OpcodeSize++; // 0F extra Byte
				}
				else{
					lstrcat(Disasm->Assembly,"???");
					wsprintf(Instruction,"%02X",Code);
					lstrcat(Disasm->Opcode,Instruction);
					Disasm->OpcodeSize=2;
					Disasm->PrefixSize=PrefixesSize;
					(*Index)++;
				}
			}
			break;

			case 5:{	// CMPXCHG8B
				if( (BYTE)Opcode[i+2]>=0x08 && (BYTE)Opcode[i+2]<=0x0F )	// Valid Ranges of values
				{	// Scope of valid operations
					(*Index)++;
					i=*Index;
					// FIX BIT D/W for proper disassembling
					Bit_D=(Op&0x02)>>1;
					Bit_W=(Op&0x01);	// Get bit w (full/partial reg size)
					Mod_RM_SIB_EX(&Disasm,&Opcode,i,AddrPrefix,SEG,&Index,Code,RegPrefix,SegPrefix,AddrPrefix,Bit_D,Bit_W,RepPrefix);
					Disasm->PrefixSize=PrefixesSize;
					Disasm->OpcodeSize++;	// 0F extra Byte
				}
				else{	// Invalid Operations
					lstrcat(Disasm->Assembly,"???");
					wsprintf(Instruction,"%02X",Code);
					lstrcat(Disasm->Opcode,Instruction);
					Disasm->OpcodeSize=2;
					Disasm->PrefixSize=PrefixesSize;
					(*Index)++;
				}
		   }
		   break;
          }
        }
        break;

        case 0x16: // PUSH SS
        {
            lstrcat(Disasm->Assembly,"push ss");
            strcpy_s(Disasm->Remarks,";Push SS register to the stack");
            lstrcat(Disasm->Opcode,"16");
            Disasm->PrefixSize=PrefixesSize;
        }
        break;
            
        case 0x17: // POP SS
        {
            lstrcat(Disasm->Assembly,"pop ss");
            strcpy_s(Disasm->Remarks,";Pop top stack to SS");
            lstrcat(Disasm->Opcode,"17");
            Disasm->PrefixSize=PrefixesSize;
        }
        break;
            
        case 0x1E: // PUSH DS
        {
            lstrcat(Disasm->Assembly,"push ds");
            strcpy_s(Disasm->Remarks,";Push DS register to the stack");
            lstrcat(Disasm->Opcode,"1E");
            Disasm->PrefixSize=PrefixesSize;
        }
        break;

        case 0x1F: // POP DS
        {
            lstrcat(Disasm->Assembly,"pop ds");
            strcpy_s(Disasm->Remarks,";Pop top stack to DS"); 
            lstrcat(Disasm->Opcode,"1F");
            Disasm->PrefixSize=PrefixesSize;
        }
        break;
            
        case 0x27: // DAA
        {
            lstrcat(Disasm->Assembly,"daa");
            lstrcat(Disasm->Opcode,"27");
            Disasm->PrefixSize=PrefixesSize;
        }
        break;
            
        case 0x2F: // DAS
        {
            lstrcat(Disasm->Assembly,"das");
            lstrcat(Disasm->Opcode,"2F");
            Disasm->PrefixSize=PrefixesSize;
        }
        break;
            
        case 0x37: // AAA
        {
            lstrcat(Disasm->Assembly,"aaa");
            lstrcat(Disasm->Opcode,"37");
            Disasm->PrefixSize=PrefixesSize;
        }
        break;
            
        case 0x3F: // AAS
        {
            lstrcat(Disasm->Assembly,"aas");
            lstrcat(Disasm->Opcode,"3F");
            Disasm->PrefixSize=PrefixesSize;
        }
        break;

		case 0x40:case 0x41: // INC XXX/XX
		case 0x42:case 0x43: // INC XXX/XX
		case 0x44:case 0x45: // INC XXX/XX
		case 0x46:case 0x47: // INC XXX/XX
        {
			wsprintf(menemonic,"inc %s",Regs[RM][Op&0x0F]); // Find reg by Masking (Op&0x0F)
            lstrcat(Disasm->Assembly,menemonic);
			wsprintf(menemonic,"%02X",Op);
            lstrcat(Disasm->Opcode,menemonic);
			Disasm->PrefixSize=PrefixesSize;
        }
        break;

		case 0x48:case 0x49: // DEC XXX/XX
		case 0x4A:case 0x4B: // DEC XXX/XX
		case 0x4C:case 0x4D: // DEC XXX/XX
		case 0x4E:case 0x4F: // DEC XXX/XX
        {
			wsprintf(menemonic,"dec %s",Regs[RM][Op&0x0F-0x08]);// Find reg by Masking (Op&0x0F-0x08)
            lstrcat(Disasm->Assembly,menemonic);
			wsprintf(menemonic,"%02X",Op);
            lstrcat(Disasm->Opcode,menemonic);
			Disasm->PrefixSize=PrefixesSize;
        }
        break;

		case 0x50:case 0x51: // PUSH XXX/XX
		case 0x52:case 0x53: // PUSH XXX/XX
		case 0x54:case 0x55: // PUSH XXX/XX
		case 0x56:case 0x57: // PUSH XXX/XX
        {
            // 'Assume' EntryPoint on PUSH
            if((RM==REG32) && ((Op&0x0F)==REG_EBP))
                FunctionsEntryPoint=TRUE;

			wsprintf(menemonic,"push %s",Regs[RM][Op&0x0F]);// Find reg by Masking (Op&0x0F)
            lstrcat(Disasm->Assembly,menemonic);
			wsprintf(menemonic,"%02X",Op);
            lstrcat(Disasm->Opcode,menemonic);
			Disasm->PrefixSize=PrefixesSize;
        }
        break;

		case 0x58:case 0x59: // POP XXX/XX
		case 0x5A:case 0x5B: // POP XXX/XX
		case 0x5C:case 0x5D: // POP XXX/XX
		case 0x5E:case 0x5F: // POP XXX/XX
        {
			wsprintf(menemonic,"pop %s",Regs[RM][(Op&0x0F)-0x08]);// Find reg by Masking (Op&0x0F-0x08)
            lstrcat(Disasm->Assembly,menemonic);
			wsprintf(menemonic,"%02X",Op);
            lstrcat(Disasm->Opcode,menemonic);
			Disasm->PrefixSize=PrefixesSize;
        }
        break;

		case 0x60: // PUSHAD/W 
        {
			if(!RegPrefix) // if RegPrefix == 0
				lstrcat(Disasm->Assembly,"pushad");
			else /*if(RegPrefix==1)*/ // Change Reg Size
				lstrcat(Disasm->Assembly,"pushaw");
            
            lstrcat(Disasm->Opcode,"60");
			Disasm->PrefixSize=PrefixesSize;
        }
        break;

		case 0x61: // POPAD/W
        {
			if(!RegPrefix) // if RegPrefix == 0
				lstrcat(Disasm->Assembly,"popad");
			else /*if(RegPrefix==1)*/ // Change Reg Size
				lstrcat(Disasm->Assembly,"popaw");
            
            lstrcat(Disasm->Opcode,"61");
			Disasm->PrefixSize=PrefixesSize;
        }
        break;

		case 0x68: // PUSH XXXXXXXX
        {
			if(!RegPrefix) // no Reg Prefix
			{   // PUSH 4 bytes
                SwapDword((BYTE*)(Opcode+i+1),&dwOp,&dwMem);
                Address=dwMem;
                PushString=TRUE; // search for string ref
				wsprintf(menemonic,"push %08Xh",dwMem);
				lstrcat(Disasm->Assembly,menemonic);
				wsprintf(menemonic,"68 %08X",dwOp);
				lstrcat(Disasm->Opcode,menemonic);
				Disasm->OpcodeSize=5;
				Disasm->PrefixSize=PrefixesSize;
				(*Index)+=4;
				break;
			}
	      //else
		  //{
				// PUSH 2 bytes
				SwapWord((BYTE*)(Opcode+i+1),&wOp,&wMem);
                wsprintf(menemonic,"push %04Xh",wMem);
				lstrcat(Disasm->Assembly,menemonic);
				wsprintf(menemonic,"68 %04X",wOp);
				lstrcat(Disasm->Opcode,menemonic);
				Disasm->OpcodeSize=3;
				Disasm->PrefixSize=PrefixesSize;
				(*Index)+=2;
		   //}
        }
        break;

		case 0x6A: // PUSH XX
        {
			if((BYTE)Opcode[i+1]>=0x80) // Signed Numebers (Negative)
				wsprintf(menemonic,"push -%02Xh",(0x100-(BYTE)Opcode[i+1]));
			else
				wsprintf(menemonic,"push %02Xh",(BYTE)Opcode[i+1]); // Unsigned Numbers (Positive)
            
			lstrcat(Disasm->Assembly,menemonic);
            wsprintf(menemonic,"6A%02X",(BYTE)*(Opcode+i+1));
            lstrcat(Disasm->Opcode,menemonic);
            Disasm->OpcodeSize=2;
			Disasm->PrefixSize=PrefixesSize;
            ++(*Index);
        }
        break;

		case 0x6C: case 0x6D: // INSB/INSW/INSD
        {
			if((Op&0x0F)==0x0C){
				lstrcat(Disasm->Assembly,"insb");
				wsprintf(menemonic,";Byte ptr ES:[%s], DX",Regs[ADDRM][7]);
				strcpy_s(Disasm->Remarks,StringLen(menemonic)+1,menemonic);
			}
			else{
				if((Op&0x0F)==0x0D){
					if(!RegPrefix) // If RegPrefix == 0
					{
						lstrcat(Disasm->Assembly,"insd");
						wsprintf(menemonic,";Dword ptr ES:[%s], DX",Regs[ADDRM][7]);
						strcpy_s(Disasm->Remarks,StringLen(menemonic)+1,menemonic);
					}
					else /*if(RegPrefix==1)*/ // Found RegPrefix == 1
                    {
							lstrcat(Disasm->Assembly,"insw");
							wsprintf(menemonic,";Word ptr ES:[%s], DX",Regs[ADDRM][7]);
							strcpy_s(Disasm->Remarks,StringLen(menemonic)+1,menemonic);
                    }
				}
			}

			wsprintf(menemonic,"%02X",Op);
            lstrcat(Disasm->Opcode,menemonic);
			Disasm->PrefixSize=PrefixesSize;
        }
        break;

		case 0x6E: case 0x6F: // OUTSB/OUTSW/OUTSD
        {
			if((Op&0x0F)==0x0E){
				lstrcat(Disasm->Assembly,"outsb");
				wsprintf(menemonic,";DX, Byte ptr ES:[%s]",Regs[ADDRM][7]);
				strcpy_s(Disasm->Remarks,StringLen(menemonic)+1,menemonic);
			}
			else{
				if((Op&0x0F)==0x0F){			
					if(!RegPrefix) // If RegPrefix == 0
					{
						lstrcat(Disasm->Assembly,"outsd");
						wsprintf(menemonic,";DX, Dword ptr ES:[%s]",Regs[ADDRM][7]);
						strcpy_s(Disasm->Remarks,StringLen(menemonic)+1,menemonic);
					}
					else /*if(RegPrefix==1)*/ // Found RegPrefix == 1
					{
						lstrcat(Disasm->Assembly,"outsw");
						wsprintf(menemonic,";DX, Word ptr ES:[%s]",Regs[ADDRM][7]);
						strcpy_s(Disasm->Remarks,StringLen(menemonic)+1,menemonic);
					}
				}
			}

			wsprintf(menemonic,"%02X",Op);
            lstrcat(Disasm->Opcode,menemonic);
			Disasm->PrefixSize=PrefixesSize;
        }
        break;

		case 0x70: case 0x71: case 0x72: case 0x73: // JUMP XXXXXXXX
		case 0x74: case 0x75: case 0x76: case 0x77: // JUMP XXXXXXXX 
		case 0x78: case 0x79: case 0x7A: case 0x7B: // JUMP XXXXXXXX
		case 0x7C: case 0x7D: case 0x7E: case 0x7F: // JUMP XXXXXXXX
		case 0xE0: case 0xE1: case 0xE2: case 0xEB: // LOOPX XXXXXXXX
        case 0xE3:									// JECXZ - JCXZ   
        {
			DWORD_PTR JumpAddress=0;
			BYTE JumpSize;
			char temp[10];
			JumpSize=(BYTE)Opcode[i+1];

            // Short Jump $+2
			if((BYTE)Opcode[i+1]>0x7F)
				JumpAddress=Disasm->Address + ((2 + PrefixesSize + JumpSize)-0x100);
			else
				JumpAddress=Disasm->Address + 2 + JumpSize + PrefixesSize;
			
			GetJumpInstruction(Op,temp);
            
			if(Op==0xE3 && AddrPrefix==1){
                strcpy_s(temp,"jcxz");
			}
            Address=JumpAddress;
			wsprintf(menemonic,"%s %08X",temp,JumpAddress);
			lstrcat(Disasm->Assembly,menemonic);
			wsprintf(menemonic,"%02X%02X",Op,(BYTE)Opcode[i+1]);
			lstrcat(Disasm->Opcode,menemonic);
			Disasm->OpcodeSize=2;
			Disasm->PrefixSize=PrefixesSize;
            Disasm->CodeFlow.Jump = TRUE;
            Disasm->CodeFlow.BranchSize = SHORT_JUMP;
            
            if(((BYTE)Opcode[i])==0xEB)
                DirectJmp=TRUE;

			++(*Index);
        }
        break;

		case 0x90: // NOP (XCHG EAX, EAX) 
        {
            lstrcat(Disasm->Assembly,"nop");
            lstrcat(Disasm->Opcode,"90");
			Disasm->PrefixSize=PrefixesSize;
        }
        break;

		case 0x91:case 0x92:	// XCHG XXX, XXX
		case 0x93:case 0x94:	// XCHG XXX, XXX
		case 0x95:case 0x96:	// XCHG XXX, XXX
		case 0x97:				// XCHG XXX, XXX
		{
			Mod_11_RM(1,1,&Opcode,&Disasm,"xchg",RegPrefix,Op,&Index); //+ 0x30
			Disasm->PrefixSize=PrefixesSize;
		}
		break;

		case 0x98: // CWDE/CDW (Prefix) 
        {
			if(!RegPrefix)
				lstrcat(Disasm->Assembly,"cwde");
			else /*if(RegPrefix==1)*/
				lstrcat(Disasm->Assembly,"cbw");

            lstrcat(Disasm->Opcode,"98");
			Disasm->PrefixSize=PrefixesSize;
        }
        break;

		case 0x99: // CWDE/CDW (Prefix) 
        {
			if(!RegPrefix)
				lstrcat(Disasm->Assembly,"cdq");
			else /*if(RegPrefix==1)*/
				lstrcat(Disasm->Assembly,"cwd");

            lstrcat(Disasm->Opcode,"98");            
			Disasm->PrefixSize=PrefixesSize;
        }
        break;

        case 0x9A: case 0xEA: // CALL/JMP XXXX:XXXXXXXX (FAR CALL)
        {
            char temp[10];
            
            switch(Op){
               case 0x9A:strcpy_s(temp,"call");break;
               case 0xEA:strcpy_s(temp,"jmp");break;
            }
            
            if(AddrPrefix==0){
                SwapDword((BYTE*)(Opcode+i+1),&dwOp,&dwMem);
                SwapWord((BYTE*)(Opcode+i+5),&wOp,&wMem);
                
                wsprintf(menemonic,"%s %04X:%08X",temp,wMem,dwMem);
                lstrcat(Disasm->Assembly,menemonic);
                wsprintf(menemonic,"%02X %08X %04X",Op,dwOp,wOp);
                lstrcat(Disasm->Opcode,menemonic);
                Disasm->OpcodeSize=7;
                Disasm->PrefixSize=PrefixesSize;
                (*Index)+=6;
            }
            else{
                WORD w_op,w_mem;
                SwapWord((BYTE*)(Opcode+i+3),&wOp,&wMem);
                SwapWord((BYTE*)(Opcode+i+1),&w_op,&w_mem);  
                
                wsprintf(menemonic,"%s %04X:%08X",temp,wMem,w_mem);
                lstrcat(Disasm->Assembly,menemonic);
                wsprintf(menemonic,"%02X %04X %04X",Op,w_op,wOp);
                lstrcat(Disasm->Opcode,menemonic);
                Disasm->OpcodeSize=5;
                Disasm->PrefixSize=PrefixesSize;
                (*Index)+=4;
            }
			
            wsprintf(menemonic,";Far %s",temp);
            strcpy_s(Disasm->Remarks,StringLen(menemonic)+1,menemonic);
        }
		break;

		case 0x9B: // WAIT
        {
            lstrcat(Disasm->Assembly,"wait");
            lstrcat(Disasm->Opcode,"9B");
            lstrcat(Disasm->Remarks,";Prevent the CPU from accessing memory used by coProccesor");
			Disasm->PrefixSize=PrefixesSize;
        }
        break;

		case 0x9C: // PUSHFD/PUSHFW 
        {
			if(!RegPrefix)
				lstrcat(Disasm->Assembly,"pushfd");
			else /*if(RegPrefix==1)*/
				lstrcat(Disasm->Assembly,"pushfw");

            lstrcat(Disasm->Opcode,"9C");            
			Disasm->PrefixSize=PrefixesSize;
        }
        break;

		case 0x9D: // POPFD/POPFW 
        {
			if(!RegPrefix)
				lstrcat(Disasm->Assembly,"popfd");
			else /*if(RegPrefix==1)*/
				lstrcat(Disasm->Assembly,"popfw");

            lstrcat(Disasm->Opcode,"9D");
			Disasm->PrefixSize=PrefixesSize;
        }
        break;

		case 0x9E: // SAHF
        {
            lstrcat(Disasm->Assembly,"sahf");
            lstrcat(Disasm->Opcode,"9E");
            lstrcat(Disasm->Remarks,";AH = 8bits of EFLAGS");
			Disasm->PrefixSize=PrefixesSize;
        }
        break;

		case 0x9F: // LAHF
        {
            lstrcat(Disasm->Assembly,"lahf");
            lstrcat(Disasm->Opcode,"9F");
            lstrcat(Disasm->Remarks,";EFLAGS = 8bits of AH");
			Disasm->PrefixSize=PrefixesSize;
        }
        break;

        case 0xA0:case 0xA2: // MOV AL, BYTE PTR XX:[XXXXXXXX], AL
        {
            if(!AddrPrefix){
                SwapDword((BYTE*)(Opcode+i+1),&dwOp,&dwMem);
                switch(Op){
					case 0xA0:wsprintf(menemonic,"mov al, Byte ptr %s:[%08Xh]",Segs[SEG],dwMem);break;
					case 0xA2:wsprintf(menemonic,"mov Byte ptr %s:[%08Xh], al",Segs[SEG],dwMem);break;
                }
                
                lstrcat(Disasm->Assembly,menemonic);
                wsprintf(menemonic,"%02X %08X",Op,dwOp);
                lstrcat(Disasm->Opcode,menemonic);
                Disasm->OpcodeSize=5;
                Disasm->PrefixSize=PrefixesSize;
                (*Index)+=4;
				break;
            }
          //else 
          //{
                SwapWord((BYTE*)(Opcode+i+1),&wOp,&wMem);
                switch(Op){
					case 0xA0:wsprintf(menemonic,"mov al, Byte ptr %s:[%04Xh]",Segs[SEG],wMem);break;
					case 0xA2:wsprintf(menemonic,"mov Byte ptr %s:[%04Xh], al",Segs[SEG],wMem);break;
                }
                lstrcat(Disasm->Assembly,menemonic);
                wsprintf(menemonic,"%02X %04X",Op,wOp);
                lstrcat(Disasm->Opcode,menemonic);
                Disasm->OpcodeSize=3;
                Disasm->PrefixSize=PrefixesSize;
                (*Index)+=2;
          //}
        }
		break;

        case 0xA1:case 0xA3: // MOV EAX/AX, BYTE PTR XX:[XXXXXXXX], EAX/AX
        {
            if(!AddrPrefix)// no addr size change
            {
                SwapDword((BYTE*)(Opcode+i+1),&dwOp,&dwMem);
                switch(Op){
					case 0xA1:wsprintf(menemonic,"mov %s, %s ptr %s:[%08Xh]",Regs[RM][0],RSize,Segs[SEG],dwMem);break;
					case 0xA3:wsprintf(menemonic,"mov %s ptr %s:[%08Xh], %s",RSize,Segs[SEG],dwMem,Regs[RM][0]);break;
                }
                lstrcat(Disasm->Assembly,menemonic);
                wsprintf(menemonic,"%02X %08X",Op,dwOp);
                lstrcat(Disasm->Opcode,menemonic);
                Disasm->OpcodeSize=5;
                Disasm->PrefixSize=PrefixesSize;
                (*Index)+=4;
				break;
            }
          //else if(AddrPrefix==1)
          //{
                SwapWord((BYTE*)(Opcode+i+1),&wOp,&wMem);
                switch(Op) // change addr size DWORD->WORD
                {
					case 0xA1:wsprintf(menemonic,"mov %s, %s ptr %s:[%04Xh]",Regs[RM][0],RSize,Segs[SEG],wMem);break;
					case 0xA3:wsprintf(menemonic,"mov %s ptr %s:[%04Xh], %s",RSize,Segs[SEG],wMem,Regs[RM][0]);break;
                }
                lstrcat(Disasm->Assembly,menemonic);
                wsprintf(menemonic,"%02X %04X",Op,wOp);
                lstrcat(Disasm->Opcode,menemonic);
                Disasm->OpcodeSize=3;
                Disasm->PrefixSize=PrefixesSize;
                (*Index)+=2;
          //}						
        }
		break;

        case 0xA4:case 0xA5: // MOVSB/MOVSW/MOVSD
        {
			if(RepPrefix==1 && (BYTE)Opcode[i-1]==0xF3)
				strcpy_s(Disasm->Assembly,"rep ");

			if((Op&0x0F)==0x04)
				wsprintf(menemonic,";Byte ptr %s:[%s], Byte ptr %s:[%s]",Segs[SEG_ES],Regs[ADDRM][7],Segs[SEG],Regs[ADDRM][6]);
			else if((Op&0x0F)==0x05)
				  wsprintf(menemonic,";%s ptr %s:[%s], %s ptr %s:[%s]",RSize,Segs[SEG_ES],Regs[ADDRM][7],RSize,Segs[SEG],Regs[ADDRM][6]);
			
			lstrcat(Disasm->Assembly,"movs");
            strcpy_s(Disasm->Remarks,StringLen(menemonic)+1,menemonic);
			wsprintf(menemonic,"%02X",Op);
            lstrcat(Disasm->Opcode,menemonic);
			Disasm->PrefixSize=PrefixesRSize;
        }
        break;

		case 0xA6:case 0xA7: // CMPSB/CMPSW/CMPSD
        {
			if((Op&0x0F)==0x06){
				if(RepPrefix==1)
					wsprintf(menemonic,";Byte ptr %s:[%s], Byte ptr %s:[%s]",Segs[SEG_ES],Regs[ADDRM][7],Segs[SEG],Regs[ADDRM][6]);
				else 
					wsprintf(menemonic,";Byte ptr %s:[%s], Byte ptr %s:[%s]",Segs[SEG],Regs[ADDRM][6],Segs[SEG_ES],Regs[ADDRM][7]);
			}
			else if((Op&0x0F)==0x07){
				if(RepPrefix==1)
				  wsprintf(menemonic,";%s ptr %s:[%s], %s ptr %s:[%s]",RSize,Segs[SEG_ES],Regs[ADDRM][7],RSize,Segs[SEG],Regs[ADDRM][6]);
				else
				  wsprintf(menemonic,";%s ptr %s:[%s], %s ptr %s:[%s]",RSize,Segs[SEG],Regs[ADDRM][6],RSize,Segs[SEG_ES],Regs[ADDRM][7]);
			}
			
			lstrcat(Disasm->Assembly,"cmps");
            strcpy_s(Disasm->Remarks,StringLen(menemonic)+1,menemonic);
			wsprintf(menemonic,"%02X",Op);
            lstrcat(Disasm->Opcode,menemonic);
			Disasm->PrefixSize=PrefixesRSize;
        }
        break;

		case 0xAA:case 0xAB: // STOSB/STOSW/STOSD
        {
			if(RepPrefix==1 && (BYTE)Opcode[i-1]==0xF3)
				strcpy_s(Disasm->Assembly,"rep ");

			if((Op&0x0F)==0x0A)
				wsprintf(menemonic,";Byte ptr %s:[%s]",Segs[SEG_ES],Regs[ADDRM][7]);
			else if((Op&0x0F)==0x0B)
				  wsprintf(menemonic,";%s ptr %s:[%s]",RSize,Segs[SEG_ES],Regs[ADDRM][7]);
			
			lstrcat(Disasm->Assembly,"stos");
            strcpy_s(Disasm->Remarks,StringLen(menemonic)+1,menemonic);
			wsprintf(menemonic,"%02X",Op);
            lstrcat(Disasm->Opcode,menemonic);
			Disasm->PrefixSize=PrefixesRSize;
        }
        break;

		case 0xAC:case 0xAD: // LODSB/LODSW/LODSD
        {
			if(RepPrefix==1 && (BYTE)Opcode[i-1]==0xF3)
				strcpy_s(Disasm->Assembly,"rep ");

			if((Op&0x0F)==0x0C)
				wsprintf(menemonic,";Byte ptr %s:[%s]",Segs[SEG_DS],Regs[ADDRM][6]);
			else if((Op&0x0F)==0x0D)
				  wsprintf(menemonic,";%s ptr %s:[%s]",RSize,Segs[SEG_DS],Regs[ADDRM][6]);
			
			lstrcat(Disasm->Assembly,"lods");
            strcpy_s(Disasm->Remarks,StringLen(menemonic)+1,menemonic);
			wsprintf(menemonic,"%02X",Op);
            lstrcat(Disasm->Opcode,menemonic);
			Disasm->PrefixSize=PrefixesRSize;
        }
        break;

		case 0xAE:case 0xAF: // SCASB/SCASW/SCASD
        {
			if((Op&0x0F)==0x0E)
				wsprintf(menemonic,";Byte ptr %s:[%s]",Segs[SEG_ES],Regs[ADDRM][7]);
			else if((Op&0x0F)==0x0F)
				  wsprintf(menemonic,";%s ptr %s:[%s]",RSize,Segs[SEG_ES],Regs[ADDRM][7]);
			
			lstrcat(Disasm->Assembly,"scas");
            strcpy_s(Disasm->Remarks,StringLen(menemonic)+1,menemonic);
			wsprintf(menemonic,"%02X",Op);
            lstrcat(Disasm->Opcode,menemonic);
			Disasm->PrefixSize=PrefixesRSize;
        }
        break;

		case 0xB0:case 0xB1: // MOV XX, XX
	    case 0xB2:case 0xB3: // MOV XX, XX
		case 0xB4:case 0xB5: // MOV XX, XX
		case 0xB6:case 0xB7: // MOV XX, XX
        {
            wsprintf(menemonic,"mov %s, %02Xh",Regs[REG8][Op&0xF],(BYTE)Opcode[i+1]);
            lstrcat(Disasm->Assembly,menemonic);
            wsprintf(menemonic,"%02X%02X",Op,(BYTE)*(Opcode+i+1));
            lstrcat(Disasm->Opcode,menemonic);
            Disasm->OpcodeSize=2;
			Disasm->PrefixSize=PrefixesSize;
            ++(*Index);
        }
        break;

        case 0xB8:case 0xB9: // MOV XX/XXX, XXXXXXXX
        case 0xBA:case 0xBB: // MOV XX/XXX, XXXXXXXX
        case 0xBC:case 0xBD: // MOV XX/XXX, XXXXXXXX
        case 0xBE:case 0xBF: // MOV XX/XXX, XXXXXXXX
        {
            if(!RegPrefix) // check if default prefix has changed
            {
                SwapDword((BYTE*)(Opcode+i+1),&dwOp,&dwMem);
                Address=dwMem;
                PushString=TRUE; // try search for string ref in mov instruction
                wsprintf(menemonic,"mov %s, %08Xh",Regs[RM][(Op&0xF)-0x08],dwMem);
                lstrcat(Disasm->Assembly,menemonic);
                wsprintf(menemonic,"%02X %08X",Op,dwOp);
                lstrcat(Disasm->Opcode,menemonic);
                Disasm->OpcodeSize=5;
                Disasm->PrefixSize=PrefixesSize;
                (*Index)+=4;
				break;
            }
          //else
          //{
                SwapWord((BYTE*)(Opcode+i+1),&wOp,&wMem);
                wsprintf(menemonic,"mov %s, %04Xh",Regs[RM][(Op&0xF)-0x08],wMem);
                lstrcat(Disasm->Assembly,menemonic);    
                wsprintf(menemonic,"%02X %04X",Op,wOp);
                lstrcat(Disasm->Opcode,menemonic);
                Disasm->OpcodeSize=3;
                Disasm->PrefixSize=PrefixesSize;
                (*Index)+=2;
          //}
        }
		break;

		case 0xC2:case 0xCA: // RET/F XXXX
		{
			char code[6];
			switch(Op){
                case 0xC2:{
					wsprintf(code,"ret");
					Disasm->CodeFlow.Ret=TRUE;
                }
                break;
				case 0xCA:wsprintf(code,"retf");break;
			}
            
            SwapWord((BYTE*)(Opcode+i+1),&wOp,&wMem);
            
			if(wMem>=0xA000){
              wsprintf(menemonic,"%s %05X",code,wMem);
			}
			else{
                wsprintf(menemonic,"%s %04Xh",code,wMem);
			}

			lstrcat(Disasm->Assembly,menemonic);
            wsprintf(menemonic,"%02X %04X",Op,wOp);

			lstrcat(Disasm->Opcode,menemonic);
			Disasm->OpcodeSize=3;
			Disasm->PrefixSize=PrefixesSize;
			if(LockPrefix==1){
                lstrcat(Disasm->Remarks,";<Illegal Lock Prefix>");
			}
            
            (*Index)+=2;
        }
		break;

		case 0xC3: // RET
        {
            lstrcat(Disasm->Assembly,"ret");
            lstrcat(Disasm->Opcode,"C3");
			Disasm->PrefixSize=PrefixesSize;
			lstrcat(Disasm->Remarks,";Pop IP");
            Disasm->CodeFlow.Ret=TRUE;
        }
        break;

		case 0xC8: // ENTER XXXX, XX
		{
            SwapWord((BYTE*)(Opcode+i+1),&wOp,&wMem);
			wsprintf(menemonic,"enter %04Xh, %02Xh",wMem,(BYTE)Opcode[i+3]);
			lstrcat(Disasm->Assembly,menemonic);
			wsprintf(menemonic,"C8 %04X %02X",wOp,(BYTE)Opcode[i+3]);
			lstrcat(Disasm->Opcode,menemonic);
			Disasm->OpcodeSize=4;
			Disasm->PrefixSize=PrefixesSize;
            lstrcat(Disasm->Remarks,";Creates a stack frame for a procedure (HLL)");
			(*Index)+=3;
		}
		break;

		case 0xC9: // LEAVE
        {
            lstrcat(Disasm->Assembly,"leave");
            lstrcat(Disasm->Opcode,"C9");
            lstrcat(Disasm->Remarks,";Releases the local variables");
			Disasm->PrefixSize=PrefixesSize;
        }
        break;

		case 0xCB: // RETF
        {
            lstrcat(Disasm->Assembly,"retf");
            lstrcat(Disasm->Opcode,"CB");
			Disasm->PrefixSize=PrefixesSize;
        }
        break;

		case 0xCC: // INT 3
        {
            lstrcat(Disasm->Assembly,"int3");
            lstrcat(Disasm->Opcode,"CC");
			Disasm->PrefixSize=PrefixesSize;
        }
        break;

		case 0xCD: // INT XX
        {
			wsprintf(menemonic,"int %02Xh",(BYTE)Opcode[i+1]);
            lstrcat(Disasm->Assembly,menemonic);
            wsprintf(menemonic,"CD %02X",(BYTE)*(Opcode+i+1));
            lstrcat(Disasm->Opcode,menemonic);
            Disasm->OpcodeSize=2;
			Disasm->PrefixSize=PrefixesSize;
            ++(*Index);
        }
        break;

		case 0xCE: // INTO
        {
            lstrcat(Disasm->Assembly,"into");
            lstrcat(Disasm->Opcode,"CE");
            lstrcat(Disasm->Remarks,";Generate INT 4 if OF=1");
			Disasm->PrefixSize=PrefixesSize;
        }
        break;

		case 0xCF: // IRETD/W
        {
			if(!RegPrefix){
				lstrcat(Disasm->Assembly,"iretd");
			}
			else /*if(RegPrefix==1)*/{
				lstrcat(Disasm->Assembly,"iretw");
			}
            lstrcat(Disasm->Opcode,"CF");
			Disasm->PrefixSize=PrefixesSize;
        }
        break;

		case 0xD4:case 0xD5: // AAM/AAD
        {
			char opcode[5];
			switch(Op)
			{
                case 0xD4: 
                { 
                    wsprintf(opcode,"aam"); 
                    lstrcat(Disasm->Remarks,";ASCII adjust AX after multiply");
                } 
                break;

                case 0xD5:
                {
                    wsprintf(opcode,"aad");
                    lstrcat(Disasm->Remarks,";ASCII adjust AX before devision");
                }
                break;
			}
			wsprintf(menemonic,"%s %02Xh",opcode,(BYTE)Opcode[i+1]);
            lstrcat(Disasm->Assembly,menemonic);
            wsprintf(menemonic,"%02X%02X",Op,(BYTE)*(Opcode+i+1));
            lstrcat(Disasm->Opcode,menemonic);
            Disasm->OpcodeSize=2;
			Disasm->PrefixSize=PrefixesSize;
            ++(*Index);
        }
        break;

		case 0xD6: // SALC
        {
            lstrcat(Disasm->Assembly,"salc");
            lstrcat(Disasm->Opcode,"D6");
            lstrcat(Disasm->Remarks,";Set the Carry flag in AL");
			Disasm->PrefixSize=PrefixesSize;
        }
        break;

		case 0xD7: // XLAT
		{
			lstrcat(Disasm->Assembly,"xlat");
			lstrcat(Disasm->Opcode, "D7");
			wsprintf(menemonic,";Byte ptr %s:[%s+al]",Segs[SEG],Regs[ADDRM][3]);
			lstrcat(Disasm->Remarks,menemonic);
			Disasm->PrefixSize=PrefixesSize;
		}
		break;
		
		case 0xE5: // IN EAX/AX, XX
        {
			// special case Opcode, insted of reading DWORD (4 bytes), we read 1 BYTE.
			char mene[10]="";
			GetInstruction(Op,mene); // get instruction from opcode

			if(RegPrefix==0) // no prefix
			{   
				// read 4 bytes into EAX
				wsprintf(menemonic,"%s %s, %02Xh",mene,Regs[REG32][0],(BYTE)Opcode[i+1]);
				lstrcat(Disasm->Assembly,menemonic);
			}
			else /*if(RegPrefix==1)*/ // prefix is being used
			{   
				// read 2 bytes into AX
				wsprintf(menemonic,"%s %s, %02Xh",mene,Regs[REG16][0],(BYTE)Opcode[i+1]);
				lstrcat(Disasm->Assembly,menemonic);
			}

			wsprintf(menemonic,"%02X%02X",Op,(BYTE)Opcode[i+1]);
			lstrcat(Disasm->Opcode,menemonic);
			Disasm->OpcodeSize=2;
			Disasm->PrefixSize=PrefixesSize;
			lstrcat(Disasm->Remarks,";I/O Instruction");
			++(*Index);
        }
        break;

		case 0xE6: // OUT XX, AL
        {
			char mene[10]="";
			GetInstruction(Op,mene);
            wsprintf(menemonic,"%s %02Xh, al",mene,(BYTE)Opcode[i+1]);
            lstrcat(Disasm->Assembly,menemonic);
            strcpy_s(Disasm->Remarks,"");
            wsprintf(menemonic,"%02X%02X",Op,(BYTE)*(Opcode+i+1));
            lstrcat(Disasm->Opcode,menemonic);
            Disasm->OpcodeSize=2;
			Disasm->PrefixSize=PrefixesSize;
			lstrcat(Disasm->Remarks,";I/O Instruction");
            ++(*Index);
        }
        break;

		case 0xE7: // OUT XX, AX/EAX
        {
			// special case Opcode, insted of reading DWORD (4 bytes), we read 1 BYTE.
			char mene[10]="";
			GetInstruction(Op,mene); // get instruction from opcode

			if(RegPrefix==0) // no prefix
			{   
				// read 1 byte into EAX
				wsprintf(menemonic,"%s %02Xh, %s",mene,(BYTE)Opcode[i+1],Regs[REG32][0]);
				lstrcat(Disasm->Assembly,menemonic);
			}
			else /*if(RegPrefix==1)*/ // prefix is being used
			{   
				// read 1 byte into AX
				wsprintf(menemonic,"%s %02Xh, %s",mene,(BYTE)Opcode[i+1],Regs[REG16][0]);
				lstrcat(Disasm->Assembly,menemonic);
			}
			
			wsprintf(menemonic,"%02X%02X",Op,(BYTE)Opcode[i+1]);
			lstrcat(Disasm->Opcode,menemonic);
			Disasm->OpcodeSize=2;
			Disasm->PrefixSize=PrefixesSize;
			lstrcat(Disasm->Remarks,";I/O Instruction");
			++(*Index);
        }
        break;

		case 0xE8:case 0xE9: // CALL/JMP XXXX/XXXXXXXX
		{
			DWORD_PTR CallAddress=0;
			DWORD_PTR CallSize=0;
			char temp[10];

            // Select instruction
			switch(Op)
			{
                case 0xE8:
                { 
                  strcpy_s(temp,"call"); 
                  CallApi=TRUE; 
                  Disasm->CodeFlow.Call=TRUE; 
                  Disasm->CodeFlow.BranchSize = NEAR_JUMP;
                } 
                break;

                case 0xE9:
                { 
                  strcpy_s(temp,"jmp"); 
                  Disasm->CodeFlow.Jump=TRUE; 
                  Disasm->CodeFlow.BranchSize = NEAR_JUMP;
                  DirectJmp=TRUE;
                } 
                break;
			}

			if(!RegPrefix)
			{
                SwapDword((BYTE*)(Opcode+i+1),&dwOp,&dwMem);
				dwMem+= Disasm->Address + CallSize + 5 + (PrefixesSize-RegPrefix);
				Address=dwMem;
                wsprintf(menemonic,"%s %08X",temp,dwMem);
				lstrcat(Disasm->Assembly,menemonic);
				wsprintf(menemonic,"%02X %08X",Op,dwOp);
				lstrcat(Disasm->Opcode,menemonic);
				Disasm->OpcodeSize=5;
				Disasm->PrefixSize = PrefixesSize;
                (*Index)+=4;
				break;
            }
	      //else 
		  //{
                CallApi=FALSE;
                SwapWord((BYTE*)(Opcode+i+1),&wOp,&wMem);

                if(wMem>=0x0000F000){
					CallAddress = ((wMem + 4 + (PrefixesSize-RegPrefix))-0x0000F000);
                }
                else{
					CallAddress = (Disasm->Address-0x00400000) + wMem + 4 + (PrefixesSize-RegPrefix);
                }

				wsprintf(menemonic,"%s %08X",temp, CallAddress);
				lstrcat(Disasm->Assembly,menemonic);
				wsprintf(menemonic,"%02X %04X",Op,wOp);
				lstrcat(Disasm->Opcode,menemonic);
				Disasm->OpcodeSize=3;
				Disasm->PrefixSize = PrefixesSize;
                (*Index)+=2;
		  //}
		}
		break;

		case 0xEC: // IN AL, DX
        {
            lstrcat(Disasm->Assembly,"in al, dx");
            lstrcat(Disasm->Opcode,"EC");
			Disasm->PrefixSize=PrefixesSize;
			lstrcat(Disasm->Remarks,";I/O Instruction");
        }
        break;

		case 0xED: // IN AX/EAX, DX
        {
			wsprintf(menemonic,"in %s, dx",Regs[RM][0]);
			lstrcat(Disasm->Assembly,menemonic);
            lstrcat(Disasm->Opcode,"ED");
			Disasm->PrefixSize=PrefixesSize;
			lstrcat(Disasm->Remarks,";I/O Instruction");
        }
        break;

		case 0xEE: // OUT DX, AL
        {
            lstrcat(Disasm->Assembly,"out dx, al");
            lstrcat(Disasm->Opcode,"EE");
			Disasm->PrefixSize=PrefixesSize;
			lstrcat(Disasm->Remarks,";I/O Instruction");
        }
        break;

		case 0xEF: // OUT DX, AX/EAX
        {
			wsprintf(menemonic,"out dx, %s",Regs[RM][0]);
			lstrcat(Disasm->Assembly,menemonic);
            lstrcat(Disasm->Opcode,"EF");
			Disasm->PrefixSize=PrefixesSize;
			lstrcat(Disasm->Remarks,";I/O Instruction");
        }
        break;

		case 0xF1: // ICEBP (INT1)
        {
			lstrcat(Disasm->Assembly,"int1");
            lstrcat(Disasm->Opcode,"F1");
			Disasm->PrefixSize=PrefixesSize;
			lstrcat(Disasm->Remarks,";(icebp)");
        }
        break;

		case 0xF4: // HLT (HALT)
        {
			lstrcat(Disasm->Assembly,"hlt");
            lstrcat(Disasm->Opcode,"F4");
			Disasm->PrefixSize=PrefixesSize;
			lstrcat(Disasm->Remarks,";Halts CPU until RESET");
        }
        break;

		case 0xF5: // CMC
        {
			lstrcat(Disasm->Assembly,"cmc");
            lstrcat(Disasm->Opcode,"F4");
			Disasm->PrefixSize=PrefixesSize;
			lstrcat(Disasm->Remarks,";inverts the Carry Flag");
        }
        break;

		case 0xF8: // CLC
        {
			lstrcat(Disasm->Assembly,"clc");
            lstrcat(Disasm->Opcode,"F8");
			Disasm->PrefixSize=PrefixesSize;
			lstrcat(Disasm->Remarks,";Clears the Carry Flag");
        }
        break;

		case 0xF9: // STC
        {
			lstrcat(Disasm->Assembly,"stc");
            lstrcat(Disasm->Opcode,"F9");
			Disasm->PrefixSize=PrefixesSize;
			lstrcat(Disasm->Remarks,";Sets the Carry Flag to 1");
        }
        break;

		case 0xFA: // CLI
        {
			lstrcat(Disasm->Assembly,"cli");
            lstrcat(Disasm->Opcode,"FA");
			Disasm->PrefixSize=PrefixesSize;
			lstrcat(Disasm->Remarks,";Set Interrupt flag to 0");
        }
        break;

		case 0xFB: // STI
        {
			lstrcat(Disasm->Assembly,"sti");
            lstrcat(Disasm->Opcode,"FB");
			Disasm->PrefixSize=PrefixesSize;
			lstrcat(Disasm->Remarks,";Set Interrupt flag to 1");
        }
        break;

		case 0xFC: // CLD
        {
			lstrcat(Disasm->Assembly,"cld");
            lstrcat(Disasm->Opcode,"FC");
			Disasm->PrefixSize=PrefixesSize;
			lstrcat(Disasm->Remarks,";Set Direction Flag to 0");
        }
        break;

		case 0xFD:{ // STD
			lstrcat(Disasm->Assembly,"std");
            lstrcat(Disasm->Opcode,"FD");
			Disasm->PrefixSize=PrefixesSize;
			lstrcat(Disasm->Remarks,";Set Direction Flag to 1");
        }
        break;
    }
}

void FlushDecoded(DISASSEMBLY *Disasm)
{
	// Clear all information of the decoded Instruction 
    strcpy_s(Disasm->Assembly,"");   // Clear Assembly.
    strcpy_s(Disasm->Remarks,"");    // Clear commets.
    strcpy_s(Disasm->Opcode,"");     // Clear opcodes code.
    Disasm->OpcodeSize = 1;        // Lowest opcode size.
	Disasm->PrefixSize = 0;        // Size Of Prefixes is Zero.
    Disasm->CodeFlow.Call=FALSE;   // No Call CodeFlow.
    Disasm->CodeFlow.Jump=FALSE;   // No Jump CodeFlow.
    Disasm->CodeFlow.Ret=FALSE;    // No Ret  CodeFlow.
    Disasm->CodeFlow.BranchSize=0; // BranchSize is ZERO (unknown)
}

//////////////////////////////////////////////////////////////////////////
//							Disassembler								//	
//////////////////////////////////////////////////////////////////////////
void WINAPI Disassembler(/*LPVOID lpParam*/) // Thread Worker for Decoding Instructions
{
	/*        
		Name: Disassembler Procedure:
		=====

		Parameters:  None
        ===========

		Return Values: None
        ===============

		Usage: 
        =====
        This is the main procedure that,
		deals with the actual decoding of machine code,
		to assembly lines.
		the procedure uses the pointer to the memory,
		mapped file, using pe file structs we can define,
		the section to begin the decoding from.
		all decoded assembly are showed on the Disasm ,
		list control.

		פרמטרים: אין פרמטרים
		ערך מוחזר: הפרוצדורה לא מחזירה שום ערך

		זוהי הפרוצדורה הראשית שמטפלת בפיענוח קוד המכונה
		לשפת אססמבלי.
		הפרוצדורה משתמשת במצביע לקובץ שנימצא בזיכרון
		ותוך כשי שימוש במבני של קובץ הרצה אנו מסוגלים
		לחשב איזה חלק בקובץ אנו צריכים לפענח.
		שכן רק חלק אחד בקובץ מכיל מידע  על הקוד עצמו 
	    בעוד שהחלקים האחרים משמשים לשמירת המשתנים,
		תמונות, אייקונים וכו.
		כל הקוד המפוענח מוצג על גבי אובייקט הרשימה
	*/
  	
    CODE_BRANCH CBranch;
    // Index of opcode to decode
    DWORD_PTR Index=0;
	DWORD_PTR DisasmAddress=0;
	// Exe Entry point in memory  (+ImageBase in memory)
	DWORD_PTR EntryPoint=0,OEP=0,Padding=0,i,Position=0,EndSection=0,StartSection=0,FuncEnd=-1,ProcAddr,fIndex;
	// Number of bytes we need to decode.
	DWORD BytesToDecode=0;
	// Index of item in the disasm list control
	DWORD ListIndex=0;
    // Function Counter
    DWORD FunctionCounter=0,NoFunctionCounter=0,StringRefCounter=0;
    signed int percent,step=-1;
    // get disasm list control handler
	HWND hDisasm=GetDlgItem(mainhWnd,IDC_DISASM);
	// Creates an Disasm Struct
    DISASSEMBLY Disasm; 
	// PE Information structs.
	HMENU hMenu=GetMenu(mainhWnd);
    // fucntion Scanner
    TreeMapItr itr;
    // Temp text
    char MessageText[MAX_PATH]="";
    bool error=FALSE;//,TraceAddr;
	DisassemblerReady=FALSE;
	
	// Pointer to linear address (machine code to decode)
	char *Linear="";
    
    // Init reference
    CallApi=FALSE;
    JumpApi=FALSE;
    CallAddrApi=FALSE;
    PushString=FALSE;
    Address=0;
    DisasmIsWorking=TRUE;
    DisassemblerReady=FALSE;
    FunctionsEntryPoint=FALSE;

	// Pointer to the Start of the Mapped File In Memory (MZ Header)
    Linear=DisasmPtr; 
	// Update structs from the file pointer and PE
	DosHeader=(IMAGE_DOS_HEADER*)DisasmPtr;
	// NT Header begins at = Dos_Header+e_lfanew; (NT Header - PE)
	NTheader=(IMAGE_NT_HEADERS*)(DosHeader->e_lfanew+DisasmPtr);
	//SectionHeader begins at = NTheader->OptionalHeader + sizeof(IMAGE_OPTIONAL_HEADER)
	SectionHeader = (IMAGE_SECTION_HEADER *)((UCHAR *)(&(NTheader->OptionalHeader))+sizeof(IMAGE_OPTIONAL_HEADER)); // -0x10 in 64bit native
	// Get Entry Point of EXE
	EntryPoint=NTheader->OptionalHeader.AddressOfEntryPoint;

    OutDebug(mainhWnd,"");
    Sleep(55); // Delay
    SelectLastItem(GetDlgItem(mainhWnd,IDC_LIST)); // Selects the Last Item
    
    OutDebug(mainhWnd,"-- PVDasm Analysis --");
    Sleep(55);
    SelectLastItem(GetDlgItem(mainhWnd,IDC_LIST)); // Selects the Last Item
    
    wsprintf(MessageText,"Found Entry Point: %08X",EntryPoint);
    OutDebug(mainhWnd,MessageText);
    Sleep(55);
    SelectLastItem(GetDlgItem(mainhWnd,IDC_LIST)); // Selects the Last Item

	// Calculate section where EntryPoint begins at.
	error=NULL;
	SectionHeader=RVAToOffset(NTheader,SectionHeader,EntryPoint,&error);
	// On invalid section, PVDasm uses the last section as default.
	// TODO, if error found (invalid section), add a select section screen to disassemble.
	
    wsprintf(MessageText,"Disassembling section: \"%s\"",SectionHeader->Name);
    OutDebug(mainhWnd,MessageText);
    Sleep(55);
    SelectLastItem(GetDlgItem(mainhWnd,IDC_LIST)); // Selects the Last Item

	// Get Number of bytes to Decode.
	BytesToDecode=SectionHeader->SizeOfRawData-1; // -1 for security

    wsprintf(MessageText,"Total bytes to decode: %ld Bytes",BytesToDecode);
    OutDebug(mainhWnd,MessageText);
    Sleep(55);
    SelectLastItem(GetDlgItem(mainhWnd,IDC_LIST)); // Selects the Last Item

	// Adjust Address to start decode from.
	Linear+=SectionHeader->PointerToRawData;

    wsprintf(MessageText,"Flushing internal variables.");
    OutDebug(mainhWnd,MessageText);
    Sleep(55);
    SelectLastItem(GetDlgItem(mainhWnd,IDC_LIST)); // Selects the Last Item
	// Reintialize the struct to its default content.
	FlushDecoded(&Disasm);
    
	// Calculate Adress where we begin at: OEP=ImageBase+EntryPoint
	// OEP: original Entry Point
	Disasm.Address=SectionHeader->VirtualAddress+NTheader->OptionalHeader.ImageBase;
    StartSection=Disasm.Address;

    OEP=EntryPoint+NTheader->OptionalHeader.ImageBase; // Original Entry Point

    wsprintf(MessageText,"Found OEP: %08X",OEP);
    OutDebug(mainhWnd,MessageText);
    Sleep(55);
    SelectLastItem(GetDlgItem(mainhWnd,IDC_LIST)); // Selects the Last Item

    EndSection=(SectionHeader->VirtualAddress+NTheader->OptionalHeader.ImageBase)+BytesToDecode; 
    wsprintf(MessageText,"Excepcted end of disassembly at address: %08X",EndSection);
    SetDlgItemText(mainhWnd,IDC_MESSAGE2,MessageText);

    EnableMenuItem(hMenu,IDC_START_DISASM,MF_GRAYED);
    // Reset Content of ListView
    SendMessage(hDisasm,LVM_DELETEALLITEMS,0,0);

    SendDlgItemMessage(mainhWnd,IDC_DISASM_PROGRESS,PBM_SETRANGE32,(WPARAM)0,(LPARAM)BytesToDecode);
    
    wsprintf(MessageText,"Loading %s...",DISASM_VER);
    OutDebug(mainhWnd,MessageText);
    Sleep(55);
    SelectLastItem(GetDlgItem(mainhWnd,IDC_LIST)); // Selects the Last Item

    wsprintf(MessageText,"Starting Disassembly at: %08X",Disasm.Address);
    OutDebug(mainhWnd,MessageText);
    OutDebug(mainhWnd,"~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
    Sleep(55);
    SelectLastItem(GetDlgItem(mainhWnd,IDC_LIST)); // Selects the Last Item
    
    // Show the progress bar in Disassembly Mode
    ShowWindow(GetDlgItem(mainhWnd,IDC_DISASM_PROGRESS),SW_SHOW);

    // Disable ToolBar Call/Jump Tracing buttons
    SendMessage ( hWndTB, TB_ENABLEBUTTON, ID_RET_JUMP,  (LPARAM) FALSE );
    SendMessage ( hWndTB, TB_ENABLEBUTTON, ID_RET_CALL,  (LPARAM) FALSE );
    SendMessage ( hWndTB, TB_ENABLEBUTTON, ID_EXEC_JUMP, (LPARAM) FALSE );
    SendMessage ( hWndTB, TB_ENABLEBUTTON, ID_EXEC_CALL, (LPARAM) FALSE );

    //////////////////////////////////////////////////////////////////////////
    //					FIRST PASS DISASSEMBLY								//
    //////////////////////////////////////////////////////////////////////////
	try{
		if(disop.FirstPass==TRUE){ // check for Firstpass flag
			if(LoadFirstPass==TRUE){    
				Index=(EntryPoint-SectionHeader->VirtualAddress);  // EP Offset
				try{
					
					wsprintf(MessageText,"Starting First Pass Analyzer...");
					OutDebug(mainhWnd,MessageText);            
					SelectLastItem(GetDlgItem(mainhWnd,IDC_LIST));
					
					//Analyze file with FirstPass
					FirstPass( 
						Index,
						BytesToDecode,
						NTheader->OptionalHeader.ImageBase+EntryPoint,
						Linear,
						StartSection,
						EndSection
						);
					
					wsprintf(MessageText,"First Pass Analyzer Process... [Success]");
					OutDebug(mainhWnd,MessageText);
					Sleep(55);
					SelectLastItem(GetDlgItem(mainhWnd,IDC_LIST));
				}
				catch(...){
					wsprintf(MessageText,"First Pass Analyzer Process... [Failed]");
					OutDebug(mainhWnd,MessageText);
					Sleep(55);
					SelectLastItem(GetDlgItem(mainhWnd,IDC_LIST));
				}
				LoadFirstPass=FALSE;
			}
		}
	}
	catch(...){}
    
    //////////////////////////////////////////////////////////////////////////    
    //						DISASSEMBLER CORE								//
    //////////////////////////////////////////////////////////////////////////    
	wsprintf(MessageText,"Disassembling File...");
    OutDebug(mainhWnd,MessageText);            
    SelectLastItem(GetDlgItem(mainhWnd,IDC_LIST));
    Disasm.Address=SectionHeader->VirtualAddress+NTheader->OptionalHeader.ImageBase;
    // Start at EP Immidiately
    if(disop.StartFromEP==TRUE){
        Index=(EntryPoint-SectionHeader->VirtualAddress);  // New Index
        Disasm.Address=NTheader->OptionalHeader.ImageBase+EntryPoint;
        // ReSet the ProgressBar's Range
        SendDlgItemMessage(mainhWnd,IDC_DISASM_PROGRESS,PBM_SETRANGE32,(WPARAM)0,(LPARAM)BytesToDecode-Index);
    }
    else { Index=0; }

    ListIndex=0;
    FlushDecoded(&Disasm);

    try{ // Start try{} block to protect in case of exceptions.
		for(;Index<BytesToDecode;Index++,ListIndex++){ // Loop instructions
            // Clear References
            CallApi=FALSE;
            JumpApi=FALSE;
            CallAddrApi=FALSE;
            PushString=FALSE;
            FunctionsEntryPoint=FALSE;
            Address=0;
            Padding=0;
            CBranch.Branch_Destination=0;
            CBranch.Current_Index=0;

            // Calculate Percent of Disassembly state
            percent=signed int(((float)Index/(float)BytesToDecode)*100.0);
        
            //
            // If data locations has been defined by the user
            // Extract the bytes from it.
            // in the form of:
            //
            // DB 'H'
            // DB 'I'
            // DB  0
			//

			try{
				if(CheckDataAtAddress(Disasm.Address)==TRUE){
					// Decode form of DB XX 'ascii'
					wsprintf(Disasm.Opcode,"%02X",(BYTE)(*(Linear+Index)));
					if((BYTE)(*(Linear+Index))>0x9F){
						wsprintf(Disasm.Assembly,"db 0%02Xh",(BYTE)(*(Linear+Index)));
					}
					else{
						wsprintf(Disasm.Assembly,"db %02Xh",(BYTE)(*(Linear+Index)));
					}
					
					if(disop.UpperCased_Disasm==PV_TRUE)
						CharUpper(Disasm.Assembly);
					
					if((BYTE)(*(Linear+Index)) != 0x00){
						if((BYTE)(*(Linear+Index)) == 0x0A){ // new line hex code
							lstrcpy(Disasm.Remarks,"; ''");
						}
						else{
							wsprintf(Disasm.Remarks,"; '%c'",(BYTE)(*(Linear+Index)));
						}
					}
					else{
						wsprintf(Disasm.Remarks,"; 0");
					}
					
					// Show Decoded instruction, size, remarks...
					SaveDecoded(Disasm,disop,ListIndex);
					// Clear all information
					FlushDecoded(&Disasm);	
					Disasm.Address++; // 1 Byte Opcode 
					
					// Progress-Bar Position
					Position++;
					if(percent>step){   
						step=percent;
						PostMessage(GetDlgItem(mainhWnd,IDC_DISASM_PROGRESS),PBM_SETPOS,(WPARAM)Position,0);
						PostMessage(GetDlgItem(mainhWnd,IDC_DISASM_PROGRESS),PBM_STEPIT,0,0);
						PostMessage(mainhWnd,WM_USER+MY_MSG,0,(LPARAM)Disasm.Address);
					}
					continue;
				}
			}
			catch(...){}

            // =================================
            // Check if we are at entry Point
            // =================================
            if(Disasm.Address==OEP){
                disop.ShowAddr=FALSE; // Disable Showing Address
                strcpy_s(Disasm.Assembly,EP);
                SaveDecoded(Disasm,disop,ListIndex);
                FlushDecoded(&Disasm);  // reset all content
                disop.ShowAddr=TRUE; // Enable Showing Address 
                ListIndex++;
            }

			for(fIndex=0;fIndex<fFunctionInfo.size();fIndex++){
				if(fFunctionInfo[fIndex].FunctionStart==Disasm.Address && Disasm.Address!=NTheader->OptionalHeader.ImageBase+EntryPoint){
					disop.ShowAddr=FALSE; // Disable Showing Address

					if(lstrcmp(fFunctionInfo[fIndex].FunctionName,"")==0){
						wsprintf(Disasm.Assembly,"; ====== Proc_%08X Proc ======",Disasm.Address);
					}
					else{
						wsprintf(Disasm.Assembly,"; ====== %s Proc ======",fFunctionInfo[fIndex].FunctionName);
					}

					SaveDecoded(Disasm,disop,ListIndex);
					FlushDecoded(&Disasm);  // reset all content
					disop.ShowAddr=TRUE; // Enable Showing Address 
					ListIndex++;
                    FuncEnd=fFunctionInfo[fIndex].FunctionEnd;
                    ProcAddr=fFunctionInfo[fIndex].FunctionStart;
					break;
				}
			}
        
            // =====================
            // Force Disasm Option
            // =====================
            if(disop.ForceDisasmBeforeEP==FALSE){            
                if((Disasm.Address)<OEP){ // Still not at EP?
                    // Decode form of DB XX 'ascii'
                    wsprintf(Disasm.Opcode,"%02X",(BYTE)(*(Linear+Index)));
                    wsprintf(Disasm.Assembly,"DB %02X",(BYTE)(*(Linear+Index)));
                    wsprintf(Disasm.Remarks,";  ASCII '%c'",(BYTE)(*(Linear+Index)));
                    // Show Decoded instruction, size, remarks...
                    SaveDecoded(Disasm,disop,ListIndex);
                    // Clear all information
                    FlushDecoded(&Disasm);
                    Disasm.Address++; // 1 Byte Opcode 
                    
                    // Progress-Bar Position
                    Position++;
                    if(percent>step){   
                        step=percent;
                        PostMessage(GetDlgItem(mainhWnd,IDC_DISASM_PROGRESS),PBM_SETPOS,(WPARAM)Position,0);
                        PostMessage(GetDlgItem(mainhWnd,IDC_DISASM_PROGRESS),PBM_STEPIT,0,0);
                        PostMessage(mainhWnd,WM_USER+MY_MSG,0,(LPARAM)Disasm.Address);
                    }
                    continue; // Continue for loop
                }
            }
            else{
                // Stop at user defined's byte length Distance from EP
                if( Disasm.Address>=(OEP-RangeFromEP) ){
                    disop.ForceDisasmBeforeEP=FALSE;
                }
            }

            // ==============================================
            // Auto 00 Byte Resolver
            // Analyze Bytes, e.g: "00000000" as DB 4 Dup(0)
            // ==============================================
            if(disop.AutoDB==TRUE){                
                // Locate and Analyze 00 repeative bytes
                while(((BYTE)(*(Linear+Index)))==0x00){
                    // analyse only! before entryPoint
                    //if(Disasm.Address<OEP)
                    //{
                        ++Padding;
                        ++Index;
                    //}
                    //else break;
                }
                
                // when over 3 repeative, show them
                if(Padding>2){
                    for(i=0;i<Padding;i++){
						if((i+3)>=11){
                            break;
						}
                        
                        lstrcat(Disasm.Opcode,"00");
                    }

					if(Padding>=11){
                      lstrcat(Disasm.Opcode,"...");
					}
                    
                    wsprintf(Disasm.Assembly,"DB %ld DUP(0)",Padding);
                    SaveDecoded(Disasm,disop,ListIndex);
                    Disasm.Address+=Padding;
                    FlushDecoded(&Disasm);
                    Position++;
                    if(percent>step){
                        step=percent;
                        PostMessage(GetDlgItem(mainhWnd,IDC_DISASM_PROGRESS),PBM_SETPOS,(WPARAM)Position,0);
                        PostMessage(GetDlgItem(mainhWnd,IDC_DISASM_PROGRESS),PBM_STEPIT,0,0);
                        PostMessage(mainhWnd,WM_USER+MY_MSG,0,(LPARAM)Disasm.Address);
                    }
                    
                    Index--;
                    // Continue the Loop, No Decode.
                    continue;
                }
                else Index-=Padding;  // else return back from where we started
            }

			//==============================//
            //			DECODE CORE			//
            //==============================//
			
			try{
				Decode(&Disasm,Linear,&Index); // Decode Opcodes
			}
			catch(...){} // Do nothing on failure.
			
            // Uppercase Decoded Assembly (By The User)
			if(disop.UpperCased_Disasm==PV_TRUE){
				CharUpper(Disasm.Assembly);
			}


            // ================================
            // Add Possible Function EntryPoint
            // =================================
            // if(FunctionsEntryPoint==TRUE)
            //    fFunctionInfo.insert(fFunctionInfo.end(),ListIndex);
			
            //=====================================//
            //========= IMPORTS ANALYZER ==========//
            //=====================================//
            try{
                if(CallApi==TRUE || JumpApi==TRUE || CallAddrApi==TRUE){
                    // Resolve API ?
                    strcpy_s(MessageText,"");
                    if(GetAPIName(MessageText)==TRUE){
                        char temp[MAX_PATH];

                        if(CallApi==TRUE || CallAddrApi==TRUE){
							if(disop.UpperCased_Disasm==1){
                                wsprintf(temp,"CALL %s",MessageText);
							}
							else{
                                wsprintf(temp,"call %s",MessageText);
							}

                            // Code Flow can't be traced if analyzed
                            Disasm.CodeFlow.Call=TRUE;
                        }
                        else if(JumpApi==TRUE){
							if(disop.UpperCased_Disasm==1){
                                wsprintf(temp,"JMP [%s]",MessageText);
							}
							else{
                                wsprintf(temp,"jmp [%s]",MessageText);
							}

                            // Code Flow can't be traced if analyzed
                            Disasm.CodeFlow.Jump=FALSE;
                        }

                        // Save Index for the Import
                        ImportsLines.insert(ImportsLines.end(),ListIndex);
                        strcpy_s(Disasm.Assembly,StringLen(temp)+1,temp);
                        FunctionCounter++;
                        if(disop.ShowResolvedApis==1){
                            wsprintf(MessageText,"%08X : Found Function At Pointer: [ %08X ] -> %s",Disasm.Address,Address,temp);
                            OutDebug(mainhWnd,MessageText);
                            SelectLastItem(GetDlgItem(mainhWnd,IDC_LIST)); // Selects the Last Item
                        }
                    }
                }
                else if(PushString==TRUE){  // Push String References
                    strcpy_s(MessageText,"");
                    if(GetStringXRef(MessageText,Disasm.Address)==TRUE){
                        StringRefCounter++;
                        wsprintf(Disasm.Remarks,";ASCIIZ: \"%s\",0",MessageText);
                        // Save String Index.
                        StrRefLines.insert(StrRefLines.end(),ListIndex);
                    }
                }
            }
            catch(...){
                NoFunctionCounter++;
                if(disop.ShowResolvedApis==PV_TRUE){
                    wsprintf(MessageText,"%08X : Invalid Function Pointer: [ %08X ]",Disasm.Address,Address);
                    OutDebug(mainhWnd,MessageText);
                    SelectLastItem(GetDlgItem(mainhWnd,IDC_LIST)); // Selects the Last Item
                }
            }

            //=====================//
            // For Debugging Only  //
            //=====================//
            // if(Disasm.Address==0x10007D16)
            //    ListIndex+=0;   
            //
            //=====================//

            //
            // Save Index of CodeFlows
            //
            if(Disasm.CodeFlow.Call == TRUE || Disasm.CodeFlow.Jump == TRUE){
                // See if Branch is inside our section
                if( ( Address >= StartSection ) && ( Address <= EndSection ) ){
                    // Macro to add new key and data for the 
					// XReferences
					AddNew((DWORD)Address,ListIndex)

                    // Update Information
                    CBranch.Current_Index = ListIndex;
                    CBranch.Branch_Destination=Address;
                    CBranch.BranchFlow.Call = Disasm.CodeFlow.Call;
                    CBranch.BranchFlow.Jump = Disasm.CodeFlow.Jump;
					CBranch.BranchFlow.Ret = Disasm.CodeFlow.Ret;
					CBranch.BranchFlow.BranchSize = Disasm.CodeFlow.BranchSize;

                    // Save Data
                    DisasmCodeFlow.insert(DisasmCodeFlow.end(),CBranch);
                }
            }else{ // treat the RET with current address.
				if(Disasm.CodeFlow.Ret==TRUE){
					AddNew((DWORD)Disasm.Address,ListIndex)
                    // Update Information
                    CBranch.Current_Index = ListIndex;
                    CBranch.Branch_Destination=(DWORD)Address;
                    CBranch.BranchFlow.Call = Disasm.CodeFlow.Call;
                    CBranch.BranchFlow.Jump = Disasm.CodeFlow.Jump;
					CBranch.BranchFlow.Ret = Disasm.CodeFlow.Ret;
					CBranch.BranchFlow.BranchSize = Disasm.CodeFlow.BranchSize;

                    // Save Data
                    DisasmCodeFlow.insert(DisasmCodeFlow.end(),CBranch);
				}
            }

			// Show End of Function
			if(FuncEnd!=-1){
                if(Disasm.Address>=FuncEnd){
					for(fIndex=0;fIndex<fFunctionInfo.size();fIndex++){
						if(FuncEnd==fFunctionInfo[fIndex].FunctionEnd){
							if(lstrcmp(fFunctionInfo[fIndex].FunctionName,"")==0){
								wsprintf(Disasm.Remarks,"; proc_%08X endp",ProcAddr);
							}
							else{
								wsprintf(Disasm.Remarks,"; %s endp",fFunctionInfo[fIndex].FunctionName);
							}
							break;
						}
					}
                    FuncEnd=-1;
				}
			}

			// Show Decoded instruction, size, remarks...
            SaveDecoded(Disasm,disop,ListIndex);
            
			// try show xref
			//try{
			//	DisasmAddress=Disasm.Address;
			//	LocateXref(DisasmAddress,ListIndex);
			//}
			//catch(...){}

			// Calculate total Size of an instruction + Prefixes, and
			// Fix the address of 'IP'
			Disasm.Address+=Disasm.OpcodeSize+Disasm.PrefixSize;

            // New Position
            Position+=Disasm.OpcodeSize;
            if(percent>step){
               step=percent;
               // Advance the ProgressBar
               PostMessage(GetDlgItem(mainhWnd,IDC_DISASM_PROGRESS),PBM_SETPOS,(WPARAM)Position,0);
               PostMessage(GetDlgItem(mainhWnd,IDC_DISASM_PROGRESS),PBM_STEPIT,0,0);
               PostMessage(mainhWnd,WM_USER+MY_MSG,0,(LPARAM)Disasm.Address);
            }

            //=============================================//
            // Clear all information in the disasm struct  //
            //=============================================//
			FlushDecoded(&Disasm);
		}
	}
	catch(...){ // in a case of an exception (any kind of exception)
		
        wsprintf(MessageText,"Error Disassembling File (Devs: Index %ld)",ListIndex);
		MessageBox(mainhWnd,MessageText,"PV Error",MB_OK|MB_ICONERROR);
        
        // Disable & Enable Disassembly menu items
        EnableMenuItem ( hMenu, IDC_STOP_DISASM,     MF_GRAYED  ); 
        EnableMenuItem ( hMenu, IDC_PAUSE_DISASM,    MF_GRAYED  );
        EnableMenuItem ( hMenu, IDC_START_DISASM,    MF_GRAYED  );
        EnableMenuItem ( hMenu, IDC_START_DISASM,    MF_GRAYED  );
        EnableMenuItem ( hMenu, IDC_GOTO_START,      MF_GRAYED  );
        EnableMenuItem ( hMenu, IDC_GOTO_ENTRYPOINT, MF_GRAYED  );
        EnableMenuItem ( hMenu, IDC_GOTO_ADDRESS,    MF_GRAYED  );
        EnableMenuItem ( hMenu, IDM_FIND,            MF_ENABLED );

        // Disable ToolBar Buttons
        SendMessage ( hWndTB, TB_ENABLEBUTTON, ID_EP_SHOW,      (LPARAM) FALSE );
        SendMessage ( hWndTB, TB_ENABLEBUTTON, ID_CODE_SHOW,    (LPARAM) FALSE );
        SendMessage ( hWndTB, TB_ENABLEBUTTON, ID_ADDR_SHOW,    (LPARAM) FALSE );
        SendMessage ( hWndTB, TB_ENABLEBUTTON, ID_SHOW_XREF,    (LPARAM) FALSE );
        SendMessage ( hWndTB, TB_ENABLEBUTTON, ID_SHOW_IMPORTS, (LPARAM) FALSE );
        SendMessage ( hWndTB, TB_ENABLEBUTTON, ID_SEARCH,       (LPARAM) FALSE );
        SendMessage ( hWndTB, TB_ENABLEBUTTON, ID_PATCH,        (LPARAM) FALSE );
        ShowWindow(GetDlgItem(mainhWnd,IDC_DISASM_PROGRESS),SW_HIDE);

        // Close and exit the Thread.
		CloseHandle(hDisasmThread);
		ExitThread(0);
        return;
	}

	// Load API Signature Recognition.
    if(disop.Signatures==TRUE){
		// try to load the signatures.
		try{		
			LoadApiSignature();
		}
		catch(...){} // on fail, continue.
	}

    // Hide the Progress Bar
    ShowWindow(GetDlgItem(mainhWnd,IDC_DISASM_PROGRESS),SW_HIDE);
    DisassemblerReady=TRUE; // Dissassembler is ready    

    // Number of items
    ListView_SetItemCountEx(hDisasm,ListIndex,NULL);

    // reset the progressBar
    SendDlgItemMessage(mainhWnd,IDC_DISASM_PROGRESS,PBM_SETPOS,0,0);
    
    // Show Disassembly Summery:
    // ========================
    SetDlgItemText ( mainhWnd, IDC_MESSAGE1,"Disassembly Analysis Complete.");
    SetDlgItemText ( mainhWnd, IDC_MESSAGE2,"");
    SetDlgItemText ( mainhWnd, IDC_MESSAGE3,"");
    
	// Enable ToolBar Buttons
	SendMessage ( hWndTB, TB_ENABLEBUTTON, ID_EP_SHOW,   (LPARAM) TRUE );
	SendMessage ( hWndTB, TB_ENABLEBUTTON, ID_CODE_SHOW, (LPARAM) TRUE );
	SendMessage ( hWndTB, TB_ENABLEBUTTON, ID_ADDR_SHOW, (LPARAM) TRUE );
    SendMessage ( hWndTB, TB_ENABLEBUTTON, ID_SEARCH,    (LPARAM) TRUE );
    SendMessage ( hWndTB, TB_ENABLEBUTTON, ID_PATCH,     (LPARAM) TRUE );

    wsprintf(MessageText,"Total Of Resolved Functions: %ld",FunctionCounter);
    OutDebug(mainhWnd,MessageText);
    wsprintf(MessageText,"Total Of UnResolved Functions: %ld",NoFunctionCounter);
    OutDebug(mainhWnd,MessageText);
    SelectLastItem(GetDlgItem(mainhWnd,IDC_LIST)); // Selects the Last Item
  
    // Activate/enable Menu items
    EnableMenuItem ( hMenu, IDC_START_DISASM,       MF_ENABLED );
    EnableMenuItem ( hMenu, IDC_GOTO_START,         MF_ENABLED );
	EnableMenuItem ( hMenu, IDC_GOTO_ENTRYPOINT,    MF_ENABLED );
    EnableMenuItem ( hMenu, IDC_GOTO_ADDRESS,       MF_ENABLED );
    EnableMenuItem ( hMenu, IDM_FIND,               MF_ENABLED );
    EnableMenuItem ( hMenu, IDM_PATCHER,            MF_ENABLED );
    EnableMenuItem ( hMenu, IDC_DISASM_ADD_COMMENT, MF_ENABLED );
	EnableMenuItem ( hMenu, IDM_COPY_DISASM_FILE,   MF_ENABLED );
	EnableMenuItem ( hMenu, IDM_COPY_DISASM_CLIP,   MF_ENABLED );
	EnableMenuItem ( hMenu, IDM_SELECT_ALL_ITEMS,   MF_ENABLED );
    
    // If Imports availble, show them
    if(FunctionCounter>0){
        SendMessage(hWndTB, TB_ENABLEBUTTON, ID_SHOW_IMPORTS, (LPARAM)TRUE);
        EnableMenuItem(hMenu,IDC_DISASM_IMP,MF_ENABLED);
    }
    else{
        SendMessage(hWndTB, TB_ENABLEBUTTON, ID_SHOW_IMPORTS, (LPARAM)FALSE);
        EnableMenuItem(hMenu,IDC_DISASM_IMP,MF_GRAYED);
    }
    
    // If String references are availble, show them
    if(StringRefCounter>0){
        SendMessage(hWndTB, TB_ENABLEBUTTON, ID_SHOW_XREF, (LPARAM)TRUE);
        EnableMenuItem(hMenu,IDC_STR_REFERENCES,MF_ENABLED);
    }
    else{
        SendMessage(hWndTB, TB_ENABLEBUTTON, ID_SHOW_XREF, (LPARAM)FALSE);
        EnableMenuItem(hMenu,IDC_STR_REFERENCES,MF_GRAYED);
    }
    
    // Disable Stop Disassembly menu item
    EnableMenuItem ( hMenu, IDC_STOP_DISASM,  MF_GRAYED ); 
    EnableMenuItem ( hMenu, IDC_PAUSE_DISASM, MF_GRAYED );

    //=============================================//
    // Find and Select (highlight) The EntryPoint  //
    //=============================================//
    wsprintf(MessageText,"%08X",EntryPoint+NTheader->OptionalHeader.ImageBase);
    
    Index = SearchItemText(hDisasm,MessageText); // search item
    if(Index!=-1){ // found index
		SelectItem(hDisasm,Index-1); // select and scroll to item
    }

	// Add X-references
	try{
		OutDebug(mainhWnd,"Building XRefs Database...");
		LocateXrefs();
		OutDebug(mainhWnd,"-> XRefers Complete.");
	}
	catch(...){}

	wsprintf(MessageText,"Disassembly Analysis Complete.");
    OutDebug(mainhWnd,MessageText);
    OutDebug(mainhWnd,"------------------------------------------------------");
    Sleep(55);
    SelectLastItem(GetDlgItem(mainhWnd,IDC_LIST)); // Selects the Last Item.

	RedrawWindow(hDisasm,NULL,NULL,TRUE);
    // Close Thread Handle and Kill The Thread
    CloseHandle(hDisasmThread);
    TerminateThread(hDisasmThread,0);
    ExitThread(0);
}

//////////////////////////////////////////////////////////////////////////
//							Locate Xrefs								//
//////////////////////////////////////////////////////////////////////////

void LocateXrefs()
{
	DWORD_PTR key,size,i,offset=0;
	string inst;

	ItrXref it,itr=XrefData.begin();
	for(;itr!=XrefData.end();itr++){
		key = (*itr).first;
		size=DisasmDataLines.size();
		for(i=0;i<size;i++){
			inst = DisasmDataLines[i].GetMnemonic();
			transform(inst.begin(), inst.end(), inst.begin(), tolower);
			// remove 'ret' command, no need self referrenced.
			if(StringToDword(DisasmDataLines[i].GetAddress())==key && inst.find("ret")==-1){
				offset=i;
				break;
			}
		}

		it=XrefData.find((DWORD)key);
		if(it!=XrefData.end()){
			DisasmDataLines[offset].SetReference(";Referenced by a (Un)Conditional Jump(s)");
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//						LoadApiSignature								//
//////////////////////////////////////////////////////////////////////////
//
// The Function uses a API Database, loads all imports in the,
// Disassembly and tries to analyzed its parameters.
// When an import located, we try to find it in the api-database,
// than attach its parameters into the disassembly text.
//
void LoadApiSignature()
{
	WIN32_FIND_DATA W32FindData; // Find Data Struct
	HANDLE hFile,FoundSig;
	DWORD Size,ReadBytes,ParamCount,j;
	bool Loopy=TRUE;
	char Api[128]="";
	char* FileBuf,*ptr,*FoundString,*StartPtr;
	char cFilePath[MAX_PATH]="",Params[256]="",Comments[256]="";
	char cWildCard[]="*.sig";
	
	// Get Current Path.
	GetModuleFileName(NULL,cFilePath,MAX_PATH-1);
	// Remove the 'exe' Extension, and return dir path.
	ExtractFilePath(cFilePath);
	// Attach "\sig"  - Signature Dir.
	lstrcat(cFilePath,"sig");
	// Set Current path for the Sigs's dir.
    SetCurrentDirectory(cFilePath);
	
	// Find the Signature file, no metter the name.
	FoundSig=FindFirstFile(cWildCard,&W32FindData);
	if(FoundSig!=INVALID_HANDLE_VALUE){
		// Get File Handler for READ ONLY Mode.
		hFile=CreateFile(W32FindData.cFileName,GENERIC_READ,NULL,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
		if(hFile==INVALID_HANDLE_VALUE){
			return;
		}
		
		Size = GetFileSize(hFile,NULL);
		FileBuf = new char[Size];
		
		// Check if Read is possible.
		if(ReadFile(hFile,(char*)FileBuf,Size,&ReadBytes,NULL)==FALSE){
			// Remove all and return.
			CloseHandle(hFile);
			delete[] FileBuf;
			return;
		}
		
		// Remove the Handle to the Files.
		CloseHandle(hFile);
		
		// Scan Improts/CodeFlow Indexes, only Calls APIs are acceptble for signatures.
		for(ItrImports ImportCounter=ImportsLines.begin();ImportCounter!=ImportsLines.end();ImportCounter++){
			for(int Codeflow=0;Codeflow<DisasmCodeFlow.size();Codeflow++){
				if((*ImportCounter)==DisasmCodeFlow[Codeflow].Current_Index){
					if(DisasmCodeFlow[Codeflow].BranchFlow.Call==TRUE){
						Loopy=TRUE;
						Api[0]=NULL;
						strcpy_s(Api,StringLen(DisasmDataLines[(*ImportCounter)].GetMnemonic())+1,DisasmDataLines[(*ImportCounter)].GetMnemonic()); // Get Function
						ptr=Api;
						
						// Extract the Function's Name
						while(*ptr!='!'){
							ptr++; 
						}
						ptr++;
						
						// Search The Fucntion
						FoundString=strstr(FileBuf,ptr);
						if(FoundString!=NULL){
							while(Loopy){
								j=0;
								StartPtr=FoundString;
								while(*StartPtr!='\r' && *(StartPtr+1)!='\n'){
									StartPtr--;
								}
								
								StartPtr+=2;
								while(*StartPtr!=',' && *StartPtr!='\r' && *(StartPtr+1)!='\n' ){
									Params[j]=*StartPtr;
									StartPtr++;
									j++;
								}
								Params[j]=NULL;
								
								if(lstrcmp(Params,ptr)==0){
									Loopy=FALSE;
								}
								else{
									while((*FoundString!='\r' && *(FoundString+1)!='\n')){
										FoundString++;
									}
									FoundString+=2;
									StartPtr=strstr(FoundString,ptr);
									FoundString=StartPtr;
								}
							}
							
							strcpy_s(Params,"");
							
							FoundString+=StringLen(ptr);
							if(*FoundString=='\r' && *(FoundString+1)=='\n'){
								break;
							}
							
							// Move to the First Parameter
							while(*FoundString!=','){
								FoundString++;
							}
							FoundString++;
							j=0;
							ParamCount=1;
							
							do{
								if(*FoundString==','){
									Params[j]=NULL;
									strcpy_s(Comments,StringLen(DisasmDataLines[(*ImportCounter)-ParamCount].GetComments())+1,DisasmDataLines[(*ImportCounter)-ParamCount].GetComments());
									if(StringLen(Comments)!=0){
										wsprintf(cFilePath,"; %s [ %s ]",Params,Comments);
									}
									else{
										wsprintf(cFilePath,"; %s",Params);
									}
									
									DisasmDataLines[(*ImportCounter)-ParamCount].SetComments(cFilePath);
									
									FoundString++;
									strcpy_s(Params,"");
									j=0;
									ParamCount++;
								}
								else{
									Params[j] = *FoundString;
									j++;
									FoundString++;
								}
							}while(*FoundString!='\r' && *(FoundString+1)!='\n');
							
							Params[j]=NULL;
							strcpy_s(Comments,StringLen(DisasmDataLines[(*ImportCounter)-ParamCount].GetComments())+1,DisasmDataLines[(*ImportCounter)-ParamCount].GetComments());
							
							if(StringLen(Comments)!=0){
								wsprintf(cFilePath,"; %s [ %s ]",Params,Comments);
							}
							else{
								wsprintf(cFilePath,"; %s",Params);
							}
							
							DisasmDataLines[(*ImportCounter)-ParamCount].SetComments(cFilePath);
						}
					}
				}
			}
		}
		// Free the Allocated Memory.
		delete[] FileBuf;
	}
}

//////////////////////////////////////////////////////////////////////////
//							GetAPIName									//
//////////////////////////////////////////////////////////////////////////
//
// Get API Name from Address reffered by CALL XXXXXXXX or JMP DWORD [XXXXXXXX]
// * Address is a 'global' variable.
//
bool GetAPIName(char *Api_Name)
{
	/*
	#ifdef _WIN64
	#else
	#endif
    */
	PIMAGE_THUNK_DATA thunk, thunkIAT=0; // Thunk pointer (Pointers to functions)
	IMAGE_SECTION_HEADER *Sec_Header=0;	
	PIMAGE_IMPORT_BY_NAME pOrdinalName;  // DLL imported name.
	PIMAGE_IMPORT_DESCRIPTOR importDesc; // Pointer to the Descriptor table.
    DWORD_PTR Offset;
    DWORD_PTR VPointer;
    DWORD_PTR FirstThunkCounter;
    DWORD_PTR importsStartRVA; // Hold Starting point of the RVA.   
    bool error;
    char Addr[10]="";
    char Api[MAX_PATH]="";
    unsigned char *Data = (unsigned char*) DisasmPtr; // Point to the beginning of the File
    
	if(Address>(NTheader->OptionalHeader.ImageBase+SectionHeader->VirtualAddress+SectionHeader->PointerToRawData+SectionHeader->SizeOfRawData)){
		return false; // call way outside the selected section
	}

	// make sure the offset will stay in the imagebase space address
	if(Address<NTheader->OptionalHeader.ImageBase)
		return false;

    VPointer=Offset=Address;
	// Get ImageBase
    Offset -= NTheader->OptionalHeader.ImageBase;
	// Calculate Section Offset in file.
    Offset = (Offset - SectionHeader->VirtualAddress)+SectionHeader->PointerToRawData;
    
	// Move Pointer to the Actuall Offset
    Data+=Offset;

    // Pointer to imports table
    importsStartRVA=NTheader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if(!importsStartRVA)
        return false;

	// Get Current Section
    Sec_Header=RVAToOffset(NTheader,Sec_Header,importsStartRVA,&error);
    if(!Sec_Header)
        return false;
	
	Offset = (Sec_Header->VirtualAddress - Sec_Header->PointerToRawData);

    
	// Offset to The Section in the file
    Offset = Sec_Header->VirtualAddress - Sec_Header->PointerToRawData;
	
	// Point to the Import Desctriptor Table (DLLs)
    importDesc = (PIMAGE_IMPORT_DESCRIPTOR) (importsStartRVA - Offset + (DWORD)DisasmPtr);

    if ((BYTE*)importDesc < (BYTE*)DisasmPtr || (BYTE*)importDesc > (BYTE*)DisasmPtr + NTheader->OptionalHeader.SizeOfImage) {
		return false; // Invalid import descriptor pointer
	}
    
	if(CallApi==TRUE){
		try{
			if( ((*Data)==0xFF) && ((*(Data+1))==0x25)){ // import jump.
				wsprintf(Addr,"%02X%02X%02X%02X",(*(Data+5)),
												 (*(Data+4)),
												 (*(Data+3)),
												 (*(Data+2))
						);
				VPointer = StringToDword(Addr); // Pointer to FirstThunk RVA.
			}
			else return false;
		}
		catch(...){
			return false;
		}
   }
   else
	   if(JumpApi==FALSE && CallAddrApi==FALSE){
           return false;
	   }
                  
       // IAT
       VPointer-=NTheader->OptionalHeader.ImageBase;
       while(TRUE){ // search for the import until found
           // finished with the dlls ?
		   if ((importDesc->TimeDateStamp==0) && (importDesc->Name==0)){
               break;
		   }
           
           // read thunks (pointers to the functions).
           thunk = (PIMAGE_THUNK_DATA)importDesc->Characteristics; // OriginalFirstThunk
           thunkIAT = (PIMAGE_THUNK_DATA)importDesc->FirstThunk;   // FirstThunk
           
           // no thunk availble
		   if(thunk==0){
               thunk=thunkIAT;
		   }
           
           // lets go to the Thunk table and get ready to strip the fucntions.
           thunk = (PIMAGE_THUNK_DATA)( (PBYTE)thunk - Offset + (DWORD)DisasmPtr);
           thunkIAT = (PIMAGE_THUNK_DATA)( (PBYTE)thunkIAT - Offset + (DWORD)DisasmPtr);
                        
           // Get base thunk.
           FirstThunkCounter=importDesc->FirstThunk;
           
           while(TRUE){   
               // no more imports ?
			   if ((DWORD)thunk->u1.AddressOfData==0){
                   break;
			   }
               
               if(VPointer==FirstThunkCounter){
                   wsprintf(Api,"%s.", (PBYTE)(importDesc->Name) - Offset + (DWORD)DisasmPtr);
                   Api[StringLen(Api)-4]='\0';
                   Api[StringLen(Api)-1]='!';
                   lstrcpy(Api_Name,Api);

                   if ((WORD)thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG){
                       try{
                           wsprintf(Api,"Ordinal: %u",IMAGE_ORDINAL((DWORD)thunk->u1.Ordinal));
                           lstrcat(Api_Name,Api);
                       }
                       catch(...){
                           return false;
                       }
                   }
                   else{   
                       try{
							
							pOrdinalName = (PIMAGE_IMPORT_BY_NAME)((DWORD)thunk->u1.AddressOfData);
							pOrdinalName = (PIMAGE_IMPORT_BY_NAME)((PBYTE)pOrdinalName - Offset + (DWORD)DisasmPtr);
								
							// Protect against buffer override.
							// People can inject long names (over MAX_PATH), so make sure you protect the buffer.
							int api_len = StringLen((char *)pOrdinalName->Name);
							int api_name_len = StringLen(Api_Name);
							if( (api_len+api_name_len) >= MAX_PATH ){
								//int remove_bytes = (api_len+api_name_len)-MAX_PATH+1;
								//remove_bytes = api_len - remove_bytes;
								strncpy_s(Api,StringLen((char *)pOrdinalName->Name)+1,(char *)pOrdinalName->Name,128);
								Api[50]='\0';
							}
							else{
								wsprintf(Api,"%s",pOrdinalName->Name);
							}
								
							lstrcat(Api_Name,Api);
                       }
                       catch(...){
                           return false;
                       }
                   }
                   return true;
               }
               else{
                   FirstThunkCounter+=4;// Next DWORD.
                   thunk++;				// Read next thunk.
                   thunkIAT++;			// Read next IAT thunk.
               }                              
           }
           importDesc++;
       }
     
    return false;  // default is resolve as not found api
}

//////////////////////////////////////////////////////////////////////////
//							GetStringXRef								//
//////////////////////////////////////////////////////////////////////////
//
// The function will return the string pointed by Disasm_Address,
// If located outside the Code-Section (*most likely* in the .data section)
//
bool GetStringXRef(char *xRef,DWORD_PTR Disasm_Address)
{   
    IMAGE_SECTION_HEADER *Data_Sec_Header=0;
    IMAGE_SECTION_HEADER *Sec_Header=0;
    DWORD_PTR VPointer,Addr;
    unsigned int len,len2;
    bool error;
    unsigned char *Data = (unsigned char*) DisasmPtr,ref;
    char m_xref[2048]="";

    // Calculate Section for xReference
    VPointer=Address;
    VPointer-=NTheader->OptionalHeader.ImageBase;
    Data_Sec_Header=RVAToOffset(NTheader,Data_Sec_Header,VPointer,&error);
	if(!Data_Sec_Header || error==true){
        return false;
	}

    Addr=Disasm_Address;
    Addr-=NTheader->OptionalHeader.ImageBase;
    Sec_Header=RVAToOffset(NTheader,Sec_Header,Addr,&error);
	if(!Sec_Header || error==true){
        return false;
	}

    // xRef can't be in the code Section.
    //if(Sec_Header==Data_Sec_Header)
    //    return false;

    VPointer=(VPointer-Data_Sec_Header->VirtualAddress)+Data_Sec_Header->PointerToRawData;
    
    // Is the Address outsize of the current Address?
    if(VPointer>(Data_Sec_Header->PointerToRawData+Data_Sec_Header->SizeOfRawData))
        return false;

    // Move to the Text Reference Address.
    Data+=VPointer;

	if(*Data==0x00){
        return false;
	}
    //else
    //{
      wsprintf(m_xref,"%s",Data);

      len=StringLen(m_xref);
	  if(len<2){
          return false;
	  }

	  if(len>127){
        m_xref[50]='\0';
	  }
      
      // Check Illigal Characters.
	  len2=StringLen(m_xref);
      for(len=0;len<len2; len++){
          ref=m_xref[len];
          if(ref>127)
              return false;
      }

      strcpy_s(xRef,StringLen(m_xref)+1,m_xref);
      return true;
    //}

    //return false;
}

//////////////////////////////////////////////////////////////////////////
//							SaveDecoded									//
//////////////////////////////////////////////////////////////////////////
//
// Save Decoded Disassembly in to the Vector for later
// Use with the Virtual List View
//
void SaveDecoded(DISASSEMBLY disasm,DISASM_OPTIONS dops,DWORD Index)
{
    // disassembly class object
    CDisasmData DisasmData("","","","",0,0,""); // empty members in object  .
    char Text[128];

    DisasmDataLines.insert(DisasmDataLines.end(),DisasmData);
	assert(DisasmDataLines.size() == Index+1 && DisasmDataLines.capacity() >= Index+1);

    if(dops.ShowAddr==1){
         wsprintf(Text,"%08X",disasm.Address);
         DisasmDataLines[Index].SetAddress(Text);
    }else{
         DisasmDataLines[Index].SetAddress("");
    }
    
    if(dops.ShowOpcodes==1){
        DisasmDataLines[Index].SetCode(disasm.Opcode);
    }
	else{
        DisasmDataLines[Index].SetCode("");
	}

    if(dops.ShowAssembly==1){
        DisasmDataLines[Index].SetMnemonic(disasm.Assembly);
    }
	else{
        DisasmDataLines[Index].SetMnemonic("");
	}

    if(dops.ShowRemarks==1){
        DisasmDataLines[Index].SetComments(disasm.Remarks);
    }
	else{
        DisasmDataLines[Index].SetComments("");
	}

    DisasmDataLines[Index].SetSize((DWORD)disasm.OpcodeSize);
    DisasmDataLines[Index].SetPrefixSize((DWORD)disasm.PrefixSize);
    
}

//////////////////////////////////////////////////////////////////////////
//						CheckDataAtAddress								//
//////////////////////////////////////////////////////////////////////////
//
// Function returns TRUE if address sent is a Data In Code address.
//
bool CheckDataAtAddress(DWORD_PTR Address)
{
	DWORD Start,End;
	for(TreeMapItr itr=DataAddersses.begin();itr!=DataAddersses.end();itr++){
		Start = (*itr).first; 
		End = (*itr).second;

		if(Address>=Start && Address<=End){
			return TRUE;
		}
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
//							FirstPass									//
//////////////////////////////////////////////////////////////////////////
//
// This First Pass function tries to locate Function EntryPoints,
// And Data In Code addresses.
void FirstPass(
			DWORD_PTR Index,
			DWORD_PTR NumOfDecode,
			DWORD_PTR epAddress,
			char* Linear,
			DWORD_PTR StartSection,
			DWORD_PTR EndSection
		)
{
	MapTree CodeSegments,BranchReturnPoint,OutOfScope;
	IntArray CallStack,ReturnStack,DataAddress,ProcEntry;
	FUNCTION_INFORMATION fFunc;
	DISASSEMBLY Disasm;
	DWORD_PTR ListIndex=0;
	DWORD_PTR StartSegment=0,EndSegment=0,EntryPoint;
	char TraceAddress;
	char MessageText[256]="";
	bool TracingCall=FALSE,KeepTrace=FALSE;

	Disasm.Address = EntryPoint = epAddress;
	FlushDecoded(&Disasm);
	ProcEntry.insert(ProcEntry.end(),(DWORD)epAddress); // EP is the first Proc.
	StartSegment=epAddress;
   
	for(;Index<NumOfDecode;Index++,ListIndex++){
		// Init
		CallApi=FALSE;
		JumpApi=FALSE;
		CallAddrApi=FALSE;
		PushString=FALSE;
		FunctionsEntryPoint=FALSE;
		DirectJmp=FALSE;
		Address=0;
		
		Decode(&Disasm,Linear,&Index); // Decode Disasm Address

		if(Disasm.CodeFlow.Ret==TRUE){ // RET instruction.
			EndSegment = Disasm.Address;
			CodeSegments.insert(MapTreeAdd((DWORD)StartSegment,(DWORD)EndSegment)); // Mark Code_Block.
           
			if( ProcEntry.size()!=0 ){
				fFunc.FunctionStart=ProcEntry.back();
				fFunc.FunctionEnd=Disasm.Address;
				if(fFunc.FunctionStart!=epAddress){
					wsprintf(fFunc.FunctionName,"Proc_%08X",fFunc.FunctionStart);
				}
				else{
					strcpy_s(fFunc.FunctionName,"");
				}
				AddFunction(fFunc);
			}

			if(ProcEntry.size()!=0){
				ProcEntry.pop_back(); // Remove entry.
			}
           
			if(ReturnStack.size()!=0){
				Index = ReturnStack.back();
				--Index;
				ReturnStack.pop_back();
			}

			if(CallStack.size()!=0){
				Disasm.Address = CallStack.back();
				CallStack.pop_back();
			}

			StartSegment=Disasm.Address;
			FlushDecoded(&Disasm); // Empty Buffer.
			continue;
		}

		// Locate a API Branch.
		if(CallApi==TRUE || JumpApi==TRUE || CallAddrApi==TRUE){
			if(GetAPIName(MessageText)==TRUE){
				if(CallApi==TRUE || CallAddrApi==TRUE){
					Disasm.CodeFlow.Call=FALSE;
					if(
						lstrcmp(MessageText,"KERNEL32!ExitProcess")==0 ||
						lstrcmp(MessageText,"kernel32!ExitProcess")==0
					)
					{
						EndSegment = Disasm.Address;
						CodeSegments.insert(MapTreeAdd((DWORD)StartSegment,(DWORD)EndSegment)); // Mark Code_Block.
						if(ProcEntry.size()!=0){
							fFunc.FunctionStart=ProcEntry.back();
							fFunc.FunctionEnd=Disasm.Address;
							strcpy_s(fFunc.FunctionName,"");
							AddFunction(fFunc);
						}
						break;
					}
				}
				else{
					if(JumpApi==TRUE){
						Disasm.CodeFlow.Jump=FALSE; // Disable jump branch on api branch.
					}
				}
			}
		}

		// Locate a branch.
		if(Disasm.CodeFlow.Call == TRUE || Disasm.CodeFlow.Jump == TRUE){
			// Branch Destination is in codeSection.
			if( ( Address >= StartSection ) && ( Address <= EndSection ) ){ 
				TraceAddress=TRUE;
				EndSegment=Disasm.Address;
				// Mark Code_Block.
				CodeSegments.insert(MapTreeAdd((DWORD)StartSegment,(DWORD)EndSegment));
				TreeMapItr itr = CodeSegments.find((DWORD)Address);
				if(itr!=CodeSegments.end()){
					// Are we going back to already waled address?
					if( (Address >= (*itr).first) && (Address <= (*itr).second)){
						TraceAddress=FALSE;  
						StartSegment=EndSegment;
					}
				}

				if(TraceAddress==TRUE){
					if(Disasm.CodeFlow.Call == TRUE){
						StartSegment=Address;
						ProcEntry.insert(ProcEntry.end(),(DWORD)Address);
						EntryPoint=Address;
						CallStack.insert(CallStack.end(),(DWORD)(Disasm.Address+Disasm.OpcodeSize+Disasm.PrefixSize));
						ReturnStack.insert(ReturnStack.end(),(DWORD)Index+1);
						TracingCall=TRUE;
					}
                   
					if(Disasm.CodeFlow.Jump==TRUE || Disasm.CodeFlow.Call == TRUE){   
						// Mark address which may be Data.
						if(DirectJmp==TRUE){
							DataAddress.insert(DataAddress.end(),(DWORD)(Disasm.Address+Disasm.OpcodeSize+Disasm.PrefixSize));
						}
						else{
							if(Disasm.CodeFlow.Jump==TRUE){
								BranchReturnPoint.insert(MapTreeAdd((DWORD)(Disasm.Address+Disasm.OpcodeSize+Disasm.PrefixSize),(DWORD)Index+1));
							}
						}
                       
						StartSegment=Address;
						Index += ( (Address - StartSection) - Index-1);
						Disasm.Address = Address;
						FlushDecoded(&Disasm);
						continue;
					}
				}
			}
		}
              
		Disasm.Address+=Disasm.OpcodeSize+Disasm.PrefixSize;
		FlushDecoded(&Disasm);
	}//end of for.

	// Build 'Data' Segments.
	for(int i=0;i<DataAddress.size();i++){
		Address=DataAddress[i];
		TreeMapItr itr = CodeSegments.find((DWORD)Address);

		if(itr!=CodeSegments.end()){
			CodeSegments.erase((DWORD)Address);
		}
		else{
			itr = CodeSegments.upper_bound((DWORD)Address);
			if(itr!=CodeSegments.end()){
				Index = (*itr).first;
				Index-=Address;
				Index--;
				AddNewData((DWORD)Address,(DWORD)(Address+Index));
			}
		}
	}
} //End of FirstPass function.