cd TLM
call BatchTest.bat
cd..

call wait.bat 20

cd TLM_parallel_dynamic
call BatchTest.bat
cd..

call wait.bat 20

cd TLM_parallel_events
call BatchTest.bat
cd..

call wait.bat 20

cd TLM_parallel_multi_sets
call BatchTest.bat
cd..