# jabfi #
jabfi is just another [Brainfuck](http://www.hevanet.com/cristofd/brainfuck/brainfuck.html) interpreter.
It translates the input to an intermediate instruction set and applies very basic optimizations (instruction contraction and single-instruction loop translation). It DOES NOT perform any memory bounds checking.

## Usage ##
Compile with `make` and run as `./jabfi <program_file.b>`. Run tests with `make tests`.
