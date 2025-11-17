@echo off
chcp 65001

echo Extracting %1 to %~dpn1
%~dp0\alice.exe ar extract -o "%~dpn1" %*
if %ERRORLEVEL% NEQ 0 (
	pause
)
