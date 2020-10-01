# jabfi #
jabfi is just another [Brainfuck](http://www.hevanet.com/cristofd/brainfuck/brainfuck.html) interpreter.
It translates the input to an intermediate instruction set and applies several optimizations explained below. It checks for matching loop beginnings (`[`) and ends (`]`), and wraps on out of bounds cell access.

## Usage ##
Compile with `make` and run as `./jabfi <program_file.b>`. Run tests with `make tests`.

## Optimizations ##

* Instruction contraction:
	- `++++` becomes `{OP_ADD, 4}`
	- `<<<` becomes `{OP_MOVE, -3}`
* Clear loops:
	- `[-]` or `[+]` become `{OP_CLEAR}`
* Scan loops:
	- `[<<<]` becomes `{OP_LEFT_U0, 3}` (move left in jumps of 3 until a zero is found).
* Move-on-operation:
	- `{OP_MOVE, 3}, {OP_ADD, 4}` becomes `{OP_ADD, value=4, move=3}`
* Multiplication loops:
	- Loops with that match the following two criteria can be optimized to single-pass multiplications:
		1. The loop has the same number of left and right moves (also known as balanced).
		2. It decreases the initial cell by one on each iteration.
		3. Is only comprised of `OP_ADD` operations (recall that these can include a move).
	- Example:
		`[->+>++>>+++<<<<]` ends up becoming
		`mem[pos+1] += 1 * mem[pos]`
		`mem[pos+2] += 2 * mem[pos]`
		`mem[pos+4] += 3 * mem[pos]`
		`mem[pos] = 0`
* Static cell assignments:
	- Clearing a cell and then adding to it can be replaced by a single assignment: `{OP_CLEAR, move=1}, {OP_ADD, value=-2, move=0}` becomes `{OP_SET, value=-2, move=1}`
	- At the end of a loop the current cell is set to zero, so the same principle applies: `{OP_LOOP_END}, {OP_ADD, value=7, move=0}` becomes `{OP_LOOP_END}, {OP_SET value=7, move=0}`
