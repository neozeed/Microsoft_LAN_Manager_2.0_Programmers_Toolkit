.***************************
.*  Help File For Phone
.***************************
:userdoc.
:title.Phone Help
:h1 res=607.Phone Help

:p. What is PHONE?

:artwork name='phone.bmp' align=left.

:p. PHONE is an MS (R) OS/2 Presentation Manager application that uses
  named pipes to enable any two users at an MS OS/2 workstation or
  server to call one another and converse. Each keystroke (actually
  the data from each Presentation Manager WM_CHAR message) is packaged
  and sent over the network, through a PBX to the recipient PHONE user
  (see the next section). The LOCAL section displays the keystrokes
  typed on the local machine; the REMOTE section displays those from
  the remote caller. The user starts phone calls (conversations) by
  choosing the CALL command. The user is presented with a list of known
  phone numbers (the names of other users registered on the same PBX)
  that they can call. The user ends a PHONE conversation by choosing
  the HANGUP command. When a call arrives from another PHONE user,
  PHONE rings to alert the user. The user can choose to answer or to
  ignore the call.
  
:p. Prior to starting, the workstation must be configured with the name
  of the LAN Manager server that is running the PBX service and with
  the phone number of the user (see the following section).

:p. What is PBX?

:p. PBX is a LAN Manager service that acts as the packet router. It
  maintains connections to individual PHONE users and routes data
  from one user to another (using a simple routing table). Each PHONE
  user requests a phone line (a named-pipe instance) from PBX and then
  registers (by supplying the phone number). When requested, PBX sends
  the phone numbers of all registered users to PHONE. PBX can be configured
  to support varying numbers of phone lines (one line is required by each
  user who intends to run PHONE) as well as the phone line buffer size (how
  many WM_CHAR message structures PHONE will collect before sending the data
  to the PBX service).

:p. Running PHONE

:p. To run PHONE, put the name of the server running the PBX service and the
  phone number of the workstation (any character string with as many as 15
  characters) into the [phone] section of the LANMAN.INI file on the local
  computer. For example, if the PBX service is running on a server named
  PBXHOST and the phone number of the workstation is KANSAS, LANMAN.INI
  would contain:
:sl compact.
:li.     [phone]
:li.         server = PBXHOST
:li.         number = KANSAS
:esl.


:p. Before running PHONE, the workstation must be started and the user logged
  on. The user must have IPC privilege.

:p. Running PBX

:p. Before starting PHONE on any workstations, the PBX LAN Manager service
  must be started. PBX creates a phone line for each PHONE user. A
  conversation, since it involves two users, requires two phone lines
  (one for each PHONE). By default, PBX supports 16 phone lines (as many
  as 8 simultaneous conversations). PBX also supports message packing to
  improve network traffic. By default, PBX packs up to 10 messages in a
  network packet. Both the number of phone lines and the messages per packet
  can be configured by specifying values in the LANMAN.INI file on the server
  running the PBX service. PBX reads these values from the section named [pbx]
  and uses the keywords LINES and LINEBUF. LINES can range from 6 through 200,
  supporting a maximum of 100 active conversations. LINEBUF can range from 1
  (no message packing) through 50. If incorrect values are supplied for a
  keyword, PBX continues to start, using the default value. To configure PBX
  to support 100 phone lines (up to 50 conversations) and packing of up to 20
  messages in a single network packet, LANMAN.INI should contain the following:

:sl compact.
:li.     [pbx]
:li.         line = 100
:li.         linebuf = 20
:esl.

:p. Then, in the [services] section of LANMAN.INI, add the following:

:sl compact.
:li.     [services]
:li.         pbx = C&colon.\PHONE\PBX.EXE
:esl.

:p. (This assumes you have put PBX.EXE in the C&colon.\PHONE directory.)

:p. Now, start PBX and share IPC$, as shown here:

:sl compact.
:li.     net start pbx
:li.     net share ipc$
:esl.

:p. If the server is running with user security, each user who may connect
  to the server must have IPC privileges.

:euserdoc.
