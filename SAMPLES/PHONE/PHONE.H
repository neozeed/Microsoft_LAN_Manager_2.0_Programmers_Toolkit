/*------------------------------------------
   PHONE.H header file

   Copyright (C) 1990 Microsoft Corporation 
  ------------------------------------------*/

#define ID_RESOURCE      1     // Main PM resource
#define ID_TIMER         2     // Timer

#define IDM_CALL         1    // Call
#define IDM_ANSWER       2    // Answer (not on menu)
#define IDM_HANGUP       3    // Hangup
#define IDM_ABOUT        6    // About
#define IDM_HELP         7    // Help
#define IDM_INFO         8    // Extended help

#define IDM_TOPEXIT      39   // Exit menu
#define IDM_EXIT         40   // "Exit" submenu
#define IDM_RESUME       41

#define IDD_ABOUT	 1    // "Dialog" for About
#define IDD_CALL	 2    // Call
#define IDD_ANSWER	 3    // Answer

#define IDD_FILEEDIT	 4    // Get Call Name

#define IDD_COMMENT      5    // Comment field
#define IDD_HELP         6    // Help
#define IDD_NAMELIST     7    // Listbox

#define MAIN_HELPTABLE 500    // Help table
#define MAIN_MENU      501    // Main menu for help

#define GENERAL_HP     607    // Extended help for Phone

#define WM_REMOTE WM_USER+1   // User defined PM messages
#define WM_ANSWER WM_USER+2
#define WM_DISABLE WM_USER+3
#define LCID_FIXEDFONT 1L     // Font id
#define NO  2                 // Constants
#define ERROR -1
#define YES 1
#define DISABLE 3
#define THREAD_SEM_WAIT 1000L  // Timeout constants
#define PIPE_SEM_WAIT	1000L
#define WRITE_TIME_OUT  60000L       // If user does not type in 1 minute
                                     // time out and send Write Buffer

#define PIPENMSIZE   (14+PHNUMSIZE) // sizeof   '\\SERVERNAME\PIPE\PLINE'
#define STACKSIZE (4096+512)        // Thread stack size
