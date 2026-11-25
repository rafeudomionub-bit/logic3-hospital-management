#include "structs.h"

/* Helper to read a line safely and trim newline */
static void read_line(char *buf, int size) {
    if (fgets(buf, size, stdin) != NULL) {
        buf[strcspn(buf, "\r\n")] = '\0';
    } else {
        buf[0] = '\0';
    }
}

static void clear_input_buffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { }
}

static int read_int(const char *prompt, int min_val, int max_val) {
    int val;
    char line[64];
    while (1) {
        printf("%s", prompt);
        read_line(line, sizeof(line));
        if (sscanf(line, "%d", &val) == 1 && val >= min_val && val <= max_val) {
            return val;
        }
        printf("Invalid input! Please enter a number between %d and %d.\n", min_val, max_val);
    }
}

static double read_double(const char *prompt, double min_val) {
    double val;
    char line[64];
    while (1) {
        printf("%s", prompt);
        read_line(line, sizeof(line));
        if (sscanf(line, "%lf", &val) == 1 && val >= min_val) {
            return val;
        }
        printf("Invalid amount! Please enter a value >= %.2f.\n", min_val);
    }
}

/* Ensure default Admin account exists on first run */
void ensure_default_admin(void) {
    FILE *fp = fopen(FILE_USERS, "rb");
    if (fp != NULL) {
        fclose(fp);
        return;
    }

    fp = fopen(FILE_USERS, "wb");
    if (fp == NULL) return;

    User admin;
    memset(&admin, 0, sizeof(User));
    strcpy(admin.id, "USR-001");
    strcpy(admin.username, "admin");
    strcpy(admin.password, "1234");
    strcpy(admin.fullName, "System Administrator");
    admin.role = ROLE_ADMIN;
    admin.active = 1;

    fwrite(&admin, sizeof(User), 1, fp);
    fclose(fp);
}

/* Feature 1: Role-Based Login */
int login_user(User *loggedInUser) {
    ensure_default_admin();

    char username[50], password[50];
    int attempts = 0;

    while (attempts < 3) {
        printf("\n=== HOSPITAL LOGIN ===\n");
        printf("Username: ");
        read_line(username, sizeof(username));
        printf("Password: ");
        read_line(password, sizeof(password));

        FILE *fp = fopen(FILE_USERS, "rb");
        if (fp == NULL) {
            printf("Error opening %s!\n", FILE_USERS);
            return 0;
        }

        User u;
        int found = 0;
        while (fread(&u, sizeof(User), 1, fp) == 1) {
            if (u.active && strcmp(u.username, username) == 0 && strcmp(u.password, password) == 0) {
                *loggedInUser = u;
                found = 1;
                break;
            }
        }
        fclose(fp);

        if (found) {
            printf("\nLogin successful! Welcome, %s.\n", loggedInUser->fullName);
            return 1;
        }

        attempts++;
        printf("Invalid credentials! Attempts remaining: %d\n", 3 - attempts);
    }

    printf("Too many failed login attempts.\n");
    return 0;
}

void add_staff_user(void) {
    FILE *fp = fopen(FILE_USERS, "ab");
    if (fp == NULL) {
        printf("Unable to open %s!\n", FILE_USERS);
        return;
    }

    User u;
    memset(&u, 0, sizeof(User));

    printf("\n--- Add Staff Account ---\n");
    printf("User ID (e.g., USR-002): ");
    read_line(u.id, sizeof(u.id));
    printf("Full Name: ");
    read_line(u.fullName, sizeof(u.fullName));
    printf("Username: ");
    read_line(u.username, sizeof(u.username));
    printf("Password: ");
    read_line(u.password, sizeof(u.password));
    u.role = (UserRole)read_int("Role (1=Admin, 2=Receptionist, 3=Doctor): ", 1, 3);
    u.active = 1;

    fwrite(&u, sizeof(User), 1, fp);
    fclose(fp);
    printf("Staff account created successfully!\n");
}

/* Feature 2: Doctor Management */
void add_doctor(void) {
    FILE *fp = fopen(FILE_DOCTORS, "ab");
    if (fp == NULL) {
        printf("Error opening %s!\n", FILE_DOCTORS);
        return;
    }

    Doctor d;
    memset(&d, 0, sizeof(Doctor));

    printf("\n--- Add Doctor ---\n");
    printf("Doctor ID (e.g., DR-001): ");
    read_line(d.id, sizeof(d.id));
    printf("Doctor Name: ");
    read_line(d.name, sizeof(d.name));
    printf("Specialization: ");
    read_line(d.specialization, sizeof(d.specialization));
    printf("Visiting Hours (e.g., 10:00-14:00): ");
    read_line(d.visitingHours, sizeof(d.visitingHours));
    d.consultationFee = read_double("Consultation Fee: ", 0.0);
    d.avgConsultMinutes = read_double("Average Consult Time (minutes): ", 1.0);
    d.completedConsults = 0;

    fwrite(&d, sizeof(Doctor), 1, fp);
    fclose(fp);
    printf("Doctor added successfully!\n");
}

void list_doctors(void) {
    FILE *fp = fopen(FILE_DOCTORS, "rb");
    if (fp == NULL) {
        printf("\nNo doctors registered yet.\n");
        return;
    }

    Doctor d;
    printf("\n%-10s %-22s %-20s %-16s %-10s %-8s\n",
           "ID", "Name", "Specialization", "Visiting Hours", "Fee", "AvgMin");
    printf("------------------------------------------------------------------------------------------\n");
    while (fread(&d, sizeof(Doctor), 1, fp) == 1) {
        printf("%-10s %-22s %-20s %-16s %-10.2f %-8.1f\n",
               d.id, d.name, d.specialization, d.visitingHours, d.consultationFee, d.avgConsultMinutes);
    }
    fclose(fp);
}

/* Feature 3: Doctor Schedule View (sorted by specialization) */
void view_doctor_schedule(void) {
    FILE *fp = fopen(FILE_DOCTORS, "rb");
    if (fp == NULL) {
        printf("\nNo doctor schedules available.\n");
        return;
    }

    Doctor list[200];
    int count = 0;
    while (count < 200 && fread(&list[count], sizeof(Doctor), 1, fp) == 1) {
        count++;
    }
    fclose(fp);

    if (count == 0) {
        printf("\nNo doctor schedules available.\n");
        return;
    }

    /* Sort by specialization, then visiting hours */
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            int cmpSpec = strcmp(list[i].specialization, list[j].specialization);
            if (cmpSpec > 0 || (cmpSpec == 0 && strcmp(list[i].visitingHours, list[j].visitingHours) > 0)) {
                Doctor tmp = list[i];
                list[i] = list[j];
                list[j] = tmp;
            }
        }
    }

    printf("\n=== DOCTOR SCHEDULE BY SPECIALIZATION ===\n");
    printf("%-20s %-10s %-22s %-18s %-10s\n",
           "Specialization", "ID", "Doctor Name", "Visiting Hours", "Fee");
    printf("------------------------------------------------------------------------------------\n");
    for (int i = 0; i < count; i++) {
        printf("%-20s %-10s %-22s %-18s %-10.2f\n",
               list[i].specialization, list[i].id, list[i].name,
               list[i].visitingHours, list[i].consultationFee);
    }
}

/* Feature 4: Ward and Cabin Setup */
void setup_ward_beds(void) {
    FILE *fp = fopen(FILE_WARDS, "ab");
    if (fp == NULL) {
        printf("Error opening %s!\n", FILE_WARDS);
        return;
    }

    char wardName[MAX_STR];
    WardType type;
    double dailyRate;
    int bedCount;
    char prefix[16];

    printf("\n--- Ward / Cabin Setup ---\n");
    printf("Ward/Cabin Name (e.g., General Ward A, Cabin 101, ICU): ");
    read_line(wardName, sizeof(wardName));
    type = (WardType)read_int("Type (1=General Ward, 2=Cabin, 3=ICU): ", 1, 3);
    dailyRate = read_double("Daily Rate: ", 0.0);
    bedCount = read_int("Number of Beds/Rooms to create: ", 1, 100);
    printf("Bed ID Prefix (e.g., GEN1, CAB, ICU): ");
    read_line(prefix, sizeof(prefix));

    for (int i = 1; i <= bedCount; i++) {
        Bed b;
        memset(&b, 0, sizeof(Bed));
        snprintf(b.bedId, sizeof(b.bedId), "%s-%02d", prefix, i);
        strcpy(b.wardName, wardName);
        b.type = type;
        b.dailyRate = dailyRate;
        b.isOccupied = 0;
        b.patientId[0] = '\0';
        fwrite(&b, sizeof(Bed), 1, fp);
    }

    fclose(fp);
    printf("%d bed(s) created under '%s'!\n", bedCount, wardName);
}

/* Feature 14: Update / Archive Patient (UPP history remains intact) */
void update_or_archive_patient(void) {
    char targetId[ID_LEN];
    printf("\nEnter Patient ID (e.g., PT-001001): ");
    read_line(targetId, sizeof(targetId));

    FILE *fp = fopen(FILE_PATIENTS, "r+b");
    if (fp == NULL) {
        printf("No patient records found (%s missing).\n", FILE_PATIENTS);
        return;
    }

    Patient p;
    int found = 0;
    while (fread(&p, sizeof(Patient), 1, fp) == 1) {
        if (strcmp(p.id, targetId) == 0) {
            found = 1;
            printf("\nFound Patient: %s | Age: %d | Phone: %s | Due: %.2f | Archived: %s\n",
                   p.name, p.age, p.phone, p.dueAmount, p.isArchived ? "Yes" : "No");
            printf("1. Update Patient Details\n");
            printf("2. Archive Patient (No Dues Required)\n");
            int choice = read_int("Choose option (1-2): ", 1, 2);

            if (choice == 1) {
                printf("New Name (current: %s): ", p.name);
                read_line(p.name, sizeof(p.name));
                p.age = read_int("New Age: ", 0, 130);
                printf("New Phone: ");
                read_line(p.phone, sizeof(p.phone));
                printf("New Address: ");
                read_line(p.address, sizeof(p.address));
                printf("Patient details updated!\n");
            } else if (choice == 2) {
                if (p.dueAmount > 0.009) {
                    printf("Cannot archive! Patient has pending due: %.2f\n", p.dueAmount);
                } else if (p.isAdmitted) {
                    printf("Cannot archive! Patient is currently admitted.\n");
                } else {
                    p.isArchived = 1;
                    printf("Patient archived safely. Medical history (UPP) is preserved.\n");
                }
            }

            fseek(fp, -(long)sizeof(Patient), SEEK_CUR);
            fwrite(&p, sizeof(Patient), 1, fp);
            break;
        }
    }

    fclose(fp);
    if (!found) {
        printf("Patient ID '%s' not found.\n", targetId);
    }
}
