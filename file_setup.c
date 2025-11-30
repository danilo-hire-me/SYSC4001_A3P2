#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

// Initial Rubric content
#define INITIAL_RUBRIC_CONTENT "1,A\n2,B\n3,C\n4,D\n5,E\n"
// Total exams to create (0001.txt to 9999.txt)
#define NUM_EXAMS 21 

void create_files() {
    // 1. Create the 'exams' directory if it doesn't exist
    if (mkdir("exams", 0777) == -1 && errno != EEXIST) {
        perror("Error creating exams directory");
        exit(EXIT_FAILURE);
    }
    printf("--- Creating Exam Files (0001.txt to 9999.txt) ---\n");

    // 2. Create 20 regular exam files (0001.txt to 0020.txt)
    for (int i = 1; i <= NUM_EXAMS - 1; i++) {
        char filename[32];
        char student_id_str[5];

        // Format student ID (e.g., 0001, 0002, ...)
        snprintf(student_id_str, sizeof(student_id_str), "%04d", i);
        snprintf(filename, sizeof(filename), "exams/%s.txt", student_id_str);

        FILE *f = fopen(filename, "w");
        if (f == NULL) {
            perror("Error opening exam file");
            exit(EXIT_FAILURE);
        }
        // Write student number as the first line
        fprintf(f, "%s\nThis is the content for exam %s\n", student_id_str, student_id_str);
        fclose(f);
    }
    printf("Created %d regular exam files in 'exams/'\n", NUM_EXAMS - 1);

    // 3. Create the termination exam file (9999.txt)
    FILE *f_term = fopen("exams/9999.txt", "w");
    if (f_term == NULL) {
        perror("Error opening 9999.txt");
        exit(EXIT_FAILURE);
    }
    fprintf(f_term, "9999\nThis is the termination exam.\n");
    fclose(f_term);
    printf("Created termination file exams/9999.txt\n");

    // 4. Create the initial rubric file
    FILE *f_rubric = fopen("rubric.txt", "w");
    if (f_rubric == NULL) {
        perror("Error opening rubric.txt");
        exit(EXIT_FAILURE);
    }
    fprintf(f_rubric, INITIAL_RUBRIC_CONTENT);
    fclose(f_rubric);
    printf("Created initial rubric.txt\n\n");
}

int main() {
    create_files();
    return 0;
}