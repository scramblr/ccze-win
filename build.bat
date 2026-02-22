@echo off
setlocal

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64

set VCPKG_INC=C:\Users\user\github\vcpkg\installed\x64-windows-static\include
set VCPKG_LIB=C:\Users\user\github\vcpkg\installed\x64-windows-static\lib

cl.exe /W3 /WX- /std:c17 /MT /O2 /D_CRT_SECURE_NO_WARNINGS /DPCRE2_STATIC /Isrc /I%VCPKG_INC% ^
    src\ccze.c src\color.c src\rules.c src\tool.c ^
    /Fe:ccze.exe ^
    /link %VCPKG_LIB%\pcre2-8.lib

if %errorlevel%==0 (
    echo Build successful: ccze.exe
) else (
    echo Build FAILED
    exit /b 1
)
