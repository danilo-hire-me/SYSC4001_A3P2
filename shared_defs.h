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
    RUBRIC_WRITE_SEM, 
    EXAM_LOAD_SEM,    
    Q1_MARK_SEM,      
    Q2_MARK_SEM,      
    Q3_MARK_SEM,      
    Q4_MARK_SEM,      
    Q5_MARK_SEM,      
    NUM_SEMAPHORES    
};

// --- Rubric Structure ---
// Represents one line in the rubric file
typedef struct {
    int exercise_num;
    char rubric_text; // The character that can be corrected ('A', 'B', etc.)
} RubricEntry;

// --- Exam Structure ---
typedef struct {
    int student_id; 
    // Flag to track which questions have been marked (0 = unmarked, 1 = marked)
    int marked_by_ta[NUM_EXERCISES]; 
} ExamState;

// --- Master Shared Data Structure ---
// This is the entire block of memory shared between the parent and TA processes
typedef struct {
    // Shared Resources
    RubricEntry current_rubric[NUM_EXERCISES];
    ExamState current_exam;

    // Coordination Variables 
    int next_exam_index;      
    int ta_count;             
    int is_terminated;        
    int sem_id; 
} SharedData;

#endif
