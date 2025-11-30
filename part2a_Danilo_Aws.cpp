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

using namespace std;

// Function Prototypes
void load_rubric_to_shm(SharedData* shm_ptr);
void save_rubric_to_file(SharedData* shm_ptr);
int load_next_exam_to_shm(SharedData* shm_ptr, int ta_id);
void ta_process(int ta_id, SharedData* shm_ptr);

// Helper function to read the student ID from a file
int read_student_id(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) return -1;
    string line;
    if (getline(file, line)) {
        return stoi(line);
    }
    return -1;
}

// ------------------------------------------------------------------
// FILE/SHM I/O FUNCTIONS
// ------------------------------------------------------------------

// Loads the initial rubric from file into shared memory
void load_rubric_to_shm(SharedData* shm_ptr) {
    ifstream file("rubric.txt");
    if (!file.is_open()) {
        cerr << "Error: Could not open rubric.txt" << endl;
        exit(1);
    }

    string line;
    for (int i = 0; i < NUM_EXERCISES && getline(file, line); ++i) {
        // Line format: "X, Y" where X is exercise number, Y is rubric text char
        stringstream ss(line);
        string segment;
        
        getline(ss, segment, ','); // Read exercise number
        shm_ptr->current_rubric[i].exercise_num = stoi(segment);
        
        getline(ss, segment); // Read the text (e.g., " A")
        // Trim leading space and get the first character
        if (segment.size() > 1 && segment[0] == ' ') {
            shm_ptr->current_rubric[i].rubric_text = segment[1];
        } else if (segment.size() > 0) {
            shm_ptr->current_rubric[i].rubric_text = segment[0];
        }
    }
}

// Writes the current state of the rubric from shared memory back to the file
void save_rubric_to_file(SharedData* shm_ptr) {
    ofstream file("rubric.txt");
    if (!file.is_open()) {
        cerr << "TA " << getpid() << " ERROR: Could not open rubric.txt for writing." << endl;
        return;
    }
    
    // !!! CRITICAL SECTION - UNSYNCHRONIZED WRITE (RACE CONDITION RISK) !!!
    cout << "TA " << getpid() << " -> ACCESSING the rubric file (WRITE - UNSAFE)!" << endl;
    for (int i = 0; i < NUM_EXERCISES; ++i) {
        file << shm_ptr->current_rubric[i].exercise_num << "," 
             << shm_ptr->current_rubric[i].rubric_text << endl;
    }
    cout << "TA " << getpid() << " <- FINISHED writing to the rubric file." << endl;
    // !!! END CRITICAL SECTION !!!
}

// Loads the next exam file into shared memory
int load_next_exam_to_shm(SharedData* shm_ptr, int ta_id) {
    int index = shm_ptr->next_exam_index;
    if (index >= MAX_EXAM_FILES) return 0; // No more files

    char student_id_str[5];
    int student_id_int;

    if (index == MAX_EXAM_FILES - 1) { // The 9999 file is the last one
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

    // !!! CRITICAL SECTION - UNSYNCHRONIZED WRITE (RACE CONDITION RISK) !!!
    cout << "TA " << ta_id << " -> LOADING next exam (ID: " << current_student_id << ") into shared memory (UNSAFE)!" << endl;
    
    // Update shared memory state
    shm_ptr->current_exam.student_id = current_student_id;
    for(int i = 0; i < NUM_EXERCISES; ++i) {
        shm_ptr->current_exam.marked_by_ta[i] = 0; // Reset marked status
    }
    shm_ptr->next_exam_index++; // Advance to next file index

    // Check for termination
    if (current_student_id == 9999) {
        cout << "TA " << ta_id << " DETECTED TERMINATION EXAM (9999). Setting termination flag." << endl;
        shm_ptr->is_terminated = 1;
    }

    cout << "TA " << ta_id << " <- FINISHED loading exam ID: " << current_student_id << endl;
    // !!! END CRITICAL SECTION !!!

    return 1;
}

// ------------------------------------------------------------------
// TA PROCESS LOGIC
// ------------------------------------------------------------------

void ta_process(int ta_id, SharedData* shm_ptr) {
    srand(time(NULL) ^ getpid()); // Seed random number generator

    cout << "TA " << ta_id << " (PID: " << getpid() << ") started." << endl;

    while (shm_ptr->is_terminated == 0) {
        
        // --- 1. Check Rubric and Potentially Correct ---
        for (int i = 0; i < NUM_EXERCISES; ++i) {
            
            // Random Delay (0.5 - 1.0 seconds)
            int delay_ms = 500 + (rand() % 501); 
            usleep(delay_ms * 1000); 

            // Check if correction is needed (e.g., 20% chance)
            if (rand() % 5 == 0) {
                // Correction needed
                
                // !!! CRITICAL SECTION: RUBRIC MODIFICATION/WRITE !!!
                char old_char = shm_ptr->current_rubric[i].rubric_text;
                char new_char = old_char + 1; // Increment ASCII code
                shm_ptr->current_rubric[i].rubric_text = new_char;

                cout << "TA " << ta_id << " -> Rubric Correction: Exercise " 
                     << shm_ptr->current_rubric[i].exercise_num 
                     << " changed from '" << old_char << "' to '" << new_char << "'." << endl;

                // Save change back to file (UNSYNCHRONIZED WRITE!)
                save_rubric_to_file(shm_ptr);
                // !!! END CRITICAL SECTION !!!
            } else {
                cout << "TA " << ta_id << " -> Accessing the rubric (READ): Exercise "
                     << shm_ptr->current_rubric[i].exercise_num << " (OK)." << endl;
            }
        }
        
        // --- 2. Mark Exam Questions ---
        int questions_marked = 0;
        for(int i = 0; i < NUM_EXERCISES; ++i) {
            if(shm_ptr->current_exam.marked_by_ta[i] == 1) {
                questions_marked++;
            }
        }
        
        // Loop until all questions are marked (or termination detected)
        while (questions_marked < NUM_EXERCISES && shm_ptr->is_terminated == 0) {
            
            // Randomly select an unmarked question index (0 to 4)
            int q_idx = rand() % NUM_EXERCISES; 

            // !!! CRITICAL SECTION: MARKING A QUESTION !!!
            // Check if the question is unmarked and attempt to mark it
            if (shm_ptr->current_exam.marked_by_ta[q_idx] == 0) {
                
                // Set the flag in shared memory (Race condition possible here!)
                shm_ptr->current_exam.marked_by_ta[q_idx] = 1;
                
                // Random Delay (1.0 - 2.0 seconds) for marking
                int delay_ms = 1000 + (rand() % 1001); 
                cout << "TA " << ta_id << " -> Marking question " << (q_idx + 1)
                     << " for student " << shm_ptr->current_exam.student_id
                     << "... (Delay: " << delay_ms << "ms)" << endl;
                usleep(delay_ms * 1000); 
                
                cout << "TA " << ta_id << " <- Finished marking question " << (q_idx + 1) 
                     << " for student " << shm_ptr->current_exam.student_id << "." << endl;
                
                questions_marked++;
            } else {
                 // Question was already marked (either by this TA or another)
                 // This is where a TA would try to mark another question if the current one is taken
            }
            // !!! END CRITICAL SECTION !!!
        }

        // --- 3. Load Next Exam ---
        if (shm_ptr->is_terminated == 0 && questions_marked == NUM_EXERCISES) {
            
            // !!! CRITICAL SECTION: LOADING NEXT EXAM !!!
            int success = load_next_exam_to_shm(shm_ptr, ta_id);
            if (success == 0) {
                // Should not happen if logic is correct, but handles max files limit
                shm_ptr->is_terminated = 1;
            }
            // !!! END CRITICAL SECTION !!!
        }
    }

    cout << "TA " << ta_id << " (PID: " << getpid() << ") is terminating." << endl;
}

// ------------------------------------------------------------------
// MAIN PROGRAM - PARENT PROCESS
// ------------------------------------------------------------------

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

    // --- 1. Setup Shared Memory ---
    key_t key = SHM_KEY;
    int shmid = shmget(key, sizeof(SharedData), IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget failed");
        return 1;
    }

    SharedData* shm_ptr = (SharedData*)shmat(shmid, NULL, 0);
    if (shm_ptr == (SharedData*)-1) {
        perror("shmat failed");
        return 1;
    }

    // --- 2. Initialize Shared Data ---
    cout << "Initializing shared memory..." << endl;
    shm_ptr->next_exam_index = 1; // Start with file 1 (0001.txt)
    shm_ptr->is_terminated = 0;
    shm_ptr->ta_count = n_tas;
    load_rubric_to_shm(shm_ptr);
    load_next_exam_to_shm(shm_ptr, 0); // Load the first exam (TA ID 0 is the parent/initializer)
    cout << "Initial exam ID loaded: " << shm_ptr->current_exam.student_id << endl;


    // --- 3. Fork TA Processes ---
    vector<pid_t> ta_pids;
    for (int i = 1; i <= n_tas; ++i) {
        pid_t pid = fork();
        
        if (pid < 0) {
            perror("fork failed");
            // Cleanup and exit parent
            break; 
        } else if (pid == 0) {
            // Child (TA) Process
            ta_process(i, shm_ptr);
            // Detach shared memory and exit
            shmdt(shm_ptr);
            exit(0); 
        } else {
            // Parent Process
            ta_pids.push_back(pid);
        }
    }

    // --- 4. Parent Waits for Children ---
    for (pid_t pid : ta_pids) {
        int status;
        waitpid(pid, &status, 0);
        cout << "Parent: TA (PID: " << pid << ") finished." << endl;
    }
    
    // --- 5. Cleanup Shared Memory ---
    cout << "All TAs finished. Cleaning up shared memory..." << endl;
    shmdt(shm_ptr);
    if (shmctl(shmid, IPC_RMID, NULL) < 0) {
        perror("shmctl IPC_RMID failed");
        return 1;
    }

    cout << "Simulation finished." << endl;
    return 0;
}