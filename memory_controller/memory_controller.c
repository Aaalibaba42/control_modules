#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PID_FILE "/tmp/memory_controller.pid"

bool PARENT = true;

//                 2 ** 27
#define PAGE_SIZE 134217728
void *PAGE = NULL;

// TODO put on the heap you rascal
#define MAX_STACK_SIZE 256
pid_t child_stack[MAX_STACK_SIZE];
int stack_index = -1;

void cleanup(int i)
{
    if (PARENT)
    {
        unlink(PID_FILE);
        // TODO once on the heap, free child_stack here
    }

    // Sometimes will free NULL, which is not UB so isok
    free(PAGE);

    exit(i);
}

void write_pid_file(pid_t pid)
{
    FILE *pid_file = fopen(PID_FILE, "w");

    if (!pid_file)
        cleanup(EXIT_FAILURE);

    fprintf(pid_file, "%d", pid);
    fclose(pid_file);
}

void daemonize(void)
{
    pid_t pid, sid;
    pid = fork();

    if (pid < 0)
        cleanup(EXIT_FAILURE);
    if (pid > 0)
    {
        write_pid_file(pid);
        exit(EXIT_SUCCESS);
    }

    umask(0);
    sid = setsid();
    if (sid < 0)
        cleanup(EXIT_FAILURE);
    if ((chdir("/")) < 0)
        cleanup(EXIT_FAILURE);

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

void increase_memory(void)
{
    PAGE = malloc(PAGE_SIZE);

    // Kernel's on demand paging requires us to
    // "use" the memory we allocate
    memset(PAGE, 0, PAGE_SIZE);

    // Waits for the SIGINT/SIGTERM/SIGKILL
    while (1)
        pause();
}

void sigusr1_handler(void)
{
    if (stack_index < MAX_STACK_SIZE - 1)
    {
        pid_t child_pid = fork();

        if (child_pid == -1)
            cleanup(EXIT_FAILURE);

        if (child_pid == 0)
        {
            PARENT = false;
            increase_memory();
            // Unreachable, but still
            cleanup(EXIT_SUCCESS);
        }
        stack_index++;
        child_stack[stack_index] = child_pid;
    }
}

void sigusr2_handler(void)
{
    if (stack_index >= 0)
    {
        pid_t top_pid = child_stack[stack_index];
        kill(top_pid, SIGTERM);
        waitpid(top_pid, NULL, 0);
        stack_index--;
    }
}

void signal_handler(int signum)
{
    switch (signum)
    {
    case SIGUSR1:
        sigusr1_handler();
        break;
    case SIGUSR2:
        sigusr2_handler();
        break;

    case SIGTERM:
    case SIGINT:
        cleanup(EXIT_SUCCESS);
    default:
        break;
    }
}

int fetch_pid()
{
    FILE *f = fopen(PID_FILE, "r");
    if (!f)
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
    int pid = fetch_pid();
    if (kill(pid, SIGUSR1) < 0)
        printf("Signal was not sent. Is the daemon running and does the file %s contain a valid pid ?\n", PID_FILE);
}

void down_usage()
{
    int pid = fetch_pid();
    if (kill(pid, SIGUSR2) < 0)
        printf("Signal was not sent. Is the daemon running and does the file %s contain a valid pid ?\n", PID_FILE);
}

int main(int argc, char *argv[])
{
    // If daemon is running
    if (!access(PID_FILE, F_OK))
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

    // Daemon is not running
    printf("Srarting Daemon...\n");
    daemonize();

    signal(SIGUSR1, signal_handler);
    signal(SIGUSR2, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    while (1)
        pause();

    // Unreachable, but still
    cleanup(EXIT_SUCCESS);
}
