#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MEM_SIZE		(2<<15)

/* An instruction as an operator and an operand */
typedef enum { OP_SUM, OP_DEL, OP_CLEAR, OP_RIGHT, OP_LEFT, OP_RIGHT_U0, OP_LEFT_U0, OP_IN, OP_OUT, OP_LOOP_BEGIN, OP_LOOP_END, OP_END } operator_t;
typedef uint_fast16_t operand_t;
typedef struct { operator_t operator; operand_t operand; } instruction_t;

/* A program has code and an instruction pointer */
typedef uint_fast32_t program_pos_t;
typedef struct { instruction_t* code; program_pos_t pos; } program_t;

#define PROGRAM_NEXT(program)	program->code[program->pos++]

/* A tape has memory cells and a pointer */
typedef unsigned char cell_t;
typedef uint_fast16_t tape_pos_t;
typedef struct { cell_t memory[MEM_SIZE]; tape_pos_t pos; } tape_t;

/* Initialize memory cells */
int tape_init(tape_t* tape) {
	tape->pos = 0;
	memset(tape->memory, 0, MEM_SIZE * sizeof(cell_t));
	return 0;
}

/* Values returned by `bf_run_instruction` */
enum exec_status {
	EXEC_OK,
	EXEC_LOOP_DONE,
	EXEC_END,
	EXEC_UNKNOWN
};

/* Declare several functions that need to know about each other */
int bf_run_instruction(instruction_t* instruction, program_t* program, tape_t* tape);
void bf_run_main_loop(program_t* program, tape_t* tape);
void bf_run_loop(program_t* program, tape_t* tape);

/*
 * Translates a series of identical instructions into one:
 * `++++` -> (OP_SUM, 4)
 *  `>>`  -> (OP_RIGHT, 2)
 */
inline void bf_accumulate_instructions(char* input, unsigned int* i, instruction_t* output, unsigned int* j, int op) {
	char current = input[*i];
	unsigned int aux = *i;

	while (input[++(*i)] == current) {
		;;
	}
	output[(*j)++] = (instruction_t) { .operator = op, .operand = --(*i) - aux + 1 };
}

int count_str(char c, const char* haystack, char sentinel) {

	if (strchr(haystack, sentinel) == NULL)
		return -1;

	int i;
	for (i=0; haystack[i] != sentinel; haystack[i]==c ? i++ : *haystack++);
	return i;
}

char is_monoinstruction_loop(const char* input, int i) {
	const char* ops = "+-<>,.[]";

	char arithmetic_ops[256];
	memset(arithmetic_ops, 0, 256);
	arithmetic_ops['+'] = arithmetic_ops['-'] = arithmetic_ops['<'] = arithmetic_ops['>'] = 1;

	char found = 0;
	int res;

	for (int j=0; j<strlen(ops); ++j) {
		res = count_str(ops[j], input+i+1, ']');
		if (res != 0) {
			if (found == 0) found = ops[j];
			else return 0;
		}
	}

	/* Si es OP aritm */
	if (arithmetic_ops[(int)found])
		return found;
	else
		return 0;
}

/* Compile an array of characters into an array of instruction_t */
void bf_compile(char* input, size_t size, instruction_t* output) {
	unsigned int i = -1, j = 0;
	char c;

	char clear_loop_ops[256];
	memset(clear_loop_ops, 0, 256);
	clear_loop_ops['-'] = clear_loop_ops['+'] = 1;

	while (++i < size) {
		switch (input[i]) {
			case '+': bf_accumulate_instructions(input, &i, output, &j, OP_SUM); break;
			case '-': bf_accumulate_instructions(input, &i, output, &j, OP_DEL); break;
			case '>': bf_accumulate_instructions(input, &i, output, &j, OP_RIGHT); break;
			case '<': bf_accumulate_instructions(input, &i, output, &j, OP_LEFT); break;
			case '.': output[j++] = (instruction_t) { .operator = OP_OUT, .operand = 0 }; break;
			case ',': output[j++] = (instruction_t) { .operator = OP_IN, .operand = 0 }; break;
			case '[':

				if ( (input[i+2] == ']') && clear_loop_ops[(int)input[i+1]]) {

					/* Optimized loop: clear cell */
					output[j++] = (instruction_t) { .operator = OP_CLEAR, .operand = 0 }; i+=2; break;

				} else if ( (c = is_monoinstruction_loop(input, i)) != 0 && (c=='<' || c=='>') ) {

					/* Optimized loop: move pointer until zero */
					{
						int num = count_str(c, input+i, ']');

						switch(c) {
							case '<': output[j++] = (instruction_t) { .operator = OP_LEFT_U0, .operand = num }; i+=(num+1); break;
							case '>': output[j++] = (instruction_t) { .operator = OP_RIGHT_U0, .operand = num }; i+=(num+1); break;
						}
					}


				} else {
					/* Regular loop */
					output[j++] = (instruction_t) { .operator = OP_LOOP_BEGIN, .operand = 0 };
				}

				break;
				
			case ']': output[j++] = (instruction_t) { .operator = OP_LOOP_END, .operand = 0 }; break;
		}
	}
	output[j] = (instruction_t) { .operator = OP_END, .operand = 0 };
}

/* Top level instruction loop */
void bf_run_main_loop(program_t* program, tape_t* tape) {
	instruction_t instruction;

	while ( (instruction = PROGRAM_NEXT(program)).operator != OP_END) {
		if (bf_run_instruction(&instruction, program, tape) == EXEC_LOOP_DONE) {
			fprintf(stderr, "Error: found ] @ %ld with unmatched [.\n", program->pos);
			free(program->code);
			exit(1);
		}
	}
}

/* Called upon finding an OP_LOOP_BEGIN instruction */
void bf_run_loop(program_t* program, tape_t* tape) {
	instruction_t instruction;
	program_pos_t loop_beginning = program->pos;

	if (tape->memory[tape->pos] == 0) {

		/* Skip to matching ], while we are not at EOF */
		int_fast16_t loop = 1;
		while ( loop != 0 && (instruction = PROGRAM_NEXT(program)).operator != OP_END) {
			if (instruction.operator == OP_LOOP_END) {
				--loop;
			} else if (instruction.operator == OP_LOOP_BEGIN) {
				++loop;
			}
		}

	} else {

		/* Loop until counter is zero, while we are not at EOF */
		do {
			/* Reset the pointer */
			program->pos = loop_beginning;

			/* Run the loop once */
			while ( (instruction = PROGRAM_NEXT(program)).operator != OP_LOOP_END && instruction.operator != OP_END) {
				bf_run_instruction(&instruction, program, tape);
			}

		} while (tape->memory[tape->pos] != 0 && instruction.operator != OP_END);
	}

	/* If we reached EOF during a loop there was an error */
	if (instruction.operator == OP_END) {
		fprintf(stderr, "Error: found [ @ %ld with unmatched ].\n", loop_beginning);
		free(program->code);
		exit(1);
	}
}

int bf_run_instruction(instruction_t* instruction, program_t* program, tape_t* tape) {

	switch(instruction->operator) {
		case OP_SUM: tape->memory[tape->pos] += instruction->operand; break;
		case OP_DEL: tape->memory[tape->pos] -= instruction->operand; break;
		case OP_CLEAR: tape->memory[tape->pos] = 0; break;
		case OP_RIGHT: tape->pos += instruction->operand; break;
		case OP_LEFT: tape->pos -= instruction->operand; break;
		case OP_RIGHT_U0: while (tape->memory[tape->pos]) (tape->pos) += instruction->operand; break;
		case OP_LEFT_U0: while (tape->memory[tape->pos]) (tape->pos) -= instruction->operand; break;
		case OP_IN: tape->memory[tape->pos] = (cell_t) getchar(); break;
		case OP_OUT: putchar(tape->memory[tape->pos]); fflush(stdout); break;
		case OP_LOOP_BEGIN: bf_run_loop(program, tape); break;
		case OP_LOOP_END: return EXEC_LOOP_DONE;
		default: return EXEC_UNKNOWN;
	}

	return EXEC_OK;
}

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
		exit(0);
	}

	/* Open file */
	if ( (fp = fopen(argv[1], "r")) == NULL) {
                fprintf(stderr, "Error: Could not find input file %s.\n", argv[1]);
                exit(1);
	}

	/* Get file size */
	fseek(fp, 0, SEEK_END);
	file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	/* Allocate initial buffer and read program */
	if ( (program = calloc(file_size + 1, sizeof(char))) == NULL ) {
		perror("calloc");
		fclose(fp);
		exit(1);
	}
	if (fread(program, sizeof(char), file_size, fp) != (file_size * sizeof(char))) {
		fprintf(stderr, "Error: could not read program %s.\n", argv[1]);
		free(program);
		fclose(fp);
		exit(1);
	}
	fclose(fp);

	/* Allocate buffer for compiled program */
	program_t compiled_program = {.code = malloc(sizeof(instruction_t) * file_size), .pos = 0};
	if (compiled_program.code == NULL) {
		perror("malloc");
		free(program);
		fclose(fp);
		exit(1);
	}

	/* Filter input characters and compile program */
	bf_filter(program);
	bf_compile(program, strlen(program), compiled_program.code);
	free(program);

	/* Init memory */
	tape_t tape;
	if (tape_init(&tape) != 0) {
		fprintf(stderr, "Error: could not init memory.\n");
		exit(1);
	}

	bf_run_main_loop(&compiled_program, &tape);
	free(compiled_program.code);

	return 0;
}
