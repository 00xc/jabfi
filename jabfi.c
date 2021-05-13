#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#define MEM_SIZE		(2<<15)
#define PROGRAM_NEXT(program)	program->code[program->pos++]

/* An instruction has an operator, a move and an operand */
typedef enum {
	OP_ADD,
	OP_CLEAR,
	OP_MOVE,
	OP_RIGHT_U0,
	OP_LEFT_U0,
	OP_IN,
	OP_OUT,
	OP_LOOP_BEGIN,
	OP_LOOP_END,
	OP_END,
	OP_MUL,
	OP_SET
} operator_t;
typedef int32_t operand_t;
typedef struct {
	operator_t op;
	operand_t val;
	operand_t mov;
} instruction_t;

/* A program has code and an instruction pointer */
typedef uint_fast32_t program_pos_t;
typedef struct {
	instruction_t* code;
	program_pos_t pos;
} program_t;

/* A tape has memory cells and a pointer */
typedef unsigned char cell_t;
typedef uint16_t tape_pos_t;
typedef struct {
	cell_t memory[MEM_SIZE];
	tape_pos_t pos;
} tape_t;

// Global variable to store one of OP_MUL's operands
operand_t mul_operand = 0;

/* We need to declare these function prototypes since they can call each other */
void bf_run_instruction(const instruction_t*, program_t*, tape_t*);
void bf_run_loop(program_t*, tape_t*);

#ifdef DEBUG
char* _op_to_str(operator_t operator) {
	char* ops[12];
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
 * An operator_t and a operand sign must be supplied.
 */
inline instruction_t bf_accumulate_instructions(const char* input, unsigned int* i, operator_t op, short sign) {
	unsigned int start_pos = *i;
	char current = input[start_pos];

	while (input[++(*i)] == current) {
		continue;
	}

	return (instruction_t) { .op = op, .val = (--(*i) - start_pos + 1) * sign };
}

/*
* Counts the number of occurences of `c` in `haystack` until it reaches a loop end (`]`).
* If the loop end character is not found before the end of the string, it returns -1;
*/
#ifdef __GNUC__
__attribute__((const))
#endif
int count_str(char c, const char* haystack) {
	int i;

	if (strchr(haystack, ']') == NULL)
		return -1;

	for (i=0; haystack[i] != ']'; haystack[i] == c ? i++ : *haystack++);
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
#ifdef __GNUC__
__attribute__((const))
#endif
char is_monoinstruction_loop(const char* input) {
	unsigned int i;
	const char* ops = "+-<>,.[]";
	char arithmetic_ops[256] = {0};
	char found = 0;

	arithmetic_ops['+'] = arithmetic_ops['-'] = arithmetic_ops['<'] = arithmetic_ops['>'] = 1;

	for (i = 0; i < strlen(ops); ++i) {
		if (count_str(ops[i], input + 1) != 0) {

			if (found == 0 && arithmetic_ops[(int)ops[i]] != 0) {
				found = ops[i];
			} else {
				return 0;
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
			input[j].mov = input[i].val;
			input[j].op = next.op;
			input[j].val = next.val;
			++i;

		} else {
			input[j] = input[i];
		}

		++j;

	} while (input[i++].op != OP_END);
}

/*
 * After an OP_CLEAR, an OP_ADD implies that the cell will be set to OP_ADD's operand.
 * After an OP_SET, an OP_ADD implies that the cell will be set to the sum of both operands.
 * {OP_CLEAR .mov=X}, {OP_ADD .val=Y .mov=0}       => {OP_SET .val=Y, .mov=X}
 * {OP_SET, .val=Z .mov=X}, {OP_ADD .val=Y .mov=0} => {OP_SET .val=Y+Z, .mov=X}
 */
void bf_optimize_set_cell(instruction_t* input) {
	register unsigned int i = 0, j = 0;
	instruction_t next;

	if (input[0].op == OP_ADD) {
		input[0].op = OP_SET;
		i = j = 1;
	}

	do {
		next = input[i + 1];

		/* OP_CLEAR + OP_ADD  */
		if (input[i].op == OP_CLEAR && next.op == OP_ADD && next.mov == 0) {
			input[j].op = OP_SET;
			input[j].val = next.val;
			input[j].mov = input[i].mov;
			++i;

		/* OP_SET + OP_ADD  */
		} else if (input[i].op == OP_SET && next.op == OP_ADD && next.mov == 0) {
			input[j].op = OP_SET;
			input[j].val = input[i].val + next.val;
			input[j].mov = input[i].mov;
			++i;

		} else {
			input[j] = input[i];
		}

		++j;

	} while (input[i++].op != OP_END);
}

/*
 * Returns 1 if the loop starting at current offset is a multiplication loop. A multiplication loop sets
 * a series of cells to a value multiplied by the current cell.
 *
 * Given that the memory pointer before starting the loop is `pos`,
 * [->+>++>>+++<<<<] can be optimized to run in constant time:
 *   >+                 => mem[pos+1] += 1 * mem[pos]  =>  {op=OP_MUL, val=1, mov=1}
 *     >++              => mem[pos+2] += 2 * mem[pos]  =>  {op=OP_MUL, val=2, mov=2}
 *        >>+++         => mem[pos+4] += 3 * mem[pos]  =>  {op=OP_MUL, val=3, mov=4}
 *             <<<<] [- => mem[pos] = 0                =>  {op=OP_CLEAR, mov=0}
 *
 * This is possible because the loop:
 * 1. is balanced (has the same number of left and right moves)
 * 2. decreases, in total, mem[pos] by one on each iteration.
 * Therefore the loop would be executed mem[pos] times.
 */
int is_mult_loop(const instruction_t* input) {
	operand_t balance = 0;
	operand_t change_on_base = 0;
	unsigned int i;

	for (i = 1; input[i].op != OP_LOOP_END; ++i) {

		if (input[i].op != OP_ADD)
			return 0;

		/*
		 * We use `balance` to account for all the left and right memory pointer moves.
		 * It should be zero by the end of the loop.
		 * We use `change_on_base` to check the changes to the base cell after one
		 * iteration. It should be -1 by the end of the loop.
		 */
		if ( (balance += input[i].mov) == 0 )
			change_on_base += input[i].val;
	}

	/* We need to account for the move on OP_LOOP_END */
	balance += input[i].mov;

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
 * started, so it is not necessary to move the pointer to the offsets and then back.
 */
void bf_optimize_mult_loops(instruction_t* input) {
	unsigned int i = 0, j = 0;
	operand_t offset;

	do {

		if (input[i].op == OP_LOOP_BEGIN && is_mult_loop(input + i)) {

			/*
			 * The base cell is cleared before the multiplication sequence. The .val=1
			 * is an indication to save the original value of the cell to the `mul_operand`
			 * global variable.
			 */
			input[j].op = OP_CLEAR;
			input[j].val = 1;
			input[j].mov = input[i].mov;
			++j;

			for (offset = 0, i += 1; input[i].op != OP_LOOP_END; ++i) {

				offset += input[i].mov;

				if (offset == 0) continue;

				input[j].op = OP_MUL;
				input[j].val = input[i].val;
				input[j].mov = offset;
				++j;
			}

		} else {
			input[j] = input[i];
			++j;
		}

	} while (input[i++].op != OP_END);
}

/*
 * This function recursively analizes the program to check whether all loops are correctly matched
 * or not. Returns 0 when all OP_LOOP_BEGIN and OP_LOOP_END instructions are matched. If a
 * positive number is returned, an extra OP_LOOP_END was found. If a negative number is returned,
 * an extra OP_LOOP_BEGIN was found.
 * NOTE: it should be called with offset=0.
 */
#ifdef __GNUC__
__attribute__((const))
#endif
int bf_match_loops(const char* input, int offset) {
	int i;

	for (i = offset; input[i] != '\0'; ++i) {
		if (input[i] == '[') {
			if ( (i = bf_match_loops(input, i+1)) < 0 )
				return i;
		} else if (input[i] == ']') {
			/* Return 1 if ] found on the first element */
			return i + (i == 0);
		}
	}

	return -offset;
}

/* Compile an array of characters into an array of instruction_t */
void bf_compile(char* input, instruction_t* output) {
	unsigned int i = -1, j = 0;
	unsigned int c; /* this should be a char but uint is faster */

	/* Check for unmatched loops */
	int k = bf_match_loops(input, 0);
	if (k != 0) {
		free(input);
		free(output);
		errx(EXIT_FAILURE, "Found '%c' with unmatched '%c'.",
			(k > 0) ? ']' : '[',
			(k > 0) ? '[' : ']'
		);
	}

	/* Main IR compilation loop */
	while (input[++i]) {
		switch (input[i]) {

			case '+': output[j++] = bf_accumulate_instructions(input, &i, OP_ADD,  1); break;
			case '-': output[j++] = bf_accumulate_instructions(input, &i, OP_ADD, -1); break;
			case '>': output[j++] = bf_accumulate_instructions(input, &i, OP_MOVE, 1); break;
			case '<': output[j++] = bf_accumulate_instructions(input, &i, OP_MOVE,-1); break;
			case '.': output[j++] = (instruction_t) { .op = OP_OUT }; break;
			case ',': output[j++] = (instruction_t) { .op = OP_IN }; break;
			case ']': output[j++] = (instruction_t) { .op = OP_LOOP_END }; break;
			case '[':
				if ( input[i+2] == ']' && (input[i+1] == '-' || input[i+1] == '+')) {

					/* Optimized loop: clear cell ([-] or [+]) */
					output[j++] = (instruction_t) { .op = OP_CLEAR, .val = 0 }; i+=2; break;

				} else if ( (c = is_monoinstruction_loop(input + i)) != 0 && (c == '<' || c == '>') ) {

					/* Optimized loop: move pointer until zero */
					unsigned int num = count_str(c, input+i);
					switch (c) {
						case '<': output[j++] = (instruction_t) { .op = OP_LEFT_U0,  .val = num }; i += (num + 1); break;
						case '>': output[j++] = (instruction_t) { .op = OP_RIGHT_U0, .val = num }; i += (num + 1); break;
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

	#ifdef DEBUG
	for (i=0; output[i].op != OP_END; ++i) {
		printf("%i\t%s : %i mov=%i\n", i, _op_to_str(output[i].op), output[i].val, output[i].mov);
	}
	#endif
}

/* Top level instruction loop */
void bf_run_main_loop(program_t* restrict program, tape_t* restrict tape) {
	instruction_t instruction;

	while ( (instruction = PROGRAM_NEXT(program)).op != OP_END ) {
		bf_run_instruction(&instruction, program, tape);
	}
}

/* Called upon finding an OP_LOOP_BEGIN instruction. Returns at the corresponding OP_LOOP_END */
void bf_run_loop(program_t* restrict program, tape_t* restrict tape) {

	if (tape->memory[tape->pos] != 0) {

		instruction_t* instruction;
		program_pos_t loop_beginning = program->pos;

		/* Loop until counter is zero */
		do {
			/* Reset the program pointer and run the loop once */
			program->pos = loop_beginning;
			while ( (instruction = &PROGRAM_NEXT(program))->op != OP_LOOP_END) {
				bf_run_instruction(instruction, program, tape);
			}

		} while (tape->memory[tape->pos += instruction->mov] != 0);

	} else {

		/* Skip to matching OP_LOOP_END */
		register int_fast16_t loop = 1;
		do {
			switch ( PROGRAM_NEXT(program).op ) {
				case OP_LOOP_END: --loop; break;
				case OP_LOOP_BEGIN: ++loop; break;
				default: continue;
			}
		} while (loop != 0);
	}
}

#ifdef __GNUC__
__attribute__((hot))
#endif
void bf_run_instruction(const instruction_t* instruction, program_t* program, tape_t* tape) {

	switch(instruction->op) {
		case OP_ADD: tape->memory[tape->pos += instruction->mov] += instruction->val; break;
		case OP_MOVE: tape->pos += instruction->val; break;
		case OP_CLEAR:
			tape->pos += instruction->mov;
			if (instruction->val != 0)
				mul_operand = tape->memory[tape->pos];
			tape->memory[tape->pos] = 0;
			break;
		case OP_RIGHT_U0: tape->pos += instruction->mov; while (tape->memory[tape->pos]) (tape->pos) += instruction->val; break;
		case OP_LEFT_U0:  tape->pos += instruction->mov; while (tape->memory[tape->pos]) (tape->pos) -= instruction->val; break;
		case OP_IN: tape->memory[tape->pos += instruction->mov] = (cell_t) getchar(); break;
		case OP_OUT: putchar(tape->memory[tape->pos += instruction->mov]); break;
		case OP_MUL: tape->memory[tape->pos + instruction->mov] += instruction->val * mul_operand; break;
		case OP_LOOP_BEGIN: tape->pos += instruction->mov; bf_run_loop(program, tape); break;
		case OP_SET: tape->memory[tape->pos += instruction->mov] = instruction->val; break;

		/* This instruction is executed in `bf_run_loop` to avoid an additional call to `bf_run_instruction` */
		//case OP_LOOP_END: tape->pos += instruction->mov; return EXEC_LOOP_DONE;

		default: return;
	}
}

/* Filters non-useful characters */
void bf_filter(char* input) {
	char ops[256] = { ['.'] = 1, [','] = 1, ['+'] = 1, ['-'] = 1, ['<'] = 1, ['>'] = 1, ['['] = 1, [']'] = 1 };
	char* src;
	char* dst;

	for (src=input, dst=input; *src; src++) {
		if (ops[(int)*src] == 1) {
			*dst++ = *src;
		}
	}
	*dst = 0;
}

int main(int argc, const char* argv[]) {
	FILE* fp;
	char* program;
	size_t file_size;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s program_file.b\n", argv[0]);
		exit(EXIT_SUCCESS);
	}

	/* Open file */
	fp = fopen(argv[1], "r");
	if (fp == NULL) {
		err(EXIT_FAILURE, "Could not open %s", argv[1]);
	}

	/* Get file size */
	fseek(fp, 0, SEEK_END);
	file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	/* Allocate initial buffer and read program */
	program = calloc(file_size + 2, sizeof(char));
	if (program == NULL) {
		fclose(fp);
		err(EXIT_FAILURE, "calloc");
	}
	if (fread(program, sizeof(char), file_size, fp) != file_size) {
		free(program);
		fclose(fp);
		err(EXIT_FAILURE, "Could not read program from %s", argv[1]);
	}
	fclose(fp);

	/* Filter the characters in the input buffer */
	bf_filter(program);

	/* Allocate buffer for compiled program */
	program_t compiled_program = {
		.code = malloc(file_size * sizeof(instruction_t)),
		.pos = 0
	};
	if (compiled_program.code == NULL) {
		free(program);
		err(EXIT_FAILURE, "malloc");
	}

	/* Translate program to a set of IR instructions */
	bf_compile(program, compiled_program.code);
	free(program);

	/* Init memory */
	tape_t tape = { .pos = 0 };
	memset(tape.memory, 0, MEM_SIZE * sizeof(cell_t));

	/* Run program */
	bf_run_main_loop(&compiled_program, &tape);
	free(compiled_program.code);

	return EXIT_SUCCESS;
}
