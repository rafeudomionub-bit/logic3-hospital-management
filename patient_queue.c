#include "structs.h"

static void read_line(char *buf, int size) {
    if (fgets(buf, size, stdin) != NULL) {
        buf[strcspn(buf, "\r\n")] = '\0';
    } else {
        buf[0] = '\0';
    }
}

static int read_int_pq(const char *prompt, int min_val, int max_val) {
    int val;
    char line[64];
    while (1) {
        printf("%s", prompt);
        read_line(line, sizeof(line));
        if (sscanf(line, "%d", &val) == 1 && val >= min_val && val <= max_val) {
            return val;
        }
        printf("Invalid input! Enter a number between %d and %d.\n", min_val, max_val);
    }
}

static double read_double_pq(const char *prompt, double min_val) {
    double val;
    char line[64];
    while (1) {
        printf("%s", prompt);
        read_line(line, sizeof(line));
        if (sscanf(line, "%lf", &val) == 1 && val >= min_val) {
            return val;
        }
        printf("Invalid value! Enter >= %.2f.\n", min_val);
    }
}

static void get_today_str(char *buf, int size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, size, "%Y-%m-%d", t);
}

static void get_timestamp_str(char *buf, int size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", t);
}

/* Generates a never-repeating patient ID like PT-001001 */
static void generate_next_patient_id(char *outId, int size) {
    IdCounter c;
    c.lastPatientNumber = 1000;

    FILE *fp = fopen(FILE_COUNTER, "rb");
    if (fp != NULL) {
        if (fread(&c, sizeof(IdCounter), 1, fp) != 1) {
            c.lastPatientNumber = 1000;
        }
        fclose(fp);
    }

    c.lastPatientNumber++;
    snprintf(outId, size, "PT-%06d", c.lastPatientNumber);

    fp = fopen(FILE_COUNTER, "wb");
    if (fp != NULL) {
        fwrite(&c, sizeof(IdCounter), 1, fp);
        fclose(fp);
    }
}

static int doctor_exists(const char *docId, Doctor *outDoc) {
    FILE *fp = fopen(FILE_DOCTORS, "rb");
    if (fp == NULL) return 0;

    Doctor d;
    while (fread(&d, sizeof(Doctor), 1, fp) == 1) {
        if (strcmp(d.id, docId) == 0) {
            if (outDoc != NULL) *outDoc = d;
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

static int find_patient_by_id(const char *patId, Patient *outPat) {
    FILE *fp = fopen(FILE_PATIENTS, "rb");
    if (fp == NULL) return 0;

    Patient p;
    while (fread(&p, sizeof(Patient), 1, fp) == 1) {
        if (strcmp(p.id, patId) == 0) {
            if (outPat != NULL) *outPat = p;
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

/* Feature 5: Patient Registration */
void register_patient(void) {
    Patient p;
    memset(&p, 0, sizeof(Patient));

    generate_next_patient_id(p.id, sizeof(p.id));

    printf("\n--- Register New Patient ---\n");
    printf("Generated Unique UPP ID: %s\n", p.id);
    printf("Patient Name: ");
    read_line(p.name, sizeof(p.name));
    p.age = read_int_pq("Age: ", 0, 130);
    printf("Gender (Male/Female/Other): ");
    read_line(p.gender, sizeof(p.gender));
    printf("Phone: ");
    read_line(p.phone, sizeof(p.phone));
    printf("Blood Group (e.g., O+, A-): ");
    read_line(p.bloodGroup, sizeof(p.bloodGroup));
    printf("Address: ");
    read_line(p.address, sizeof(p.address));

    p.assignedDoctorId[0] = '\0';
    p.referredDoctorId[0] = '\0';
    p.assignedBedId[0] = '\0';
    p.isAdmitted = 0;
    p.isArchived = 0;
    p.dueAmount = 0.0;

    FILE *fp = fopen(FILE_PATIENTS, "ab");
    if (fp == NULL) {
        printf("Error saving patient to %s!\n", FILE_PATIENTS);
        return;
    }
    fwrite(&p, sizeof(Patient), 1, fp);
    fclose(fp);

    printf("Patient registered successfully! ID: %s\n", p.id);
}

/* Feature 6: Patient Search (by ID or Name) */
void search_patient(void) {
    char query[MAX_STR];
    printf("\nEnter Patient ID or Name to search: ");
    read_line(query, sizeof(query));

    FILE *fp = fopen(FILE_PATIENTS, "rb");
    if (fp == NULL) {
        printf("No patient records found.\n");
        return;
    }

    Patient p;
    int found = 0;
    printf("\n%-12s %-20s %-5s %-15s %-10s %-10s\n",
           "ID", "Name", "Age", "Phone", "Admitted", "Archived");
    printf("-------------------------------------------------------------------------------\n");
    while (fread(&p, sizeof(Patient), 1, fp) == 1) {
        if (strcmp(p.id, query) == 0 || strstr(p.name, query) != NULL) {
            printf("%-12s %-20s %-5d %-15s %-10s %-10s\n",
                   p.id, p.name, p.age, p.phone,
                   p.isAdmitted ? "Yes" : "No",
                   p.isArchived ? "Yes" : "No");
            found = 1;
        }
    }
    fclose(fp);

    if (!found) {
        printf("No matching patient found for '%s'.\n", query);
    }
}

/* Helper to calculate estimated wait time for a doctor on a given date */
static int calculate_wait_time_mins(const char *docId, const char *dateStr, int isEmergency) {
    Doctor doc;
    double avgMin = 10.0;
    if (doctor_exists(docId, &doc) && doc.avgConsultMinutes > 0.0) {
        avgMin = doc.avgConsultMinutes;
    }

    FILE *fp = fopen(FILE_QUEUE, "rb");
    if (fp == NULL) return 0;

    QueueEntry q;
    int patientsAhead = 0;
    int hasServing = 0;

    while (fread(&q, sizeof(QueueEntry), 1, fp) == 1) {
        if (strcmp(q.doctorId, docId) == 0 && strcmp(q.date, dateStr) == 0) {
            if (q.status == 1) {
                hasServing = 1;
            } else if (q.status == 0) {
                if (isEmergency) {
                    if (q.isEmergency) patientsAhead++;
                } else {
                    patientsAhead++;
                }
            }
        }
    }
    fclose(fp);

    int currentRemaining = hasServing ? (int)(avgMin / 2.0) : 0;
    return (int)(patientsAhead * avgMin) + currentRemaining;
}

/* Feature 7 & 8: Book Appointment + Smart Queue Token & Slip */
void book_appointment_and_queue(void) {
    char patId[ID_LEN], docId[ID_LEN], dateStr[16], slot[16];
    Patient pat;
    Doctor doc;

    printf("\n--- Book Appointment & Issue Token ---\n");
    printf("Patient ID: ");
    read_line(patId, sizeof(patId));
    if (!find_patient_by_id(patId, &pat)) {
        printf("Patient '%s' not found!\n", patId);
        return;
    }
    if (pat.isArchived) {
        printf("Warning: Patient '%s' is archived. Unarchive or register new if needed.\n", patId);
    }

    printf("Doctor ID: ");
    read_line(docId, sizeof(docId));
    if (!doctor_exists(docId, &doc)) {
        printf("Doctor '%s' not found!\n", docId);
        return;
    }

    get_today_str(dateStr, sizeof(dateStr));
    printf("Date (YYYY-MM-DD) [Enter for today %s]: ", dateStr);
    char inputDate[16];
    read_line(inputDate, sizeof(inputDate));
    if (strlen(inputDate) > 0) {
        strncpy(dateStr, inputDate, sizeof(dateStr) - 1);
        dateStr[sizeof(dateStr) - 1] = '\0';
    }

    printf("Time Slot (e.g., 10:30): ");
    read_line(slot, sizeof(slot));

    /* Check slot conflict for same doctor, date, and timeSlot */
    FILE *afp = fopen(FILE_APPOINTMENTS, "rb");
    if (afp != NULL) {
        Appointment existing;
        while (fread(&existing, sizeof(Appointment), 1, afp) == 1) {
            if (strcmp(existing.doctorId, docId) == 0 &&
                strcmp(existing.date, dateStr) == 0 &&
                strcmp(existing.timeSlot, slot) == 0 &&
                strcmp(existing.status, "CANCELLED") != 0) {
                fclose(afp);
                printf("Conflict! Doctor %s already has an appointment on %s at %s.\n",
                       docId, dateStr, slot);
                return;
            }
        }
        fclose(afp);
    }

    int isEmergency = read_int_pq("Is this an Emergency? (1=Yes, 0=No): ", 0, 1);

    /* Next daily token number for this doctor and date */
    int nextToken = 1;
    FILE *qfp = fopen(FILE_QUEUE, "rb");
    if (qfp != NULL) {
        QueueEntry q;
        while (fread(&q, sizeof(QueueEntry), 1, qfp) == 1) {
            if (strcmp(q.doctorId, docId) == 0 && strcmp(q.date, dateStr) == 0) {
                if (q.tokenNumber >= nextToken) {
                    nextToken = q.tokenNumber + 1;
                }
            }
        }
        fclose(qfp);
    }

    int estWait = calculate_wait_time_mins(docId, dateStr, isEmergency);

    Appointment appt;
    memset(&appt, 0, sizeof(Appointment));
    snprintf(appt.apptId, sizeof(appt.apptId), "APT-%s-%03d", dateStr, nextToken);
    strcpy(appt.patientId, patId);
    strcpy(appt.doctorId, docId);
    strcpy(appt.date, dateStr);
    strcpy(appt.timeSlot, slot);
    appt.isEmergency = isEmergency;
    appt.tokenNumber = nextToken;
    strcpy(appt.status, "PENDING");

    afp = fopen(FILE_APPOINTMENTS, "ab");
    if (afp != NULL) {
        fwrite(&appt, sizeof(Appointment), 1, afp);
        fclose(afp);
    }

    QueueEntry qe;
    memset(&qe, 0, sizeof(QueueEntry));
    strcpy(qe.date, dateStr);
    strcpy(qe.doctorId, docId);
    qe.tokenNumber = nextToken;
    strcpy(qe.patientId, patId);
    qe.isEmergency = isEmergency;
    qe.estimatedWaitMins = estWait;
    qe.status = 0; /* waiting */

    qfp = fopen(FILE_QUEUE, "ab");
    if (qfp != NULL) {
        fwrite(&qe, sizeof(QueueEntry), 1, qfp);
        fclose(qfp);
    }

    /* Update patient's assigned doctor */
    FILE *pfp = fopen(FILE_PATIENTS, "r+b");
    if (pfp != NULL) {
        Patient p;
        while (fread(&p, sizeof(Patient), 1, pfp) == 1) {
            if (strcmp(p.id, patId) == 0) {
                strcpy(p.assignedDoctorId, docId);
                fseek(pfp, -(long)sizeof(Patient), SEEK_CUR);
                fwrite(&p, sizeof(Patient), 1, pfp);
                break;
            }
        }
        fclose(pfp);
    }

    /* Print Appointment & Queue Slip */
    printf("\n========================================\n");
    printf("        APPOINTMENT & QUEUE SLIP        \n");
    printf("========================================\n");
    printf("Appointment ID : %s\n", appt.apptId);
    printf("Patient        : %s (%s)\n", pat.name, pat.id);
    printf("Doctor         : %s (%s)\n", doc.name, doc.id);
    printf("Date & Slot    : %s at %s\n", appt.date, appt.timeSlot);
    printf("Priority       : %s\n", isEmergency ? "EMERGENCY" : "Normal");
    printf("Token Serial   : #%d\n", appt.tokenNumber);
    printf("Est. Wait Time : ~%d minutes\n", estWait);
    printf("========================================\n");
}

/* Call next patient from queue (emergency first, then token order) */
void call_next_patient_in_queue(const char *doctorId) {
    char today[16];
    get_today_str(today, sizeof(today));

    FILE *fp = fopen(FILE_QUEUE, "r+b");
    if (fp == NULL) {
        printf("Queue file not found.\n");
        return;
    }

    QueueEntry entries[300];
    int count = 0;
    while (count < 300 && fread(&entries[count], sizeof(QueueEntry), 1, fp) == 1) {
        count++;
    }

    /* Mark currently serving (status == 1) for this doctor as completed (status == 2) */
    int completedCount = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].doctorId, doctorId) == 0 &&
            strcmp(entries[i].date, today) == 0 &&
            entries[i].status == 1) {
            entries[i].status = 2;
            completedCount++;
        }
    }

    /* Find highest-priority waiting entry (status == 0): emergency first, then lowest tokenNumber */
    int bestIdx = -1;
    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].doctorId, doctorId) == 0 &&
            strcmp(entries[i].date, today) == 0 &&
            entries[i].status == 0) {
            if (bestIdx == -1) {
                bestIdx = i;
            } else {
                if (entries[i].isEmergency && !entries[bestIdx].isEmergency) {
                    bestIdx = i;
                } else if (entries[i].isEmergency == entries[bestIdx].isEmergency &&
                           entries[i].tokenNumber < entries[bestIdx].tokenNumber) {
                    bestIdx = i;
                }
            }
        }
    }

    if (bestIdx != -1) {
        entries[bestIdx].status = 1;
        printf("\nNow Serving Token #%d | Patient ID: %s %s\n",
               entries[bestIdx].tokenNumber,
               entries[bestIdx].patientId,
               entries[bestIdx].isEmergency ? "[EMERGENCY]" : "");
    } else {
        printf("\nNo waiting patients left in queue for Doctor %s today.\n", doctorId);
    }

    rewind(fp);
    fwrite(entries, sizeof(QueueEntry), count, fp);
    fclose(fp);

    /* Update moving average consult duration if a consultation completed */
    if (completedCount > 0) {
        int actualMins = read_int_pq("Enter actual consultation time in minutes (e.g., 10): ", 1, 180);
        FILE *dfp = fopen(FILE_DOCTORS, "r+b");
        if (dfp != NULL) {
            Doctor d;
            while (fread(&d, sizeof(Doctor), 1, dfp) == 1) {
                if (strcmp(d.id, doctorId) == 0) {
                    if (d.avgConsultMinutes <= 0.0) d.avgConsultMinutes = (double)actualMins;
                    d.avgConsultMinutes = (d.avgConsultMinutes * 0.7) + ((double)actualMins * 0.3);
                    d.completedConsults++;
                    fseek(dfp, -(long)sizeof(Doctor), SEEK_CUR);
                    fwrite(&d, sizeof(Doctor), 1, dfp);
                    break;
                }
            }
            fclose(dfp);
        }
    }
}

/* Feature 10: Appointment List */
void view_appointments(void) {
    FILE *fp = fopen(FILE_APPOINTMENTS, "rb");
    if (fp == NULL) {
        printf("\nNo appointments found.\n");
        return;
    }

    Appointment a;
    printf("\n%-16s %-10s %-8s %-11s %-6s %-6s %-10s\n",
           "Appt ID", "Patient", "Doctor", "Date", "Time", "Token", "Status");
    printf("-----------------------------------------------------------------------------\n");
    while (fread(&a, sizeof(Appointment), 1, fp) == 1) {
        printf("%-16s %-10s %-8s %-11s %-6s #%-5d %-10s\n",
               a.apptId, a.patientId, a.doctorId, a.date,
               a.timeSlot, a.tokenNumber, a.status);
    }
    fclose(fp);
}

/* Feature 11: Unified Patient Profile (Append-only records & role/referral access) */
void add_upp_record(const char *doctorId) {
    char patId[ID_LEN];
    Patient pat;
    printf("\n--- Add Unified Patient Profile (UPP) Entry ---\n");
    printf("Patient ID: ");
    read_line(patId, sizeof(patId));
    if (!find_patient_by_id(patId, &pat)) {
        printf("Patient '%s' not found!\n", patId);
        return;
    }

    RecordEntry rec;
    memset(&rec, 0, sizeof(RecordEntry));
    snprintf(rec.recordId, sizeof(rec.recordId), "REC-%ld", (long)time(NULL));
    strcpy(rec.patientId, patId);
    strcpy(rec.doctorId, doctorId);
    rec.type = (RecordType)read_int_pq(
        "Type (1=Clinical Note, 2=Prescription, 3=Test Report, 4=Referral): ", 1, 4);

    printf("Details (note/medicine/test/referral info): ");
    read_line(rec.details, sizeof(rec.details));

    if (rec.type == REC_PRESCRIPTION || rec.type == REC_TEST) {
        rec.charge = read_double_pq("Item Charge/Price: ", 0.0);
    } else {
        rec.charge = 0.0;
    }

    get_timestamp_str(rec.timestamp, sizeof(rec.timestamp));

    /* Append only - never overwrite or delete existing UPP entries */
    FILE *fp = fopen(FILE_RECORDS, "ab");
    if (fp == NULL) {
        printf("Error writing to %s!\n", FILE_RECORDS);
        return;
    }
    fwrite(&rec, sizeof(RecordEntry), 1, fp);
    fclose(fp);

    /* If referral, update patient's referredDoctorId so referred doctor gets full access */
    if (rec.type == REC_REFERRAL) {
        char refDocId[ID_LEN];
        printf("Referred Doctor ID: ");
        read_line(refDocId, sizeof(refDocId));
        FILE *pfp = fopen(FILE_PATIENTS, "r+b");
        if (pfp != NULL) {
            Patient p;
            while (fread(&p, sizeof(Patient), 1, pfp) == 1) {
                if (strcmp(p.id, patId) == 0) {
                    strcpy(p.referredDoctorId, refDocId);
                    fseek(pfp, -(long)sizeof(Patient), SEEK_CUR);
                    fwrite(&p, sizeof(Patient), 1, pfp);
                    break;
                }
            }
            fclose(pfp);
        }
    }

    printf("UPP record appended (%s). Medical history preserved.\n", rec.recordId);
}

void view_upp_profile(UserRole role, const char *userId) {
    char patId[ID_LEN];
    Patient pat;
    printf("\nEnter Patient ID to view profile: ");
    read_line(patId, sizeof(patId));
    if (!find_patient_by_id(patId, &pat)) {
        printf("Patient '%s' not found!\n", patId);
        return;
    }

    printf("\n=== PATIENT PROFILE: %s (%s) ===\n", pat.name, pat.id);
    printf("Age: %d | Gender: %s | Blood: %s | Phone: %s\n",
           pat.age, pat.gender, pat.bloodGroup, pat.phone);
    printf("Address: %s\n", pat.address);

    /* Receptionists see basic details only */
    if (role == ROLE_RECEPTIONIST) {
        printf("[Receptionist View: Clinical notes, prescriptions & test reports are restricted.]\n");
        return;
    }

    /* Doctors can view if they treated or were referred the patient; Admin can review */
    int allowed = (role == ROLE_ADMIN) ||
                  (strcmp(pat.assignedDoctorId, userId) == 0) ||
                  (strcmp(pat.referredDoctorId, userId) == 0);

    if (!allowed) {
        printf("Access denied: You are not the assigned or referred doctor for this patient.\n");
        return;
    }

    FILE *fp = fopen(FILE_RECORDS, "rb");
    if (fp == NULL) {
        printf("No medical records found yet.\n");
        return;
    }

    RecordEntry rec;
    int any = 0;
    printf("\n--- Unified Patient Profile (Chronological History) ---\n");
    while (fread(&rec, sizeof(RecordEntry), 1, fp) == 1) {
        if (strcmp(rec.patientId, patId) == 0) {
            const char *typeStr = "Note";
            if (rec.type == REC_PRESCRIPTION) typeStr = "Prescription";
            else if (rec.type == REC_TEST) typeStr = "Test Report";
            else if (rec.type == REC_REFERRAL) typeStr = "Referral";

            printf("[%s] [%s] By %s | Charge: %.2f\n  Details: %s\n",
                   rec.timestamp, typeStr, rec.doctorId, rec.charge, rec.details);
            any = 1;
        }
    }
    fclose(fp);

    if (!any) {
        printf("No entries recorded under %s.\n", patId);
    }
}
