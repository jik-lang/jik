@echo off
node scripts\generate-stdlib-index.js
if errorlevel 1 exit /b %errorlevel%
vsce package
