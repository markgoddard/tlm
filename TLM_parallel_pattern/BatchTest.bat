@echo off
cls

echo Running several TLM algorithms

cd debug

TLM_parallel_pattern.exe ../InputFiles/RM219_threads/6.txt
TLM_parallel_pattern.exe ../InputFiles/RM219_threads/7.txt
TLM_parallel_pattern.exe ../InputFiles/RM219_threads/8.txt

pause > nul

echo on

exit