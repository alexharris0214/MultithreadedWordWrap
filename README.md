# WordWrap
- CS214
- Group 39
# Authors
- Maxim Yacun (NETID: my405) and Alexander Harris (NETID: ajh273)

# Design
### Reading Inputs: ###
- We first validate the inputs within our main method. The first thing to check is the number of inputs. If we have one additional input, we know to work with standard input. If we have two additional inputs, the next thing to check is whether the third input is either a file or a directory. We use a statbuf struct to perform this check, and then proceed to call our normalize method with the proper inputs. 
- (Normalize parameters described later in test plan). 
- If no additional inputs were passed into our program, we exit with EXIT_FAILURE.

### Calling Normalize Method ###
- The normalize method is where our word wrapping algorithm exists. It contains three parameters as follows: 
  - int inputFD (the file descriptor for the input file)
  - int lineLength (the size that our lines should be wrapped to)
  - int outputID (the file descriptor of our output file)
- In the scenario where only the line length is provided to our program, we will make the following call to normalize "normalize(0, argv[1], 1)" the inputFD parameter is set to 0, as this refers to the file descriptor for standard input, which is where the data that will be wrapped will be stored. The line length parameter is set to be the second argument in our argv list, since argv[0] contains the name of our program. Finally, outputFD is set to 1, since this is the file descriptor for standard output
- In the scenario where we have two additional program arguments and the second argument is a regular file, we will make the following call to normalize "normalize(fd, argv[1], 1)", the inputFD parameter is set to be fd, which is the name of the file descriptor corresponding to the name of the input file, given by argv[2], on which we call open on and assign to the value of fd. We are still writing to standard output, so outfd is set to be 1.
-  In the scenario where we have two additional program arguments and the second argument is a directory file, we will make the following call to normalize "normalize(fd, argv[1], outputFD)". These are a series of calls that are done within a while loop, where the condition is there are more directory entries to be read. First, we create a directory pointer and assign it to dr using opendr. Then, while there are more directory entries to be read, use open on each individual file that is not ".", "..", or a file containing the prefix "wrap". Each file is then assigned a file descriptor fd using open, and then passed to the normalize function one at a time. The outputFD descriptor is created according to each file input's name. It is created/opened with the open function, with the O_CREATE parameter passed in the situation where it has not yet been made and is created with the prefix "wrap." before the input file's name.

### Normalize Method ###
- The general structure of normalize is to:
  -  Read the file's contents at a certain set of bytes at a time 
  -  Store the bytes read in a buffer
  -  Parse the buffer and recognize special characters such as white space and newlines
  -  Store the nonwhite space characters in a currentWord variable to process words between reads
  -  Write to the output file once currentword contains the complete word
  -  Repeat until there are no more bytes to process
  -  Close all open files
  -  Return with the appropriate return flag

- Our buffer is set to be a default of 16, defined as macro BUFF_SIZE, but this can work with any size buffer
- We start off by first initializing/declaring variable such as currWord, which will hold our current word, and buffer to contain that read has gotten
- A initial call to read is called first to see if there are bytes to be read in the first place, which is then stored in the buffer
- We then enter our main loop that continues as long as read does not return 0
- The buffer is then iterated over one at a time, and each character is checked to be either a non-whitespace character, a whitespace character, or a newline character
  - In the situation where we have a non-whitespace character, we will add this value to currWord
  - In the situation where we have a whitespace character, we will write the contents of currWord to the file, preceded by a white space character
  - In the situation where we have a newline character, we will not that we have seen a new line character, and if the next character in the buffer/future buffer is also a new line, we recognize this as a paragraph scenario. Otherwise, we will ignore this new line character
- If the currWord exceeds the line length, double the size of currWord and note that we have a word that exceeds the line length
- CONTINUE HERE
# Test Plan
- Our test plan consists of the following files: Example1.txt, Example2.txt, and a moreExample directory containing
  - Example1.txt
  - Example2.txt
  - Example3.txt
  - Example4.txt
  - Example5.txt
- In Example1.txt, we check a single paragraph with multiple lines, where no line has more than one consecutive white space characters. This is the normal base case.
- In Example2.txt, we check multiple paragraphs that contain some lines that are preceded with more than white space characters. This checks the edge case where we have new paragraphs beginning with white space, and newlines within a paragraph that are preceded with white space. The program should and successfully does ignore this white space, and starts by searching for the next word while having currWord be empty.
- Withing our moreExample directory, we first and foremost test the functionality of the directoy feature, but within each example file we test various different edge cases. The following test cases lie within this directory
  - In example1.txt, we contain a base case where we have a single paragraph with multiple lines, where each word is seperated by a single whitespace character. This is done to check if the normal case works for both the single file mode and directory mode.
  - In example2.txt, We have multiple paragaraphs with randomly generated Lorem Ipsum text. Some of these paragraphs contain lines where the beginning of the line is a series of whitespace characters, and we also have lines that are triple to quadruple the line length of other lines to test that our function works with varying line lengths of drastic degrees.
  - In example3.txt, we have a scenario where the start of the file is a series of new line characters. The program is expected to ignore these characters, and wait untill we have a valid nonwhitespace character, which it does succesfully. Furthermore, the end of file is preceeded by a series of consecutive new line characters, which our program succesfully ignores.
  - In example4.txt, we have a single nonwhitespace character in our file (not including the newline terminator). Our program should be expected the recognize single lines as it's own paragraph, and is expected to print a new line after the line ends, which it succesfully does so.
  - In example5.txt, we have an empty file. Despite a line being there (the presence of a new line character), our program should not recognize this as a valid line/paragraph, and not print out a new line, which it succesfully does so.

- ADD TEST CASE FOR STDIN TO STDOUT, FILE TO STDOUT, DIR TO FILES, and PIPE OUTPUT FROM FILE TO INPUT OF ANOTHER CALL   
  and call CMP to make sure THEY ARE THE SAME
