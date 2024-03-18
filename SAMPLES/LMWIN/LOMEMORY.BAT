@echo off

REM   This batch file is intended to be used in situations when the C 
REM   compiler runs out of heap space (or other low-memory situations).
REM   LMWIN.EXE can be produced by running this batch file rather than
REM   calling NMAKE or NMK. This file invokes NMAKE long enough to create
REM   a batch file of the compilation commands needed, then runs those
REM   commands without NMAKE loaded. This saves some memory by not having
REM   NMAKE present at compile time.

nmake clean
nmake lmwin.def
nmake /n /NOLOGO > lomem.bat
call lomem.bat
del lomem.bat
