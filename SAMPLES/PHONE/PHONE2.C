/*---------------------------------------------------------------
   PHONE2.C -- PHONE UTILITY

   Copyright (C) 1990 Microsoft Corporation 
  
   This code sample is provided for demonstration purposes only.
   Microsoft makes no warranty, either express or implied, as to
   its usability in any given situation.
  --------------------------------------------------------------*/

#define INCL_WIN   // OS2 header files
#define INCL_GPI
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSMEMMGR
#define INCL_DOSNMPIPES
#define INCL_DOSERRORS
#include <os2.h>

#include <stdio.h>    // C header files
#include <stddef.h>
#include <stdlib.h>
#include <process.h>
#include <ctype.h>
#include <conio.h>
#include <string.h>
#include <malloc.h>

#define  INCL_NETERRORS   // Lan header files
#include <lan.h>


#include "phone.h"        // Phone specific files
#include "easyfont.h"
#include "mail.h"


extern VOID FAR PASCAL TmpStack(int);

VOID FAR ErrorReport( PSZ, USHORT);
INT  NetCall(HWND, PCHAR);
INT  NetAns();
VOID Hangup();

VOID FAR  TalkThread( );
VOID FAR  ConnThread( );
VOID FAR RingRing(VOID);
INT FAR  ReadPipe(PHMSG *) ;
VOID   NetClose     ( VOID);
SHORT PASCAL FAR Scroll(INT,INT,  CHAR FAR *);

                             // Talk Thread
extern VOID *pTalkStack;
extern INT  tidTalk;
                            // Conn Thread
extern VOID *pConnStack;
extern INT  tidConn;

                            // Semaphores  for thread and pipe control
extern  HSYSSEM hSemPipe;
extern  HSYSSEM hSemThread;
extern  HSYSSEM hSemConn;
extern  HSYSSEM hSemBlockConn;

                             // Global parameters

PHMSG   *pPhWriteBuf;                       // Write buffer
USHORT  usPhWriteBufMax;                    // Size of Write Buffer
extern CHAR    szName[PHNUMSIZE];           // Phone Number
extern CHAR    szPipeName[PIPENMSIZE];	    // Pipe name
extern CHAR    szRemoteGuy[PHNUMSIZE];	    // Remote Phone Number
extern HPIPE hPipe;                         // Pipe Handle
extern BOOL fPlugSent;                      // PLUGIN has (not) been sent
extern BOOL  fConnectionMade;               // Connection made/not-made

extern CHAR szClientClass[];	   /*  "Phone" */
extern HAB  hab ;
extern HWND hwndClient;

VOID FAR Hangup()
{

    USHORT rc;
    PHMSG  phMessage;
    USHORT usBytesW;

    // Stop all transmissions; This flag is checked in WM_CHAR
    // section in PM code.

    fConnectionMade = FALSE;

                               // Tell the thread (Talk) to die
    rc = DosSemSet(hSemThread);
    if (rc)
            ErrorReport("DosSemSet", rc);

                               // Give them a little time
    DosSleep(2* 1000L);

    // Seize the pipe

    DosSemRequest(hSemPipe, SEM_INDEFINITE_WAIT);


    // Send a HANGUP message

    phMessage.bMType = HANGUP ;

    rc = DosWrite(hPipe,
                  (PVOID) &phMessage,
                  sizeof(PHMSG),
                  &usBytesW);
    if (rc)
        ErrorReport("Error sending HANGUP message: DosWrite", rc);

    // Set the pipe mode to Blocking again
    rc = DosSetNmPHandState(hPipe, NP_WAIT| NP_READMODE_MESSAGE);

    // Release the pipe
    DosSemClear(hSemPipe);

    // UnBlock Conn thread
    rc = DosSemClear(hSemBlockConn);
    if (rc)
            ErrorReport("DosSemClear", rc);

    return;
}


VOID FAR ErrorReport(PSZ pCode, USHORT rc)
{

    CHAR  chzMessage[80];  // Message buffer
    CHAR  chzBuffer[10];   // Return code buffer
    CHAR *pszMess;         // Pointer to message buffer
    CHAR FAR *pszCode;     // Pointer to error message

    pszMess = (CHAR *) chzMessage;  // Assign pointers to message buffer
    pszCode = (CHAR FAR *) pCode;   // Assign pointer to error message

    while (*pszMess++ = *pszCode++);  // Copy into a local buffer

    strcat(chzMessage, " Error #");   // Append error number

    if (strlen(pszMess) < 70)
        strncat(chzMessage, itoa(rc, chzBuffer, 10),10);

    // Send a popup

    WinMessageBox (HWND_DESKTOP,
                   hwndClient,
                   chzMessage,
                   szClientClass,
                   0,
                   MB_OK | MB_ICONASTERISK) ;

    return;
}


// This function is called to initiate a conversation to remote person.
// It gets the phone number in szBuff parameter. It will send a CALL
// packet to Router which will forward the packet to the remote person
// Router immediately sends a ACK packet to notify if callee is busy.
// This function will then wait for callee to respond with an ANS packet.

INT NetCall(HWND hwnd, PCHAR szBuff)
{

    USHORT rc;                           // Return Code
    CHAR szMessage[80];                  // Error buffer
    PHMSG phReadBuf;                     // Buffer to read
    PHMSG phWriteBuf;                    // Buffer to write
    USHORT  usResponse;                  // Response from user
    USHORT  usBytesR;                    // Bytes read
    USHORT  usBytesW;                    // Bytes written
    USHORT  usBytesA;			 // Bytes available
    USHORT  usPipeState; 		 // Pipe state
    USHORT  usLoopCount = 0;             // Loop count while waiting for ANS
                                         // message

    // Call the router running on a OS/2 server
    phWriteBuf.bMType = CALL;            // Call
    strcpy(phWriteBuf.msg.pszPhoneNumber, szBuff);
					 // Phone no. of remote person

    // Seize the pipe
    DosSemRequest(hSemPipe, SEM_INDEFINITE_WAIT);

    //Send CALL message
    rc = DosWrite(hPipe,
	          (PVOID) &phWriteBuf,
		  sizeof(PHMSG),
                  &usBytesW);
     if (rc)
     {
         ErrorReport("Sending CALL (DosWrite)", rc);
         DosSemClear(hSemPipe);
         return ERROR;
     }

     // Now, wait for Ack from router
     rc = DosRead(hPipe,
		  (PVOID) &phReadBuf,
                  sizeof(PHMSG),
                  &usBytesR);
     if (rc)
     {
         ErrorReport("Reading Ack of CALL (DosRead)", rc);
         DosSemClear(hSemPipe);
         return ERROR;
     }

     // Check for hangup
     if (rc = ReadPipe( &phReadBuf) == ERROR)
     {
            DosSemClear(hSemPipe);
            return ERROR;
     }

     // Did router return an error?
     if (phReadBuf.usResponse != NO_ERROR)
     {
          switch (phReadBuf.usResponse)
	  {

              case PBXERROR:
                           ErrorReport("Unexpected PBX error", 0);
                           break;
	      case NOMORELINES:
                           ErrorReport("Server is out of phone lines", 0);
                           break;
              case INVALIDNUMBER:
                           ErrorReport("Invalid target phone number", 0);
                           break;
              case LINEBUSY:
                           ErrorReport("Target phone line is busy", 0);
                           break;
              default:
                           ErrorReport("Router could not connect",
                                       phReadBuf.usResponse);
                           break;

         }
         DosSemClear(hSemPipe);
         return ERROR;
    }

    // At this time, router has forwarded CALL message to remote person
    // We have to keep looking for a ANS message from remote person.

    usResponse = MBID_YES;
    do
    {
        rc = DosPeekNmPipe(hPipe,
			   (PBYTE) &phReadBuf,
                           sizeof(phReadBuf),
                           &usBytesR,
			   (PAVAILDATA) &usBytesA,
			   &usPipeState);
        if (rc)
	{
            ErrorReport ("DosPeekNmPipe", rc);
            DosSemClear(hSemPipe);
            return ERROR;
        }

        if (usBytesR)   // Some message has arrived
	{
            rc = DosRead(hPipe,
                         (PVOID) &phReadBuf,
                         sizeof(PHMSG),
                         &usBytesR);
            if (rc)
	    {
                ErrorReport("Reading Ack of CALL (DosRead)", rc);
                DosSemClear(hSemPipe);
                return ERROR;
            }
            // Check for hangup
            if (rc = ReadPipe( &phReadBuf) == ERROR) {
                 DosSemClear(hSemPipe);
                 return ERROR;
            }
            // Ignore all messages other than ANS
 
	    if (phReadBuf.bMType == ANSWER)
	    {
		// Check the response returned
		if (phReadBuf.usResponse != NO_ERROR)
		{
                    switch (phReadBuf.usResponse)
		    {

                        case PBXERROR:
                            ErrorReport("Unexpected PBX error", 0);
                            break;
                        case DONTANSWER:
                            sprintf(szMessage, "%s %s",
                                    szBuff,
                                    " does not want to talk.");
                            WinMessageBox (HWND_DESKTOP,
                                           hwndClient,
                                           szMessage,
                                           szClientClass,
                                           0,
                                           MB_OK |
                                           MB_ICONEXCLAMATION);
 
                            break;

                        default:
                            ErrorReport("Router could not connect",
                                        phReadBuf.usResponse);
                            break;

                    }
                    DosSemClear(hSemPipe);
                    return ERROR;
	        }

                // Remote person responded that he wants to talk.
                // break out of while loop
                else
                    break;

	    }
	}

        // Wait for a 1/2 seconds.
        DosSleep(500L);

        // Give users a popup every 10 second only
        if (usLoopCount < 20)
            usLoopCount++;
        else
            usResponse =WinMessageBox(HWND_DESKTOP,
                                      hwndClient,
                                     "Waiting to connect! Want to Continue?",
                                      szClientClass,
                                      0,
                                      MB_YESNO | MB_ICONQUESTION  ) ;

    } while (usResponse==MBID_YES);

    if (usResponse == MBID_NO)
    {
        phWriteBuf.bMType = HANGUP;            // Send a hangup
        strcpy(phWriteBuf.msg.pszPhoneNumber, szBuff);
        rc = DosWrite(hPipe,
                      (PVOID) &phWriteBuf,
                      sizeof(PHMSG),
                      &usBytesW);
        DosSemClear(hSemPipe);
	return NO;
    }

    // The remote person wants to talk. Block the Conn thread
    DosSemSet(hSemBlockConn);

    // Release the pipe
    DosSemClear(hSemPipe);

    // Copy remote person's name in szRemoteGuy
    strcpy(szRemoteGuy, szBuff);

    // Start the Talk thread
    pTalkStack = (VOID *) (malloc(STACKSIZE));

    if (pTalkStack != 0 )
    {
        // fire up the thread here

        // Set the pipe in Non-Blocked Message mode.
        rc = DosSetNmPHandState(hPipe,
                                NP_NOWAIT| NP_READMODE_MESSAGE);


        tidTalk = _beginthread(TalkThread,
                               pTalkStack,
                               STACKSIZE,
                               NULL);
        if (tidTalk == -1)
	{
             ErrorReport("BeginThread", rc);
             return ERROR;
        }

    }

    DosSemClear(hSemThread);  // Thread semaphore could have
                              // been set during last Hangup
    return YES;               // Create the Call thread

}


// NetAns function is indirectly called when a CALL packet comes
// from remote caller. NetAns function establishes a connection
// to remote caller thru the router and spawns off the Talk thread.

INT NetAns()
{

    USHORT rc;                 // ReturnCode
    USHORT usResponse;         // User response
    PHMSG  phMessage;          // A PHMSG message
    CHAR   szMessage[80];      // Message buffer
    USHORT usBytesR;           // Bytes read
    USHORT usBytesW;           // Bytes written



    // Give a ring and a popup to user to see if (s)he wants to
    // respond to a call from Caller.

    RingRing();             // Give a ring

			    // Give a popup
    sprintf(szMessage,
	    "%s is Calling you. Do you want to answer?",
	    szRemoteGuy);

    usResponse = WinMessageBox (HWND_DESKTOP,
				hwndClient,
				szMessage,
				szClientClass,
				0,
				MB_YESNO | MB_ICONQUESTION  );


    if (usResponse==MBID_NO)   // No, don't want to talk
    {
                               // Tell the remote caller
         phMessage.bMType = ANSWER;
         phMessage.usResponse = DONTANSWER;
	 strcpy(phMessage.msg.pszPhoneNumber, szRemoteGuy);
         rc = DosWrite(hPipe,
                       (PVOID) &phMessage,
                       sizeof(PHMSG),
                       &usBytesW);
         if (rc)
	 {
             ErrorReport("DosWrite", rc);
             return ERROR;
         }

                               // Read off the Ack packet
         rc = DosRead(hPipe,
                      (PVOID) &phMessage,
                      sizeof(PHMSG),
                      &usBytesR);
         if (rc)
	 {
             ErrorReport("DosRead", rc);
             return ERROR;
         }

         return NO;

     }
     else if (usResponse==MBID_YES) {     // OK button was selected

         phMessage.bMType = ANSWER;
         phMessage.usResponse = NO_ERROR;
	 strcpy(phMessage.msg.pszPhoneNumber, szRemoteGuy);
         rc = DosWrite(hPipe,
                       (PVOID) &phMessage,
                       sizeof(PHMSG),
                       &usBytesW);
         if (rc)
	 {
             ErrorReport("DosWrite", rc);
	     return ERROR;
	 }

                     // Read Ack packet. It may be totally unknown
                     // type packet such as somebody else calling
                     // or remote caller may have hung up which
                     // router will tel us thru Ack packet

         rc = DosRead(hPipe,
                      (PVOID) &phMessage,
                      sizeof(PHMSG),
                      &usBytesR);
         if (rc)
	 {
              ErrorReport("DosRead", rc);
              return ERROR;
         }
         else if (rc=ReadPipe(&phMessage) == ERROR)
              return ERROR;

         if (( phMessage.bMType == ANSWER) &&
                   (phMessage.usResponse == NO_ERROR))
         {
                     //  Start the Talk Thread

             pTalkStack = malloc(STACKSIZE);

             if (pTalkStack != 0 )
	     {
                 rc = DosSemClear (hSemThread); // It may have been set
                                                // during last Hangup
                                                // fire up the thread here

                 // Set the pipe in Non-Blocked Message mode.
                 rc = DosSetNmPHandState(hPipe,
                                         NP_NOWAIT| NP_READMODE_MESSAGE);

                 tidTalk = _beginthread(TalkThread,
                                        pTalkStack,
                                        STACKSIZE,
                                        NULL);
		 if (tidTalk == -1)
		 {
                      ErrorReport("BeginThread", rc);
                      return ERROR;
                 }
                 return YES;
             }
         }
         else
	 {
	     sprintf(szMessage, "%s%s",
                     szRemoteGuy,
                     " did not wait for your answer");
             WinMessageBox (HWND_DESKTOP,
                            hwndClient,
                            szMessage,
                            szClientClass,
                            0,
                            MB_OK | MB_ICONQUESTION  );

             return ERROR;
	 }
     }
     return ERROR;
}


// TalkThread is a thread which reads in messages sent by remote caller
// and sends them as PM messages to the PM client window which
// displays the incoming messages on Remote Caller Window.

VOID FAR TalkThread(PVOID arglist )
{

    PHMSG phReadBuf[10];            // Message buffer
    USHORT usPktCnt, usCurPkt;      // blah
    USHORT rc;                      // Return code
    CHARMESSAGE *pcChar;            // Pointer of CHARMESSAGE type
    USHORT usBytesR;                // Bytes read
    HAB hab;                        // Handle to the anchor block
    HMQ hmq;                        // Handle to the message queue
    USHORT usRead;                  // DosRead return code

	//Start the Message Queue
    hab = WinInitialize((USHORT)NULL);
    hmq = WinCreateMsgQueue(hab, 100);

	// Tell the user to type now

    fConnectionMade = TRUE;  // Connection has been made

    WinMessageBox (HWND_DESKTOP, hwndClient,
		  "Connection Made! You can type now!",
		   szClientClass, 0, MB_ICONASTERISK | MB_OK ) ;


          //Now, we will loop and wait for a message. We will
          //send the message as a PM WM_REMOTE message to
          //client window which will display the message in
          //remote window.

    //Lower thread priority as WinCreateMsgQueue raises priority

    rc = DosSetPrty(PRTYS_THREAD, PRTYC_IDLETIME, 0, 0);
    if (rc)
	ErrorReport("DosSetPrty", rc);

    for (;;)  /* forever */
    {
                                         // Usually this semaphore will be
                                         // clear.
	rc = DosSemWait(hSemThread, THREAD_SEM_WAIT);

	if (rc)
	{                                // Semaphore has been set
                                         // Stop the thread
            goto Kill_Thread_Talk;
        }
                          // Get acess to pipe
        rc = DosSemRequest(hSemPipe, SEM_INDEFINITE_WAIT);
             // Check the status of the pipe
        do   // while there is any more data
	{
	    usRead =  DosRead(hPipe,
                              (PVOID)&phReadBuf,
                              10*sizeof(PHMSG),
                              &usBytesR);

            // Release the pipe.
            DosSemClear(hSemPipe);

            if ((usBytesR==0) || // No data
                (usRead==ERROR_NO_DATA))
	    {
		continue;
            }
	    else
	    {
                if ((usRead) && (usRead!=ERROR_MORE_DATA))
		{
                    ErrorReport("Talk Thread: DosRead(Pipe)", usRead);
                    goto Kill_Thread_Talk;
                }
                            // Check for Hangup and Ignore other
                            // than TALK type ,message


                usPktCnt = usBytesR / sizeof(PHMSG);
                for (usCurPkt=0; usCurPkt < usPktCnt; usCurPkt++)
		{
                    if (rc = ReadPipe(&(phReadBuf[usCurPkt])) != ERROR)
                        if ( phReadBuf[usCurPkt].bMType==TALK)
			{
                            // Extract mp1 and mp2 parameters from PHMSG
                            // type message and send to client window.

                            pcChar =(CHARMESSAGE *)&(phReadBuf[usCurPkt].msg.bMsg);
                            WinSendMsg( hwndClient, WM_REMOTE,
                                        pcChar->mp1, pcChar->mp2);

			}
		}

	    }
	} while (usRead==ERROR_MORE_DATA);


        DosSleep(100L); // give up current time slice


    }   // End of for loop

Kill_Thread_Talk:
    DosSemClear(hSemPipe);
    WinDestroyMsgQueue(hmq);
    WinTerminate(hab);
    TmpStack(1);
    free(pTalkStack);
    pTalkStack = NULL;
    _endthread();
    return;
}


/* This thread will start dialogue in response to CALL, HANGUP
 * message types. It will wait for RinRing to send a CALL packet
 * in which case, it will send a IDM_ANSWER message to PM which
 * in turn will initiate Answering process. It will also send
 * IDM_HANGUP message when RingRing routes the HangUp packet
 * from remote person.
 */

VOID FAR ConnThread(PVOID arglist)
{

    USHORT rc;
    HAB hab;                             // Handle to the anchor block  */
    HMQ hmq;                             // Handle to the message queue */
    CHAR szMessage[80];                  // Error messages
    PHMSG phMessage;                     // Phone Message Unit
    USHORT usBytesW;                     // Bytes written
    USHORT usBytesR;                     // Bytes read
    USHORT usBytesA;                     // Bytes available
    USHORT usPipeState;		       // Pipe state
               // Start the Message Queue
    hab = WinInitialize((USHORT)NULL);
    hmq = WinCreateMsgQueue(hab, 100);


    // Register with server so that anybody can call us

    phMessage.bMType = PLUGIN;
    strcpy(phMessage.msg.pszPhoneNumber, szName);

    if (rc =DosSemRequest(hSemPipe, PIPE_SEM_WAIT)!=0) {
        ErrorReport("DosSemRequest error while registering", rc);
        goto Kill_Thread_Conn;
    }


    rc = DosWrite(hPipe,
                 (PVOID) &phMessage,
                 sizeof(PHMSG),
                 &usBytesW);
    if (rc) {
          ErrorReport("DosWrite error while registering", rc);
          goto Kill_Thread_Conn;
    }
       // Wait for a ACK packet.
    rc = DosRead(hPipe,
                 (PVOID)&phMessage,
                 sizeof(PHMSG),
                 &usBytesR);
    if (rc) {
         ErrorReport("DosRead error while registering", rc);
         goto Kill_Thread_Conn;
    }
    if ((rc= ReadPipe(&phMessage)==ERROR)||(phMessage.bMType != PLUGIN))
    {
        ErrorReport("Error in connecting to server", 0);
        goto Kill_Thread_Conn;
    }
    else if (phMessage.usResponse==NOMORELINES){
        ErrorReport("All phone lines at server at busy. Restart Phone", 0);
        goto Kill_Thread_Conn;
    }
    // Find out the Line Buffer Count. This count determines the size
    // of DosWrite packets to PBX.

    usPhWriteBufMax = phMessage.msg.usLineBufCnt + 1;  // maximum buffer size
    pPhWriteBuf = (PHMSG *) malloc(sizeof(PHMSG)*usPhWriteBufMax);

    // PLUGIN has been sent
    fPlugSent = TRUE;

    DosSemClear(hSemPipe);       // Let other use pipe now


    //Lower thread priority as WinCreateMsgQueue raises priority

    rc = DosSetPrty(PRTYS_THREAD, PRTYC_IDLETIME, 0, 0);
    if (rc)
        ErrorReport("DosSetPrty", rc);

    // Now wait in a loop listening to all messages coming on the
    // pipe until Talk thread take over the pipe.


    for (;;)
    {
         DosSleep(1000L);   // This will slow down the thread

         rc = DosSemWait(hSemConn, THREAD_SEM_WAIT);

         if (rc)                        // Semaphore has been set
                                        // Stop the thread
                                        // This semaphore is
                                        // set only when exiting Phone
              goto Kill_No_Message;

         // When Talk thread is using the pipe, there is
         // no need for Conn Thread to do any thing. So,
         // we will make him wait forever and do nothing.
         // However, we will turn Conn thread into a Timer
         // thread, which will send WM_TIMER messages at every
         // WRITE_TIME_OUT interval so that buffer will be flushed.

         do {
             rc = DosSemWait(hSemBlockConn, WRITE_TIME_OUT);

             if (rc==ERROR_SEM_TIMEOUT) {
                        // Call WM_TIMER to send out the buffer

                  WinSendMsg(hwndClient,
                             WM_TIMER,
                             MPFROM2SHORT(ID_TIMER, 0),
                             0);


             }
             else if (rc) {
                  ErrorReport("DosSemWait Error in Conn", rc);
                  goto Kill_Thread_Conn;
             }

         } while (rc==ERROR_SEM_TIMEOUT);

                            // Phone just installed or HungUp last
                            // session. So, we monitor the pipe and
                            // and look for CALL or HANGUP message

                            // First seize the pipe
	 DosSemRequest(hSemPipe, SEM_INDEFINITE_WAIT);

         rc = DosPeekNmPipe( hPipe,      // Pipe handle
                             (PBYTE)&phMessage, // Buffer
                             sizeof(phMessage), // Size of buffer
                             &usBytesR,         // Bytes read
                             (PAVAILDATA) &usBytesA, // Bytes Available
			     &usPipeState);	    // Pipe state

         if (rc)            // Error Condition
	 {
              ErrorReport("Disconnected from PBX: DosPeekNmPipe", rc);
              DosSemClear(hSemPipe);
              goto Kill_Thread_Conn;
         }

         if (usBytesR)   // Some message has arrived
	 {
              rc = DosRead(hPipe,   // Pipe Handle
                           (PVOID) &phMessage, // Buffer
                           sizeof(PHMSG),      // Size of buffer
                           &usBytesR);


              if (rc)   // Error Condition
	      {
                   ErrorReport("Conn Thread DosRead", rc);
	      }
	      else if ( rc = ReadPipe(&phMessage) != ERROR) {
		  // Is it a CALL packet
		  if (phMessage.bMType == CALL)
		  {
			      // Copy the Caller's Name
		      strcpy(szRemoteGuy, phMessage.msg.pszPhoneNumber);
                              // Now, block the connection thread
                      DosSemSet(hSemBlockConn);

                      WinPostMsg( hwndClient, // Send to Client Window
                                  WM_ANSWER, // Type of Message
                                  0L, 0L);

                  }
	      }
          }

                  // Release the pipe
          DosSemClear(hSemPipe);

      }


Kill_Thread_Conn: // When this thread dies, we  will ask user to
                  // restart PHONE. In next version, we will
                  // allow reconnecting to a restarted ringring

    sprintf(szMessage,"%s",
	    "Connection to server broken. Restart Phone");

    WinMessageBox (HWND_DESKTOP,
 	           hwndClient,
		   szMessage,
		   szClientClass,
		   0,
                   MB_OK ) ;

Kill_No_Message:                    // It is important to
				    // not send messages
                                    // at this time.
    WinDestroyMsgQueue(hmq);        // Main thread may have
    WinTerminate(hab);              // died and so there
    TmpStack(1);                    // is no message queue
    free(pConnStack);
    pConnStack = NULL;
    _endthread();

    return;

}


// This function will close the pipe and take any other necessary action
// to clean up connections.

VOID NetClose ( VOID)
{
    DosClose(hPipe);
    return;
}



SHORT PASCAL FAR  Scroll(int xMaxSc, int  yMaxSc, CHAR FAR * pBuffer)
{
    int s, rc;
    CHAR ch;

    for (s = 0; s < yMaxSc/2; s++)
        for (rc = 0; rc < xMaxSc; rc++)
        {
	    ch =  *(pBuffer + rc +
                    xMaxSc * (s+(yMaxSc/2)));    //Copy from lower
            *(pBuffer + rc +  xMaxSc * s) = ch;  //line to upper line
                                                 // blank out lower line
            *(pBuffer + rc +  xMaxSc * (s+(yMaxSc/2))) =' ';
        }
    // Take care of last line if yMaxSc is odd


    if (yMaxSc %2 != 0) {
        for (rc = 0; rc <xMaxSc; rc++) {
            ch = *(pBuffer + rc + xMaxSc* (yMaxSc -1)); // last line
            *(pBuffer + rc +  xMaxSc * (yMaxSc/2)) = ch;
            *(pBuffer + rc + xMaxSc* (yMaxSc -1)) = ' ';
        }
        return (yMaxSc/2 + 1);
    }
    else
        return (yMaxSc /2) ;
}


// This routine makes a phone ringing sound.

VOID FAR RingRing(VOID)
{

    register i;        // Tune for phone ring
    static INT iTune[]={ 392, 262 ,392 , 262 , 392 ,
                         262 ,392 ,262 , 392 , 262 ,
                         392 ,262 ,392 , 262 , 392 ,
                         262 ,392 ,262 , 392 , 262 };
 

                        // First Ring
    for (i=0; i<19; DosBeep(iTune[i], 50), i++)
        ;
                        // Sleep for 2 second
    DosSleep(2000L);
                        // Second Ring
    for (i=0; i<19; DosBeep(iTune[i], 50), i++)
        ;

    return;
}


// This Routine will read messages coming on pipe and take necessary
// action for following two tests:
// 1. HANGUP:
//      CLIENTHUNGUP: Clear thread (Talk) semaphores
//      SHUTDOWN: Send Messages to PM Window to shutdown
// 2. Check if message coming on pipe is of PHMSG type
// Return values: ERROR, NO_ERROR

INT FAR  ReadPipe(PHMSG *phMessage)
{

    CHAR   szMessage[80];

    if (sizeof( *phMessage) != sizeof(PHMSG)) // Test size of packet
        return ERROR;
    switch(phMessage->bMType)
    {

	case HANGUP:   // RingRing or the Server is going down

             if (phMessage->usResponse==SHUTDOWN)
                       // Send ShutDown Message to PM

                 sprintf(szMessage, "%s %s",
                         "PBX went down!\n",
                         "Hanging Up");

                       // Either the remote client has send a
                       // normal HANGUP packet or server detected
                       // that remote client has died/broken pipe
             else
                 sprintf(szMessage, "%s%s",
                         szRemoteGuy,    // Name of remote client
                         " Hung up!\nHanging Up!");

             WinMessageBox (HWND_DESKTOP,
                            hwndClient,
                            szMessage,
                            szClientClass,
                            0,
                            MB_OK ) ;

             if (!WinPostMsg( hwndClient, // Send to Client Window
                              WM_COMMAND, // Type of Message
                              MPFROM2SHORT(IDM_HANGUP, 0),
                              0))
                  DosBeep(600,175);

             break;

        default:   return NO_ERROR;

    }
    return ERROR;
}
