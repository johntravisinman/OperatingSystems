#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

void fork_handler(FILE *in_file, FILE *out_file, char *error_stmt);
void child(FILE *in, FILE *out);

int main(int argc, char *argv[])
{
    int opt; //getopt option
    char *in_fname = "input.dat"; //Input and output filenames
    char *out_fname = "output.dat";
    FILE *in_file = NULL; //empty file pointers for input and output
    FILE *out_file = NULL;

    char error_stmt[64];
    strcpy(error_stmt, argv[0]);
    strcat(error_stmt, ": Error");

    printf("\n");

    if (argc < 2) // No arguments given
    {
        printf("No arguments given\n");
        printf("Using default filenames\n\n");
    }

    // getopt loop
    while ((opt = getopt(argc, argv, "hi:o:")) != -1)
    {
        switch (opt)
        {
        case 'h': //Help Message
            printf("This program takes three possible arguments\n\n");
            printf("    \'-h\'          : to display this help message\n");
            printf("    \'-i filename\' : to designate the input file\n");
            printf("    \'-o filename\' : to designate the output file\n\n");
            printf("If no arguments are given then the default file names are used\n\n");
            printf("    INPUT:  \'input.dat\'\n");
            printf("    OUTPUT: \'output.dat\'\n\n");
            return 0;
        case 'i': //Input filename given
            in_fname = optarg;
            break;
        case 'o': //Output filename given
            out_fname = optarg;
            break;
        default:
            printf("No arguments given\n");
            printf("Using default filenames\n\n");
            break;
        }
    }
    // Displaying filenames
    printf("Using \'%s\' for input\n", in_fname);
    printf("Using \'%s\' for output\n\n", out_fname);

    // Opening files
    in_file = fopen(in_fname, "r");
    if (in_file == NULL) //Handle error opening file
    {
        printf("Filename: \'%s\'\n", in_fname);
        perror(error_stmt);
        printf("\n");
        exit(EXIT_FAILURE);
    }
    out_file = fopen(out_fname, "a+");
    if (out_file == NULL)//Handle error opening file
    {
        printf("Filename: \'%s\'\n", out_fname);
        perror(error_stmt);
        printf("\n");
        exit(EXIT_FAILURE);
    }
    //All files opened successfully so proceed with the program
    fork_handler(in_file, out_file, error_stmt);
    //Close both files
    fclose(in_file);
    fclose(out_file);
    return 0;
}
/* Fork handling function
 * handles the forking, waiting, and child functions
 * reads first line to get the number of forks to perform
 * at this point just reads back the number of forks to console
 * then enters a while loop that terminates on the eof of input file
 * */
void fork_handler(FILE *in_file, FILE *out_file, char *error_stmt)
{
    int i; //loop iterator
    int forks = 0;
    char file_buffer[128];
    pid_t pid; //process id
    int pids[100]; //child ids

    //Get the next line containing the amount of forks, convert to int and print to console
    fgets(file_buffer, sizeof(file_buffer), in_file);
    forks = atoi(file_buffer);
    printf("Forks: %d\n\n", forks);

    //Loop n times and fork in loop to create n total chld processes 
    for (i = 0; i < forks; i++)
    {
        //Create child process
        pid = fork();
        if (pid < 0) //Fork error
        {
            perror(error_stmt);
            exit(EXIT_FAILURE);
        }
        else if (pid == 0) //Child process
        {
            printf("Child Process ID: %d\n", (int)getpid());
            child(in_file, out_file);
            exit(0);
        }
        else //Parent so wait for child to finish
        {
            wait(NULL);
            if (!feof(in_file)) //Move filepointer down two lines
            {
                fgets(file_buffer, sizeof(file_buffer), in_file);
                fgets(file_buffer, sizeof(file_buffer), in_file);
            }
            pids[i] = (int)pid; //Add child process id to array for printing later
        }
    }
    //Print the child process IDs to the output file
    fprintf(out_file, "All children were: ");
    for(i = 0; i < forks; i++) {
        fprintf(out_file, "%d ", pids[i]);
    }
    fprintf(out_file, "\n");
    return;
}

/* Child Function
 * takes two FILE* as arguments for input and output files
 * reads two lines from a file
 * expects a single integer 'n' on the first line
 * expects 'n' integers on the second line
 * stores second line in a file buffer
 * file buffer is split using strtok into individual tokens which are then stored in array
 * 
 * */
void child(FILE *in, FILE *out)
{
    char file_buffer[128]; //File buffer for holding file input data
    int n = 0;             //How many numbers to read from the file
    char *num_tk = 0;      //Variable to hold the current number returned from strtok
    int i = 0;             //Loop iterator
    int child_proccess_id = getpid();
    fgets(file_buffer, sizeof(file_buffer), in); //Read first line containing the number of numbers to read
    n = atoi(file_buffer);                       //Storing numbers to read in n
    //Allocate an array to hold n numbers
    int *numbers = (int *)malloc(n * sizeof(int));
    fgets(file_buffer, sizeof(file_buffer), in); //Read next line containing n numbers
    num_tk = strtok(file_buffer, " ");           //strtok splits a string based on the given delimeter and returns the first/next token
    while (num_tk != NULL)
    {
        numbers[i] = atoi(num_tk);
        i++;
        if (i > n)
        { //Stop reading numbers after n numbers read
            printf("Too many numbers detected\n");
            printf("Stopped reading after %d numbers\n", n);
            break;
        }
        else
        { //
            num_tk = strtok(NULL, " ");
        }
    }
    if (num_tk == NULL && i < n)
    {
        printf("Too few numbers detected\n");
        printf("Expected %d numbers\n", n);
        printf("Recieved %d numbers\n", i);
        n = i;
    }
    //Printing the reversed array to the out_file
    fprintf(out, "%d: ", child_proccess_id);
    for (i = 0; i < n; i++)
    {
        fprintf(out, "%d ", numbers[n - i - 1]);
    }
    fprintf(out, "\n");
    free(numbers);
};