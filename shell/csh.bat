@echo off

set TargetArch=x64
set CexecCacheDirectory=build\CexecCache

if "%1"=="--clear" ( 
    rmdir /S /Q %CexecCacheDirectory% 2>NUL
    goto exit
)

if not exist %CexecCacheDirectory% mkdir %CexecCacheDirectory%

rem shell\ctime -begin %CexecCacheDirectory%\ctime.ctm

cl.exe  1>NUL 2> NUL
if %ERRORLEVEL%==9009 (
echo Setting up vcvars...
    if "%TargetArch%"=="x64" (
        call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" > NUL
    ) else (
        call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars32.bat" > NUL
    )
)

For %%A in ("%1") do (
    set ScriptFileName=%%~nxA
    set ScriptPath=%%~dpA
    set ScriptFolder=%%~pA
)

set ScriptIntermediatesDirectory=%CexecCacheDirectory%\%ScriptFolder:\=^$%%ScriptFileName:\=^$%
if not exist %ScriptIntermediatesDirectory% mkdir %ScriptIntermediatesDirectory%

if %ScriptPath:~-1%==\ set ScriptPath=%ScriptPath:~0,-1%

forfiles /P %ScriptPath% /M %ScriptFileName% /C "cmd /c echo @fdate @ftime" >%ScriptIntermediatesDirectory%\time1.txt
if not exist %ScriptIntermediatesDirectory%\time.txt goto build

set /P PrevFileTime=<%ScriptIntermediatesDirectory%\time.txt
set /P CurrentFileTime=<%ScriptIntermediatesDirectory%\time1.txt

if "%CurrentFileTime%"=="%PrevFileTime%" goto run

:build
echo Recompilation...
del %ScriptIntermediatesDirectory%\executable.exe >NUL 2>&1
cl /nologo /W3 /WX /GR- /EHsc /std:c17 /Fo%ScriptIntermediatesDirectory%\ %1 /link /OUT:%ScriptIntermediatesDirectory%\executable.exe /PDB:%ScriptIntermediatesDirectory%\executable.pdb 1>%ScriptIntermediatesDirectory%\compiler-output.txt
if %errorlevel% == 0 goto run
type %ScriptIntermediatesDirectory%\compiler-output.txt
del %ScriptIntermediatesDirectory%\executable.exe >NUL 2>&1
del %ScriptIntermediatesDirectory%\time.txt >NUL 2>&1
goto exit

:run
copy %ScriptIntermediatesDirectory%\executable.exe %1.exe > NUL
%1.exe %*
del %1.exe >NUL 2>&1

echo %CurrentFileTime%>%ScriptIntermediatesDirectory%\time.txt
del %ScriptIntermediatesDirectory%\time1.txt >NUL 2>&1

:exit

rem shell\ctime -end %CexecCacheDirectory%\ctime.ctm

