
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

#define HUNTS_DIR       "hunts"
#define TREASURE_FILE   "treasures.dat"
#define CMD_FILE        "monitor_cmd.txt"
#define MAX_PATH        512


typedef struct {
    int ID, value;
    char user_name[50];
    float latitude, longitude;
    char clue[150];
} Treasure;


static volatile sig_atomic_t got_cmd       = 0;
static void sigusr1_handler(int _) { got_cmd = 1; }
static void sigterm_handler(int _) {
   
    usleep(1000000);
    exit(0);
}


static void process_cmd() {
    FILE *f = fopen(CMD_FILE, "r");
    if (!f) {
        perror("monitor: fopen CMD_FILE");
        return;
    }
    char line[256];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return; }
    fclose(f);
    line[strcspn(line, "\n")] = 0;

    char *tok = strtok(line, " ");
    if (!tok) return;

    if (strcmp(tok, "LIST_HUNTS") == 0) {
        
        DIR *d = opendir(HUNTS_DIR);
        if (!d) { perror("monitor: opendir"); return; }
        struct dirent *ent;
        while ((ent = readdir(d))) {
            if (ent->d_type!=DT_DIR) continue;
            if (!strcmp(ent->d_name,".") || !strcmp(ent->d_name,"..")) continue;
            char path[MAX_PATH];
            struct stat st;
            snprintf(path, sizeof(path), "%s/%s/%s",
                     HUNTS_DIR, ent->d_name, TREASURE_FILE);
            int count = 0;
            if (stat(path, &st)==0)
                count = st.st_size / sizeof(Treasure);
            printf("Hunt %s: %d treasures\n",
                   ent->d_name, count);
        }
        closedir(d);

    } else if (strcmp(tok, "LIST_TREASURES") == 0) {
        char *hunt = strtok(NULL, " ");
        if (hunt) {
            char cmd[MAX_PATH];
            snprintf(cmd, sizeof(cmd),
                     "./treasure_manager list %s", hunt);
            system(cmd);
        }

    } else if (strcmp(tok, "VIEW_TREASURE") == 0) {
        char *hunt = strtok(NULL, " ");
        char *id   = strtok(NULL, " ");
        if (hunt && id) {
            char cmd[MAX_PATH];
            snprintf(cmd, sizeof(cmd),
                     "./treasure_manager view %s %s",
                     hunt, id);
            system(cmd);
        }
    }
}


static void monitor_main() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigusr1_handler;
    sigaction(SIGUSR1, &sa, NULL);
    sa.sa_handler = sigterm_handler;
    sigaction(SIGTERM, &sa, NULL);

    while (1) {
        pause();
        if (got_cmd) {
            got_cmd = 0;
            process_cmd();
        }
    }
}


static pid_t monitor_pid = -1;
static int   shutting_down = 0;


static void sigchld_handler(int _) {
    int status;
    pid_t pid = waitpid(monitor_pid, &status, WNOHANG);
    if (pid == monitor_pid) {
        if (WIFEXITED(status))
            printf("Monitor exited with status %d\n",
                   WEXITSTATUS(status));
        else if (WIFSIGNALED(status))
            printf("Monitor killed by signal %d\n",
                   WTERMSIG(status));
        monitor_pid   = -1;
        shutting_down = 0;
    }
}


static void write_cmd(const char *fmt, ...) {
    FILE *f = fopen(CMD_FILE, "w");
    if (!f) { perror("hub: fopen CMD_FILE"); return; }
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, "\n");
    fclose(f);
}


static void cmd_start_monitor() {
    if (monitor_pid > 0) {
        printf("Monitor already running (pid %d)\n", monitor_pid);
        return;
    }
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sa.sa_flags   = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    pid_t pid = fork();
    if (pid < 0) {
        perror("hub: fork");
        return;
    }
    if (pid == 0) {
        monitor_main();
        _exit(0);
    }
    
    monitor_pid   = pid;
    shutting_down = 0;
    printf("Monitor started (pid %d)\n", pid);
}

int main() {
    char line[256];
    while (1) {
        printf("hub> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = 0;
        char *cmd = strtok(line, " ");
        if (!cmd) continue;

        if (strcmp(cmd, "start_monitor") == 0) {
            cmd_start_monitor();

        } else if (strcmp(cmd, "list_hunts") == 0) {
            if (monitor_pid < 0)
                printf("Error: monitor not running.\n");
            else if (shutting_down)
                printf("Error: monitor shutting down, please wait...\n");
            else {
                write_cmd("LIST_HUNTS");
                kill(monitor_pid, SIGUSR1);
            }

        } else if (strcmp(cmd, "list_treasures") == 0) {
            char *hunt = strtok(NULL, " ");
            if (!hunt)
                printf("Usage: list_treasures <hunt_id>\n");
            else if (monitor_pid < 0)
                printf("Error: monitor not running.\n");
            else if (shutting_down)
                printf("Error: monitor shutting down, please wait...\n");
            else {
                write_cmd("LIST_TREASURES %s", hunt);
                kill(monitor_pid, SIGUSR1);
            }

        } else if (strcmp(cmd, "view_treasure") == 0) {
            char *hunt = strtok(NULL, " ");
            char *id   = strtok(NULL, " ");
            if (!hunt || !id)
                printf("Usage: view_treasure <hunt_id> <treasure_id>\n");
            else if (monitor_pid < 0)
                printf("Error: monitor not running.\n");
            else if (shutting_down)
                printf("Error: monitor shutting down, please wait...\n");
            else {
                write_cmd("VIEW_TREASURE %s %s", hunt, id);
                kill(monitor_pid, SIGUSR1);
            }

        } else if (strcmp(cmd, "stop_monitor") == 0) {
            if (monitor_pid < 0)
                printf("Error: monitor not running.\n");
            else {
                shutting_down = 1;
                kill(monitor_pid, SIGTERM);
                printf("Sent stop request to monitor (pid %d)\n",
                       monitor_pid);
            }

        } else if (strcmp(cmd, "exit") == 0) {
            if (monitor_pid > 0)
                printf("Error: monitor still running, cannot exit.\n");
            else
                break;

        } else {
            printf("Unknown command: %s\n", cmd);
        }
    }
    return 0;
}
