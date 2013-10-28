@echo off
cls

echo Running several TLM algorithms

cd debug

TLM.exe ../InputFiles/RM219_avg_out/1.txt
TLM.exe ../InputFiles/RM219_avg_out/2.txt
TLM.exe ../InputFiles/RM219_avg_out/3.txt
TLM.exe ../InputFiles/RM219_avg_out/4.txt
TLM.exe ../InputFiles/RM219_avg_out/5.txt
TLM.exe ../InputFiles/RM219_avg_out/6.txt
TLM.exe ../InputFiles/RM219_avg_out/7.txt
TLM.exe ../InputFiles/RM219_avg_out/8.txt
TLM.exe ../InputFiles/RM219_avg_out/9.txt


pause > nul

echo on

exit