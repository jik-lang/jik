REM test
@echo off
for /R ../src/jik %%f in (*.h *.c) do clang-format -i "%%f"
clang-format -i ../jiklib/core.h
echo Done.
