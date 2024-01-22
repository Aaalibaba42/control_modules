#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PID_FILE "/tmp/memory_controller.pid"

#define LOG_FILE "/tmp/memory_controller.log"

bool SHOULD_LOG = true;

// 2 ** 27
#define PAGE_SIZE 134217728
void *PAGE = NULL;

// TODO put on the heap you rascal
#define MAX_STACK_SIZE 256
pid_t child_stack[MAX_STACK_SIZE];
int stack_index = -1;

void my_log(char *str);

void cleanup(int i)
{
    // the parent
    if (SHOULD_LOG)
        unlink(PID_FILE);

    free(PAGE);

    char *s = NULL;
    asprintf(&s, "Cleaning up: %i", i);
    my_log(s);
    free(s);

    exit(i);
}

void my_log(char *str)
{
    if (!SHOULD_LOG)
        return;

    FILE *log_file = fopen(LOG_FILE, "a");
    if (log_file != NULL)
    {
        fprintf(log_file, "%s\n", str);
        fclose(log_file);
    }
    else
    {
        my_log("Error opening log file");
        cleanup(EXIT_FAILURE);
    }
}

void write_pid_file(pid_t pid)
{
    FILE *pid_file = fopen(PID_FILE, "w");
    if (pid_file != NULL)
    {
        fprintf(pid_file, "%d", pid);
        fclose(pid_file);
    }
    else
    {
        my_log("Error opening PID file");
        cleanup(EXIT_FAILURE);
    }
}

void daemonize(void)
{
    char *s = NULL;
    pid_t pid, sid;
    pid = fork();

    if (pid < 0)
    {
        my_log("Failed fork");
        cleanup(EXIT_FAILURE);
    }
    if (pid > 0)
    {
        write_pid_file(pid);
        asprintf(&s, "fork pid: %d", pid);
        my_log(s);
        free(s);
        s = NULL;
        my_log("Parent Suicide");
        exit(EXIT_SUCCESS);
    }

    umask(0);
    sid = setsid();
    asprintf(&s, "sid: %d", sid);
    my_log(s);
    free(s);
    s = NULL;
    asprintf(&s, "pid after setsid: %d", getpid());
    my_log(s);
    free(s);
    s = NULL;
    if (sid < 0)
    {
        my_log("Failed setsid");
        cleanup(EXIT_FAILURE);
    }
    if ((chdir("/")) < 0)
    {
        my_log("Failed chdir");
        cleanup(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    free(s);
}

void increase_memory(void)
{
    SHOULD_LOG = false;
    PAGE = malloc(PAGE_SIZE);
    memset(PAGE, 0, PAGE_SIZE);
    while (1)
        pause();
}

void sigusr1_handler(void)
{
    char *s = NULL;
    if (stack_index < MAX_STACK_SIZE - 1)
    {
        pid_t child_pid = fork();

        if (child_pid == -1)
        {
            my_log("Fork error");
            cleanup(EXIT_FAILURE);
        }

        if (child_pid == 0)
        {
            asprintf(&s, "Increase memory process %i created.", getpid());
            my_log(s);
            free(s);
            s = NULL;
            increase_memory();
            cleanup(EXIT_SUCCESS);
        }
        else
        {
            stack_index++;
            child_stack[stack_index] = child_pid;
            asprintf(&s, "Child process %i added to the stack.", child_pid);
            my_log(s);
            free(s);
            s = NULL;
        }
    }
    else
        my_log("Stack is full. Cannot add more processes.");
}

void sigusr2_handler(void)
{
    char *s = NULL;
    if (stack_index >= 0)
    {
        pid_t top_pid = child_stack[stack_index];
        kill(top_pid, SIGTERM);
        waitpid(top_pid, NULL, 0);

        asprintf(&s, "Child process %i killed and removed from the stack.",
                 top_pid);
        my_log(s);
        free(s);
        s = NULL;
        stack_index--;
    }
    else
        my_log("Stack is empty. No processes to kill.");
}

void signal_handler(int signum)
{
    switch (signum)
    {
    case SIGUSR1: {
        my_log("Received SIGUSR1");
        sigusr1_handler();
        break;
    }
    case SIGUSR2: {
        my_log("Received SIGUSR2");
        sigusr2_handler();
        break;
    }

    case SIGTERM:
    case SIGINT: {
        my_log("Received SIGTERM or SIGINT");
        cleanup(EXIT_SUCCESS);
        break;
    }
    default: {
        char *s = NULL;
        asprintf(&s, "Received signal: %i", signum);
        my_log(s);
        free(s);
        s = NULL;
        break;
    }
    }
}

int get_pidfile()
{
    FILE *f = fopen(PID_FILE, "r");
    if (f == NULL)
    {
        printf("Could not open pid_file\n");
        exit(EXIT_FAILURE);
    }
    char *s = NULL;
    size_t t = 0;
    getline(&s, &t, f);
    int r = atoi(s);
    free(s);
    return r;
}

void up_usage()
{
    int pid = get_pidfile();
    kill(pid, SIGUSR1);
}

void down_usage()
{
    int pid = get_pidfile();
    kill(pid, SIGUSR2);
}

int main(int argc, char *argv[])
{
    if (access("/tmp/memory_controller.pid", F_OK) == 0)
    {
        if (argc != 2)
        {
            printf("Usage: ./memory_controller [up|down]\n");
            exit(EXIT_FAILURE);
        }

        if (!strcmp(argv[1], "up"))
            up_usage();
        else if (!strcmp(argv[1], "down"))
            down_usage();
        else
        {
            printf("Usage: ./memory_controller [up|down]\n");
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }
    else
    {
        printf("Srarting Daemon...\n");
        daemonize();

        signal(SIGUSR1, signal_handler);
        signal(SIGUSR2, signal_handler);
        signal(SIGTERM, signal_handler);
        signal(SIGINT, signal_handler);

        char *s = NULL;
        asprintf(&s, "pid in main thing: %i", getpid());
        my_log(s);
        free(s);

        while (1)
        {
            pause();
        }

        // Unreachable, but still
        cleanup(EXIT_SUCCESS);
    }
}
