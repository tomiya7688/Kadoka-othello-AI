@echo off
setlocal
where python >nul 2>nul || (echo [ERROR] python was not found in PATH. & exit /b 1)
where gh >nul 2>nul || (echo [ERROR] GitHub CLI ^(gh^) was not found in PATH. & exit /b 1)
python tools\next_issue.py
exit /b %errorlevel%
