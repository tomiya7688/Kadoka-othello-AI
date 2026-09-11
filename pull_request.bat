@echo off
setlocal
where python >nul 2>nul || (echo [ERROR] python was not found in PATH. & exit /b 1)
where git >nul 2>nul || (echo [ERROR] git was not found in PATH. & exit /b 1)
where gh >nul 2>nul || (echo [ERROR] GitHub CLI ^(gh^) was not found in PATH. & exit /b 1)
python tools\pull_request.py
exit /b %errorlevel%
