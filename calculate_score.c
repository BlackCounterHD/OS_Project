#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TREASURE_FILE "treasures.dat"
#define MAX_USERS 100

typedef struct {
    int ID, value;
    char user_name[50];
    float latitude, longitude;
    char clue[150];
} Treasure;

typedef struct {
    char user_name[50];
    int total_value;
} UserScore;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <hunt_id>\n", argv[0]);
        return 1;
    }
    char path[512];
    snprintf(path, sizeof(path), "hunts/%s/%s", argv[1], TREASURE_FILE);

    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("fopen");
        return 1;
    }

    UserScore users[MAX_USERS];
    int user_count = 0;
    Treasure t;
    while (fread(&t, sizeof(Treasure), 1, f) == 1) {
        int i, found = 0;
        for (i = 0; i < user_count; ++i) {
            if (strcmp(users[i].user_name, t.user_name) == 0) {
                users[i].total_value += t.value;
                found = 1;
                break;
            }
        }
        if (!found && user_count < MAX_USERS) {
            strncpy(users[user_count].user_name, t.user_name, sizeof(users[user_count].user_name)-1);
            users[user_count].user_name[sizeof(users[user_count].user_name)-1] = '\0';
            users[user_count].total_value = t.value;
            ++user_count;
        }
    }
    fclose(f);

    printf("Scores for hunt %s:\n", argv[1]);
    for (int i = 0; i < user_count; ++i) {
        printf("%s: %d\n", users[i].user_name, users[i].total_value);
    }
    return 0;
}
