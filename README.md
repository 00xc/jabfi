# jabfi #
jabfi is just another [Brainfuck](http://www.hevanet.com/cristofd/brainfuck/brainfuck.html) interpreter.
It translates the input to an intermediate instruction set and applies basic optimizations (instruction contraction, single-instruction loop translation, move-on-operation). It DOES NOT perform any memory bounds checking.

## Usage ##
Compile with `make` and run as `./jabfi <program_file.b>`. Run tests with `make tests`.
