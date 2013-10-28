@echo off
cls

echo Running several TLM algorithms

cd debug

TLM.exe ../InputFiles/Variable_absolute/1.txt
TLM.exe ../InputFiles/Variable_absolute/2.txt
TLM.exe ../InputFiles/Variable_absolute/3.txt
TLM.exe ../InputFiles/Variable_absolute/4.txt
TLM.exe ../InputFiles/Variable_absolute/5.txt
TLM.exe ../InputFiles/Variable_absolute/6.txt
TLM.exe ../InputFiles/Variable_absolute/7.txt

pause > nul

echo on

exit