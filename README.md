# MultiThreaded WordWrap
- CS214
- Group 53
# Authors
- Maxim Yacun (NETID: my405) and Alexander Harris (NETID: ajh273)

# Design
### Data structures ###
- Two queues are globally maintained for storing files to be normalized and directories to be traversed. This uses a queue struct, which contain a mutex and cond for synchronizing, a value to tell whether the queue has been closed, and a start and end node.
- "Nodes" are a linked list structure which contain a pointer to a pathName struct and a pointer to the next node in the list.
- For ease of composing filenames when they need to be used, the data stored in our queues' nodes are pathName structs.
  - the pathName struct contains two strings which represent the "prefix" of a file's path and the actual file's name. For example, a file named "example.txt" inside of the directory "dir" will have a prefix of "dir" and a fileName of "example.txt". Our program has helper functions to compose an "input name" and an "output name" from a given pathName struct. This basically adds the necessary slashes and combines these two strings into a single path to the file which the struct represents for use in opening files or directories. The only difference between an "input name" and an "output name" is that the filename has "wrap." added before it.
- The flag struct is most easily described as an integer with a mutex. This is used for globally tracking the number of active directory threads, as well as globally tracking the status which the program should exit with.

### Main method: Preparing the fileQueue and dirQueue for traversal ###
- We first ensure the inputs within our main method have the proper number for non-recursive or recursive mode. The arguments to the program are also checked for validity throughout, such as for "-rM,N" where M and N need to be positive integers. 
- The program determines whether it is running in recursive mode or non-recursive mode by checking for the first argument to start with "-r"
  - This is later used to ensure the recursive and non-recursive modes are not used incorrectly, and for running different parts of main depending on the application
- After the lineLength argument is tested for validity and converted from string to integer, the rest of the arguments are assumed to be files and directories (if they are not existing files/dirs, the program will detect it later on and report the issue). We initialize the global data structures (queues and flags) and then iteratively enqueue these files and directories to the fileQueue and dirQueue, respectively. 
  - If no files or directories are given as arguments and the program is not in recursive mode, it reads from STDIN and outputs to STDOUT, after which it exits with whatever status normalize() returns. If the program is in recursive mode and no files or directories are given, it reports the issue and exits with EXIT_FAILURE.
- If the program is being run in recursive mode, the wrapper threads and directory threads are created to deal with the enqueued files and directories in their respective queues. The worker threads are immediately joined to the main thread once they terminate, the rest of dynamically allocated data is freed, and the program exits with whatever status is stored in the global exitFlag.
- If the program is being run in non-recursive mode, main reuses the same basic machinery as recursive mode to process files. First, main dequeues all of the files given as arguments and outputs their wrapped version to STDOUT. Then, it dequeues all directory arguments and enqueues every file contained in only the top-level directory (and not in subdirectories). Lastly, it dequeues all remaining files in the file queue (those which were contained in directories) and outputs the wrapped version to "wrap.filename" wherever the original file is located. Main deallocates all dynamically allocated data and exits with the globally stored exitFlag.

### Working with Threads ###
- In our main method, we first initialize the number of threads for the file worker method, which takes care of normalizing the files within a root directory and it's subdirectories, and the number of threads for the directory worker method, which takes care of traversing the directories and sending files to the fileworker for it to work on.
- The way these two workers collaborate/communicate is with use of the fileQueue. Directory threads add to it and wrapper threads take from it.
- The directory workers contains a directory queue, which is locked so only one thread can enqueue and dequeue at a time.
- Whenever the directory worker encounters a directory file, it enqueue's it onto the directory queue, for another thread to pick it up. If it encounters a regular file, it enqueues it onto the file queue.
- The file worker will then pick up any files from the fileQueue, one thread at a time, and begin to call the normalize method and place it's output in the correct spot.
- The fileWorker threads will repeat this process until the fileQueue is empty AND the directory queue is closed, meaning that there are no more files coming into the queue.
- The dirQueue threads will repeat until the directoryQueue is empty AND there are no active directory threads currently traversing a directory, signaling that there are no more subdirectories/files to be read.
   1. In order to do this, we created a flag struct that keeps track of the number of current threads running. This is done by having an integer variable within that struct, which is incremented and decremented at the beginning and end respectively of the main loop of directory worker.
   2. This struct is initiated globally to be accessed by all threads.
   3. Therefore, this struct also contains a mutex lock so that only one thread can modify this value at a given time.
   4. Any given thread should initialize this variable when enqueuing a directory, and decrement when done reading through the directory it picked up.
- The locking mechanism for the queue struct lies within the queue's enqueue and dequeue method. Since there were multiple parts of the directory and file worker that could be worked on asynchronous, it was better to leave the mutual exclusion logic within the enqueue and dequeue methods since they were responsible for accessing and modifying the queues in order to minimize lock and unlock calls.
- The amount of active directory threads is kept track by a global struct of type flag that lives within the directory worker method. 
- The threads (both file and directory) are created and joined within the main method and do not return any values
- In order to catch the error flag that can be encountered in numerous different locations of our program, we create another struct of type flag globally called errorFlag.
    1. The integer value of this struct represents the error condition encountered within our program at any given time.
    2. Since any thread could encounter an error at any given type, its lock is also used to ensure only one thread can update the error flag's condition
    3. Some sources of errorFlag being updated are: When normalize does not return successful, if a thread encounters any type of error opening a file, directory, or any general error condition that may prevent the program from completing such as malloc return types, etc.


### Calling Normalize Method ###
- The normalize method is where our word wrapping algorithm exists. It contains three parameters as follows: 
  - int inputFD (the file descriptor for the input file)
  - int lineLength (the size that our lines should be wrapped to)
  - int outputID (the file descriptor of our output file)
- In the scenario where only the line length is provided to our program, we will make the following call to normalize: "normalize(0, argv[1], 1);". The inputFD parameter is set to 0, as this refers to the file descriptor for standard input as a source. The line length parameter is set to be the second argument in our argv list (argv[1]), since argv[0] contains the name of our program. Finally, outputFD is set to 1, since this is the file descriptor for standard output.
- In the scenario where there are two arguments given to the program and the second argument is a regular file, we will make the following call to normalize: "normalize(fd, argv[1], 1);". Before calling normalize(), open is called on the input file given in argv[2] and assigned to fd. This scenario still requires writing to standard output, so outputFD is set to 1.
-  In the scenario, there are two arguments provided and the second argument is a directory file, we will make the following call to normalize: "normalize(fd, argv[1], outputFD)". This is done serially within a while loop with each file in the directory provided by argv[2]. First, we create a directory pointer and assign it to dr using opendr. Then, while there are more directory entries to be read, use open on each filename if it does not start with "." or "wrap." (if it does, move on to the next filename). The file is then assigned a file descriptor fd using open, and then passed to the normalize function one at a time as above. The outputFD descriptor is assigned to a file with the same filename as the input file but with the prefix "wrap.", using a helper function getOutputName(filename). The new output filename returned from getOutputName is passed to the open function using the O_CREATE flag (for the situation where it has not yet been made) to get the file descriptor passed to outputFD.

### Normalize Method ###
- The general structure of normalize is to:
  -  Read the file's contents to a buffer, BUFFER_SIZE bytes at a time 
  -  Parse the buffer character-by-character and recognize special characters such as white space and newlines
    - Store the non-whitespace characters in a write buffer called currWord (to allow storage of words which span calls of read())
    - Each time a whitespace character is reached (i.e. the end of a "word"), 
      - Start a new paragraph if needed by writing two consecutive newlines to the output file (the need for this is kept track of using a variable newPG)
      - For words not at the beginning of a new line, write a single space to output
      - For words which will not fit at the end of the current output file line, write a single newline to output
      - Finally, write the output buffer currWord to the output file
  -  Repeat until read returns no more bytes to process
  -  If there is a final word left in the currWord buffer, such as a file which ends in a non-whitespace character, write it to output with the necessary whitespace
  -  Return with EXIT_FAILURE if a word was too long to fit in a line or the input file was empty, return with EXIT_SUCCESS otherwise


- Our read buffer is set to be a default of 16, defined as a macro with the name BUFFER_SIZE, but this works with any size buffer
- Any whitespace written to output is always written immediately before a new word is written. 
  - This was a deliberate choice which makes it easier to prevent spaces without words after them or empty paragraphs from being written to the output file.
  - Although the program moves through the buffer character-by-character, the necessary trackers are made to note the current spot in a given line in the output file, consecutive newline characters, or when a new paragraph needs to be started before the next write of a word. All of these are used to ensure text is properly normalized to the lineLength parameter and has spaces only between words and exclusively 1-2 newline characters between lines.
- We start off by first initializing/declaring variables such as currWord (the write buffer), trackers for important characters in input and current location in output, and a buffer to contain what read has gotten from input
  - The currWord buffer is initially the size of line length + 1 (extra \0 character at the end to allow for strlen() functionality).
    - currWord's size is doubled to accomodate consecutive non-whitespace characters exceeding the length of a line 
    - therefore, size of currWord later denotes whether there was a word that exceeds the line length
- An initial call to read is done first to see if there are any bytes to be read from input, which is then stored in the buffer
- We then enter our main loop that continues as long as read gets back more than 0 bytes
- The buffer is then iterated over one at a time, and each character is checked to be either a non-whitespace character or a whitespace character (with newlines as a special subset of whitespace)
  - In the situation where we have a non-whitespace character, we will add this value to currWord
  - In the situation where we have a whitespace character, we will write the contents of currWord to the file, preceded by a space character
  - In the situation where we have a newline character, we will note that we have seen a new line character, and if the next character in the buffer/future buffer is also a new line, we recognize this as a paragraph scenario with the tracker newPG. Otherwise, we will ignore this new line character
   - When there is another word to be written to output, newPG will signify to start a new paragraph before writing this new word.

# Test Plan 

### PA3 Requirements ###
1) Correctly wrap files as specified in PA2
2) Recursive directory traversal: When given the "-r" argument, ww will wrap the files in the given directory and all subdirectories
3) Multi-threaded wrapping: When given the argument "-rN", ww will use N threads for wrapping all files in given directory and all subdirectories
- N must be a positive integer
4) Multi-threaded directory traversal: When given the argument "-rM,N", ww will use N threads for wrapping files and M threads to read through the given directory and its subdirectories.
- M must be a positive integer
5) All directory threads terminate at the same time; once the queue is empty and no directory thread is actively traversing a directory, the queue must "close" and the directory threads exit
- The program must keep track of the number of active directory-traversing threads so that the directory queue can be closed when they are finished
- The program must have a way to detect whether the queue is empty so that directory threads can wait for more directories to traverse or exit when the queue is closed
6) Must maintain synchronized, dynamic queues accessible to multiple threads for storing files to wrap and directories to be traversed.
- Requires properly functioning mutual exclusion and synchronization
- The program should not be able to enter deadlock
7) ww must not exit with any warnings from AddressSanitizer or LeakSanitizer
8) The program must maintain a global value to track whether any thread encounters an error condition so that ww can return the correct exit code
9) The program should detect when the arguments given to it are invalid, report this to the user, and stop.
10) EXTRA CREDIT: The user should be able to give multiple files and directories as arguments in either non-recursive or recursive mode which should be processed correctly respective of the mode chosen
- Files given directly as arguments should output to STDOUT
- When in non-recursive mode, files inside of subdirectories of directories given as arguments should not be normalized
- In recursive mode, every file in every subdirectory of directories given as arguments should be normalized properly 

### Test Plan for Wrapping all files correctly as in PA2, non-recursive mode (condition 1) ###
Overall, the test plan first involves three major I/O cases: STDIN to STDOUT, file to STDOUT, and directory to files
  - We run the program in all three of these cases (for the first, simply piping example1.txt or example2.txt to STDIN and comparing to the second case) to ensure our file I/O is working properly

Beyond trying different line length arguments, the test plan also involves a variety of formats for input text to ensure our normalizing function properly.
- Our test plan consists of the following files: example1.txt, example2.txt, and a moreExample directory containing
  - .example0.txt
  - example1.txt
  - example2.txt
  - example3.txt
  - example4.txt
  - example5.txt
- In example1.txt, we check a single paragraph with multiple lines, where no line has more than one consecutive white space character. This is an easy case where not much formatting is required beyond normalizing to the line length.
- In example2.txt, we check multiple paragraphs that contain some lines which are preceded by many white space characters. This checks the case where we have new paragraphs beginning with white space (such as a Tab indent) in the input to ensure our program ignores this. As well, it checks that newlines within a paragraph that are preceded with white space properly start with non-whitespace characters in the output.
- With our moreExample directory, we first and foremost test the functionality of the directory feature, but within each example file we test various different edge cases. The following test cases lie within this directory
  - .example0.txt: this is the same file as example4.txt, except it has a prefix of "." to ensure our program properly skips over files with this prefix
  - example1.txt: this is the same file as example1.txt explained above
  - example2.txt: this is the same file as example2.txt explained above
  - example3.txt: this file contains a very long word which serves as an easy test for when a word is longer than the given line length. Furthermore, it contains a large number of whitespace characters (spaces, tabs, and newlines) before and after paragraphs to ensures that the program properly normalizes paragraphs to always start with non-whitespace. Multiple newlines after the final paragraph makes sure the program properly ignores these and the output ends with exactly one newline character.
  - example4.txt: a single non-whitespace character in our file. this tests the extreme minimum for output file size, which the assignment specifies should be two bytes; a non-whitespace character followed by a newline.
  - example5.txt: an empty file. As the assignment does not specify the exact behavior in this scenario, it was our decision to make the program create an output file that is also empty and this tests that our program successfully does that.

- In addition to files we have manually created, we also test with standard input as our source file. All of the edge cases that previous test cases have covered in previous test cases (excluding paragraphs and line breaks since we cannot test them within standard input), have been tested with standard input. Each line that is entered is immediately wrapped and outputted to standard output (this behavior is exactly as seen in a class demonstration of the professor's working version of WordWrap). The command prompt then waits for more bytes to be written to standard input until EOF is signified to standard input with CTRL + D.
- To verify differences in white spacing where the contents of non-whitespace is the same, we create two input files "example1.txt" and "example1B.txt" with two different versions of white space. We use STDOUT to pipe the output from each test case to files named "output1.txt" and "output2.txt". We then use cmp to verify the program is writing the same output in both cases.
- Finally, To verify an edge case where an already-formatted file is passed to the program, we used file to STDOUT I/O with example2.txt as input and STDOUT piped to a file named output1.txt. The program is then run one more time with output1.txt as input and STDOUT piped to a file named output2.txt. The program "cmp" is then run with output1.txt and output2.txt as parameters to ensure that these files are exactly the same. 


### Test Plan for Recursive Directory Traversal Mode ###
For the recursive directory traversal method, we can safely make the assumption that normalize is working as intended with the above test cases working as intended. These test cases will test potentially problematic edge cases working with multiple threads and different directory structures.

While running these test cases, we use the printf() function in our thread workers to display which threads start and exit when, to ensure we are using a proper amount of threads as the user requests as well as closing them all at the right time. (Tests conditions 2, 3, and 4 for thread number) 
Additionally, all of our tests utilize LeakSanitizer and AddressSanitizer including when compiling ww; this ensures test condition #7 since we have no systematic way of freeing data all at once that requires any testing.

- To rigorously test conditions 2, 3, 4, and 5, we have a directory structured like below:
                              
                              root
                subdir1               subdir2
          (some text files)     (some text files)   subdir3      subdir5
                                                    subdir4
                                               (some text files)
                                               
  This evaluates every possible structure: multiple subdirectories in one, a directory with only files, a directory with files and subdirectories, several nested subdirectories, and an empty subdirectory. We run the program with the "-r", "-rN", and "-rM,N" modes while tracking the opening and closing of threads to ensure the proper amount are used. 
   In the "-rM,N" case, we test cases where M = N, M is significantly higher than N, and N is significantly higher than M to ensure that either thread type outrunning the other does not cause issues.
   To test condition #6, we run the program with print statements that display the accessing thread and clock time before and after each attempt to lock and unlock a mutex. This allows us to verify that our mutual exclusion method allows access to shared data to only one thread at a time. Further, we can prove that our program will not enter deadlock because there is only one place where two mutexes are locked at the same time. This way, two threads cannot be waiting to access the two different resources being locked; a thread must have already locked one resource to wait for the other every time.
   
 - In the next test case is our "base case" for directory traversal. We have a directory structure with a couple of test files for wrapping within the root directory. This is to assure that the word wrap still works as intended from last project with multi-threading. This is also done to test that the directory queue is closing correctly, despite nothing ever being enqueued by the directory threads.
- We also test an edge case where we have a directory structure with multiple subdirectories, all having no regular files within them, including the root directory. This is done to assure the file threads are closing properly, even when never being used to wrap files.

- To test condition #8, we give the program a single empty text file among normally populated text files to wrap and run it in recursive mode with multiple file-wrapping threads. normalize() prints an error message and returns EXIT_FAILURE if it runs into an empty text file. This test verifies that global tracking of exit code works as intended, since only one file worker will encounter this EXIT_FAILURE from normalize while all others should get EXIT_SUCCESS. We verify that ww returned EXIT_FAILURE by running "echo $?", which results in "1" when this test case is successful.

- To test condition #9, we run WordWrap with invalid arguments such as -r0,0 or a file that does not exist to ensure the program reports back the correct error and fails to execute. Additionally, we test that the program detects when arguments are not formatted correctly by testing without a lineLength argument and giving no file arguments when in recursive mode. 

### Test Plan for Extra Credit (condition 10) ###
To test that providing multiple directories and files works as intended, we pass the "root" directory described above as well as a second, similar directory and a third empty directory. In between these directories as arguments, we also give two different text files in the same directory as the executable to ensure that they both get output to STDOUT. We try this same structure in both recursive and non-recursive mode, and then verify that in recursive mode, all files in every subdirectory are normalized, and that in non-recursive mode, only files inside of the immediate directory given are normalized. 

The program should still be able to take data from STDIN and output to STDOUT in non-recursive mode (it would not make sense to do this in recursive mode, so it is explicitly disallowed in our implementation). We test by giving the program only a line length argument to ensure that it works exactly as in Project II when using this mode.

The program still needs to maintain the ability to return with the right exit code if an error is encountered. We pass one empty file in one of the arguments, with the rest being unproblematic to ensure that running ww with these results in an exit code of "1" when checked with "echo $?". 

Lastly, there are a couple of edge cases we thought would be helpful to check for validity: 
- The first is when we specify recursive mode but provide a single file; the file is wrapped using a singular file thread, and the directory threads and the rest of the wrapper threads do nothing besides being created and destroyed. 
- We also test the case where we provide a file that is within a directory; in this case, main needs to detect that there is a prefix for this file and enqueue it with the proper prefix so that it can later be found by the file system. For example, we give "/root/example1.txt" as an argument.
