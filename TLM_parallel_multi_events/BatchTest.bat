@echo off
cls

echo Running several TLM algorithms

cd debug

TLM_parallel_multi_events.exe ../InputFiles/RM219_threads/1.txt
TLM_parallel_multi_events.exe ../InputFiles/RM219_threads/2.txt
TLM_parallel_multi_events.exe ../InputFiles/RM219_threads/3.txt
TLM_parallel_multi_events.exe ../InputFiles/RM219_threads/4.txt
TLM_parallel_multi_events.exe ../InputFiles/RM219_threads/5.txt
TLM_parallel_multi_events.exe ../InputFiles/RM219_threads/6.txt
TLM_parallel_multi_events.exe ../InputFiles/RM219_threads/7.txt
TLM_parallel_multi_events.exe ../InputFiles/RM219_threads/8.txt

TLM_parallel_multi_events.exe ../InputFiles/LargeOffice_threads/1.txt
TLM_parallel_multi_events.exe ../InputFiles/LargeOffice_threads/2.txt
TLM_parallel_multi_events.exe ../InputFiles/LargeOffice_threads/3.txt
TLM_parallel_multi_events.exe ../InputFiles/LargeOffice_threads/4.txt
TLM_parallel_multi_events.exe ../InputFiles/LargeOffice_threads/5.txt
TLM_parallel_multi_events.exe ../InputFiles/LargeOffice_threads/6.txt
TLM_parallel_multi_events.exe ../InputFiles/LargeOffice_threads/7.txt
TLM_parallel_multi_events.exe ../InputFiles/LargeOffice_threads/8.txt

TLM_parallel_multi_events.exe ../InputFiles/FreeSpace/1.txt
TLM_parallel_multi_events.exe ../InputFiles/FreeSpace/2.txt
TLM_parallel_multi_events.exe ../InputFiles/FreeSpace/3.txt
TLM_parallel_multi_events.exe ../InputFiles/FreeSpace/4.txt
TLM_parallel_multi_events.exe ../InputFiles/FreeSpace/5.txt
TLM_parallel_multi_events.exe ../InputFiles/FreeSpace/6.txt
TLM_parallel_multi_events.exe ../InputFiles/FreeSpace/7.txt
TLM_parallel_multi_events.exe ../InputFiles/FreeSpace/8.txt


pause > nul

echo on

exit