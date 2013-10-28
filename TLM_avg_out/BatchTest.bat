@echo off
cls

echo Running several TLM algorithms

cd debug

TLM_avg_out.exe ../InputFiles/RM219_avg_out/1.txt
TLM_avg_out.exe ../InputFiles/RM219_avg_out/2.txt
TLM_avg_out.exe ../InputFiles/RM219_avg_out/3.txt



pause > nul

echo on

exit