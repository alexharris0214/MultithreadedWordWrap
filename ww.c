#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define BUFFER_SIZE 16


/*
* Returns a string with the prefix "wrap."
* placed in front of the desired string passed in,
* which in this case is the file name of the input file
* lying within the specified directory
*/
char *getOutputName(char *output){
    char *outputFileName = (char *)malloc(sizeof(output) + 5); // 5 is hardcoded to be length of "wrap." (5 character)

    // manually assigning the prefix
    outputFileName [0]= 'w';
    outputFileName [1]= 'r';
    outputFileName [2]= 'a';
    outputFileName [3]= 'p';
    outputFileName [4]= '.';

    // assigning the rest of the string according the input file name
    for(int i = 0; i<sizeof(output); i++){
        outputFileName[5+i] = output[i];
    }

    return outputFileName;
}

/*
*   Uses an input buffer to read data from input file fd,
*   processes data from input buffer character-by-character,
*   composing non-whitespace characters into words via output buffer,
*   and then using whitespace characters as cues for writing words 
*   or starting new paragraphs in the output file.
*/
int normalize(int fd, int lineLength, char *output){
    int outputFile = 1; // by default, output is to STDOUT

    if(output != NULL){ // if we have a desired outputFile, switch to that
        char *outputFilename = getOutputName(output);
        outputFile = open(outputFilename, O_WRONLY|O_CREAT|O_TRUNC, 0700); // creating file if it does not exist with user RWX permissions
        free(outputFilename);
        if(outputFile <= 0){ // checking to see if open returned successfully 
            puts("ERROR: Could not open/create desired output file.\n");
            perror(outputFile);
            return EXIT_FAILURE;
        }
    }
    
    int lineSpot = 0;           //keeps track of the spot we are at in the current output file line
    int seenTerminator = 0;     //keeps track of consecutive newlines
    int paragraphFilled = 0;    //keeps track of if the current paragraph has any non-whitespace text in it yet
    int i;

    char buffer[BUFFER_SIZE];                                           //buffer for data read from input
    char *currWord = (char *)malloc(sizeof(char)*(lineLength + 1));     //buffer for words to write to output
    int currWordSize = sizeof(char)*(lineLength + 1);                   //keep track of output buffer size
    memset(currWord, '\0', currWordSize);

    int bytesRead;

    int inputFD = fd;

    if(fd == 0){    //if we are reading from stdinput, store input in a temporary file
        inputFD = fileno(tmpfile());
        bytesRead = read(fd, buffer, BUFFER_SIZE);
        while(bytesRead > 0){
            write(inputFD, buffer, bytesRead);
            memset(buffer, '\0', sizeof(buffer));
            bytesRead = read(fd, buffer, BUFFER_SIZE);
        }
        close(fd);
        lseek(inputFD, 0, SEEK_SET);
        memset(buffer, '\0', sizeof(buffer));
    }

    bytesRead = read(inputFD, buffer, BUFFER_SIZE);
    while(bytesRead > 0){

        for(i = 0; i < bytesRead; i++){
            if(isspace(buffer[i])){     //current character is whitespace
                if(currWord[0] != '\0'){
                    if(lineSpot == 0){                                  //if we're at the beginning of the line
                        write(outputFile, currWord, strlen(currWord));
                        lineSpot += strlen(currWord);
                    } else if(strlen(currWord) >= lineLength){
                        write(outputFile, "\n", 1);
                        write(outputFile, currWord, strlen(currWord));
                        write(outputFile, "\n", 1);
                        seenTerminator = 1;
                        lineSpot = 0;
                    } else if(lineSpot + 1 + strlen(currWord) <= lineLength){ //need to have room for a space preceding the word
                        write(outputFile, " ", 1);
                        write(outputFile, currWord, strlen(currWord));  //write currWord preceded by a space
                        lineSpot += (1 + strlen(currWord));
                    } else {
                        write(outputFile, "\n", 1);
                        write(outputFile, currWord, strlen(currWord));
                        lineSpot = (1 + strlen(currWord));
                    }
                    if(buffer[i] == '\n'){
                        seenTerminator = 1;
                    } else {
                        seenTerminator = 0;
                    }
                    memset(currWord, '\0', currWordSize);  //clear currWord and start a new word
                
                } else if(buffer[i] == '\n' && paragraphFilled){       //if currWord is empty and the current paragraph contains non-whitespace characters, check if we need to start a new paragraph
                    
                    if(seenTerminator){                 //if this is the second consecutive newline, we can start a new paragraph
                        write(outputFile, "\n\n", 2);
                        lineSpot = 0;
                        seenTerminator = 0;
                        paragraphFilled = 0;
                    } else {                            //if this is the first consecutive newline, keep track of it
                        seenTerminator = 1;
                    }
                }

            } else {                                                //if the current character is non-whitespace, then we can add it to the write buffer
                if(strlen(currWord) == (currWordSize - 1)){
                    currWord = (char *)realloc(currWord, (currWordSize*2));
                    currWordSize *= 2;
                    memset((currWord + currWordSize/2), '\0', (currWordSize/2));
                }
                paragraphFilled = 1;
                currWord[strlen(currWord)] = (char)buffer[i];
            }
        }
        // Resetting the buffer before we reassign to it
        // Only need to reset buffer to empty when bytes read will not completely overwrite
        memset(buffer, '\0', sizeof(buffer));
            
        bytesRead = read(inputFD, buffer, BUFFER_SIZE);
    }
    //Need to print last word
    if(currWord[0] != '\0'){
        if(strlen(currWord) >= lineLength){
            write(outputFile, "\n", 1);
            write(outputFile, currWord, strlen(currWord));
            write(outputFile, "\n", 1);
        } else if(lineSpot == 0){                                 //if we're at the beginning of the line
            write(outputFile, currWord, strlen(currWord));
        } else if(lineSpot + 1 + strlen(currWord) <= lineLength){ //need to have room for a space preceding the word
            write(outputFile, " ", 1);
            write(outputFile, currWord, strlen(currWord));        //write currWord preceded by a space
        } else {                                                  //if the word plus a space didn't fit, we need to start a new line
            write(outputFile, "\n", 1);
            write(outputFile, currWord, strlen(currWord));
            lineSpot = (1 + strlen(currWord));
        }
    }

    if(fd == 0)    //if we read from standard input, we must close the temporary file
        close(inputFD);

    if(currWordSize > (sizeof(char)*(lineLength + 1))){       //we had a word larger than a line
        printf("ERROR: File %d contains a word longer than specified line length.\n", inputFD);
        free(currWord);
        return EXIT_FAILURE;
    }

    free(currWord);
    return EXIT_SUCCESS;
}


int main(int argc, char const *argv[])
{
    /*handle incorrect arguments here*/

    int fd = -1; // File pointer
    DIR *dr = -1; // Directory pointer
    struct stat statbuf; // Holds file information to determine its type
    struct dirent *dp;                                                          /* WHAT IS DP? */
    int err; // Variable to store error information
    int exitFlag = EXIT_SUCCESS; //determine what exit status we return

    for(int i = 0; i < strlen(argv[1]); i++){ //check if length argument is a number
        if(!isdigit(argv[1][i]))
            return EXIT_FAILURE;
    }

    int length = atoi(argv[1]);

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
                puts("ERROR: Could not open directory.\n");
            }
            chdir(argv[2]); // Changing the working directory to have access to the files we need
            
            while ((dp = readdir(dr)) != NULL){ // while there are files to be read

                // case where name of file is just a reference to current/parent directory
                if(strcmp(dp->d_name, "..") ==0 || strcmp(dp->d_name, ".") ==0 || strstr(dp->d_name, "wrap") != NULL){
                    continue;
                }
        
                // open the current file that we are on and work on it
                fd = open(dp->d_name, O_RDONLY);
                if(fd > 0){
                    if(normalize(fd, length, dp->d_name) == EXIT_FAILURE)
                        exitFlag = EXIT_FAILURE;
                }
                close(fd);
            }
            free(dr);
        } else{ // file argument is a regular file
            fd = open(argv[2], O_RDONLY);
            if(fd <= 0){
                puts("ERROR: Could not open file.\n");
                return EXIT_FAILURE;
            }
            exitFlag = normalize(fd, length, NULL);
            close(fd);
        }
        // no file was passed in, reading from standard input
    } else if(argc==2){
        normalize(0, length, NULL);
        return EXIT_FAILURE;
    } else { // fall through case when not enough arguments are passed
        puts("ERROR: Not enough arguments.\n");
        return EXIT_FAILURE;
    }
    return exitFlag;
}
