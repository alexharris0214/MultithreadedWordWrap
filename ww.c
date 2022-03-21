#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#define BUFFSIZE 256



void reformatFile(int fd, int lineLength, char *output){
    
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
                if(strcmp(dp->d_name, "..") ==0 || strcmp(dp->d_name, ".") ==0){
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
}
