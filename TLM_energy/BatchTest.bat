@echo off
cls

echo Running several TLM algorithms

cd debug

TLM_energy.exe ../InputFiles/RM219_1e-4/1.txt
TLM_energy.exe ../InputFiles/RM219_1e-4/2.txt
TLM_energy.exe ../InputFiles/RM219_1e-4/3.txt
TLM_energy.exe ../InputFiles/RM219_1e-4/4.txt
TLM_energy.exe ../InputFiles/RM219_1e-4/5.txt
TLM_energy.exe ../InputFiles/RM219_1e-4/6.txt
TLM_energy.exe ../InputFiles/RM219_1e-4/7.txt

TLM_energy.exe ../InputFiles/LargeOffice_140/1.txt
TLM_energy.exe ../InputFiles/LargeOffice_140/2.txt
TLM_energy.exe ../InputFiles/LargeOffice_140/3.txt
TLM_energy.exe ../InputFiles/LargeOffice_140/4.txt
TLM_energy.exe ../InputFiles/LargeOffice_140/5.txt
TLM_energy.exe ../InputFiles/LargeOffice_140/6.txt
TLM_energy.exe ../InputFiles/LargeOffice_140/7.txt

TLM_energy.exe ../InputFiles/RM219_1e-5/1.txt
TLM_energy.exe ../InputFiles/RM219_1e-5/2.txt
TLM_energy.exe ../InputFiles/RM219_1e-5/3.txt
TLM_energy.exe ../InputFiles/RM219_1e-5/4.txt
TLM_energy.exe ../InputFiles/RM219_1e-5/5.txt
TLM_energy.exe ../InputFiles/RM219_1e-5/6.txt
TLM_energy.exe ../InputFiles/RM219_1e-5/7.txt

TLM_energy.exe ../InputFiles/LargeOffice_160/1.txt
TLM_energy.exe ../InputFiles/LargeOffice_160/2.txt
TLM_energy.exe ../InputFiles/LargeOffice_160/3.txt
TLM_energy.exe ../InputFiles/LargeOffice_160/4.txt
TLM_energy.exe ../InputFiles/LargeOffice_160/5.txt
TLM_energy.exe ../InputFiles/LargeOffice_160/6.txt
TLM_energy.exe ../InputFiles/LargeOffice_160/7.txt




pause > nul

echo on

exit