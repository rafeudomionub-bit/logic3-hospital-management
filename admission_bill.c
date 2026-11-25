#include "structs.h"

static void read_line_ab(char *buf, int size) {
    if (fgets(buf, size, stdin) != NULL) {
        buf[strcspn(buf, "\r\n")] = '\0';
    } else {
        buf[0] = '\0';
    }
}

static int read_int_ab(const char *prompt, int min_val, int max_val) {
    int val;
    char line[64];
    while (1) {
        printf("%s", prompt);
        read_line_ab(line, sizeof(line));
        if (sscanf(line, "%d", &val) == 1 && val >= min_val && val <= max_val) {
            return val;
        }
        printf("Invalid input! Enter a number between %d and %d.\n", min_val, max_val);
    }
}

static void get_now_str(char *buf, int size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", t);
}

static const char *ward_type_label(WardType t) {
    if (t == WARD_GENERAL) return "General";
    if (t == WARD_CABIN)   return "Cabin";
    if (t == WARD_ICU)     return "ICU";
    return "Unknown";
}

/* Feature 12: Bed Availability */
void view_bed_availability(void) {
    FILE *fp = fopen(FILE_WARDS, "rb");
    if (fp == NULL) {
        printf("\nNo wards or beds configured yet.\n");
        return;
    }

    Bed b;
    int total = 0, vacant = 0, occupied = 0;

    printf("\n=== REAL-TIME WARD & BED AVAILABILITY ===\n");
    printf("%-10s %-20s %-10s %-10s %-10s %-12s\n",
           "Bed ID", "Ward/Cabin", "Type", "Rate/Day", "Status", "Patient ID");
    printf("------------------------------------------------------------------------------\n");
    while (fread(&b, sizeof(Bed), 1, fp) == 1) {
        total++;
        if (b.isOccupied) occupied++;
        else vacant++;

        printf("%-10s %-20s %-10s %-10.2f %-10s %-12s\n",
               b.bedId, b.wardName, ward_type_label(b.type),
               b.dailyRate, b.isOccupied ? "OCCUPIED" : "VACANT",
               b.isOccupied ? b.patientId : "-");
    }
    fclose(fp);

    printf("------------------------------------------------------------------------------\n");
    printf("Total Beds: %d | Vacant: %d | Occupied: %d\n", total, vacant, occupied);
}

/* Feature 13: Bed Allocation */
void allocate_bed(void) {
    char patId[ID_LEN], bedId[ID_LEN];
    printf("\n--- Allocate Bed / Cabin ---\n");
    printf("Patient ID: ");
    read_line_ab(patId, sizeof(patId));

    FILE *pfp = fopen(FILE_PATIENTS, "r+b");
    if (pfp == NULL) {
        printf("Patients file not found!\n");
        return;
    }

    Patient pat;
    int patFound = 0;
    long patPos = 0;
    while (fread(&pat, sizeof(Patient), 1, pfp) == 1) {
        if (strcmp(pat.id, patId) == 0) {
            patFound = 1;
            patPos = ftell(pfp) - (long)sizeof(Patient);
            break;
        }
    }

    if (!patFound) {
        fclose(pfp);
        printf("Patient '%s' not found!\n", patId);
        return;
    }
    if (pat.isAdmitted) {
        fclose(pfp);
        printf("Patient '%s' is already admitted in bed %s!\n", patId, pat.assignedBedId);
        return;
    }

    view_bed_availability();
    printf("\nEnter Vacant Bed ID to assign: ");
    read_line_ab(bedId, sizeof(bedId));

    FILE *wfp = fopen(FILE_WARDS, "r+b");
    if (wfp == NULL) {
        fclose(pfp);
        printf("Wards file not found!\n");
        return;
    }

    Bed b;
    int bedFound = 0;
    while (fread(&b, sizeof(Bed), 1, wfp) == 1) {
        if (strcmp(b.bedId, bedId) == 0) {
            bedFound = 1;
            if (b.isOccupied) {
                printf("Bed '%s' is already occupied!\n", bedId);
                fclose(wfp);
                fclose(pfp);
                return;
            }
            b.isOccupied = 1;
            strcpy(b.patientId, patId);
            fseek(wfp, -(long)sizeof(Bed), SEEK_CUR);
            fwrite(&b, sizeof(Bed), 1, wfp);
            break;
        }
    }
    fclose(wfp);

    if (!bedFound) {
        fclose(pfp);
        printf("Bed ID '%s' not found!\n", bedId);
        return;
    }

    pat.isAdmitted = 1;
    strcpy(pat.assignedBedId, bedId);
    fseek(pfp, patPos, SEEK_SET);
    fwrite(&pat, sizeof(Patient), 1, pfp);
    fclose(pfp);

    printf("Bed %s assigned to %s (%s) successfully!\n", bedId, pat.name, pat.id);
}

/* Helper to print an itemized invoice */
static void print_invoice(const Bill *bill, const Patient *pat) {
    printf("\n==============================================\n");
    printf("          HOSPITAL ITEMIZED INVOICE           \n");
    printf("==============================================\n");
    printf("Bill ID        : %s\n", bill->billId);
    printf("Generated At   : %s\n", bill->generatedAt);
    printf("Patient        : %s (%s)\n", pat->name, pat->id);
    printf("Admission Days : %d\n", bill->admissionDays);
    printf("----------------------------------------------\n");
    printf("Bed / Cabin Charges    : %10.2f\n", bill->bedCharge);
    printf("Doctor Consultation Fee: %10.2f\n", bill->doctorCharge);
    printf("Medicine Charges (UPP) : %10.2f\n", bill->medicineCharge);
    printf("Lab / Test Charges(UPP): %10.2f\n", bill->testCharge);
    printf("----------------------------------------------\n");
    printf("TOTAL PAYABLE          : %10.2f\n", bill->totalAmount);
    printf("Payment Status         : %s\n", bill->isPaid ? "PAID" : "UNPAID");
    printf("==============================================\n");
}

/* Feature 15 & 16: Billing & Invoice (reads medicine & test charges directly from UPP) */
void generate_patient_bill(void) {
    char patId[ID_LEN];
    printf("\n--- Generate Itemized Bill & Invoice ---\n");
    printf("Patient ID: ");
    read_line_ab(patId, sizeof(patId));

    FILE *pfp = fopen(FILE_PATIENTS, "r+b");
    if (pfp == NULL) {
        printf("Patients file not found!\n");
        return;
    }

    Patient pat;
    int patFound = 0;
    long patPos = 0;
    while (fread(&pat, sizeof(Patient), 1, pfp) == 1) {
        if (strcmp(pat.id, patId) == 0) {
            patFound = 1;
            patPos = ftell(pfp) - (long)sizeof(Patient);
            break;
        }
    }

    if (!patFound) {
        fclose(pfp);
        printf("Patient '%s' not found!\n", patId);
        return;
    }

    int days = 0;
    double dailyRate = 0.0;
    if (pat.isAdmitted && strlen(pat.assignedBedId) > 0) {
        days = read_int_ab("Number of admitted days: ", 1, 365);
        FILE *wfp = fopen(FILE_WARDS, "rb");
        if (wfp != NULL) {
            Bed b;
            while (fread(&b, sizeof(Bed), 1, wfp) == 1) {
                if (strcmp(b.bedId, pat.assignedBedId) == 0) {
                    dailyRate = b.dailyRate;
                    break;
                }
            }
            fclose(wfp);
        }
    }

    double doctorFee = 0.0;
    if (strlen(pat.assignedDoctorId) > 0) {
        FILE *dfp = fopen(FILE_DOCTORS, "rb");
        if (dfp != NULL) {
            Doctor d;
            while (fread(&d, sizeof(Doctor), 1, dfp) == 1) {
                if (strcmp(d.id, pat.assignedDoctorId) == 0) {
                    doctorFee = d.consultationFee;
                    break;
                }
            }
            fclose(dfp);
        }
    }

    /* Pull medicine & test charges directly from UPP records */
    double medCharge = 0.0, testCharge = 0.0;
    FILE *rfp = fopen(FILE_RECORDS, "rb");
    if (rfp != NULL) {
        RecordEntry rec;
        while (fread(&rec, sizeof(RecordEntry), 1, rfp) == 1) {
            if (strcmp(rec.patientId, patId) == 0) {
                if (rec.type == REC_PRESCRIPTION) medCharge += rec.charge;
                else if (rec.type == REC_TEST)    testCharge += rec.charge;
            }
        }
        fclose(rfp);
    }

    Bill bill;
    memset(&bill, 0, sizeof(Bill));
    snprintf(bill.billId, sizeof(bill.billId), "INV-%ld", (long)time(NULL));
    strcpy(bill.patientId, patId);
    bill.admissionDays = days;
    bill.bedCharge = days * dailyRate;
    bill.doctorCharge = doctorFee;
    bill.medicineCharge = medCharge;
    bill.testCharge = testCharge;
    bill.totalAmount = bill.bedCharge + bill.doctorCharge + bill.medicineCharge + bill.testCharge;
    bill.isPaid = 0;
    get_now_str(bill.generatedAt, sizeof(bill.generatedAt));

    FILE *bfp = fopen(FILE_BILLS, "ab");
    if (bfp != NULL) {
        fwrite(&bill, sizeof(Bill), 1, bfp);
        fclose(bfp);
    }

    pat.dueAmount = bill.totalAmount;
    fseek(pfp, patPos, SEEK_SET);
    fwrite(&pat, sizeof(Patient), 1, pfp);
    fclose(pfp);

    print_invoice(&bill, &pat);
}

/* Feature 17: Discharge (finalize payment, free bed, mark patient discharged) */
void discharge_patient(void) {
    char patId[ID_LEN];
    printf("\n--- Patient Discharge & Payment Settlement ---\n");
    printf("Patient ID: ");
    read_line_ab(patId, sizeof(patId));

    FILE *pfp = fopen(FILE_PATIENTS, "r+b");
    if (pfp == NULL) {
        printf("Patients file not found!\n");
        return;
    }

    Patient pat;
    int patFound = 0;
    long patPos = 0;
    while (fread(&pat, sizeof(Patient), 1, pfp) == 1) {
        if (strcmp(pat.id, patId) == 0) {
            patFound = 1;
            patPos = ftell(pfp) - (long)sizeof(Patient);
            break;
        }
    }

    if (!patFound) {
        fclose(pfp);
        printf("Patient '%s' not found!\n", patId);
        return;
    }

    printf("Patient: %s | Current Due: %.2f | Admitted: %s\n",
           pat.name, pat.dueAmount, pat.isAdmitted ? "Yes" : "No");

    int confirm = read_int_ab("Confirm full payment & discharge? (1=Yes, 0=No): ", 0, 1);
    if (!confirm) {
        fclose(pfp);
        printf("Discharge cancelled.\n");
        return;
    }

    /* Mark unpaid bills for this patient as PAID */
    FILE *bfp = fopen(FILE_BILLS, "r+b");
    if (bfp != NULL) {
        Bill b;
        while (fread(&b, sizeof(Bill), 1, bfp) == 1) {
            if (strcmp(b.patientId, patId) == 0 && !b.isPaid) {
                b.isPaid = 1;
                fseek(bfp, -(long)sizeof(Bill), SEEK_CUR);
                fwrite(&b, sizeof(Bill), 1, bfp);
                fseek(bfp, 0, SEEK_CUR);
            }
        }
        fclose(bfp);
    }

    /* Free the assigned bed if admitted */
    if (pat.isAdmitted && strlen(pat.assignedBedId) > 0) {
        FILE *wfp = fopen(FILE_WARDS, "r+b");
        if (wfp != NULL) {
            Bed bed;
            while (fread(&bed, sizeof(Bed), 1, wfp) == 1) {
                if (strcmp(bed.bedId, pat.assignedBedId) == 0) {
                    bed.isOccupied = 0;
                    bed.patientId[0] = '\0';
                    fseek(wfp, -(long)sizeof(Bed), SEEK_CUR);
                    fwrite(&bed, sizeof(Bed), 1, wfp);
                    break;
                }
            }
            fclose(wfp);
        }
    }

    pat.isAdmitted = 0;
    pat.assignedBedId[0] = '\0';
    pat.dueAmount = 0.0;
    fseek(pfp, patPos, SEEK_SET);
    fwrite(&pat, sizeof(Patient), 1, pfp);
    fclose(pfp);

    printf("Payment settled, bed freed, and %s (%s) discharged successfully!\n",
           pat.name, pat.id);
}

/* Feature 18: Reports (Revenue, Discharge/Bill records, Hospital counts) */
void view_hospital_reports(void) {
    int totalPatients = 0, admittedPatients = 0, archivedPatients = 0;
    FILE *pfp = fopen(FILE_PATIENTS, "rb");
    if (pfp != NULL) {
        Patient p;
        while (fread(&p, sizeof(Patient), 1, pfp) == 1) {
            totalPatients++;
            if (p.isAdmitted) admittedPatients++;
            if (p.isArchived) archivedPatients++;
        }
        fclose(pfp);
    }

    int totalDoctors = 0;
    FILE *dfp = fopen(FILE_DOCTORS, "rb");
    if (dfp != NULL) {
        Doctor d;
        while (fread(&d, sizeof(Doctor), 1, dfp) == 1) totalDoctors++;
        fclose(dfp);
    }

    int totalBeds = 0, occupiedBeds = 0;
    FILE *wfp = fopen(FILE_WARDS, "rb");
    if (wfp != NULL) {
        Bed b;
        while (fread(&b, sizeof(Bed), 1, wfp) == 1) {
            totalBeds++;
            if (b.isOccupied) occupiedBeds++;
        }
        fclose(wfp);
    }

    double collectedRevenue = 0.0, pendingDues = 0.0;
    int paidInvoices = 0, unpaidInvoices = 0;
    FILE *bfp = fopen(FILE_BILLS, "rb");
    if (bfp != NULL) {
        Bill bill;
        while (fread(&bill, sizeof(Bill), 1, bfp) == 1) {
            if (bill.isPaid) {
                collectedRevenue += bill.totalAmount;
                paidInvoices++;
            } else {
                pendingDues += bill.totalAmount;
                unpaidInvoices++;
            }
        }
        fclose(bfp);
    }

    printf("\n==============================================\n");
    printf("          HOSPITAL ACTIVITY REPORTS           \n");
    printf("==============================================\n");
    printf("Total Registered Patients : %d\n", totalPatients);
    printf("Currently Admitted        : %d\n", admittedPatients);
    printf("Archived Patients         : %d\n", archivedPatients);
    printf("Registered Doctors        : %d\n", totalDoctors);
    printf("Ward/Cabin Beds           : %d (Occupied: %d, Vacant: %d)\n",
           totalBeds, occupiedBeds, totalBeds - occupiedBeds);
    printf("----------------------------------------------\n");
    printf("Paid Invoices             : %d\n", paidInvoices);
    printf("Unpaid Invoices           : %d\n", unpaidInvoices);
    printf("Total Collected Revenue   : %.2f\n", collectedRevenue);
    printf("Total Pending Dues        : %.2f\n", pendingDues);
    printf("==============================================\n");
}
