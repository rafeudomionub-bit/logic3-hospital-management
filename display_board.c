#include "structs.h"

#ifdef _WIN32
#include <windows.h>
static void sleep_seconds(int sec) {
    Sleep(sec * 1000);
}
static void clear_screen(void) {
    system("cls");
}
#else
#include <unistd.h>
static void sleep_seconds(int sec) {
    sleep(sec);
}
static void clear_screen(void) {
    system("clear");
}
#endif

static void get_today_date(char *buf, int size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, size, "%Y-%m-%d", t);
}

static void render_display_board(const char *today) {
    clear_screen();
    printf("====================================================================\n");
    printf("           HOSPITAL WAITING ROOM - LIVE QUEUE DISPLAY BOARD          \n");
    printf("           Date: %-12s   (Auto-refreshes every 4s)         \n", today);
    printf("====================================================================\n");
    printf("%-10s %-22s %-14s %-10s\n",
           "Doc ID", "Doctor Name", "Now Serving", "Waiting");
    printf("--------------------------------------------------------------------\n");

    FILE *dfp = fopen(FILE_DOCTORS, "rb");
    if (dfp == NULL) {
        printf("No doctors configured yet (doctors.dat not found).\n");
        printf("====================================================================\n");
        return;
    }

    Doctor d;
    int anyDoc = 0;
    while (fread(&d, sizeof(Doctor), 1, dfp) == 1) {
        anyDoc = 1;
        int servingToken = 0;
        int servingEmergency = 0;
        int waitingCount = 0;

        FILE *qfp = fopen(FILE_QUEUE, "rb");
        if (qfp != NULL) {
            QueueEntry q;
            while (fread(&q, sizeof(QueueEntry), 1, qfp) == 1) {
                if (strcmp(q.doctorId, d.id) == 0 && strcmp(q.date, today) == 0) {
                    if (q.status == 1) {
                        servingToken = q.tokenNumber;
                        servingEmergency = q.isEmergency;
                    } else if (q.status == 0) {
                        waitingCount++;
                    }
                }
            }
            fclose(qfp);
        }

        char servingStr[32];
        if (servingToken > 0) {
            if (servingEmergency) {
                snprintf(servingStr, sizeof(servingStr), "#%d [EMRG]", servingToken);
            } else {
                snprintf(servingStr, sizeof(servingStr), "#%d", servingToken);
            }
        } else {
            strcpy(servingStr, "None");
        }

        printf("%-10s %-22s %-14s %-10d\n",
               d.id, d.name, servingStr, waitingCount);
    }
    fclose(dfp);

    if (!anyDoc) {
        printf("No doctors found in doctors.dat.\n");
    }
    printf("====================================================================\n");
    printf("Close this window to stop the Live Display Board.\n");
}

int main(void) {
    char today[16];
    get_today_date(today, sizeof(today));

    while (1) {
        render_display_board(today);
        sleep_seconds(4);
    }

    return 0;
}
