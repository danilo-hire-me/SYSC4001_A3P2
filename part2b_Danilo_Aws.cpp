#include "shared_defs.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

using namespace std;

// Union for semctl
union semun {
    int              val;
    struct semid_ds *buf;
    unsigned short  *array;
    struct seminfo  *__buf;
};

// --- Semaphore Utility Functions ---

// P (Wait/Decrement) operation
void sem_wait(int semid, int semnum) {
    struct sembuf sb;
    sb.sem_num = semnum;
    sb.sem_op = -1;
    sb.sem_flg = 0;
    if (semop(semid, &sb, 1) == -1) {
        perror("semop P failed");
        exit(EXIT_FAILURE);
    }
}

// V (Signal/Increment) operation
void sem_signal(int semid, int semnum) {
    struct sembuf sb;
    sb.sem_num = semnum;
    sb.sem_op = 1;
    sb.sem_flg = 0;
    if (semop(semid, &sb, 1) == -1) {
        perror("semop V failed");
        exit(EXIT_FAILURE);
    }
}

int read_student_id(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) return -1;
    string line;
    if (getline(file, line)) {
        return stoi(line);
    }
    return -1;
}

// FILE/SHM I/O FUNCTIONS

void load_rubric_to_shm(SharedData* shm_ptr) {
    ifstream file("rubric.txt");
    if (!file.is_open()) {
        cerr << "Error: Could not open rubric.txt" << endl;
        exit(1);
    }

    string line;
    for (int i = 0; i < NUM_EXERCISES && getline(file, line); ++i) {
        stringstream ss(line);
        string segment;
        
        getline(ss, segment, ','); 
        shm_ptr->current_rubric[i].exercise_num = stoi(segment);
        
        getline(ss, segment); 
        if (segment.size() > 1 && segment[0] == ' ') {
            shm_ptr->current_rubric[i].rubric_text = segment[1];
        } else if (segment.size() > 0) {
            shm_ptr->current_rubric[i].rubric_text = segment[0];
        }
    }
}

void save_rubric_to_file(SharedData* shm_ptr, int ta_id) {
    sem_wait(shm_ptr->sem_id, RUBRIC_WRITE_SEM); 

    // --- CRITICAL SECTION START: Rubric File Write (Mutex protected) ---
    ofstream file("rubric.txt");
    if (!file.is_open()) {
        cerr << "TA " << ta_id << " ERROR: Could not open rubric.txt for writing." << endl;
        sem_signal(shm_ptr->sem_id, RUBRIC_WRITE_SEM);
        return;
    }
    
    cout << "TA " << ta_id << " -> ACCESSING the rubric file (WRITE - SAFE)." << endl;
    for (int i = 0; i < NUM_EXERCISES; ++i) {
        file << shm_ptr->current_rubric[i].exercise_num << "," 
             << shm_ptr->current_rubric[i].rubric_text << endl;
    }
    cout << "TA " << ta_id << " <- FINISHED writing to the rubric file." << endl;
    // --- CRITICAL SECTION END ---

    sem_signal(shm_ptr->sem_id, RUBRIC_WRITE_SEM);
}

int load_next_exam_to_shm(SharedData* shm_ptr, int ta_id) {
    int index = shm_ptr->next_exam_index;
    if (index >= MAX_EXAM_FILES) return 0; 

    char student_id_str[5];
    int student_id_int;

    if (index == MAX_EXAM_FILES - 1) { 
        student_id_int = 9999;
        snprintf(student_id_str, sizeof(student_id_str), "9999");
    } else {
        student_id_int = index + 1;
        snprintf(student_id_str, sizeof(student_id_str), "%04d", student_id_int);
    }
    
    string filename = "exams/" + string(student_id_str) + ".txt";
    int current_student_id = read_student_id(filename);

    if (current_student_id == -1) {
        cerr << "TA " << ta_id << " ERROR: Could not read student ID from " << filename << endl;
        return 0;
    }

    // --- CRITICAL SECTION START: Next Exam Load (Mutex protected) ---
    cout << "TA " << ta_id << " -> LOADING next exam (ID: " << current_student_id << ") into shared memory (SAFE)." << endl;
    
    shm_ptr->current_exam.student_id = current_student_id;
    for(int i = 0; i < NUM_EXERCISES; ++i) {
        shm_ptr->current_exam.marked_by_ta[i] = 0; 
    }
    shm_ptr->next_exam_index++; 

    if (current_student_id == 9999) {
        cout << "TA " << ta_id << " DETECTED TERMINATION EXAM (9999). Setting termination flag." << endl;
        shm_ptr->is_terminated = 1;
    }

    cout << "TA " << ta_id << " <- FINISHED loading exam ID: " << current_student_id << endl;
    // --- CRITICAL SECTION END ---

    return 1;
}

// TA PROCESS LOGIC

void ta_process(int ta_id, SharedData* shm_ptr) {
    srand(time(NULL) ^ getpid()); 

    cout << "TA " << ta_id << " (PID: " << getpid() << ") started." << endl;

    while (shm_ptr->is_terminated == 0) {
        
        // --- 1. Check Rubric and Potentially Correct ---
        for (int i = 0; i < NUM_EXERCISES; ++i) {
            
            int delay_ms = 500 + (rand() % 501); 
            usleep(delay_ms * 1000); 

            if (rand() % 5 == 0) {
                // Correction needed
                char old_char = shm_ptr->current_rubric[i].rubric_text;
                char new_char = old_char + 1; 
                shm_ptr->current_rubric[i].rubric_text = new_char;

                cout << "TA " << ta_id << " -> Rubric Correction: Exercise " 
                     << shm_ptr->current_rubric[i].exercise_num 
                     << " changed from '" << old_char << "' to '" << new_char << "'." << endl;

                save_rubric_to_file(shm_ptr, ta_id); 
            } else {
                cout << "TA " << ta_id << " -> Accessing the rubric (READ): Exercise "
                     << shm_ptr->current_rubric[i].exercise_num << " (OK)." << endl;
            }
        }
        
        // --- 2. Mark Exam Questions ---
        while (shm_ptr->is_terminated == 0) { // Keep the outer loop for the TA's life cycle

    // --- RECALCULATE marked_count inside the loop to reflect other TAs' work ---
    int marked_count = 0;
    for(int i = 0; i < NUM_EXERCISES; ++i) {
        if(shm_ptr->current_exam.marked_by_ta[i] == 1) {
            marked_count++;
        }
    }
    int questions_remaining = NUM_EXERCISES - marked_count;

    if (questions_remaining == 0) {
        // Exam is finished by other TAs or this TA, break out to Step 3 (Load Next Exam)
        break; 
    }
    
    // --- Attempt to mark one question  ---
    int q_idx = rand() % NUM_EXERCISES; 
    int sem_num = Q1_MARK_SEM + q_idx; 

    sem_wait(shm_ptr->sem_id, sem_num);

    // --- CRITICAL SECTION START: Question Marking ---
    if (shm_ptr->current_exam.marked_by_ta[q_idx] == 0) {
        
        shm_ptr->current_exam.marked_by_ta[q_idx] = 1;

        int delay_ms = 1000 + (rand() % 1001); 
        cout << "TA " << ta_id << " -> Marking question " << (q_idx + 1)
             << " for student " << shm_ptr->current_exam.student_id
             << "... (Delay: " << delay_ms << "ms)" << endl;
        usleep(delay_ms * 1000); 
        
        cout << "TA " << ta_id << " <- Finished marking question " << (q_idx + 1) 
             << " for student " << shm_ptr->current_exam.student_id << "." << endl;
    } else {
         cout << "TA " << ta_id << " -> Question " << (q_idx + 1) << " already marked/claimed. Trying another." << endl;
    }
            // --- CRITICAL SECTION END ---

            sem_signal(shm_ptr->sem_id, sem_num);
        }

        // --- 3. Load Next Exam ---
        if (shm_ptr->is_terminated == 0) {
            
            sem_wait(shm_ptr->sem_id, EXAM_LOAD_SEM);
            
            // Check if the exam is fully marked to decide if THIS TA should load the next one
            int fully_marked = 1;
            for(int i = 0; i < NUM_EXERCISES; ++i) {
                if(shm_ptr->current_exam.marked_by_ta[i] == 0) {
                    fully_marked = 0;
                    break;
                }
            }
            
            if (fully_marked) {
                load_next_exam_to_shm(shm_ptr, ta_id);
            }
            
            sem_signal(shm_ptr->sem_id, EXAM_LOAD_SEM);
        }
    }

    cout << "TA " << ta_id << " (PID: " << getpid() << ") is terminating." << endl;
}

// MAIN PROGRAM - PARENT PROCESS

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <number_of_TAs (n>=2)>" << endl;
        return 1;
    }

    int n_tas = stoi(argv[1]);
    if (n_tas < 2) {
        cerr << "Error: Number of TAs must be >= 2." << endl;
        return 1;
    }
    vector<pid_t> ta_pids;

    int shmid = -1;
    SharedData* shm_ptr = (SharedData*)-1;
    int semid = -1;

    // --- 1. Setup Shared Memory ---
    shmid = shmget(SHM_KEY, sizeof(SharedData), IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget failed"); goto cleanup;
    }

    shm_ptr = (SharedData*)shmat(shmid, NULL, 0);
    if (shm_ptr == (SharedData*)-1) {
        perror("shmat failed"); goto cleanup;
    }

    // --- 2. Setup and Initialize Semaphores ---
    semid = semget(SEM_KEY, NUM_SEMAPHORES, IPC_CREAT | 0666);
    if (semid == -1) {
        perror("semget failed"); goto cleanup;
    }
    shm_ptr->sem_id = semid; 

    union semun arg;
    unsigned short initial_values[NUM_SEMAPHORES];

    // Initialize all semaphores to 1 (mutex status)
    for (int i = 0; i < NUM_SEMAPHORES; ++i) {
        initial_values[i] = 1; 
    }
    arg.array = initial_values;

    if (semctl(semid, 0, SETALL, arg) == -1) {
        perror("semctl SETALL failed"); goto cleanup;
    }
    cout << "Semaphores created and initialized to 1." << endl;

    // --- 3. Initialize Shared Data ---
    cout << "Initializing shared memory..." << endl;
    shm_ptr->next_exam_index = 1; 
    shm_ptr->is_terminated = 0;
    shm_ptr->ta_count = n_tas;
    load_rubric_to_shm(shm_ptr);
    
    // Load the first exam 
    if (!load_next_exam_to_shm(shm_ptr, 0)) {
        goto cleanup;
    }
    cout << "Initial exam ID loaded: " << shm_ptr->current_exam.student_id << endl;

    // --- 4. Fork TA Processes ---
    
    for (int i = 1; i <= n_tas; ++i) {
        pid_t pid = fork();
        
        if (pid < 0) {
            perror("fork failed"); break;
        } else if (pid == 0) {
            // Child (TA) Process
            ta_process(i, shm_ptr);
            shmdt(shm_ptr);
            exit(0); 
        } else {
            // Parent Process
            ta_pids.push_back(pid);
        }
    }

    // --- 5. Parent Waits for Children ---
    for (pid_t pid : ta_pids) {
        int status;
        waitpid(pid, &status, 0);
        cout << "Parent: TA (PID: " << pid << ") finished." << endl;
    }
    
    // --- 6. Cleanup Shared Resources ---
cleanup:
    cout << "All TAs finished. Cleaning up shared resources..." << endl;
    
    if (shm_ptr != (SharedData*)-1) {
        shmdt(shm_ptr);
    }
    if (shmid != -1) {
        if (shmctl(shmid, IPC_RMID, NULL) < 0) {
            perror("shmctl IPC_RMID failed");
        }
    }
    
    if (semid != -1) {
        if (semctl(semid, 0, IPC_RMID, arg) < 0) {
            perror("semctl IPC_RMID failed");
        }
    }

    cout << "Simulation finished." << endl;
    return 0;
}
