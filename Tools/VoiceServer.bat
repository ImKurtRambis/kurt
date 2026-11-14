@echo off
rem Starts the local voices (Tools\VoiceServer.py). The game runs this; you can also run it by hand to test.
rem Visual Studio's C++ compiler, when installed, lets PyTorch compile CSM, which makes suspects answer much faster.
setlocal
rem everything it needs is in AI\: never look for models or updates online
set "HF_HUB_OFFLINE=1"
set "TRANSFORMERS_OFFLINE=1"
set "HF_HUB_DISABLE_TELEMETRY=1"
set "VSW=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
if exist "%VSW%" for /f "usebackq delims=" %%i in (`"%VSW%" -latest -products * -property installationPath`) do set "VS=%%i"
if defined VS if exist "%VS%\VC\Auxiliary\Build\vcvars64.bat" call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0.."
"AI\python\python.exe" -u "Tools\VoiceServer.py" %*
