#include "structs.h"

/* Functions from auth_setup.c */
extern void ensure_default_admin(void);
extern int login_user(User *loggedInUser);
extern void add_staff_user(void);
extern void add_doctor(void);
extern void list_doctors(void);
extern void view_doctor_schedule(void);
extern void setup_ward_beds(void);
extern void update_or_archive_patient(void);

/* Functions from patient_queue.c */
extern void register_patient(void);
extern void search_patient(void);
extern void book_appointment_and_queue(void);
extern void call_next_patient_in_queue(const char *doctorId);
extern void view_appointments(void);
extern void add_upp_record(const char *doctorId);
extern void view_upp_profile(UserRole role, const char *userId);

/* Functions from admission_bill.c */
extern void view_bed_availability(void);
extern void allocate_bed(void);
extern void generate_patient_bill(void);
extern void discharge_patient(void);
extern void view_hospital_reports(void);

static int read_menu_choice(int min_val, int max_val) {
    int choice;
    char line[64];
    while (1) {
        printf("\nEnter choice (%d-%d): ", min_val, max_val);
        if (fgets(line, sizeof(line), stdin) != NULL) {
            line[strcspn(line, "\r\n")] = '\0';
            if (sscanf(line, "%d", &choice) == 1 && choice >= min_val && choice <= max_val) {
                return choice;
            }
        }
        printf("Invalid choice! Please enter a number from %d to %d.\n", min_val, max_val);
    }
}

static void admin_menu(const User *user) {
    int ch;
    do {
        printf("\n================ ADMIN MENU (%s) ================\n", user->fullName);
        printf("1. Add Staff Account\n");
        printf("2. Add Doctor\n");
        printf("3. List All Doctors\n");
        printf("4. View Doctor Schedule (by Specialization)\n");
        printf("5. Setup Ward / Cabin / ICU Beds\n");
        printf("6. View Bed Availability\n");
        printf("7. Search Patient\n");
        printf("8. Update / Archive Patient\n");
        printf("9. View All Appointments\n");
        printf("10. View Hospital Reports\n");
        printf("0. Logout\n");

        ch = read_menu_choice(0, 10);
        switch (ch) {
            case 1: add_staff_user(); break;
            case 2: add_doctor(); break;
            case 3: list_doctors(); break;
            case 4: view_doctor_schedule(); break;
            case 5: setup_ward_beds(); break;
            case 6: view_bed_availability(); break;
            case 7: search_patient(); break;
            case 8: update_or_archive_patient(); break;
            case 9: view_appointments(); break;
            case 10: view_hospital_reports(); break;
            case 0: printf("Logging out...\n"); break;
        }
    } while (ch != 0);
}

static void receptionist_menu(const User *user) {
    int ch;
    do {
        printf("\n============= RECEPTIONIST MENU (%s) =============\n", user->fullName);
        printf("1. Register New Patient (Unique ID)\n");
        printf("2. Search Patient (ID / Name)\n");
        printf("3. View Doctor Schedule\n");
        printf("4. Book Appointment & Issue Smart Queue Token\n");
        printf("5. View Appointments\n");
        printf("6. View Bed Availability\n");
        printf("7. Allocate Bed / Cabin\n");
        printf("8. Update / Archive Patient\n");
        printf("9. Generate Patient Bill & Invoice\n");
        printf("10. Discharge Patient & Settle Payment\n");
        printf("11. View Basic Patient Info\n");
        printf("0. Logout\n");

        ch = read_menu_choice(0, 11);
        switch (ch) {
            case 1: register_patient(); break;
            case 2: search_patient(); break;
            case 3: view_doctor_schedule(); break;
            case 4: book_appointment_and_queue(); break;
            case 5: view_appointments(); break;
            case 6: view_bed_availability(); break;
            case 7: allocate_bed(); break;
            case 8: update_or_archive_patient(); break;
            case 9: generate_patient_bill(); break;
            case 10: discharge_patient(); break;
            case 11: view_upp_profile(user->role, user->id); break;
            case 0: printf("Logging out...\n"); break;
        }
    } while (ch != 0);
}

static void doctor_menu(const User *user) {
    int ch;
    do {
        printf("\n=============== DOCTOR MENU (%s) ===============\n", user->fullName);
        printf("1. Call Next Patient in Queue (Emergency First)\n");
        printf("2. Search Patient\n");
        printf("3. View Unified Patient Profile (UPP)\n");
        printf("4. Add UPP Entry (Note / Prescription / Test / Referral)\n");
        printf("5. View Doctor Schedule\n");
        printf("6. View Bed Availability\n");
        printf("0. Logout\n");

        ch = read_menu_choice(0, 6);
        switch (ch) {
            case 1: call_next_patient_in_queue(user->id); break;
            case 2: search_patient(); break;
            case 3: view_upp_profile(user->role, user->id); break;
            case 4: add_upp_record(user->id); break;
            case 5: view_doctor_schedule(); break;
            case 6: view_bed_availability(); break;
            case 0: printf("Logging out...\n"); break;
        }
    } while (ch != 0);
}

int main(void) {
    ensure_default_admin();

    int running = 1;
    while (running) {
        printf("\n====================================================\n");
        printf("   LOGIC 3 - HOSPITAL MANAGEMENT SYSTEM (CSE 1290)  \n");
        printf("====================================================\n");
        printf("1. Staff Login\n");
        printf("2. Exit Program\n");

        int topChoice = read_menu_choice(1, 2);
        if (topChoice == 2) {
            running = 0;
            printf("Exiting Hospital Management System. Goodbye!\n");
            break;
        }

        User loggedIn;
        if (login_user(&loggedIn)) {
            if (loggedIn.role == ROLE_ADMIN) {
                admin_menu(&loggedIn);
            } else if (loggedIn.role == ROLE_RECEPTIONIST) {
                receptionist_menu(&loggedIn);
            } else if (loggedIn.role == ROLE_DOCTOR) {
                doctor_menu(&loggedIn);
            }
        }
    }

    return 0;
}
