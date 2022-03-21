#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#define BUFFSIZE 256


/*
* Reads contents of given file and stores them
* in a string array
*/

char **readFile(){

}

// Returns a sorted list given a list of strings
char **sort(char **list){
    // NEEDS TO BE IMPLEMENTED
}

/*
* Take the sorted list and writes them to standard output if the file argument 
* was a regular file
* if file argument was a directory, ouput will be written to a file with the name
* of the file within specified direcotry with the prefix "wrap"
*/
void writeContent(char **list, char *fileName){
    // NEEDS TO BE IMPLEMENTED
}

int main(int argc, char const *argv[])
{
    int fd = -1; // File pointer
    DIR *dr = -1; // Directory pointer
    char buff[BUFFSIZE]; // Buffer
    struct stat statbuf; // Holds file information to determine its type
    struct dirent *dp;
    int err; // Variable to store error information
    char **wordList; // String array to store the words we read in for sorting
    
    /*
    * Checks to see if valid arguments are passed in
    * if it was, check to see if file argument is a directory or normal file
    * if not, exit with failure
    */
    if(argc > 2){
        err = stat(argv[2], &statbuf);
        // Checking to see if fstat returned a valid stat struct
        if(err){
            perror(argv[2]); //printing out what went wrong when accessing the file
            return EXIT_FAILURE;
        }

        // file argument is a directory
        if(S_ISDIR(statbuf.st_mode)){
            dr = opendir(argv[2]);
            if(dr <= 0 ){ 
                puts("Could not open directory");
            }
        } else{ // file argument is a regular file
            fd = open(argv[2], O_RDONLY);
            if(dr <= 0){
                puts("Could not open file");
                return EXIT_FAILURE;
            }
        }
    } else { // fall through case when not enough arguments are passed
        puts("Not enough arguments");
        return EXIT_FAILURE;
    }

    /* At this point, a valid file or directory must have been opened and assigned to either global variable fd or dr
    * One of them must be negative (but not both), to indicate which one we are using for our program
    */

    // Scenario in which we have a file
    if(fd != -1){
        wordList = readFile(fd);
        sort(wordList);
        writeContent(wordList, argv[2]);
    } else { // Situation in which we have a directory
        while ((dp = readdir(dr)) != NULL)
        {
            struct stat stbuf;
            if( stat(filename_qfd,&stbuf ) == -1 )
            {
            printf("Unable to stat file: %s\n",filename_qfd) ;
            continue ;
            }

            if ( ( stbuf.st_mode & S_IFMT ) == S_IFDIR )
            {
            continue;
            // Skip directories
            }
            else
            {
            char* new_name = get_new_name( dp->d_name ) ;// returns the new string
                                                // after removing reqd part
            sprintf(new_name_qfd,"%s/%s",dir,new_name) ;
            rename( filename_qfd , new_name_qfd ) ;
            }
        }
        }
    }

}
