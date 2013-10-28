@echo off
cls

echo Running several TLM algorithms

cd debug

TLM_delay_spread_pulse.exe ../InputFiles/RM219/1.txt
TLM_delay_spread_pulse.exe ../InputFiles/RM219/2.txt
TLM_delay_spread_pulse.exe ../InputFiles/RM219/3.txt
TLM_delay_spread_pulse.exe ../InputFiles/RM219/4.txt
TLM_delay_spread_pulse.exe ../InputFiles/RM219/5.txt
TLM_delay_spread_pulse.exe ../InputFiles/RM219/6.txt
TLM_delay_spread_pulse.exe ../InputFiles/RM219/7.txt
TLM_delay_spread_pulse.exe ../InputFiles/RM219/8.txt


pause > nul

echo on

exit