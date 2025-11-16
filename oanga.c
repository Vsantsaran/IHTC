int POP_SIZE = 8000, N_GEN = 120000, NURSE_SKILL_LEVEL_ALLOWANCE, NURSE_MAX_LOAD_ALLOWANCE, N_GEN_MANDATORY = 10000;
float p_m = 5e-2f, THREASHOLD = 1000, upper_bound = 0.9, lower_bound = 0.2, p_c = 0.1;
// THREASHOLD: the maximum violation and cost of the chromosomes after normalization (inter/extra-polation)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>
#include "cJSON.h"
#define NUM_THREADS 4
#define CHECK_INTERVAL 120 // Pause every 2 minutes (120 seconds)

#ifdef _WIN32
#include <direct.h>  // Windows
#else
#include <unistd.h>  // Linux
#endif

#include <stdbool.h>
//#include <pthread.h>
#include <limits.h>

// Assertions for debugging
#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "Assertion failed: %s\nFile: %s, Line: %d\n", message, __FILE__, __LINE__); \
            exit(EXIT_FAILURE); \
        } \
    } while (false)

// Enum types
typedef enum {
    UNKNOWN = -1,  // Default value for unspecified gender
    A = 0,
    B = 1
} gender;
typedef enum { early, late, night } shift_types;
typedef enum { optional, mandatory } PatientType;

// Structs for nurses, patients, rooms, etc.
typedef struct {
    int day;
    shift_types shift_time;
    int max_load;
    int load_left;
    int* rooms;
    int num_rooms;
} Shifts;

typedef struct {
    int id;
    int skill_level;
    Shifts* shift;
    int num_shifts;
} Nurses;

typedef struct {
    int room_mixed_age;
    int room_nurse_skill;
    int continuity_of_care;
    int nurse_excessive_workload;
    int open_operating_theater;
    int surgeon_transfer;
    int patient_delay;
    int unscheduled_optional;
} Weights;

typedef struct {
    int id;
    int length_of_stay;
    int room_id;
    char* age;
    gender gen;
    int* workload_produced;
    int* skill_level_required;
    int is_admitted;
    int num_nurses_allotted;
    int* nurses_allotted;
} Occupants;

typedef struct {
    int id;
    int mandatory;
    gender gen;
    char* age_group;
    int length_of_stay;
    int surgery_release_day;
    int surgery_due_day;
    int surgery_duration;
    int surgeon_id;
    int* incompatible_room_ids;
    int num_incompatible_rooms;
    int* workload_produced;
    int* skill_level_required;
    int assigned_room_no;
    int assigned_ot;
    int admission_day;
    int is_admitted;
    int* nurses_allotted;
    int num_nurses_allotted;
} Patient;

typedef struct {
    int id;
    int* max_surgery_time;
    int* time_left; // default: max_surgery_time
    int* patients_assigned;
    int num_assigned_patients; // a fixed number - how many patients are assigned to this surgeon
    int current_size; // dynamic size of the patients_assigned array
} Surgeon;

typedef struct {
    int id;
    int* max_ot_time;
    int* time_left; // default: max_ot_time
} OTs;

typedef struct {
    int id;
    int cap;
    int num_patients_allocated;
    int occupants_cap;
    gender* gender_days_info;
    int* num_patients_info;
    int* nurses_allotted;
    int length_of_nurses_allotted;
    gender gen;
} Rooms;

typedef struct {
    int load_sum;
    int max_skill_req;
} Rooms_req;

typedef struct {
    int surgeon_id;
    int* assigned_patients;
    int num_assigned_patients;
    int surgery_duration_sum;
    int isNull;
} Surgeon_data;

// Function prototypes
const char* gender_to_string(gender g);
const char* shift_types_to_string(shift_types shift);
gender string_to_gender(const char* str);
shift_types string_to_shift_types(const char* str);
int str2int(char* a);

int CURR_ITER;
int objectiveFunction(int*, char type);
int excessiveNurseLoad();
int countdistinctnurses_occupants(int o);
int countdistinctnurses(int p);
int coc();
int findSuitableRoomInteGA(int p, int d);
int nurseAllocationInteGA();

// global variables to store the indices of the random numbers generated for mutation
int swap_mutation_r1 = -1, swap_mutation_r2 = -1, insert_mutation_r1 = -1, insert_mutation_r2 = -1;
int inversion_mutation_r1 = -1, inversion_mutation_r2 = -1;

// Global declarations of variables

int days;                      // Total number of days in the schedule
int skill_levels;              // Total number of skill levels
int num_occupants;             // Total number of occupants
int num_patients;              // Total number of patients
int num_surgeons;              // Total number of surgeons
int num_ots;                   // Total number of operating theaters (OTs)
int num_rooms;                 // Total number of rooms
int num_nurses;                // Total number of nurses
int mandatory_count = 0;       // Counter for mandatory patients
int optional_count = 0;        // Counter for optional patients
int current_ot_index = 0;      // Index to track current operating theater
int** room_shift_nurse;
int size = 0;
int* current_size_dm_nurse;
int* room_gender_map;
int** size_of_room_schedule;

const char* gender_to_string(gender g) {
    switch (g) {
    case A: return "Male";
    case B: return "Female";
    default: return "Unknown";
    }
}

const char* shift_types_to_string(shift_types shift) {
    switch (shift) {
    case early: return "early";
    case late: return "late";
    case night: return "night";
    default: return "unknown";
    }
}

// Enum conversion functions
gender string_to_gender(const char* str) {
    if (strcmp(str, "A") == 0) return A;
    if (strcmp(str, "B") == 0) return B;
    return A; // Invalid value
}

shift_types string_to_shift_types(const char* str) {
    if (strcmp(str, "early") == 0) return early;
    if (strcmp(str, "late") == 0) return late;
    if (strcmp(str, "night") == 0) return night;
    return early; // Invalid value
}

int str2int(char* a)
{
    int number = 0;
    char* p = a;
    p++; // skip the first character (like 'p' or 'n')

    while (*p != '\0') {
        number = number * 10 + (*p - '0');
        p++;
    }
    return number;
}

Nurses* nurses;
Nurses*** dm_nurses_availability;  //nurse assignment
int** max_load_updated; //nurse assignment
Weights* weights = NULL; // Global pointer for weights
Occupants* occupants = NULL; // Global pointer for occupants
Patient* patients;
Patient** mandatory_patients = NULL;
Patient** optional_patients = NULL;
char**** room_schedule = NULL;
Surgeon* surgeon = NULL;
OTs* ot = NULL;
Rooms* room = NULL;
Rooms_req** rooms_requirement = NULL;

int** POPULATION, * G_BEST, CHROMOSOME_SIZE_INTE_GA, ** MANDATORY_POPULATION;
int** CROSSOVER_OFFSPRING_STORAGE_PLACE, ** CROSSOVER_PARENT_STORAGE_PLACE;
int* MUTATED_OFFSPRING_STORAGE_PLACE, * MUTATE_PARENT_STORAGE_PLACE, * chromosome;
// OTs** ot_data_arr, *** ot_days_data_arr;

// Function to parse occupants
void parse_occupants(cJSON* occupants_array) {
    num_occupants = cJSON_GetArraySize(occupants_array);
    occupants = (Occupants*)calloc(num_occupants, sizeof(Occupants));
    if (!occupants) {
        printf("Memory allocation failed for occupants.\n");
        exit(1);
    }
    for (int i = 0; i < num_occupants; i++) {
        cJSON* item = cJSON_GetArrayItem(occupants_array, i);
        if (!item) continue;
        // Parse individual fields
        cJSON* id_json = cJSON_GetObjectItem(item, "id");
        cJSON* gender_json = cJSON_GetObjectItem(item, "gender");
        cJSON* age_group_json = cJSON_GetObjectItem(item, "age_group");
        cJSON* length_of_stay_json = cJSON_GetObjectItem(item, "length_of_stay");
        cJSON* workload_produced_json = cJSON_GetObjectItem(item, "workload_produced");
        cJSON* skill_level_required_json = cJSON_GetObjectItem(item, "skill_level_required");
        cJSON* room_id_json = cJSON_GetObjectItem(item, "room_id");
        // Parse ID
        if (id_json && cJSON_IsString(id_json)) {
            int id_int = str2int(id_json->valuestring);
            occupants[i].id = id_int;
        }
        else {
            printf("Error parsing occupant ID for index %d\n", i);
            occupants[i].id = -1; // Set to invalid value
        }
        // Parse Gender
        if (gender_json && cJSON_IsString(gender_json)) occupants[i].gen = string_to_gender(gender_json->valuestring);
        else occupants[i].gen = -1; // Default invalid gender
        // Parse Age Group
        if (age_group_json && cJSON_IsString(age_group_json)) occupants[i].age = strdup(age_group_json->valuestring);
        else occupants[i].age = NULL;
        // Parse Length of Stay
        if (length_of_stay_json && cJSON_IsNumber(length_of_stay_json)) occupants[i].length_of_stay = length_of_stay_json->valueint;
        else occupants[i].length_of_stay = 0; // Default invalid value
        // Parse Room ID
        if (room_id_json && cJSON_IsString(room_id_json)) {
            int room_id_int = str2int(room_id_json->valuestring);
            occupants[i].room_id = room_id_int;
        }
        else {
            occupants[i].room_id = -1; // Default invalid room ID
        }
        // Parse Workload Produced
        if (workload_produced_json && cJSON_IsArray(workload_produced_json)) {
            int size = cJSON_GetArraySize(workload_produced_json);
            occupants[i].workload_produced = (int*)calloc(size, sizeof(int));
            if (!occupants[i].workload_produced) {
                printf("Memory allocation failed for workload_produced of occupant %d\n", i);
                exit(1);
            }
            for (int j = 0; j < size; j++) {
                occupants[i].workload_produced[j] = cJSON_GetArrayItem(workload_produced_json, j)->valueint;
            }
        }
        else {
            occupants[i].workload_produced = NULL;
        }
        // Parse Skill Level Required
        if (skill_level_required_json && cJSON_IsArray(skill_level_required_json)) {
            int size = cJSON_GetArraySize(skill_level_required_json);
            occupants[i].skill_level_required = (int*)calloc(size, sizeof(int));
            if (!occupants[i].skill_level_required) {
                printf("Memory allocation failed for skill_level_required of occupant %d\n", i);
                exit(1);
            }
            for (int j = 0; j < size; j++) {
                occupants[i].skill_level_required[j] = cJSON_GetArrayItem(skill_level_required_json, j)->valueint;
            }
        }
        else {
            occupants[i].skill_level_required = NULL;
        }
        occupants[i].is_admitted = 1;
        occupants[i].num_nurses_allotted = 0;
        occupants[i].nurses_allotted = NULL;
    }
}

void free_occupants() {
    for (int i = 0; i < num_occupants; i++) {
        if (occupants[i].workload_produced) free(occupants[i].workload_produced);
        if (occupants[i].skill_level_required) free(occupants[i].skill_level_required);
        if (occupants[i].nurses_allotted) free(occupants[i].nurses_allotted);
    }
    free(occupants);
    occupants = NULL; // Avoid dangling pointer
}

void assign_occupants_to_rooms(void) {
    // here we'll update the occupants_Cap filed of each room
    // method - parse the occupants array (arr of structs) and find out what's its room id and then increase the
    // occupants_cap filed of that room
    int i, r_id;
    for (i = 0; i < num_occupants; ++i) {
        r_id = occupants[i].room_id;
        room[r_id].occupants_cap++;
        //room[r_id].gen = occupants[i].gen;
    }
}

// Function to parse weights
void parse_weights(cJSON* weights_json) {
    if (!weights_json) {
        printf("No weights data found.\n");
        return;
    }
    weights->room_mixed_age = cJSON_GetObjectItem(weights_json, "room_mixed_age") ? cJSON_GetObjectItem(weights_json, "room_mixed_age")->valueint : 0;
    weights->room_nurse_skill = cJSON_GetObjectItem(weights_json, "room_nurse_skill") ? cJSON_GetObjectItem(weights_json, "room_nurse_skill")->valueint : 0;
    weights->continuity_of_care = cJSON_GetObjectItem(weights_json, "continuity_of_care") ? cJSON_GetObjectItem(weights_json, "continuity_of_care")->valueint : 0;
    weights->nurse_excessive_workload = cJSON_GetObjectItem(weights_json, "nurse_excessive_workload") ? cJSON_GetObjectItem(weights_json, "nurse_excessive_workload")->valueint : 0;
    weights->open_operating_theater = cJSON_GetObjectItem(weights_json, "open_operating_theater") ? cJSON_GetObjectItem(weights_json, "open_operating_theater")->valueint : 0;
    weights->surgeon_transfer = cJSON_GetObjectItem(weights_json, "surgeon_transfer") ? cJSON_GetObjectItem(weights_json, "surgeon_transfer")->valueint : 0;
    weights->patient_delay = cJSON_GetObjectItem(weights_json, "patient_delay") ? cJSON_GetObjectItem(weights_json, "patient_delay")->valueint : 0;
    weights->unscheduled_optional = cJSON_GetObjectItem(weights_json, "unscheduled_optional") ? cJSON_GetObjectItem(weights_json, "unscheduled_optional")->valueint : 0;
}

void parse_patients(cJSON* patients_array) {
    num_patients = cJSON_GetArraySize(patients_array);
	mandatory_count = 0; // Reset mandatory count
	optional_count = 0; // Reset optional count
    // printf("Number of patients: %d\n", num_patients);
    patients = (Patient*)calloc(num_patients, sizeof(Patient));
    if (!patients) {
        printf("Memory allocation failed for patients.\n");
        exit(1);
    }
    // Allocate memory for mandatory and optional patient pointers
    mandatory_patients = (Patient**)calloc(num_patients, sizeof(Patient*));
    optional_patients = (Patient**)calloc(num_patients, sizeof(Patient*));
    if (!mandatory_patients || !optional_patients) {
        printf("Memory allocation failed for mandatory or optional patient arrays.\n");
        exit(1);
    }
    for (int i = 0; i < num_patients; i++) {
        // int prev_id=0;
        cJSON* item = cJSON_GetArrayItem(patients_array, i);
        if (!item) continue;
        patients[i].assigned_room_no = -1;
        patients[i].admission_day = -1;
        patients[i].assigned_ot = -1;
        // Parse individual fields of each patient
        cJSON* id_json = cJSON_GetObjectItem(item, "id");
        if (id_json && cJSON_IsString(id_json)) {
            int id_int = str2int(id_json->valuestring);
            patients[i].id = id_int;
        }
        else {
            printf("Missing or invalid ID for patient at index %d.\n", i);
            patients[i].id = -1;
        }

        cJSON* mandatory_json = cJSON_GetObjectItem(item, "mandatory");
        if (mandatory_json && cJSON_IsBool(mandatory_json)) {
            patients[i].mandatory = cJSON_IsTrue(mandatory_json) ? 1 : 0;
        }
        else {
            patients[i].mandatory = 0;
            printf("Missing or invalid mandatory status for patient.\n");
        }
        // Assign to mandatory or optional array based on the "mandatory" field
        if (patients[i].mandatory) {
            mandatory_patients[mandatory_count++] = patients + i;//.................................................................
        }
        else {
            optional_patients[optional_count++] = patients + i;
        }
        cJSON* gender_json = cJSON_GetObjectItem(item, "gender");
        if (gender_json && cJSON_IsString(gender_json)) {
            patients[i].gen = string_to_gender(gender_json->valuestring);
        }
        else
            patients[i].gen = A;

        cJSON* age_groups_json = cJSON_GetObjectItem(item, "age_group");
        if (age_groups_json && cJSON_IsString(age_groups_json)) {
            patients[i].age_group = strdup(age_groups_json->valuestring); // Allocate and copy the string
            if (!patients[i].age_group) {
                printf("Memory allocation failed for age_group of patient at index %d.\n", i);
                exit(1);
            }
        }
        else {
            patients[i].age_group = strdup("adult"); // Default value
            if (!patients[i].age_group) {
                printf("Memory allocation failed for default age_group of patient at index %d.\n", i);
                exit(1);
            }
            printf("Missing or invalid age group for the patient at index %d.\n", i);
        }
        cJSON* length_of_stay_json = cJSON_GetObjectItem(item, "length_of_stay");
        if (length_of_stay_json && cJSON_IsNumber(length_of_stay_json)) {
            patients[i].length_of_stay = length_of_stay_json->valueint;
        }
        else {
            patients[i].length_of_stay = 0;
            printf("Missing or invalid length_of_stay for patient at index %d.\n", i);
        }
        cJSON* surgery_release_day_json = cJSON_GetObjectItem(item, "surgery_release_day");
        if (surgery_release_day_json && cJSON_IsNumber(surgery_release_day_json)) {
            patients[i].surgery_release_day = surgery_release_day_json->valueint;
        }
        else {
            patients[i].surgery_release_day = 0;
            printf("Missing or invalid surgery_release_day for patient at index %d.\n", i);
        }
        cJSON* surgery_duration_json = cJSON_GetObjectItem(item, "surgery_duration");
        if (surgery_duration_json && cJSON_IsNumber(surgery_duration_json)) {
            patients[i].surgery_duration = surgery_duration_json->valueint;
        }
        else {
            patients[i].surgery_duration = 0;
            printf("Missing or invalid surgery_duration for patient at index %d.\n", i);
        }
        if (patients[i].mandatory) {
            cJSON* surgery_due_day_json = cJSON_GetObjectItem(item, "surgery_due_day");
            if (surgery_due_day_json && cJSON_IsNumber(surgery_due_day_json)) {
                patients[i].surgery_due_day = surgery_due_day_json->valueint;
            }
            else {
                patients[i].surgery_due_day = -1;
                printf("Missing or invalid surgery_due_day for mandatory patient at index %d.\n", i);
            }
        }
        else {
            patients[i].surgery_due_day = -1;
        }
        cJSON* surgeon_id_json = cJSON_GetObjectItem(item, "surgeon_id");
        if (surgeon_id_json && cJSON_IsString(surgeon_id_json)) {
            int surgeon_id_int = str2int(surgeon_id_json->valuestring);
            patients[i].surgeon_id = surgeon_id_int;
            // printf("%d\n" , surgeon_id_int);
        }
        cJSON* incompatible_rooms_json = cJSON_GetObjectItem(item, "incompatible_room_ids");
        if (incompatible_rooms_json && cJSON_IsArray(incompatible_rooms_json)) {
            patients[i].num_incompatible_rooms = cJSON_GetArraySize(incompatible_rooms_json);
            patients[i].incompatible_room_ids = (int*)calloc(patients[i].num_incompatible_rooms, sizeof(int));
            if (!patients[i].incompatible_room_ids) {
                printf("Memory allocation failed for incompatible_room_ids at index %d.\n", i);
                exit(1);
            }
            for (int j = 0; j < patients[i].num_incompatible_rooms; j++) {
                cJSON* room_id_json = cJSON_GetArrayItem(incompatible_rooms_json, j);
                if (cJSON_IsString(room_id_json)) {
                    patients[i].incompatible_room_ids[j] = str2int(room_id_json->valuestring);
                }
                else {
                    printf("Invalid room ID format for patient at index %d, room %d.\n", i, j);
                    patients[i].incompatible_room_ids[j] = -1;
                }
            }
        }
        else {
            patients[i].incompatible_room_ids = NULL;
            patients[i].num_incompatible_rooms = 0;
            printf("Missing or invalid incompatible_room_ids for patient at index %d.\n", i);
        }
        cJSON* workload_json = cJSON_GetObjectItem(item, "workload_produced");
        if (workload_json && cJSON_IsArray(workload_json)) {
            int size = cJSON_GetArraySize(workload_json);
            patients[i].workload_produced = (int*)calloc(size, sizeof(int));
            if (!patients[i].workload_produced) {
                printf("Memory allocation failed for workload_produced at index %d.\n", i);
                exit(1);
            }
            for (int j = 0; j < size; j++) {
                patients[i].workload_produced[j] = cJSON_GetArrayItem(workload_json, j)->valueint;
            }
        }
        else {
            patients[i].workload_produced = NULL;
            printf("Missing or invalid workload_produced for patient at index %d.\n", i);
        }
        cJSON* skills_json = cJSON_GetObjectItem(item, "skill_level_required");
        if (skills_json && cJSON_IsArray(skills_json)) {
            int size = cJSON_GetArraySize(skills_json);
            patients[i].skill_level_required = (int*)calloc(size, sizeof(int));
            if (!patients[i].skill_level_required) {
                printf("Memory allocation failed for skill_level_required at index %d.\n", i);
                exit(1);
            }
            for (int j = 0; j < size; j++) {
                patients[i].skill_level_required[j] = cJSON_GetArrayItem(skills_json, j)->valueint;
            }
        }
        else {
            patients[i].skill_level_required = NULL;
            printf("Missing or invalid skill_level_required for patient at index %d.\n", i);
        }
        patients[i].is_admitted = 0;
        patients[i].nurses_allotted = NULL; // Initialize to NULL
        patients[i].num_nurses_allotted = 0;
    }
    mandatory_patients = (Patient**)realloc(mandatory_patients, mandatory_count * sizeof(Patient*));
    ASSERT(mandatory_patients, "Memory allocation failed for mandatory_patients.");
    optional_patients = (Patient**)realloc(optional_patients, optional_count * sizeof(Patient*));
	ASSERT(optional_patients, "Memory allocation failed for optional_patients.");
    // Check if memory allocation was successful
    // Print counts for verification
    printf("Number of mandatory patients: %d\n", mandatory_count);
    printf("Number of optional patients: %d\n", optional_count);
}

void free_patients() {
    if (!patients) return; // No need to free if patients is NULL
    for (int i = 0; i < num_patients; i++) {
        if (patients[i].incompatible_room_ids) free(patients[i].incompatible_room_ids);
        if (patients[i].workload_produced) free(patients[i].workload_produced);
        if (patients[i].skill_level_required) free(patients[i].skill_level_required);
        if (patients[i].nurses_allotted) free(patients[i].nurses_allotted);
    }
    if (patients) free(patients);
    patients = NULL; // Set to NULL to avoid dangling pointer issues
}

// function to parse the surgeons
void parse_surgeons(cJSON* surgeons_array) {
    num_surgeons = cJSON_GetArraySize(surgeons_array);
    // printf("%d\n" , num_surgeons);
    surgeon = (Surgeon*)calloc(num_surgeons, sizeof(Surgeon));
    if (!surgeon) {
        printf("Memory allocation failed for surgeons.\n");
        exit(1);
    }
    for (int i = 0; i < num_surgeons; i++) {
        cJSON* item = cJSON_GetArrayItem(surgeons_array, i);
        if (!item) continue;
        cJSON* id_json = cJSON_GetObjectItem(item, "id");
        if (id_json && cJSON_IsString(id_json)) {
            (surgeon)[i].id = str2int(id_json->valuestring); // Dynamically allocate and copy the string
        }
        else {
            (surgeon)[i].id = -1;
            printf("Missing or invalid ID for surgeon at index %d.\n", i);
        }
        cJSON* max_surgery_time_json = cJSON_GetObjectItem(item, "max_surgery_time");
        if (max_surgery_time_json && cJSON_IsArray(max_surgery_time_json)) {
            int size = cJSON_GetArraySize(max_surgery_time_json);
            (surgeon)[i].max_surgery_time = (int*)calloc(size, sizeof(int));
            (surgeon)[i].time_left = (int*)calloc(size, sizeof(int));
            if (!(surgeon)[i].max_surgery_time) {
                printf("Memory allocation failed for max_surgery_time at index %d.\n", i);
                exit(1);
            }
            for (int j = 0; j < size; j++) {
                (surgeon)[i].max_surgery_time[j] = cJSON_GetArrayItem(max_surgery_time_json, j)->valueint;
                (surgeon)[i].time_left[j] = cJSON_GetArrayItem(max_surgery_time_json, j)->valueint;
            }
        }
        else {
            (surgeon)[i].max_surgery_time = NULL;
            (surgeon)[i].time_left = NULL;
            printf("Missing or invalid max_surgery_time for surgeon at index %d.\n", i);
        }
        surgeon[i].patients_assigned = NULL;
    }
}

void assign_patients_to_surgeon(void) {
    for (int i = 0; i < num_patients; i++) {
        int surgeon_index = patients[i].surgeon_id;  // Get the surgeon_id from the patient
        if (surgeon[surgeon_index].patients_assigned == NULL) {
            surgeon[surgeon_index].patients_assigned = (int*)calloc(100, sizeof(int)); // create a block for 100 patients
            surgeon[surgeon_index].current_size = 100;
        }
        else
            if (surgeon[surgeon_index].num_assigned_patients == surgeon[surgeon_index].current_size) {// increase the current_size by 100
                surgeon[surgeon_index].current_size += 100;
                surgeon[surgeon_index].patients_assigned = (int*)realloc(surgeon[surgeon_index].patients_assigned, (surgeon[surgeon_index].current_size) * sizeof(int));
            }
        ASSERT(surgeon[surgeon_index].patients_assigned, "Dynamic Memory Allocation Error");
        surgeon[surgeon_index].patients_assigned[surgeon[surgeon_index].num_assigned_patients++] = patients[i].id;
    }
}

void free_surgeons() {
    if (!surgeon) return; // Check if surgeon array exists

    for (int i = 0; i < num_surgeons; i++) {
        if (surgeon[i].patients_assigned) free(surgeon[i].patients_assigned);
        if (surgeon[i].max_surgery_time) free(surgeon[i].max_surgery_time);
    }

    if (surgeon) free(surgeon); // Free the array of Surgeon structs
    surgeon = NULL; // Avoid dangling pointer
}

// Function to parse the operating theatres
void parse_ots(cJSON* ot_array) {
    num_ots = cJSON_GetArraySize(ot_array);
    //printf("Number of Operating Theatres: %d\n", num_ots);
    ot = (OTs*)calloc(num_ots, sizeof(OTs));
    if (!ot) {
        printf("Memory allocation failed for operating theatres.\n");
        exit(1);
    }
    for (int i = 0; i < num_ots; i++) {
        cJSON* item = cJSON_GetArrayItem(ot_array, i);
        if (!item) continue;
        // Parse 'id'
        cJSON* id_Json = cJSON_GetObjectItem(item, "id");
        if (id_Json && cJSON_IsString(id_Json)) {
            ot[i].id = str2int(id_Json->valuestring);
        }
        else {
            ot[i].id = -1;
            printf("Missing or invalid ID for operating theatre at index %d.\n", i);
            continue;  // Skip further processing for this OT
        }
        // Parse 'max_ot_time'
        cJSON* max_ot_time_json = cJSON_GetObjectItem(item, "availability");
        if (max_ot_time_json && cJSON_IsArray(max_ot_time_json)) {
            int num_days = cJSON_GetArraySize(max_ot_time_json);
            ot[i].max_ot_time = (int*)calloc(num_days, sizeof(int));
            ot[i].time_left = (int*)calloc(num_days, sizeof(int));
            if (!ot[i].max_ot_time) {
                printf("Memory allocation failed for max_ot_time.\n");
                exit(1);
            }
            for (int n_day = 0; n_day < num_days; n_day++) {
                cJSON* time_item = cJSON_GetArrayItem(max_ot_time_json, n_day);
                int time = time_item->valueint;
                if (time_item && cJSON_IsNumber(time_item)) {
                    ot[i].max_ot_time[n_day] = time_item->valueint;
                    ot[i].time_left[n_day] = time_item->valueint;
                }
                else printf("Invalid max_ot_time value at index. Defaulting to 0.\n");

            }
        }
    }
}

void free_ots() {
    if (!ot) return; // Check if 'ot' is already NULL

    for (int i = 0; i < num_ots; i++) {
        if (ot[i].max_ot_time) free(ot[i].max_ot_time);
        if (ot[i].time_left) free(ot[i].time_left);
    }

    if (ot) free(ot); // Free the array of OT structs
    ot = NULL; // Avoid dangling pointer
}

void parse_rooms(cJSON* room_array) {
    num_rooms = cJSON_GetArraySize(room_array);
    room = (Rooms*)calloc(num_rooms, sizeof(Rooms));
    if (!room) {
        printf("Memory allocation failed for room array");
        exit(1);
    }
    for (int i = 0; i < num_rooms; i++) {
        cJSON* item = cJSON_GetArrayItem(room_array, i);
        if (!item) continue;
        room[i].length_of_nurses_allotted = 0;
        room[i].nurses_allotted = NULL;
        room[i].nurses_allotted = NULL;
        room[i].num_patients_info = (int*)calloc(days, sizeof(int));
        room[i].gender_days_info = (gender*)calloc(days, sizeof(gender));
        for (int m = 0; m < days; m++) room[i].gender_days_info[m] = -1;
        cJSON* id_json = cJSON_GetObjectItem(item, "id");
        // room[i].occupied_cap = -1;
        if (id_json && cJSON_IsString(id_json)) room[i].id = str2int(id_json->valuestring);
        else {
            room[i].id = -1;
            printf("Missing or invalid ID for room id.");
        }
        cJSON* capacity_json = cJSON_GetObjectItem(item, "capacity");
        if (capacity_json && cJSON_IsNumber(capacity_json)) room[i].cap = capacity_json->valueint;
        else printf("Missing or invalid capacity for room at index %d. Defaulting to 0.\n", i);
        room[i].gen = -1;
	room[i].length_of_nurses_allotted = 0;
    }
}

void free_rooms(void) {
    if (!room) return;
    int i;
    for (i = 0; i < num_rooms; ++i) {
        if (room[i].nurses_allotted) free(room[i].nurses_allotted);
        if (room[i].num_patients_info) free(room[i].num_patients_info);
        if (room[i].gender_days_info) free(room[i].gender_days_info);
    }
    if (room) free(room);
    room = NULL; // Avoid dangling pointer
}

void parse_nurse(cJSON* nurses_array) {
    // Get the number of nurses from the JSON array
    num_nurses = cJSON_GetArraySize(nurses_array);
    nurses = (Nurses*)calloc(num_nurses, sizeof(Nurses)); // Allocate memory for nurses
    if (!nurses) {
        printf("Memory allocation failed for nurses.\n");
        exit(1);
    }
    for (int i = 0; i < num_nurses; i++) {
        cJSON* item = cJSON_GetArrayItem(nurses_array, i);
        if (!item) continue;
        // Parse 'id'
        cJSON* id_json = cJSON_GetObjectItem(item, "id");
        if (id_json && cJSON_IsString(id_json)) {
            nurses[i].id = str2int(id_json->valuestring); // Convert string ID to integer
        }
        else {
            nurses[i].id = -1;
            printf("Missing or invalid ID for nurse at index %d.\n", i);
        }
        // Parse 'skill_level'
        cJSON* skill_level_json = cJSON_GetObjectItem(item, "skill_level");
        if (skill_level_json && cJSON_IsNumber(skill_level_json)) {
            nurses[i].skill_level = skill_level_json->valueint;
        }
        else {
            nurses[i].skill_level = -1; // Default error value
            printf("Missing or invalid skill level for nurse at index %d.\n", i);
        }
        // Parse 'working_shifts'
        cJSON* shifts_json = cJSON_GetObjectItem(item, "working_shifts");
        if (shifts_json && cJSON_IsArray(shifts_json)) {
            int shift_count = cJSON_GetArraySize(shifts_json);
            nurses[i].shift = (Shifts*)calloc(shift_count, sizeof(Shifts)); // Allocate memory for shifts
            if (!nurses[i].shift) {
                printf("Memory allocation failed for shifts of nurse at index %d.\n", i);
                exit(1);
            }
            nurses[i].num_shifts = shift_count; // Store the number of shifts for the nurse
            for (int j = 0; j < shift_count; j++) {
                cJSON* shift_item = cJSON_GetArrayItem(shifts_json, j);
                if (shift_item) {
                    cJSON* day_json = cJSON_GetObjectItem(shift_item, "day");
                    if (day_json && cJSON_IsNumber(day_json)) {
                        nurses[i].shift[j].day = day_json->valueint;
                    }
                    else {
                        nurses[i].shift[j].day = -1; // Default error value
                        printf("Missing or invalid day for nurse %d, shift %d.\n", i, j);
                    }

                    // Parse 'shift'
                    cJSON* shift_json = cJSON_GetObjectItem(shift_item, "shift");
                    if (shift_json && cJSON_IsString(shift_json)) {
                        nurses[i].shift[j].shift_time = string_to_shift_types(shift_json->valuestring); // Convert string to enum
                    }
                    else {
                        nurses[i].shift[j].shift_time = early; // Default error value
                        printf("Missing or invalid shift time for nurse %d, shift %d.\n", i, j);
                    }

                    // Parse 'max_load'
                    cJSON* max_load_json = cJSON_GetObjectItem(shift_item, "max_load");
                    if (max_load_json && cJSON_IsNumber(max_load_json)) {
                        nurses[i].shift[j].max_load = max_load_json->valueint;
                        nurses[i].shift[j].load_left = max_load_json->valueint;
                    }
                    else {
                        nurses[i].shift[j].max_load = -1; // Default error value
                        printf("Missing or invalid max load for nurse %d, shift %d.\n", i, j);
                    }
                    nurses[i].shift[j].rooms = NULL;
                    nurses[i].shift[j].num_rooms = 0; // Initialize num_rooms to 0
                }
            }
        }
        else {
            nurses[i].shift = NULL;
            nurses[i].num_shifts = 0; // Set number of shifts to zero
            printf("Missing or invalid working_shifts for nurse at index %d.\n", i);
        }
    }
}

void free_nurses() {
    if (nurses) {
        for (int i = 0; i < num_nurses; i++)
            if (nurses[i].shift) {
                for (int j = 0; j < nurses[i].num_shifts; j++)
                    if (nurses[i].shift[j].rooms)
                        free(nurses[i].shift[j].rooms); // Free the rooms array if allocated
                free(nurses[i].shift); // Free the shifts array for the nurse
            }
        if (nurses) free(nurses); // Free the nurses array itself
        nurses = NULL; // Avoid dangling pointer
    }
}

// Function to parse the JSON file
void parse_json(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Unable to open file.\n");
        return;
    }
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* json_data = (char*)calloc(1, file_size + 1);
    if (!json_data) {
        perror("malloc");
        exit(1);
    }
    size_t got = fread(json_data, 1, file_size, file);
    if (got != file_size) {
        fprintf(stderr, "Short read: got %zu of %ld bytes\n", got, file_size);
        exit(1);
    }
    fclose(file);
    json_data[file_size] = '\0';
    cJSON* root = cJSON_Parse(json_data);
    if (!root) {
        printf("Error parsing JSON.\n");
        if (json_data) free(json_data);
        return;
    }
    days = cJSON_GetObjectItem(root, "days")->valueint;
    skill_levels = cJSON_GetObjectItem(root, "skill_levels")->valueint;
    // Parse weights
    cJSON* weights_json = cJSON_GetObjectItem(root, "weights");
    weights = (Weights*)calloc(1, sizeof(Weights));
    parse_weights(weights_json);
    // Parse occupants
    cJSON* occupants_array = cJSON_GetObjectItem(root, "occupants");
    parse_occupants(occupants_array);
    // parse patients
    cJSON* patients_array = cJSON_GetObjectItem(root, "patients");
    parse_patients(patients_array);
    // parse surgeons
    cJSON* surgery_array = cJSON_GetObjectItem(root, "surgeons");
    parse_surgeons(surgery_array);
    assign_patients_to_surgeon();
    // parse ots
    cJSON* ot_array = cJSON_GetObjectItem(root, "operating_theaters");
    parse_ots(ot_array);
    // parse rooms
    cJSON* room_array = cJSON_GetObjectItem(root, "rooms");
    parse_rooms(room_array);
    assign_occupants_to_rooms();
    // parse nurses
    cJSON* nurse_array = cJSON_GetObjectItem(root, "nurses");
    parse_nurse(nurse_array);
    if (json_data) free(json_data);
    cJSON_Delete(root);
}

// Function to print occupants data
void print_occupants() {
    printf("\nOccupants Data:\n");
    for (int i = 0; i < num_occupants; i++) {
        printf("Occupant id %d:\n", occupants[i].id);
        printf("  Gender: %d\n", occupants[i].gen);
        printf("  Age Group: %s\n", occupants[i].age);
        printf("  Length of Stay: %d\n", occupants[i].length_of_stay);
        printf("  Room ID: %d\n", occupants[i].room_id);
        if (occupants[i].workload_produced) {
            printf("  Workload Produced: ");
            for (int j = 0; j < occupants[i].length_of_stay * 3; j++) {
                printf("%d ", occupants[i].workload_produced[j]);
            }
            printf("\n");
        }
        if (occupants[i].skill_level_required) {
            printf("  Skill Levels Required: ");
            for (int j = 0; j < occupants[i].length_of_stay * 3; j++) {
                printf("%d ", occupants[i].skill_level_required[j]);
            }
            printf("\n");
        }
    }
}

void print_surgeons(Surgeon* surgeons) {
    if (!surgeons || num_surgeons <= 0) {
        printf("No surgeons to display.\n");
        return;
    }
    printf("List of Surgeons:\n");
    for (int i = 0; i < num_surgeons; i++) {
        printf("Surgeon ID: %d\n", surgeons[i].id);
        printf("Max Surgery Time (by day): ");
        if (surgeons[i].max_surgery_time) for (int j = 0; j < days; j++) printf("%d ", surgeons[i].max_surgery_time[j]);
        else printf("No data available");

        printf("Surgeon Time Left (by day): ");
        if (surgeons[i].time_left) for (int j = 0; j < days; j++) printf("%d ", surgeons[i].time_left[j]);
        else printf("No data available");

        printf("\n-----------------------------------\n");
    }
}

// Printing the ots values
void print_ots(OTs* ots) {
    if (!ots || num_ots <= 0) {
        printf("No ots to display.\n");
        return;
    }
    for (int i = 0; i < num_ots; i++) {
        printf("\nid: %d", ot[i].id);
        printf("\nMax OT Time (by day): ");
        if (ot[i].max_ot_time) for (int j = 0; j < days; j++) printf("%d ", ot[i].max_ot_time[j]);
        else printf("No data available.");

        printf("OT Time left (by day): ");
        if (ot[i].time_left) for (int j = 0; j < days; j++) printf("%d ", ot[i].time_left[j]);
        else printf("No data available.");

        printf("\n-----------------------------------\n");
    }
}

// Printing the rooms
void print_rooms() {
    if (!room || num_rooms <= 0) {
        printf("No rooms to display.\n");
        return;
    }
    for (int i = 0; i < num_rooms; i++)
        printf("%d\t%d\t%d\n", room[i].id, room[i].cap, room[i].num_patients_allocated);
    printf("\n-----------------------------------\n");
}

void print_nurses() {
    if (nurses == NULL || num_nurses == 0) {
        printf("No nurse data available.\n");
        return;
    }

    for (int i = 0; i < num_nurses; i++) {
        printf("Nurse ID: %d\n", nurses[i].id);
        printf("Skill Level: %d\n", nurses[i].skill_level);
        printf("Number of Shifts: %d\n", nurses[i].num_shifts);

        for (int j = 0; j < nurses[i].num_shifts; j++) {
            printf("  Shift %d:\n", j + 1);
            printf("    Day: %d\n", nurses[i].shift[j].day);
            printf("    Shift Time: %d\n", nurses[i].shift[j].shift_time);
            printf("    Max Load: %d\n", nurses[i].shift[j].max_load);
            printf("    Number of Rooms: %d\n", nurses[i].shift[j].num_rooms);

            printf("    Assigned Rooms: ");
            for (int k = 0; k < nurses[i].shift[j].num_rooms; k++) {
                printf("r%d ", nurses[i].shift[j].rooms[k]);
            }
            printf("\n");
        }
        printf("\n");
    }
}


void print_patients(Patient* patient_struct_array) {
    int i;
    if (!patient_struct_array) return;
    for (i = 0; i < num_patients; ++i) {
        printf("\nPatient ID: %d\n", patient_struct_array[i].id);
        printf("Mandatory: %d\n", patient_struct_array[i].mandatory);
        printf("Gender: %d\n", patient_struct_array[i].gen);
        printf("Age Group: %s\n", patient_struct_array[i].age_group);
        printf("Length of Stay: %d\n", patient_struct_array[i].length_of_stay);
        printf("Surgery Release Day: %d\n", patient_struct_array[i].surgery_release_day);
        printf("Surgery Duration: %d\n", patient_struct_array[i].surgery_duration);
        printf("Surgery Due Day: %d\n", patient_struct_array[i].surgery_due_day);
        printf("Surgeon ID: %d\n", patient_struct_array[i].surgeon_id);
        printf("Assigned room no:%d \n", patient_struct_array[i].assigned_room_no);
        // Print incompatible rooms if they exist
        if (patient_struct_array[i].num_incompatible_rooms > 0) {
            printf("Incompatible Rooms: ");
            for (int j = 0; j < patient_struct_array[i].num_incompatible_rooms; j++) {
                printf("%d ", patient_struct_array[i].incompatible_room_ids[j]);
            }
            printf("\n");
        }
        // Print workload produced if it exists
        if (patient_struct_array[i].workload_produced != NULL) {
            printf("Workload Produced: ");
            for (int j = 0; j < 3 * patient_struct_array[i].length_of_stay; j++) {
                printf("%d ", patient_struct_array[i].workload_produced[j]);
            }
            printf("\n");
        }
        // Print skill level required if it exists
        if (patient_struct_array[i].skill_level_required != NULL) {
            printf("Skill Level Required: ");
            for (int j = 0; j < 3 * patient_struct_array[i].length_of_stay; j++) {
                printf("%d ", patient_struct_array[i].skill_level_required[j]);
            }
            putchar('\n');
        }
    }
}

void print_optional_patients() {
    printf("==== Optional Patients ====\n");
    for (int i = 0; i < optional_count; i++) {
        printf("\n----- Patient %d -----\n", i);
        printf("Patient ID: %d\n", optional_patients[i]->id);
        printf("Mandatory: %d\n", optional_patients[i]->mandatory);
        printf("Gender: %d\n", optional_patients[i]->gen);
        printf("Age Group: %s\n", optional_patients[i]->age_group);
        printf("Length of Stay: %d\n", optional_patients[i]->length_of_stay);
        printf("Surgery Release Day: %d\n", optional_patients[i]->surgery_release_day);
        printf("Surgery Duration: %d\n", optional_patients[i]->surgery_duration);
        printf("Surgery Due Day: %d\n", optional_patients[i]->surgery_due_day);
        printf("Surgeon ID: %d\n", optional_patients[i]->surgeon_id);
        // Print incompatible rooms if they exist
        if (optional_patients[i]->num_incompatible_rooms > 0) {
            printf("Incompatible Rooms: ");
            for (int j = 0; j < optional_patients[i]->num_incompatible_rooms; j++) {
                printf("%d ", optional_patients[i]->incompatible_room_ids[j]);
            }
            printf("\n");
        }
        // Print workload produced if it exists
        if (optional_patients[i]->workload_produced) {
            printf("Workload Produced: ");
            for (int j = 0; j < 3 * optional_patients[i]->length_of_stay; j++) {
                printf("%d ", optional_patients[i]->workload_produced[j]);
            }
            printf("\n");
        }
        // Print skill level required if it exists
        if (optional_patients[i]->skill_level_required) {
            printf("Skill Level Required: ");
            for (int j = 0; j < 3 * optional_patients[i]->length_of_stay; j++)
                printf("%d ", optional_patients[i]->skill_level_required[j]);
            putchar('\n');
        }
    }
    printf("==================================================================\n");
}

void print_mandatory_patients() {
    printf("==== Mandatory Patients ====\n");
    for (int i = 0; i < mandatory_count; i++) {
        printf("\n----- Patient %d -----\n", i);
        printf("Patient ID: %d\n", mandatory_patients[i]->id);
        printf("Mandatory: %d\n", mandatory_patients[i]->mandatory);
        printf("Gender: %d\n", mandatory_patients[i]->gen);
        printf("Age Group: %s\n", mandatory_patients[i]->age_group);
        printf("Length of Stay: %d\n", mandatory_patients[i]->length_of_stay);
        printf("Surgery Release Day: %d\n", mandatory_patients[i]->surgery_release_day);
        printf("Surgery Duration: %d\n", mandatory_patients[i]->surgery_duration);
        printf("Surgery Due Day: %d\n", mandatory_patients[i]->surgery_due_day);
        printf("Surgeon ID: %d\n", mandatory_patients[i]->surgeon_id);
        // Print incompatible rooms if they exist
        if (mandatory_patients[i]->num_incompatible_rooms > 0) {
            printf("Incompatible Rooms: ");
            for (int j = 0; j < mandatory_patients[i]->num_incompatible_rooms; j++) {
                printf("%d ", mandatory_patients[i]->incompatible_room_ids[j]);
            }
            printf("\n");
        }
        // Print workload produced if it exists
        if (mandatory_patients[i]->workload_produced != NULL) {
            printf("Workload Produced: ");
            for (int j = 0; j < 3 * mandatory_patients[i]->length_of_stay; j++) {
                printf("%d ", mandatory_patients[i]->workload_produced[j]);
            }
            printf("\n");
        }
        // Print skill level required if it exists
        if (mandatory_patients[i]->skill_level_required != NULL) {
            printf("Skill Level Required: ");
            for (int j = 0; j < 3 * mandatory_patients[i]->length_of_stay; j++) {
                printf("%d ", mandatory_patients[i]->skill_level_required[j]);
            }
            printf("\n");
        }
    }
    printf("==================================================================\n");
}

//...................................................FUNCTION BELOW ARE FOR THE ROOM_GENDER_MAP............................................

void initialize_room_gender_map(int** room_gender_map) {
    // Allocate memory for room_gender_map (array of pointers to rows)
    *room_gender_map = (int*)calloc(num_rooms, sizeof(int)); // Allocate memory for row pointers
    if (!*room_gender_map) {
        printf("Memory allocation failed for room_gender_map.\n");
        exit(1);
    }

    // Allocate memory for each row and initialize values to -1
    for (int i = 0; i < num_rooms; i++) {
        (*room_gender_map)[i] = -1;
    }
}

void populate_room_gender_map(int** room_gender_map) {
    // populates the room_gender_map
    if (!*room_gender_map) {
        printf("Error: room_gender_map is not initialized.\n");
        return;
    }
    for (int i = 0; i < num_rooms; ++i) {
        ((*room_gender_map)[i]) = -1;
    }

    // Populate room_gender_map based on occupants
    for (int i = 0; i < num_occupants; ++i) {
        int room_id = occupants[i].room_id;

        if (room_id >= 0 && room_id < num_rooms) {
            (*room_gender_map)[room_id] = occupants[i].gen; // Correct indexing
        }
        else {
            printf("Warning: Invalid room_id %d for occupant %d.\n", room_id, i);
        }
    }
}

void print_map(int** room_gender_map) {
    for (int i = 0; i < num_rooms; i++)
        printf("room number: %d\tgender: %d\n", i, (*room_gender_map)[i]);
}

//-----------------------------------------BELOW: QUICK SORT IMPLEMENTATION FOR THE OT ARRAY--------------------------------------

int sorting_day;
void print_ot_data_arr(OTs** ot_data_ar, int d) {
    printf("\nDay: %d\n", d);
    for (int i = 0; i < num_ots; ++i)
        printf("id: %d -- max_ot_time: %d\n", ot_data_ar[i]->id, ot_data_ar[i]->max_ot_time[d]);
}

int compare_ots(const void* a, const void* b) {
    OTs* ot_a = *(OTs**)a;
    OTs* ot_b = *(OTs**)b;

    // Compare max_ot_time for the global sorting day
    if (ot_b->max_ot_time[sorting_day] != ot_a->max_ot_time[sorting_day]) {
        if (ot_b->max_ot_time[sorting_day] > ot_a->max_ot_time[sorting_day]) return 1;
        else
            if (ot_b->max_ot_time[sorting_day] < ot_a->max_ot_time[sorting_day]) return -1;
        //return ot_b->max_ot_time[sorting_day] - ot_a->max_ot_time[sorting_day]; // Descending order
    }
    return ot_a->id - ot_b->id; // Secondary sort by id (ascending order)
}

void sort_ot_data_arr(OTs** ot_data_arr, int d) {
    // sorts ot_data_arr using Quick sort algorithm
    if (ot_data_arr == NULL || num_ots <= 0) {
        fprintf(stderr, "Error: Invalid input array\n");
        return;
    }
    // print_ot_data_arr(ot_data_arr , d);
    // quick_sort_OTs_on_max_ot_time(ot_data_arr, 0, num_ots-1, d);
    sorting_day = d;
    qsort(ot_data_arr, num_ots, sizeof(OTs*), compare_ots);
    // print_ot_data_arr(ot_data_arr , d);
}

//-----------------------------------------ABOVE: QUICK SORT IMPLEMENTATION FOR THE OT ARRAY--------------------------------------

//...............FUNCTION FOR CREATING THE 3-D ARRAY TO STORE THE PATIENTS IN THE ROOM ON A PARTICULAR DAY.........

void initialize_3d_array() {
    // Allocate memory for room_schedule: 3D array (rooms x days x patients)
    room_schedule = (char****)calloc(num_rooms, sizeof(char***));  // Allocate space for rooms
    size_of_room_schedule = (int**)calloc(num_rooms, sizeof(int*));  // Allocate space for size tracking by room
    ASSERT(room_schedule, "room_schedule could not be allocated memory");
    ASSERT(size_of_room_schedule, "room_schedule could not be allocated memory");

    for (int i = 0; i < num_rooms; i++) {
        room_schedule[i] = (char***)calloc(days, sizeof(char**));  // Allocate space for days per room
        size_of_room_schedule[i] = (int*)calloc(days, sizeof(int));  // Allocate space for size tracking per day
    }
}

void free_3d_array() {
    // Free the 3D array
    if (room_schedule) {
        for (int i = 0; i < num_rooms; i++) {
            if (room_schedule[i]) {
                for (int j = 0; j < days; j++) {
                    if (room_schedule[i][j]) {
                        for (int k = 0; k < size_of_room_schedule[i][j]; k++) if (room_schedule[i][j][k]) free(room_schedule[i][j][k]); // Free each string
                        free(room_schedule[i][j]); // Free the array of strings
                    }
                }
                free(room_schedule[i]); // Free the array of days
            }
            free(size_of_room_schedule[i]); // Free the size tracking array
        }
        free(room_schedule); // Free the room schedule array
        free(size_of_room_schedule); // Free the size tracking array
        room_schedule = NULL; // Avoid dangling pointer
        size_of_room_schedule = NULL; // Avoid dangling pointer
    }
}

char* id_to_str(char prefix, int id) {
    char* result = (char*)calloc(6, sizeof(char)); // Enough space for prefix + number + null terminator
    if (result == NULL) {
        return NULL; // Return NULL if memory allocation fails
    }
    snprintf(result, 6, "%c%d", prefix, id); // Format the string
    return result;
}

void put_occupants(void) {
    // here we'll add occupants in the room_schedule array
    int i, r_id, los, j;
    for (i = 0; i < num_occupants; ++i) {
        r_id = occupants[i].room_id;
        los = occupants[i].length_of_stay;

        for (j = 0; j < los; ++j) {
            if (j >= days) break;

            if (room_schedule[r_id][j]) {
                // Corrected: Use a temporary pointer before realloc
                int new_size = size_of_room_schedule[r_id][j] + 1;
                char** temp = (char**)realloc(room_schedule[r_id][j], new_size * sizeof(char*));

                if (!temp) {
                    ASSERT(0, "Dynamic Memory Allocation Error.");
                    return;
                }

                room_schedule[r_id][j] = temp;
                room_schedule[r_id][j][size_of_room_schedule[r_id][j]] = id_to_str('a', occupants[i].id);
                size_of_room_schedule[r_id][j] = new_size;
            }
            else {
                // Corrected: Allocate memory properly
                room_schedule[r_id][j] = (char**)calloc(2, sizeof(char*));
                if (!room_schedule[r_id][j]) {
                    ASSERT(0, "Dynamic Memory Allocation Error.");
                    return;
                }

                size_of_room_schedule[r_id][j] = 1;
                room_schedule[r_id][j][0] = id_to_str('a', occupants[i].id);
            }
        }
    }
}

void create_3d_array(char type) {
    initialize_3d_array();
    int i, j, admission_day, r_id, los, number;

    put_occupants();
    if (type == 'p') number = num_patients;
    else 
        if (type == 'm') number = mandatory_count;
        else {
            printf("\nWrong type given: %c", type);
            printf("function: create_3d_array");
            exit(EXIT_FAILURE);
        }
    for (i = 0; i < number; ++i) {
        if (patients[i].admission_day == -1 || (type == 'm' && !patients[i].mandatory)) continue;
        admission_day = patients[i].admission_day;
        r_id = patients[i].assigned_room_no;
        los = patients[i].length_of_stay;

        for (j = admission_day; j < admission_day + los; ++j) {
            if (j >= days) break;

            if (room_schedule[r_id][j]) {
                // Corrected: Use a temporary pointer before realloc
                int new_size = size_of_room_schedule[r_id][j] + 1;
                char** temp = (char**)realloc(room_schedule[r_id][j], new_size * sizeof(char*));

                ASSERT(temp, "Dynamic Memory Allocation Error.");

                room_schedule[r_id][j] = temp;
                room_schedule[r_id][j][size_of_room_schedule[r_id][j]] = id_to_str('p', patients[i].id);
                size_of_room_schedule[r_id][j] = new_size;
            }
            else {
                // Corrected: Allocate memory properly
                room_schedule[r_id][j] = (char**)calloc(1, sizeof(char*));
                if (!room_schedule[r_id][j]) {
                    ASSERT(0, "Dynamic Memory Allocation Error.");
                    return;
                }

                size_of_room_schedule[r_id][j] = 1;
                room_schedule[r_id][j][0] = id_to_str('p', patients[i].id);
            }
        }
    }
    //put_occupants();
}

void print_room_schedule(void) {
    printf("Room Schedule:\n");

    for (int r = 0; r < num_rooms; ++r) {
        printf("Room %d:\n", r);

        for (int d = 0; d < days; ++d) {
            if (room_schedule[r][d] && size_of_room_schedule[r][d] > 0) {
                printf("  Day %d: ", d);

                for (int p = 0; p < size_of_room_schedule[r][d]; ++p) {
                    if (room_schedule[r][d][p]) { // Ensure pointer is valid before printing
                        char* id_str = room_schedule[r][d][p];
                        int id = str2int(id_str); // Extract numerical ID
                        printf("[%s (ID: %d)] ", id_str, id);
                    }
                }
                printf("\n");
            }
        }
        printf("\n");
    }
}

void swap_nurse_pointers(Nurses** nurse1, Nurses** nurse2) {
    Nurses* temp = *nurse1;
    *nurse1 = *nurse2;
    *nurse2 = temp;
}

int partition(Nurses** arr, int low, int high) {
    int pivot = arr[high]->shift->max_load;
    int i = low - 1;
    for (int j = low; j <= high - 1; j++) {

        if (arr[j]->shift->load_left > pivot) {
            i++;
            swap_nurse_pointers(&arr[i], &arr[j]);
        }
    }
    swap_nurse_pointers(&arr[i + 1], &arr[high]);
    return i + 1;
}

void quicksort(Nurses** arr, int low, int high) {
    if (low < high) {
        int pi = partition(arr, low, high);
        quicksort(arr, low, pi - 1);
        quicksort(arr, pi + 1, high);
    }
}

void allocate_dm_nurses_availability() {
    dm_nurses_availability = (Nurses***)calloc(days * 3, sizeof(Nurses**));
    max_load_updated = (int**)calloc(num_nurses, (sizeof(int*)));
    if (!dm_nurses_availability) {
        perror("Failed to allocate memory for main array");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < days * 3; ++i) {
        dm_nurses_availability[i] = NULL;  // Initialize each slot to NULL
        // max_load_updated[i] = (int*)calloc(days * 3, sizeof(int));
    }
    if (!max_load_updated) {
        perror("Failed to allocate memory for max_load_updated");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < num_nurses; ++i) {
        max_load_updated[i] = (int*)calloc(days * 3, sizeof(int));
        if (!max_load_updated[i]) {
            perror("Failed to allocate memory for max_load_updated row");
            exit(EXIT_FAILURE);
        }
        for (int j = 0; j < days * 3; ++j) {
            max_load_updated[i][j] = -1;
        }
    }
}

void allocate_sub_array(Nurses*** main_array, int index, int sub_size) {
    main_array[index] = (Nurses**)calloc(sub_size, sizeof(Nurses*));
    if (!main_array[index]) {
        perror("Failed to allocate memory for sub-array");
        exit(EXIT_FAILURE);
    }
}

void append_to_sub_array(Nurses*** main_array, int index, int* current_sizes, Nurses* nurse) {
    int current_size = current_sizes[index];
    int new_size = current_size + 1;
    Nurses** temp = (Nurses**)realloc(main_array[index], new_size * sizeof(Nurses*));
    if (temp) main_array[index] = temp;
    else ASSERT(!temp, "Memory allocation error.\n");
    main_array[index][current_size] = nurse; // Add pointer to Nurse structure
    current_sizes[index] = new_size;        // Update current size
}

void create_dm_nurses_availability() {
    allocate_dm_nurses_availability();
    current_size_dm_nurse = (int*)calloc(days * 3, sizeof(int));
    if (!current_size_dm_nurse) {
        perror("Failed to allocate memory for current_size_dm_nurse");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < 3 * days; i++) allocate_sub_array(dm_nurses_availability, i, 2);
    for (int i = 0; i < num_nurses; i++) {
        for (int j = 0; j < nurses[i].num_shifts; j++) {
            int index = 3 * nurses[i].shift[j].day + nurses[i].shift[j].shift_time;
            if (index < 0 || index >= 3 * days) {
                fprintf(stderr, "\nInvalid index: %d\n", index);
                continue;
            }
            append_to_sub_array(dm_nurses_availability, index, current_size_dm_nurse, nurses + i);
            max_load_updated[i][index] = nurses[i].shift[j].max_load;
            //printf("%d\n" , max_load_updated[i][index]);
        }
    }
}

void free_dm_nurses_availability() {
    for (int i = 0; i < 3 * days; i++) if (dm_nurses_availability[i]) free(dm_nurses_availability[i]);
    free(dm_nurses_availability);
    dm_nurses_availability = NULL; // Avoid dangling pointer
    for (int i = 0; i < num_nurses; i++) if (max_load_updated[i]) free(max_load_updated[i]);
    free(max_load_updated);
    max_load_updated = NULL; // Avoid dangling pointer
}

void print_dm_nurses() {
    for (int i = 0; i < 3 * days; i++) {
        printf("......Shift %d.......\n", i);
        for (int j = 0; j < current_size_dm_nurse[i]; j++)
            if (dm_nurses_availability[i][j]) printf("id: %d\t max_load: %d\t", dm_nurses_availability[i][j]->id, dm_nurses_availability[i][j]->shift->max_load);
            else printf("NULL\t");
        putchar('\n');
    }
}

// ---------------------------------------------function for creating rooms_requirement---------------------------------------------------
void initialize_rooms_req(int num_rooms) {
    rooms_requirement = (Rooms_req**)calloc(num_rooms, sizeof(Rooms_req*));
    if (rooms_requirement == NULL) {
        perror("Failed to allocate memory for rooms_requirement");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < num_rooms; i++) {
        rooms_requirement[i] = (Rooms_req*)calloc(3 * days, sizeof(Rooms_req));
        if (rooms_requirement[i] == NULL) {
            perror("Failed to allocate memory for a row");
            exit(EXIT_FAILURE);
        }
        for (int j = 0; j < 3 * days; j++) {
            rooms_requirement[i][j].load_sum = 0;
            rooms_requirement[i][j].max_skill_req = 0;
        }
    }
}

void create_rooms_req() {
    initialize_rooms_req(num_rooms);
    for (int i = 0; i < num_rooms; i++) {
        for (int j = 0; j < days; j++) {
            if ((room_schedule[i][j] && !size_of_room_schedule[i][j]) || (!room_schedule[i][j] && size_of_room_schedule[i][j]))
                ASSERT(0, "Something wrong is happening.");
            if (!room_schedule[i][j]) {
                // No patients assigned, set defaults
                for (int x = 0; x < 3; x++) {
                    rooms_requirement[i][3 * j + x].load_sum = 0;
                    rooms_requirement[i][3 * j + x].max_skill_req = 0;
                }
                continue;
            }

            for (int x = 0; x < 3; x++) {
                int sum = 0;
                int max_skill = 0;

                for (int k = 0; k < size_of_room_schedule[i][j]; k++) {
                    int id = str2int(room_schedule[i][j][k]);

                    // Ensure valid index before accessing arrays
                    int workload_index = 3 * j + x;
                    int skill_index = 3 * j + x;
                    if (j >= patients[id].length_of_stay) continue;
                    sum += patients[id].workload_produced[workload_index];
                    max_skill = (max_skill > patients[id].skill_level_required[skill_index])
                        ? max_skill
                        : patients[id].skill_level_required[skill_index];
                }

                // Assign calculated values
                rooms_requirement[i][3 * j + x].load_sum = sum;
                rooms_requirement[i][3 * j + x].max_skill_req = max_skill;
            }
        }
    }
}

void print_rooms_req() {
    for (int i = 0; i < num_rooms; i++) {
        printf("\n==============================\n");
        printf("Room ID: %d\n", i);
        printf("==============================\n");

        for (int j = 0; j < days; j++) {
            printf("  Day %d:\n", j);
            for (int x = 0; x < 3; x++) {
                int index = 3 * j + x;
                printf("    Shift %d -> Load Sum: %d, Max Skill Required: %d\n",
                    x, rooms_requirement[i][index].load_sum,
                    rooms_requirement[i][index].max_skill_req);
            }
        }
    }
}

void cleanup_rooms_req(int num_rooms) {
    if (rooms_requirement == NULL) { printf("rooms_requirement is already NULL!"); exit(EXIT_FAILURE); }
    for (int i = 0; i < num_rooms; i++) if (rooms_requirement[i]) free(rooms_requirement[i]);
    free(rooms_requirement);
    rooms_requirement = NULL; // Avoid dangling pointer
}

void initialize_room_shift_nurse() {
    room_shift_nurse = (int**)calloc(num_rooms, sizeof(int*));

    for (int i = 0; i < num_rooms; i++) {
        room_shift_nurse[i] = (int*)calloc(3 * days, sizeof(int));
    }
}

void print_max_loadd_updated() {
    for (int i = 0; i < num_nurses; i++) {
        printf("Max_load of the nurse %d\n", i);
        for (int j = 0; j < 3 * days; j++) {
            printf("%d\t", max_load_updated[i][j]);
        }
        printf("\n");
    }
}

//..................................FUNCTION FOR OUTPUT JSON FILE....................

int countDigits(int num) {
    int count = 1;
    while (num) {
        num /= 10;
        if (num) ++count;
    }
    return count;
}

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#endif

void create_json_file(Patient* patients, int num_patients, Nurses* nurse, int num_nurses, int num_rooms, char* output_path) {
    
    FILE* file = fopen(output_path, "w");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    // Calculate ID padding sizes
    int patient_digits = countDigits(num_patients);
    int nurse_digits = countDigits(num_nurses);
    int room_digits = countDigits(num_rooms);
    int ot_digits = countDigits(num_ots);

    // Write JSON data
    fprintf(file, "{");

    // Patients array
    fprintf(file, "  \"patients\": [\n");
    for (int i = 0; i < num_patients; i++) {
        fprintf(file, "    {\n");
        fprintf(file, "      \"id\": \"p%0*d\",\n", patient_digits, patients[i].id);

        if (patients[i].admission_day != -1) {
            fprintf(file, "      \"admission_day\": %d,\n", patients[i].admission_day);
            fprintf(file, "      \"room\": \"r%0*d\",\n", room_digits, patients[i].assigned_room_no);
            fprintf(file, "      \"operating_theater\": \"t%0*d\"\n", ot_digits, patients[i].assigned_ot);
        }
        else {
            fprintf(file, "      \"admission_day\": \"none\"\n");
        }
        fprintf(file, "    }%s\n", (i < num_patients - 1) ? "," : "");
    }
    fprintf(file, "  ],\n");

    // Nurses array
    fprintf(file, "  \"nurses\": [\n");
    for (int i = 0; i < num_nurses; i++) {
        fprintf(file, "    {\n");
        fprintf(file, "      \"id\": \"n%0*d\",\n", patient_digits, nurses[i].id);
        fprintf(file, "      \"assignments\": [\n");

        for (int j = 0; j < nurses[i].num_shifts; j++) {
            fprintf(file, "        {\n");
            fprintf(file, "          \"day\": %d,\n", nurses[i].shift[j].day);
            fprintf(file, "          \"shift\": \"%s\",\n", shift_types_to_string(nurses[i].shift[j].shift_time));
            fprintf(file, "          \"rooms\": [\n");

            for (int k = 0; k < nurses[i].shift[j].num_rooms; k++)
                fprintf(file, "            \"r%0*d\"%s\n", room_digits, nurses[i].shift[j].rooms[k], (k < nurses[i].shift[j].num_rooms - 1) ? "," : "");
            fprintf(file, "          ]\n");
            fprintf(file, "        }%s\n", (j < nurses[i].num_shifts - 1) ? "," : "");
        }

        fprintf(file, "      ]\n");
        fprintf(file, "    }%s\n", (i < num_nurses - 1) ? "," : "");
    }
    fprintf(file, "  ]\n");
    fprintf(file, "}\n");

    fclose(file);
    printf("JSON file saved at: %s\n", output_path);
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------

//Integrated_GA.c
//-------------------------------------------------------------------------------------------------------------------------------------------------------
int*** surgeon_day_theatre_count;
int last_swap_pos1 = -1, last_swap_pos2 = -1;

typedef struct {
    int* G_BEST;
    int thread_id;
} ThreadData;

//pthread_mutex_t lock;
int* global_best_solution;
int global_best_fitness = INT_MAX;
int stop_threads = 0; // Flag to stop threads

void update_num_patients_info(void);

void reset_valuesInteGA(void) {
    //reset the patient structure values
    //reset rooms structure values.
    //reset the surgeon structure values.
    //reset the OT structure values.
    //reset the nurse structure values.
    //reset the occupants structure values.

    int i, j;

    //patient_structure
    for (i = 0; i < num_patients; i++) {
        patients[i].is_admitted = 0;
        patients[i].assigned_ot = -1;
        patients[i].assigned_room_no = -1;
        patients[i].admission_day = -1;
        if (patients[i].nurses_allotted) free(patients[i].nurses_allotted);
        patients[i].nurses_allotted = NULL;
        patients[i].num_nurses_allotted = 0;
    }

    //room_structure
    for (i = 0; i < num_rooms; i++) {
        room[i].num_patients_allocated = 0;
        room[i].occupants_cap = 0;
        for (int j = 0; j < days; ++j) {
            room[i].num_patients_info[j] = 0;
            room[i].gender_days_info[j] = -1;
        }
        if (room[i].nurses_allotted) free(room[i].nurses_allotted);
        room[i].nurses_allotted = NULL;
        room[i].length_of_nurses_allotted = 0;
    }
    //assign_occupants_to_rooms();
    update_num_patients_info();

    for (i = 0; i < num_nurses; ++i)
        for (j = 0; j < nurses[i].num_shifts; ++j) {
            if (nurses[i].shift[j].rooms) free(nurses[i].shift[j].rooms);
            nurses[i].shift[j].rooms = NULL;
            nurses[i].shift[j].num_rooms = 0;
            nurses[i].shift[j].load_left = nurses[i].shift[j].max_load;
        }

    for (i = 0; i < num_occupants; ++i) {
        if (occupants[i].nurses_allotted) free(occupants[i].nurses_allotted);
        occupants[i].nurses_allotted = NULL;
        occupants[i].num_nurses_allotted = 0;
    }

    //surgeon_structure
    for (int i = 0; i < num_surgeons; i++) for (int j = 0; j < days; j++) surgeon[i].time_left[j] = surgeon[i].max_surgery_time[j];

    //OT_structure
    for (int i = 0; i < num_ots; i++) for (int j = 0; j < days; j++) ot[i].time_left[j] = ot[i].max_ot_time[j];
}

void printOtDaysDataArr(OTs*** arr)
{
    int d, i;
    for (d = 0; d < days; ++d) {
        putchar('\n');
        for (i = 0; i < num_ots; ++i)
            printf("id: %d\tmax_ot_time: %d", arr[d][i]->id, arr[d][i]->max_ot_time[d]);
    }
}

void sort_ot_days_data_arr(OTs*** ot_days_data_arr, int day) {
    if (ot_days_data_arr == NULL || num_ots <= 0 || day < 0 || day >= days) {
        fprintf(stderr, "Error: Invalid input array or day\n");
        return;
    }
    sorting_day = day;
    qsort(ot_days_data_arr[day], num_ots, sizeof(OTs*), compare_ots);
}

int admitPatientsInteGA(int* chromosome, char type) {
    int i, j, k, p_id, s_id, r_id, d, ot_id, max = 0, flag, number;
    int current_ot_index = 0, unscheduled_mandatory = 0, scheduled_optional = 0, scheduled_mandatory = 0;
    
    // Allocate memory for OT scheduling data
    OTs** ot_data_arr = (OTs**)calloc(num_ots, sizeof(OTs*));
    OTs*** ot_days_data_arr = (OTs***)calloc(days, sizeof(OTs**)), * current_ot;

    if (!ot_data_arr || !ot_days_data_arr) {
        fprintf(stderr, "Memory allocation failed.\n");
        free(ot_data_arr);
        for (i = 0; i < days; ++i) if (ot_days_data_arr[i]) free(ot_days_data_arr[i]);
        free(ot_days_data_arr);
        ot_data_arr = NULL;
        ot_days_data_arr = NULL; // Avoid dangling pointers
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < num_ots; ++i) ot_data_arr[i] = ot + i;

    for (i = 0; i < days; ++i) {
        ot_days_data_arr[i] = (OTs**)calloc(num_ots, sizeof(OTs*));
        if (!ot_days_data_arr[i]) {
            fprintf(stderr, "Memory allocation failed for ot_days_data_arr[%d].\n", i);
            free(ot_data_arr);
            for (i = 0; i < days; ++i) if (ot_days_data_arr[i]) free(ot_days_data_arr[i]);
            free(ot_days_data_arr);
            ot_data_arr = NULL;
            ot_days_data_arr = NULL; // Avoid dangling pointers
            exit(EXIT_FAILURE);
        }
        memcpy(ot_days_data_arr[i], ot_data_arr, num_ots * sizeof(OTs*));
        sort_ot_data_arr(ot_days_data_arr[i], i);
    }
    //printOtDaysDataArr(ot_days_data_arr);
    for (int i = 0; i < num_patients; i++)
        if (max < chromosome[i])
            max = chromosome[i];
    // Allocate memory for patient scheduling tracking
    int* unscheduled_mandatory_patients = (int*)calloc(num_patients, sizeof(int));
    int* scheduled_optional_patients = (int*)calloc(num_patients, sizeof(int));
    int* scheduled_mandatory_patients = (int*)calloc(num_patients, sizeof(int));

    if (!unscheduled_mandatory_patients || !scheduled_optional_patients || !scheduled_mandatory_patients) {
        fprintf(stderr, "Memory allocation failed for patient arrays.\n");
        free(unscheduled_mandatory_patients);
        free(scheduled_optional_patients);
        free(scheduled_mandatory_patients);
        unscheduled_mandatory_patients = NULL;
        scheduled_optional_patients = NULL;
        scheduled_mandatory_patients = NULL; // Avoid dangling pointers
        for (i = 0; i < days; ++i) free(ot_days_data_arr[i]);
        free(ot_days_data_arr);
        free(ot_data_arr);
        ot_days_data_arr = NULL;
        ot_data_arr = NULL; // Avoid dangling pointers
        exit(EXIT_FAILURE);
    }

    // Initialize patient admission status (reset from previous runs)
    for (i = 0; i < num_patients; i++) {
        patients[i].is_admitted = 0;
        patients[i].admission_day = -1;
        patients[i].assigned_ot = -1;
        patients[i].assigned_room_no = -1;
    }

    // ------------------------------------------------------------ START SCHEDULING ------------------------------------------------------------

    for (int p = 0, count = 0; p < num_patients; ++p) {
        if (type == 'm' && !patients[p].mandatory) continue;

        p_id = chromosome[count++];

        // Validate p_id is within bounds
        if (p_id < 0 || p_id >= num_patients) {
            fprintf(stderr, "Error: Invalid patient ID %d in chromosome\n", p_id);
            continue;
        }

        s_id = patients[p_id].surgeon_id;

        // Validate surgeon ID
        if (s_id < 0 || s_id >= num_surgeons) {
            fprintf(stderr, "Error: Invalid surgeon ID %d for patient %d\n", s_id, p_id);
            continue;
        }

        flag = patients[p_id].mandatory;

        for (d = patients[p_id].surgery_release_day; d < days; ++d) {
            // Check if the due date has passed if the patient is mandatory
            if (flag && patients[p_id].surgery_due_day < d) {
                unscheduled_mandatory++;
                unscheduled_mandatory_patients[p_id] = 1;  // Use p_id as index
                printf("\nbreaking...\tunscheduled_mandatory found -> p_id: %d", p_id);
                break;
            }

            if (surgeon[s_id].time_left[d] < patients[p_id].surgery_duration) continue;

            // --------------------------------------------------------------FINDING OT----------------------------------------------------------
            current_ot_index = 0;
            ot_id = -1;
            current_ot = NULL;

            // Looking for OTs available which have been assigned to other patients who also have the same surgeon
            for (j = 0; j < surgeon[s_id].num_assigned_patients; j++) {
                int assigned_patient_id = surgeon[s_id].patients_assigned[j];

                // Validate assigned patient ID
                if (assigned_patient_id < 0 || assigned_patient_id >= num_patients) continue;

                if (patients[assigned_patient_id].admission_day != d) continue; // Patient not admitted on this day

                int assigned_ot_id = patients[assigned_patient_id].assigned_ot;
                if (assigned_ot_id < 0 || assigned_ot_id >= num_ots) continue;

                if (ot[assigned_ot_id].time_left[d] < patients[p_id].surgery_duration) continue;

                current_ot = ot + assigned_ot_id;
                ot_id = current_ot->id;
                break;
            }

            if (current_ot == NULL) {
                // If no OT found, look for the first available OT
                while (current_ot_index < num_ots) {
                    current_ot = ot_days_data_arr[d][current_ot_index++];
                    if (!current_ot || current_ot->time_left[d] < patients[p_id].surgery_duration) continue;
                    ot_id = current_ot->id;
                    break;
                }
            }

            if (ot_id == -1) continue; // No suitable OT found

            // --------------------------------------------------------------FINDING ROOM----------------------------------------------------------
            r_id = findSuitableRoomInteGA(p_id, d);
            if (r_id == -1) continue; // No suitable room found

            // Validate room ID
            if (r_id < 0 || r_id >= num_rooms) {
                fprintf(stderr, "Error: Invalid room ID %d returned\n", r_id);
                continue;
            }

            // --------------------------------------------------------------ADMIT PATIENT AND MODIFY ATTRIBUTES----------------------------------------------------------
            for (k = d; k < days && k < (d + patients[p_id].length_of_stay); k++) {
                room[r_id].num_patients_info[k]++;
                room[r_id].gender_days_info[k] = patients[p_id].gen;
            }

            room[r_id].num_patients_allocated++;
            room[r_id].gen = patients[p_id].gen;
            patients[p_id].admission_day = d;
            patients[p_id].assigned_ot = ot_id;
            patients[p_id].assigned_room_no = r_id;
            patients[p_id].is_admitted = 1;
            surgeon[s_id].time_left[d] -= patients[p_id].surgery_duration;
            ot[ot_id].time_left[d] -= patients[p_id].surgery_duration;

            if (!flag) {
                scheduled_optional++;
                scheduled_optional_patients[p_id] = 1;
            }
            else {
                scheduled_mandatory++;
                scheduled_mandatory_patients[p_id] = 1;
            }

            // Validate surgeon_day_theatre_count indices
            if (patients[p_id].surgeon_id >= 0 && patients[p_id].surgeon_id < num_surgeons &&
                d >= 0 && d < days &&
                ot_id >= 0 && ot_id < num_ots) {
                surgeon_day_theatre_count[patients[p_id].surgeon_id][d][ot_id]++;
            }

            break; // Move to the next patient after admitting one
        }
    }

    // REMOVED: The redundant counting loop that was causing double counting
    // The counts are already correctly maintained in the scheduling loop above

    // Clean up memory
    if (unscheduled_mandatory_patients) {
        free(unscheduled_mandatory_patients);
        unscheduled_mandatory_patients = NULL;
    }
    if (scheduled_mandatory_patients) {
        free(scheduled_mandatory_patients);
        scheduled_mandatory_patients = NULL;
    }
    if (scheduled_optional_patients) {
        free(scheduled_optional_patients);
        scheduled_optional_patients = NULL;
    }
    for (i = 0; i < days; ++i) free(ot_days_data_arr[i]);
    free(ot_days_data_arr);
    free(ot_data_arr);
    ot_days_data_arr = NULL;
    ot_data_arr = NULL; // Avoid dangling pointers

    return unscheduled_mandatory;
}

int findSuitableRoomInteGA(int p_id, int d) {
    int i, j, k, flag;
    gender g = patients[p_id].gen;
    char* age = patients[p_id].age_group;
    int room_to_be_assigned = -1;

    // Check non-empty rooms (already assigned gender)
    for (i = 0; i < num_rooms; i++) {
        flag = 0;
        // Check room's gender for each day of the patient's stay AND // Check room capacity for each day of stay
        for (j = d; j < days && j < d + patients[p_id].length_of_stay; j++) {
            if (room[i].gender_days_info[j] != g || room[i].cap <= room[i].num_patients_info[j]) {
                flag = 1;
                break;
            }
        }
        if (flag) continue;

        // Check age group compatibility
        // if (strcmp(room[i].age_group, age) != 0) continue;
        // Check incompatible rooms
        for (k = 0; k < patients[p_id].num_incompatible_rooms; k++) {
            if (room[i].id == patients[p_id].incompatible_room_ids[k]) {
                flag = 1;
                break;
            }
        }
        if (flag) continue;

        // Assign room and set gender for all days
        for (j = d; j < days && j < d + patients[p_id].length_of_stay; j++) room[i].gender_days_info[j] = g;
        return room[i].id;
    }

    // Check empty rooms (gender not set)
    for (i = 0; i < num_rooms; i++) {
        flag = 0;
        for (j = d; j < days && j < d + patients[p_id].length_of_stay; j++) {
            if (room[i].gender_days_info[j] != -1 || room[i].cap <= room[i].num_patients_info[j]) {
                flag = 1;
                break;
            }
        }
        if (flag) continue;

        // Check incompatible rooms
        for (k = 0; k < patients[p_id].num_incompatible_rooms; k++) {
            if (room[i].id == patients[p_id].incompatible_room_ids[k]) {
                flag = 1;
                break;
            }
        }
        if (flag) continue;

        // Assign room and set gender for all days
        for (j = d; j < days && j < d + patients[p_id].length_of_stay; j++) room[i].gender_days_info[j] = g;
        return room[i].id;
    }

    return -1; // No suitable room found
}

//----------------------------------------------------ABOVE: FUNCTIONS FOR PATIENT ADMISSION----------------------------------------------------------

//-----------------------------------------------------------BELOW: GENETIC ALGORITHM-----------------------------------------------------------------

//-------------------------------------------------------BELOW: FUNCTION DEFINITIONS FOR GA-----------------------------------------------------------

// all taken from ihtc_ga.c
void freeDataStructuresInteGA(void);
void applyIntegratedGA(char type);
void evaluateViolationsAndCost(int* chromosome, char type);
void crossoverTournamentSelectionInteGA(void);
void mutationTournamentSelectionInteGA(void);
void generatePopulationInteGA(void);
bool mutationElitismInteGA(char type);
bool crossoverElitismInteGA(char type);
void orderCrossoverInteGA(char type);
void printPopulationInteGA(char);
void swapMutationInteGA(char type);
void generateNewChromosomeInteGA(int chromo_num);

int findViolations(void);
int findCost(void);
int patient_delay(void);
int elective_unscheduled_patients(void);
int open_operating_theatre(void);
//int room_age_mix(void);
int skill_level_mix(void);
int surgeon_transfer(void);

//-------------------------------------------------------ABOVE: FUNCTION DEFINITIONS FOR GA-----------------------------------------------------------

void printPopulationInteGA(char type)
{
    int i, j, number;
    if (type == 'm') number = mandatory_count;
    if (type == 'p') number = num_patients;

    for (i = 0; i < POP_SIZE; ++i) {
        printf("Chromosome %d: ", i + 1);
        for (j = 0; j < number; ++j)
            printf("%d ", POPULATION[i][j]);
        putchar('\n');
    }
}

void orderCrossoverInteGA(char type) {
    int r1, r2, i, k, m, number;
    int actual_chromosome_size = CHROMOSOME_SIZE_INTE_GA - 5; // Exclude last 5 positions for objectives/fitness

    if (type == 'p') number = num_patients;
    else if (type == 'm') number = mandatory_count;
    else {
        printf("\nWrong type given: %c", type);
        printf("function: orderCrossoverInteGA");
        exit(EXIT_FAILURE);
    }
    // if (number != actual_chromosome_size) printf("\nError: number[%d] != Chromosome size - 5 [%d]\n", number, actual_chromosome_size);
    if (actual_chromosome_size <= 0) return;

    // Use the larger of the two for visited array allocation to handle all possible gene values
    int visited_array_size = actual_chromosome_size;

    bool* visited1 = (bool*)calloc(num_patients, sizeof(bool));
    bool* visited2 = (bool*)calloc(num_patients, sizeof(bool));

    if (!visited1 || !visited2) {
        printf("Memory allocation failed!\n");
        if (visited1) free(visited1);
        if (visited2) free(visited2);
        exit(1);
    }

    // Generate crossover points within actual chromosome bounds (excluding last 5 positions)
    do {
        r1 = rand() % actual_chromosome_size;
        r2 = rand() % actual_chromosome_size;
    } while (r1 == r2);

    if (r1 > r2) {
        int temp = r1;
        r1 = r2;
        r2 = temp;
    }

    // Initialize offspring arrays (only the gene portion, not the objective/fitness part)
    for (i = 0; i < actual_chromosome_size; i++) {
        CROSSOVER_OFFSPRING_STORAGE_PLACE[0][i] = -1;
        CROSSOVER_OFFSPRING_STORAGE_PLACE[1][i] = -1;
    }

    // Copy genes between crossover points and mark as visited
    for (i = r1; i <= r2; i++) {
        int gene1 = CROSSOVER_PARENT_STORAGE_PLACE[0][i];
        int gene2 = CROSSOVER_PARENT_STORAGE_PLACE[1][i];

        // Bounds check for gene values (genes should be valid patient IDs)
        if (gene1 >= 0) {
            CROSSOVER_OFFSPRING_STORAGE_PLACE[0][i] = gene1;
            visited1[gene1] = true;
        }
        else {
            // Handle invalid gene - this shouldn't happen but add safety
            printf("Warning: Invalid gene1 value %d at position %d\n", gene1, i);
        }

        if (gene2 >= 0) {
            CROSSOVER_OFFSPRING_STORAGE_PLACE[1][i] = gene2;
            visited2[gene2] = true;
        }
        else {
            // Handle invalid gene - this shouldn't happen but add safety
            printf("Warning: Invalid gene2 value %d at position %d\n", gene2, i);
        }
    }

    // Fill remaining positions using order crossover logic
    k = (r2 + 1) % actual_chromosome_size;
    m = k;

    for (i = 0; i < actual_chromosome_size; i++) {
        int index = (r2 + 1 + i) % actual_chromosome_size;

        // Process first offspring
        int gene1 = CROSSOVER_PARENT_STORAGE_PLACE[1][index];
        if (gene1 >= 0 && !visited1[gene1]) {
            // Find next empty position in offspring 1
            while (k < actual_chromosome_size && CROSSOVER_OFFSPRING_STORAGE_PLACE[0][k] != -1) {
                k = (k + 1) % actual_chromosome_size;
            }
            if (k < actual_chromosome_size) {
                CROSSOVER_OFFSPRING_STORAGE_PLACE[0][k] = gene1;
                visited1[gene1] = true;
            }
        }

        // Process second offspring
        int gene2 = CROSSOVER_PARENT_STORAGE_PLACE[0][index];
        if (gene2 >= 0 && !visited2[gene2]) {
            // Find next empty position in offspring 2
            while (m < actual_chromosome_size && CROSSOVER_OFFSPRING_STORAGE_PLACE[1][m] != -1) {
                m = (m + 1) % actual_chromosome_size;
            }
            if (m < actual_chromosome_size) {
                CROSSOVER_OFFSPRING_STORAGE_PLACE[1][m] = gene2;
                visited2[gene2] = true;
            }
        }
    }

    // Handle any remaining empty positions (fill with valid unused genes)
    for (i = 0; i < actual_chromosome_size; i++) {
        if (CROSSOVER_OFFSPRING_STORAGE_PLACE[0][i] == -1) {
            // Find an unused gene for offspring 1 (genes are patient IDs from 0 to number-1)
            for (int j = 0; j < number; j++) {
                if (!visited1[j]) {
                    CROSSOVER_OFFSPRING_STORAGE_PLACE[0][i] = j;
                    visited1[j] = true;
                    break;
                }
            }
        }

        if (CROSSOVER_OFFSPRING_STORAGE_PLACE[1][i] == -1) {
            // Find an unused gene for offspring 2 (genes are patient IDs from 0 to number-1)
            for (int j = 0; j < number; j++) {
                if (!visited2[j]) {
                    CROSSOVER_OFFSPRING_STORAGE_PLACE[1][i] = j;
                    visited2[j] = true;
                    break;
                }
            }
        }
    }

    // Copy objective values and fitness from parents to offspring (last 5 positions)
    // These will be recalculated later, but we preserve the structure
    for (i = actual_chromosome_size; i < CHROMOSOME_SIZE_INTE_GA; i++) {
        CROSSOVER_OFFSPRING_STORAGE_PLACE[0][i] = CROSSOVER_PARENT_STORAGE_PLACE[0][i];
        CROSSOVER_OFFSPRING_STORAGE_PLACE[1][i] = CROSSOVER_PARENT_STORAGE_PLACE[1][i];
    }

    // Clean up memory
    if (visited1) free(visited1);
    if (visited2) free(visited2);
    visited1 = NULL;
    visited2 = NULL;
}

//----------------------------------BELOW: FUNCTION DEFINITIONS FOR MUTATION: SWAP, INSERT, INVERSION Mutations-----------------------------

void swapMutationInteGA(char type)
{   // take the offspring from MUTATED_OFFSPRING_STORAGE_PLACE and mutate it using SWAP MUTATION method
    int r1, r2, i, j;
    //p_m = abs((iter / (float)N_GEN) - p_m);

    /*
    iter == 0 -> p_m = 50% biased mutation
    iter == N_GEN -> p_m = 10% biased mutation
    */
    int number;

    if (type == 'p') number = num_patients;
    else
        if (type == 'm') number = mandatory_count;
        else {
            printf("\nWrong type given: %c", type);
            printf("function: swapMutationInteGA");
            exit(EXIT_FAILURE);
        }

    memcpy(MUTATED_OFFSPRING_STORAGE_PLACE, MUTATE_PARENT_STORAGE_PLACE, CHROMOSOME_SIZE_INTE_GA * sizeof(int));

    if (swap_mutation_r1 != -1 && swap_mutation_r2 != -1) {
        r1 = swap_mutation_r1;
        r2 = swap_mutation_r2;
    }
    else {
        // select two random patients
        do {
            r1 = rand() % number;
            r2 = rand() % number;
            swap_mutation_r1 = r1; // Store for future use
            swap_mutation_r2 = r2; // Store for future use
        } while (r1 == r2);
    }

    if ((rand() / (float)RAND_MAX) <= p_m) {
        // perform the biased mutation
        if (patients[MUTATED_OFFSPRING_STORAGE_PLACE[r1]].mandatory) {
            // select the nearest optional patient
            for (i = r1 - 1; i >= 0; --i)
                if (!patients[MUTATED_OFFSPRING_STORAGE_PLACE[i]].mandatory) {
                    r1 = i;
                    break;
                }
            if (i == -1)
                for (i = r1 + 1; i < r2; ++i)
                    if (!patients[MUTATED_OFFSPRING_STORAGE_PLACE[i]].mandatory) {
                        r1 = i;
                        break;
                    }
            if (i == r2) return; //no mutation if all the neighbours of r1 are mandaotory patients.
        }

        if (!patients[MUTATED_OFFSPRING_STORAGE_PLACE[r2]].mandatory) {
            // select the nearest mandatory patient
            for (i = r2 + 1; i < number; ++i)
                if (patients[MUTATED_OFFSPRING_STORAGE_PLACE[i]].mandatory) {
                    r2 = i;
                    break;
                }
            if (i == number)
                for (i = r2 - 1; i > r1; --i)
                    if (patients[MUTATED_OFFSPRING_STORAGE_PLACE[i]].mandatory) {
                        r2 = i;
                        break;
                    }
            if (i == r1) return; //no mutation if all the neighbours of r1 are mandaotory patients.
        }

        if (r2 <= r1) return;
    }
    MUTATED_OFFSPRING_STORAGE_PLACE[r1] += MUTATED_OFFSPRING_STORAGE_PLACE[r2];
    MUTATED_OFFSPRING_STORAGE_PLACE[r2] = MUTATED_OFFSPRING_STORAGE_PLACE[r1] - MUTATED_OFFSPRING_STORAGE_PLACE[r2];
    MUTATED_OFFSPRING_STORAGE_PLACE[r1] -= MUTATED_OFFSPRING_STORAGE_PLACE[r2];

    for (i = 0; i < 5; ++i) MUTATED_OFFSPRING_STORAGE_PLACE[number + i] = 0;
}

void insertMutationInteGA(char type)
{
    int from, to, i, number;

    if (type == 'p') number = num_patients;
    else
        if (type == 'm') number = mandatory_count;
        else {
            printf("\nWrong type given: %c", type);
            printf("function: insertMutationInteGA");
            exit(EXIT_FAILURE);
        }

    memcpy(MUTATED_OFFSPRING_STORAGE_PLACE, MUTATE_PARENT_STORAGE_PLACE, CHROMOSOME_SIZE_INTE_GA * sizeof(int));

    // Ensure 'from' is not equal to 'to'
    if (insert_mutation_r1 != -1 && insert_mutation_r2 != -1) {
        from = insert_mutation_r1;
        to = insert_mutation_r2;
    }
    else {
        do {
            from = rand() % number;
            to = rand() % number;
            insert_mutation_r1 = from; // Store for future use
            insert_mutation_r2 = to; // Store for future use
        } while (from == to);
    }

    int gene = MUTATED_OFFSPRING_STORAGE_PLACE[from];

    // Shift elements to make room
    if (from < to) for (int i = from; i < to; i++) MUTATED_OFFSPRING_STORAGE_PLACE[i] = MUTATED_OFFSPRING_STORAGE_PLACE[i + 1];
    else for (int i = from; i > to; i--) MUTATED_OFFSPRING_STORAGE_PLACE[i] = MUTATED_OFFSPRING_STORAGE_PLACE[i - 1];
    MUTATED_OFFSPRING_STORAGE_PLACE[to] = gene;

    for (i = 0; i < 5; ++i) MUTATED_OFFSPRING_STORAGE_PLACE[number + i] = -1;
}

void inversionMutationInteGA(char type)
{
    int start, end, i, number;

    if (type == 'p') number = num_patients;
    else
        if (type == 'm') number = mandatory_count;
        else {
            printf("\nWrong type given: %c", type);
            printf("function: inversionMutationInteGA");
            exit(EXIT_FAILURE);
        }

    // Pick two distinct positions such that start < end
    if (inversion_mutation_r1 != -1 && inversion_mutation_r2 != -1) {
        start = inversion_mutation_r1;
        end = inversion_mutation_r2;
    }
    else {
        do {
            start = rand() % number;
            end = rand() % number;
            inversion_mutation_r1 = start; // Store for future use
            inversion_mutation_r2 = end; // Store for future use
        } while (start == end);
    }

    if (start > end) {
        int temp = start;
        start = end;
        end = temp;
    }

    // Reverse the segment between start and end
    while (start < end) {
        int temp = MUTATED_OFFSPRING_STORAGE_PLACE[start];
        MUTATED_OFFSPRING_STORAGE_PLACE[start] = MUTATED_OFFSPRING_STORAGE_PLACE[end];
        MUTATED_OFFSPRING_STORAGE_PLACE[end] = temp;
        start++;
        end--;
    }

    for (i = 0; i < 5; ++i) MUTATED_OFFSPRING_STORAGE_PLACE[number + i] = -1;
}

//----------------------------------ABOVE: FUNCTION DEFINITIONS FOR MUTATION: SWAP, INSERT, INVERSION Mutations-----------------------------

void update_num_patients_info() {
    int i, r_id, j;
    for (i = 0; i < num_occupants; i++) {
        r_id = occupants[i].room_id;
        for (j = 0; j < occupants[i].length_of_stay; j++) {
            room[r_id].num_patients_info[j]++;
            room[r_id].gender_days_info[j] = occupants[i].gen;
        }
    }
}

void normalize(char type) {
    /* normalize the violations and cost of the chromosomes
    * Use the same Obj.- Func. Only the violations and cost have be extrapolated and interpolated, respectively between 0 and 10000.
    */
    int i, v_floor, c_floor, number;
    float v, c;
    int max_violations = 0, max_cost = 0;
    if (type == 'p') number = num_patients;
    else
        if (type == 'm') number = mandatory_count;
        else {
            printf("\nWrong type given: %c", type);
            printf("function: normalize");
            exit(EXIT_FAILURE);
        }

    for (i = 0; i < POP_SIZE; ++i) {
        if (POPULATION[i][number] > max_violations) max_violations = POPULATION[i][number];
        if (POPULATION[i][number + 1] > max_cost) max_cost = POPULATION[i][number + 1];
    }
    // Replace the normalization loop with:
    for (i = 0; i < POP_SIZE; ++i) {
        if (max_violations > 0) v = (POPULATION[i][number] * THREASHOLD) / max_violations;
        else v = 0;  // or THREASHOLD if you want to penalize zero violations

        if (max_cost > 0) c = (POPULATION[i][number + 1] * THREASHOLD) / max_cost;
        else c = 0;

        v_floor = v;
        c_floor = c;
        if ((v_floor + 0.5) < v) v_floor++;
        if ((c_floor + 0.5) < c) c_floor++;
        POPULATION[i][number + 2] = v_floor;
        POPULATION[i][number + 3] = c_floor;
    }
}

void applyIntegratedGA(char type)
{
    int i, j, k, last_improvement = 0, last_improvement_iter = 0, v_floor, c_floor, max_violations, max_cost, number, gen;
    float v, c;
    if (type == 'p') number = num_patients;
    else
        if (type == 'm') number = mandatory_count;
        else {
            printf("\nWrong type given: %c", type);
            printf("function: applyIntegratedGA");
            exit(EXIT_FAILURE);
        }

    if (type == 'm') generatePopulationInteGA();
    //printPopulationInteGA();
    for (i = 0; i < POP_SIZE; ++i) evaluateViolationsAndCost(POPULATION[i], type);
    normalize(type); // normalize all the violations and costs of the chromosomes, i.e. bring them in 0 to "THREASHOLD" range

    for (i = 0; i < POP_SIZE; ++i) POPULATION[i][CHROMOSOME_SIZE_INTE_GA - 1] = objectiveFunction(POPULATION[i], type);
    memcpy(G_BEST, POPULATION[0], CHROMOSOME_SIZE_INTE_GA * sizeof(int));
    for (i = 1; i < POP_SIZE; ++i) {
        if (G_BEST[number]) {
            if (POPULATION[i][number] < G_BEST[number])
                memcpy(G_BEST, POPULATION[i], CHROMOSOME_SIZE_INTE_GA * sizeof(int));
            else {
                if ((POPULATION[i][number] == G_BEST[number]) && (POPULATION[i][CHROMOSOME_SIZE_INTE_GA - 1] < G_BEST[CHROMOSOME_SIZE_INTE_GA - 1]))
                    memcpy(G_BEST, POPULATION[i], CHROMOSOME_SIZE_INTE_GA * sizeof(int));
            }
        }
        else {
            if (!POPULATION[i][number] && (POPULATION[i][CHROMOSOME_SIZE_INTE_GA - 1] < G_BEST[CHROMOSOME_SIZE_INTE_GA - 1]))
                memcpy(G_BEST, POPULATION[i], CHROMOSOME_SIZE_INTE_GA * sizeof(int));
        }
    }
    /*
    // Allocate memory for OT scheduling data
    ot_data_arr = (OTs**)calloc(num_ots, sizeof(OTs*));
    ot_days_data_arr = (OTs***)calloc(days, sizeof(OTs**));
    
    if (!ot_data_arr || !ot_days_data_arr) {
        fprintf(stderr, "Memory allocation failed.\n");
        if (ot_data_arr) free(ot_data_arr);
        if (ot_days_data_arr) {
            for (i = 0; i < days; ++i) {
                if (ot_days_data_arr[i]) free(ot_days_data_arr[i]);
            }
            free(ot_days_data_arr);
        }
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < num_ots; ++i) ot_data_arr[i] = ot + i;

    for (i = 0; i < days; ++i) {
        ot_days_data_arr[i] = (OTs**)calloc(num_ots, sizeof(OTs*));
        if (!ot_days_data_arr[i]) {
            fprintf(stderr, "Memory allocation failed for ot_days_data_arr[%d].\n", i);
            // Cleanup already allocated memory
            free(ot_data_arr);
            for (j = 0; j < i; ++j) {
                if (ot_days_data_arr[j]) free(ot_days_data_arr[j]);
            }
            free(ot_days_data_arr);
            exit(EXIT_FAILURE);
        }

        for (j = 0; j < num_ots; ++j) {
            ot_days_data_arr[i][j] = ot_data_arr[j];
        }
        sort_ot_data_arr(ot_days_data_arr[i], i);
    }
    */

    // ---------------------------------------------------------------START GENETIC ALGORITHM-------------------------------------------------------------
    if (type == 'm') gen = N_GEN_MANDATORY;
    else gen = N_GEN;
    for (CURR_ITER = 0; CURR_ITER < gen; ++CURR_ITER) {
        if (type == 'p') p_c = 0.2;
        if ((rand() / (float)RAND_MAX) < p_c) {
            crossoverTournamentSelectionInteGA();
            // printf("\nCrossover t - selection successful!");
            orderCrossoverInteGA(type);
            // printf("\nCrossover operation successful!");

            for (j = 0; j < 2; ++j) {
                evaluateViolationsAndCost(CROSSOVER_OFFSPRING_STORAGE_PLACE[j], type);
                // printf("\nCrossover cost & violation evaluation successful!");
                max_violations = 0;
                max_cost = 0;
                for (i = 0; i < POP_SIZE; ++i) {
                    if (POPULATION[i][number] > max_violations) max_violations = POPULATION[i][number];
                    if (POPULATION[i][number + 1] > max_cost) max_cost = POPULATION[i][number + 1];
                }
                if (max_violations == 0) max_violations = 1;
                if (max_cost == 0) max_cost = 1;
                bool has_changed = false;
                if (max_violations) {
                    v = (CROSSOVER_OFFSPRING_STORAGE_PLACE[j][number] * THREASHOLD) / max_violations;
                    v_floor = v;
                    if ((v_floor + 0.5) < v) v_floor++;
                    printf("\n[cross] v_floor = %d\tv = %f", v_floor, v);
                    CROSSOVER_OFFSPRING_STORAGE_PLACE[j][number + 2] = v_floor;
                    has_changed = true;
                }
                if (max_cost) {
                    c = (CROSSOVER_OFFSPRING_STORAGE_PLACE[j][number + 1] * THREASHOLD) / max_cost;
                    c_floor = c;
                    if ((c_floor + 0.5) < c) c_floor++;
                    printf("\n[cross] c_floor = %d\tc = %f", c_floor, c);
                    CROSSOVER_OFFSPRING_STORAGE_PLACE[j][number + 3] = c_floor;
                    has_changed = true;
                }
                if (has_changed)
                    CROSSOVER_OFFSPRING_STORAGE_PLACE[j][CHROMOSOME_SIZE_INTE_GA - 1] = objectiveFunction(CROSSOVER_OFFSPRING_STORAGE_PLACE[j], type);
            }
            bool replaced = crossoverElitismInteGA(type);
            // printf("\nCrossover elitism successful!");
            if (replaced) {
                normalize(type);
                for (i = 0; i < POP_SIZE; ++i) POPULATION[i][CHROMOSOME_SIZE_INTE_GA - 1] = objectiveFunction(POPULATION[i], type);
                for (j = 0; j < 2; ++j) {
                    if (G_BEST[number]) {
                        if (CROSSOVER_OFFSPRING_STORAGE_PLACE[j][number] < G_BEST[number]) {
                            memcpy(G_BEST, CROSSOVER_OFFSPRING_STORAGE_PLACE[j], CHROMOSOME_SIZE_INTE_GA * sizeof(int));
                            last_improvement_iter = CURR_ITER;
                        }
                        else
                            if ((CROSSOVER_OFFSPRING_STORAGE_PLACE[j][number] == G_BEST[number]) &&
                                (CROSSOVER_OFFSPRING_STORAGE_PLACE[j][CHROMOSOME_SIZE_INTE_GA - 1] < G_BEST[CHROMOSOME_SIZE_INTE_GA - 1])) {
                                memcpy(G_BEST, CROSSOVER_OFFSPRING_STORAGE_PLACE[j], CHROMOSOME_SIZE_INTE_GA * sizeof(int));
                                last_improvement_iter = CURR_ITER;
                            }
                    }
                    else
                        if (!CROSSOVER_OFFSPRING_STORAGE_PLACE[j][number] &&
                            (CROSSOVER_OFFSPRING_STORAGE_PLACE[j][CHROMOSOME_SIZE_INTE_GA - 1] < G_BEST[CHROMOSOME_SIZE_INTE_GA - 1])) {
                            memcpy(G_BEST, CROSSOVER_OFFSPRING_STORAGE_PLACE[j], CHROMOSOME_SIZE_INTE_GA * sizeof(int));
                            last_improvement_iter = CURR_ITER;
                        }
                }
            }
        }
        else {
            mutationTournamentSelectionInteGA();
            // printf("\nMutation t - selection successful!");
            swapMutationInteGA(type);
            // printf("\nMutation operation successful!");
            evaluateViolationsAndCost(MUTATED_OFFSPRING_STORAGE_PLACE, type);
            // printf("\nMutation cost & violation evaluation successful!");
            max_violations = 0;
            max_cost = 0;
            for (i = 0; i < POP_SIZE; ++i) {
                if (POPULATION[i][number] > max_violations) max_violations = POPULATION[i][number];
                if (POPULATION[i][number + 1] > max_cost) max_cost = POPULATION[i][number + 1];
            }
            if (max_violations == 0) max_violations = 1;
            if (max_cost == 0) max_cost = 1;
            bool has_changed = false;
            if (max_violations) {
                v = (MUTATED_OFFSPRING_STORAGE_PLACE[number] * THREASHOLD) / max_violations;
                v_floor = v;
                if ((v_floor + 0.5) < v) v_floor++;
                printf("\n[mut] v_floor = %d\tv = %f", v_floor, v);
                MUTATED_OFFSPRING_STORAGE_PLACE[number + 2] = v_floor;
                has_changed = true;
            }
            if (max_cost) {
                c = (MUTATED_OFFSPRING_STORAGE_PLACE[number + 1] * THREASHOLD) / max_cost;
                c_floor = c;
                if ((c_floor + 0.5) < c) c_floor++;
                printf("\n[mut] c_floor = %d\tc = %f", c_floor, c);
                MUTATED_OFFSPRING_STORAGE_PLACE[number + 3] = c_floor;
                has_changed = true;
            }
            if (has_changed)
                MUTATED_OFFSPRING_STORAGE_PLACE[CHROMOSOME_SIZE_INTE_GA - 1] = objectiveFunction(MUTATED_OFFSPRING_STORAGE_PLACE, type);
            bool replaced = mutationElitismInteGA(type);
            // printf("\nMutation elitism successful!");
            if (replaced) {
                normalize(type);
                for (i = 0; i < POP_SIZE; ++i) POPULATION[i][CHROMOSOME_SIZE_INTE_GA - 1] = objectiveFunction(POPULATION[i], type);
                if (G_BEST[number]) {
                    if (MUTATED_OFFSPRING_STORAGE_PLACE[number] < G_BEST[number]) {
                        memcpy(G_BEST, MUTATED_OFFSPRING_STORAGE_PLACE, CHROMOSOME_SIZE_INTE_GA * sizeof(int));
                        last_improvement_iter = CURR_ITER;
                    }
                    else
                        if ((MUTATED_OFFSPRING_STORAGE_PLACE[number] == G_BEST[number]) &&
                            (MUTATED_OFFSPRING_STORAGE_PLACE[CHROMOSOME_SIZE_INTE_GA - 1] < G_BEST[CHROMOSOME_SIZE_INTE_GA - 1])) {
                            memcpy(G_BEST, MUTATED_OFFSPRING_STORAGE_PLACE, CHROMOSOME_SIZE_INTE_GA * sizeof(int));
                            last_improvement_iter = CURR_ITER;
                        }
                }
                else
                    if (!MUTATED_OFFSPRING_STORAGE_PLACE[number] &&
                        (MUTATED_OFFSPRING_STORAGE_PLACE[CHROMOSOME_SIZE_INTE_GA - 1] < G_BEST[CHROMOSOME_SIZE_INTE_GA - 1])) {
                        memcpy(G_BEST, MUTATED_OFFSPRING_STORAGE_PLACE, CHROMOSOME_SIZE_INTE_GA * sizeof(int));
                        last_improvement_iter = CURR_ITER;
                    }
            }
        }
    }

    printf("\ntype: %c", type);
    printf("\nViolations in G_BEST: %d", G_BEST[number]);
    printf("\nCost of G_BEST: %d", G_BEST[number + 1]);
    printf("\nNormalized violations in G_BEST: %d", G_BEST[number + 2]);
    printf("\nNormalized cost in G_BEST: %d", G_BEST[number + 3]);
    printf("\nG_BEST Fitness: %d", G_BEST[CHROMOSOME_SIZE_INTE_GA - 1]);
    printf("\n\nPrinting G_BEST at the end of applyGA function: \n");
    for (i = 0; i < CHROMOSOME_SIZE_INTE_GA; ++i) printf("%d ", G_BEST[i]);

    printf("\nLast improvement from the population: %d", last_improvement);
    printf("\nLast improvement in the iteration: %d\n", last_improvement_iter);
}

// ---------------------------------------------------------------------BELOW: FITNESS FUNCTION---------------------------------------------------------------------

int objectiveFunction(int* chromosome, char type) {
    int number;
    float ALPHA = ((upper_bound - lower_bound) / N_GEN) * CURR_ITER + lower_bound;

    if (type == 'p') number = num_patients;
    else
        if (type == 'm') number = mandatory_count;
        else {
            printf("\nWrong type given: %c", type);
            printf("function: objectiveFunction");
            exit(EXIT_FAILURE);
        }

    // Objective function: weighted sum of violations and cost
    return ALPHA * chromosome[number + 2] + (1 - ALPHA) * chromosome[number + 3];
    // chnage - using the normalized values of the violations and cost
}

void evaluateViolationsAndCost(int* chromosome, char type) {
    int number, violations;

    if (type == 'p') number = num_patients;
    else
        if (type == 'm') number = mandatory_count;
        else {
            printf("\nWrong type given: %c", type);
            printf("function: evaluateViolationsAndCost");
            exit(EXIT_FAILURE);
        }

    reset_valuesInteGA();
    violations = admitPatientsInteGA(chromosome, type);
    if (room_schedule) free_3d_array();
    if (rooms_requirement) cleanup_rooms_req(num_rooms);
    create_3d_array(type);
    create_rooms_req();
    chromosome[number] = nurseAllocationInteGA(); // returns the number of shifts that went without any nurse
    chromosome[number] += violations;
    chromosome[number + 1] = findCost();
    printf("\n[%c] violations in chromosome: %d", type, chromosome[number]);
}

int findViolations(void) {
    int i, cost = 0;
    for (i = 0; i < num_patients; i++) if (patients[i].admission_day == -1 && patients[i].mandatory) cost++;
    return cost;
}

int findCost(void) {
    // add validator code here
    int final_cost = 0;
    final_cost += patient_delay() * weights->patient_delay; // patient delay cost
    final_cost += elective_unscheduled_patients() * weights->unscheduled_optional; // elective unscheduled patients cost
    final_cost += open_operating_theatre() * weights->open_operating_theater; // open operating theatre cost
    final_cost += coc() * weights->continuity_of_care; // cost of cancellation of care
    final_cost += excessiveNurseLoad() * weights->nurse_excessive_workload; // excessive nurse load cost
    final_cost += skill_level_mix() * weights->room_nurse_skill; // skill level mix cost
    final_cost += surgeon_transfer() * weights->surgeon_transfer; // surgeon transfer cost
    //final_cost += room_age_mix(); // not written correctly
    return final_cost;
}

// SURGEON TRANSFER
void allocate_surgeon_day_theatre_count() {
    int d = 0, s = 0;
    surgeon_day_theatre_count = (int***)calloc(num_surgeons, sizeof(int**));
    ASSERT(surgeon_day_theatre_count, "Dynamic Memory Allocation Error!");
    for (s = 0; s < num_surgeons; s++) {
        surgeon_day_theatre_count[s] = (int**)calloc(days, sizeof(int*));
        ASSERT(surgeon_day_theatre_count[s], "Dynamic Memory Allocation Error!");
        for (d = 0; d < days; d++) {
            surgeon_day_theatre_count[s][d] = (int*)calloc(num_ots, sizeof(int));
            ASSERT(surgeon_day_theatre_count[s][d], "Dynamic Memory Allocation Error!");
        }
    }
}

int surgeon_transfer() {
    int count = 0, cost = 0, s, d, t;
    for (s = 0; s < num_surgeons; s++)
        for (d = 0; d < days; d++)
        {
            count = 0;
            for (t = 0; t < num_ots; t++)
                if (surgeon_day_theatre_count[s][d][t] > 0)
                    count++;
            if (count > 1)
            {
                cost += (count - 1);
            }
        }
    return cost;
}

//PATIENT DELAY
int patient_delay() {
    int cost = 0;
    for (int p = 0; p < num_patients; p++) {
        if (patients[p].admission_day != -1 && patients[p].admission_day > patients[p].surgery_release_day) {
            cost += patients[p].admission_day - patients[p].surgery_release_day;
        }
    }
    return cost;
}

//OPTIONAL UNSCHEDULED
int elective_unscheduled_patients() {
    int cost = 0;
    for (int i = 0; i < num_patients; i++) {
        if (patients[i].admission_day == -1 && !patients[i].mandatory) {
            cost++;
        }
    }
    return cost;
}

//OPEN OT
int open_operating_theatre() {
    int cost = 0;
    for (int t = 0; t < num_ots; t++) {
        for (int d = 0; d < days; d++) {
            if (ot[t].max_ot_time[d] != ot[t].time_left[d]) {
                cost++;
            }
        }
    }
    return cost;
}

//ROOM AGE MIX
// int room_age_mix() {
//     int cost = 0;

//     for (int i = 0; i < num_rooms; i++) {
//         for (int j = 0; j < days; j++) {
//             if (size_of_room_schedule[i][j]) {
//                 int min_age = 1000, max_age = -1;

//                 for (int m = 0; m < size_of_room_schedule[i][j]; m++) {
//                     char* entry = room_schedule[i][j][m];

//                     int id = str2int(entry);
//                     int age = (entry[0] == 'p') ? patients[id].age_group : occupants[id].age;
//                     //'''''''''''''''''''''''''''''''''''''''''''''''''change'''''''''''''''''''''''''''''''''''''''''''''''''''''''
//                     if (age < min_age)
//                      min_age = age;
//                     if (age > max_age) max_age = age;
//                 }
//                 cost += (max_age - min_age);
//             }
//         }
//     }
//     return cost;
// }

int skill_level_mix() {
    int cost = 0, id, n, day, s1;
    char* p;

    for (int i = 0; i < num_rooms; i++) {
        for (int s = 0; s < 3 * days; s++) {
            day = s / 3;
            n = room_shift_nurse[i][s];

            for (int j = 0; j < size_of_room_schedule[i][day]; j++) {
                p = room_schedule[i][day][j];
                id = str2int(p);

                if (p[0] == 'p') {  // Check if it's a patient
                    s1 = s - patients[id].admission_day * 3;
                    if (patients[id].skill_level_required[s1] > nurses[n].skill_level) {
                        cost += patients[id].skill_level_required[s1] - nurses[n].skill_level;
                    }
                }
                else {
                    if (occupants[id].skill_level_required[s] > nurses[n].skill_level) {
                        cost += occupants[id].skill_level_required[s] - nurses[n].skill_level;
                    }
                }
            }
        }
    }
    return cost;
}

int excessiveNurseLoad() {
    int n, s, r, load, shift_var, cost = 0;
    for (n = 0; n < num_nurses; n++) {
        for (s = 0; s < nurses[n].num_shifts; s++) {
            int day = nurses[n].shift[s].day;
            int shift_time = nurses[n].shift[s].shift_time;
            shift_var = 3 * day + shift_time;
            for (r = 0; r < nurses[n].shift[s].num_rooms; r++) {
                load = rooms_requirement[nurses[n].shift[s].rooms[r]][shift_var].load_sum;
                if (load > nurses[n].shift[s].load_left)
                    cost += load - nurses[n].shift[s].load_left;
            }
        }
    }
    return cost;
}

int coc() {
    int cost = 0, count;
    for (int o = 0; o < num_occupants; o++)
    {
        count = countdistinctnurses_occupants(o);
        if (count > 0)
        {
            cost += count;
        }
    }
    for (int p = 0; p < num_patients; p++)
    {
        if (patients[p].admission_day != -1)
        {
            count = countdistinctnurses(p);
            if (count > 0)
            {
                cost += count;
            }
        }
    }
    return cost;

}

int countdistinctnurses(int p) {
    int* tag = (int*)calloc(num_nurses, sizeof(int));
    int i, n, count = 0;
    for (i = 0; i < patients[p].num_nurses_allotted; i++) {
        n = patients[p].nurses_allotted[i];
        if (n != -1 && !tag[n]) {
            tag[n] = 1;
            count++;
        }
    }
    return count;
}

int countdistinctnurses_occupants(int o) {
    int* tag = (int*)calloc(num_nurses, sizeof(int));
    int i, n, count = 0;
    for (i = 0; i < occupants[o].num_nurses_allotted; i++) {
        n = occupants[o].nurses_allotted[i];
        if (n != -1 && !tag[n]) {
            tag[n] = 1;
            count++;
        }
    }
    return count;
}

// ---------------------------------------------------------------------ABOVE: FITNESS FUNCTION---------------------------------------------------------------------
void crossoverTournamentSelectionInteGA(void)
{
    // select 2 parents using Tournament Selection method
    int r11, r12, r13, r21, r22, r23, f11, f12, f13, f21, f22, f23, i;
    int best_fitness, best_fitness_idx1, best_fitness_idx2;
    int actual_chromosome_size = CHROMOSOME_SIZE_INTE_GA - 5; // Exclude last 5 positions
    // printf("\nActual Chromosome Size: %d\t[printing in crossover t-selection]\n", actual_chromosome_size);
    
    /*
	printf("\nPopulation in Crossover Tournament Selection:\n");
    for (i = 0; i < POP_SIZE; i++) {
        for (int j = 0; j < CHROMOSOME_SIZE_INTE_GA; j++) {
            printf("%d ", POPULATION[i][j]);
        }
        printf("\n");
	}
    */

    // select first parent
    do {
        r11 = rand() % (POP_SIZE);
        r12 = rand() % (POP_SIZE);
        r13 = rand() % (POP_SIZE);
    } while (r11 == r12 || r12 == r13 || r11 == r13);

    // select the chromosome1 with the best fitness
    f11 = POPULATION[r11][CHROMOSOME_SIZE_INTE_GA - 1];
    f12 = POPULATION[r12][CHROMOSOME_SIZE_INTE_GA - 1];
    f13 = POPULATION[r13][CHROMOSOME_SIZE_INTE_GA - 1];
    best_fitness = f11;
    best_fitness_idx1 = r11;

    if (f12 < best_fitness) {
        if (f13 < f12) {
            best_fitness = f13;
            best_fitness_idx1 = r13;
        }
        else {
            best_fitness = f12;
            best_fitness_idx1 = r12;
        }
    }
    else {
        if (f13 < best_fitness) {
            best_fitness = f13;
            best_fitness_idx1 = r13;
        }
    }

    // select second parent
    do {
        r21 = rand() % (POP_SIZE);
        r22 = rand() % (POP_SIZE);
        r23 = rand() % (POP_SIZE);
    } while (r21 == r22 || r22 == r23 || r21 == r23 ||
        r11 == r21 || r11 == r22 || r11 == r23 ||
        r12 == r21 || r12 == r22 || r12 == r23 ||
        r13 == r21 || r13 == r22 || r13 == r23);

    // select the chromosome2 with the best fitness
    f21 = POPULATION[r21][CHROMOSOME_SIZE_INTE_GA - 1];
    f22 = POPULATION[r22][CHROMOSOME_SIZE_INTE_GA - 1];
    f23 = POPULATION[r23][CHROMOSOME_SIZE_INTE_GA - 1];
    best_fitness = f21;
    best_fitness_idx2 = r21;

    if (f22 < best_fitness) {
        if (f23 < f22) {
            best_fitness = f23;
            best_fitness_idx2 = r23;
        }
        else {
            best_fitness = f22;
            best_fitness_idx2 = r22;
        }
    }
    else {
        if (f23 < best_fitness) {
            best_fitness = f23;
            best_fitness_idx2 = r23;
        }
    }

    // CRITICAL FIX: Validate and clean the genetic portion before copying
    for (i = 0; i < actual_chromosome_size; i++) {
        int gene1 = POPULATION[best_fitness_idx1][i];
        int gene2 = POPULATION[best_fitness_idx2][i];

        // Validate and fix invalid genes in parent 1
        if (gene1 < 0 || gene1 >= num_patients) {
            printf("Warning: Invalid gene1 value %d at position %d in parent selection. Replacing with valid value.\n", gene1, i);
            // Replace with a valid patient ID (you might want to use a more sophisticated replacement strategy)
            gene1 = i % num_patients;
        }

        // Validate and fix invalid genes in parent 2
        if (gene2 < 0 || gene2 >= num_patients) {
            printf("Warning: Invalid gene2 value %d at position %d in parent selection. Replacing with valid value.\n", gene2, i);
            // Replace with a valid patient ID
            gene2 = i % num_patients;
        }

        CROSSOVER_PARENT_STORAGE_PLACE[0][i] = gene1;
        CROSSOVER_PARENT_STORAGE_PLACE[1][i] = gene2;
    }

    // Copy the objective/fitness values as-is (they won't be used in crossover gene operations)
    for (i = actual_chromosome_size; i < CHROMOSOME_SIZE_INTE_GA; i++) {
        CROSSOVER_PARENT_STORAGE_PLACE[0][i] = POPULATION[best_fitness_idx1][i];
        CROSSOVER_PARENT_STORAGE_PLACE[1][i] = POPULATION[best_fitness_idx2][i];
    }

    /*
	printf("\nSelected Parents (after validation) in Crossover Tournament Selection:\n");
    for (i = 0; i < CHROMOSOME_SIZE_INTE_GA; i++) {
        printf("\n%d ", CROSSOVER_PARENT_STORAGE_PLACE[0][i]);
		printf("%d ", CROSSOVER_PARENT_STORAGE_PLACE[1][i]);
	}
    */
}

// Alternative function if you want to ensure chromosome integrity from the start
void validateAndFixChromosome(int* chromosome, int chromosome_length) {
    int actual_genes = chromosome_length - 5;
    bool* used = (bool*)calloc(num_patients, sizeof(bool));

    if (!used) {
        printf("Memory allocation failed in validateAndFixChromosome!\n");
        return;
    }

    // First pass: mark valid genes as used
    for (int i = 0; i < actual_genes; i++) {
        if (chromosome[i] >= 0 && chromosome[i] < num_patients) {
            used[chromosome[i]] = true;
        }
    }

    // Second pass: fix invalid genes
    int next_available = 0;
    for (int i = 0; i < actual_genes; i++) {
        if (chromosome[i] < 0 || chromosome[i] >= num_patients ||
            (i > 0 && used[chromosome[i]])) { // Also handle duplicates

            // Find next available gene
            while (next_available < num_patients && used[next_available]) {
                next_available++;
            }

            if (next_available < num_patients) {
                printf("Fixing invalid/duplicate gene at position %d: %d -> %d\n",
                    i, chromosome[i], next_available);
                chromosome[i] = next_available;
                used[next_available] = true;
            }
            else {
                // Fallback: use modulo if we run out of available genes
                chromosome[i] = i % num_patients;
            }
        }
    }

    free(used);
}

// Enhanced tournament selection with chromosome validation
void crossoverTournamentSelectionInteGAWithValidation(void)
{
    // select 2 parents using Tournament Selection method
    int r11, r12, r13, r21, r22, r23, f11, f12, f13, f21, f22, f23;
    int best_fitness, best_fitness_idx1, best_fitness_idx2;

    // select first parent
    do {
        r11 = rand() % (POP_SIZE);
        r12 = rand() % (POP_SIZE);
        r13 = rand() % (POP_SIZE);
    } while (r11 == r12 || r12 == r13 || r11 == r13);

    // select the chromosome1 with the best fitness
    f11 = POPULATION[r11][CHROMOSOME_SIZE_INTE_GA - 1];
    f12 = POPULATION[r12][CHROMOSOME_SIZE_INTE_GA - 1];
    f13 = POPULATION[r13][CHROMOSOME_SIZE_INTE_GA - 1];
    best_fitness = f11;
    best_fitness_idx1 = r11;

    if (f12 < best_fitness) {
        if (f13 < f12) {
            best_fitness = f13;
            best_fitness_idx1 = r13;
        }
        else {
            best_fitness = f12;
            best_fitness_idx1 = r12;
        }
    }
    else {
        if (f13 < best_fitness) {
            best_fitness = f13;
            best_fitness_idx1 = r13;
        }
    }

    // select second parent
    do {
        r21 = rand() % (POP_SIZE);
        r22 = rand() % (POP_SIZE);
        r23 = rand() % (POP_SIZE);
    } while (r21 == r22 || r22 == r23 || r21 == r23 ||
        r11 == r21 || r11 == r22 || r11 == r23 ||
        r12 == r21 || r12 == r22 || r12 == r23 ||
        r13 == r21 || r13 == r22 || r13 == r23);

    // select the chromosome2 with the best fitness
    f21 = POPULATION[r21][CHROMOSOME_SIZE_INTE_GA - 1];
    f22 = POPULATION[r22][CHROMOSOME_SIZE_INTE_GA - 1];
    f23 = POPULATION[r23][CHROMOSOME_SIZE_INTE_GA - 1];
    best_fitness = f21;
    best_fitness_idx2 = r21;

    if (f22 < best_fitness) {
        if (f23 < f22) {
            best_fitness = f23;
            best_fitness_idx2 = r23;
        }
        else {
            best_fitness = f22;
            best_fitness_idx2 = r22;
        }
    }
    else {
        if (f23 < best_fitness) {
            best_fitness = f23;
            best_fitness_idx2 = r23;
        }
    }

    // Copy selected parents
    memcpy(CROSSOVER_PARENT_STORAGE_PLACE[0], POPULATION[best_fitness_idx1], (CHROMOSOME_SIZE_INTE_GA) * sizeof(int));
    memcpy(CROSSOVER_PARENT_STORAGE_PLACE[1], POPULATION[best_fitness_idx2], (CHROMOSOME_SIZE_INTE_GA) * sizeof(int));

    // Validate and fix the chromosomes before crossover
    validateAndFixChromosome(CROSSOVER_PARENT_STORAGE_PLACE[0], CHROMOSOME_SIZE_INTE_GA);
    validateAndFixChromosome(CROSSOVER_PARENT_STORAGE_PLACE[1], CHROMOSOME_SIZE_INTE_GA);
}

void mutationTournamentSelectionInteGA(void)
{   // select 2 parents using Tournament Selection method
    int r1, r2, r3, f1, f2, f3;
    int best_fitness, best_fitness_idx;

    // select first parent
    do {
        r1 = rand() % (POP_SIZE);
        r2 = rand() % (POP_SIZE);
        r3 = rand() % (POP_SIZE);
    } while (r1 == r2 || r2 == r3 || r1 == r3);

    // select a chromosome with the best fitness
    f1 = POPULATION[r1][CHROMOSOME_SIZE_INTE_GA - 1];
    f2 = POPULATION[r2][CHROMOSOME_SIZE_INTE_GA - 1];
    f3 = POPULATION[r3][CHROMOSOME_SIZE_INTE_GA - 1];
    best_fitness = f1;
    best_fitness_idx = r1;

    if (f2 < best_fitness) {
        if (f3 < f2) {
            best_fitness = f3;
            best_fitness_idx = r3;
        }
        else {
            best_fitness = f2;
            best_fitness_idx = r2;
        }
    }
    else
        if (f3 < best_fitness) {
            best_fitness = f3;
            best_fitness_idx = r3;
        }

    memcpy(MUTATE_PARENT_STORAGE_PLACE, POPULATION[best_fitness_idx], (CHROMOSOME_SIZE_INTE_GA) * sizeof(int));
}

bool crossoverElitismInteGA(char type)
{
    int i, worst_fitness_chromosome_index = 0, second_worst_fitness_chromosome_index = 0, number;
    bool replaced = false;
    if (type == 'p') number = num_patients;
    else
        if (type == 'm') number = mandatory_count;
        else {
            printf("\nWrong type given: %c", type);
            printf("function: crossoverElitismInteGA");
            exit(EXIT_FAILURE);
        }

    for (i = 1; i < POP_SIZE; ++i)
        if (POPULATION[i][CHROMOSOME_SIZE_INTE_GA - 1] > POPULATION[worst_fitness_chromosome_index][CHROMOSOME_SIZE_INTE_GA - 1])
            worst_fitness_chromosome_index = i;

    for (i = 1; i < POP_SIZE; ++i)
        if (POPULATION[i][CHROMOSOME_SIZE_INTE_GA - 1] > POPULATION[second_worst_fitness_chromosome_index][CHROMOSOME_SIZE_INTE_GA - 1] &&
            i != worst_fitness_chromosome_index)
            second_worst_fitness_chromosome_index = i;

    // With the worst chromosome

    // offspring 0 with the worst chromosome
    if (CROSSOVER_OFFSPRING_STORAGE_PLACE[0][number]) {
        if (CROSSOVER_OFFSPRING_STORAGE_PLACE[0][CHROMOSOME_SIZE_INTE_GA - 1] < POPULATION[worst_fitness_chromosome_index][CHROMOSOME_SIZE_INTE_GA - 1]) {
            memcpy(POPULATION[worst_fitness_chromosome_index], CROSSOVER_OFFSPRING_STORAGE_PLACE[0], CHROMOSOME_SIZE_INTE_GA * sizeof(int));
            replaced = true;
        }
        else
            if (!POPULATION[worst_fitness_chromosome_index][number] ||
                CROSSOVER_OFFSPRING_STORAGE_PLACE[0][number] < POPULATION[worst_fitness_chromosome_index][number]) {
                memcpy(POPULATION[worst_fitness_chromosome_index], CROSSOVER_OFFSPRING_STORAGE_PLACE[0], CHROMOSOME_SIZE_INTE_GA * sizeof(int));
                replaced = true;
            }
    }
    else
        if (CROSSOVER_OFFSPRING_STORAGE_PLACE[0][CHROMOSOME_SIZE_INTE_GA - 1] < POPULATION[worst_fitness_chromosome_index][CHROMOSOME_SIZE_INTE_GA - 1]) {
            memcpy(POPULATION[worst_fitness_chromosome_index], CROSSOVER_OFFSPRING_STORAGE_PLACE[0], CHROMOSOME_SIZE_INTE_GA * sizeof(int));
            replaced = true;
        }

    // offspring 1 with the worst chromosome
    if (CROSSOVER_OFFSPRING_STORAGE_PLACE[1][number]) {
        if (CROSSOVER_OFFSPRING_STORAGE_PLACE[1][CHROMOSOME_SIZE_INTE_GA - 1] < POPULATION[worst_fitness_chromosome_index][CHROMOSOME_SIZE_INTE_GA - 1]) {
            memcpy(POPULATION[worst_fitness_chromosome_index], CROSSOVER_OFFSPRING_STORAGE_PLACE[1], CHROMOSOME_SIZE_INTE_GA * sizeof(int));
            replaced = true;
        }
        else
            if (!POPULATION[worst_fitness_chromosome_index][number] ||
                CROSSOVER_OFFSPRING_STORAGE_PLACE[1][number] < POPULATION[worst_fitness_chromosome_index][number]) {
                memcpy(POPULATION[worst_fitness_chromosome_index], CROSSOVER_OFFSPRING_STORAGE_PLACE[1], CHROMOSOME_SIZE_INTE_GA * sizeof(int));
                replaced = true;
            }
    }
    else
        if (CROSSOVER_OFFSPRING_STORAGE_PLACE[1][CHROMOSOME_SIZE_INTE_GA - 1] < POPULATION[worst_fitness_chromosome_index][CHROMOSOME_SIZE_INTE_GA - 1]) {
            memcpy(POPULATION[worst_fitness_chromosome_index], CROSSOVER_OFFSPRING_STORAGE_PLACE[1], CHROMOSOME_SIZE_INTE_GA * sizeof(int));
            replaced = true;
        }
    // With second worst chromosome

    // offspring 0 with the second worst chromosome
    if (CROSSOVER_OFFSPRING_STORAGE_PLACE[0][number]) {
        if (CROSSOVER_OFFSPRING_STORAGE_PLACE[0][CHROMOSOME_SIZE_INTE_GA - 1] < POPULATION[second_worst_fitness_chromosome_index][CHROMOSOME_SIZE_INTE_GA - 1]) {
            memcpy(POPULATION[second_worst_fitness_chromosome_index], CROSSOVER_OFFSPRING_STORAGE_PLACE[0], CHROMOSOME_SIZE_INTE_GA * sizeof(int));
            replaced = true;
        }
        else
            if (!POPULATION[second_worst_fitness_chromosome_index][number] ||
                CROSSOVER_OFFSPRING_STORAGE_PLACE[0][number] < POPULATION[second_worst_fitness_chromosome_index][number]) {
                memcpy(POPULATION[second_worst_fitness_chromosome_index], CROSSOVER_OFFSPRING_STORAGE_PLACE[0], CHROMOSOME_SIZE_INTE_GA * sizeof(int));
                replaced = true;
            }
    }
    else
        if (CROSSOVER_OFFSPRING_STORAGE_PLACE[0][CHROMOSOME_SIZE_INTE_GA - 1] < POPULATION[second_worst_fitness_chromosome_index][CHROMOSOME_SIZE_INTE_GA - 1]) {
            memcpy(POPULATION[second_worst_fitness_chromosome_index], CROSSOVER_OFFSPRING_STORAGE_PLACE[0], CHROMOSOME_SIZE_INTE_GA * sizeof(int));
            replaced = true;
        }

    // offspring 1 with the second worst chromosome
    if (CROSSOVER_OFFSPRING_STORAGE_PLACE[1][number]) {
        if (CROSSOVER_OFFSPRING_STORAGE_PLACE[1][CHROMOSOME_SIZE_INTE_GA - 1] < POPULATION[second_worst_fitness_chromosome_index][CHROMOSOME_SIZE_INTE_GA - 1]) {
            memcpy(POPULATION[second_worst_fitness_chromosome_index], CROSSOVER_OFFSPRING_STORAGE_PLACE[1], CHROMOSOME_SIZE_INTE_GA * sizeof(int));
            replaced = true;
        }
        else
            if (!POPULATION[second_worst_fitness_chromosome_index][number] ||
                CROSSOVER_OFFSPRING_STORAGE_PLACE[1][number] < POPULATION[second_worst_fitness_chromosome_index][number]) {
                memcpy(POPULATION[second_worst_fitness_chromosome_index], CROSSOVER_OFFSPRING_STORAGE_PLACE[1], CHROMOSOME_SIZE_INTE_GA * sizeof(int));
                replaced = true;
            }
    }
    else
        if (CROSSOVER_OFFSPRING_STORAGE_PLACE[1][CHROMOSOME_SIZE_INTE_GA - 1] < POPULATION[second_worst_fitness_chromosome_index][CHROMOSOME_SIZE_INTE_GA - 1]) {
            memcpy(POPULATION[second_worst_fitness_chromosome_index], CROSSOVER_OFFSPRING_STORAGE_PLACE[1], CHROMOSOME_SIZE_INTE_GA * sizeof(int));
            replaced = true;
        }
    return replaced;
}

bool mutationElitismInteGA(char type)
{
    int i, worst_fitness_chromosome_index = 0, number;
    bool replaced = false;
    if (type == 'p') number = num_patients;
    else
        if (type == 'm') number = mandatory_count;
        else {
            printf("\nWrong type given: %c", type);
            printf("function: mutationElitismInteGA");
            exit(EXIT_FAILURE);
        }

    for (i = 1; i < POP_SIZE; ++i)
        if (POPULATION[i][CHROMOSOME_SIZE_INTE_GA - 1] > POPULATION[worst_fitness_chromosome_index][CHROMOSOME_SIZE_INTE_GA - 1])
            worst_fitness_chromosome_index = i;

    if (MUTATED_OFFSPRING_STORAGE_PLACE[number]) {
        if (MUTATED_OFFSPRING_STORAGE_PLACE[CHROMOSOME_SIZE_INTE_GA - 1] < POPULATION[worst_fitness_chromosome_index][CHROMOSOME_SIZE_INTE_GA - 1]) {
            memcpy(POPULATION[worst_fitness_chromosome_index], MUTATED_OFFSPRING_STORAGE_PLACE, CHROMOSOME_SIZE_INTE_GA * sizeof(int));
            replaced = true;
        }
        else
            if (!POPULATION[worst_fitness_chromosome_index][number] ||
                MUTATED_OFFSPRING_STORAGE_PLACE[number] < POPULATION[worst_fitness_chromosome_index][number]) {
                memcpy(POPULATION[worst_fitness_chromosome_index], MUTATED_OFFSPRING_STORAGE_PLACE, CHROMOSOME_SIZE_INTE_GA * sizeof(int));
                replaced = true;
            }
    }
    else
        if (MUTATED_OFFSPRING_STORAGE_PLACE[CHROMOSOME_SIZE_INTE_GA - 1] < POPULATION[worst_fitness_chromosome_index][CHROMOSOME_SIZE_INTE_GA - 1]) {
            memcpy(POPULATION[worst_fitness_chromosome_index], MUTATED_OFFSPRING_STORAGE_PLACE, CHROMOSOME_SIZE_INTE_GA * sizeof(int));
            replaced = true;
        }

    return replaced;
}

//---------------------------------------------------------ABOVE: GENETIC ALGORITHM-------------------------------------------------------------

/*.........................
1. room_requ - #rooms X #shifts
2. room_schedule - #room X #days X #patient (in that room + on that day)
3. dm_nurse_availibility - #shifts X #nurses (matrix of struct pointers)
.........................*/

int nurseAllocationInteGA(void)
{
    int r_id, r_index, fitness = 0, p_index, sh, s, rand_nurse, unattended_shifts = 0, j, iter, most_suitable_shift;
    Nurses* nurse = NULL;
    int* new_rooms = NULL, day, temp, i, * new_nurses_p = NULL, * new_nurses = NULL, * new_nurses_occ = NULL;
    bool flag = false;
    int** track_room_shift = (int**)calloc(num_rooms, sizeof(int*));
    if (!track_room_shift) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    for (i = 0; i < num_rooms; i++) {
        track_room_shift[i] = (int*)calloc(days * 3, sizeof(int));
        if (!track_room_shift[i]) {
            perror("Memory allocation failed");
            exit(EXIT_FAILURE);
        }
        for (j = 0; j < days * 3; ++j) track_room_shift[i][j] = -1;
    }

    for (r_index = 0; r_index < num_rooms; ++r_index) {
        r_id = room[r_index].id;
        for (sh = 0; sh < days * 3; ++sh) {
            if (room[r_index].num_patients_info[sh / 3] == 0) continue; // no requirement for this shift
            if (track_room_shift[r_index][sh] != -1) continue;
            iter = 0;
            most_suitable_shift = -1;
            // first checking the room if any nurse is available - increasing continuity of care
            if (room[r_index].length_of_nurses_allotted) {
                if (room[r_index].length_of_nurses_allotted == 0) {
                    most_suitable_shift = -1;
                }
                else {
                    // existing do-while loop
                    do {
                        if (++iter > (num_nurses * 2)) break; // running this randomization for num_nurses*2 times
                        rand_nurse = rand() % (room[r_index].length_of_nurses_allotted);
                        nurse = &nurses[room[r_index].nurses_allotted[rand_nurse]];
                        if ((nurse->skill_level + NURSE_SKILL_LEVEL_ALLOWANCE) < rooms_requirement[r_index][sh].max_skill_req)
                            continue;
                        for (s = 0; s < nurse->num_shifts; ++s) {
                            if ((nurse->shift[s].day * 3 + nurse->shift[s].shift_time) != sh) continue;
                            if ((nurse->shift[s].load_left + NURSE_MAX_LOAD_ALLOWANCE) < rooms_requirement[r_index][sh].load_sum)
                                break;
                            most_suitable_shift = s;
                            break;
                        }
                    } while (most_suitable_shift == -1);
                }
            }

            // checking in the shift sh for nurses in case there's no suitable nurse already assigned to that room
            if (most_suitable_shift == -1) {
                iter = 0;
                if (most_suitable_shift == -1) {
                    if (current_size_dm_nurse[sh] == 0) {
                        ++unattended_shifts;
                        continue;
                    }
                    // existing do-while loop
                    do {
                        if (++iter > num_nurses * 2) break; // running this randomization for num_nurses*2 times
                        rand_nurse = rand() % (current_size_dm_nurse[sh]);
                        nurse = dm_nurses_availability[sh][rand_nurse];
                        if ((nurse->skill_level + NURSE_SKILL_LEVEL_ALLOWANCE) < rooms_requirement[r_index][sh].max_skill_req) continue;
                        for (s = 0; s < nurse->num_shifts; ++s) {
                            if ((nurse->shift[s].day * 3 + nurse->shift[s].shift_time) != sh) continue;
                            if ((nurse->shift[s].load_left + NURSE_MAX_LOAD_ALLOWANCE) < rooms_requirement[r_index][sh].load_sum) break;
                            most_suitable_shift = s;
                            break;
                        }
                    } while (most_suitable_shift == -1);
                }
            }

            if (most_suitable_shift == -1) { ++unattended_shifts; continue; }

            track_room_shift[r_index][sh] = nurse->id;
            // Allocate room to nurse's shift
            if (!nurse->shift[most_suitable_shift].num_rooms) new_rooms = calloc(1, sizeof(int));
            else new_rooms = realloc(nurse->shift[most_suitable_shift].rooms, (nurse->shift[most_suitable_shift].num_rooms + 1) * sizeof(int));
            if (!new_rooms) ASSERT(0, "Memory Allocation Error!");
            nurse->shift[most_suitable_shift].rooms = new_rooms;
            nurse->shift[most_suitable_shift].rooms[nurse->shift[most_suitable_shift].num_rooms++] = r_id;
            nurse->shift[most_suitable_shift].load_left -= rooms_requirement[r_index][sh].load_sum;
            if (nurse->shift[most_suitable_shift].load_left < 0) nurse->shift[most_suitable_shift].load_left = 0;

            // Allocate nurse to room
            if (!room[r_index].length_of_nurses_allotted) new_nurses = calloc(1, sizeof(int));
            else new_nurses = realloc(room[r_index].nurses_allotted, (room[r_index].length_of_nurses_allotted + 1) * sizeof(int));
            if (!new_nurses) ASSERT(0, "Memory Allocation Error!");
            room[r_index].nurses_allotted = new_nurses;
            room[r_index].nurses_allotted[room[r_index].length_of_nurses_allotted++] = nurse->id;

            // Update patients and occupants
            day = sh / 3;
            for (p_index = 0; p_index < size_of_room_schedule[r_index][day]; ++p_index) {
                char* entry = room_schedule[r_index][day][p_index];
                if (*entry == 'p') {
                    int id = str2int(entry);
                    if (!patients[id].num_nurses_allotted) new_nurses_p = calloc(1, sizeof(int));
                    else new_nurses_p = realloc(patients[id].nurses_allotted, (patients[id].num_nurses_allotted + 1) * sizeof(int));
                    if (!new_nurses_p) ASSERT(0, "Memory Allocation Error!");
                    patients[id].nurses_allotted = new_nurses_p;
                    patients[id].nurses_allotted[patients[id].num_nurses_allotted++] = nurse->id;
                }
                else if (*entry == 'a') {
                    int occ = str2int(entry);
                    if (!occupants[occ].num_nurses_allotted) new_nurses_occ = calloc(1, sizeof(int));
                    else new_nurses_occ = realloc(occupants[occ].nurses_allotted, (occupants[occ].num_nurses_allotted + 1) * sizeof(int));
                    if (!new_nurses_occ) ASSERT(0, "Memory Allocation Error!");
                    occupants[occ].nurses_allotted = new_nurses_occ;
                    occupants[occ].nurses_allotted[occupants[occ].num_nurses_allotted++] = nurse->id;
                }
            }
        }
    }

    temp = unattended_shifts;
    for (i = 0, unattended_shifts = 0; i < num_rooms; ++i) for (j = 0; j < days * 3; ++j)
        if (track_room_shift[i][j] == -1 && room[i].num_patients_info[j / 3]) unattended_shifts++;
    if (temp != unattended_shifts) ASSERT(0, "Kuch gadbad hai re baaba - in calculation of unattended_shifts");
    for (int i = 0; i < num_rooms; i++) free(track_room_shift[i]);
    free(track_room_shift);
    track_room_shift = NULL; // Avoid dangling pointer
    return unattended_shifts;
}

void generateNewChromosomeInteGA(int chromo_num)
{
    int j, r1, r2;

    // copy all the genes from (chrmo_num-1)th chrmosome to (chromo_num)th chromosome
    for (j = 0; j < CHROMOSOME_SIZE_INTE_GA; ++j) POPULATION[chromo_num][j] = POPULATION[chromo_num - 1][j];

    for (j = 0; j < CHROMOSOME_SIZE_INTE_GA; ++j) {
        do {
            if (!mandatory_count) {
				printf("\nMandatory count is 0. Cannot perform mutation.");
				exit(EXIT_FAILURE);
            }
            r1 = rand() % mandatory_count;
            r2 = rand() % mandatory_count;
        } while (r1 == r2);

        POPULATION[chromo_num][r1] += POPULATION[chromo_num][r2];
        POPULATION[chromo_num][r2] = POPULATION[chromo_num][r1] - POPULATION[chromo_num][r2];
        POPULATION[chromo_num][r1] -= POPULATION[chromo_num][r2];
    }
}

void generatePopulationInteGA(void)
{
    int i, count;
    for (i = 0, count = 0; i < num_patients; ++i) if (patients[i].mandatory) POPULATION[0][count++] = patients[i].id;
    printf("\nNumber of mandatory patients (printing in generatePopulationInteGA() function) is: %d", count);
    for (i = 0; i < 5; ++i) POPULATION[0][mandatory_count + i] = -1;
    for (i = 1; i < POP_SIZE; ++i) generateNewChromosomeInteGA(i);
}

void initDataStructuresInteGA(void)
{
    int i, size = CHROMOSOME_SIZE_INTE_GA, j;

    MUTATED_OFFSPRING_STORAGE_PLACE = (int*)calloc(size, sizeof(int));
    ASSERT(MUTATED_OFFSPRING_STORAGE_PLACE, "Dynamic Memory Allocation Error for MUTATED_OFFSPRING_STORAGE_PLACE");

    MUTATE_PARENT_STORAGE_PLACE = (int*)calloc(size, sizeof(int));
    ASSERT(MUTATE_PARENT_STORAGE_PLACE, "Dynamic Memory Allocation Error for MUTATE_PARENT_STORAGE_PLACE");

    G_BEST = (int*)calloc(size, sizeof(int));
    ASSERT(G_BEST, "Dynamic Memory Allocation Error for G_BEST");

    CROSSOVER_OFFSPRING_STORAGE_PLACE = (int**)calloc(2, sizeof(int*));
    ASSERT(CROSSOVER_OFFSPRING_STORAGE_PLACE, "Dynamic Memory Allocation Error for CROSSOVER_OFFSPRING_STORAGE_PLACE");

    for (i = 0; i < 2; ++i) {
        CROSSOVER_OFFSPRING_STORAGE_PLACE[i] = (int*)calloc(size, sizeof(int));
        ASSERT(CROSSOVER_OFFSPRING_STORAGE_PLACE[i], "Dynamic Memory Allocation Error for CROSSOVER_OFFSPRING_STORAGE_PLACE[i]");
    }

    CROSSOVER_PARENT_STORAGE_PLACE = (int**)calloc(2, sizeof(int*));
    ASSERT(CROSSOVER_PARENT_STORAGE_PLACE, "Dynamic Memory Allocation Error for CROSSOVER_PARENT_STORAGE_PLACE");

    for (i = 0; i < 2; ++i) {
        CROSSOVER_PARENT_STORAGE_PLACE[i] = (int*)calloc(size, sizeof(int));
        ASSERT(CROSSOVER_PARENT_STORAGE_PLACE[i], "Dynamic Memory Allocation Error for CROSSOVER_PARENT_STORAGE_PLACE[i]");
    }

    POPULATION = (int**)calloc(POP_SIZE, sizeof(int*));
    ASSERT(POPULATION, "Dynamic Memory Allocation Error for POPULATION");

    for (i = 0; i < POP_SIZE; ++i) {
        POPULATION[i] = (int*)calloc(size, sizeof(int));
        ASSERT(POPULATION[i], "Dynamic Memory Allocation Error for a CHROMOSOME");
        for (j = 0; j < 5; ++j) POPULATION[i][mandatory_count + j] = -1;
    }
}

void freeDataStructuresInteGA(void)
{
    int i;
    free(MUTATED_OFFSPRING_STORAGE_PLACE);
    free(MUTATE_PARENT_STORAGE_PLACE);
    MUTATED_OFFSPRING_STORAGE_PLACE = NULL; // Avoid dangling pointer
    MUTATE_PARENT_STORAGE_PLACE = NULL; // Avoid dangling pointer

    for (i = 0; i < 2; ++i) if (CROSSOVER_OFFSPRING_STORAGE_PLACE[i]) free(CROSSOVER_OFFSPRING_STORAGE_PLACE[i]);
    free(CROSSOVER_OFFSPRING_STORAGE_PLACE);
    CROSSOVER_OFFSPRING_STORAGE_PLACE = NULL; // Avoid dangling pointer

    for (i = 0; i < 2; ++i) if (CROSSOVER_PARENT_STORAGE_PLACE[i]) free(CROSSOVER_PARENT_STORAGE_PLACE[i]);
    free(CROSSOVER_PARENT_STORAGE_PLACE);
    CROSSOVER_PARENT_STORAGE_PLACE = NULL; // Avoid dangling pointer

    for (i = 0; i < POP_SIZE; ++i) if (POPULATION[i]) free(POPULATION[i]);
    free(POPULATION);
    POPULATION = NULL; // Avoid dangling pointer

    if (G_BEST) free(G_BEST);
    G_BEST = NULL; // Avoid dangling pointer
}

static void resizeDataStructures(void) {
    int size = CHROMOSOME_SIZE_INTE_GA, i;
    /*
    * Resize all the Data Structures: 
    * All 3 Nurse Allocation DS
    * All the Storage DS - like MUTATED_OFFSPRING_STORAGE_PLACE, CROSSOVER_OFFSPRING_STORAGE_PLACE, etc.
    */
    MUTATED_OFFSPRING_STORAGE_PLACE = (int*)realloc(MUTATED_OFFSPRING_STORAGE_PLACE, size * sizeof(int));
    ASSERT(MUTATED_OFFSPRING_STORAGE_PLACE, "Dynamic Memory Allocation Error for MUTATED_OFFSPRING_STORAGE_PLACE");
    
    MUTATE_PARENT_STORAGE_PLACE = (int*)realloc(MUTATE_PARENT_STORAGE_PLACE, size * sizeof(int));
    ASSERT(MUTATE_PARENT_STORAGE_PLACE, "Dynamic Memory Allocation Error for MUTATE_PARENT_STORAGE_PLACE");
    
    G_BEST = (int*)realloc(G_BEST, size * sizeof(int));
    ASSERT(G_BEST, "Dynamic Memory Allocation Error for G_BEST");

    for (i = 0; i < 2; ++i) {
        CROSSOVER_OFFSPRING_STORAGE_PLACE[i] = (int*)realloc(CROSSOVER_OFFSPRING_STORAGE_PLACE[i], size * sizeof(int));
        ASSERT(CROSSOVER_OFFSPRING_STORAGE_PLACE[i], "Dynamic Memory Allocation Error for CROSSOVER_OFFSPRING_STORAGE_PLACE[i]");
    }

    for (i = 0; i < 2; ++i) {
        CROSSOVER_PARENT_STORAGE_PLACE[i] = (int*)realloc(CROSSOVER_PARENT_STORAGE_PLACE[i], size * sizeof(int));
        ASSERT(CROSSOVER_PARENT_STORAGE_PLACE[i], "Dynamic Memory Allocation Error for CROSSOVER_PARENT_STORAGE_PLACE[i]");
    }
}

static void generateSubOptionalChromosome(int chromo_num) {
    int i, j, r1, r2;

    // Copy the optional part from chromosome 0
    for (i = mandatory_count; i < CHROMOSOME_SIZE_INTE_GA-5; ++i) POPULATION[chromo_num][i] = POPULATION[chromo_num-1][i];

    for (i = CHROMOSOME_SIZE_INTE_GA - 6; i >= (mandatory_count*2); --i) {
        do {
            r1 = mandatory_count + (int)(rand()) % optional_count;
            r2 = mandatory_count + (int)(rand()) % optional_count;
        } while (r1 == r2);
        // printf("\nrandom numbers are: %d and %d", r1, r2);
        // Swap POPULATION[chromo_num][i] and POPULATION[chromo_num][j]
        int temp = POPULATION[chromo_num][r1];
        POPULATION[chromo_num][r1] = POPULATION[chromo_num][r2];
        POPULATION[chromo_num][r2] = temp;
    }
}

static void appendOptionals(void) {
    /*
    * Transfer the poluation of MANDATORY_POPULATION to POPULATION
    * Append Optional patients at the end of each solution in the population.
    */
    int i;
    for (i = 0; i < POP_SIZE; ++i) {
		if (POPULATION[i]) POPULATION[i] = (int*)realloc(POPULATION[i], CHROMOSOME_SIZE_INTE_GA * sizeof(int));
		ASSERT(POPULATION[i], "Dynamic Memory Allocation Error for POPULATION[i]");
        memcpy(POPULATION[i], MANDATORY_POPULATION[i], mandatory_count * sizeof(int));
    }

    for (i = 0; i < optional_count; ++i) POPULATION[0][mandatory_count + i] = optional_patients[i]->id;
    for (i = 1; i < POP_SIZE; ++i) generateSubOptionalChromosome(i);
}

int main(int argc, char* argv[]) {
    clock_t start, end, algo_start, algo_end;
    double cpu_time_used, algo_cpu_time_used;
    char* input_path = NULL;
    char* output_path = NULL;
    char type;
    int j, count, N_MANDATORY_GA_RUNS = 10, i, number;
    int CONVERGENCE_STOPPING_CRITERION = 1000;  // Default value

    start = clock();

    // =======================================================
    // PARSE COMMAND LINE ARGUMENTS
    // =======================================================
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input_json> <output_json> [options]\n", argv[0]);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  -P <size>     Population size (default: 8000)\n");
        fprintf(stderr, "  -G <gens>     Generations (default: 120000)\n");
        fprintf(stderr, "  -lb <bound>   Lower bound (default: 0.2)\n");
        fprintf(stderr, "  -ub <bound>   Upper bound (default: 0.9)\n");
        fprintf(stderr, "  -pc <prob>    Crossover probability (default: 0.1)\n");
        fprintf(stderr, "  -pm <prob>    Mutation probability (default: 0.05)\n");
        fprintf(stderr, "  -T <thresh>   Normalization threshold (default: 1000)\n");
        return EXIT_FAILURE;
    }

    // First two arguments are required: input and output files
    input_path = argv[1];
    output_path = argv[2];

    printf("\n=======================================================\n");
    printf("  INTEGRATED GENETIC ALGORITHM - NURSE SCHEDULING\n");
    printf("=======================================================\n");
    printf("Input file:  %s\n", input_path);
    printf("Output file: %s\n", output_path);

    // Parse optional arguments
    for (i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-P") == 0 && i + 1 < argc) {
            POP_SIZE = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-G") == 0 && i + 1 < argc) {
            N_GEN = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-lb") == 0 && i + 1 < argc) {
            lower_bound = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "-ub") == 0 && i + 1 < argc) {
            upper_bound = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "-pc") == 0 && i + 1 < argc) {
            p_c = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "-pm") == 0 && i + 1 < argc) {
            p_m = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "-T") == 0 && i + 1 < argc) {
            THREASHOLD = atof(argv[++i]);
        }
        else if (strcmp(argv[i], "-C") == 0 && i + 1 < argc) {
            CONVERGENCE_STOPPING_CRITERION = atoi(argv[++i]);
        }
        else {
            fprintf(stderr, "Warning: Unknown argument: %s\n", argv[i]);
        }
    }

    printf("\nGA Parameters:\n");
    printf("  Population size:      %d\n", POP_SIZE);
    printf("  Generations:          %d\n", N_GEN);
    printf("  Mandatory GA runs:    %d\n", N_GEN_MANDATORY);
    printf("  Crossover prob:       %.3f\n", p_c);
    printf("  Mutation prob:        %.3f\n", p_m);
    printf("  Lower bound (alpha):  %.2f\n", lower_bound);
    printf("  Upper bound (alpha):  %.2f\n", upper_bound);
    printf("  Threshold:            %.0f\n", THREASHOLD);
    printf("=======================================================\n\n");

    // =======================================================
    // PARSE INPUT JSON
    // =======================================================
    printf("Parsing input file...\n");
    parse_json(input_path);
    srand((unsigned int)time(NULL));

    // =======================================================
    // PHASE 1: MANDATORY PATIENTS ONLY
    // =======================================================
    type = 'm';
    number = mandatory_count;
    size = mandatory_count + 5;
    CHROMOSOME_SIZE_INTE_GA = size;

    printf("\n=======================================================\n");
    printf("  PHASE 1: MANDATORY PATIENTS (%d patients)\n", mandatory_count);
    printf("=======================================================\n");

    // Allocate mandatory population storage
    MANDATORY_POPULATION = (int**)calloc(POP_SIZE, sizeof(int*));
    ASSERT(MANDATORY_POPULATION, "Dynamic Memory Allocation Error for MANDATORY_POPULATION");

    for (i = 0; i < POP_SIZE; ++i) {
        MANDATORY_POPULATION[i] = (int*)calloc(mandatory_count, sizeof(int));
        ASSERT(MANDATORY_POPULATION[i], "Dynamic Memory Allocation Error for MANDATORY_POPULATION[i]");
    }

    // Set allowances based on problem size
    if (number < 50) {
        NURSE_SKILL_LEVEL_ALLOWANCE = 1;
        NURSE_MAX_LOAD_ALLOWANCE = 1;
    }
    else if (number < 100) {
        NURSE_SKILL_LEVEL_ALLOWANCE = 3;
        NURSE_MAX_LOAD_ALLOWANCE = 3;
    }
    else if (number < 300) {
        NURSE_SKILL_LEVEL_ALLOWANCE = 3;
        NURSE_MAX_LOAD_ALLOWANCE = 5;
    }
    else if (number < 700) {
        NURSE_SKILL_LEVEL_ALLOWANCE = 3;
        NURSE_MAX_LOAD_ALLOWANCE = 5;
    }
    else {
        NURSE_SKILL_LEVEL_ALLOWANCE = 4;
        NURSE_MAX_LOAD_ALLOWANCE = 7;
    }

    printf("Nurse allowances: skill=%d, load=%d\n",
        NURSE_SKILL_LEVEL_ALLOWANCE, NURSE_MAX_LOAD_ALLOWANCE);

    // Initialize data structures
    printf("Initializing data structures...\n");
    allocate_surgeon_day_theatre_count();
    initDataStructuresInteGA();
    initialize_room_gender_map(&room_gender_map);
    initialize_room_shift_nurse();
    populate_room_gender_map(&room_gender_map);
    create_dm_nurses_availability();

    for (i = 0; i < 3 * days; i++) {
        if (current_size_dm_nurse[i] > 1) {
            quicksort(dm_nurses_availability[i], 0, current_size_dm_nurse[i] - 1);
        }
    }

    algo_start = clock();

    // =======================================================
    // Run GA to get violation-free mandatory solutions
    // =======================================================
    printf("\nRunning mandatory-only GA (up to %d runs)...\n", N_MANDATORY_GA_RUNS);

    for (i = 0, count = 0; i < N_MANDATORY_GA_RUNS; i++) {
        if (count >= POP_SIZE) break;

        printf("  Mandatory GA run %d/%d...\n", i + 1, N_MANDATORY_GA_RUNS);
        applyIntegratedGA(type);

        // Collect violation-free solutions
        for (j = 0; j < POP_SIZE; ++j) {
            if (count >= POP_SIZE) break;

            if (POPULATION[j][mandatory_count] == 0) {  // No violations
                memcpy(MANDATORY_POPULATION[count], POPULATION[j], mandatory_count * sizeof(int));
                count++;
                printf("    ✓ Found violation-free solution %d/%d\n", count, POP_SIZE);
            }
        }
    }

    if (count < POP_SIZE) {
        fprintf(stderr, "\nWARNING: Only found %d/%d violation-free mandatory solutions\n", count, POP_SIZE);
        fprintf(stderr, "Filling remaining slots with best available solutions...\n");

        // Fill remaining slots with best available (even if they have violations)
        for (j = 0; j < POP_SIZE && count < POP_SIZE; ++j) {
            int already_added = 0;
            for (int k = 0; k < count; ++k) {
                if (memcmp(MANDATORY_POPULATION[k], POPULATION[j], mandatory_count * sizeof(int)) == 0) {
                    already_added = 1;
                    break;
                }
            }
            if (!already_added) {
                memcpy(MANDATORY_POPULATION[count], POPULATION[j], mandatory_count * sizeof(int));
                count++;
            }
        }
    }

    printf("Completed mandatory phase with %d solutions after %d GA runs\n", count, i);

    // =======================================================
    // PHASE 2: ALL PATIENTS (MANDATORY + OPTIONAL)
    // =======================================================
    printf("\n=======================================================\n");
    printf("  PHASE 2: ALL PATIENTS (%d mandatory + %d optional)\n",
        mandatory_count, optional_count);
    printf("=======================================================\n");

    type = 'p';
    size = num_patients + 5;
    CHROMOSOME_SIZE_INTE_GA = size;

    printf("Appending optional patients...\n");
    appendOptionals();
    resizeDataStructures();

    // Free mandatory population storage
    for (i = 0; i < POP_SIZE; ++i) {
        free(MANDATORY_POPULATION[i]);
    }
    free(MANDATORY_POPULATION);
    MANDATORY_POPULATION = NULL;

    // Clean up temporary structures
    if (room_schedule) free_3d_array();
    if (rooms_requirement) cleanup_rooms_req(num_rooms);
    room_schedule = NULL;
    rooms_requirement = NULL;

    printf("Running full GA with all patients...\n");
    applyIntegratedGA(type);

    algo_end = clock();

    // =======================================================
    // FINAL EVALUATION
    // =======================================================
    printf("\n=======================================================\n");
    printf("  FINAL EVALUATION\n");
    printf("=======================================================\n");

    evaluateViolationsAndCost(G_BEST, 'p');

    printf("\nFinal Results:\n");
    printf("  Violations:      %d\n", G_BEST[num_patients]);
    printf("  Cost:            %d\n", G_BEST[num_patients + 1]);
    printf("  Fitness:         %d\n", G_BEST[CHROMOSOME_SIZE_INTE_GA - 1]);

    // =======================================================
    // SAVE OUTPUT JSON
    // =======================================================
    printf("\nSaving solution to: %s\n", output_path);

    // Extract directory from output_path for compatibility
    char output_dir[256] = ".";  // Default to current directory
    char* last_slash = strrchr(output_path, '/');
    if (last_slash != NULL) {
        size_t dir_len = last_slash - output_path;
        if (dir_len < sizeof(output_dir)) {
            strncpy(output_dir, output_path, dir_len);
            output_dir[dir_len] = '\0';
        }
    }
    /*
    // Extract just the filename without extension
    char base_name[256];
    const char* file_start = last_slash ? last_slash + 1 : output_path;
    const char* extension = strrchr(file_start, '.');
    size_t name_len = extension ? (size_t)(extension - file_start) : strlen(file_start);

    if (name_len < sizeof(base_name)) {
        strncpy(base_name, file_start, name_len);
        base_name[name_len] = '\0';
    }
    else {
        strncpy(base_name, file_start, sizeof(base_name) - 1);
        base_name[sizeof(base_name) - 1] = '\0';
    }
    */
    create_json_file(patients, num_patients, nurses, num_nurses, num_rooms, output_path);

    // =======================================================
    // CLEANUP
    // =======================================================
    printf("\nCleaning up memory...\n");
    free_occupants();
    free_patients();
    free_surgeons();
    free_ots();
    free_rooms();
    free_nurses();
    free_dm_nurses_availability();
    if (room_schedule) free_3d_array();
    if (rooms_requirement) cleanup_rooms_req(num_rooms);
    freeDataStructuresInteGA();
    free(weights);
    weights = NULL;

    // =======================================================
    // TIMING REPORT
    // =======================================================
    end = clock();
    cpu_time_used = ((double)(end - start)) / (CLOCKS_PER_SEC * 60.0);
    algo_cpu_time_used = ((double)(algo_end - algo_start)) / (CLOCKS_PER_SEC * 60.0);

    printf("\n=======================================================\n");
    printf("  TIMING REPORT\n");
    printf("=======================================================\n");
    printf("Total time:      %.2f minutes (%.1f seconds)\n",
        cpu_time_used, cpu_time_used * 60);
    printf("Algorithm time:  %.2f minutes (%.1f seconds)\n",
        algo_cpu_time_used, algo_cpu_time_used * 60);
    printf("Overhead:        %.2f minutes (%.1f seconds)\n",
        cpu_time_used - algo_cpu_time_used,
        (cpu_time_used - algo_cpu_time_used) * 60);
    printf("=======================================================\n");

    printf("\n✓ Program completed successfully!\n\n");

    return 0;
}
