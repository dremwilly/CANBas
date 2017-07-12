// CANBAS 
// A tiny BASIC interpreter for scripting with the PCAN-Basic API
//
// Version 1.0.x
// Autor U.Wilhelm
//
// Based on the C-Interpreter from H.Schildt
// The program was originally published in Dr. Dobb's Journal in August, 1989 entitled "Building your own C interpreter"
// http://www.drdobbs.com/cpp/building-your-own-c-interpreter/184408184 
//
// New features: 
// change syntax to run Basic Keywords, use console features like goto xy, set color etc.
// add Remarks, add CAN Handling, use Windows Messages, add Wait and RND numbers, 
// use a Staus Line etc.
//
// This is Software is at is. Use it or change it for your need...
// I will not do any support - debug it and optimize it
//
// PCAN is a registered Trademark of PEAK-System Technik GmbH
// The used Sofwtware API for CAN is part of the PEAK-System Software API PCANBasic. 
// The C Header Files and the Documentation could be download from www.peak-system.com 
// Every PEAK-System CAN Interface have a license to use the Interface DLL (pcanbasic.dll) 
// The DLLs (32 and/or 64Bit) are not part of my packages. 
// If you own a PEAK-System CAN Interface you also own a license of the API.
//

#include "stdio.h"
#include "setjmp.h"
#include "math.h"
#include "ctype.h"
#include "stdlib.h"
#include <windows.h>
#include <Wincon.h>

#include "tools.h"
#include "pcanbasic.h"

#define true 1
#define false 0

// #define CANSIMU

// Params of basic machine
#define NUM_LAB 100
#define LAB_LEN 20 
#define FOR_NEST 100
#define SUB_NEST 100
#define PROG_SIZE 200000

#define TABLEN	8

#define DELIMITER  1
#define VARIABLE  2
#define NUMBER    3
#define COMMAND   4
#define STRING	  5
#define QUOTE	  6
#define REMARK	  7

// tokens
#define PRINT 1
#define INPUT 2
#define IF    3
#define THEN  4
#define FOR   5
#define NEXT  6
#define TO    7
#define GOTO  8
#define EOL   9
#define FINISHED  10
#define GOSUB 11
#define RETURN 12
#define END 13
#define WAIT 14
#define MESSAGEBOX 15
#define CAN_RESET 16
#define CAN_GETSTATUS 17
#define CAN_WRITE 18
#define CAN_READ 19
#define CAN_FILTERMESSAGES 20
#define CAN_GETVALUE 21
#define CAN_SETVALUE 22
#define CAN_GETERRORTEXT 23
#define CAN_CLEARQUEUE 24
#define GOTOXY 25
#define SETCOLOR 26
#define CLRSCRN 27
#define RANDOM 28
#define CAN_WAITID 29
#define STATUSLINE 30
#define TICKCOUNT 31


// name of CANN DLL to load
char g_LibFileName[] = "PCANBasic";

// StdOut Handler
HANDLE g_hStdOut;
//Console Buffer Info
CONSOLE_SCREEN_BUFFER_INFO g_CSB_Info;
//Coord of Status Line
COORD g_coordStatus;
DWORD StatusCounter,StartStatusCounter;
BOOL g_statusline=false;
/*
typedef struct _CONSOLE_SCREEN_BUFFER_INFO {
  COORD      dwSize;
  COORD      dwCursorPosition;
  WORD       wAttributes;
  SMALL_RECT srWindow;
  COORD      dwMaximumWindowSize;
} CONSOLE_SCREEN_BUFFER_INFO;
*/

// Pointer to code
char *prog;  /* holds expression to be analyzed */
jmp_buf e_buf; /* hold environment for longjmp() */

// Store our 26 VARs
long variables[26]= {    /* 26 user variables,  A-Z */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0
};

// The keyword lookup table
struct commands { 
  char command[20];
  char tok;
} table[] = { /* Commands must be entered lowercase in this table. */
  "print", PRINT, 
  "input", INPUT,
  "if", IF,
  "then", THEN,
  "goto", GOTO,
  "for", FOR,
  "next", NEXT,
  "to", TO,
  "gosub", GOSUB,
  "return", RETURN,
  "end", END,
  "wait", WAIT, 
  "messagebox", MESSAGEBOX,
  "can_reset", CAN_RESET,
  "can_getstatus",CAN_GETSTATUS,
  "can_write", CAN_WRITE,
  "can_read", CAN_READ,
  "can_filtermessages",CAN_FILTERMESSAGES,
  "can_getvalue", CAN_GETVALUE,
  "can_setvalue",CAN_SETVALUE,
  "can_geterrortext",CAN_GETERRORTEXT,
  "can_waitid",CAN_WAITID,
  "can_clearqueue",CAN_CLEARQUEUE,
  "gotoxy",GOTOXY,
  "setcolor",SETCOLOR,
  "clrscrn",CLRSCRN,
  "random",RANDOM,
  "statusline", STATUSLINE,
  "tickcount", TICKCOUNT,
  "", END  /* mark end of table */
};

// Token 
char token[80];
char token_type,tok;

// a LABLE storing struct
struct label {
  char name[LAB_LEN];
  char *p;  /* points to place to go in source file*/
};
// Label Table
struct label label_table[NUM_LAB];

// The STACK
struct for_stack {
  int var; /* counter variable */
  long target;  /* target value */
  char *loc;
} fstack[FOR_NEST]; /* stack for FOR/NEXT loop */

struct for_stack fpop();

char *gstack[SUB_NEST];	/* stack for gosub */

int ftos;  /* index to top of FOR stack */
int gtos;  /* index to top of GOSUB stack */

int global_break=false;
BOOL g_CANSIMU=false;

// Extern Functions - from Tools.c
extern BOOL LoadDLL();
extern BOOL UnloadDLL();
extern int GetFunctionAdress(HINSTANCE h_module);

extern long htoi(char s[]);
extern print_howto(),print_help(),print_help_devicetype(),print_help_baudrate();
extern print_help_hwtype(),print_help_ioport(),print_help_int(),print_help_syntax();

// Intern Function
void print(), scan_labels(), find_eol(), exec_goto();
void exec_if(), exec_for(), next(), fpush(), input();
void gosub(), greturn(), gpush(), label_init();
void serror(), get_exp(), putback();
void level2(), level3(), level4(), level5(), level6(), primitive();
void unary(), arith(), wait(), basic_messagebox();
char *find_label(), *gpop();
void assignment(),gotoxy(),setcolor(),clrscrn(),random(),can_waitid(), tickcount();
void can_reset(),can_getstatus(),can_write(),can_read(),can_setvalue(),can_getvalue(),can_geterrortext(),can_filtermessages(),can_clearqueue(),statusline();
int iswhite(),isdelim(),look_up(),find_var();
unsigned long load_program(char *p,char *fname);
int CheckParams(char *devicehandle, char *baudrate, char *hwtype, char *ioport, char *interrupt);
int ConsoleHandler(DWORD CEvent);


// The Main....
main(argc, argv)
int argc;
char *argv[];
{
  char version[20]= "1.0.1";
  char buffer[255];
  DWORD myTickCounter,StartTickCounter;
  char *p_buf;
  char *t;
  unsigned long filesize;
  int i, ret;

  
  if(SetConsoleCtrlHandler( (PHANDLER_ROUTINE)ConsoleHandler,TRUE)==FALSE)  { // unable to install handler...display message to the user
        printf("CANBas -> Unable to install Ctrl-Handler!\n");
        return -1; }

  SetConsoleTitle("CANBAS");
  g_hStdOut=GetStdHandle(STD_OUTPUT_HANDLE );
  g_coordStatus.X=0;
  g_coordStatus.Y=24;

  //Handle all the basic stuff to be sure that the command line paramaters are OK
  printf("CANBas - a simple CAN scripting language\n");
  printf("using the PCAN-Basic Interface DLL for PEAK-System CAN Adapters.\n");
  printf("PCAN is a registered Trademark of PEAK-System GmbH.\n");
  #ifndef X64
	printf("32Bit Version: %s \n",version);   
  #else
    printf("64Bit Version: %s \n",version);
  #endif

  if(argc<2) {
	print_howto();
	exit(1);
  }

  if(!stricmp(argv[1],"HELP"))
  {
	if(argv[2]==NULL)
		print_help();
	else
		if(!stricmp(argv[2],"DEVICETYPE"))
			print_help_devicetype();
		else
			if(!stricmp(argv[2],"BAUDRATE"))
				print_help_baudrate();
			else
				if(!stricmp(argv[2],"HWTYPE"))
					print_help_hwtype();
				else
					if(!stricmp(argv[2],"IOPORT"))
						print_help_ioport();
					else
						if(!stricmp(argv[2],"INT"))
							print_help_int();
						else
							print_howto();
	exit(1);
  }
  else{
	  if(!stricmp(toupper(argv[1]),"SYNTAX"))
	  {
		print_help_syntax();
		exit(1);
	  }
	  else{
		if(!stricmp(toupper(argv[1]),"SAMPLE"))
		{
			printf("Sample:\nuse basic file TestECU.bas with a USB Adapter Channel 1 running with 500k\n");
			printf("\nCANBas TestECU.bas 0x51 0x001c\n");
			exit(1);
		 }
	  }
  }

  // Check if DeviceType and Baudrate are valid PCANBasic Parametes
  // For that we need to see if the Arguments counter is >3
  if(argc<4)
  {
	  if(argc==3 && !stricmp(argv[2],"SIMU"))
	  {
		  printf("Run ins SIMU mode - ignore CAN");
		  g_CANSIMU=true;
	  }
	  else
	  {
		printf("missing arguments...\n");
		print_howto();
		exit(1);
	  }
  }
  else
  {
	  if(!CheckParams(argv[2], argv[3],argv[4], argv[5], argv[6])) 
	  exit(1);
  }

  // Allocate the memory for the program
  if(!(p_buf=(char *) malloc(PROG_SIZE+1))) {
    printf("allocation failure - out of memory!\n");
    exit(1);
  }
  // Set Console Windows Name to the loaded program file
  sprintf(buffer,"CANBas running: %s",argv[1]);
  SetConsoleTitle(buffer);

  // load the program to execute...
  filesize=load_program(p_buf,argv[1]);
  if(filesize<=0) exit(1);
  p_buf[filesize]='\0';

  // used for check the "real" programm code after remove the REM (#) 
  // printf("%s\n",p_buf);
  
  
  if(setjmp(e_buf)) exit(1); /* initialize the long jump buffer */

  prog = p_buf;
  scan_labels(); /* find the labels in the program */
  ftos = 0; /* initialize the FOR stack index */
  gtos = 0; /* initialize the GOSUB stack index */

  if(!g_CANSIMU)
  {
	if(!LoadDLL())
		  exit(1);
	#ifndef CANSIMU
	CANStatus = g_CAN_Initialize(CANChannel, CANBaudrate, CANHwType, CANIOPort, CANInterrupt);
	// If CANSatatus is OK, we could use the CAN Cahnnel CANChannel for all other functions...
	if(CANStatus!=PCAN_ERROR_OK)
	{
	  printf("Error while initialize CAN Interface: %d\n", CANStatus);
	  exit(1);
	}
	#endif
  }

  // Hole Startzeit
  #ifdef _DEBUG
	StartTickCounter=GetTickCount();
  #endif

  StartStatusCounter=0; 
  // Here we start...
  do {

	//Here we update the status line
	if(g_statusline)
	{
		StatusCounter=GetTickCount();  
		if(StatusCounter-StartStatusCounter>1000)
		{
			//Get the actually Info of Cursor
			GetConsoleScreenBufferInfo(g_hStdOut,&g_CSB_Info);
			SetConsoleTextAttribute(g_hStdOut,FOREGROUND_INTENSITY | FOREGROUND_GREEN);
			SetConsoleCursorPosition(g_hStdOut,g_coordStatus);
				if(!g_CANSIMU)
				{
					ret = g_CAN_GetStatus(CANChannel);
					g_CAN_GetErrorText(ret,0x00,buffer);
					printf("used CAN Ch. 0x%02x, BTR0/BTR1: 0x%04x State: %s\tUpdate:%u",CANChannel, CANBaudrate,buffer, StatusCounter);
				}else
					printf("used CAN Ch. SIMU, BTR0/BTR1: SIMU State: SIMU\tUpdate:%u", StatusCounter);
			
			SetConsoleCursorPosition(g_hStdOut,g_CSB_Info.dwCursorPosition);
			SetConsoleTextAttribute(g_hStdOut,g_CSB_Info.wAttributes);
			StartStatusCounter=GetTickCount();  
		}
	}

    token_type = get_token();
    /* check for assignment statement */
    if(token_type==VARIABLE) {
      putback(); /* return the var to the input stream */
      assignment(); /* must be assignment statement */
    }
    else /* is command */
	{
      switch(tok) {
        case PRINT:
			print();
  		break;

        case GOTO:
			exec_goto();
		break;
	
		case IF:
			exec_if();
		break;
	
		case FOR:
			exec_for();
		break;
	
		case NEXT:
			next();
		break;
  	
		case INPUT:
			input();
		break;
    
		case GOSUB:
			gosub();
		break;
	
		case RETURN:
			greturn();
		break;

		case WAIT:
			wait();
		break;

		case MESSAGEBOX:
			basic_messagebox();
		break;

		case CAN_RESET:
			can_reset();
		break;

		case CAN_GETSTATUS:
			can_getstatus();
		break;

		case CAN_WRITE:
			can_write();
		break;

		case CAN_READ:
			can_read();
		break;
		
		case CAN_FILTERMESSAGES:
			can_filtermessages();
		break;

		case CAN_GETVALUE:
			can_getvalue();
		break;

		case CAN_SETVALUE:
			can_setvalue();
		break;

		case CAN_GETERRORTEXT:
			can_geterrortext();
		break;

		case CAN_WAITID:
			can_waitid();
		break;

		case CAN_CLEARQUEUE:
			can_clearqueue();
		break;

		case GOTOXY:
			gotoxy();
		break;

		case SETCOLOR:
			setcolor();
		break;

		case CLRSCRN:
			clrscrn();
		break;

		case RANDOM:
			random();
		break;

		case STATUSLINE:
			statusline();
		break;

		case TICKCOUNT:
			tickcount();
		break;
		
		case END:
			tok = FINISHED;
      }
	}
  } while (tok != FINISHED && global_break!=true);

  //End of Programm...
  #ifdef _DEBUG
	myTickCounter=GetTickCount();
	printf("used ticks: %d\n", myTickCounter-StartTickCounter);
  #endif

  if(!g_CANSIMU)
  {
	#ifndef CANSIMU
		g_CAN_Uninitialize(CANChannel);
	#endif
	UnloadDLL();
  }
  exit;
}

// Load a program to buffer - OLD VERSION
/*
unsigned long load_program_old(char *p,char *fname)
{
  FILE *fp;
  char temp_c;
  unsigned long i=0;
  // Open File
  if(!(fp=fopen(fname, "rb"))) return 0;
  i = 0;
  do{
	temp_c=getc(fp);
    *p = temp_c; // copy to prog buffer
    p++; i++; // increment prog buffer pointer and size counter
  }while(!feof(fp) && i<PROG_SIZE); // until End of File or Buffer reached
  i--; // to return right size...
  fclose(fp); // close file
  return i; //return real size in Bytes
}
*/


// Load file to buffer - remove the # lines
unsigned long load_program(char *p,char *fname)
{
  FILE *fp;
  char temp_c;
  unsigned long filesize=0;
  int j,i;
  char LineBuffer[512];
  // Open File
  if(!(fp=fopen(fname, "rb"))) return 0;
  j,i = 0;
  // remove REM Lines (#)
  // and read the rest into buffer
  do{
      //read ONE single line 
	  i=0;
	  do{
		  temp_c=getc(fp);
		  LineBuffer[i++]=temp_c;
	  }while(!feof(fp) && (temp_c!=0x0a) );
	 // if(!feof(fp) && i>0) // not end of file?
	  if(i>0) // not end of file?
	  {
		LineBuffer[i]='\0'; // add a String delimiter at the end of the line
		// Here we have a single Line in LineBuffer
		for(j=0;j<strlen(LineBuffer);j++)
		{
			if(LineBuffer[j]==' ' || LineBuffer[j]=='\t' || LineBuffer[j]==0x0a || LineBuffer[j]==0x0d)
				j++;
			else
			{
				if(LineBuffer[j]=='#')
				{
					//Start of REM Block
					//LineBuffer[0]='\0';
					break;
				}else //any other Char - > it´s code
				{
					//	break;	
					//Now we have one line in the buffer
					memcpy(p,LineBuffer,strlen(LineBuffer));
					// printf("%s",LineBuffer);
					p=p+strlen(LineBuffer); // copy this line to the gloabl programm buffer
					filesize=filesize+strlen(LineBuffer); //add the size of the added line to the char counter of the gloabl programm buffer
					break;
				}
			}
		}
		/*
		//Now we have one line in the buffer
		memcpy(p,LineBuffer,strlen(LineBuffer));
		// printf("%s",LineBuffer);
		p=p+strlen(LineBuffer); // copy this line to the gloabl programm buffer
		filesize=filesize+strlen(LineBuffer); //add the size of the added line to the char counter of the gloabl programm buffer
		i=0;// reset i to zero - new line
		*/
	  }
  }while(!feof(fp) && i<PROG_SIZE); // until End of File or Buffer reached
  fclose(fp); // close file
  return filesize-1; //return real size in Bytes
}

/* Assign a variable a value. */
void assignment()
{
  long var, value;

  /* get the variable name */
  get_token();
  if(!isalpha(*token)) {
    serror(4);
    return;
  }

  var = toupper(*token)-'A';
 
  /* get the equals sign */
  get_token();
  if(*token!='=') {
    serror(3);
    return;
  }

  /* get the value to assign to var */
  get_exp(&value);

  /* assign the value */
  variables[var] = value;
}


/* wait for xx ms */
void wait()
{
  long answer;

  get_token(); /* get next list item */
  if(tok==EOL || tok==FINISHED)
	serror(0); 
  if(token_type==QUOTE) /* is string */
     serror(0);
  else { /* is expression */
    putback();
    get_exp(&answer);
    get_token();
    Sleep(answer);
  }
  if(tok!=EOL && tok!=FINISHED) serror(0); 
}

/* Execute a simple version of the BASIC PRINT statement */
void print()
{
  long answer;
  int len=0, spaces;
  char last_delim;

  do {
    get_token(); /* get next list item */
    if(tok==EOL || tok==FINISHED) break;
    if(token_type==QUOTE) { /* is string */
      printf(token);
      len += strlen(token);
      get_token();
    }
    else { /* is expression */
      putback();
      get_exp(&answer);
      get_token();
      len += printf("%d", answer);
    }
    last_delim = *token; 

    if(*token==';') {
      /* compute number of spaces to move to next tab */
      spaces = TABLEN - (len % TABLEN); 
      len += spaces; /* add in the tabbing position */
      while(spaces) { 
	printf(" ");
        spaces--;
      }
    }
    else if(*token==',') /* do nothing */;
    else if(tok!=EOL && tok!=FINISHED) serror(0); 
  } while (*token==';' || *token==',');

  if(tok==EOL || tok==FINISHED) {
    if(last_delim != ';' && last_delim!=',') printf("\n");
  }
  else serror(0); /* error is not , or ; */

}

/* Execute a simple version of the BASIC PRINT statement */
void basic_messagebox()
{
  long answer;
  int len=0, spaces;
  char last_delim;
  char printbuffer[1024];
  char buffer[255];
  sprintf(printbuffer,"");

  do {
    get_token(); /* get next list item */
    if(tok==EOL || tok==FINISHED) break;
    if(token_type==QUOTE) { /* is string */
      strcat(printbuffer,token);
      len += strlen(token);
      get_token();
    }
    else { /* is expression */
      putback();
      get_exp(&answer);
      get_token();
      len += sprintf(buffer,"%d", answer);
	  strcat(printbuffer, buffer);
    }
    last_delim = *token; 

    if(*token==';') {
      /* compute number of spaces to move to next tab */
      spaces = 8 - (len % 8); 
      len += spaces; /* add in the tabbing position */
      while(spaces) { 
	    strcat(printbuffer," ");
        spaces--;
      }
    }
    else if(*token==',') /* do nothing */;
    else if(tok!=EOL && tok!=FINISHED) serror(0); 
  } while (*token==';' || *token==',');

  if(tok==EOL || tok==FINISHED) {
    if(last_delim != ';' && last_delim!=',') strcat(printbuffer,"\n");
  }
  else serror(0); /* error is not , or ; */
  MessageBox(NULL,printbuffer, "CANBAS Info", MB_ICONINFORMATION);
}


/* Find all labels. */
void scan_labels()
{
  int addr;
  char *temp;

  label_init();  /* zero all labels */
  temp = prog;   /* save pointer to top of program */

  /* if the first token in the file is a label */
  get_token();
  if(token_type==NUMBER) {
    strcpy(label_table[0].name,token);
    label_table[0].p=prog;
  }

  find_eol();
  do {     
    get_token();
    if(token_type==NUMBER) {
      addr = get_next_label(token);
      if(addr==-1 || addr==-2) {
          (addr==-1) ?serror(5):serror(6);
      }
      strcpy(label_table[addr].name, token);
      label_table[addr].p = prog;  /* current point in program */
    }
    /* if not on a blank line, find next line */
    if(tok!=EOL) find_eol();
  } while(tok!=FINISHED);
  prog = temp;  /* restore to original */
}

/* Find the start of the next line. */
void find_eol()
{
  while(*prog!='\n'  && *prog!='\0') ++prog;
  if(*prog) prog++;
}

/* Return index of next free position in label array. 
   A -1 is returned if the array is full.
   A -2 is returned when duplicate label is found.
*/
get_next_label(s)
char *s;
{
  register int t;

  for(t=0;t<NUM_LAB;++t) {
    if(label_table[t].name[0]==0) return t;
    if(!strcmp(label_table[t].name,s)) return -2; /* dup */
  }

  return -1;
}

/* Find location of given label.  A null is returned if
   label is not found; otherwise a pointer to the position
   of the label is returned.
*/
char *find_label(s)
char *s;
{
  register int t;

  for(t=0; t<NUM_LAB; ++t) 
    if(!strcmp(label_table[t].name,s)) return label_table[t].p;
  return '\0'; /* error condition */
}

/* Execute a GOTO statement. */
void exec_goto()
{

  char *loc;

  get_token(); /* get label to go to */
  /* find the location of the label */
  loc = find_label(token);
  if(loc=='\0')
    serror(7); /* label not defined */

  else prog=loc;  /* start program running at that loc */
}

/* Initialize the array that holds the labels. 
   By convention, a null label name indicates that
   array position is unused.
*/
void label_init()
{
  register int t;

  for(t=0; t<NUM_LAB; ++t) label_table[t].name[0]='\0';
}

/* Execute an IF statement. */
void exec_if()
{
  int x , y, cond;
  char op;

  get_exp(&x); /* get left expression */

  get_token(); /* get the operator */
  if(!strchr("=<>", *token)) {
    serror(0); /* not a legal operator */
    return;
  }
  op=*token;

  get_exp(&y); /* get right expression */

  /* determine the outcome */
  cond = 0;
  switch(op) {
    case '<':
      if(x<y) cond=1;
      break;
    case '>':
      if(x>y) cond=1;
      break;
    case '=':
      if(x==y) cond=1;
      break;
  }
  if(cond) { /* is true so process target of IF */
    get_token();
    if(tok!=THEN) {
      serror(8);
      return;
    }/* else program execution starts on next line */
  }
  else find_eol(); /* find start of next line */
}

/* Execute a FOR loop. */
void exec_for()
{
  struct for_stack i;
  int value;

  get_token(); /* read the control variable */
  if(!isalpha(*token)) {
    serror(4);
    return;
  }

  i.var=toupper(*token)-'A'; /* save its index */

  get_token(); /* read the equals sign */
  if(*token!='=') {
    serror(3);
    return;
  }

  get_exp(&value); /* get initial value */

  variables[i.var]=value;

  get_token();
  if(tok!=TO) serror(9); /* read and discard the TO */

  get_exp(&i.target); /* get target value */

  /* if loop can execute at least once, push info on stack */
  if(value>=variables[i.var]) { 
    i.loc = prog;
    fpush(i);
  }
  else  /* otherwise, skip loop code altogether */
    while(tok!=NEXT) get_token();
}

/* Execute a NEXT statement. */
void next()
{
  struct for_stack i;

  i = fpop(); /* read the loop info */

  variables[i.var]++; /* increment control variable */
  if(variables[i.var]>i.target) return;  /* all done */
  fpush(i);  /* otherwise, restore the info */
  prog = i.loc;  /* loop */
}

/* Push function for the FOR stack. */
void fpush(i)
struct for_stack i;
{
   if(ftos>FOR_NEST)
    serror(10);

  fstack[ftos]=i;
  ftos++;
}

struct for_stack fpop()
{
  ftos--;
  if(ftos<0) serror(11);
  return(fstack[ftos]);
}

/* Execute a simple form of the BASIC INPUT command */
void input()
{
  char var;
  long i;

  get_token(); /* see if prompt string is present */
  if(token_type==QUOTE) {
    printf(token); /* if so, print it and check for comma */
    get_token();
    if(*token!=',') serror(1);
    get_token();
  }
  else printf("? "); /* otherwise, prompt with / */
  var = toupper(*token)-'A'; /* get the input var */

  scanf("%d", &i); /* read input */

  variables[var] = i; /* store it */
}

/* Execute a GOSUB command. */
void gosub()
{
  char *loc;

  get_token();
  /* find the label to call */
  loc = find_label(token);
  if(loc=='\0')
    serror(7); /* label not defined */
  else {
    gpush(prog); /* save place to return to */
    prog = loc;  /* start program running at that loc */
  }
}

/* Return from GOSUB. */
void greturn()
{
   prog = gpop();
}

/* GOSUB stack push function. */
void gpush(s)
char *s;
{
  gtos++;

  if(gtos==SUB_NEST) {
    serror(12);
    return;
  }
  gstack[gtos]=s;
}

/* GOSUB stack pop function. */
char *gpop()
{
  if(gtos==0) {
    serror(13);
    return 0;
  }
  return(gstack[gtos--]);
}

/* Entry point into parser. */
void get_exp(result)
long *result;
{
  get_token();
  if(!*token) {
    serror(2);
    return;
  }
  level2(result);
  putback(); /* return last token read to input stream */
}


/* display an error message */
void serror(error)
int error;
{
  static char *e[]= {   
    "syntax error", 
    "unmatched parentheses", 
    "no expression present",
    "equals sign expected",
    "not a variable",
    "label table full",
    "duplicate label",
    "undefined label",
    "THEN expected",
    "TO expected",
    "too many nested FOR loops",
    "NEXT without FOR",
    "too many nested GOSUBs",
    "RETURN without GOSUB",
	"CAN ID out of range",
	"CAN DLC out of range",
	"CAN MsgType out of range",
	"GOTOXY cursor position out of range"
  }; 
  printf("%s\n", e[error]); 
  longjmp(e_buf, 1); /* return to save point */
}

/* Get a token. */
int get_token()
{

  register char *temp;

  token_type=0; tok=0;
  temp=token;

  if(*prog=='\0') { /* end of file */
    *token=0;
    tok = FINISHED;
    return(token_type=DELIMITER);
  }

  while(iswhite(*prog)) ++prog;  /* skip over white space */

  if(*prog=='\r') { /* crlf */
    ++prog; ++prog;
    tok = EOL; *token='\r';
    token[1]='\n'; token[2]=0;
    return (token_type = DELIMITER);
  }

  if(*prog=='#') { /* REMARK */
    while(*prog!='\r' && *prog!='\0') ++prog;
    return (token_type = REMARK);
  }

  if(strchr("+-*^/%=;(),><|&~", *prog)){ /* delimiter */
    *temp=*prog;
    prog++; /* advance to next position */
    temp++;
    *temp=0; 
    return (token_type=DELIMITER);
  }
    
  if(*prog=='"') { /* quoted string */
    prog++;
    while(*prog!='"'&& *prog!='\r') *temp++=*prog++;
    if(*prog=='\r') serror(1);
    prog++;*temp=0;
    return(token_type=QUOTE);
  }
  
  if(isdigit(*prog)) { /* number */
    while(!isdelim(*prog)) *temp++=*prog++;
    *temp = '\0';
    return(token_type = NUMBER);
  }

  if(isalpha(*prog)) { /* var or command */
    while(!isdelim(*prog)) *temp++=*prog++;
    token_type=STRING;
  }

  *temp = '\0';

  /* see if a string is a command or a variable */
  if(token_type==STRING) {
    tok=look_up(token); /* convert to internal rep */
    if(!tok) token_type = VARIABLE;
    else token_type = COMMAND; /* is a command */
  }
  return token_type;
}



/* Return a token to input stream. */
void putback() 
{
  char *t; 
  t = token; 
  for(; *t; t++) prog--; 
}

/* Look up a a token's internal representation in the
   token table.
*/
int look_up(s)
char *s;
{
  register int i;
  char *p;
  /* convert to lowercase */
  p = s;
  while(*p){ *p = tolower(*p); p++; }
  /* see if token is in table */
  for(i=0; *table[i].command; i++)
      if(!strcmp(table[i].command, s)) return table[i].tok;
  return 0; /* unknown command */
}

/* Return true if c is a delimiter. */
int isdelim(c)
char c; 
{
  if(strchr(" ;,+-<>/*%^=()|&~", c) || c==9 || c=='\r' || c==0) 
    return 1;  
  return 0;
}

/* Return 1 if c is space or tab. */
int iswhite(c)
char c;
{
  if(c==' ' || c=='\t') return 1;
  else return 0;
}



/*  Add or subtract two terms. */
void level2(result)
long *result;
{
  register char  op; 
  int hold; 

  level3(result); 
  while((op = *token) == '+' || op == '-') {
    get_token(); 
    level3(&hold); 
    arith(op, result, &hold);
  }
}

/* Multiply or divide two factors. */
void level3(result)
long *result;
{
  register char  op; 
  int hold;

  level4(result); 
  while((op = *token) == '*' || op == '/' || op == '%' || op == '|' || op == '&' || op == '~' ) {
    get_token(); 
    level4(&hold); 
    arith(op, result, &hold); 
  }
}

/* Process integer exponent. */
void level4(result)
long *result;
{
  int hold; 

  level5(result); 
  if(*token== '^') {
    get_token(); 
    level4(&hold); 
    arith('^', result, &hold); 
  }
}

/* Is a unary + or -. */
void level5(result)
long *result;
{
  register char  op; 

  op = 0; 
  if((token_type==DELIMITER) && *token=='+' || *token=='-') {
    op = *token; 
    get_token(); 
  }
  level6(result); 
  if(op)
    unary(op, result); 
}

/* Process parenthesized expression. */
void level6(result)
long *result;
{
  if((*token == '(') && (token_type == DELIMITER)) {
    get_token(); 
    level2(result); 
    if(*token != ')')
      serror(1);
    get_token(); 
  }
  else
    primitive(result);
}

/* Find value of number or variable. */
void primitive(result)
long *result;
{

  switch(token_type) {
  case VARIABLE:
    *result = find_var(token);
    get_token(); 
    return; 
  case NUMBER:
	  // now we try to see if we could also use HEX (0x...)
    if(token[1]=='x')
     *result = htoi(token);
	else
     *result = atoi(token);
    get_token();
    return;
  default:
    serror(0);
  }
}

/* Perform the specified arithmetic. */
void arith(o, r, h)
char o;
long *r, *h;
{
  register long t, ex;

  switch(o) {
    case '-':
      *r = *r-*h; 
    break; 
    case '+':
      *r = *r+*h; 
    break; 
    case '*':  
      *r = *r * *h; 
    break; 
    case '/':
      *r = (*r)/(*h);
    break; 
    case '%':
      t = (*r)/(*h); 
      *r = *r-(t*(*h)); 
    break; 
	case '&':
	  *r=*r && *h;
	break;
	case '|':
	  *r=*r || *h;
	break;
	case '~':
	  *r=*r^*h;
	break;
	case '^':
      ex = *r; 
      if(*h==0) {
        *r = 1; 
		break; 
	  }
      for(t=*h-1; t>0; --t) *r = (*r) * ex;
    break;       
  }
}

/* Reverse the sign. */
void unary(o, r)
char o;
long *r;
{
  if(o=='-') *r = -(*r);
}

/* Find the value of a variable. */
long find_var(char *s)
{
  if(!isalpha(*s)){
    serror(4); /* not a variable */
    return 0;
  }
  return variables[toupper(*token)-'A'];
}

/*
CheckParams(devicehandle, baudrate, hwtype, ioport, interrupt)
char *devicehandle;
char *baudrate;
char *hwtype;
char *ioport;
char *interrupt;*/
int CheckParams(char *devicehandle, char *baudrate, char *hwtype, char *ioport, char *interrupt)
{
  if(!strncmp(devicehandle,"0x",2) || !strncmp(devicehandle,"0X",2))
	CANChannel=htoi(devicehandle);
  else
	CANChannel=atoi(devicehandle);
  if(CANChannel<0x21 || CANChannel>0x62)
	CANChannel=0;
  
  if(!strncmp(baudrate,"0x",2) || !strncmp(baudrate,"0X",2))
	CANBaudrate=htoi(baudrate);
  else
	CANBaudrate=atoi(baudrate);
  if(CANBaudrate<0x01 || CANBaudrate >0xFFFF)
	CANBaudrate=0;

  if(hwtype!=NULL) //if not a NULL
  {
	if(!strncmp(hwtype,"0x",2) || !strncmp(hwtype,"0X",2))
		CANHwType=htoi(hwtype);
	else
		CANHwType=atoi(hwtype);
	if(CANHwType<1 || CANHwType>10)
		CANHwType=0;
 	if(ioport!=NULL)// if not a NULL
	{
		if(!strncmp(ioport,"0x",2) || !strncmp(ioport,"0X",2))
			CANIOPort=htoi(ioport);
		else
			CANIOPort=atoi(ioport);
		if(CANIOPort<0x01 || CANIOPort>0x7FF)
			CANIOPort=0;
		if(interrupt!=NULL)// if not a NULL
		{
			if(!strncmp(interrupt,"0x",2) || !strncmp(interrupt,"0X",2))
				CANInterrupt=htoi(interrupt);
			else
				CANInterrupt=atoi(interrupt);
			if(CANInterrupt<0x01 || CANInterrupt>0x0a)
				CANInterrupt=0;
		}
	}
  }
  return true;
}


void can_reset()
{	
	if(!g_CANSIMU)
		g_CAN_Reset(CANChannel);
	return;
}

void can_getstatus()
{
	unsigned ret;
	char var;
	if(!g_CANSIMU)
		ret = g_CAN_GetStatus(CANChannel);
	else
		ret=PCAN_ERROR_ILLDATA;
	get_token(); /* see if prompt string is present */
	if(tok==EOL || tok==FINISHED)
		serror(0);
	var = toupper(*token)-'A'; /* get the input var */
	variables[var] = ret; /* store it */
	get_token(); /* see if prompt string is present */
    if(tok!=EOL && tok!=FINISHED)
	  serror(0); /* error is not , or ; */
	return;
}

void can_write()
{
  long answer;
  int len=0;
  char last_delim;
  int CANArray[12]={0,0,0,0,0,0,0,0,0,0,0,0};
  int counter=0;
  int i;

  do {
    get_token(); /* get next list item */
    if(tok==EOL || tok==FINISHED) break;
    if(token_type==QUOTE) { /* is string */
      serror(0);
    }
    else { /* is expression */
      putback();
      get_exp(&answer);
      get_token();
      CANArray[counter++]= answer;
    }
    last_delim = *token; 
    if(*token==',') /* do nothing */;
    else if(tok!=EOL && tok!=FINISHED) serror(0); 
  } while ( *token==',');

  if(tok==EOL || tok==FINISHED) {
/*    if(last_delim != ';' && last_delim!=',') printf("\n");
	*/
  }
  else serror(0); /* error is not , or ; */

 // now check if the Data have any "logic"
    if(CANArray[0]<0 || CANArray[0]>CAN_MAX_EXTENDED_ID)
		serror(9);
	if(CANArray[1]<0 || CANArray[1]>8)
		serror(10);
	if(CANArray[2]<0 || CANArray[2]>2)
		serror(11);

	CANSendMsg.ID=CANArray[0]; // Must have field
	CANSendMsg.LEN=CANArray[1]; // Must have field
	CANSendMsg.MSGTYPE=CANArray[2]; // Must have field
	for(i=0;i<=CANSendMsg.LEN;i++) // run to LEN
		CANSendMsg.DATA[i]=CANArray[i+3];
	if(!g_CANSIMU)
		i=g_CAN_Write(CANChannel, &CANSendMsg);
}

void can_read()
{
  int len=0;
  long CANArray[12]={0,0,0,0,0,0,0,0,0,0,0,0};
  int counter=0;
  char var;
  long i;

  // CAN Read calling and store all Information inside the CANArray
  if(!g_CANSIMU)
	i=g_CAN_Read(CANChannel, &CANRecvMsg, &CANTimestamp);
  else
	i=PCAN_ERROR_ILLDATA;
  if(i==PCAN_ERROR_OK){
	CANArray[0]=CANRecvMsg.ID;
	CANArray[1]=CANRecvMsg.LEN;
	CANArray[2]=CANRecvMsg.MSGTYPE;
	for(i=0;i<CANRecvMsg.LEN;i++)
		CANArray[3+i]=CANRecvMsg.DATA[i];
  }
  else
  {
    // If CAN ERROR, than ID negative
	CANArray[0]=-1;
	CANArray[3]=i;
  }
  i=0;
  do {
	get_token(); /* see if prompt string is present */
	if(tok==EOL || tok==FINISHED) break;
	var = toupper(*token)-'A'; /* get the input var */
	variables[var] = CANArray[i++]; /* store it */
	get_token(); /* see if prompt string is present */
  } while ( *token==',' && i<13);
  if(tok!=EOL && tok!=FINISHED)
	  serror(0); /* error is not , or ; */
}


void can_filtermessages()
{
 int ret;
 int i;
 long answer;
 long params[3];
 i=0;
 do{
	get_token(); /* get next list item */
	if(tok==EOL || tok==FINISHED)
		serror(0); 
	if(token_type==QUOTE) /* is string */
		serror(0);
	else { /* is expression */
		putback();
		get_exp(&answer);
		get_token();
		params[i++]=(answer);
	}
  }
  while(i<3);
  if(tok!=EOL && tok!=FINISHED) serror(0); 
  // Check if Params are confirm
  if(params[0]<0 || params[0]>CAN_MAX_EXTENDED_ID || params[1]<0 || params[1]>CAN_MAX_EXTENDED_ID)
	  serror(9);
  if(params[2]!=PCAN_MESSAGE_STANDARD || params[2]!=PCAN_MESSAGE_EXTENDED)
	  serror(10); 
  if(!g_CANSIMU)
	ret=g_CAN_FilterMessages(CANChannel,params[0],params[1],params[2]);
}

void can_getvalue()
{
	unsigned params[2];
	long answer;
    char var;
	// 1 Parameter holen 
	get_token(); /* get next list item */
	if(tok==EOL || tok==FINISHED)
		serror(0); 
	if(token_type==QUOTE) /* is string */
		serror(0);
	else { /* is expression */
		putback();
		get_exp(&answer);
		get_token();
		params[0]=(answer);
	}
	if(!g_CANSIMU)
		g_CAN_GetValue(CANChannel,params[0],&params[1],sizeof(params[1]));
	else
		params[1]=0;
	get_token(); /* see if prompt string is present */
	if(tok==EOL || tok==FINISHED)
		serror(0);
	var = toupper(*token)-'A'; /* get the input var */
	variables[var] = params[1]; /* store it */
	get_token(); /* see if prompt string is present */
    if(tok!=EOL && tok!=FINISHED)
	  serror(0); /* error is not , or ; */
	return;
}

void can_setvalue()
{
 int ret;
 int i;
 long answer;
 long params[3];
 i=0;
 do{
	get_token(); /* get next list item */
	if(tok==EOL || tok==FINISHED)
		serror(0); 
	if(token_type==QUOTE) /* is string */
		serror(0);
	else { /* is expression */
		putback();
		get_exp(&answer);
		get_token();
		params[i++]=(answer);
	}
  }
  while(i<2);
  if(tok!=EOL && tok!=FINISHED) serror(0); 
  // Check if Params are confirm
  if(params[0]>=0 && params[0]<=0xff && params[1]>=0 && params[1]<=0xFFFF)
	if(!g_CANSIMU)
	  ret=g_CAN_SetValue(CANChannel,params[0],params[1],sizeof(params[1]));
  else
	serror(0);
}

void can_geterrortext()
{
	unsigned param;
	long answer;
	char Buffer[255];
	// memset(Buffer,'\0',255);
	// 1 Parameter holen 
	get_token(); /* get next list item */
	if(tok==EOL || tok==FINISHED)
		serror(0); 
	if(token_type==QUOTE) /* is string */
		serror(0);
	else { /* is expression */
		putback();
		get_exp(&answer);
		get_token();
		param=(answer);
	}
	if(!g_CANSIMU)
	{
		g_CAN_GetErrorText(param,0x00,Buffer);
		printf("%s\n",Buffer);
	}
	else
		printf("No Info available in SIMU mode\n");
	return;
}


void gotoxy()
{
 int i;
 long answer;
 long params[2];
 COORD mycoord;
 i=0;

 do{
	get_token(); /* get next list item */
	if(tok==EOL || tok==FINISHED)
		serror(0); 
	if(token_type==QUOTE) /* is string */
		serror(0);
	else { /* is expression */
		putback();
		get_exp(&answer);
		get_token();
		params[i++]=(answer);
	}
  }
  while(i<2);
  if(tok!=EOL && tok!=FINISHED) serror(0); 
  // Check if Params are confirm
  if(params[0]<0 || params[0]>80 || params[1]<0 || params[1]>25)
	  serror(17);
  // Move Cursor to params[0],params[1]
  mycoord.X=(short)params[0];
  mycoord.Y=(short)params[1];
  SetConsoleCursorPosition(g_hStdOut,mycoord);
};


// set the fore/background color of the Console using param1
void setcolor()
{
 long answer;
 long param;

 get_token(); /* get next list item */
 if(tok==EOL || tok==FINISHED)
	serror(0); 
 if(token_type==QUOTE) /* is string */
	serror(0);
 else { /* is expression */
	putback();
	get_exp(&answer);
	get_token();
	param=(answer);
 }
 if(tok!=EOL && tok!=FINISHED) serror(0); 
 // Check if Params are confirm
 if(param<0 || param>15)
	  serror(0);
  
  switch(param)
  {
	case 0:	// White on Black normal
		SetConsoleTextAttribute(g_hStdOut,FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	break;
	case 1:	// Red on Black
		SetConsoleTextAttribute(g_hStdOut,FOREGROUND_INTENSITY | FOREGROUND_RED);
	break;
	case 2:	// Green on Black
		SetConsoleTextAttribute(g_hStdOut,FOREGROUND_INTENSITY | FOREGROUND_GREEN);
	break;
	case 3:	// Yellow on Black
		SetConsoleTextAttribute(g_hStdOut, FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN);
	break;
	case 4:	// Blue on Black
		SetConsoleTextAttribute(g_hStdOut,FOREGROUND_INTENSITY |FOREGROUND_BLUE);
	break;
	case 5:	// Magenta on Black
		SetConsoleTextAttribute(g_hStdOut,FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_BLUE);
	break;
	case 6:	// Cyan on Black
		SetConsoleTextAttribute(g_hStdOut,FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE);
	break;
	case 7:	// Black on Gray
		SetConsoleTextAttribute(g_hStdOut,BACKGROUND_INTENSITY | BACKGROUND_INTENSITY);
	break;
	case 8:	// Black on White
		SetConsoleTextAttribute(g_hStdOut,BACKGROUND_INTENSITY | FOREGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE);
	break;
	case 9:	// Red on White
		SetConsoleTextAttribute(g_hStdOut,BACKGROUND_INTENSITY | FOREGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | FOREGROUND_RED);
	break;
	case 10: // Green on White
		SetConsoleTextAttribute(g_hStdOut,BACKGROUND_INTENSITY | FOREGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | FOREGROUND_GREEN);
	break;
	case 11: // Yellow on White
		SetConsoleTextAttribute(g_hStdOut,BACKGROUND_INTENSITY | FOREGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN);
	break;
	case 12: // Blue on White
		SetConsoleTextAttribute(g_hStdOut,BACKGROUND_INTENSITY | FOREGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | FOREGROUND_BLUE);
	break;
	case 13: // Magenta on White
		SetConsoleTextAttribute(g_hStdOut,BACKGROUND_INTENSITY | FOREGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_BLUE);
	break;
	case 14: // Cyan on White
		SetConsoleTextAttribute(g_hStdOut,BACKGROUND_INTENSITY | FOREGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_BLUE);
	break;
	case 15: // White on Black Intensive
		SetConsoleTextAttribute(g_hStdOut,FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	break;
	default : // White on Black
		SetConsoleTextAttribute(g_hStdOut,FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
	break;
  } 
};

// Clear the Console Buffer in a differnet way .. not fine but...
void clrscrn()
{
  // Clear Text Screen
  system("cls");
};

void statusline()
{
	unsigned param;
	long answer;
    
	// 1 Parameter holen 
	get_token(); /* get next list item */
	if(tok==EOL || tok==FINISHED)
		serror(0); 
	if(token_type==QUOTE) /* is string */
		serror(0);
	else { /* is expression */
		putback();
		get_exp(&answer);
		get_token();
		param=(answer);
	}
	if(param>0)
		g_statusline=true;
	else
	{
		g_statusline= false;
		SetConsoleCursorPosition(g_hStdOut,g_coordStatus);
		// clrscrn();
		// printf("                                                                              ");
	}
}

void tickcount()
{
	char var;
	get_token(); /* see if prompt string is present */
	if(tok==EOL || tok==FINISHED)
		serror(0);
	var = toupper(*token)-'A'; /* get the input var */
	variables[var] = GetTickCount();  ; /* store it */
	get_token(); /* see if prompt string is present */
    if(tok!=EOL && tok!=FINISHED)
	  serror(0); /* error is not , or ; */
	return;
}

// Generate a Random Number 
// get paranm 1 to set from to range, write in param 2 the random value
void random()
{
	unsigned params[2];
	long answer;
    char var;
	// 1 Parameter holen 
	get_token(); /* get next list item */
	if(tok==EOL || tok==FINISHED)
		serror(0); 
	if(token_type==QUOTE) /* is string */
		serror(0);
	else { /* is expression */
		putback();
		get_exp(&answer);
		get_token();
		params[0]=(answer);
	}
	get_token(); /* see if prompt string is present */
	if(tok==EOL || tok==FINISHED)
		serror(0);
	var = toupper(*token)-'A'; /* get the input var */
	srand(time(NULL));
	variables[var] = rand()%(params[0]+1); /* store it */
	get_token(); /* see if prompt string is present */
    if(tok!=EOL && tok!=FINISHED)
	  serror(0); /* error is not , or ; */
	return;
}

// CAN_WAITID
// a simple implementation to waid for a defined CAN-ID
// like CAN Read but check if ID x came in time y (in ms)
// NOTE: All other CAN Frames which will be receiced in this timesequenz will be deleted!
//
void can_waitid()
{
  int len=0;
  long CANArray[11]={0,0,0,0,0,0,0,0,0,0,0};
  int counter=0;
  long answer;
  char var;
  long i,timeout;
  long params[2]; // First is Timeout in ms , second is ID to Wait for
  DWORD TickCounter,StartTickCounter;
  BOOL received=false;
  i=0;

  // first param is the timeout in ms
  // second is the ID we wait for...
  do{
	get_token(); /* get next list item */
	if(tok==EOL || tok==FINISHED)
		serror(0); 
	if(token_type==QUOTE) /* is string */
		serror(0);
	else { /* is expression */
		putback();
		get_exp(&answer);
		get_token();
		params[i++]=(answer);
		}
	}
  while(i<2);
  StartTickCounter=GetTickCount();  
  
  do{
	// CAN Read calling and store all Information inside the CANArray
	if(!g_CANSIMU)
		i=g_CAN_Read(CANChannel, &CANRecvMsg, &CANTimestamp);
	else
		i=PCAN_ERROR_ILLDATA;
	if(i==PCAN_ERROR_OK){ // A CAN Message was read
		//Message is "real"
		if(CANRecvMsg.ID==params[1]) // yes - thats what we wait for...
		{
			received=true;
			CANArray[0]=CANRecvMsg.LEN;
			CANArray[1]=CANRecvMsg.MSGTYPE;
			for(i=0;i<CANRecvMsg.LEN;i++)
				CANArray[2+i]=CANRecvMsg.DATA[i];
		}
		else
		{
			//break;
		}
	}else // If no CAN Frame received - we wait for 1ms
		Sleep(1); //wait 1 ms ..if possible
	TickCounter=GetTickCount();
  }
  while( ((TickCounter-StartTickCounter)<params[0]) && !received);
  
  if(!received) // Timeout reached or any other problem...
  {
    // If not received within the time, than DLC(LEN) and all other Data will be negative...
	  for(i=0;i<10;i++)
		CANArray[i]=-1;
  }
  i=0;
  do {
	get_token(); /* see if prompt string is present */
	if(tok==EOL || tok==FINISHED) break;
	var = toupper(*token)-'A'; /* get the input var */
	variables[var] = CANArray[i++]; /* store it */
	get_token(); /* see if prompt string is present */
  } while ( *token==',' && i<11);
  if(tok!=EOL && tok!=FINISHED)
	  serror(0); /* error is not , or ; */
}


// CAN_CLEARQUEUE
// clear the incomming queue off the device driver
// read Messages until Queue is empty!
void can_clearqueue()
{
  long i=0;
  if(g_CANSIMU) //If simulation - we dont care about the queue
	  return;
  do{
		// CAN Read calling and store all Information inside the CANArray
		i=g_CAN_Read(CANChannel, &CANRecvMsg, &CANTimestamp);
		//Sleep(1); //wait 1 ms ..if possible
  }while(i==PCAN_ERROR_OK);
  return;
}

int ConsoleHandler(DWORD CEvent)
{
    int ret;
    switch(CEvent)
    {
    case CTRL_C_EVENT:
        ret=MessageBox(NULL,"Do you want to stop CANBas?","CANBas Info",MB_OKCANCEL);
		if(ret==IDOK)
			global_break=true;
        break;
    case CTRL_BREAK_EVENT:
        ret=MessageBox(NULL,"Do you want to stop CANBas?","CANBas Info",MB_OKCANCEL);
		if(ret==IDOK)
			global_break=true;
        break;
    case CTRL_CLOSE_EVENT:
        MessageBox(NULL,"Program being closed!","CANBas Info",MB_OK);
		global_break=true;
        break;
    case CTRL_LOGOFF_EVENT:
        MessageBox(NULL,"User is logging off!","CANBas Info",MB_OK);
		global_break=true;
        break;
    case CTRL_SHUTDOWN_EVENT:
        MessageBox(NULL,"User is logging off!","CANBas Info",MB_OK);
		global_break=true;
        break;

    }
    return TRUE;
}