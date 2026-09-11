@echo off
setlocal
cd /d "%~dp0"
call pull_request.bat
exit /b %ERRORLEVEL%
