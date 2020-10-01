#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEM_SIZE		(2<<15)

/* An instruction as an operator and an operand */
typedef enum { OP_ADD, OP_CLEAR, OP_MOVE, OP_RIGHT_U0, OP_LEFT_U0, OP_IN, OP_OUT, OP_LOOP_BEGIN, OP_LOOP_END, OP_END, OP_MUL, OP_SET } operator_t;
typedef int32_t operand_t;
typedef struct { operator_t op; operand_t val; operand_t mov; } instruction_t;

/* A program has code and an instruction pointer */
typedef uint_fast32_t program_pos_t;
typedef struct { instruction_t* code; program_pos_t pos; } program_t;

#define PROGRAM_NEXT(program)	program->code[program->pos++]

/* A tape has memory cells and a pointer */
typedef unsigned char cell_t;
typedef uint16_t tape_pos_t;
typedef struct { cell_t memory[MEM_SIZE]; tape_pos_t pos; } tape_t;

/* We need to declare these function prototypes since they can call each other */
void bf_run_instruction(const instruction_t*, program_t*, tape_t*);
void bf_run_loop(program_t*, tape_t*);

#ifdef DEBUG
char* op_to_str(operator_t operator) {
	char* ops[13];
	ops[OP_ADD]="OP_ADD"; ops[OP_CLEAR]="OP_CLEAR"; ops[OP_MOVE]="OP_MOVE";
	ops[OP_RIGHT_U0]="OP_RIGHT_U0"; ops[OP_LEFT_U0]="OP_LEFT_U0"; ops[OP_IN]="OP_IN";
	ops[OP_OUT]="OP_OUT"; ops[OP_LOOP_END]="OP_LOOP_END"; ops[OP_LOOP_BEGIN]="OP_LOOP_BEGIN"; ops[OP_END]="OP_END";
	ops[OP_MUL]="OP_MUL"; ops[OP_SET]="OP_SET";

	return ops[operator];
}
#endif

/*
 * Translates a series of identical instructions into one:
 * `++++` -> (OP_ADD, 4)
 *  `<<`  -> (OP_MOVE, -2)
 */
inline void bf_accumulate_instructions(const char* input, unsigned int* i, instruction_t* output, unsigned int* j, operator_t op, int sign) {
	char current = input[*i];
	unsigned int start_pos = *i;

	while (input[++(*i)] == current) {
		continue;
	}

	output[(*j)++] = (instruction_t) { .op = op, .val = (--(*i) - start_pos + 1) * sign };
}

/*
* Counts the number of occurences of `c` in `haystack` until it reaches the `sentinel` character
* If the sentinel character is not found before the end of the string, it returns -1;
*/
int count_str(char c, const char* haystack, char sentinel) {

	if (strchr(haystack, sentinel) == NULL)
		return -1;

	int i;
	for (i=0; haystack[i] != sentinel; haystack[i] == c ? i++ : *haystack++);
	return i;
}

/*
 * Returns 0 on loops that have more than one unique instruction or when that unique instruction is
 * not arithmetic (,.[]). Returns that single instruction character otherwise. Expects the input
 * pointer to be past the initial `[`.
 * [>>>>] -> '>'
 * [+<<-] -> 0
 * [,,,]  -> 0
 */
char is_monoinstruction_loop(const char* input, unsigned int i) {
	const char* ops = "+-<>,.[]";
	char arithmetic_ops[256];
	char found = 0;

	memset(arithmetic_ops, 0, 256);
	arithmetic_ops['+'] = arithmetic_ops['-'] = arithmetic_ops['<'] = arithmetic_ops['>'] = 1;

	for (unsigned int j=0; j<strlen(ops); ++j) {
		if (count_str(ops[j], input+i+1, ']') != 0) {

			if (found != 0 || arithmetic_ops[(int)ops[j]] == 0) {
				return 0;
			} else {
				found = ops[j];
			}
		}
	}

	return found;
}

/* Translate each `OP_MOVE, OP_X` sequence into a single `OP_X` with a move */
void bf_optimize_move_on_op(instruction_t* input) {
	unsigned i = 0, j = 0;
	instruction_t next;

	do {
		next = input[i + 1];
		if ( input[i].op == OP_MOVE && next.op != OP_END && next.op != OP_MOVE ) {
			input[j].mov = input[i++].val;
			input[j].op = next.op;
			input[j].val = next.val;
			++j;
		} else {
			input[j++] = input[i];
		}

	} while (input[i++].op != OP_END);
}

/*
 * Performs two types of conversions:
 * 1. After an OP_CLEAR, and OP_ADD implies that the cell will be set to the operand.
 *   {OP_CLEAR .mov=X}, {OP_ADD .val=Y .mov=0} => {OP_SET .val=Y, .mov=X}
 * 2. After an OP_LOOP_END, the current cell will be set to zero always, so the same applies:
 *   {OP_LOOP_END}, {OP_ADD .val=X .mov=0} => {OP_LOOP_END}, {OP_SET .val=X .mov=0}
 */
void bf_optimize_set_cell(instruction_t* input) {
	unsigned i = 0, j = 0;
	instruction_t next;

	if (input[0].op == OP_ADD) {
		input[0].op = OP_SET;
	}

	do {
		next = input[i + 1];

		if (next.op == OP_ADD && next.mov == 0) {
			switch (input[i].op) {

				case OP_CLEAR:
					input[j].op = OP_SET;
					input[j].val = next.val;
					input[j++].mov = input[i++].mov;
					break;

				case OP_LOOP_END:
					input[j++] = input[i++];
					input[j] = input[i];
					input[j++].op = OP_SET;
					break;

				default:
					input[j++] = input[i];
					break;
			}

		} else {
			input[j++] = input[i];
		}

	} while (input[i++].op != OP_END);
}

/*
 * Returns 1 if the loop starting at `offset` is a multiplication loop. A multiplication loop sets
 * a series of cells to a value multiplied by the current cell.
 *
 * Given that the memory pointer before the loop is `pos`,
 * [->+>++>>+++<<<<] can be optimized to run in constant time:
 * mem[pos+1] += 1 * mem[pos]   =>   {op=OP_MUL, val=1, mov=1}
 * mem[pos+2] += 2 * mem[pos]   =>   {op=OP_MUL, val=2, mov=2}
 * mem[pos+4] += 3 * mem[pos]   =>   {op=OP_MUL, val=3, mov=4}
 * mem[pos] = 0                 =>   {op=OP_CLEAR, mov=0}
 *
 * This is possible because the loop:
 * 1. is balanced (has the same number of left and right moves)
 * 2. decreases, in total, mem[pos] by one on each iteration.
 * Therefore it will be executed mem[pos] times.
 */
int is_mult_loop(instruction_t* output, unsigned int offset) {
	operand_t balance = 0;
	operand_t change_on_base = 0;
	unsigned int i;

	for (i=offset+1; output[i].op != OP_LOOP_END; ++i) {

		if (output[i].op != OP_ADD) {
			return 0;
		}

		/*
		 * We use `balance` to account for all the left and right memory pointer moves.
		 * It should be zero by the end of the loop.
		 * We use `change_on_base` to check the changes to the base cell after one
		 * iteration. It should be -1 by the end of the loop.
		 */
		if ( (balance += output[i].mov) == 0 )
			change_on_base += output[i].val;
	}

	/* We need to account for the move on OP_LOOP_END */
	balance += output[i].mov;

	/* Check if the loop can be optimized based on the previous rules. */
	if (balance != 0 || change_on_base != -1)
		return 0;

	return 1;
}

/*
 * Transform multiplication loops into a series of OP_MUL plus one OP_CLEAR.
 * NOTE: the .mov member of the instruction_t struct does not behave in the same way for OP_MUL.
 * For regular instructions, this field indicates a pointer move + an action. For OP_MUL it just
 * indicates an offset on which to take the action (the pointer itself will not be modified).
 * This is done because multiplication loops are guaranteed to end on the same cell as they
 * started, so it is not necessary to move the pointer to the offsets and then back. It is also
 * easier to keep the pointer fixed to the base cell, since it is one of the operands of the
 * multiplication.
 */
void bf_optimize_mult_loops(instruction_t* input) {
	unsigned int i = 0, j = 0;
	operand_t offset;

	do {
		if ( input[i].op == OP_LOOP_BEGIN && is_mult_loop(input, i) ) {

			/* Add an OP_MOVE if OP_LOOP_BEGIN was contracted */
			if (input[i].mov != 0) {
				input[j++] = (instruction_t) { .op = OP_MOVE, .val = input[i].mov };
			}

			/* Replace loop body with OP_MUL instructions */
			for (offset=0, i+=1; input[i].op != OP_LOOP_END; ++i) {
				offset += input[i].mov;

				/*
				 * `is_mult_loop()` guarantees that the base cell will have a value
				 * of zero by the end of the loop.
				 */
				if (offset == 0) continue;

				input[j].op = OP_MUL;
				input[j].val = input[i].val;
				input[j].mov = offset;
				++j;
			}

			/* Clear the base cell */
			input[j++] = (instruction_t) { .op = OP_CLEAR, .val=1 };

		} else {
			input[j++] = input[i];
		}

	} while (input[i++].op != OP_END);
}

/*
 * This function returns 0 when all OP_LOOP_BEGIN and OP_LOOP_END instructions are matched. If a
 * positive number is returned, an extra OP_LOOP_END was found. If a negative number is returned,
 * an extra OP_LOOP_BEGIN was found.
 * NOTE: it should be called with offset=0.
 */
int bf_match_loops(const instruction_t* input, int offset) {

	for (int i = offset; input[i].op != OP_END; ++i) {
		if (input[i].op == OP_LOOP_BEGIN) {
			if ( (i = bf_match_loops(input, i+1)) < 0)
				return i;
		} else if (input[i].op == OP_LOOP_END) {
			return i;
		}
	}

	return -offset;
}

/* Compile an array of characters into an array of instruction_t */
void bf_compile(char* input, instruction_t* output) {
	unsigned int i = -1, j = 0;
	unsigned int c; /* this should be a char but uint is faster */

	/* Main IR compilation loop */
	while (input[++i]) {
		switch (input[i]) {

			case '-': bf_accumulate_instructions(input, &i, output, &j, OP_ADD, -1); break;
			case '+': bf_accumulate_instructions(input, &i, output, &j, OP_ADD, 1); break;
			case '<': bf_accumulate_instructions(input, &i, output, &j, OP_MOVE, -1); break;
			case '>': bf_accumulate_instructions(input, &i, output, &j, OP_MOVE, 1); break;
			case '.': output[j++] = (instruction_t) { .op = OP_OUT }; break;
			case ',': output[j++] = (instruction_t) { .op = OP_IN }; break;
			case ']': output[j++] = (instruction_t) { .op = OP_LOOP_END }; break;
			case '[':
				if ( input[i+2] == ']' && (input[i+1] == '-' || input[i+1] == '+')) {

					/* Optimized loop: clear cell ([-] or [+]) */
					output[j++] = (instruction_t) { .op = OP_CLEAR, .val = 1 }; i+=2; break;

				} else if ( (c = is_monoinstruction_loop(input, i)) != 0 && (c=='<' || c=='>') ) {

					/* Optimized loop: move pointer until zero */
					unsigned int num = count_str(c, input+i, ']');
					switch (c) {
						case '<': output[j++] = (instruction_t) { .op = OP_LEFT_U0,  .val = num }; i += (num+1); break;
						case '>': output[j++] = (instruction_t) { .op = OP_RIGHT_U0, .val = num }; i += (num+1); break;
					}

				} else {

					/* Regular loop */
					output[j++] = (instruction_t) { .op = OP_LOOP_BEGIN };
				}

				break;
		}
	}
	output[j] = (instruction_t) { .op = OP_END };

	/* Additional optimizations. These can be disabled and the interpreter will still work */
	bf_optimize_move_on_op(output);
	bf_optimize_mult_loops(output);
	bf_optimize_set_cell(output);

	/* Check for unmatched loops */
	int k = bf_match_loops(output, 0);
	if (k > 0) {
		fprintf(stderr, "Error: found ] with unmatched [.\n");
		free(input);
		free(output);
		exit(EXIT_FAILURE);
	} else if (k < 0) {
		fprintf(stderr, "Error: found [ with unmatched ].\n");
		free(input);
		free(output);
		exit(EXIT_FAILURE);
	}

	#ifdef DEBUG
	for (i=0; output[i].op != OP_END; ++i) {
		printf("%i\t%s : %i mov=%i\n", i, op_to_str(output[i].op), output[i].val, output[i].mov);
	}
	#endif
}

/* Top level instruction loop */
void bf_run_main_loop(program_t* program, tape_t* tape) {
	instruction_t instruction;

	while ( (instruction = PROGRAM_NEXT(program)).op != OP_END ) {
		bf_run_instruction(&instruction, program, tape);
	}
}

/* Called upon finding an OP_LOOP_BEGIN instruction */
inline void bf_run_loop(program_t* program, tape_t* tape) {
	register instruction_t instruction;
	program_pos_t loop_beginning = program->pos;

	if (tape->memory[tape->pos] != 0) {

		/* Loop until counter is zero */
		do {
			/* Reset the program pointer and run the loop once */
			program->pos = loop_beginning;
			while ( (instruction = PROGRAM_NEXT(program)).op != OP_LOOP_END) {
				bf_run_instruction(&( (instruction_t) {instruction.op, instruction.val, instruction.mov} ), program, tape);
			}

		} while (tape->memory[tape->pos += instruction.mov] != 0);

	} else {

		/* Skip to matching OP_LOOP_END */
		register int_fast16_t loop = 1;
		do {
			switch ( (instruction = PROGRAM_NEXT(program)).op ) {
				case OP_LOOP_END: --loop; break;
				case OP_LOOP_BEGIN: ++loop; break;
				default: continue;
			}
		} while (loop != 0);

	}
}

inline void bf_run_instruction(const instruction_t* instruction, program_t* program, tape_t* tape) {

	switch(instruction->op) {
		case OP_ADD: tape->memory[tape->pos += instruction->mov] += instruction->val; break;
		case OP_MOVE: tape->pos += instruction->val; break;
		case OP_CLEAR: tape->memory[tape->pos += instruction->mov] = 0; break;
		case OP_RIGHT_U0: tape->pos += instruction->mov; while (tape->memory[tape->pos]) (tape->pos) += instruction->val; break;
		case OP_LEFT_U0: tape->pos += instruction->mov; while (tape->memory[tape->pos]) (tape->pos) -= instruction->val; break;
		case OP_IN: tape->memory[tape->pos += instruction->mov] = (cell_t) getchar(); break;
		case OP_OUT: putchar(tape->memory[tape->pos += instruction->mov]); fflush(stdout); break;
		case OP_MUL: tape->memory[tape->pos + instruction->mov] += instruction->val * tape->memory[tape->pos]; break;
		case OP_LOOP_BEGIN: tape->pos += instruction->mov; bf_run_loop(program, tape); break;
		case OP_SET: tape->memory[tape->pos += instruction->mov] = instruction->val;

		// This instruction is executed in `bf_run_loop` to avoid an additional call to `bf_run_instruction`
		//case OP_LOOP_END: tape->pos += instruction->mov; return EXEC_LOOP_DONE;

		default: return;
	}
}

/* Filters non-useful characters */
void bf_filter(char* input) {
	char* src;
	char* dst;

	char ops[256];
	memset(ops, 0, 256*sizeof(char));
	ops['.'] = ops[','] = ops['+'] = ops['-'] = ops['<'] = ops['>'] = ops['['] = ops[']'] = 1;

	for (src=input, dst=input; *src; src++) {
		if (ops[(int)*src]) {
			*dst++ = *src;
		}
	}
	*dst = 0;
}

int main(int argc, char const *argv[]) {
	FILE* fp;
	char* program;
	size_t file_size;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s program_file.b\n", argv[0]);
		exit(EXIT_SUCCESS);
	}

	/* Open file */
	if ( (fp = fopen(argv[1], "r")) == NULL) {
		fprintf(stderr, "Error: Could not find input file %s.\n", argv[1]);
		exit(EXIT_FAILURE);
	}

	/* Get file size */
	fseek(fp, 0, SEEK_END);
	file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	/* Allocate initial buffer and read program */
	if ( (program = calloc(file_size + 1, sizeof(char))) == NULL ) {
		perror("calloc");
		fclose(fp);
		exit(EXIT_FAILURE);
	}
	if (fread(program, sizeof(char), file_size, fp) != (file_size * sizeof(char))) {
		fprintf(stderr, "Error: could not read program from %s.\n", argv[1]);
		free(program);
		fclose(fp);
		exit(EXIT_FAILURE);
	}
	fclose(fp);

	/* Allocate buffer for compiled program */
	program_t compiled_program = {.code = malloc(sizeof(instruction_t) * file_size), .pos = 0};
	if (compiled_program.code == NULL) {
		perror("malloc");
		free(program);
		exit(EXIT_FAILURE);
	}

	/* Filter input characters and compile program */
	bf_filter(program);
	bf_compile(program, compiled_program.code);
	free(program);

	/* Init memory */
	tape_t tape = {.pos = 0};
	memset(tape.memory, 0, MEM_SIZE * sizeof(cell_t));

	/* Run program */
	bf_run_main_loop(&compiled_program, &tape);
	free(compiled_program.code);

	return EXIT_SUCCESS;
}
