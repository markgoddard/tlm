@echo off
cls

echo Running several TLM algorithms

cd debug

TLM_delay_spread.exe ../InputFiles/RM219/1.txt
TLM_delay_spread.exe ../InputFiles/RM219/2.txt
TLM_delay_spread.exe ../InputFiles/RM219/3.txt
TLM_delay_spread.exe ../InputFiles/RM219/4.txt


pause > nul

echo on

exit