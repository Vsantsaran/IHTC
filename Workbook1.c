#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include "cJSON.h"
#include "cJSON.c"


// Global declarations of variables
int days;
int skill_levels;
int num_occupants;
int num_patients;
int num_surgeons;
int num_ots;
int num_rooms;
int num_nurses;
int mandatory_count = 0;
int optional_count = 0;
int current_ot_index = 0;

// int is_admitted[1000] = {0}; // we're not using this array anywhere so let's comment it out
int num_gender_A_rooms, num_gender_B_rooms, num_empty_rooms;
int *current_size_dm_nurse;
int *room_gender_map;

int ***size_of_room_schedule;

// Enums for the mapped values
typedef enum {A, B} gender;
typedef enum {early, late, night} shift_types;
typedef enum {optional, mandatory} PatientType;

const char* gender_to_string(gender g) {
    switch (g) {
        case A: return "Male";
        case B: return "Female";
        default: return "Unknown";
    }
}

const char* shift_types_to_string(shift_types shift) {
    switch(shift) {
        case early: return "early";
        case late: return "late";
        case night: return "night";
        default: return "unknown";
    }
}

// Enum conversion functions
gender string_to_gender(const char *str) {
    if (strcmp(str, "A") == 0) return A;
    if (strcmp(str, "B") == 0) return B;
    return A; // Invalid value
}

shift_types string_to_shift_types(const char *str){
    if(strcmp(str, "early") == 0) return early;
    if(strcmp(str, "late") == 0) return late;
    if(strcmp(str, "night") == 0) return night;
    return early; // Invalid value
}

int str2int(char *a)
{   /* ex. We'll get a string like p01 or n010.
    We have to find the number in the string and return that number in integer form.
    */
    int number=0, i, n, num;
    char *p = a;
    p++;
    n = strlen(p);
    for (i=0; i<n; ++i){
        num = p[i] - '0';
        number += (num * pow(10, n-i-1));
    }
    return number;
}
// Struct definitions
typedef struct {
    int day;
    shift_types shift_time;
    int max_load;
    int *rooms;
    int num_rooms;
} Shifts;

typedef struct {
    int id;
    int skill_level;
    Shifts *shift;
    int num_shifts;  
} Nurses;

Nurses *nurses;
Nurses ***dm_nurses_availability;  //nurse assignment
int **max_load_updated; //nurse assignment

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


Weights *weights = NULL; // Global pointer for weights
typedef struct {
    int id;
    int length_of_stay;
    int room_id;
    char *age;
    gender gen;
    int *workload_produced;
    int *skill_level_required;
} Occupants;


Occupants *occupants = NULL; // Global pointer for occupants
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
    int *incompatible_room_ids;
    int num_incompatible_rooms;
    int *workload_produced;
    int *skill_level_required;
    int assigned_room_no;
    int assigned_ot;
    int admission_day;
} Patient;


Patient *patients;
Patient **mandatory_patients = NULL;
Patient **optional_patients = NULL;
Patient ****room_schedule;

typedef struct {
    int id;
    int *max_surgery_time;
    int *time_left; // default: max_surgery_time
    int *patients_assigned;
    int num_assigned_patients; // a fixed number - how many patients are assigned to this surgeon
    int current_size; // dynamic size of the patients_assigned array
} Surgeon;


Surgeon *surgeon;
typedef struct {
    int id;
    int *max_ot_time;
    int *time_left; // default: max_ot_time
} OTs;


OTs *ot;
typedef struct {
    int id;
    int cap;
    int num_patients_allocated;
    // int *patients_allocated;
    int occupants_cap;
    int *nurses_alloted;
    int length_of_nurses_alloted;
} Rooms;
// one idea is that make the patients_assigned array a static array and remove num_patients_assigned field from the structure.

Rooms *room;

typedef struct RoomsList {
    Rooms *pointer;
    struct RoomsList *next;
    struct RoomsList *prev;
} GenderRoom;
GenderRoom *gender_A_rooms = NULL, *gender_B_rooms = NULL, *empty_rooms = NULL;

typedef struct{
    int load_sum;
    int max_skill_req;
} Rooms_req;
Rooms_req **rooms_requirement;

typedef struct {
    int surgeon_id;
    int *assigned_patients;
    int num_assigned_patients;
    int surgery_duration_sum;
    int isNull; 
    /* added isNull in the struct cuz s_data_arr has null values as well and we need to sort that array on the basis of 
    surgery_duration_sum. if that cell is null then we cant really sort the array as that may contrive problems while selecting the pivot
    so for those null pointers we'll create this same structure and put 1 in the isNull field. this way we'll know that this pointer is 
    a null one and we can deal with it accordingly.
    */
} Surgeon_data;


// Define the HeapNode and PriorityQueue structures
typedef struct {
    int mandatory;  // Higher is better
    int delay;      // Higher is better
    int patient_id; // Lower is better
} HeapNode;

typedef struct {
    HeapNode *data; // Dynamically allocated array for heap nodes
    int current_size;       // Current number of elements in the priority queue
    int capacity;   // Maximum capacity of the priority queue
} PriorityQueue;


typedef struct LL {
    Patient *pointer;
    struct LL *next, *prev;
} Node;

// ----------------------------------------------BELOW: FUNCTION PROTOTYPES FOR PAS AND SCP--------------------------------------------

GenderRoom *appendGenderRoomNode(GenderRoom *, GenderRoom *);
GenderRoom *makeGenderRoomNode(Rooms *);
void free_s_data_arr(Surgeon_data **, int);
void make_3_room_arrays(int *);
void update_LOS_of_patients(int);
int findSuitableRoom(int, GenderRoom *);
void sort_s_data_arr(Surgeon_data **, int, Node *);
void sort_ot_data_arr(OTs **, int);
int find_max_surgeon_id(Node *);
OTs *admitOptionalFromPQ(PriorityQueue *, int, OTs **, OTs *);
OTs *admitMandatoryFromPQ(PriorityQueue *, int, OTs **, OTs *, int);
GenderRoom *deleteGenderRoomNode(GenderRoom *, GenderRoom *);
void free_3_room_arrays(void);
GenderRoom *removeAndAppendGenderRoom(int, GenderRoom *);
void update_LOS_of_patients(int);
void admit_patients(int *, PriorityQueue *);

// ----------------------------------------------ABOVE: FUNCTION PROTOTYPES FOR PAS AND SCP--------------------------------------------


// Function prototypes
void initPQ(PriorityQueue *pq, int initialCapacity);
void insertNodeInPQ(PriorityQueue *pq, HeapNode node);
HeapNode extractMaxFromPQ(PriorityQueue *pq);
HeapNode peekMaxFromPQ(PriorityQueue *pq);
int isEmptyPQ(PriorityQueue *pq);
void heapifyUp(PriorityQueue *pq, int index);
void heapifyDown(PriorityQueue *pq, int index);
int compareNodesForPQ(HeapNode a, HeapNode b);
void resizePQ(PriorityQueue *pq);
void freePQ(PriorityQueue *pq);

// Function to parse occupants
void parse_occupants(cJSON *occupants_array) {
    num_occupants = cJSON_GetArraySize(occupants_array);
    occupants = (Occupants *)calloc(num_occupants, sizeof(Occupants));
    if (!occupants) {
        printf("Memory allocation failed for occupants.\n");
        exit(1);
    }
    for (int i = 0; i < num_occupants; i++) {
        cJSON *item = cJSON_GetArrayItem(occupants_array, i);
        if (!item) continue;
        // Parse individual fields
        cJSON *id_json = cJSON_GetObjectItem(item, "id");
        cJSON *gender_json = cJSON_GetObjectItem(item, "gender");
        cJSON *age_group_json = cJSON_GetObjectItem(item, "age_group");
        cJSON *length_of_stay_json = cJSON_GetObjectItem(item, "length_of_stay");
        cJSON *workload_produced_json = cJSON_GetObjectItem(item, "workload_produced");
        cJSON *skill_level_required_json = cJSON_GetObjectItem(item, "skill_level_required");
        cJSON *room_id_json = cJSON_GetObjectItem(item, "room_id");
        // Parse ID
        if (id_json && cJSON_IsString(id_json)) {
            int id_int = str2int(id_json->valuestring);
            occupants[i].id = id_int;
        } else {
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
        } else {
            occupants[i].room_id = -1; // Default invalid room ID
        }
        // Parse Workload Produced
        if (workload_produced_json && cJSON_IsArray(workload_produced_json)) {
            int size = cJSON_GetArraySize(workload_produced_json);
            occupants[i].workload_produced = (int *)calloc(size, sizeof(int));
            if (!occupants[i].workload_produced) {
                printf("Memory allocation failed for workload_produced of occupant %d\n", i);
                exit(1);
            }
            for (int j = 0; j < size; j++) {
                occupants[i].workload_produced[j] = cJSON_GetArrayItem(workload_produced_json, j)->valueint;
            }
        } else {
            occupants[i].workload_produced = NULL;
        }
        // Parse Skill Level Required
        if (skill_level_required_json && cJSON_IsArray(skill_level_required_json)) {
            int size = cJSON_GetArraySize(skill_level_required_json);
            occupants[i].skill_level_required = (int *)calloc(size, sizeof(int));
            if (!occupants[i].skill_level_required) {
                printf("Memory allocation failed for skill_level_required of occupant %d\n", i);
                exit(1);
            }
            for (int j = 0; j < size; j++) {
                occupants[i].skill_level_required[j] = cJSON_GetArrayItem(skill_level_required_json, j)->valueint;
            }
        } else {
            occupants[i].skill_level_required = NULL;
        }
    }
}

void free_occupants() {
    for (int i = 0; i < num_occupants; i++) {
        free(occupants[i].workload_produced);
        free(occupants[i].skill_level_required);
    }
    free(occupants);
}

void assign_occupants_to_rooms(void) {
    // here we'll update the occupants_Cap filed of each room
    // method - parse the occupants array (arr of structs) and find out what's its room id and then increase the 
    // occupants_cap filed of that room
    int i, r_id;
    for (i=0; i<num_occupants; ++i) {
        r_id = occupants[i].room_id;
        room[r_id].occupants_cap++;
}
}

// Function to parse weights
void parse_weights(cJSON *weights_json) {
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


void parse_patients(cJSON *patients_array) {
    num_patients = cJSON_GetArraySize(patients_array);
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
        cJSON *item = cJSON_GetArrayItem(patients_array, i);
        if (!item) continue;
        patients[i].assigned_room_no = -1;
        patients[i].admission_day = -1;
        patients[i].assigned_ot = -1;
        // Parse individual fields of each patient
        cJSON *id_json = cJSON_GetObjectItem(item, "id");
        int id_int = str2int(id_json->valuestring);
        if (id_json && cJSON_IsString(id_json)) {
            patients[i].id = id_int;
        }
        cJSON *mandatory_json = cJSON_GetObjectItem(item, "mandatory");
        if (mandatory_json && cJSON_IsBool(mandatory_json)) {
            patients[i].mandatory = cJSON_IsTrue(mandatory_json) ? 1 : 0;
        } else {
            patients[i].mandatory = 0;
            printf("Missing or invalid mandatory status for patient.\n");
        }
        // Assign to mandatory or optional array based on the "mandatory" field
        if (patients[i].mandatory) {
            mandatory_patients[mandatory_count++] = patients+i;//.................................................................
        } else {
            optional_patients[optional_count++] = patients+i;
        }
        cJSON *gender_json = cJSON_GetObjectItem(item, "gender");
        if (gender_json && cJSON_IsString(gender_json)) {
            patients[i].gen = string_to_gender(gender_json->valuestring);
        } 
        else
            patients[i].gen = A;
		 
        cJSON *age_groups_json = cJSON_GetObjectItem(item, "age_group");
        if (age_groups_json && cJSON_IsString(age_groups_json)) {
            patients[i].age_group = strdup(age_groups_json->valuestring); // Allocate and copy the string
            if (!patients[i].age_group) {
                printf("Memory allocation failed for age_group of patient at index %d.\n", i);
                exit(1);
        }
    } else {
        patients[i].age_group = strdup("adult"); // Default value
        if (!patients[i].age_group) {
            printf("Memory allocation failed for default age_group of patient at index %d.\n", i);
            exit(1);
    }
        printf("Missing or invalid age group for the patient at index %d.\n", i);
}
        cJSON *length_of_stay_json = cJSON_GetObjectItem(item, "length_of_stay");
        if (length_of_stay_json && cJSON_IsNumber(length_of_stay_json)) {
            patients[i].length_of_stay = length_of_stay_json->valueint;
        } else {
            patients[i].length_of_stay = 0;
            printf("Missing or invalid length_of_stay for patient at index %d.\n", i);
        }
        cJSON *surgery_release_day_json = cJSON_GetObjectItem(item, "surgery_release_day");
        if (surgery_release_day_json && cJSON_IsNumber(surgery_release_day_json)) {
            patients[i].surgery_release_day = surgery_release_day_json->valueint;
        } else {
            patients[i].surgery_release_day = 0;
            printf("Missing or invalid surgery_release_day for patient at index %d.\n", i);
        }
        cJSON *surgery_duration_json = cJSON_GetObjectItem(item, "surgery_duration");
        if (surgery_duration_json && cJSON_IsNumber(surgery_duration_json)) {
            patients[i].surgery_duration = surgery_duration_json->valueint;
        } else {
            patients[i].surgery_duration = 0;
            printf("Missing or invalid surgery_duration for patient at index %d.\n", i);
        }
        if (patients[i].mandatory) {
            cJSON *surgery_due_day_json = cJSON_GetObjectItem(item, "surgery_due_day");
            if (surgery_due_day_json && cJSON_IsNumber(surgery_due_day_json)) {
                patients[i].surgery_due_day = surgery_due_day_json->valueint;
            } else {
                patients[i].surgery_due_day = -1;
                printf("Missing or invalid surgery_due_day for mandatory patient at index %d.\n", i);
            }
        } else {
            patients[i].surgery_due_day = -1;
        }
       cJSON *surgeon_id_json = cJSON_GetObjectItem(item, "surgeon_id");
        if (surgeon_id_json && cJSON_IsString(surgeon_id_json)) {
            int surgeon_id_int = str2int(surgeon_id_json->valuestring);
            patients[i].surgeon_id = surgeon_id_int;
        // printf("%d\n" , surgeon_id_int);
        }    
        cJSON *incompatible_rooms_json = cJSON_GetObjectItem(item, "incompatible_room_ids");
        if (incompatible_rooms_json && cJSON_IsArray(incompatible_rooms_json)) {
            patients[i].num_incompatible_rooms = cJSON_GetArraySize(incompatible_rooms_json);
            patients[i].incompatible_room_ids = (int*)calloc(patients[i].num_incompatible_rooms, sizeof(int));
            if (!patients[i].incompatible_room_ids) {
                printf("Memory allocation failed for incompatible_room_ids at index %d.\n", i);
                exit(1);
            }
            for (int j = 0; j < patients[i].num_incompatible_rooms; j++) {
                cJSON *room_id_json = cJSON_GetArrayItem(incompatible_rooms_json, j);
                if (cJSON_IsString(room_id_json)) {
                    patients[i].incompatible_room_ids[j] = str2int(room_id_json->valuestring);
                } else {
                    printf("Invalid room ID format for patient at index %d, room %d.\n", i, j);
                    patients[i].incompatible_room_ids[j] = -1;
                }
            }
        } else {
            patients[i].incompatible_room_ids = NULL;
            patients[i].num_incompatible_rooms = 0;
            printf("Missing or invalid incompatible_room_ids for patient at index %d.\n", i);
        }
        cJSON *workload_json = cJSON_GetObjectItem(item, "workload_produced");
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
        } else {
            patients[i].workload_produced = NULL;
            printf("Missing or invalid workload_produced for patient at index %d.\n", i);
        }
        cJSON *skills_json = cJSON_GetObjectItem(item, "skill_level_required");
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
        } else {
            patients[i].skill_level_required = NULL;
            printf("Missing or invalid skill_level_required for patient at index %d.\n", i);
        }
    }
    // Print counts for verification
    // printf("Number of mandatory patients: %d\n", mandatory_count);
    // printf("Number of optional patients: %d\n", optional_count);
}


void free_patients() {
    if (!patients) {
        return; // No need to free if patients is NULL
    }
    for (int i = 0; i < num_patients; i++) {
        // Free incompatible room IDs array
        if (patients[i].incompatible_room_ids)
            free(patients[i].incompatible_room_ids);
		 
        // Free workload array
        if (patients[i].workload_produced)
            free(patients[i].workload_produced);
		 
        // Free skill level array
        if (patients[i].skill_level_required)
            free(patients[i].skill_level_required);
		 
    }
    // Free the main patients array
    free(patients);
    patients = NULL; // Set to NULL to avoid dangling pointer issues
}

// function to parse the surgeons
void parse_surgeons(cJSON *surgeons_array) {
    num_surgeons = cJSON_GetArraySize(surgeons_array);
   // printf("%d\n" , num_surgeons);
    surgeon = (Surgeon *)calloc(num_surgeons, sizeof(Surgeon));
    if (!surgeon) {
        printf("Memory allocation failed for surgeons.\n");
        exit(1);
    }
    for (int i = 0; i < num_surgeons; i++) {
        cJSON *item = cJSON_GetArrayItem(surgeons_array, i);
        if (!item) continue;
        cJSON *id_json = cJSON_GetObjectItem(item, "id");
        if (id_json && cJSON_IsString(id_json)) {
            (surgeon)[i].id = str2int(id_json->valuestring); // Dynamically allocate and copy the string
        } 
        else {
            (surgeon)[i].id = -1;
            printf("Missing or invalid ID for surgeon at index %d.\n", i);
        }
        cJSON *max_surgery_time_json = cJSON_GetObjectItem(item, "max_surgery_time");
        if (max_surgery_time_json && cJSON_IsArray(max_surgery_time_json)) {
            int size = cJSON_GetArraySize(max_surgery_time_json);
            (surgeon)[i].max_surgery_time = (int *)calloc(size, sizeof(int));
            (surgeon)[i].time_left = (int *)calloc(size, sizeof(int));
            if (!(surgeon)[i].max_surgery_time) {
                printf("Memory allocation failed for max_surgery_time at index %d.\n", i);
                exit(1);
            }
            for (int j = 0; j < size; j++) {
                (surgeon)[i].max_surgery_time[j] = cJSON_GetArrayItem(max_surgery_time_json, j)->valueint;
                (surgeon)[i].time_left[j] = cJSON_GetArrayItem(max_surgery_time_json, j)->valueint;
            }
        } else {
            (surgeon)[i].max_surgery_time = NULL;
            (surgeon)[i].time_left = NULL;
            printf("Missing or invalid max_surgery_time for surgeon at index %d.\n", i);
        }
        surgeon[i].patients_assigned = NULL;
    }
}


void assign_patients_to_surgeon(void) {
    // Clear previously assigned patients (if needed, to prevent incorrect assignments)
    // for (int i = 0; i < num_surgeons; i++) {
    //     if (surgeons[i].patients_assigned != NULL) {
    //         free(surgeons[i].patients_assigned);
    //     }
    //     // Initialize the patients_assigned array for each surgeon
    //     surgeons[i].patients_assigned = (int *)calloc(num_patients, sizeof(int));
    //     if (surgeons[i].patients_assigned == NULL) {
    //         fprintf(stderr, "Memory allocation for patients_assigned failed for surgeon %d.\n", surgeons[i].id);
    //         return;
    //     }
    //     surgeons[i].num_assigned = 0;  // Reset the assigned count for each surgeon
    // }
    // Assign patients to surgeons based on surgeon_id
    for (int i = 0; i < num_patients; i++) {
        int surgeon_index = patients[i].surgeon_id;  // Get the surgeon_id from the patient 
        if (surgeon[surgeon_index].patients_assigned == NULL) {
            surgeon[surgeon_index].patients_assigned = (int *) calloc (100, sizeof(int)); // create a block for 100 patients
            surgeon[surgeon_index].current_size = 100;
        }
        else 
            if (surgeon[surgeon_index].num_assigned_patients == surgeon[surgeon_index].current_size) {// increase the current_size by 100
            	surgeon[surgeon_index].current_size += 100;
                surgeon[surgeon_index].patients_assigned = (int *) realloc (surgeon[surgeon_index].patients_assigned, (surgeon[surgeon_index].current_size) * sizeof(int));
            }
        surgeon[surgeon_index].patients_assigned[surgeon[surgeon_index].num_assigned_patients] = patients[i].id;
        surgeon[surgeon_index].num_assigned_patients++;  // Increment the assigned count
    }
}

    

void free_surgeons() {
    if (!surgeon) return;
    for (int i = 0; i < num_surgeons; i++) {
        if (surgeon[i].patients_assigned != NULL)
            free(surgeon[i].patients_assigned); // Free dynamically allocated array
        free(surgeon[i].max_surgery_time);  // Free the dynamically allocated max_surgery_time array
    }
    free(surgeon); // Free the array of Surgeon structs
}

// Function to parse the operating theatres
void parse_ots(cJSON *ot_array) {
    num_ots = cJSON_GetArraySize(ot_array);
    //printf("Number of Operating Theatres: %d\n", num_ots);
    ot = (OTs *)calloc(num_ots, sizeof(OTs));
    if (!ot) {
        printf("Memory allocation failed for operating theatres.\n");
        exit(1);
    }
    for (int i = 0; i < num_ots; i++) {
        cJSON *item = cJSON_GetArrayItem(ot_array, i);
        if (!item) continue;
        // Parse 'id'
         cJSON *id_Json = cJSON_GetObjectItem(item, "id");
        if (id_Json && cJSON_IsString(id_Json)) {
            ot[i].id = str2int(id_Json->valuestring);
        } 
        else {
            ot[i].id = -1;
            printf("Missing or invalid ID for operating theatre at index %d.\n", i);
            continue;  // Skip further processing for this OT
        }
        // Parse 'max_ot_time'
        cJSON *max_ot_time_json = cJSON_GetObjectItem(item, "availability");
        if (max_ot_time_json && cJSON_IsArray(max_ot_time_json)) {
            int size = cJSON_GetArraySize(max_ot_time_json);
            ot[i].max_ot_time = (int *)calloc(size, sizeof(int));
            ot[i].time_left = (int *)calloc(size, sizeof(int));
            if (!ot[i].max_ot_time) {
                printf("Memory allocation failed for max_ot_time.\n");
                //free_ots(i);  // Free already allocated memory
                exit(1);
            }
            for (int j = 0; j < size; j++) {
                cJSON *time_item = cJSON_GetArrayItem(max_ot_time_json, j);
                if (time_item && cJSON_IsNumber(time_item)) ot[i].max_ot_time[j] = time_item->valueint;
                else printf("Invalid max_ot_time value at index. Defaulting to 0.\n");
                ot[i].time_left[j] = ot[i].max_ot_time[j];
            }
        }
    }
}

void free_ots(){
    if(!ot) return;
    for(int i = 0 ; i < num_ots ; i++) {
        free(ot[i].max_ot_time);
        free(ot[i].time_left);
    }
    free(ot);
}

void parse_rooms(cJSON *room_array) {
    num_rooms = cJSON_GetArraySize(room_array);
    room = (Rooms*)calloc(num_rooms, sizeof(Rooms));
    if(!room){
        printf("Memory allocation failed for room array");
        exit(1);
    }
    for(int i = 0 ; i < num_rooms ; i++){
        cJSON *item = cJSON_GetArrayItem(room_array , i);
        if(!item) continue;
		room[i].nurses_alloted = NULL;						  
        cJSON *id_json = cJSON_GetObjectItem(item , "id");
		// room[i].occupied_cap = -1;
        if(id_json && cJSON_IsString(id_json)) room[i].id = str2int(id_json->valuestring);
        else{
            room[i].id = -1;
            printf("Missing or invalid ID for room id.");
        }
        cJSON *capacity_json = cJSON_GetObjectItem(item, "capacity");
        if (capacity_json && cJSON_IsNumber(capacity_json)) room[i].cap = capacity_json->valueint;
        else printf("Missing or invalid capacity for room at index %d. Defaulting to 0.\n", i);
    }
}

void free_rooms(void){
    if(!room) return;
    int i;
    for (i=0; i<num_rooms; ++i)
        free(room[i].nurses_alloted);
    free(room);
}

void parse_nurse(cJSON *nurses_array) {
    // Get the number of nurses from the JSON array
    num_nurses = cJSON_GetArraySize(nurses_array);
    nurses = (Nurses *)calloc(num_nurses, sizeof(Nurses)); // Allocate memory for nurses
    if (!nurses) {
        printf("Memory allocation failed for nurses.\n");
        exit(1);
    }
    for (int i = 0; i < num_nurses; i++) {
        cJSON *item = cJSON_GetArrayItem(nurses_array, i);
        if (!item) continue;
        // Parse 'id'
        cJSON *id_json = cJSON_GetObjectItem(item, "id");
        if (id_json && cJSON_IsString(id_json)) {
            nurses[i].id = str2int(id_json->valuestring); // Convert string ID to integer
        } else {
            nurses[i].id = -1;
            printf("Missing or invalid ID for nurse at index %d.\n", i);
        }
        // Parse 'skill_level'
        cJSON *skill_level_json = cJSON_GetObjectItem(item, "skill_level");
        if (skill_level_json && cJSON_IsNumber(skill_level_json)) {
            nurses[i].skill_level = skill_level_json->valueint;
        } else {
            nurses[i].skill_level = -1; // Default error value
            printf("Missing or invalid skill level for nurse at index %d.\n", i);
        }
        // Parse 'working_shifts'
        cJSON *shifts_json = cJSON_GetObjectItem(item, "working_shifts");
        if (shifts_json && cJSON_IsArray(shifts_json)) {
            int shift_count = cJSON_GetArraySize(shifts_json);
            nurses[i].shift = (Shifts *)calloc(shift_count, sizeof(Shifts)); // Allocate memory for shifts
            if (!nurses[i].shift) {
                printf("Memory allocation failed for shifts of nurse at index %d.\n", i);
                exit(1);
            }
            nurses[i].num_shifts = shift_count; // Store the number of shifts for the nurse
            for (int j = 0; j < shift_count; j++) {
                cJSON *shift_item = cJSON_GetArrayItem(shifts_json, j);
                if (shift_item) {
                    cJSON *day_json = cJSON_GetObjectItem(shift_item, "day");
                    if (day_json && cJSON_IsNumber(day_json)) {
                        nurses[i].shift[j].day = day_json->valueint;
                    } else {
                        nurses[i].shift[j].day = -1; // Default error value
                        printf("Missing or invalid day for nurse %d, shift %d.\n", i, j);
                    }

                    // Parse 'shift'
                    cJSON *shift_json = cJSON_GetObjectItem(shift_item, "shift");
                    if (shift_json && cJSON_IsString(shift_json)) {
                        nurses[i].shift[j].shift_time = string_to_shift_types(shift_json->valuestring); // Convert string to enum
                    } else {
                        nurses[i].shift[j].shift_time = early; // Default error value
                        printf("Missing or invalid shift time for nurse %d, shift %d.\n", i, j);
                    }

                    // Parse 'max_load'
                    cJSON *max_load_json = cJSON_GetObjectItem(shift_item, "max_load");
                    if (max_load_json && cJSON_IsNumber(max_load_json)) {
                        nurses[i].shift[j].max_load = max_load_json->valueint;
                    } else {
                        nurses[i].shift[j].max_load = -1; // Default error value
                        printf("Missing or invalid max load for nurse %d, shift %d.\n", i, j);
                    }
                    nurses[i].shift[j].rooms = NULL;  // Initialize rooms as NULL
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
        free(nurses); // Free the nurses array itself
    }
}

// Function to parse the JSON file
void parse_json(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Unable to open file.\n");
        return;
    }
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *json_data = (char *)calloc(1, file_size + 1);
    fread(json_data, 1, file_size, file);
    fclose(file);
    json_data[file_size] = '\0';
    cJSON *root = cJSON_Parse(json_data);
    if (!root) {
        printf("Error parsing JSON.\n");
        free(json_data);
        return;
    }
    days = cJSON_GetObjectItem(root, "days")->valueint;
    skill_levels = cJSON_GetObjectItem(root, "skill_levels")->valueint;
    // Parse weights
    cJSON *weights_json = cJSON_GetObjectItem(root, "weights");
    weights = (Weights *)calloc(1, sizeof(Weights));
    parse_weights(weights_json);
    // Parse occupants
    cJSON *occupants_array = cJSON_GetObjectItem(root, "occupants");
    parse_occupants(occupants_array);
    // parse patients
    cJSON *patients_array = cJSON_GetObjectItem(root , "patients");
    parse_patients(patients_array);
    // parse surgeons
    cJSON *surgery_array = cJSON_GetObjectItem(root , "surgeons");
    parse_surgeons(surgery_array);
    assign_patients_to_surgeon();
    // parse ots
    cJSON *ot_array = cJSON_GetObjectItem(root , "operating_theaters");
    parse_ots(ot_array);
    // parse rooms
    cJSON *room_array = cJSON_GetObjectItem(root , "rooms");
    parse_rooms(room_array);
    assign_occupants_to_rooms();
    // parse nurses
    cJSON *nurse_array = cJSON_GetObjectItem(root , "nurses");
    parse_nurse(nurse_array);
    free(json_data);
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
            for (int j = 0; j < occupants[i].length_of_stay*3; j++) {
                printf("%d ", occupants[i].workload_produced[j]);
            }
            printf("\n");
        }
        if (occupants[i].skill_level_required) {
            printf("  Skill Levels Required: ");
            for (int j = 0; j < occupants[i].length_of_stay*3; j++) {
                printf("%d ", occupants[i].skill_level_required[j]);
            }
            printf("\n");
        }
    }
}

void print_surgeons(Surgeon *surgeons) {
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
void print_ots(OTs *ots){
    if (!ots || num_ots <= 0){
        printf("No ots to display.\n");
        return;
    }
    for(int i=0 ; i < num_ots ; i++) {
        printf("Max OT Time (by day): ");
        if(ot[i].max_ot_time) for(int j = 0 ; j < days ; j++ ) printf("%d " , ots[i].max_ot_time[j]);
        else printf("No data available.");

        printf("OT Time left (by day): ");
        if(ot[i].time_left) for(int j = 0 ; j < days ; j++ ) printf("%d " , ots[i].time_left[j]);
        else printf("No data available.");

        printf("\n-----------------------------------\n");
    }
}

// Printing the rooms
void print_rooms(){
    if(!room || num_rooms <= 0){
        printf("No rooms to display.\n");
        return;
    }
    for(int i = 0 ; i < num_rooms ; i++)
        printf("%d\t%d\t%d\n" , room[i].id , room[i].cap, room[i].num_patients_allocated);
    printf("\n-----------------------------------\n");
}


// Printing the nurses
void print_nurses() {
    if (!nurses || num_nurses <= 0) {
        printf("No nurses to display.\n");
        return;
    }
    for (int i = 0; i < num_nurses; i++) {
        printf("Nurse ID: %d\n", nurses[i].id);
        printf("Skill Level: %d\n", nurses[i].skill_level);
        // Print shifts for each nurse
        if (nurses[i].num_shifts > 0 && nurses[i].shift) {
            for (int j = 0; j < nurses[i].num_shifts; j++) {
                printf("  Day: %d, Shift: ", nurses[i].shift[j].day);
                // Convert shift_type enum to string
                switch (nurses[i].shift[j].shift_time) {
                    case early: puts("early"); break;
                    case late: puts("late"); break;
                    case night: puts("Night"); break;
                    default: puts("Unknown"); break;
                }
                printf(", Max Load: %d\n", nurses[i].shift[j].max_load);
            }
        } 
        else printf("  No shifts assigned.\n");
    }
}


void print_patients(Patient *patient_struct_array) {
    int i;
    if (!patient_struct_array) return;
    for (i=0; i<num_patients; ++i) {
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
            for (int j = 0; j < 3*patient_struct_array[i].length_of_stay; j++) {
                printf("%d ", patient_struct_array[i].workload_produced[j]);
            }
            printf("\n");
        }
        // Print skill level required if it exists
        if (patient_struct_array[i].skill_level_required != NULL) {
            printf("Skill Level Required: ");
            for (int j = 0; j < 3*patient_struct_array[i].length_of_stay; j++) {
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
            for (int j = 0; j < 3*optional_patients[i]->length_of_stay; j++) {
                printf("%d ", optional_patients[i]->workload_produced[j]);
            }
            printf("\n");
        }
        // Print skill level required if it exists
        if (optional_patients[i]->skill_level_required) {
            printf("Skill Level Required: ");
            for (int j = 0; j < 3*optional_patients[i]->length_of_stay; j++)
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
            for (int j = 0; j < 3*mandatory_patients[i]->length_of_stay; j++) {
                printf("%d ", mandatory_patients[i]->workload_produced[j]);
            }
            printf("\n");
        }
        // Print skill level required if it exists
        if (mandatory_patients[i]->skill_level_required != NULL) {
            printf("Skill Level Required: ");
            for (int j = 0; j < 3*mandatory_patients[i]->length_of_stay; j++) {
                printf("%d ", mandatory_patients[i]->skill_level_required[j]);
            }
            printf("\n");
        }
    }
    printf("==================================================================\n");
}

//..................FUNCTIONS BELOW ARE FOR SORTING mandatory_patients & optional_patients ARRAYS ON surgery_release_day...................

Node *makeNode(Patient *struct_pointer) {
    Node *node = (Node *) calloc (1, sizeof(Node));
    node->pointer = struct_pointer;
    return node;
}

void appendNode(Node **sorted_array, Patient *struct_pointer, int index) {
    Node *node = makeNode(struct_pointer), *temp;
    if (sorted_array[index] == NULL) {
        sorted_array[index] = node;
    }
    else {
        temp = sorted_array[index];
        sorted_array[index] = node;
        node->next = temp;
        node->next->prev = node;
    }
}

Node **sorted_mandatory_patients = NULL;
void sort_mandatory_patients_on_release_day(Patient **patient_array, int n) {
    sorted_mandatory_patients = (Node **) calloc (days, sizeof(Node *));
    for (int i=0; i<n; ++i)
        appendNode(sorted_mandatory_patients, patient_array[i], patient_array[i]->surgery_release_day);
}

Node **sorted_optional_patients = NULL;
void sort_optional_patients_on_release_day(Patient **patient_array, int n) {
    sorted_optional_patients = (Node **) calloc (days, sizeof(Node *));
    for (int i=0; i<n; ++i)
        appendNode(sorted_optional_patients, patient_array[i], patient_array[i]->surgery_release_day);
}

void parse_linked_list(Node *head) {
	 // prints the linked list
    putchar('\n');
    for (Node *self=head; self; self = self->next) {
        printf("%d, ", self->pointer->surgery_release_day);
    }
    printf("\b\b");
}

void print_sorted_mandatory_array(void) {
    int i;
    for(i=0; i<days; ++i) {
        Node *head = sorted_mandatory_patients[i];
        parse_linked_list(head);
    }
}

void print_sorted_optional_array(void) {
    int i;
    for(i=0; i<days; ++i) {
        Node *head = sorted_optional_patients[i];
        parse_linked_list(head);
    }
}

void free_linked_list(Node *head) {
    if (head->next)
        free_linked_list(head->next);
    free(head);
}

void free_patients_sorted_array(Node **sorted_array) {
    // free sorted_array
    int i;
    for(i=0; i<days; ++i)
        if (sorted_array[i])
            free_linked_list(sorted_array[i]);
    free(sorted_array);
}

void copy_paste_pointers_from_sorted_mandatory_array_to_mandatory_patients(void) {
    // Here we will copy all the pointer from the sorted_array to the patients array in the sorted order.
    int i, j=0;
    Node *self;
    for(i=0; i<days; ++i)
        for(self=sorted_mandatory_patients[i]; self; self=self->next)
            mandatory_patients[j++] = self->pointer;
}

void copy_paste_pointers_from_sorted_optional_array_to_optional_patients(void) {
    // Here we will copy all the pointer from the sorted_array to the patients array in the sorted order.
    int i, j=0;
    Node *self;
    for(i=0; i<days; ++i)
        for(self=sorted_optional_patients[i]; self; self=self->next)
            optional_patients[j++] = self->pointer;
}

// ..............FUNCTIONS ABOVE ARE FOR SORTING mandatory_patients & optional_patients ARRAYS ON surgery_release_day................

// .................FUNCTIONS BELOW ARE FOR SORTING mandatory_patients & optional_patients ARRAYS ON length_of_stay..................


Node *find_smallest_iter(Node *head) {
	// finds the smallest node in a linked list
    Node *smallest=head, *self;
    for (self=head->next; self; self=self->next)
        if (self->pointer->length_of_stay < smallest->pointer->length_of_stay)
            smallest = self;
    return smallest;
}

Node *insertion_sort_for_linked_list(Node *head) {
	// runs insertion sort algorithm on the patients array (mandatory adn optional).
    if (!head || !head->next) return head; // No sorting needed for 0 or 1 node.
    Node *sorted = NULL; // New head of the sorted list
    Node *current = head;
    while (current) {
        Node *next = current->next; // Save the next node
        // Insert current node into the sorted list
        if (!sorted || current->pointer->length_of_stay < sorted->pointer->length_of_stay) {
            // Insert at the head
            current->next = sorted;
            if (sorted) sorted->prev = current; // Update prev for old head
            sorted = current;
            sorted->prev = NULL; // New head has no prev
        } else {
            // Traverse sorted list to find the correct position
            Node *temp = sorted;
            while (temp->next && temp->next->pointer->length_of_stay < current->pointer->length_of_stay) {
                temp = temp->next;
            }
            // Insert current after temp
            current->next = temp->next;
            if (temp->next) temp->next->prev = current; // Update prev for next node
            temp->next = current;
            current->prev = temp; // Update prev for current node
        }
        current = next; // Move to the next node in the original list
    }
    return sorted;
}

void sort_mandatory_patients_on_LOS(void) {
    // here we will sort the sorted_mandatory_patients array on LOS of the patient.
    // Approach: send each linked list to the insertion_sort_for_linked_list(Node *head) function.
    // It'll sort that list and return the head of the list.
    int i;
    for (i=0; i<days; ++i) {
        Node *head = sorted_mandatory_patients[i];
        head = insertion_sort_for_linked_list(head);
        sorted_mandatory_patients[i] = head;
    }
    copy_paste_pointers_from_sorted_mandatory_array_to_mandatory_patients();
}

void append_optional_to_mandatory(Node **sorted_mandatory_patients, Node **sorted_optional_patients) {
    // combines the 2 data structures - appends the optional_patients array (pointer array) at the tail of the mandatory_patients array
    for (int i = 0; i < days; ++i) {
        //printf("i am here\n");
        Node *mandatory_head = sorted_mandatory_patients[i];
        Node *optional_head = sorted_optional_patients[i];

        if (!mandatory_head) {
            sorted_mandatory_patients[i] = optional_head;
        } else {
            Node *tail = mandatory_head;
            while (tail->next) {
                tail = tail->next;
            }
            tail->next = optional_head;
            if (optional_head) {
                optional_head->prev = tail;
            }
        }
    }
}


void print_sorted_mandatory_patients(){
    for(int i = 0 ; i < days ; i++){
        printf("\n.........DAY %d.......\n", i+1);
        Node *mandatory_head = sorted_mandatory_patients[i];
        Node *tail = sorted_mandatory_patients[i];
        while(tail){
            printf("%d\t" , tail->pointer->id);
            tail = tail->next;
        }
        printf("\n");
    }
}

// ...................FUNCTION BELOW ARE FOR THE ROOM_GENDER_MAP.....................//


void initialize_room_gender_map(int **room_gender_map) {
	// initializes the room_gender_map
    *room_gender_map = (int *)calloc(num_rooms, sizeof(int)); // Allocate memory for room_gender_map
    if (!*room_gender_map) {
        printf("Memory allocation failed for room_gender_map.\n");
        exit(1);
    }
    memset(*room_gender_map, -1, num_rooms * sizeof(int)); // Initialize values to -1
}

void populate_room_gender_map(int *room_gender_map) {
	 // populates the room_gender_map
    if (!room_gender_map) {
        printf("Error: room_gender_map is not initialized.\n");
        return;
    }
    for (int i = 0; i < num_rooms; ++i) {
        room_gender_map[i] = -1; 
    }
    for (int i = 0; i < num_occupants; ++i) {
        if (occupants[i].room_id >= 0 && occupants[i].room_id < num_rooms) {
            room_gender_map[occupants[i].room_id] = occupants[i].gen; 
        } else {
            printf("Warning: Invalid room_id %d for occupant %d.\n", occupants[i].room_id, i);
        }
    }
}

void print_map(int *room_gender_map){
    for(int i = 0 ; i<num_rooms ; i++)
        printf("room number: %d\tgender: %d\n", i, room_gender_map[i]);
}


// .......FUNCTIONs BELOW - PRIORITY QUEUE IMPLEMENTATION FOR KEEPING TRACK OF CARRY FORWARDED PATIENTS..........//

// Initialize the priority queue
void heapifyUp(PriorityQueue *pq, int index) {
	 // runs the max/min heapify from leaf to the root, i.e. O (log n) time
    while (index > 0) {
        int parent = (index - 1) / 2;
        if (compareNodesForPQ(pq->data[index], pq->data[parent]) > 0) {
            // Swap child with parent
            HeapNode temp = pq->data[index];
            pq->data[index] = pq->data[parent];
            pq->data[parent] = temp;
        } else {
            break;
        }
        index = parent;
    }
}

void heapifyDown(PriorityQueue *pq, int index) {
	// runs the max/min heapify from root to leaf, i.e. O (log n) time
    while (2 * index + 1 < pq->current_size) {
        int leftChild = 2 * index + 1;
        int rightChild = 2 * index + 2;
        int largest = index;

        if (leftChild < pq->current_size && compareNodesForPQ(pq->data[leftChild], pq->data[largest]) > 0) {
            largest = leftChild;
        }
        if (rightChild < pq->current_size && compareNodesForPQ(pq->data[rightChild], pq->data[largest]) > 0) {
            largest = rightChild;
        }
        if (largest != index) {
            // Swap parent with the largest child
            HeapNode temp = pq->data[index];
            pq->data[index] = pq->data[largest];
            pq->data[largest] = temp;
            index = largest;
        } else
            break;
    }
}

void initPQ(PriorityQueue *pq, int initialCapacity) {
	// initialCapacity is 20
    pq->current_size = 0;
    pq->capacity = initialCapacity;
    pq->data = (HeapNode *) calloc (initialCapacity, sizeof(HeapNode));
    if (!pq->data) {
        printf("Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
}

// Insert a new element into the priority queue
void insertNodeInPQ(PriorityQueue *pq, HeapNode node) {
	// inserts a node in the PQ
    if (pq->current_size == pq->capacity) {
        resizePQ(pq); // Resize the array if it's full
    }
    pq->data[pq->current_size] = node; // Add the new node at the end
    heapifyUp(pq, pq->current_size);   // Restore the heap property
    pq->current_size++;
}

// Extract the element with the highest priority
HeapNode extractMaxFromPQ(PriorityQueue *pq) {
    if (isEmptyPQ(pq)) {
        printf("Priority Queue Underflow\n");
        exit(EXIT_FAILURE);
    }
    HeapNode maxNode = pq->data[0];
    pq->data[0] = pq->data[--pq->current_size];
    heapifyDown(pq, 0);
    return maxNode;
}

// Peek at the element with the highest priority without removing it
HeapNode peekMaxFromPQ(PriorityQueue *pq) {
    // peeks the current root of heap (Or of PQ)
    if (isEmptyPQ(pq)) {
        printf("Priority Queue is empty\n");
        exit(EXIT_FAILURE);
    }
    return pq->data[0];
}

// Check if the priority queue is empty
int isEmptyPQ(PriorityQueue *pq) {
	 // checks if the PQ is empty
    return pq->current_size == 0;
}

// Compare two HeapNodes based on priority
int compareNodesForPQ(HeapNode a, HeapNode b) {
	// compares 2 nodes and returns the difference of either p_id, mandatory or delay
    if (a.mandatory != b.mandatory) {
        return a.mandatory - b.mandatory; // Higher mandatory is better
    }
    if (a.delay != b.delay) {
        return a.delay - b.delay;         // Higher delay is better
    }
    return b.patient_id - a.patient_id;   // Lower patient_id is better
}

void resizePQ(PriorityQueue *pq) {
	// resizes PQ's capacity
    pq->capacity *= 2; // Double the capacity
    pq->data = (HeapNode *)realloc(pq->data, pq->capacity * sizeof(HeapNode));
    if (!pq->data) {
        printf("Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
}

void freePQ(PriorityQueue *pq) {
    free(pq->data);
    // pq->data = NULL;
    // pq->current_size = 0;
    // pq->capacity = 0;
    free(pq);
}

HeapNode makeHeapNode(int mandatory, int delay, int p_id) {
	// creates a heap_node for PQ
    HeapNode node = {mandatory, delay, p_id};
    return node;
}


//-----------------------------------------BELOW: QUICK SORT IMPLEMENTATION FOR THE SURGEONS' ARRAY--------------------------------------

// int getPivot_for_surgeons(Surgeon_data **s_data_arr, int n) {
//     // if the length is more than two then we'll find the avg of 3 random numbers
//     // this function will not be invoked if n is 2 or less than 2
//     int s1 = n / 4, s2 = (3 * n) / 4, s3 = n / 2;
//     // return (s_data_arr[s1]->surgery_duration_sum + s_data_arr[s2]->surgery_duration_sum + s_data_arr[s3]->surgery_duration_sum) / 3;
//     int count = 0, sum = 0;

//     if (!s_data_arr[s1]->isNull) {
//         sum += s_data_arr[s1]->surgery_duration_sum;
//         count++;
//     }
//     if (!s_data_arr[s2]->isNull) {
//         sum += s_data_arr[s2]->surgery_duration_sum;
//         count++;
//     }
//     if (!s_data_arr[s3]->isNull) {
//         sum += s_data_arr[s3]->surgery_duration_sum;
//         count++;
//     }
//     return count > 0 ? sum / count : 0;
// }

void swap_for_surgeons(Surgeon_data **data_arr, int i, int j) {
    Surgeon_data *temp = data_arr[i];
    data_arr[i] = data_arr[j];
    data_arr[j] = temp;
}

int partition_for_surgeons(Surgeon_data **s_data_arr, int low, int high) {
    int i, j;
    int pivot = s_data_arr[high]->surgery_duration_sum;
     i = low-1;  // `i` starts before the first index.
    
    for (int j = low; j < high; ++j) {
        if (s_data_arr[j] == NULL) {
            fprintf(stderr, "Error: NULL pointer in s_data_arr during partition\n");
            exit(EXIT_FAILURE);
        }

        if (s_data_arr[j]->surgery_duration_sum > pivot) {
            ++i;
            swap_for_surgeons(s_data_arr, i, j);  // Swap current element to the left partition.
        }
    }

    // Return the partition index.
    swap_for_surgeons(s_data_arr, i+1 , high-1);
    return i + 1;  
}

// void quick_sort_surgeons_on_s_duration_sum(Surgeon_data **s_data_arr, int n) {
//     // Here, we'll sort the surgeons' array of length - max_surgeon_id + 1 on surgery_duration_sum.
//     int pivot, n_left;
//     if (n == 1) return;
//     if (n == 2) {if (s_data_arr[0]->surgery_duration_sum < s_data_arr[1]->surgery_duration_sum) swap_for_surgeons(s_data_arr, 0, 1); return;}
//     pivot = s_data_arr[n/2]->surgery_duration_sum;
//     n_left = partition_for_surgeons(s_data_arr, n, pivot);
//     quick_sort_surgeons_on_s_duration_sum(s_data_arr, n_left);
//     quick_sort_surgeons_on_s_duration_sum(s_data_arr+n_left, n-n_left);
// }

void quick_sort_surgeons_on_s_duration_sum(Surgeon_data **s_data_arr, int low, int high){
    if(low < high){
        int pi = partition_for_surgeons(s_data_arr, low, high);

        quick_sort_surgeons_on_s_duration_sum(s_data_arr, low, pi-1);
        quick_sort_surgeons_on_s_duration_sum(s_data_arr, pi+1, high);
    }
}
int sorting_day;

void print_s_data_arr(Surgeon_data **s_data_arr, int n, int d){
    printf("\nDay: %d",d);
    for(int i = 0 ; i<n ; i++){
        printf("\nid: %d -- duration_Sum: %d \n  " , s_data_arr[i]->surgeon_id, s_data_arr[i]->surgery_duration_sum);
        // for(int j = 0 ; j<s_data_arr[i]->num_assigned_patients ; j++){
        //     printf("%d \t" , s_data_arr[i]->assigned_patients[j]);
        // }
        printf("\n");
    }
}

int compare_surgeon(const void *a, const void *b) {
    Surgeon_data *s_a = *(Surgeon_data **)a;
    Surgeon_data *s_b = *(Surgeon_data **)b;

    // Compare max_ot_time for the global sorting day
    if (s_b->surgery_duration_sum != s_a->surgery_duration_sum) {
        return s_b->surgery_duration_sum - s_a->surgery_duration_sum; // Descending order
    }
    return s_b->surgeon_id - s_a->surgeon_id; // Secondary sort by id (ascending order)
}

void sort_s_data_arr(Surgeon_data **s_data_arr, int n, Node *head) {
    // sort this array using quick sort algorithm on surgeon_id
    int i, p_id, s_id;
    Node *self;
    for(self=head; self; self=self->next) {
        s_id = self->pointer->surgeon_id;
        p_id = self->pointer->id;
        if (s_data_arr[s_id] == NULL) {
            s_data_arr[s_id] = (Surgeon_data *) calloc (1, sizeof(Surgeon_data));
            s_data_arr[s_id]->assigned_patients = (int *) calloc (++s_data_arr[s_id]->num_assigned_patients, sizeof(int));
        }
        else
            s_data_arr[s_id]->assigned_patients = (int *) realloc (s_data_arr[s_id]->assigned_patients, ++s_data_arr[s_id]->num_assigned_patients * sizeof(int));

        s_data_arr[s_id]->assigned_patients[s_data_arr[s_id]->num_assigned_patients-1] = p_id;
        s_data_arr[s_id]->surgeon_id = s_id;
        s_data_arr[s_id]->surgery_duration_sum += self->pointer->surgery_duration;
        s_data_arr[s_id]->isNull = 0;
    }
    
    // for null pointers - we're creating a structure and putting: 
    // 1 in isNull field    -1 in surgeon_id field  0 in num_assigned_patients  NULL in assigned_patients array.
    for (i=0; i<n; ++i)
        if (s_data_arr[i] == NULL) {
            s_data_arr[i] = (Surgeon_data *) calloc (1, sizeof(Surgeon_data));
            s_data_arr[i]->isNull = 1;
            s_data_arr[i]->surgeon_id = -1;
        }
       // print_s_data_arr(s_data_arr, n);
    //quick_sort_surgeons_on_s_duration_sum(s_data_arr, 0, n-1);  
   qsort(s_data_arr, n, sizeof(Surgeon_data *), compare_surgeon );
       print_s_data_arr(s_data_arr, n, sorting_day);
}



//-----------------------------------------ABOVE: QUICK SORT IMPLEMENTATION FOR THE SURGEON ARRAY--------------------------------------

//-----------------------------------------BELOW: QUICK SORT IMPLEMENTATION FOR THE OT ARRAY--------------------------------------


void swap_for_OTs(OTs **data_arr, int i, int j) {
	 // swap OTs in the ot_data_arr array
    OTs *temp = data_arr[i];
    data_arr[i] = data_arr[j];
    data_arr[j] = temp;
}

int partition_for_OTs(OTs **ot_data_arr, int low, int high, int d) {
    int i = low - 1;  // `i` starts before the first index.
      int pivot = ot_data_arr[high ]->max_ot_time[d] ;

    for (int j = low; j < high; ++j) {
        if (ot_data_arr[j] == NULL) {
            fprintf(stderr, "Error: NULL pointer in ot_data_arr during partition\n");
            exit(EXIT_FAILURE);
        }

        if (ot_data_arr[j]->max_ot_time[d] > pivot) {
            ++i;
            swap_for_OTs(ot_data_arr, i, j);  // Swap current element to the left partition.
        }
    }
    swap_for_OTs(ot_data_arr, i+1, high);
    // Return the partition index.
    return i + 1;  

   
}


// void quick_sort_OTs_on_max_ot_time(OTs **ot_data_arr, int n, int d) {
//     if (n == 1) return;
//     if (n == 2) {if (ot_data_arr[0]->max_ot_time[d] < ot_data_arr[1]->max_ot_time[d]) swap_for_OTs(ot_data_arr, 0, 1); return;}
// //    printf("%d\n" , getPivot_for_OTs(ot_data_arr, n, d));
//    //int pivot = ot_data_arr[n/2]->max_ot_time[d];
//     int pi = partition_for_OTs(ot_data_arr, n, d);
//      quick_sort_OTs_on_max_ot_time(ot_data_arr, pi-1, d);
//      quick_sort_OTs_on_max_ot_time(ot_data_arr , pi+1 , d);
// }


void print_ot_data_arr(OTs **ot_data_arr , int d){
    printf("Day: %d\n",d);
    for(int i = 0 ; i < num_ots ; i++){
        printf("id: %d -- max_ot_time: %d\n", ot_data_arr[i]->id, ot_data_arr[i]->max_ot_time[d]);
    }
}

void quick_sort_OTs_on_max_ot_time(OTs **ot_data_arr, int low, int high , int day){
if(low < high){
    int pi = partition_for_OTs(ot_data_arr , low , high , day);
    //printf("\n%d\t%d",pi,day);
    quick_sort_OTs_on_max_ot_time(ot_data_arr , low, pi-1 , day);
   // print_ot_data_arr(ot_data_arr , day);
    quick_sort_OTs_on_max_ot_time(ot_data_arr, pi+1 , high, day);

}
}



int compare_ots(const void *a, const void *b) {
    OTs *ot_a = *(OTs **)a;
    OTs *ot_b = *(OTs **)b;

    // Compare max_ot_time for the global sorting day
    if (ot_b->max_ot_time[sorting_day] != ot_a->max_ot_time[sorting_day]) {
        return ot_b->max_ot_time[sorting_day] - ot_a->max_ot_time[sorting_day]; // Descending order
    }
    return ot_a->id - ot_b->id; // Secondary sort by id (ascending order)
}

void sort_ot_data_arr(OTs **ot_data_arr, int d) {
	 // sorts ot_data_arr using Quick sort algorithm
    if (ot_data_arr == NULL || num_ots <= 0) {
        fprintf(stderr, "Error: Invalid input array\n");
        return;
    }
   // print_ot_data_arr(ot_data_arr , d);
   // quick_sort_OTs_on_max_ot_time(ot_data_arr, 0, num_ots-1, d);
   sorting_day = d;
   qsort(ot_data_arr, num_ots, sizeof(OTs *), compare_ots );
   // print_ot_data_arr(ot_data_arr , d);
}

//-----------------------------------------ABOVE: QUICK SORT IMPLEMENTATION FOR THE OT ARRAY--------------------------------------

// -----------------------------------------------------BELOW: PATIENT ADMISSION------------------------------------------------------

//..............................................FUNCTION BELOW ARE FOR PATIENT ADMISSION...........................................


GenderRoom *makeGenderRoomNode(Rooms *pointer) {
	// creates a GenderRoom node and puts pointer in the pointer's field
    GenderRoom *node;
    if ((node = (GenderRoom *) calloc (1, sizeof(GenderRoom))) == NULL) {
        perror("Dynamic memory allocation error");
        exit(EXIT_FAILURE);
    }
    node->pointer = pointer;
    return node;
}

GenderRoom *appendGenderRoomNode(GenderRoom *head, GenderRoom *node) {
	// this function appends the "node" at the beginning of the linked list - head
    if (!head) return node;
    head->prev = node;
    node->next = head;
    node->prev = NULL;
    return node;
}

GenderRoom *deleteGenderRoomNode(GenderRoom *head, GenderRoom *node) {
	// this function will take our "node" from the linked list - head and return the new head in case the head changes
    if (!head) return NULL;
    GenderRoom *prev_ptr;
    if (head == node) {
        prev_ptr = head;
        head = head->next;
        return head;
    }
    if (node->next) {
        prev_ptr = node->prev;
        prev_ptr->next = node->next;
        node->next->prev = prev_ptr;
        return head;
    }
    prev_ptr = node->prev;
    prev_ptr->next = node->next;
    return head;
}

void make_3_room_arrays(int *room_gender_map) {
	// cerates 3 link_lists for room_gender so that we'll have a linked list for each gender. 
    int i;
    num_gender_A_rooms = 0;
    num_gender_B_rooms = 0;
    num_empty_rooms = 0;

    for (i=0; i<num_rooms; ++i) {
        if (room_gender_map[i] == -1)
            empty_rooms = appendGenderRoomNode(empty_rooms, makeGenderRoomNode(room+i));
        if (room_gender_map[i] == 1) 
            gender_B_rooms = appendGenderRoomNode(gender_B_rooms, makeGenderRoomNode(room+i));
        if (room_gender_map[i] == 0)
            gender_A_rooms = appendGenderRoomNode(gender_A_rooms, makeGenderRoomNode(room+i));
    }
}

void free_3_room_arrays(void) {
     // free 3 room_gender linked_lists.
    GenderRoom *self, *temp;
    
    // free empty rooms
    for (self=empty_rooms; self->next; ) {
        temp = self->next;
        free(self);
        self = temp;
    }
    free(self);

    // free gender A rooms
    for (self=gender_A_rooms; self->next; ) {
        temp = self->next;
        free(self);
        self = temp;
    }
    free(self);
    
// free gender B rooms
    for (self=gender_B_rooms; self->next; ) {
        temp = self->next;
        free(self);
        self = temp;
    }
    free(self);
}

// int find_length_of_DLL(Node *head) {
//     int count;
//     Node *self;
//     for(self=head, count=0; self; self=self->next) count++;
//     return count;
// }

int find_max_surgeon_id(Node *head) {
    int max_s_id = -1, i;
    Node *self = head;
    while(self){
        if(self->pointer->surgeon_id > max_s_id){
            max_s_id = self->pointer->surgeon_id;
        }
        self = self->next;
    }

    return max_s_id;
}

GenderRoom *removeAndAppendGenderRoom(int r_id, GenderRoom *src) {
    // remove the r_id room from src genderRoom array and append it in the emptyRooms genderRoom array
    // return: the new head of src
    GenderRoom *self = src;
   while (self->next && self->pointer->id != r_id) {
    self = self->next;  
}
    //for (self=src; self->pointer->id != r_id; self=self->next);
    if (self == src) { // it's the very first room in the genderRoom array,
    // so it'll be differently dealt with.
        // take out self from src genderRoom array
        src = src->next;

        // append self in emptyRooms genderRoom array
        empty_rooms->prev = self;
        self->next = empty_rooms;
        self->prev = NULL; // we can leave this step as self->prev is already NULL. But just to be on the safer side, we're doing this step.
        empty_rooms = self;
        return src;
    }

    src = deleteGenderRoomNode(src, self);
    empty_rooms = appendGenderRoomNode(empty_rooms, self);
    return src;
}

int findSuitableRoom(int p_id, GenderRoom *gender_head) {
    // return - room_id OR -1 in case no room is suitable
    // assign available Room and return the room_id
    // considerations - capacity, gender, compatibility & age-mix
    // do something so that the sum_workload_of_a_room does not exceed too much as that will create issues while assigning nurses
    int flag, i, j, r_id, g = patients[p_id].gen, n;
    char *age = patients[p_id].age_group;
    GenderRoom *self, *prev_ptr, *temp;

    for (self=gender_head; self; self=self->next) {
        flag = 0;
        // parse the gender_rooms array and for each room - 
        // find its gender, capacity, num_patients_allocated and whether it is compatible to the patient at hand or not
        r_id = self->pointer->id;
        if (room[r_id].cap <= (room[r_id].num_patients_allocated + room[r_id].occupants_cap)) continue;
        // check for compatibility
        for (j=0; j < patients[p_id].num_incompatible_rooms; ++j)
            if (r_id == patients[p_id].incompatible_room_ids[j]) { flag = 1; break; }
        if(flag) continue;
        return r_id;
    }
    
    if (patients[p_id].assigned_room_no == -1) {
        // start parsing the empty_rooms array as all the rooms of this gender are either incompatible to this patient 
        // or their capacity has reached full
        for (self=empty_rooms; self; self=self->next) {
            r_id = self->pointer->id;
            if (room[r_id].num_patients_allocated) {
            // just to check whether our 3 room_gender_arrays are properly allocated or not
                fprintf(stderr, "\nThe number of patients allocated is not 0 in empty_array");
                exit(EXIT_FAILURE);
            }
            for (j=0; j<patients[p_id].num_incompatible_rooms; ++j)
                if (r_id == patients[p_id].incompatible_room_ids[j]) { flag = 1; break; }
            if(flag) continue;
            // now remove this room node from empty room array and append it in the associated gender array
            if (self == empty_rooms) {
                empty_rooms = empty_rooms->next;
                empty_rooms->prev = NULL;
            }
            else {
                prev_ptr = self->prev;
                prev_ptr->next = self->next;
                self->next->prev = prev_ptr;
                self->prev = NULL;
                self->next = NULL;
            }
            // append self at head of the associated gender array, i.e. gender_head
            gender_head = appendGenderRoomNode(gender_head, self);
            return r_id;
        }
        return -1;
    }
}

void update_LOS_of_patients(int d) {
    /* Check if any patient's length_of_stay is over and if yes then remove that patient from - 
        1. the room he's been assigned to.
        2. any assignment in the NRA problem pertaining to this patient. 
    */
    
    int i, g, r_id, admit_day, los, days_passed;
    Node *p, *self;
    GenderRoom *gen_array;
    Occupants occ;

    // first consider the occupants - 
    // if an occupant's LOS is over then throw him out of the room and make space of a patient.
    for (i=0; i<num_occupants; ++i) {
        occ = occupants[i];
        r_id = occ.room_id;
        g = occ.gen;
        days_passed = d - occ.length_of_stay;
        if (days_passed >= 0) {
            // throw the occupant out
            room[r_id].occupants_cap--;
            if (!room[r_id].occupants_cap && !room[r_id].num_patients_allocated)
                    // remove the room from the associated gender array and append at the head of the empty_array linked list
                    g ? removeAndAppendGenderRoom(r_id, gender_B_rooms) : removeAndAppendGenderRoom(r_id, gender_A_rooms);
        }
    }

    for (i=0; i <= d; ++i) {
        p = sorted_mandatory_patients[i];
        // parse the sorted_mandatory_patients array (arr of linked lists pointers)
        for(self=p; self; self=self->next) {
            // extract the data about patient
            admit_day = self->pointer->admission_day;
            r_id = self->pointer->assigned_room_no;
            g = self->pointer->gen;
            los = self->pointer->length_of_stay;
            // release_day = self->pointer->surgery_release_day;
            // due_day = self->pointer->surgery_due_day;

            if (admit_day != -1) { // if true - means the patient has been admitted.
                days_passed = d - admit_day;
                if (days_passed >= los) {
                    // release the patient
                    // remove this patient's data from everywhere, i.e. 
                    // from room (capacity, gender, age-mix)
                    // update the data in nurses array about this patient
                    room[r_id].num_patients_allocated--;
                    // ignore age-mix
                    // consider if the room is now empty then what will you do
                    if (!room[r_id].occupants_cap && !room[r_id].num_patients_allocated)
                        // remove this room from the associated gender array and append it in empty_rooms array
                        g ? removeAndAppendGenderRoom(r_id, gender_B_rooms) : removeAndAppendGenderRoom(r_id, gender_A_rooms);
                }
            }
        }
    }
}

void free_s_data_arr(Surgeon_data **s_data_arr, int n) {
	// frees s_data_arr array
    int i;
    for (i=0; i<n; ++i)
        free(s_data_arr[i]->assigned_patients);
    free(s_data_arr);
}

// --------------------BELOW: Functions for admitting patients from the PQ (linked list functions as well)---------------------

typedef struct optionals_PQ {
    HeapNode node;
    struct optionals_PQ *next;
} Mand_opt_PQ;

Mand_opt_PQ *makeMand_opt_PQNode(HeapNode heap_node) {
    Mand_opt_PQ *node;
    if ((node = (Mand_opt_PQ *) calloc (1, sizeof(Mand_opt_PQ))) == NULL) {
        perror("\nDynamic memory allocation error: not enough memory for Mand_opt_PQ");
        exit(EXIT_FAILURE);
    }
    node->node = heap_node;
    return node;
}

Mand_opt_PQ *insertNodeInMand_opt_PQ(Mand_opt_PQ *head, Mand_opt_PQ *node) {
    // append at the head
    node->next = head;
    head = node;
    return head;
}

// Mand_opt_PQ *deleteNodeInMand_opt_PQ(Mand_opt_PQ *head, Mand_opt_PQ *node) {
//     // append at the head
//     Mand_opt_PQ *self;
//     if (head == node)
//         head = head->next;
//     else {
//         for (self=head; self->next != node; self=self->next);
//         if (self->next == node)
//             self->next = node->next;
//     }
//     node->next = NULL;
//     return node;
// }

void freeMand_opt_PQ(Mand_opt_PQ *head) {
    // frees the Mand_opt_PQ singly linked list
    if (!head) return;
    freeMand_opt_PQ(head->next);
    free(head);
}

OTs *admitMandatoryFromPQ(PriorityQueue *pq, int d, OTs **ot_data_arr, OTs *current_ot, int current_ot_index) {
    // this function will admit patients from the PQ (who could not be admitted before bcz of some reason)
    HeapNode self, node;
    Mand_opt_PQ *head = NULL, *mand_ptr = NULL;
    int i, j, p_id, flag, delay, r_id, s_duration, ot_id, p_counter, ot_counter, temp;

    for (p_counter=0, flag=0; (p_counter < pq->current_size && pq->data[p_counter].mandatory); ++p_counter) {
        node = extractMaxFromPQ(pq);
        p_id = node.patient_id;
        s_duration = patients[p_id].surgery_duration;
        // assign available OT
        // first - select a suitable OT
        if (!current_ot->time_left[d]) 
            if (current_ot_index != num_ots)
                current_ot = ot_data_arr[current_ot_index++];

        while (current_ot->time_left[d] < s_duration) {
            if (current_ot_index == num_ots) {flag = 1; break;}
            current_ot = ot_data_arr[current_ot_index++];
        }
        if (flag) {
            flag = 0;
            for (i=0; i<num_ots; ++i) {
                if (ot[i].time_left[d] >= s_duration) {
                    // assign this ot to this patient
                    flag = 1;
                    patients[p_id].assigned_ot = i;
                    ot[i].time_left -= s_duration;
                    break;
                }
            }
            // that means no ot was assigned
            if (!flag) {
                head = insertNodeInMand_opt_PQ(head, makeMand_opt_PQNode(node));
                return NULL;
            }
        }
        else {
            // assign OT--------------------------------------------------------------------------------------------------
            patients[p_id].assigned_ot = current_ot->id;
            // update the time left
            current_ot->time_left -= s_duration;
        }
        // assign a room
        r_id = patients[p_id].gen ? findSuitableRoom(p_id, gender_B_rooms) : findSuitableRoom(p_id, gender_A_rooms);
        if (r_id == -1) {
            // means there's no room in the gender_room_array and empty_array that is compatible with this patient
            // so in this case - put the patient in the PQ and move on
            insertNodeInPQ(pq, makeHeapNode(patients[p_id].mandatory, node.delay, p_id));
            head = insertNodeInMand_opt_PQ(head, makeMand_opt_PQNode(node));
            break;
        }
        else {
            patients[p_id].assigned_room_no = r_id;
            room[r_id].num_patients_allocated++;
        }
        // assign admission day
        patients[p_id].admission_day = d;
    }
    // transfer all the patients in the PQ which were taken out for admission but bcz of resource constraints could not be admitted.
    for (mand_ptr=head; mand_ptr; mand_ptr=mand_ptr->next)
        insertNodeInPQ(pq, mand_ptr->node);
    freeMand_opt_PQ(head);
    return current_ot;
}

OTs *admitOptionalFromPQ(PriorityQueue *pq, int d, OTs **ot_data_arr, OTs *current_ot){
    // here we'll admit the optional patients from the PQ
    int i, j, p_id, s_id, temp, s_duration, flag, r_id;
    HeapNode node;
    Mand_opt_PQ *head = NULL, *opt_ptr = NULL;

    // firstly take out all mandatory patients and put them in a linked list
    // after admitting the optional patients put mandatory patients back in the PQ
    for (i=0, node=pq->data[0]; node.mandatory; node=pq->data[++i]) {
        node = extractMaxFromPQ(pq);
        head = insertNodeInMand_opt_PQ(head, makeMand_opt_PQNode(node));
    }
    
    // now admit the optional patients
    for (i=0, node=pq->data[0]; pq->current_size; node=pq->data[++i]) {
        p_id = node.patient_id;
        s_duration = patients[p_id].surgery_duration;
        // assign available OT
        // first - select a suitable OT
        if (!current_ot->time_left[d]) 
            if (current_ot_index != num_ots)
                current_ot = ot_data_arr[current_ot_index++];

        while (current_ot->time_left[d] < s_duration) {
            if (current_ot_index == num_ots) {flag = 1; break;}
            current_ot = ot_data_arr[current_ot_index++];
        }
        if (flag) {
            flag = 0;
            for (i=0; i<num_ots; ++i) {
                if (ot[i].time_left[d] >= s_duration) {
                    // assign this ot to this patient
                    flag = 1;
                    patients[p_id].assigned_ot = i;
                    ot[i].time_left -= s_duration;
                    break;
                }
            }
            // that means no ot was assigned
            if (!flag) {
                head = insertNodeInMand_opt_PQ(head, makeMand_opt_PQNode(node));
                return NULL;
            }
        }
        else {
            // assign OT-------------------------------------------------------------------------------------------
            patients[p_id].assigned_ot = current_ot->id;
            // update the time left
            current_ot->time_left -= s_duration;
        }
        // assign a room
        r_id = patients[p_id].gen ? findSuitableRoom(p_id, gender_B_rooms) : findSuitableRoom(p_id, gender_A_rooms);
        if (r_id == -1) {
            // means there's no room in the gender_room_array and empty_array that is compatible with this patient
            // so in this case - put the patient in the PQ and move on
            insertNodeInPQ(pq, makeHeapNode(patients[p_id].mandatory, node.delay, p_id));
            head = insertNodeInMand_opt_PQ(head, makeMand_opt_PQNode(node));
            continue;
        }
        patients[p_id].assigned_room_no = r_id;
        room[r_id].num_patients_allocated++;
        // assign admission day
        patients[p_id].admission_day = d;

        // transfer all the patients in the PQ which were taken out for admission but bcz of resource constraints could not be admitted.
        for (opt_ptr=head; opt_ptr; opt_ptr=opt_ptr->next)
            insertNodeInPQ(pq, opt_ptr->node);
        freeMand_opt_PQ(head);
        return current_ot;
    }
}


// --------------------ABOVE: Functions for admitting optional patients from the PQ (linked list functions as well)---------------------


void admit_patients(int *room_gender_map, PriorityQueue *pq) {
    /*
        we'll go patient by patient. 
        when a patient comes, we'll check what's his release_day and due_day
            if the due_day == release_day then admit him at any cost
            1. check his s_id - if the surgeon is not available then insert him into the PQ
            2. if surgeon is available, 
            
            - put patients with same surgeon in one OT, thereby controlling surgeon_transfer contraint
            - do this for each linked-list in the sorted_mandatory_patients and sorted_optional_patients arrays
            - for this, firstly create an array of size #(patients in the 1 linked-list) and put surgeon-id of each
            patient as the element
            - then sort it using counting sort (as we already know #surgeons OR we can find that also while parsing 
            the linked-list)
    */
    
    int i, j, k, l, d, g, r_id, p_id, s_id, flag, s_duration, current_ot_index, s_duration_sum, max_surgeon_id, len_surgeon_array, temp_patient_id;
    Surgeon_data **s_data_arr;
    HeapNode node;
    Node *head;
    OTs **ot_data_arr, *current_ot, *new_ot;

    make_3_room_arrays(room_gender_map);
     // first, make another OT array (array of pointers to structures)
    // copy all the OT struct pointers to the ot_data_array.
    ot_data_arr = (OTs **)calloc(num_ots, sizeof(OTs *));
        if(ot_data_arr == NULL){
            printf("Memory not allocated.\n");
            exit(-1);
        }
    for (i=0; i<num_ots; ++i)
        ot_data_arr[i] = ot+i;
    
    // ---------------------------------------------------------START ADMISSION------------------------------------------------------------
    for (d=0; d<days; ++d) {
        // update LOS for all patients
        if (d) update_LOS_of_patients(d);

        sort_ot_data_arr(ot_data_arr, d);
        current_ot_index = 0;
        current_ot = ot_data_arr[0];
        flag = 0;

        Node *head = sorted_mandatory_patients[d];
        //printf("sorted_mandatory_patients[%d]: %p\n", d, (void*)sorted_mandatory_patients[d]);
        //printf("head:%p\n", head);
        max_surgeon_id = find_max_surgeon_id(head);
        len_surgeon_array = max_surgeon_id + 1;
        // Added 1 to max_surgeon_id cuz we need space for the nth (max_is) also, so we'll have to make the total length (n+1)
        s_data_arr = (Surgeon_data **) calloc (len_surgeon_array, sizeof(Surgeon_data *));
        sort_s_data_arr(s_data_arr, len_surgeon_array, head);
        
        //incrementing the current ot while we get to an ot which is available that day
        while (!current_ot->time_left[d]) {
            if (current_ot_index == num_ots) {flag = 1; break;}
            current_ot = ot_data_arr[current_ot_index++];
        }

        // extreme situation - when we come to a new day and find that no OT is available
        if (flag) {
            for(i=0; i<len_surgeon_array; ++i) {
                if(!s_data_arr[i]->isNull) {
                    for (k=0; k<s_data_arr[i]->num_assigned_patients; ++k) {
                        temp_patient_id = s_data_arr[i]->assigned_patients[k];
                        insertNodeInPQ(pq, makeHeapNode(patients[temp_patient_id].mandatory, 0, temp_patient_id));
                    }
                }
            }
            continue; // go to the next day and then admit these patients
        }

        flag = 0;
        // admit the mandatory patients previously not admitted from the PQ
       if (d) current_ot = admitMandatoryFromPQ(pq, d, ot_data_arr, current_ot, current_ot_index);
        if (!current_ot) {
            for(i=0; i<len_surgeon_array; ++i)
                if(!s_data_arr[i]->isNull)
                    for (k=0; k<s_data_arr[i]->num_assigned_patients; ++k) {
                        temp_patient_id = s_data_arr[i]->assigned_patients[k];
                        insertNodeInPQ(pq, makeHeapNode(patients[temp_patient_id].mandatory, 0, temp_patient_id));
                    }
            continue; // go to the next day and then admit these patients
        }

        head = sorted_mandatory_patients[d];
        max_surgeon_id = find_max_surgeon_id(head);
        len_surgeon_array = max_surgeon_id + 1;
        // Added 1 to max_surgeon_id cuz we need space for the nth (max_is) also, so we'll have to make the total length (n+1)
        s_data_arr = (Surgeon_data **) calloc (len_surgeon_array, sizeof(Surgeon_data *)); 
        sort_s_data_arr(s_data_arr, len_surgeon_array, head);

        for(i=0; i<len_surgeon_array; ++i)
            if(!s_data_arr[i]->isNull) {
                if (!surgeon[s_data_arr[i]->surgeon_id].time_left[d]) {
                    // the surgeon is not available and hence add all the patients to the PQ
                    for (k=0; k<s_data_arr[i]->num_assigned_patients; ++k) {
                        temp_patient_id = s_data_arr[i]->assigned_patients[k];
                        insertNodeInPQ(pq, makeHeapNode(patients[temp_patient_id].mandatory, 0, temp_patient_id));
                    }
                    continue;
                }
                // if the control comes here, that means the surgeon has time on that day to operate patients
                for (j=0; j < s_data_arr[i]->num_assigned_patients; ++j) {
                    p_id = s_data_arr[i]->assigned_patients[j];
                    // s_id = s_data_arr[i]->surgeon_id;
                    s_duration = patients[p_id].surgery_duration;
                    // s_duration_sum = s_data_arr[i]->surgery_duration_sum;
                    if (patients[p_id].mandatory) {
                        // assign available OT
                        // first - select a suitable OT
                        while (current_ot->time_left[d] < s_duration) {
                                if (current_ot_index == num_ots) {flag = 1; break;}
                                current_ot = ot_data_arr[current_ot_index++];
                            }
                        if (flag) {
                            for (k=0; k<s_data_arr[i]->num_assigned_patients; ++k) {
                                temp_patient_id = s_data_arr[i]->assigned_patients[k];
                                insertNodeInPQ(pq, makeHeapNode(patients[temp_patient_id].mandatory, 0, temp_patient_id));
                            }
                            break;
                        }
                        if (s_duration <= current_ot->time_left[d]) { 
                            // no need to check now actually but check only to be safe
                            // assign OT--------------------------------------------------------------------------------------------------
                            patients[p_id].assigned_ot = current_ot->id;
                            // update the time left
                            current_ot->time_left -= s_duration;
                        }
                        // assign a room
                        r_id = patients[p_id].gen ? findSuitableRoom(p_id, gender_B_rooms) : findSuitableRoom(p_id, gender_A_rooms);
                        if (r_id == -1) {
                            // means there's no room in the gender_room_array and empty_array that is compatible with this patient
                            // so in this case - put the patient in the PQ and move on
                            insertNodeInPQ(pq, makeHeapNode(patients[p_id].mandatory, 0, p_id));
                            continue;
                        }
                        patients[p_id].assigned_room_no = r_id;
                        room[r_id].num_patients_allocated += 1;        
                        // assign admission day
                        patients[p_id].admission_day = d;
                    }
                    // the patient is optional - 
                    /*
                    Bcz we know that in the sorted_mandatory_patients array, first there are mandatory patients in the linked list and then
                    the optional ones are there. So the control will only come to this else block when all the mandatory patients have been
                    dealt with. 
                    Now we will try to admit these optional patients as well and if the conditions dont match then we'll put the into the PQ
                    for the next day.
                    The next day we'll try to 
                    1. first admit all the mandatory patients in the PQ.
                    2. Then we'll admit today's mandatory patients 
                    3. then yesterday's optional patients from the PQ and 
                    4. then today's optional patients.
                    */
                    else {
                        current_ot = admitOptionalFromPQ(pq, d, ot_data_arr, current_ot);
                        // admit today's optionals
                        // otherwise - put into the PQ
                        if (current_ot) {
                            // write code here for admitting today's optional patients
                            r_id = patients[p_id].gen ? findSuitableRoom(p_id, gender_B_rooms) : findSuitableRoom(p_id, gender_A_rooms);
                            if (r_id == -1) {
                                insertNodeInPQ(pq, makeHeapNode(patients[p_id].mandatory, 0, p_id));
                                continue;
                            }
                            patients[p_id].admission_day = d;
                            patients[p_id].assigned_room_no = r_id;
                            patients[p_id].assigned_ot = current_ot->id;
                        }
                        else {
                            for (k=0; k<s_data_arr[i]->num_assigned_patients; ++k) {
                                temp_patient_id = s_data_arr[i]->assigned_patients[k];
                                insertNodeInPQ(pq, makeHeapNode(patients[temp_patient_id].mandatory, 0, temp_patient_id));
                            }
                        }
                    }
                }
            }
       free_s_data_arr(s_data_arr, len_surgeon_array);
    }
}


/////////////////////////////////////////////////////////////////////////////////////////
// -----------------------------------------------FUNCTIONS BELOW ARE FOR NURSE ASSIGNMENTS--------------------------------------------------

//...............FUNCTION FOR CREATING THE 3-D ARRAY TO STORE THE PATIENTS IN THE ROOM ON A PARTICULAR DAY.........

void initialize_3d_array() {
    room_schedule = (Patient ****)calloc(num_rooms, sizeof(Patient ***));
    size_of_room_schedule = (int ***)calloc(num_rooms, sizeof(int **));
    
    for (int i = 0; i < num_rooms; i++) {
        room_schedule[i] = (Patient ***)calloc(days, sizeof(Patient **));
        size_of_room_schedule[i] = (int **)calloc(days, sizeof(int *));
        
        for (int j = 0; j < days; j++) {
            room_schedule[i][j] = (Patient **)calloc(10, sizeof(Patient *)); // Dynamic array for patients
            size_of_room_schedule[i][j] = (int *)calloc(1, sizeof(int)); // Track size of dynamic array
            *(size_of_room_schedule[i][j]) = 0; // Initialize size to 0
        }
    }
}

void create_3d_array(int day , int room_id , Patient *patient_ptr, int patient_id){
    initialize_3d_array();
    int i = 0;
    while( i < *size_of_room_schedule[room_id][day] ) i++;
    room_schedule[room_id][day][i] = patient_ptr ;

}

void free_3d_array() {
    if (room_schedule == NULL || size_of_room_schedule == NULL) {
        return; 
    }
    for (int i = 0; i < num_rooms; i++) {
        for (int j = 0; j < days; j++) {
            if (room_schedule[i][j] != NULL) {
                free(room_schedule[i][j]);
            }
            if (size_of_room_schedule[i][j] != NULL) {
                free(size_of_room_schedule[i][j]);
            }
        }
        if (room_schedule[i] != NULL) {
            free(room_schedule[i]);
        }
        if (size_of_room_schedule[i] != NULL) {
            free(size_of_room_schedule[i]);
        }
    }
    free(room_schedule);
    free(size_of_room_schedule);
}


void swap_nurse_pointers(Nurses **nurse1, Nurses **nurse2) {
    Nurses *temp = *nurse1;
    *nurse1 = *nurse2;
    *nurse2 = temp;
}

int partition(Nurses **arr, int low, int high) {
    int pivot = arr[high]->shift->max_load;										   
    int i = low - 1; 
    for (int j = low; j <= high - 1; j++) {

        if (arr[j]->shift->max_load > pivot) {
            i++;
            swap_nurse_pointers(&arr[i], &arr[j]);						 																					
        }
    }
    swap_nurse_pointers(&arr[i + 1], &arr[high]);
    return i + 1;					 
}

void quicksort(Nurses **arr, int low, int high) {
    if (low < high) {
        int pi = partition(arr, low, high);
        quicksort(arr, low, pi - 1);
        quicksort(arr, pi + 1, high);
    }					
}

void sorting_nurse_id_max_load() {
    for (int i = 0; i < 3 * days; i++) {
        if (current_size_dm_nurse[i] > 1) {
            quicksort(dm_nurses_availability[i], 0, current_size_dm_nurse[i] - 1);
        }											
    }									   
}

void allocate_dm_nurses_availability() {
    dm_nurses_availability = (Nurses ***)calloc(days * 3, sizeof(Nurses **));
    max_load_updated = (int **) calloc (num_nurses, (sizeof(int *)));
    if (!dm_nurses_availability) {
        perror("Failed to allocate memory for main array");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < days * 3; ++i) {
        dm_nurses_availability[i] = NULL;  // Initialize each slot to NULL
        max_load_updated[i] = (int *)calloc (days*3, sizeof(int));
        }
    if (!max_load_updated) {
        perror("Failed to allocate memory for max_load_updated");
        exit(EXIT_FAILURE);															   
    }
    for (int i = 0; i < num_nurses; ++i) {							   
        max_load_updated[i] = (int *)calloc(days * 3, sizeof(int));
        if (!max_load_updated[i]) {
            perror("Failed to allocate memory for max_load_updated row");
            exit(EXIT_FAILURE);
        }
        for (int j = 0; j < days * 3; ++j) {
            max_load_updated[i][j] = -1;							   
        }
    }
}

void allocate_sub_array(Nurses ***main_array, int index, int sub_size) {
    main_array[index] = (Nurses **)calloc(sub_size, sizeof(Nurses *));
    if (!main_array[index]) {
        perror("Failed to allocate memory for sub-array");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < sub_size; ++i) {
        main_array[index][i] = NULL; // Initialize subarray elements to NULL
    }
}

void append_to_sub_array(Nurses ***main_array, int index, int *current_sizes, Nurses *nurse) {
    int current_size = current_sizes[index];
    int new_size = current_size + 1;
    main_array[index] = (Nurses **)realloc(main_array[index], new_size * sizeof(Nurses *));
    if (!main_array[index]) {
        perror("Failed to reallocate memory for sub-array");
        exit(EXIT_FAILURE);
    }
    main_array[index][current_size] = nurse; // Add pointer to Nurse structure
    current_sizes[index] = new_size;        // Update current size
}

void initialize_current_size_dm_nurse() {
    current_size_dm_nurse = (int *)calloc(days * 3, sizeof(int));
    if (!current_size_dm_nurse) {
        perror("Failed to allocate memory for current_size_dm_nurse");
        exit(EXIT_FAILURE);										
    }
}

void create_dm_nurses_availability() {
    allocate_dm_nurses_availability();
    initialize_current_size_dm_nurse();
    for (int i = 0; i < 3 * days; i++) {
        allocate_sub_array(dm_nurses_availability, i, 2); // Initial subarray size												   
    }
    for (int i = 0; i < num_nurses; i++) {
        for (int j = 0; j < nurses[i].num_shifts; j++) {
            int index = 3 * nurses[i].shift[j].day + nurses[i].shift[j].shift_time;
            if (index < 0 || index >= 3 * days) {
                fprintf(stderr, "\nInvalid index: %d\n", index);
                continue;
            }
            append_to_sub_array(dm_nurses_availability, index, current_size_dm_nurse, &nurses[i]);
            max_load_updated[i][index] = nurses[i].shift[j].max_load;
            //printf("%d\n" , max_load_updated[i][index]);
        }
    }
}

void free_all_dm_nurses(Nurses ***main_array, int main_size, int *current_sizes) {
    for (int i = 0; i < main_size; ++i) {
        if (main_array[i]) {
            free(main_array[i]); // Free each subarray
        }  
    }
    free(main_array);        // Free main array
    free(current_sizes);     // Free size tracking array
}

void print_dm_nurses() {
    for (int i = 0; i < 3 * days; i++) {
        printf("......Shift %d.......\n", i);
        for (int j = 0; j < current_size_dm_nurse[i]; j++) {
            if (dm_nurses_availability[i][j]) {
                printf("id: %d\t max_load: %d\t", dm_nurses_availability[i][j]->id , dm_nurses_availability[i][j]->shift->max_load);
            } else {
                printf("NULL\t");
            }		  
        }
        printf("\n");
    }
}

// ---------------------------------------------function for creating rooms_requirement---------------------------------------------------
void initialize_rooms_req(int num_rooms) {
    rooms_requirement = (Rooms_req **)calloc(num_rooms, sizeof(Rooms_req *));
    if (rooms_requirement == NULL) {
        perror("Failed to allocate memory for rooms_requirement");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < num_rooms; i++) {
        rooms_requirement[i] = (Rooms_req *)calloc(3*days, sizeof(Rooms_req));
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

void create_rooms_req(){
    for(int i = 0 ; i < num_rooms ; i++){
        for(int j = 0 ; j < days ; j++){
            for(int x = 0 ; x < 3 ; x++){
                int sum = 0;
                int max_skill = 0;
                for(int k = 0 ; k < *size_of_room_schedule[i][j] ; k++){
                    Patient * temp =  room_schedule[i][j][k];
                    sum += temp->workload_produced[3*j+x];
                    max_skill = (max_skill > temp->skill_level_required[3 * j + x]) 
                                        ? max_skill 
                                        : temp->skill_level_required[3 * j + x];
                }
                rooms_requirement[i][3*j+x].load_sum = sum;
                rooms_requirement[i][3*j+x].max_skill_req = max_skill;
            }
        }
    }
}

void cleanup_rooms_req(int num_rooms) {
    for (int i = 0; i < num_rooms; i++) {
        free(rooms_requirement[i]); // Free each row
    }
    free(rooms_requirement); // Free the array of rows
}

void nurse_assignments() {
    for (int i = 0; i < days; i++) {
        for (int j = 0; j < num_rooms; j++) {
            for (int k = 0; k < 3; k++) {
                int m = 0;
                Nurses **arr = dm_nurses_availability[i * 3 + k];
                int arr_size = current_size_dm_nurse[i * 3 + k]; 
                
                if (room[j].nurses_alloted == NULL) {
                    room[j].nurses_alloted = (int *)calloc(1, sizeof(int));
                    room[j].length_of_nurses_alloted = 0;
                }
                
                Nurses *nurse_to_be_assigned = NULL;
                
                /*conditions check to assign the nurse
                1. if the max_load is not exhausted
                2. if the skill_level_required is matching the skill_level_required for jth room
                3. also remaining_load should be greater than or equal to the load_sum for that room in the shift 3*i + k.
                4. ignoring continuity of care 
                5. going for 0 scenario penalty for constraint s2 and s4.
                6. ignoring continuity of care
                */
                while (m < arr_size) {
                    nurse_to_be_assigned = arr[m];
                    int max_load_of_nurse_to_be_assigned = max_load_updated[nurse_to_be_assigned->id][3 * i + k];
                    
                    if (max_load_of_nurse_to_be_assigned != -1 &&
                        max_load_of_nurse_to_be_assigned >= rooms_requirement[j][3*i + k].load_sum &&
                        nurse_to_be_assigned->skill_level >= rooms_requirement[j][3*i + k].max_skill_req) {
                        break; 
                    }
                    m++;
                }
                
                // If a valid nurse is found, assign to the room
                if (m < arr_size && nurse_to_be_assigned != NULL) {
                    room[j].nurses_alloted = (int *)realloc(
                        room[j].nurses_alloted,
                        (room[j].length_of_nurses_alloted + 1) * sizeof(int));
                    
                    if (room[j].nurses_alloted == NULL) {
                        perror("Memory allocation failed for nurses_alloted");
                        exit(EXIT_FAILURE);
                    }
                    room[j].nurses_alloted = (int *)realloc(room[j].nurses_alloted, 2 * sizeof(int));
                    room[j].nurses_alloted[room[j].length_of_nurses_alloted] = nurse_to_be_assigned->id;
                    room[j].length_of_nurses_alloted++;
                    
                    max_load_updated[nurse_to_be_assigned->id][3 * i + k] -= rooms_requirement[j]->load_sum;
                } else {
                    fprintf(stderr, "No valid nurse available for Room %d, Shift %d\n", j, 3 * i + k);
                }
            }
        }
    }
}																																	

void print_max_loadd_updated(){
    for(int i = 0 ; i < num_nurses ; i++){
        printf("Max_load of the nurse %d\n" , i);
        for(int j = 0 ; j < 3 * days ; j++){
            printf("%d\t", max_load_updated[i][j]);
        }
        printf("\n");
    }
}

//..................................FUNCTION FOR OUTPUT JSON FILE....................
void create_json_file(Patient *patients, int num_patients, Nurses *nurse, int num_nurses) {
    // Open file in write mode
    FILE *file = fopen("output.json", "w");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }
    // Add patients array
    fprintf(file, "  \"patients\": [\n");
    for (int i = 0; i < num_patients; i++) {
        fprintf(file, "    {\n");
        fprintf(file, "      \"id\": \"%d\",\n", patients[i].id);
        fprintf(file, "      \"admission_day\": %d,\n", patients[i].admission_day);
        fprintf(file, "      \"room\": \"r%d\",\n", patients[i].assigned_room_no);  // Add "r" prefix to room ID
        fprintf(file, "      \"operating_theater\": \"t%d\"\n", patients[i].assigned_ot); // Add "t" prefix to OT ID
        fprintf(file, "    }");
        if (i < num_patients - 1) {
            fprintf(file, ",");
        }
        fprintf(file, "\n");
    }
    fprintf(file, "  ],\n");
    // Add nurses array
    fprintf(file, "  \"nurses\": [\n");
    for (int i = 0; i < num_nurses; i++) {
        fprintf(file, "    {\n");
        fprintf(file, "      \"id\": \"n%d\",\n", nurses[i].id);
        fprintf(file, "      \"skill_level\": %d,\n", nurses[i].skill_level);
        fprintf(file, "      \"assignments\": [\n");

        // Shifts for each nurse
        for (int j = 0; j < nurses[i].num_shifts; j++) {
            fprintf(file, "        {\n");
            fprintf(file, "          \"day\": %d,\n", nurses[i].shift[j].day);
            // Convert shift_time enum to string representation
            const char *shift_name = shift_types_to_string(nurses[i].shift[j].shift_time);
            fprintf(file, "          \"shift\": \"%s\",\n", shift_name);
            fprintf(file, "          \"rooms\": [\n");
            for (int k = 0; k < nurses[i].shift[j].num_rooms; k++) {
                fprintf(file, "            \"r%d\"%s\n", nurses[i].shift[j].rooms[k],
                        (k < nurses[i].shift[j].num_rooms - 1) ? "," : "");
            }
            fprintf(file, "          ]\n");
            fprintf(file, "        }%s\n", (j < nurses[i].num_shifts - 1) ? "," : "");
        }
        fprintf(file, "      ]\n");
        fprintf(file, "    }%s\n", (i < num_nurses - 1) ? "," : "");
    }
    fprintf(file, "}\n");
    fprintf(file, "}\n");
    // Close the file
    fclose(file);
    printf("JSON file 'output.json' created successfully.\n");
}

int main (void) {
    
    parse_json("C:/sant_saran/C in One/IHTC/New folder/sol_test09.json");
    PriorityQueue *pq;
    
    initialize_room_gender_map(&room_gender_map);
    
    pq = (PriorityQueue *) calloc (1, sizeof(PriorityQueue));
    initPQ(pq, 20); // InitalCapacity = 20. It'll resizePQ itself if we try to insert more HeapNodes in it. 
    populate_room_gender_map(room_gender_map);
    // printf("\nroom: 6\tgender: %d", room_gender_map[6]);
    //print_map(room_gender_map);
    
    sort_mandatory_patients_on_release_day(mandatory_patients, mandatory_count);
    sort_mandatory_patients_on_LOS();
    //print_sorted_mandatory_array();
    sort_optional_patients_on_release_day(optional_patients, optional_count);
    append_optional_to_mandatory(sorted_mandatory_patients, sorted_optional_patients);
    //print_sorted_optional_array();
    create_dm_nurses_availability();
    sorting_nurse_id_max_load();
    admit_patients(room_gender_map, pq);
    //nurse_assignments();
    
    //print_dm_nurses();
    
   // print_mandatory_patients();
     //print_sorted_mandatory_array();
     //print_sorted_mandatory_patients();
    //create_json_file(patients , num_patients , nurses , num_nurses);
    // Use the parsed data in your algorithm
    // int *surgery_time[num_surgeons][days];
    // printf("Weights:\n");
    // printf("Room Mixed Age: %d\n", weights->room_mixed_age);
    // printf("Room Nurse Skill: %d\n", weights->room_nurse_skill);
    // printf("Continuity of Care: %d\n", weights->continuity_of_care);
    // printf("Nurse Excessive Workload: %d\n", weights->nurse_excessive_workload);
    // printf("Open Operating Theater: %d\n", weights->open_operating_theater);
    // printf("Surgeon Transfer: %d\n", weights->surgeon_transfer);
    // printf("Patient Delay: %d\n", weights->patient_delay);
    // printf("Unscheduled Optional: %d\n", weights->unscheduled_optional);

    // print_surgeons(surgeon);
    //print_ots(ot);
    // print_rooms();
    // print_nurses();
    // print_patients(patients);
    // print_occupants();
    // print_mandatory_patients();
    // print_optional_patients();


    //Free allocated memory
   // free_patients_sorted_array(sorted_mandatory_patients);
    //free_patients_sorted_array(sorted_optional_patients);
    free_occupants();
    free_patients();
    free_surgeons();
    free_ots();
    free_rooms();
    free_nurses();
    free(weights);		
    return 0;																			
}


/*
UPDATES SECTION: Here we will put all the updates related to this program. Whether it's about the heuristic or anything else.
1. Wrote code for admit_patients function and some utility functions
2. Working on: written in admit_patients function
3. Surgeons sorting function needs updation - we have to account for null values as well.
*/