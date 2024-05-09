@echo off
csc /nologo src/build/Build.cs src/build/BuildTools.cs
Build.exe
del Build.exe
:End
