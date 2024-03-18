/*
   PBX.H -- Message related declarations used by PBX.

   Copyright (C) 1990 Microsoft Corporation 
*/

#ifndef _PBXH
#define _PBXH

#ifdef INCL_GLOBAL
#define GLOBAL
#else
#define GLOBAL  extern
#endif

#include  "mail.h"

// Defines
#define DEFAULTTIMEOUT        1000L       // Default Time-out value
#define DEFAULTSTACKSIZE      4096        // Default Stack size

#define PBXCOMPONENT      "PBX"           // LANMAN.INI Section name
#define LINESNUMBER       "LINES"         // LANMAN.INI Lines number
#define LINEBUFNUMBER     "LINEBUF"       // LANMAN.INI Linebuf number

#define MINFHANDLES    35                 // Minimum number of file handles
#define MAXRTHREADS    16                 // Max number of Router threads
#define LINESPERTHREAD 16                 // Phone lines per Router thread
#define SEMTIMEOUT     1000L              // Phone line access sem time out
#define RINGSEM        "\\SEM\\RRING.SEM" // Shared Memory access sem
#define LINESEM        "\\SEM\\RLINE.SEM" // Phone line semaphore name

// Define short-hand for the LAN Manager Service exit status codes
#define SRVCODE(c,m)   ((ULONG)(((ULONG)c << 16) | (ULONG)m))
#define NORMALSTATUS   (SRVCODE(SERVICE_UIC_NORMAL,SERVICE_UIC_M_NULL))
#define ERRORSTATUS    (SRVCODE(SERVICE_UIC_INTERNAL,SERVICE_UIC_M_ERRLOG))

// Define short-hand macro interfaces to the error reporting routines
#define ERRRPT(x,y)    { ErrorRpt(x, __FILE__, __LINE__, y); }
#define ERREXIT(x,y)   { ErrorExit(x, __FILE__, __LINE__, y); }

// Structures
// Phone Routing Table
//   The routine table will contain one entry per phone line.  The first
//   field contains the phone number associated with the line, the
//   second field contains the line state, the third the index in the
//   routing table of the target phone line when a connection has been
//   made between two clients, and the last field is the named-pipe handle
//   for the phone line.
typedef struct _ROUTEENT {
  CHAR    pszPhoneNumber[PHNUMSIZE];      // Phone number for entry
  enum {  Open = 1,                       //   No connection exists
          Busy,                           //   Valid connection exists
          Pending,                        //   Partial connection exists
          Invalid                         //   Line is unowned
  }       fState;                         // Phone line state
  USHORT  usConnection;                   // Current connection index number
                                          //   usConnection is only valid
                                          //   when the line is Busy or
                                          //   Pending
  ULONG   ulLineSem;                      // Named-Pipe access semaphore
  HPIPE   hPhoneLine;                     // Named-Pipe handle
} ROUTEENT;

// Router thread allocation structure
typedef struct _RTHREAD {
  SHORT      sRouterID;                   // Router thread ID
  ULONG      hsemExitSem;                 // Exit  semaphore
  ULONG      hsemPauseSem;                // Pause semaphore
  SEL        selLineBuf;                  // Phone line buffer selector
  PHMSG FAR  *pbLineBuf;                  // Phone line buffer pointer
  MUXSEMLIST mxslSems;                    // Named-pipe semaphores
} RTHREAD;

// Shared Memory Segment map
typedef struct _RMEMORY {
  HSYSSEM      hssmMemLock;               // Memory access semaphore
  ULONG        hsemPBX;                   // Main thread semaphore
  USHORT       usRThreadCnt;              // Number of Router threads
  USHORT       usLineCnt;                 // Total number of phone lines
  USHORT       usLinesUsed;               // Number of allocated phone lines
  USHORT       usLineBufCnt;              // Line buffer count
  USHORT       usLineBufSize;             // Line buffer size in bytes
  SEL          selRouteTable;             // Selector for Router table
  ROUTEENT FAR *pbRouteTable;             // Pointer to Router table
  RTHREAD      bRouters[MAXRTHREADS];     // Router thread structs
} RMEMORY;

// Globals
GLOBAL SEL           selRMemory;          // Shared memory segment selector
GLOBAL RMEMORY FAR   *pbRMemory;          // Shared memory segment pointer
GLOBAL struct service_status ssStatus;    // LAN Manager Service struct


// Function Prototypes
void     main(void);                                // Main thread
void     ErrorRpt(char *, char *, USHORT, USHORT);  // Error reporter
void     ErrorExit(char *, char *, USHORT, USHORT); // Error reporter
void     ExitHandler(ULONG);                        // Exit clean-up
void FAR Router(USHORT);                            // Packet router

#endif
