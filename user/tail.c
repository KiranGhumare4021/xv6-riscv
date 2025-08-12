/*
 * Minimal implementation of the Unix `tail` command for xv6 or user-space Unix-like systems.
 *
 *   Supported functionality:
 *   It supports reading the last N lines from a file, stdin, or piped input.
 *
 *   Supported Use Cases (All Covered):
 *   --------------------------------------
 *   tail                          → read last 10 lines from stdin
 *   cat <filename> | tail         → read last 10 lines from piped input
 *   tail <filename>               → read last 10 lines from a file
 *   cat <filename> | tail -3      → read last 3 lines from piped input (shorthand)
 *   tail -3 <filename>            → read last 3 lines from a file
 *   cat <filename> | tail -n 3    → read last 3 lines from piped input (standard form)
 *   tail -n 3                     → read last 3 lines from stdin
 *   tail -n 3 <filename>          → read last 3 lines from file
 *   tail -5                       → read last 5 lines from stdin
 *
 *   Error Cases (Properly Handled):
 *   --------------------------------------
 *   tail 3                        → if 3 is file, error: "cannot open file 3"
 *   tail -n -6                    → error: "Invalid number: -6"
 *   tail -m 3 <filename>          → error: "unsupported flag -m
 *                                           Usage: tail -n <number> [filename]"
 *   tail -n -n -5 <filename>      → error: "Too many arguments."
 *   cat <filename> | tail -n 0    → no output
 *   tail -0                       → no output
 *   tail -n                       → error: "option requires an argument -- n 
 *                                           Usage: tail -n <number> [filename]"
 *   tail -n <filename>            → error: "Invalid number: README"
 *   
 *   Notes:
 *   --------------------------------------
 *   - Default lines printed = 10, unless specified with -n or -<N>
 *   - File input is optional when reading from stdin or pipe
 *   - Invalid flags or missing arguments are caught and explained
 *
 * Edge cases handled:
 *   - Invalid/missing arguments
 *   - Invalid flags/ multiple arguments
 *   - Invalid number of lines (non-numeric, <= 0)
 *   - File opening errors
 *   - Input that doesn't end with newline
 *
 * Limitations:
 *   - Maximum line length: 256 characters
 *   - Input buffer: 512 bytes
 *   - Lines longer than 256 characters will be truncated
 */

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

/**
 *   tail(fd, no_of_lines)
 *
 *   This function prints the last 'no_of_lines' lines from a file or input stream.
 *
 *   Parameters:
 *   - fd: The file descriptor (0 for stdin, or from open() for a file).
 *   - no_of_lines: How many lines to print from the end.
 *
 *   How it works:
 *   - It reads the file/input in chunks (using a buffer of fixed size).
 *   - It stores each line in a circular queue (a fixed-size array that overwrites old lines when full).
 *   - If the number of lines in the input is greater than 'no_of_lines',
 *     only the last 'no_of_lines' are kept in memory.
 *   - Once the entire input is read, it prints the collected lines in the correct order.
 *
 *   Special cases:
 *   - If no_of_lines is zero or negative, nothing is printed.
 *   - If the file has fewer lines than requested, all lines are printed.
 *
 *   Notes:
 *   - Uses dynamic memory allocation for storing each line.
 *   - Assumes each line is less than MAX_LINE_LENGTH characters.
 *   - Handles both files and piped input.
**/

void
tail(int fd, int no_of_lines) 
{
    int n, start = 0, line_count=0, j=0;
    // handling 0 input case where the input is allowed
    if (no_of_lines == 0) {
      while ((n = read(fd, buf, sizeof(buf))) > 0);
      exit(0);
    }
    char **circular_queue = malloc(no_of_lines * sizeof(char *));
    
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

    /* This will handle the last line that does not end with '\n' */ 
    if(j>0) {
        circular_queue[start] = malloc(MAX_LINE_LENGTH);
        strcpy(circular_queue[start], line_store);
        start = (start+1)%no_of_lines;
        j=0;
        line_count++;
    }

    /* If the file has fewer lines than requested, all lines are printed. */
    int range = line_count<no_of_lines ? line_count: no_of_lines;

    for(int i=0;i<range;i++) {
        printf("%s\n", circular_queue[(start+i)%range]);
    }
    
    free(circular_queue);
}


/*
    helper(argc, argv)

    Parses command-line arguments to determine:
    - The number of lines to print (default: 10)
    - The file to read from (or stdin/pipe if no file is provided)

    Returns:
    - A dynamically allocated array of two integers:
        result[0] = file descriptor (fd)
        result[1] = number of lines to print (no_of_lines)

    Supports these formats:
    - tail                     → last 10 lines from stdin
    - tail <file>              → last 10 lines from file
    - tail -n <N>              → last N lines from stdin
    - tail -n <N> <file>       → last N lines from file
    - tail -<N>                → shorthand for -n <N>
    - tail -<N> <file>         → shorthand with file

    Error handling:
    - Invalid flags or missing arguments cause error messages and exit.
    - Invalid or non-numeric line values are rejected.
*/
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

        else if (argv[i][0] == '-') {
            if (is_number(argv[i] + 1)) {
                no_of_lines = atoi(argv[i] + 1);

                if (i + 1 < argc) {
                    fd = open_file(argv[i + 1]);
                }
                break;
            }
            else {
                fprintf(2, "unsupported flag %s\nUsage: tail -n <number> [filename]\n", argv[i]);
                exit(1);
                break;
            }
        }
    }

    // Takes care of the case tail <filename>
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
            "  tail file.txt\n"
            "  cat file.txt | tail -n 5\n"
            "  tail -5 file.txt\n"
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
