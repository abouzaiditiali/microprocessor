# ARM Pipelined Simulator with L1 Cache

## Overview

This project implements a cycle-accurate simulator of a 5-stage pipelined ARM processor, developed entirely from scratch. The simulator includes:

- Instruction fetch, decode, execute, memory, and write-back stages  
- A branch prediction unit  
- Level 1 (L1) instruction and data caches  
- An interactive shell interface to control and inspect simulation

This simulator serves as an educational and debugging tool for exploring microarchitectural concepts like data hazards, control flow prediction, and memory hierarchy design.

---

## How to Use the Simulator

After compiling the project, run the simulator with:

```bash
./sim <input_file>

1. go
   Simulate the program until it indicates that the simulator should halt.

2. run <n>
   Simulate the execution of the machine for n instructions.

3. mdump <low> <high>
   Dump the contents of memory from location <low> to location <high>
   to the screen and the dump file (dumpsim).

4. rdump
   Dump the current instruction count, the contents of registers X0–X31,
   flags N and Z, and the program counter (PC) to the screen and dumpsim.

5. input <reg_num> <reg_val>
   Set general-purpose register <reg_num> to value <reg_val>.

6. ?
   Print out a list of all shell commands.

7. quit
   Quit the shell.

