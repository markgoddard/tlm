@echo off
cls

echo Running several TLM algorithms

cd debug

TLM_energy_pulse.exe ../InputFiles/RM219_1e-5/1.txt
TLM_energy_pulse.exe ../InputFiles/RM219_1e-5/2.txt
TLM_energy_pulse.exe ../InputFiles/RM219_1e-5/3.txt
TLM_energy_pulse.exe ../InputFiles/RM219_1e-5/4.txt
TLM_energy_pulse.exe ../InputFiles/RM219_1e-5/5.txt
TLM_energy_pulse.exe ../InputFiles/RM219_1e-5/6.txt
TLM_energy_pulse.exe ../InputFiles/RM219_1e-5/7.txt





pause > nul

echo on

exit