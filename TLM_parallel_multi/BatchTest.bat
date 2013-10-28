@echo off
cls

echo Running several TLM algorithms

cd debug

TLM_parallel_multi.exe ../InputFiles/FreeSpace/8.txt

pause > nul

echo on

exit