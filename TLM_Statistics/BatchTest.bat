@echo off
cls

echo Running several TLM algorithms

cd debug

TLM_statistics.exe ../InputFiles/RM219_stats/2.txt
TLM_statistics.exe ../InputFiles/RM219_stats/3.txt
TLM_statistics.exe ../InputFiles/RM219_stats/4.txt
TLM_statistics.exe ../InputFiles/RM219_stats/5.txt

pause > nul

echo on

exit