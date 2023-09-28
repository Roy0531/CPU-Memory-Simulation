#include <stdio.h>

#include <stdlib.h>

#include <unistd.h>

#include <sys/types.h>

#include <sys/wait.h>

#include <ctype.h>

// data struct used for communicating data between parent and child processes
struct Request {
  int r_or_w;
  int data;
  int address;
  int mode;
};

// extract integer instcution/data from a string line and trim comments
int trim(const char * str) {
  int num = -1;
  int i = 0;
  int flag = 0;

  // skip comma in a line 
  // ex) line: .1000 -> 1000
  if (str[0] == '.') {
    i++;
  }

  // keep reading until whitespace is encountered
  while (isdigit((unsigned char) str[i])) {
    if (num == -1) {
      num = 0;
    }
    num = num * 10 + (str[i] - '0');
    i++;
  }
  return num;
}

int main(int argc, char * argv[]) {
  if (argc != 3) {
    printf("Usage: %s <input_file> <integer value>\n", argv[0]);
    return 1;
  }

  pid_t pid;
  int c_to_p_fd[2];
  int p_to_c_fd[2];
  struct Request req;
  int timer = atoi(argv[2]);
  int instCnt = 0;
  int mode = 0;

  // check if pipe() succeeded
  if (pipe(c_to_p_fd) < 0 || pipe(p_to_c_fd) < 0) {
    printf("pipe error");
    exit(-1);
  }
  // check if fork() succeeded, if not, exit
  if ((pid = fork()) < 0) {
    //fork failed
    printf("Error occured while forking.");
    exit(-1);
  } else if (pid == 0) {
    // CNILD PROCESS(memory)

    // read an input file
    FILE * file;
    int memory[2000] = {
      0
    };
    char line[100];
    int mp = 0;
    int data;

    file = fopen(argv[1], "r");
    if (file == NULL) {
      // opening a file failed
      perror("Error opening input file");
      return 1;
    }

    // store data read into the memory array by reading line by line, then trim each line so only info needed gets stored
    while (fgets(line, sizeof(line), file) != NULL) {
      if (mp > 1999) {
        printf("Memory overflow");
        exit(-1);
      }

      // if lines beginning with a comma is encountered, change the address wherein next data is stored
      if (line[0] == '.') {
        data = trim(line);
        mp = data;
        continue;
      }

      data = trim(line);

      // return data == -1 indicates a space, so skip it
      if (data == -1) {
        continue;
      }

      memory[mp] = data;
      mp++;
    }
    fclose(file);

    close(p_to_c_fd[1]);
    close(c_to_p_fd[0]);

    // receive communication from the CPU repeatedly whether to read from or write into memory  
    while (read(p_to_c_fd[0], & req, sizeof(req)) > 0) {
      // req.r_or_w == 0 is read from, otherwise write into memory
      if (req.r_or_w == 0) {
        // detect invalid memory access under user mode
        if (req.mode == 0 && req.address > 999) {
          printf("Memory violation: accessing system address %d in user mode\n", req.address);
          _exit(-1);
        }
        req.data = memory[req.address];
        // return fetched data to the cpu
        write(c_to_p_fd[1], & req, sizeof(req));
      } else {
        // write data into memory 
        memory[req.address] = req.data;
      }
    }
    
    close(p_to_c_fd[0]);
    close(c_to_p_fd[1]);
    // printf("Exiting the child\n");
    _exit(EXIT_SUCCESS);
  } else {
    // PARENT PROCESS(cpu)

    int pc = 0;
    int sp = -1;
    int ir = 0;
    int ac = 0;
    int x = 0;
    int y = 0;

    close(p_to_c_fd[0]);
    close(c_to_p_fd[1]);
    
    // keep communicating with memory as long as instruction terminates the program
    while (1) {
      // fetch instruction from memory
      req.r_or_w = 0;
      req.address = pc;
      req.data = 0;
      pc++;
      write(p_to_c_fd[1], & req, sizeof(req));
      read(c_to_p_fd[0], & req, sizeof(req));
      ir = req.data;

      // if instrcution is 50, terminate the fetch loop
      if (ir == 50) {
        break;
      }

      switch (ir) {
      case 1: //Load the value into the AC
        req.r_or_w = 0;
        req.address = pc;
        pc++;
        write(p_to_c_fd[1], & req, sizeof(req));
        read(c_to_p_fd[0], & req, sizeof(req));

        ac = req.data;

        break;
      case 2: // Load the value at the address into the AC
        req.r_or_w = 0;
        req.address = pc;
        pc++;
        write(p_to_c_fd[1], & req, sizeof(req));
        read(c_to_p_fd[0], & req, sizeof(req));

        req.address = req.data;
        write(p_to_c_fd[1], & req, sizeof(req));
        read(c_to_p_fd[0], & req, sizeof(req));

        ac = req.data;

        break;
      case 3: // Load the value from the address found in the given address into the AC
        req.r_or_w = 0;
        req.address = pc;
        pc++;
        write(p_to_c_fd[1], & req, sizeof(req));
        read(c_to_p_fd[0], & req, sizeof(req));

        req.address = req.data;

        write(p_to_c_fd[1], & req, sizeof(req));
        read(c_to_p_fd[0], & req, sizeof(req));

        req.address = req.data;

        write(p_to_c_fd[1], & req, sizeof(req));
        read(c_to_p_fd[0], & req, sizeof(req));

        ac = req.data;

        break;
      case 4: // Load the value at (address+X) into the AC
        req.r_or_w = 0;
        req.address = pc;
        pc++;
        write(p_to_c_fd[1], & req, sizeof(req));
        read(c_to_p_fd[0], & req, sizeof(req));

        req.address = req.data + x;

        write(p_to_c_fd[1], & req, sizeof(req));
        read(c_to_p_fd[0], & req, sizeof(req));

        ac = req.data;

        break;
      case 5: // Load the value at (address+Y) into the AC
        req.r_or_w = 0;
        req.address = pc;
        pc++;
        write(p_to_c_fd[1], & req, sizeof(req));
        read(c_to_p_fd[0], & req, sizeof(req));

        req.address = req.data + y;

        write(p_to_c_fd[1], & req, sizeof(req));
        read(c_to_p_fd[0], & req, sizeof(req));

        ac = req.data;

        break;
      case 6: // Load from (Sp+X) into the AC 
        req.r_or_w = 0;
        req.address = sp + x; // need to check if the sp points to a valid address?
        write(p_to_c_fd[1], & req, sizeof(req));
        read(c_to_p_fd[0], & req, sizeof(req));

        ac = req.data;

        break;
      case 7: // Store the value in the AC into the address
        req.r_or_w = 0;
        req.address = pc;
        pc++;
        write(p_to_c_fd[1], & req, sizeof(req));
        read(c_to_p_fd[0], & req, sizeof(req));

        req.address = req.data;
        req.r_or_w = 1;
        req.data = ac;
        write(p_to_c_fd[1], & req, sizeof(req));

        break;
      case 8: // Gets a random int from 1 to 100 into the AC
        ac = (rand() % 100) + 1;
        break;
      case 9: // If port=1, writes AC as an int to the screen, If port=2, writes AC as a char to the screen
        req.r_or_w = 0;
        req.address = pc;
        pc++;
        write(p_to_c_fd[1], & req, sizeof(req));
        read(c_to_p_fd[0], & req, sizeof(req));

        if (req.data == 1) {
          printf("%d", ac);
        } else if (req.data == 2) {
          char c = (char) ac;
          printf("%c", c);
        }
        break;
      case 10: // Add the value in X to the AC
        ac = ac + x;
        break;
      case 11: // Add the value in Y to the AC
        ac = ac + y;
        break;
      case 12: // Subtract the value in X from the AC
        // printf("insruction: 12\n");
        ac = ac - x;
        break;
      case 13: // Subtract the value in Y from the AC
        ac = ac - y;
        break;
      case 14: // Copy the value in the AC to X
        x = ac;
        break;
      case 15: // Copy the value in X to the AC
        ac = x;
        break;
      case 16: // Copy the value in the AC to Y
        y = ac;
        break;
      case 17: // Copy the value in Y to the AC
        ac = y;
        break;
      case 18: // Copy the value in AC to the SP
        sp = ac;
        break;
      case 19: // Copy the value in SP to the AC 
        ac = sp;
        break;
      case 20: // Jump to the address
        req.r_or_w = 0;
        req.address = pc;
        pc++;
        write(p_to_c_fd[1], & req, sizeof(req));
        read(c_to_p_fd[0], & req, sizeof(req));

        pc = req.data; // need to check if the address is in the user area??
        break;
      case 21: // Jump to the address only if the value in the AC is zero
        req.r_or_w = 0;
        req.address = pc;
        pc++;
        write(p_to_c_fd[1], & req, sizeof(req));
        read(c_to_p_fd[0], & req, sizeof(req));

        if (ac == 0) {
          pc = req.data; // need to check if the address is in the user area??
        }
        break;
      case 22: // Jump to the address only if the value in the AC is not zero
        req.r_or_w = 0;
        req.address = pc;
        pc++;
        write(p_to_c_fd[1], & req, sizeof(req));
        read(c_to_p_fd[0], & req, sizeof(req));
        if (ac != 0) {
          pc = req.data; // need to check if the address is in the user area??
        }
        break;
      case 23: // Push return address onto stack, jump to the address
        req.r_or_w = 1;
        if (sp <= -1) {
          sp = 999;
        } else {
          sp--;
        }

        req.address = sp;
        req.data = pc + 1;
        write(p_to_c_fd[1], & req, sizeof(req));

        req.r_or_w = 0;
        req.address = pc;
        write(p_to_c_fd[1], & req, sizeof(req));
        read(c_to_p_fd[0], & req, sizeof(req));

        pc = req.data;
        break;
      case 24: // Pop return address from the stack, jump to the address
        req.r_or_w = 0;
        req.address = sp;
        sp++;
        write(p_to_c_fd[1], & req, sizeof(req));
        read(c_to_p_fd[0], & req, sizeof(req));
        pc = req.data;
        break;
      case 25: // Increment the value in X
        x++;
        break;
      case 26: // Decrement the value in X
        x--;
        break;
      case 27: // Push AC onto stack
        req.r_or_w = 1;
        if (sp == -1) {
          sp = 999;
        } else {
          sp--;
        }
        req.address = sp;
        req.data = ac;
        write(p_to_c_fd[1], & req, sizeof(req));
        break;
      case 28: // Pop from stack into AC
        req.r_or_w = 0;
        req.address = sp;
        sp++;
        write(p_to_c_fd[1], & req, sizeof(req));
        read(c_to_p_fd[0], & req, sizeof(req));
        ac = req.data;
        break;
      case 29: // Perform system call
        if (mode != 1) {
          mode = 1;
          req.mode = mode;
          int temp_sp = sp;
          sp = 1999;

          req.r_or_w = 1;
          req.address = sp;
          req.data = temp_sp;

          write(p_to_c_fd[1], & req, sizeof(req));
          sp--;

          req.address = sp;
          req.data = pc;
          write(p_to_c_fd[1], & req, sizeof(req));
          pc = 1500;
        }
        break;
      case 30: // return from system call
        sp = 1998;
        req.r_or_w = 0;
        req.address = sp;

        write(p_to_c_fd[1], & req, sizeof(req));
        read(c_to_p_fd[0], & req, sizeof(req));
        pc = req.data;

        sp++;
        req.address = sp;
        write(p_to_c_fd[1], & req, sizeof(req));
        read(c_to_p_fd[0], & req, sizeof(req));
        sp = req.data;
        mode = 0;
        req.mode = mode;
        break;
      default:
        printf("ir: %d\n", ir);
        printf("Unknown instruction\n");
      }
      // increment timer counter
      instCnt++;
      

      //timer interrupt
      if (instCnt % timer == 0 && mode != 1) {
        
        mode = 1;
        req.mode = mode;
        int temp_sp = sp;
        sp = 1999;

        req.r_or_w = 1;
        req.address = sp;
        req.data = temp_sp;
        write(p_to_c_fd[1], & req, sizeof(req));
        sp--;

        req.address = sp;
        req.data = pc;
        write(p_to_c_fd[1], & req, sizeof(req));
        pc = 1000;
      }
    }

    // ------------------------------------------
    close(p_to_c_fd[1]);
    close(c_to_p_fd[0]);
    waitpid(pid, NULL, 0);
    // printf("Exiting the parent\n");
  }

  return 0;
}