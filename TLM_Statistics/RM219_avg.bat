@echo off
cls

echo Running several TLM algorithms

cd debug

TLM_Statistics.exe ../InputFiles/RM219_avg/0.txt
TLM_Statistics.exe ../InputFiles/RM219_avg/1.txt
TLM_Statistics.exe ../InputFiles/RM219_avg/2.txt
TLM_Statistics.exe ../InputFiles/RM219_avg/3.txt
TLM_Statistics.exe ../InputFiles/RM219_avg/4.txt

pause > nul

echo on

exit