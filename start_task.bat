@echo off
setlocal
cd /d "%~dp0"
call next_issue.bat
exit /b %ERRORLEVEL%
