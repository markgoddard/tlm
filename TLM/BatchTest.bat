@echo off
cls

echo Running several TLM algorithms

cd debug

TLM.exe ../InputFiles/RM219_avg.txt
pause > nul

echo on

exit