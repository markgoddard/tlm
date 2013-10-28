@echo off
cls

echo Running several TLM algorithms

cd debug

TLM_parallel.exe ../InputFiles/RM219_parallel/15.txt
TLM_parallel.exe ../InputFiles/RM219_parallel/16.txt
TLM_parallel.exe ../InputFiles/RM219_parallel/17.txt
TLM_parallel.exe ../InputFiles/RM219_parallel/18.txt
TLM_parallel.exe ../InputFiles/RM219_parallel/19.txt
TLM_parallel.exe ../InputFiles/RM219_parallel/20.txt





pause > nul

echo on

exit