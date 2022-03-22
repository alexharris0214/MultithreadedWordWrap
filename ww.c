#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>


/*
* Returns a string with the prefix "wrap."
* placed in front of the desired string passed in,
* which in this case is the file name of the input file
* lying within the specified directory
*/
char *getOutputName(char *output){
    char *outputFileName = malloc(sizeof(output) + 6); // 6 is hardcoded to be length of "wrao." (5 character), and 1 for string terminator

    // manually assining the prefix
    outputFileName [0]= 'w';
    outputFileName [1]= 'r';
    outputFileName [2]= 'a';
    outputFileName [3]= 'p';
    outputFileName [4]= '.';

    // assigning the rest of the string according the input file name
    for(int i = 0; i<sizeof(output); i++){
        outputFileName[5+i] = output[i];
    }
    // placing terminator character
    outputFileName[sizeof(output) + 5] = '\0';

    return outputFileName;
}

void reformatFile(int fd, int lineLength, char *output){
    int outputFile = 1; // by default, output is to STDOUT
    char *buff = malloc(lineLength);

    if(output != NULL){ // if we have a desired outputFile, switch to that
        char *outputFileName = getOutputName(output);
        outputFile = open(outputFileName, O_WRONLY|O_CREAT|O_TRUNC, 0700); // creating file if it does not exist with user RWX permissions
        if(outputFile <= 0){ // checking to see if open returned successfully
            puts("Could not open/create desired outpfile");
            perror(outputFile);
            return EXIT_FAILURE;
        }
    }
    
    /* BEGING READ/WRITE IMPLEMENTATION HERE */


    // testing that writing is writing to correct place
    write(outputFile, "Hlo", 3);
}


int main(int argc, char const *argv[])
{
    int fd = -1; // File pointer
    DIR *dr = -1; // Directory pointer
    struct stat statbuf; // Holds file information to determine its type
    struct dirent *dp;
    int err; // Variable to store error information
    
    /*
    * Checks to see if valid arguments are passed in
    * if it was, check to see if file argument is a directory or normal file
    * if not, exit with failure
    */
    if(argc == 3){
        err = stat(argv[2], &statbuf);
        // Checking to see if fstat returned a valid stat struct
        if(err){
            perror(argv[2]); //printing out what went wrong when accessing the file
            return EXIT_FAILURE;
        }

        // file argument is a directory
        if(S_ISDIR(statbuf.st_mode)){
            dr = opendir(argv[2]);
            if(dr <= 0 ){ // error checking to see if directory was opened
                puts("Could not open directory");
            }
            chdir(argv[2]); // Changing the working directory to have access to the files we need
            while ((dp = readdir(dr)) != NULL){ // while theyre are files to be read

                // case where name of file is just a reference to current/parent directory
                if(strcmp(dp->d_name, "..") ==0 || strcmp(dp->d_name, ".") ==0 || strstr(dp->d_name, "wrap") != NULL){
                    continue;
                }
        
                // open the current file that we are on and work on it
                fd = open(dp->d_name, O_RDONLY);
                if(fd > 0){
                    reformatFile(fd, argv[1], dp->d_name);
                }
            }
        } else{ // file argument is a regular file
            fd = open(argv[2], O_RDONLY);
            if(fd <= 0){
                puts("Could not open file");
                return EXIT_FAILURE;
            }
            reformatFile(fd, argv[1], NULL);
        }
        // no file was passed in, reading from standard input
    } else if(argc==2){
        reformatFile(0, argv[1], NULL);
        return EXIT_FAILURE;
    } else { // fall through case when not enough arguments are passed
        puts("Not enough arguments");
        return EXIT_FAILURE;
    }
    return 0;
}
