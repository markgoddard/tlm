@echo off
cls

echo Running several TLM algorithms

cd debug

TLM_avg_out_stats.exe ../InputFiles/RM219_avg_out/1.txt
TLM_avg_out_stats.exe ../InputFiles/RM219_avg_out/2.txt
TLM_avg_out_stats.exe ../InputFiles/RM219_avg_out/3.txt
TLM_avg_out_stats.exe ../InputFiles/RM219_avg_out/4.txt
TLM_avg_out_stats.exe ../InputFiles/RM219_avg_out/5.txt


pause > nul

echo on

exit