#include "tools.h"
#include <tchar.h>

typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);

LPFN_ISWOW64PROCESS fnIsWow64Process;

// link to token table
extern struct commands { /* keyword lookup table */
  char command[20];
  char tok;
} table[];


/***************************************************************************************

 Dynamic Load of the DLL and all function pointer

****************************************************************************************/

//
// Function: GetFunctionAdress
// Parameter: instance of DLL
// ret value: true if OK false if pointer not vallid
//
// load the function pointer from the DLL spec. by handle
//

int GetFunctionAdress(HINSTANCE h_module)
{
  //Lade alle Funktionen
  if(h_module == NULL)
   return false;

  g_CAN_Initialize = (PCAN_Initialize) GetProcAddress(h_module, "CAN_Initialize");
  if(g_CAN_Initialize == NULL)
   return false;

  g_CAN_Uninitialize = (PCAN_Uninitialize) GetProcAddress(h_module, "CAN_Uninitialize");
  if(g_CAN_Uninitialize == NULL)
   return false;

  g_CAN_Reset = (PCAN_Reset) GetProcAddress(h_module, "CAN_Reset");
  if(g_CAN_Reset == NULL)
   return false;

  g_CAN_GetStatus = (PCAN_GetStatus) GetProcAddress(h_module, "CAN_GetStatus");
  if(g_CAN_GetStatus == NULL)
   return false;

  g_CAN_Read = (PCAN_Read) GetProcAddress(h_module, "CAN_Read");
  if(g_CAN_Read == NULL)
   return false;

  g_CAN_Write = (PCAN_Write) GetProcAddress(h_module, "CAN_Write");
  if(g_CAN_Write == NULL)
   return false;

  g_CAN_FilterMessages = (PCAN_FilterMessages) GetProcAddress(h_module, "CAN_FilterMessages");
  if(g_CAN_FilterMessages == NULL)
   return false;

  g_CAN_GetValue = (PCAN_GetValue) GetProcAddress(h_module, "CAN_GetValue");
  if(g_CAN_GetValue == NULL)
   return false;

  g_CAN_SetValue = (PCAN_SetValue) GetProcAddress(h_module, "CAN_SetValue");
  if(g_CAN_SetValue == NULL)
   return false;

  g_CAN_GetErrorText = (PCAN_GetErrorText) GetProcAddress(h_module, "CAN_GetErrorText");
  if(g_CAN_GetErrorText == NULL)
   return false;

  return true;
}

//
// Function: Load DLL
// Parameter: none
// ret value: true if OK, false if DLL not found or can not open
//
// load the DLL and get function pointers
//

BOOL LoadDLL()
{
	if(g_i_DLL==NULL)
	{
		g_i_DLL = LoadLibrary("PCANBasic");
		if(g_i_DLL == NULL)
		{
			printf("ERROR: can not load pcanbasic.dll\n");
			return false;
		}	
		else
		{
			#ifdef _DEBUG
				printf("DLL Handle: 0x%x\n",g_i_DLL);
			#endif
			if(GetFunctionAdress( g_i_DLL )==true)
			{
				printf("Load function adress for pcan_basic.dll\n");
			}
			else
			{
				printf("ERROR: can not load Function Adress\n");
				return false;
			}
		}
	}
	return true;
}

//
// Function: Unload DLL
// Parameter: none
// ret value: true if OK 
//
// unload the DLL and free all pointers
//
BOOL UnloadDLL()
{
 if(g_i_DLL)
 {
  FreeLibrary(g_i_DLL);
  g_CAN_Initialize = NULL;
  g_CAN_Uninitialize = NULL;
  g_CAN_Reset = NULL;
  g_CAN_GetStatus = NULL;
  g_CAN_Read = NULL;
  g_CAN_Write = NULL;
  g_CAN_FilterMessages = NULL;
  g_CAN_GetValue = NULL;
  g_CAN_SetValue = NULL;
  g_CAN_GetErrorText = NULL;
  return true;
 }
 return false;
}



// Convert a HEX String (0x0000) to a Integer Var
long htoi(char s[])
{
    int i = 0; /* Iterate over s */
    long n = 0; /* Built up number */
	long t; //Temp
     /* Remove "0x" or "0X" */
    if ( s[0] == '0' && s[1] == 'x' || s[1] == 'X' )
        i = 2;
    while ( s[i] != '\0' )
    {
        if ( s[i] >= 'A' && s[i] <= 'F' )
            t = s[i] - 'A' + 10;
        else if ( s[i] >= 'a' && s[i] <= 'f' )
            t = s[i] - 'a' + 10;
        else if ( s[i] >= '0' && s[i] <= '9' )
            t = s[i] - '0';
        else
            return n;
         n = 16 * n + t;
        ++i;
    }
     return n;
}

void print_help_baudrate()
{
	printf("Baudrate, use:\n");
	printf("0x0014\tfor MBit/s\n");
	printf("0x0016\tfor 800 kBit/s\n");
	printf("0x001C\tfor 500 kBit/s\n");
	printf("0x011C\tfor 250 kBit/s\n");
	printf("0x031C\tfor 125 kBit/s\n");
	printf("0x432F\tfor 100 kBit/s\n");
	printf("0xC34E\tfor 95,238 kBit/s\n");
	printf("0x4B14\tfor 83,333 kBit/s\n");
	printf("0x472F\tfor 50 kBit/s\n");
	printf("0x1414\tfor 47,619 kBit/s\n");
	printf("0x1D14\tfor 33,333 kBit/s\n");
	printf("0x532F\tfor 20 kBit/s\n");
	printf("0x672F\tfor 10 kBit/s\n");
	printf("0x7F7F\tfor 5 kBit/s\n");
	printf("or use your own BTR0/BTR1 values.\n");
}

void print_help_devicetype()
{
	printf("DeviceType, use:\n");
	printf("0x51\tfor PCAN-USB interface, channel 1\n");
	printf("0x5n\tfor PCAN-USB interface, channel n\n");
	printf("n could be from 1 to 8\n\n");
	printf("0x41\tfor PCAN-PCI interface, channel 1\n");
	printf("0x4n\tfor PCAN-PCI interface, channel n\n");
	printf("n could be from 1 to 8\n\n");
	printf("0x21\tfor PCAN-ISA interface, channel 1\n");
	printf("0x2n\tfor PCAN-ISA interface, channel n\n");
	printf("n could be from 1 to 8\n\n");
	printf("0x61\tfor PCAN-PC Card interface, channel 1\n");
	printf("0x62\tfor PCAN-PC Card interface, channel 2\n\n");
	printf("0x31\tfor PCAN-Dongle LPT Interface, channel 1\n");
}

void print_help_hwtype()
{
	printf("HardwareType ,only for NON PLUG&PLAY, use:\n");
	printf("0x01\tfor PCAN-ISA 82C200\n");
	printf("0x09\tfor PCAN-ISA SJA1000\n");
	printf("0x04\tfor PHYTEC ISA\n");
	printf("0x02\tfor PCAN-Dongle 82C200\n");
	printf("0x03\tfor PCAN-Dongle EPP 82C200\n");
	printf("0x05\tfor PCAN-Dongle SJA1000\n");
	printf("0x06\tfor PCAN-Dongle EPP SJA1000\n");
}

void print_help_ioport()
{
	printf("IO Port Adress ,only for NON PLUG&PLAY\n");
	printf("Example: 888 (0x387) for parallel Port Dongle\n");
	printf("Example: 768 (0x300) for ISA CAN card\n");
}

void print_help_int()
{
	printf("Interrupt number, only for NON PLUG&PLAY\n");
	printf("Example: 7 for parallel Port Dongle\n");
	printf("Example: 10 for ISA based CAN cards\n");
}


void print_help()
{
  print_help_baudrate();
  printf("\n");
  print_help_devicetype();
}

void print_help_syntax()
{
 int i=0;
 printf("\nLimits and Syntax for then CAN Basic Interpreter CANBas:\n");
 printf("Prog. Size: max. 200.000 byte\n");
 printf("used Labels: max. 100\n");
 printf("Label len max. 20 char\n");
 printf("ForNext: max. 100\n"); 	
 printf("GoSubs: max. 100\n");
 printf("VARs: only 26, ""A"" to ""Z"" single char, stored in an signed long type\n");
 printf("no float values supported\n");
 printf("no strings supported\n\n");
 printf("available keywords:\n");
 while(strlen(table[i].command)>0)
	 printf("%s\t",table[i++].command);
 printf("\n");
}

void print_howto()
{
 printf("usage: CANBas <filename> <DeviceType> <Baudrate> [HWType] [IOPort] [INT]\n");
 printf("[] Paramas are optional for non PNP Devices\n");
 printf("Parameters could be placed as HEX (0x) or Decimal values.\n");
 printf("For more Information type: CANBas HELP <DeviceType|HWType|IOPort|INT>\n");
 printf("For language Information type: CANBas SYNTAX\n");
 printf("For Sample: CANBas SAMPLE\n");
}
