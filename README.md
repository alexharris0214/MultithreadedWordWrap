# WordWrap
- CS214
- Group 39
# Authors
- Maxim Yacun (NETID: my405) and Alexander Harris (NETID: ajh273)

# Design
### Reading Inputs: ###
- We first validate the inputs within our main method. The first thing to check is the number of inputs. If we have one additional input, we know to work with standard input. If we have two additional inputs, the next thing to check is whether the third input is either a file or a directory. We use a statbuf struct to peform this check, and then proceed to call our normalize method with the proper inputs. 
- (Normalize parameters described later in test plan). 
- If no additional inputs were passed in to our program, we exit with EXIT_FAILURE.

### Calling Normalize Method ###
- The normalize method is where our word wrapping algorithm exists. It contains three parameters as follows: int inputFD (the filedescriptor for out input file, int linelength (the size that our lines should be wrapped to), and int outputID (the file descriptor of our  output file).
- In the scenario where only the line length is provided to our program, we will make the following call to normalize "normalize(0, argv[1], 1)" the inputFD parameter is set to 0, as this refers to the file descriptor for standard input, which is where the data that will be wrapped will be stored. The line length parameter is set to be the second argument in our argv list, since argv[0] contains the name of our program. Finally, outputFD is set to 1, since this is the file descriptor for standard output
- In the scenario where we have two additional program arguments and the second argument is a regular file, we will make the following call to normalize "normalize(fd, argv[1], 1)", the inputFD parameter is set to be fd, which is the name of the file descriptor corresponding to the name of the input file, given by argv[2], on which we call open on and assign to the value of fd. We are still writing to standard output, so outfd is set to be 1.
-  In the scenario where we have two additional program arguments and the second argument is a directory file, we will make the following call to normalize "normalize(fd, argv[1], outputFD)". These are a series of calls that are done within a while loop, where the condition is there are more directory entries to be read. First, we create a direcotry pointer and assigin it to dr using opendr. Then, while there are more directory entries to be read, use open on each individual file that is not ".", "..", or a file containing the prefix "wrap". Each file is then assigned a file descrptor fd using open, and then passed to the normalize function one at a time. The outputFD descitor is created accoding to each file inputs name. It is created/opened with the open functin, with the O_CREATE parameter passed in the situration where it has not yet been made, and is created with prefix "wrap." before the input file's name.

### Normalize Method ###
- The general stucture of normalize is to:
  -  Read the file's contents at a certain set of bytes at a time 
  -  Store the bytes read in in a buffer
  -  Parse the buffer and recognize special characters such as white space and newlines
  -  Store the non white space characters in a currentWord variable to process words between reads
  -  Write to the output file once currentword contains the complete word
  -  Repeat until there are no more bytes to process
  -  Close all open files
  -  Return with appropiate return flag

- Our buffer is set to be a default of 16, defined as macro BUF_SIZE, but this can work with any size buffer
- We start off by first initalizing/declaring variable such as currword, which will hold our current word, and buffer to contain that read has gotten
- A initial call to read is called first to see if there are bytes to be read in the first place, which is then stored in the buffer
- We then enter our main loop that continues as long as read does not return 0
- The buffer is then iterated over one at a time, and each character is checked to be either a non-whitespace character, a whitespace character, or a newline character
  - In the situation where we have a non-whitepsace character, we
  - In the situation where we have a whitespace character, we
  - In the situation where we have a newline character, we
# Test Plan

