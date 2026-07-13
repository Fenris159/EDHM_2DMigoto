@echo off
REM Optional local deploy hook. CI and Zip Release builds do not require this file
REM to do anything; Release|Debug post-build events call it if present.
REM Create CopyToGamesLocal.bat (gitignored) for machine-specific copy commands.
if exist "%~dp0CopyToGamesLocal.bat" (
  call "%~dp0CopyToGamesLocal.bat" %*
)
exit /b 0
