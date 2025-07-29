#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#define MAX_BUFFER_SIZE 512
#define MAX_LINE_LENGTH 256


char buf[MAX_BUFFER_SIZE];
char line_store[MAX_BUFFER_SIZE];

int 
is_number(char *str) {
    for (int i = 0; str[i]; i++) {
        if (str[i] < '0' || str[i] > '9')
            return 0;
    }
    return 1;
}

int
open_file(char *file) {
    int fd = open(file, O_RDONLY);
    if (fd < 0) {
        fprintf(2, "tail: cannot open file %s\n", file);
        exit(1);
    }
    return fd;
}

void
tail(int fd, int no_of_lines) 
{
    if (no_of_lines <= 0) {
        return;
    }
    char **circular_queue = malloc(no_of_lines * sizeof(char *));
    int n, start = 0, line_count=0, j=0;
    while((n = read(fd, buf, sizeof(buf))) > 0) {
        for(int i=0;i<n;i++) {
            if(buf[i]=='\n') {
                line_store[j] = '\0';
                circular_queue[start] = malloc(MAX_LINE_LENGTH);
                strcpy(circular_queue[start], line_store);
                start = (start+1)%no_of_lines;
                j=0;
                line_count++;
            }
            else {
                line_store[j] = buf[i];
                j++;
            }
        }
    }

    if(j>0) {
        circular_queue[start] = malloc(MAX_LINE_LENGTH);
        strcpy(circular_queue[start], line_store);
        start = (start+1)%no_of_lines;
        j=0;
        line_count++;
    }

    int range = no_of_lines;
    if (line_count<no_of_lines) {
        range = line_count;
    }
    for(int i=0;i<range;i++) {
        printf("%s\n", circular_queue[(start+i)%range]);
    }
}

int* helper(int argc, char *argv[]) {
    int fd = 0, no_of_lines = 10;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0) {
            if (i + 1 >= argc) {
                fprintf(2, "option requires an argument -- n\nUsage: tail -n <number> [filename]\n");
                exit(1);
            }

            if (!is_number(argv[i + 1])) {
                fprintf(2, "Invalid number: %s\n", argv[i + 1]);
                exit(1);
            }

            no_of_lines = atoi(argv[i + 1]);

            if (i + 2 < argc) {
                fd = open_file(argv[i + 2]);
            }
            break; 
        }

        else if (argv[i][0] == '-' && is_number(argv[i] + 1)) {
            no_of_lines = atoi(argv[i] + 1);

            if (i + 1 < argc) {
                fd = open_file(argv[i + 1]);
            }
            break;
        }
    }

    if (argc == 2 && argv[1][0] != '-') {
        fd = open_file(argv[1]);
    }

    int *result = malloc(2 * sizeof(int));
    result[0] = fd;
    result[1] = no_of_lines;
    return result;
}

int
main(int argc, char *argv[])
{
    if (argc>4) {
        fprintf(2,
            "Too many arguments.\n"
            "Correct usage:\n"
            "  tail                    # read last 10 lines from stdin\n"
            "  tail <file>             # read last 10 lines from file\n"
            "  tail -n <N> [file]      # read last N lines from stdin or file\n"
            "  tail -<N> [file]        # shorthand: same as -n <N>\n"
            "Examples:\n"
            "  tail README.txt\n"
            "  cat file.txt | tail -n 5\n"
            "  tail -5 somefile\n"
        );
        exit(1);
    }
    int *args = helper(argc, argv);
    int fd = args[0];
    int no_of_lines = args[1];
    free(args);
    tail(fd, no_of_lines);
    if (fd>0) {
        close(fd);
    }
    exit(0);
}
