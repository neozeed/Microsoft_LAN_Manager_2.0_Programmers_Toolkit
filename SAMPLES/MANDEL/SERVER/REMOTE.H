/****************************************************************************

    REMOTE.H -- Data structures and definition for remote server
		functionality.

    Copyright (C) 1990 Microsoft Corporation.

****************************************************************************/


#define     TYPE_ECHOPACKET	    0


#define     MAILSLOT_REQUEST	"\\MAILSLOT\\MREQUEST"
#define     MAILSLOT_REPLY	"\\MAILSLOT\\MREPLY"
#define     PIPE_SERVER 	"\\PIPE\\MANDEL"


extern HSEM hsemBitmap;
extern PULONG pvBitmapData;
extern RECTL	rclBitmapData;

typedef struct _calcbuf {
    RECTL   rclDraw;
    double  dblPrecision;
    ULONG   ulThreshold;
    CPOINT  cptLL;
} CALCBUF;


typedef struct _echopacket {
    USHORT	usType;
    char	szServerName[2 + CNLEN + 1];
}   ECHOPACKET;

typedef ECHOPACKET far * PECHOPACKET;


void InitCollect( HAB );
void FindMandelServers( void );


void
RemoteMandelCalc(HPS	    hps,
		 HWND	    hwnd,
		 PCPOINT    pcptLL,
		 PRECTL     prcl,
		 double     precision,
		 ULONG	    ulThreshold
		);
