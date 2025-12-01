#Create input files

```bash
# Compile the setup utility
gcc file_setup.c -o file_setup -std=c99

# Run the setup utility (creates exams/ folder and rubric.txt)
./file_setup

#Compile programs
# Compile Part 2.a (Unsynchronized)
g++ part2a_Danilo_Aws.cpp -o part2a -std=c++11 -lrt

# Compile Part 2.b (Synchronized)
g++ part2b_Danilo_Aws.cpp -o part2b -std=c++11 -lrt

#Execution
#change the a to a b in ./part2a to execute  synchronized version
#./part2a n (n being as many TAs as you want)
./part2a 3 //(for 3 tas)

#Cleanuo in case of crash

ipcs -s -m
ipcrm -m <shmid>
ipcrm -s <semid>
