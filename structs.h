#ifndef STRUCTS_H
#define STRUCTS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_STR 100
#define MAX_TEXT 300
#define ID_LEN 32

#define FILE_USERS        "users.dat"
#define FILE_PATIENTS     "patients.dat"
#define FILE_DOCTORS      "doctors.dat"
#define FILE_WARDS        "wards.dat"
#define FILE_APPOINTMENTS "appointments.dat"
#define FILE_QUEUE        "queue.dat"
#define FILE_RECORDS      "records.dat"
#define FILE_COUNTER      "counter.dat"
#define FILE_BILLS        "bills.dat"

typedef enum {
    ROLE_ADMIN = 1,
    ROLE_RECEPTIONIST = 2,
    ROLE_DOCTOR = 3
} UserRole;

typedef enum {
    WARD_GENERAL = 1,
    WARD_CABIN = 2,
    WARD_ICU = 3
} WardType;

typedef enum {
    REC_NOTE = 1,
    REC_PRESCRIPTION = 2,
    REC_TEST = 3,
    REC_REFERRAL = 4
} RecordType;

typedef struct {
    char id[ID_LEN];
    char username[50];
    char password[50];
    char fullName[MAX_STR];
    UserRole role;
    int active;
} User;

typedef struct {
    char id[ID_LEN];
    char name[MAX_STR];
    int age;
    char gender[16];
    char phone[24];
    char bloodGroup[8];
    char address[MAX_TEXT];
    char assignedDoctorId[ID_LEN];
    char referredDoctorId[ID_LEN];
    char assignedBedId[ID_LEN];
    int isAdmitted;
    int isArchived;
    double dueAmount;
} Patient;

typedef struct {
    char id[ID_LEN];
    char name[MAX_STR];
    char specialization[MAX_STR];
    char visitingHours[64];
    double consultationFee;
    double avgConsultMinutes;
    int completedConsults;
} Doctor;

typedef struct {
    char bedId[ID_LEN];
    char wardName[MAX_STR];
    WardType type;
    double dailyRate;
    int isOccupied;
    char patientId[ID_LEN];
} Bed;

typedef struct {
    char apptId[ID_LEN];
    char patientId[ID_LEN];
    char doctorId[ID_LEN];
    char date[16];
    char timeSlot[16];
    int isEmergency;
    int tokenNumber;
    char status[24];
} Appointment;

typedef struct {
    char date[16];
    char doctorId[ID_LEN];
    int tokenNumber;
    char patientId[ID_LEN];
    int isEmergency;
    int estimatedWaitMins;
    int status;
} QueueEntry;

typedef struct {
    char recordId[ID_LEN];
    char patientId[ID_LEN];
    char doctorId[ID_LEN];
    RecordType type;
    char details[MAX_TEXT];
    double charge;
    char timestamp[32];
} RecordEntry;

typedef struct {
    char billId[ID_LEN];
    char patientId[ID_LEN];
    int admissionDays;
    double bedCharge;
    double doctorCharge;
    double medicineCharge;
    double testCharge;
    double totalAmount;
    int isPaid;
    char generatedAt[32];
} Bill;

typedef struct {
    int lastPatientNumber;
} IdCounter;

#endif
