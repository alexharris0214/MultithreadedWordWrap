#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>

#define BUFFER_SIZE 16

struct pathName {
    char *prefix;
    char *fileName;
};

struct node {
    struct pathName *path;
    struct node *next;
} node;

struct queue {
    struct node *start;
    struct node *end;
    pthread_mutex_t lock;
    pthread_cond_t *dequeue_ready;
} *fileQueue, *dirQueue;

int activeDThreads = 0;

/*
* char *output: a string containing a filename which needs an output file to write to
*
* Returns a string with the prefix "wrap." in front of the string passed to the char *output parameter.
*/
char *getInputName(struct pathName *file){
    char *inputName = (char *)malloc(sizeof(char)*(strlen(file->prefix) + strlen(file->fileName) + 1));

    memset(inputName, '\0', sizeof(inputName));
    strcat(inputName, file->prefix);
    strcat(inputName, file->fileName);

    return inputName;
}
char *getOutputName(struct pathName *file){
    char *outputName = (char *)malloc(sizeof(char)*(strlen(file->prefix) + strlen(file->fileName) + 6));
    memset(outputName, '\0', sizeof(outputName));
    
    char *wrap = "wrap.\0";

    strcat(outputName, file->prefix);
    strcat(outputName, wrap);
    strcat(outputName, file->fileName);

    return outputName;
}

/*
*   int inputFD: a file descriptor which points to source of input data,
*   int lineLength: describes number of characters in a line for output formatting
*   int outputFD: a file descriptor pointing to the file to write formatted input to
*      (if NULL, outputs to STDOUT)
*   
*   Uses an input buffer to read data from input file fd,
*   processes data from input buffer character-by-character,
*   composing non-whitespace characters into words via output buffer,
*   and then using whitespace characters as cues for writing words 
*   or starting new paragraphs in the output file.
*/
int normalize(int inputFD, int lineLength, int outputFD){
    
    int lineSpot = 0;           //keeps track of the spot we are at in the current output file line
    int seenTerminator = 0;     //keeps track of consecutive newlines; if the last character was a newline, seenTerminator = 1
    int newPG = 0;              //keeps track of when to start a new paragraph before writing a new word
    int i;

    char buffer[BUFFER_SIZE];                                           //char array buffer for data read from input
    char *currWord = (char *)malloc(sizeof(char)*(lineLength + 1));     //string buffer for words to write to output
    int currWordSize = sizeof(char)*(lineLength + 1);                   //keep track of output buffer size for calculations & error-checking
    memset(currWord, '\0', currWordSize);                               //initialize buffer with null chars (it is a string)
    
    int bytesRead; // variable keeping track of how many bytes were read in

    bytesRead = read(inputFD, buffer, BUFFER_SIZE);
    if(bytesRead == 0){         //if file is empty, we can continue no further
        puts("ERROR: Input appears to contain zero bytes.");
        free(currWord);
        return EXIT_FAILURE;
    }         
    while(bytesRead > 0){
        for(i = 0; i < bytesRead; i++){         //iterate thru bytes read, one char at a time
            if(isspace(buffer[i])){ 

                if(buffer[i] == '\n'){
                    if(seenTerminator){
                        newPG = 1;
                        lineSpot = 0;
                    } else {
                        seenTerminator = 1;
                    }
                } else {
                    seenTerminator = 0;
                }

                if(currWord[0] != '\0'){        // go on to write currWord to output on reaching whitespace if currWord is non-empty
                    if(newPG){   //start a new paragraph before the next word is written if needed
                        write(outputFD, "\n\n", 2);
                        newPG = 0;
                        lineSpot = 0;
                    }
                    if(lineSpot == 0){                                  //if at the beginning of the line, write just the current token regardless of length
                        write(outputFD, currWord, strlen(currWord));
                        lineSpot += strlen(currWord);
                    } else if(lineSpot + 1 + strlen(currWord) <= lineLength){ //if not at the beginning of a line and the current word plus a space fits, add it to the current line
                        write(outputFD, " ", 1);
                        write(outputFD, currWord, strlen(currWord));    //write currWord preceded by a space
                        lineSpot += (1 + strlen(currWord));
                    } else {                                            //currWord does not fit at the end of the current line. start a new line
                        write(outputFD, "\n", 1);                       
                        write(outputFD, currWord, strlen(currWord));
                        lineSpot = strlen(currWord);
                    }
                    memset(currWord, '\0', currWordSize);           //clear currWord buffer and start a new word
                }

            } else {                                                //if the current character is non-whitespace, then we can add it to the write buffer
                seenTerminator = 0;
                if(strlen(currWord) == (currWordSize - 1)){         //if adding a character will exceed string buffer (-1 for the final \0 character), 
                    currWord = (char *)realloc(currWord, (currWordSize*2));   
                    currWordSize *= 2;      //currWordSize will serve as an indicator for word > lineLength error
                    memset((currWord + currWordSize/2), '\0', (currWordSize/2));   
                }
                currWord[strlen(currWord)] = (char)buffer[i];
            }
        }
        // Resetting the buffer before reassigning to it
        memset(buffer, '\0', sizeof(buffer));
            
        bytesRead = read(inputFD, buffer, BUFFER_SIZE);
    }
    //Need to print last word after program is finished reading input
    if(currWord[0] != '\0'){
        if(newPG){   //start a new paragraph before the next word is written if needed
            write(outputFD, "\n\n", 2);
            lineSpot = 0;
        }
        if(lineSpot == 0){                                        //if we're at the beginning of the line, just write the word
            write(outputFD, currWord, strlen(currWord));
        } else if(lineSpot + 1 + strlen(currWord) <= lineLength){ 
            write(outputFD, " ", 1);
            write(outputFD, currWord, strlen(currWord));          //write currWord preceded by a space
        } else {                                                  //if the word plus a space didn't fit, we need to put it on a new line
            write(outputFD, "\n", 1);
            write(outputFD, currWord, strlen(currWord));
        }
    }

    //every file must end in a newline
    write(outputFD, "\n", 1);

    if(currWordSize > (sizeof(char)*(lineLength + 1))){       //we had a word larger than a line; need to error out
        puts("ERROR: Input contains a word longer than specified line length.");
        free(currWord);
        return EXIT_FAILURE;
    }

    free(currWord);
    return EXIT_SUCCESS;
}


void init_queue(struct queue *queue){
    queue->start = NULL;
    queue->end = NULL;
}

void enqueue(struct queue *queue, struct pathName *file){
    pthread_mutex_lock(&queue->lock);

        node *temp = malloc(sizeof(node));

        // checking to see if malloc returned correctly
        if(temp == NULL){
            return EXIT_FAILURE;
        }
        // allocating new node (temp) accordingly
        temp->path = *file;
        temp->next = NULL;
        
        // if the end exists, attach the new node to the next pointer of end
        if(queue->end != NULL){
            queue->end->next = temp;
        }
        queue->end = temp; // change the end to point to the new node

        if(queue->start == NULL){ // make sure that head always points to something if data exists
            queue->start  = temp;
        }

        struct node *new = (struct node *)malloc(sizeof(struct node));
        new->next = NULL;
        new->path = file;
        queue->end->next = new;
        queue->end = new;
        
        pthread_cond_signal(&queue->dequeue_ready);

    pthread_mutex_unlock(&queue->lock);
    return;
}

struct pathName *dequeue(struct queue *queue){
    pthread_mutex_lock(&queue->lock);

        //cond_wait for dequeue->ready
        //set a temp value to current queue->start ; we will use this temp value to return later
        //set queue->start to temp->next
        //save pathname of temp
        //free temp

        pthread_cond_wait(&queue->dequeue_ready, &queue->lock);

        struct node *temp = queue->start;
        queue->start = temp->next;
        struct pathName *dequeuedFile = temp->path;
        free(temp);

    pthread_mutex_unlock(&queue->lock);

    return dequeuedFile; //return pathname of temp
}

void *fileWorker(void * arg){
    while(activeDThreads || fileQueue->start != NULL){
        struct pathName *dequeuedFile = dequeue(fileQueue);
        char *currFile = getInputName(dequeuedFile);
        free(dequeuedFile);
        
        int fd = open(currFile, O_RDONLY);
        free(currFile);
        if(fd <= 0){ // checking to see if file is opened successfully
        puts("ERROR: Could not open file.\n");
            return EXIT_FAILURE;            //FIXME: make this change the global exit status using mutex
        }
        // reformatting file
        normalize(fd, arg, 1); //FIXME: make global exit status with mutex to keep track of normalize() exit status
        close(fd);
    }
}

void *dirWorker(void * arg){
    while(!dirQueue->start == NULL || activeDThreads){
        //struct pathName *dequeuedDir = dequeue(dirQueue); should we just use this to block?
        activeDThreads++;
        struct dirent *dp;
        struct stat statbuf; // Holds file information to determine its type

        pthread_mutex_lock(&dirQueue->lock);
        activeDThreads++;
        pthread_mutex_unlock(&dirQueue->lock);

        struct pathName path = dequeue(dirQueue);
        DIR *dr = opendir(strcat(path.prefix, path.fileName));
        
        // iterating through current directory
        struct pathName *newPath;
        while((dp = readdir(dr)) != NULL){
            stat(dp, &statbuf);
            // checking to see if a file or directory was read
            if(S_ISDIR(statbuf.st_mode)){
                //MODIFY NEW PATH HERE
                enqueue(dirQueue, newPath);
            } else {
                //MODIFY NEW PATH HERE
                enqueue(fileQueue, newPath);
            }
        }


        pthread_mutex_lock(&dirQueue->lock);
        activeDThreads--;
        pthread_mutex_unlock(&dirQueue->lock);
   }
}

int main(int argc, char **argv)
{

    int fd = -1; // File pointer
    DIR *dr = NULL; // Directory pointer
    struct stat statbuf; // Holds file information to determine its type
    struct dirent *dp;  // Temp variable to hold individual temp entries
    int err; // Variable to store error information
    int exitFlag = EXIT_SUCCESS; //determine what exit status we return

    int numOfWrappingThreads = 0;
    int numOfDirectoryThreads = 0;

    /*
    * checking to see if arguments were passed in before accessing
    * returns exit failure if no arguments were passed in 
    */
   if(argc==4){
        if(strlen(argv[1]) ==2){
           numOfWrappingThreads = 1;
           numOfDirectoryThreads = 1;
        } else if(strlen(argv[1]) == 3){
           numOfWrappingThreads = atoi(argv[1]);
           numOfDirectoryThreads = 1;
        } else if(strlen(argv[1]) == 5){
           numOfWrappingThreads = argv[1][4] - '0';
           numOfDirectoryThreads = argv[1][2] - '0';
        } else {
           puts("Invalid argument for thread mode");
           return EXIT_FAILURE;
        }

        pthread_t wrapperThreads[numOfWrappingThreads];
        pthread_t dirThreads[numOfDirectoryThreads];

        struct pathName *path = malloc(sizeof(struct pathName));
        path->prefix = "";
        path->fileName = argv[2];
        
        enqueue(dirQueue, path);
        for(int i = 0; i<numOfDirectoryThreads; i++){
            pthread_create(&dirThreads[i], NULL, dirWorker, argv);
        }
        for(int i = 0; i<numOfWrappingThreads; i++){
            pthread_create(&wrapperThreads[i], NULL, fileWorker, argv);
        }
        for(int i = 0; i<numOfDirectoryThreads; i++){
            pthread_join(dirThreads[i], NULL);
        }
        for(int i = 0; i<numOfWrappingThreads; i++){
            pthread_join(wrapperThreads[i], NULL);
        }
   } else {
        if(argc>1){
            for(int i = 0; i < strlen(argv[1]); i++){ //check if length argument is a number
                if(!isdigit(argv[1][i])){               // checking each digit individually
                    puts("ERROR: Invalid lineLength argument; must be an integer.");
                    return EXIT_FAILURE;
                }
            }
        } else {
            puts("ERROR: Not enough arguments given.");
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

                int outputFD;
                while ((dp = readdir(dr)) != NULL){ // while there are files to be read

                    //ignoring files which start with "." or "wrap."
                    if(strstr(dp->d_name, ".") == dp->d_name || strstr(dp->d_name, "wrap.") == dp->d_name){
                        continue;
                    }
            
                    // open the current file that we are on and work on it
                    fd = open(dp->d_name, O_RDONLY);
                    if(fd > 0 && dp->d_name){ // checking to see if file opened successfully
                        char *outputFilename = getOutputName(dp->d_name);
                        outputFD = open(outputFilename, O_WRONLY|O_CREAT|O_TRUNC, 0700);    // creating file if it does not exist with user RWX permissions
                        free(outputFilename);                                        
                        if(outputFD <= 0){                                                  // checking to see if open returned successfully 
                            puts("ERROR: Could not open/create desired output file.\n");
                            perror("outputFD");
                            return EXIT_FAILURE;
                        }
                        if(normalize(fd, length, outputFD) == EXIT_FAILURE)     //save exit status to return later, as the program still needs to process the rest of the directory
                            exitFlag = EXIT_FAILURE;
                        close(outputFD);
                    }
                    // close file when we are done
                    close(fd);
                }
                // freeing directory object
                free(dr);
            } else if(S_ISREG(statbuf.st_mode)){ // file argument is a regular file
                fd = open(argv[2], O_RDONLY);
                if(fd <= 0){ // checking to see if file is opened successfully
                    puts("ERROR: Could not open file.\n");
                    return EXIT_FAILURE;
                }
                // reformatting file and assigning the result to return flag
                exitFlag = normalize(fd, length, 1);
                close(fd);
            } else {
                puts("ERROR: Invalid file type passed as argument.\n");
                    return EXIT_FAILURE;
            }
        } else if(argc==2){     // no file was passed in; reading from standard input
            normalize(0, length, 1);
            return EXIT_FAILURE;
        } else {                // fall through case when not enough arguments are passed
            puts("ERROR: Not enough arguments.\n");
            return EXIT_FAILURE;
        }
        return exitFlag;
   }
}