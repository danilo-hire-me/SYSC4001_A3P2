#ifndef SHARED_DEFS_H
#define SHARED_DEFS_H

#include <sys/types.h>

// Constants
#define NUM_EXERCISES 5
#define SEM_KEY 54321 // Semaphor key
#define MAX_EXAM_FILES 21 // 20 regular exams + 1 (9999) termination file
#define SHM_KEY 12345 // Unique key for shared memory segment


 // semaphore indices

enum{
    RUBRIC_WRITE_SEM, // Index 0: Mutex to protect writing to the rubric file
    EXAM_LOAD_SEM,    // Index 1: Mutex to protect loading the next exam into shared memory
    Q1_MARK_SEM,      // Index 2: Mutex for marking Question 1
    Q2_MARK_SEM,      // Index 3: Mutex for marking Question 2
    Q3_MARK_SEM,      // Index 4: Mutex for marking Question 3
    Q4_MARK_SEM,      // Index 5: Mutex for marking Question 4
    Q5_MARK_SEM,      // Index 6: Mutex for marking Question 5
    NUM_SEMAPHORES    // Total count: 7
};

// --- Rubric Structure ---
// Represents one line in the rubric file (e.g., "1, A")
typedef struct {
    int exercise_num;
    char rubric_text; // The character that can be corrected ('A', 'B', etc.)
} RubricEntry;

// --- Exam Structure ---
typedef struct {
    int student_id; // e.g., 0001
    // Flag to track which questions have been marked (0 = unmarked, 1 = marked)
    int marked_by_ta[NUM_EXERCISES]; 
} ExamState;

// --- Master Shared Data Structure ---
// This is the entire block of memory shared between the parent and TA processes
typedef struct {
    // Shared Resources
    RubricEntry current_rubric[NUM_EXERCISES];
    ExamState current_exam;

    // Coordination Variables (Needed for Part 2.a/b)
    int next_exam_index;      // Index/ID of the next exam file to load (e.g., 1 to 21)
    int ta_count;             // Total number of TAs (processes) created
    int is_terminated;        // Flag to signal all TAs to stop (e.g., when 9999 is reached)
    int sem_id; 
} SharedData;

#endif // SHARED_DEFS_H
