#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include "cJSON.h"

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
    } while (0)

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
    int num_nurses_alloted;
    int* nurses_alloted;
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
    int* nurses_alloted;
    int length_of_nurses_alloted;
    gender gen;
} Rooms;

typedef struct RoomsList {
    Rooms* pointer;
    struct RoomsList* next;
    struct RoomsList* prev;
} GenderRoom;

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

typedef struct {
    int patient_id;
    int mandatory;
    int due_day;
    int delay; // Lower is better
    int los;
} HeapNode;

typedef struct {
    HeapNode* data;
    int current_size;
    int capacity;
} PriorityQueue;

typedef struct LL {
    Patient* pointer;
    struct LL* next, * prev;
} Node;

typedef struct {
    Rooms** data;
    int size;
    int capacity;
} RoomVector;

typedef struct {
    int index;
    int* chromosome;
    PriorityQueue* pq;
} ThreadArgs;

// Global variables declaration
extern int days;
extern int skill_levels;
extern int num_occupants;
extern int num_patients;
extern int num_surgeons;
extern int num_ots;
extern int num_rooms;
extern int num_nurses;
extern int mandatory_count;
extern int optional_count;
extern int current_ot_index;
extern int num_gender_A_rooms, num_gender_B_rooms, num_empty_rooms;
extern int* current_size_dm_nurse;
extern int* room_gender_map;
extern int** size_of_room_schedule;

extern Nurses* nurses;
extern Nurses*** dm_nurses_availability;
extern int** max_load_updated;
extern Weights* weights;
extern Occupants* occupants;
extern Patient* patients;
extern Patient** mandatory_patients;
extern Patient** optional_patients;
extern char**** room_schedule;
extern Surgeon* surgeon;
extern OTs* ot;
extern Rooms* room;
extern GenderRoom* gender_A_rooms;
extern GenderRoom* gender_B_rooms;
extern GenderRoom* empty_rooms;
extern RoomVector* v_A, * v_B, * v_empty;
extern Rooms_req** rooms_requirement;

// Function prototypes
const char* gender_to_string(gender g);
const char* shift_types_to_string(shift_types shift);
gender string_to_gender(const char* str);
shift_types string_to_shift_types(const char* str);
int str2int(char* a);

GenderRoom* appendGenderRoomNode(GenderRoom*, GenderRoom*);
GenderRoom* makeGenderRoomNode(Rooms*);
void free_s_data_arr(Surgeon_data**, int);
void update_LOS_of_patients(int);
int findSuitableRoom(int, RoomVector*);
void sort_s_data_arr(Surgeon_data**, int, Node*);
void sort_ot_data_arr(OTs**, int);
int find_max_surgeon_id(Node*);
GenderRoom* deleteGenderRoomNode(GenderRoom*, GenderRoom*);
GenderRoom* removeAndAppendGenderRoom(int, GenderRoom*);
void admit_patients(int*, PriorityQueue*);
void init_Rooms(RoomVector* vector, int initialcap);
void pushback(RoomVector* vector, Rooms* room);
Rooms* removebyid(RoomVector* vector, int room_id);
void moveRoom(RoomVector* source, RoomVector* destination, int room_id);
void freevector(RoomVector* vector);

void initPQ(PriorityQueue* pq, int initialCapacity);
void insertNodeInPQ(PriorityQueue* pq, HeapNode node);
HeapNode extractMaxFromPQ(PriorityQueue* pq);
HeapNode peekMaxFromPQ(PriorityQueue* pq);
int isEmptyPQ(PriorityQueue* pq);
void heapifyUp(PriorityQueue* pq, int index);
void heapifyDown(PriorityQueue* pq, int index);
int compareNodesForPQ(HeapNode a, HeapNode b);
void resizePQ(PriorityQueue* pq);
void freePQ(PriorityQueue* pq);
