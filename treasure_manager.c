#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>

#define TREASURE_FILE "treasures.dat"
#define LOG_FILE "logged_hunt"
#define HUNTS_DIR "hunts"
#define MAX_PATH 512

typedef struct {
    int ID, value;
    char user_name[50];
    float latitude, longitude;
    char clue[150];
} Treasure;

void write_str(const char *str) {
    write(STDOUT_FILENO, str, strlen(str));
}

void write_int(int num) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", num);
    write(STDOUT_FILENO, buf, strlen(buf));
}

void write_float(float f) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f", f);
    write(STDOUT_FILENO, buf, strlen(buf));
}

int ensure_hunt_dir(const char *hunt_id) {
    char path[MAX_PATH];
    snprintf(path, MAX_PATH, "%s/%s", HUNTS_DIR, hunt_id);
    if (mkdir(HUNTS_DIR, 0755) == -1 && errno != EEXIST) return -1;
    if (mkdir(path, 0755) == -1 && errno != EEXIST) return -1;
    return 0;
}

void log_action(const char *hunt_id, const char *action) {
    char log_path[MAX_PATH];
    snprintf(log_path, MAX_PATH, "%s/%s/%s", HUNTS_DIR, hunt_id, LOG_FILE);

    int fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) return;

    dprintf(fd, "%s\n", action);
    close(fd);

    char symlink_name[MAX_PATH];
    snprintf(symlink_name, MAX_PATH, "logged_hunt-%s", hunt_id);
    unlink(symlink_name);
    symlink(log_path, symlink_name);
}

void add_t(const char *hunt_id) {
    if (ensure_hunt_dir(hunt_id) != 0) {
        perror("mkdir");
        return;
    }

    Treasure t;
    char input_buf[200];

    write_str("Enter Treasure ID: ");
    read(STDIN_FILENO, input_buf, sizeof(input_buf));
    t.ID = atoi(input_buf);

    write_str("Enter Username: ");
    read(STDIN_FILENO, t.user_name, sizeof(t.user_name));
    t.user_name[strcspn(t.user_name, "\n")] = 0;

    write_str("Enter Latitude: ");
    read(STDIN_FILENO, input_buf, sizeof(input_buf));
    t.latitude = atof(input_buf);

    write_str("Enter Longitude: ");
    read(STDIN_FILENO, input_buf, sizeof(input_buf));
    t.longitude = atof(input_buf);

    write_str("Enter Clue: ");
    read(STDIN_FILENO, t.clue, sizeof(t.clue));
    t.clue[strcspn(t.clue, "\n")] = 0;

    write_str("Enter Value: ");
    read(STDIN_FILENO, input_buf, sizeof(input_buf));
    t.value = atoi(input_buf);

    char file_path[MAX_PATH];
    snprintf(file_path, MAX_PATH, "%s/%s/%s", HUNTS_DIR, hunt_id, TREASURE_FILE);

    int fd = open(file_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("open");
        return;
    }

    write(fd, &t, sizeof(Treasure));
    close(fd);

    log_action(hunt_id, "add");
    write_str("Treasure added successfully.\n");
}

void list_t(const char *hunt_id) {
    char file_path[MAX_PATH];
    snprintf(file_path, MAX_PATH, "%s/%s/%s", HUNTS_DIR, hunt_id, TREASURE_FILE);

    struct stat st;
    if (stat(file_path, &st) == -1) {
        perror("stat");
        return;
    }

    write_str("Hunt ID: ");
    write_str(hunt_id);
    write_str("\nFile Size: ");
    write_int((int)st.st_size);
    write_str(" bytes\nLast Modified: ");
    write_str(ctime(&st.st_mtime));

    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return;
    }

    Treasure t;
    write_str("\nTreasure List:\n");
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        write_str("ID: "); write_int(t.ID);
        write_str(", User: "); write_str(t.user_name);
        write_str(", Lat: "); write_float(t.latitude);
        write_str(", Long: "); write_float(t.longitude);
        write_str(", Value: "); write_int(t.value);
        write_str(", Clue: "); write_str(t.clue);
        write_str("\n");
    }

    close(fd);
    log_action(hunt_id, "list");
}

void view_t(const char *hunt_id, int target_id) {
    char file_path[MAX_PATH];
    snprintf(file_path, MAX_PATH, "%s/%s/%s", HUNTS_DIR, hunt_id, TREASURE_FILE);

    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return;
    }

    Treasure t;
    while (read(fd, &t, sizeof(Treasure)) == sizeof(Treasure)) {
        if (t.ID == target_id) {
            write_str("ID: "); write_int(t.ID);
            write_str("\nUser: "); write_str(t.user_name);
            write_str("\nLat: "); write_float(t.latitude);
            write_str("\nLong: "); write_float(t.longitude);
            write_str("\nValue: "); write_int(t.value);
            write_str("\nClue: "); write_str(t.clue);
            write_str("\n");
            close(fd);
            log_action(hunt_id, "view");
            return;
        }
    }

    write_str("Treasure with ID ");
    write_int(target_id);
    write_str(" not found.\n");
    close(fd);
}
//not implemented yet t_remove; h_remove
int main(int argc, char *argv[]) {
    if (argc < 3) {
        const char *msg = "Usage: ./treasure_manager <command> <hunt_id> [<id>]\n";
        write(STDERR_FILENO, msg, strlen(msg));
        return 1;
    }

    if (strcmp(argv[1], "add") == 0) {
        add_t(argv[2]);
    } else if (strcmp(argv[1], "list") == 0) {
        list_t(argv[2]);
    } else if (strcmp(argv[1], "view") == 0 && argc == 4) {
        view_t(argv[2], atoi(argv[3]));
    } else {
        const char *msg = "Invalid command or arguments.\n";
        write(STDERR_FILENO, msg, strlen(msg));
    }

    return 0;
}
