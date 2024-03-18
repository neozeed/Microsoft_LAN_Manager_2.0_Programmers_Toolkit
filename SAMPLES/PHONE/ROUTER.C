/*
  ROUTER.C -- PHONE switching program.

  Copyright (C) 1990 Microsoft Corporation 
  
  For a general description, see PBX.C header.

  ROUTER.C contains the function that manages the routing table.  One
  Router thread is created for every LINESPERTHREAD phone lines
  requested.  All threads use the same copy of the routing table and
  serialize access during volatile operations (e.g., adding a name to
  the table).

  Each entry in the routing table contains the phone number (e.g., name)
  of the owning client, a state flag, a connection index, and the
  associated named-pipe handle.  The phone line can be in one of four
  states (indicated by the state flag):

     a) Open ----- The phone line is not being used, the client
                   may make calls and answer calls.
     b) Busy ----- The phone line is busy, the client is currently
                   in conversation with another client.  The
                   connection index contains the routing table
                   offset of the target client phone.
     c) Pending -- The phone line is either in the process of hanging
                   up or establishing a connection.
     d) Invalid -- The phone line is unowned by any client.

   Phone Protocol:

   ROUTER.C implements a packet-switched, connection-oriented protocol.
   Each phone message is packaged into a single structure (except for
   LISTQUERY messages, see below).  There are seven defined message
   types:

     PLUGIN:  Register a client instance of PHONE.EXE with the Server
       running PBX.EXE.  The routing table is checked for a duplicate
       phone number and an acknowledgement is sent to the client.

     UNPLUG:  Remove a client from the routing table.  The associated
       routing table entry is marked as Invalid (i.e., available for
       another client to use).  This call always succeeds and no
       acknowledgement is sent.

     CALL  :  Request a connection to another client.  This call succeeds
       if the requested client is plugged into the Server (i.e., an
       entry exists in the routing table for the requested phone number)
       and their line is Open.  The lines for the both clients are
       placed into the Pending state.  An acknowledgement is returned to
       the client and the request is forwarded to the target client.

     ANSWER:  Respond to a CALL request from a client.  This call succeeds
       if both clients are still waiting for the connection (i.e., their
       lines are in the Pending state).  Then the lines for both clients
       are placed in the Busy state.  An acknowledgement is returned to the
       submitting client and the request is forwarded onto the original
       client (the client who issued the CALL).

     HANGUP:  Request that the connection with another client be broken.
       This call always succeeds and places the line for the submitting
       client into the Open state with no acknowledgement sent back.  If
       a valid connection existed, then the request is forwarded on to
       the target client and their line is placed into an Open state.

     TALK  :  This a data packet that is routed directly to the target
       client.  If a valid connection does not exist, the packet is
       ignored.  No changes to the line state occur and no acknowledgements
       are returned.

     LISTQUERY:  This is a special purpose packet and is a request for
       a list of all clients currently registered with the Server (i.e.,
       whose phone numbers exist in the routing table).  The line state
       for the submitting client is left unchanged.  An acknowledgement
       is returned to the submitting client that contains all the numbers
       currently in the routing table.

   Each message is a single unit, however, multiple messages may be sent
   to PBX.EXE and which will be read in a block.  The number of messages
   that may be buffered is specified by the LINEBUF keyword (see header
   of PBX.C for full details).

   This code example is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied, 
   as to its usability in any given situation.
*/

// Includes
#define   INCL_DOS
#define   INCL_DOSERRORS
#include  <os2.h>

#include  <lan.h>

#include  <errno.h>
#include  <process.h>
#include  <stddef.h>
#include  <stdio.h>
#include  <string.h>

#include  "mail.h"
#include  "pbx.h"

// Function Prototypes (for functions not in PBX.H)
BOOL     LockSharedSeg  (void);
void     UnLockSharedSeg(void);
void     DropLine     (HPIPE hLine);
BOOL     SendPKT      (PHMSG FAR    *pbPhMsg,
                       USHORT       usSemIndex,
                       ROUTEENT FAR *pbREnt);
USHORT   TableOffset  (char *pszPhoneNumber);
void     Plugin       (PHMSG    FAR *pbPhMsg,
                       USHORT       usSemIndex,
                       ROUTEENT FAR *pbREnt);
void     Unplug       (USHORT       usSemIndex,
                       ROUTEENT FAR *pbREnt);
void     Call         (PHMSG    FAR *pbPhMsg,
                       USHORT       usSemIndex,
                       USHORT       usThreadNum,
                       ROUTEENT FAR *pbREnt);
void     Answer       (PHMSG    FAR *pbPhMsg,
                       USHORT       usSemIndex,
                       ROUTEENT FAR *pbREnt);
void     Hangup       (PHMSG    FAR *pbPhMsg,
                       USHORT       usSemIndex,
                       ROUTEENT FAR *pbREnt);
void     ListQuery    (PHMSG    FAR *pbPhMsg,
                       USHORT       usSemIndex,
                       ROUTEENT FAR *pbREnt);
BOOL     ProcessPacket(USHORT       usSemIndex,
                       USHORT       usThreadNum,
                       ROUTEENT FAR *pbREnt,
                       RTHREAD  FAR *pbRThread);


/* LockSharedSeg -----------------------------------------------------------
  Description:  Obtain shared memory segment locking semaphore
  Input      :  None
  Output     :  TRUE   --- Lock was obtained
                FALSE  --- Lock was not obtained
--------------------------------------------------------------------------*/

BOOL LockSharedSeg(void)
{
  USHORT  usRetCode;

  usRetCode = DosSemRequest(pbRMemory->hssmMemLock,
                            SEM_INDEFINITE_WAIT);
  if (usRetCode)
    ERRRPT("DosSemRequest", usRetCode)
  return (usRetCode == NO_ERROR ? TRUE : FALSE);
}


/* UnLockSharedSeg ---------------------------------------------------------
  Description:  Release shared memory segment lock
  Input      :  None
  Output     :  None
--------------------------------------------------------------------------*/

void UnLockSharedSeg(void)
{
  USHORT  usRetCode;

  usRetCode = DosSemClear(pbRMemory->hssmMemLock);
  if (usRetCode)
    ERRRPT("DosSemClear", usRetCode)
  return;
}


/* DropLine ----------------------------------------------------------------
  Description:  Drop a phone line by disconnecting the current client and
                resetting the line into the connect state.
  Input      :  hLine ---- Phone line handle
  Output     :  None
--------------------------------------------------------------------------*/

void DropLine(HPIPE hLine)
{
  USHORT  usRetCode;                      // Return code

  pbRMemory->usLinesUsed--;               // Decrement line count

  // Disconnect phone line
  usRetCode = DosDisConnectNmPipe(hLine);
  if (usRetCode)
    ERRRPT("DosDisConnectNmPipe", usRetCode)

  // Place phone line into connect state (for next incoming client)
  usRetCode = DosConnectNmPipe(hLine);
  if (usRetCode && usRetCode != ERROR_PIPE_NOT_CONNECTED)
    ERRRPT("DosConnectNmPipe", usRetCode)

  return;
}


/* SendPKT -----------------------------------------------------------------
  Description:  Send PHMSG structure on the supplied phone line
  Input      :  pbPhMsg ----- Pointer PHMSG message block
                usSemIndex -- Index into local section of routing table
                pbREnt  ----- Pointer to local routing table section
  Output     :  True if the request succeeds, False otherwise
--------------------------------------------------------------------------*/

BOOL SendPKT(PHMSG    FAR *pbPhMsg,
             USHORT       usSemIndex,
             ROUTEENT FAR *pbREnt)
{
  BOOL    fStatus = TRUE;                 // Assume success
  USHORT  usByteCnt;
  USHORT  usRetCode;

  // Obtain access to the pipe instance and write out the packet
  usRetCode = DosSemRequest(&(pbREnt[usSemIndex].ulLineSem), SEMTIMEOUT);
  if (usRetCode == NO_ERROR) {
    usRetCode = DosWrite(pbREnt[usSemIndex].hPhoneLine, // Phone line
                         pbPhMsg,                       // Packet
                         sizeof(PHMSG),                 // Packet size
                         &usByteCnt);                   // Bytes written
    if (usRetCode || usByteCnt != sizeof(PHMSG)) {
      ERRRPT("DosWrite", usRetCode)
      if (usRetCode == ERROR_BROKEN_PIPE) {
        Unplug(usSemIndex, pbREnt);       // Remove client from table
        fStatus = FALSE;
      }
    }
    DosSemClear(&(pbREnt[usSemIndex].ulLineSem));
  }
  else {
    ERRRPT("DosSemRequest", usRetCode)
    fStatus = FALSE;
  }
  return fStatus;
}


/* TableOffset -------------------------------------------------------------
  Description:  Locate a phone number in the routing table and return its
                offset
  Input      :  pszPhoneNumber -- Phone number to locate
  Output     :  Table offset of phone number if found
                usLineCnt if the phone number is not found
--------------------------------------------------------------------------*/

USHORT TableOffset(char *pszPhoneNumber)
{
  USHORT  usLineCnt;                      // Total number of phone lines
  USHORT  usOffset;                       // Offset into Routing Table

  // Check all table entries for one which is valid and whose
  // phone number matches the one supplied by the caller
  for (usOffset = 0, usLineCnt = pbRMemory->usLineCnt;
       usOffset < usLineCnt;
       usOffset++) {
    if (pbRMemory->pbRouteTable[usOffset].fState != Invalid &&
        !strcmp(pszPhoneNumber,
                pbRMemory->pbRouteTable[usOffset].pszPhoneNumber))
      break;
  }

  return usOffset;
}


/* Plugin ------------------------------------------------------------------
  Description:  Add a name to the routing table.
  Input      :  pbPhMsg ----- Pointer PHMSG message block
                usSemIndex -- Index into local section of routing table
                pbREnt  ----- Pointer to local routing table section
  Output     :  None
--------------------------------------------------------------------------*/

void Plugin (PHMSG    FAR *pbPhMsg,
             USHORT       usSemIndex,
             ROUTEENT FAR *pbREnt)
{
  USHORT  usTemp;                         // Temporary variable

  // See if the phone number already exists in the table
  usTemp = TableOffset(pbPhMsg->msg.pszPhoneNumber);

  // If the phone number was not found to already exist in the
  // routing table, then assign the current entry to the client
  if (usTemp >= pbRMemory->usLineCnt) {
    pbPhMsg->usResponse   = NO_ERROR;
    strcpy(pbREnt[usSemIndex].pszPhoneNumber, pbPhMsg->msg.pszPhoneNumber);
    // Inform client of maximum packet size
    pbPhMsg->msg.usLineBufCnt = pbRMemory->usLineBufCnt;
    pbREnt[usSemIndex].fState = Open;
    pbRMemory->usLinesUsed++;
  }

  // Phone number already exists in the table
  else
    pbPhMsg->usResponse = DUPLICATENUMBER;

  // Send client an acknowledgement, if it fails remove the client from
  // the routing table
  SendPKT(pbPhMsg, usSemIndex, pbREnt);
  return;
}


/* Unplug ------------------------------------------------------------------
  Description:  Remove a phone number from the routing table.
  Input      :  usSemIndex ---- Index into local section of routing table
                pbREnt -------- Pointer to local routing table section
--------------------------------------------------------------------------*/

void Unplug (USHORT       usSemIndex,
             ROUTEENT FAR *pbREnt)
{
  PHMSG   bPhMsg;                         // PHMSG block
  USHORT  usTemp;                         // Temporary variable

  // Invalidate the routing table entry owned by the client
  // If the phone line was involved in a connection, then forward
  // a hangup packet to the target client
  if (pbREnt[usSemIndex].fState == Busy) {
    bPhMsg.bMType = HANGUP;
    strcpy(bPhMsg.msg.pszPhoneNumber, pbREnt[usSemIndex].pszPhoneNumber);
    bPhMsg.usResponse = NO_ERROR;
    usTemp = pbREnt[usSemIndex].usConnection;
    pbRMemory->pbRouteTable[usTemp].fState = Open;
    SendPKT(&bPhMsg, 0, &(pbRMemory->pbRouteTable[usTemp]));
  }

  // Reset the phone line
  DropLine(pbREnt[usSemIndex].hPhoneLine);

  // Mark table entry as available
  pbREnt[usSemIndex].fState = Invalid;
  return;
}


/* Call --------------------------------------------------------------------
  Description:  Start establishing a connection between two phone lines
  Input      :  pbPhMsg ----- Pointer PHMSG message block
                usSemIndex -- Index into local section of routing table
                usThreadNum - Thread instance number
                pbREnt  ----- Pointer to local routing table section
  Output     :  None
--------------------------------------------------------------------------*/

void Call (PHMSG    FAR *pbPhMsg,
           USHORT       usSemIndex,
           USHORT       usThreadNum,
           ROUTEENT FAR *pbREnt)
{
  USHORT  usTemp;                         // Temporary variable

  // Get the offset into the routing table of the target client
  usTemp = TableOffset(pbPhMsg->msg.pszPhoneNumber);

  // Is the client trying to call their own phone?
  if (usTemp == ((usThreadNum * LINESPERTHREAD) + usSemIndex)) {
    pbPhMsg->usResponse = CANTCALLSELF;
  }

  // Was the target phone number in the routing table?
  else if (usTemp >= pbRMemory->usLineCnt) {
    pbPhMsg->usResponse = INVALIDNUMBER;
  }

  // Target phone number was located, is the line available?
  else if (pbRMemory->pbRouteTable[usTemp].fState != Open) {
    pbPhMsg->usResponse = LINEBUSY;
  }

  // Target client exists and is available
  else {
    // Place both lines into the Pending state and save the
    // routing table offsets
    pbREnt[usSemIndex].fState              = Pending;
    pbRMemory->pbRouteTable[usTemp].fState = Pending;
    pbREnt[usSemIndex].usConnection        = usTemp;
    pbRMemory->pbRouteTable[usTemp].usConnection =
      (usThreadNum * LINESPERTHREAD) + usSemIndex;

    // Forward the request on to the target client (passing the
    // the phone number of the caller)
    strcpy(pbPhMsg->msg.pszPhoneNumber, pbREnt[usSemIndex].pszPhoneNumber);
    pbPhMsg->usResponse = NO_ERROR;
    if (!SendPKT(pbPhMsg, 0, &(pbRMemory->pbRouteTable[usTemp]))) {
      pbREnt[usSemIndex].fState = Open;
      pbPhMsg->usResponse = INVALIDNUMBER;
    }
  }

  // Send client an acknowledgement, if it fails remove the client from
  // the routing table
  SendPKT(pbPhMsg, usSemIndex, pbREnt);
  return;
}


/* Answer ------------------------------------------------------------------
  Description:  Complete a connection between two clients
  Input      :  pbPhMsg ----- Pointer PHMSG message block
                usSemIndex -- Index into local section of routing table
                pbREnt  ----- Pointer to local routing table section
  Output     :  None
--------------------------------------------------------------------------*/

void Answer (PHMSG    FAR *pbPhMsg,
             USHORT       usSemIndex,
             ROUTEENT FAR *pbREnt)
{
  USHORT  usTemp;                         // Temporary variable

  // Get the offset of the target client (the caller)
  usTemp = pbREnt[usSemIndex].usConnection;

  // Is the caller still waiting for the connection?  If not,
  // re-open the callee's line.
  if(pbRMemory->pbRouteTable[usTemp].fState != Pending) {
    pbPhMsg->usResponse = CLIENTHANGUP;
    pbREnt[usSemIndex].fState = Open;
  }

  // Does the callee want to answer the call?  If not, re-open
  // both phone lines and forward the packet on to the caller
  else if (pbPhMsg->usResponse != NO_ERROR) {
    pbPhMsg->usResponse = DONTANSWER;
    pbREnt[usSemIndex].fState              = Open;
    pbRMemory->pbRouteTable[usTemp].fState = Open;
    if (!SendPKT(pbPhMsg, 0, &(pbRMemory->pbRouteTable[usTemp]))) {
      pbPhMsg->usResponse = CLIENTHANGUP;
    }
  }

  // Otherwise, complete the connection (since both still want it)
  else {
    // Mark both lines as Busy
    pbREnt[usSemIndex].fState              = Busy;
    pbRMemory->pbRouteTable[usTemp].fState = Busy;

    // Forward the request on to the target client (passing the
    // the phone number of the callee)
    strcpy(pbPhMsg->msg.pszPhoneNumber, pbREnt[usSemIndex].pszPhoneNumber);
    pbPhMsg->usResponse = NO_ERROR;
    if (!SendPKT(pbPhMsg, 0, &(pbRMemory->pbRouteTable[usTemp]))) {
        pbREnt[usSemIndex].fState = Open;
        pbPhMsg->usResponse = CLIENTHANGUP;
    }
  }

  // Send client an acknowledgement, if it fails remove the client from
  // the routing table and inform the newly connected target (if needed)
  if (!SendPKT(pbPhMsg, usSemIndex, pbREnt)) {
    if (pbPhMsg->usResponse == NO_ERROR) {    // Did a connection exist?
      pbPhMsg->bMType     = HANGUP;
      pbPhMsg->usResponse = NO_ERROR;
      strcpy(pbPhMsg->msg.pszPhoneNumber,
             pbREnt[usSemIndex].pszPhoneNumber);
      SendPKT(pbPhMsg, 0, &(pbRMemory->pbRouteTable[usTemp]));
    }
  }
  return;
}


/* Hangup ------------------------------------------------------------------
  Description:  Break a connection between two clients
  Input      :  pbPhMsg ----- Pointer PHMSG message block
                usSemIndex -- Index into local section of routing table
                pbREnt  ----- Pointer to local routing table section
  Output     :  None
--------------------------------------------------------------------------*/

void Hangup (PHMSG    FAR *pbPhMsg,
             USHORT       usSemIndex,
             ROUTEENT FAR *pbREnt)
{
  USHORT  usTemp;                         // Temporary variable

  // Get offset of potential target client
  usTemp = pbREnt[usSemIndex].usConnection;

  // If a connection exists, then forward the request on to the
  // target client (passing the phone number of the submitter)
  if (pbREnt[usSemIndex].fState              == Busy &&
      pbRMemory->pbRouteTable[usTemp].fState == Busy  ) {
    strcpy(pbPhMsg->msg.pszPhoneNumber, pbREnt[usSemIndex].pszPhoneNumber);
    pbPhMsg->usResponse = NO_ERROR;

    // Forward request to target client and place their line into the
    // Open state
    pbRMemory->pbRouteTable[usTemp].fState = Open;
    SendPKT(pbPhMsg, 0, &(pbRMemory->pbRouteTable[usTemp]));
  }

  // Place the line for the submitting client into the Open state
  pbREnt[usSemIndex].fState = Open;
  return;
}


/* ListQuery ---------------------------------------------------------------
  Description:  Send a list of all phone number currently registered in
                the routine table
  Input      :  pbPhMsg ----- Pointer PHMSG message block
                usSemIndex -- Index into local section of routing table
                pbREnt  ----- Pointer to local routing table section
  Output     :  None
--------------------------------------------------------------------------*/

void ListQuery (PHMSG    FAR *pbPhMsg,
                USHORT       usSemIndex,
                ROUTEENT FAR *pbREnt)
{
  USHORT      usLMsgSize;                 // LISTMSG size (in bytes)
  SEL         selLMsg;                    // LISTMSG selector
  LISTMSG FAR *pLMsg;                     // LISTMSG pointer
  USHORT      usLIndex;                   // Index into LISTMSG array
  USHORT      usByteCnt;                  // DosWrite byte count
  USHORT      usTemp;                     // Temporary variable
  char FAR    *pszTemp;                   // Temporary variable
  USHORT      usRetCode;                  // Return code

  // Determine how large of a LISTMSG block to allocate
  usLMsgSize = (pbRMemory->usLinesUsed * PHNUMSIZE) + sizeof(LISTMSG);

  // Allocate a segment to hold the LISTMSG block
  usRetCode = DosAllocSeg(usLMsgSize, &selLMsg, SEG_NONSHARED);
  if (usRetCode) {
    ERRRPT("DosAllocSeg", usRetCode)

    // Send acknowledgement anyway, using PHMSG block is safe because
    // the first two fields of both structures are identical
    pbPhMsg->usResponse = PBXERROR;
    SendPKT(pbPhMsg, usSemIndex, pbREnt);
  }

  // Build and send the LISTMSG block
  else {
    // Initialize the block
    pLMsg = MAKEP(selLMsg, 0);
    pLMsg->bMType       = LISTQUERY;
    pLMsg->usResponse   = NO_ERROR;
    pLMsg->usNumberCnt  = pbRMemory->usLinesUsed;

    // Copy all the names into the block
    for (usLIndex = usTemp = 0,
         pszTemp = ((char *)pLMsg) + sizeof(LISTMSG);
         usLIndex < pbRMemory->usLinesUsed &&
         usTemp   < pbRMemory->usLineCnt;
         usTemp++) {
      if (pbRMemory->pbRouteTable[usTemp].fState != Invalid) {
        strcpy(pszTemp, pbRMemory->pbRouteTable[usTemp].pszPhoneNumber);
        pszTemp += PHNUMSIZE;
        usLIndex++;
      }
    }

    // Obtain access semaphore and send the block to the client
    usRetCode = DosSemRequest(&(pbREnt[usSemIndex].ulLineSem), SEMTIMEOUT);
    if (usRetCode == NO_ERROR) {
      usRetCode = DosWrite(pbREnt[usSemIndex].hPhoneLine,
                           pLMsg, usLMsgSize, &usByteCnt);
      if (usRetCode || usByteCnt != usLMsgSize) {
        ERRRPT("DosWrite", usRetCode)
        if (usRetCode == ERROR_BROKEN_PIPE) {
          // Submitting client no longer exists, drop target from the
          // routing table
          Unplug(usSemIndex, pbREnt);
        }
      }
      DosSemClear(&(pbREnt[usSemIndex].ulLineSem));
    }

    // Access to the pipe could not be obtained, ACK an error anyway
    // to keep the client from hanging
    else {
      pbPhMsg->usResponse = PBXERROR;
      SendPKT(pbPhMsg, usSemIndex, pbREnt);
    }

    // Free the storage allocate for the LISTMSG structure
    usRetCode = DosFreeSeg(selLMsg);
    if (usRetCode)
      ERRRPT("DosFreeSeg", usRetCode)
  }
  return;
}


/* ProcessPacket -----------------------------------------------------------
  Description:  Read and respond to a packet that has arrived on a phone
                line (named-pipe instance)
  Input      :  usSemIndex -- MUXSEMLIST offset
                usThreadNum - Thread instance number
                pbREnt     -- Pointer to subset of routing table
                pbRThread  -- Pointer to Router info struct
  Output     :  None
--------------------------------------------------------------------------*/

BOOL ProcessPacket(USHORT       usSemIndex,
                   USHORT       usThreadNum,
                   ROUTEENT FAR *pbREnt,
                   RTHREAD  FAR *pbRThread)
{
  BOOL         fPktStatus;                // Packet status
  USHORT       usByteCnt;                 // Byte transfer count
  USHORT       usCurPkt;                  // Current packet instance
  USHORT       usPktCnt;                  // Total number of packets read
  USHORT       usTemp;                    // Temporary variable
  USHORT       usRetCode;                 // Return code

  fPktStatus = FALSE;                   // Assume no available packet

  // Read the packet(s) (after obtaining access semaphore)
  //   Use the semaphore index number to offset into the subset
  //   of the routing table managed by this thread
  usRetCode = DosSemRequest(&(pbREnt[usSemIndex].ulLineSem), SEMTIMEOUT);
  if (usRetCode == NO_ERROR) {
    usRetCode = DosRead(pbREnt[usSemIndex].hPhoneLine,
                        pbRThread->pbLineBuf,
                        pbRMemory->usLineBufSize,
                        &usByteCnt);
    DosSemClear(&(pbREnt[usSemIndex].ulLineSem));
    if (usRetCode && usRetCode != ERROR_NO_DATA)
      ERRRPT("DosRead", usRetCode)

    // Process the packet(s)
    if (usRetCode != ERROR_NO_DATA && usByteCnt != 0) {
      fPktStatus = TRUE;                  // Packet was found

      // Determine number of packets read
      usPktCnt = usByteCnt / sizeof(PHMSG);

      // Process each packet
      for (usCurPkt=0; usCurPkt < usPktCnt; usCurPkt++) {

        // Process TALK packets locall for fastest transfer
        if (pbRThread->pbLineBuf[usCurPkt].bMType == TALK) {
          // Forward the packet only if the line is Busy
          // Otherwise, ignore the packet
          if (pbREnt[usSemIndex].fState == Busy) {
            usTemp = pbREnt[usSemIndex].usConnection;
            SendPKT(&(pbRThread->pbLineBuf[usCurPkt]),
                    0, &(pbRMemory->pbRouteTable[usTemp]));
          }
        }

        // Process all other packets via specialized routines
        else {
          // All other packets require locked access to the global
          // routing table
          if (LockSharedSeg()) {
            switch ((pbRThread->pbLineBuf[usCurPkt]).bMType) {
              case PLUGIN   : Plugin(&(pbRThread->pbLineBuf[usCurPkt]),
                                     usSemIndex, pbREnt);
                              break;
              case UNPLUG   : Unplug(usSemIndex, pbREnt);
                              break;
              case CALL     : Call(&(pbRThread->pbLineBuf[usCurPkt]),
                                   usSemIndex, usThreadNum, pbREnt);
                              break;
              case ANSWER   : Answer(&(pbRThread->pbLineBuf[usCurPkt]),
                                     usSemIndex, pbREnt);
                              break;
              case HANGUP   : Hangup(&(pbRThread->pbLineBuf[usCurPkt]),
                                     usSemIndex, pbREnt);
                              break;
              case LISTQUERY: ListQuery(&(pbRThread->pbLineBuf[usCurPkt]),
                                        usSemIndex, pbREnt);
                              break;
              default       :
                ERRRPT("Unknown Packet Type",
                       (USHORT)(pbRThread->pbLineBuf[usCurPkt]).bMType)
                break;
            }
            UnLockSharedSeg();
          }

          // An error occurred while trying to obtain the shared memory
          // segment lock, send an ACK (with an error) for those requests
          // expecting it
          else {
            if (pbRThread->pbLineBuf[usCurPkt].bMType == PLUGIN ||
                pbRThread->pbLineBuf[usCurPkt].bMType == CALL   ||
                pbRThread->pbLineBuf[usCurPkt].bMType == ANSWER ||
                pbRThread->pbLineBuf[usCurPkt].bMType == LISTQUERY ) {
              pbRThread->pbLineBuf[usCurPkt].usResponse = PBXERROR;
              SendPKT(&(pbRThread->pbLineBuf[usCurPkt]),
                      usSemIndex, pbREnt);
            }
          }
        }
      }
    }
  }

  return fPktStatus;
}


/* Router ------------------------------------------------------------------
  Description:  See file header
  Input      :  usThreadNum -- Router thread instance number
  Output     :  None
--------------------------------------------------------------------------*/

void FAR Router(USHORT usThreadNum)
{
  PHMSG        bPhMsg;                    // PHMSG block (temporary)
  char         pszBuf[80];                // Temporary string buffer
  ROUTEENT FAR *pbREnt;                   // Map subset of routing table
  RTHREAD  FAR *pbRThread;                // Pointer to Router info struct
  AVAILDATA    bAvail;                    // DosPeekNmPipe info struct
  USHORT       usByteCnt;                 // DosPeekNmPipe byte count
  USHORT       usPipeState;               // DosPeekNmPipe pipe state
  USHORT       usSemIndex;                // Semaphore index
  USHORT       usTemp;                    // Temporary variable
  USHORT       usRetCode;                 // Return code

  // Set pointer to the information structure for this thread and
  // to that portion of the global routing table managed by this
  // thread
  pbRThread = &(pbRMemory->bRouters[usThreadNum]);
  pbREnt    = (pbRMemory->pbRouteTable) + (usThreadNum * LINESPERTHREAD);

  // Wait until the main thread has completed all initialization
  usRetCode = DosSemWait(&pbRThread->hsemPauseSem,
                         SEM_INDEFINITE_WAIT);

  // Process all incoming requests until told to exit or pause
  // The main thread will set hsemPauseSem to pause a Router thread
  // and hsemExitSem to indicate a shutdown should occur
  for (;;) {

    // Wait for input on one of the phone lines
    usRetCode = DosMuxSemWait(&usSemIndex,
                              &pbRThread->mxslSems,
                              SEM_INDEFINITE_WAIT);
    if (usRetCode)
      ERRRPT("DosMuxSemWait", usRetCode)

    // Requests to pause or exit should prevent the Router from processing
    // any more packets, therefore, check these semaphores after returning
    // from DosMuxSemWait

    // Check for pause
    usRetCode = DosSemWait(&(pbRThread->hsemPauseSem), SEM_INDEFINITE_WAIT);
    if (usRetCode)
      ERREXIT("DosSemWait", usRetCode)

    // Check for shutdown
    if (DosSemWait(&(pbRThread->hsemExitSem), SEM_IMMEDIATE_RETURN))
      break;

    // Process the data on the phone line
    //   If no packet was found on the pipe, then perhaps the client
    //   has terminated abnormally.  Check the pipe state and hangup
    //   if it's not still connected.
    if (!ProcessPacket(usSemIndex, usThreadNum, pbREnt, pbRThread)) {
      // Do not pass any buffer space since all that is needed is
      // the return code and pipe state
      usRetCode = DosPeekNmPipe(pbREnt[usSemIndex].hPhoneLine,
                                NULL,
                                0,
                                &usByteCnt,
                                &bAvail,
                                &usPipeState);

      // If the call failed or the pipe is not connected, then force an
      // unplug to occur
      if (usRetCode || usPipeState != NP_CONNECTED) {
        sprintf(pszBuf, "Connection to %s has been lost",
                pbREnt[usSemIndex].pszPhoneNumber);
        ERRRPT(pszBuf, usPipeState)
        Unplug(usSemIndex, pbREnt);
      }
    }

    // Reset the semaphore to wait for the next event
    usRetCode = DosSemSet(pbRThread->mxslSems.amxs[usSemIndex].hsem);
    if (usRetCode)
      ERRRPT("DosSemSet", usRetCode)
  }

  // The notification semaphore has been set by the main PBX.EXE thread
  // indicating that a shutdown should occur, notify all clients which
  // have a connection established to another client (the protocol does
  // not support a mechanism that can be used to notify all clients)
  bPhMsg.bMType     = HANGUP;
  bPhMsg.usResponse = SHUTDOWN;
  for (usTemp=0; usTemp < LINESPERTHREAD; usTemp++) {
    if (pbREnt[usTemp].fState == Busy) {
      // First, set line into a Pending state to prevent forwarding of the
      // hangup on to the target client (they will be notified by their
      // own Router thread)
      pbREnt[usTemp].fState = Pending;
      Hangup(&bPhMsg, usTemp, pbREnt);    // Notify client
    }
  }

  // Terminate processing
  // Inform the ExitHandler that this thread has completed by clearing
  // the exit semaphore (NOTE:  Do not call ErrorExit (i.e., ERREXIT)
  // if an error occurs since it calls ExitHandler also)
  usRetCode = DosSemClear(&(pbRThread->hsemExitSem));
  if (usRetCode)
    ERRRPT("DosSemClear", usRetCode)
  _endthread();
  return;
}
