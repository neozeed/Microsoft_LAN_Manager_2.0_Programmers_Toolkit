/*
   MAIL.H -- Message related declarations used by Phone.

   Copyright (C) 1990 Microsoft Corporation 

   The Lan Manager header file LAN.H and OS2.H must both be included
   prior to this file.
*/

#ifndef _MAILH
#define _MAILH

// Defines
#define PHNUMSIZE         (SNLEN+1)       // Maximum Phone number size

#define PHONELINE         "\\PIPE\\PLINE" // Named-Pipe name (Phone Line)

#define COMPONENTNAME     "PHONE"         // LANMAN.INI Component name
#define PHONENUMBER       "NUMBER"        // LANMAN.INI Phone number
#define SERVERNAME        "SERVER"        // LANMAN.INI Server name

#define MAXLINES          200             // Maximum number of phone lines
#define MINLINES          6               // Minimum number of phone lines
#define DEFAULTLINES      16              // Default number of phone lines
#define MAXLINEBUF        50              // Maximum line buffer count
#define MINLINEBUF        1               // Minimum line buffer count
#define DEFAULTLINEBUF    10              // Default line buffer count

// Response error codes (passed in PHMSG.usResponse)
#define PBXERROR          1               // Unexpected PBX error
#define SHUTDOWN          2               // PBX.EXE is shutting down
#define CLIENTHANGUP      3               // Client requested a hangup
#define DONTANSWER        4               // Call rejected
#define NOMORELINES       5               // Server is out of phone lines
#define INVALIDNUMBER     6               // Invalid target phone number
#define LINEBUSY          7               // Target phone line is busy
#define DUPLICATENUMBER   8               // Duplicate phone number
#define CANTCALLSELF      9               // Cannot Call self error

// Message packet types (message identifiers)
typedef enum _MTYPE {
  PLUGIN = 1,                             // Plug a phone in message
  UNPLUG,                                 // Unplug a phone message
  CALL,                                   // Make a phone call message
  ANSWER,                                 // Answer a phone call message
  HANGUP,                                 // Hangup phone message
  TALK,                                   // Talk on phone message
  LISTQUERY                               // Client list request
} MTYPE;

// PM message unit
typedef struct _CHARMESSAGE {
      MPARAM mp1;                         // PM WM_CHAR mp1 argument
      MPARAM mp2;                         // PM WM_CHAR mp2 argument
} CHARMESSAGE;

// Message packet structure
typedef struct _PHMSG {
  MTYPE   bMType;                         // Phone message type
  USHORT  usResponse;                     // NO_ERROR = Success
  union   {
    CHAR    pszPhoneNumber[PHNUMSIZE];    // PLUGIN -- Phone number
                                          // UNPLUG -- Phone number
                                          // CALL   -- Target phone number
                                          // ANSWER -- Target phone number
    USHORT  usLineBufCnt;                 // PLUGIN -- Line buffer count
                                          //   (returned in ACK to client)
    CHARMESSAGE bMsg;                     // TALK   -- Message data
  }  msg;
}  PHMSG;

// LISTMSG is the struct used to return the Phone Numbers currently in
// the routing table.  Each Phone Number is PHNUMSIZE in length (including
// the null terminating byte) and they start at the end of the LISTMSG
// structure.
typedef struct _LISTMSG {
  MTYPE    bMType;                        // Only LISTQUERY is valid
  USHORT   usResponse;                    // NO_ERROR = Success
  USHORT   usNumberCnt;                   // Number of client numbers passed
} LISTMSG;

#endif
