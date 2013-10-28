@echo off
cls

echo Running several TLM algorithms

cd debug

TLM_float.exe ../InputFiles/RM219_verification.txt

pause > nul

echo on

exit