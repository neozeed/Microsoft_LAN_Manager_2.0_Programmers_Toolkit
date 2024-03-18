@Echo Off
Rem Copyright (C) 1990, Microsoft Corporation. All rights reserved.
Rem Batch file to remote exec a command [on some DMAKE slave]
if "%1" == "" goto Usage
(echo exec_it: & echo	%1 %2 %3 %4 %5 %6 %7 %8 %9) | dmake /f - /local:0 /nologo
goto end
:Usage
echo Usage: remote command [args ...]
:end
