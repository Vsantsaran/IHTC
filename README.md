IHTP: Integrated Healthcare Timetabling Problem

There are two main files, viz. nga.c and oanga.c.

To reproduce the results, just setup your folders in the same way as done in the bash scripts (run_parallel ones).

You also have to run the cJSON.c file together with the GA file, i.e. either nga.c or oanga.c.

Then run the validating scripts to validate all the solution files in parallel. 

Or, if you want to test only one instance then you may also do it manually, e.g. "./IHTP_Validator <source_file> <solution_file>".

Ensure that you have already compiled the IHTP_Validator.cc file and have its executable file.
