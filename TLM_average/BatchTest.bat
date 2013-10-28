@echo off
cls

echo Running several TLM algorithms

cd debug

TLM_average.exe ../InputFiles/RM219_avg/1.txt
TLM_average.exe ../InputFiles/RM219_avg/2.txt
TLM_average.exe ../InputFiles/RM219_avg/3.txt






pause > nul

echo on

exit