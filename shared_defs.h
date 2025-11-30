#ifndef SHARED_DEFS_H
#define SHARED_DEFS_H

#include <sys/types.h>

// Constants
#define NUM_EXERCISES 5
#define MAX_RUBRIC_LINE_LEN 256
#define MAX_EXAM_FILES 21 // 20 regular exams + 1 (9999) termination file
#define SHM_KEY 12345 // Unique key for shared memory segment

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

    // Note: Semaphores would be added here or created separately for Part 2.b
} SharedData;

#endif // SHARED_DEFS_H