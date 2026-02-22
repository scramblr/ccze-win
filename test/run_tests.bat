@echo off
setlocal

set CCZE=%~dp0..\ccze.exe
set PASS=0
set FAIL=0

echo Running ccze regression tests...
echo.

REM Test 1: plain passthrough preserves content
%CCZE% --no-color "%~dp0java.log" > "%TEMP%\ccze_actual.txt" 2>&1
findstr /c:"INFO" "%TEMP%\ccze_actual.txt" >nul 2>&1
if %errorlevel%==0 (
    echo [PASS] java.log plain output contains expected text
    set /a PASS+=1
) else (
    echo [FAIL] java.log plain output missing expected text
    type "%TEMP%\ccze_actual.txt"
    set /a FAIL+=1
)

REM Test 2: output contains all log lines
findstr /c:"ERROR" "%TEMP%\ccze_actual.txt" >nul 2>&1
if %errorlevel%==0 (
    echo [PASS] output contains ERROR line
    set /a PASS+=1
) else (
    echo [FAIL] output missing ERROR line
    set /a FAIL+=1
)

REM Test 3: output contains WARNING line
findstr /c:"WARNING" "%TEMP%\ccze_actual.txt" >nul 2>&1
if %errorlevel%==0 (
    echo [PASS] output contains WARNING line
    set /a PASS+=1
) else (
    echo [FAIL] output missing WARNING line
    set /a FAIL+=1
)

REM Test 4: missing file exits with code 1
%CCZE% nonexistent_file.log >nul 2>&1
if %errorlevel%==1 (
    echo [PASS] missing file returns exit code 1
    set /a PASS+=1
) else (
    echo [FAIL] missing file should return exit code 1, got %errorlevel%
    set /a FAIL+=1
)

REM Test 5: no args reads stdin (just check it doesn't crash with empty input)
echo. | %CCZE% --no-color >nul 2>&1
if %errorlevel%==0 (
    echo [PASS] stdin mode runs without crash
    set /a PASS+=1
) else (
    echo [FAIL] stdin mode crashed with exit code %errorlevel%
    set /a FAIL+=1
)

echo.
echo Results: %PASS% passed, %FAIL% failed
if %FAIL% GTR 0 exit /b 1
exit /b 0
