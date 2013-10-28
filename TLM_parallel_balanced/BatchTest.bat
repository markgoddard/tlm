@echo off
cls

echo Running several TLM algorithms

cd debug

TLM_parallel_balanced.exe ../InputFiles/FreeSpace.txt

pause > nul

echo on

exit