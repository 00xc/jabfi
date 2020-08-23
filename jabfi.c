#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MEM_SIZE		(2<<15)

/* Tape struct */
typedef uint_fast8_t cell_t;
typedef uint_fast16_t tape_pos_t;
typedef struct {
	cell_t memory[MEM_SIZE];
	tape_pos_t pos;
} tape_t;

/* Program struct */
typedef uint_fast32_t program_pos_t;
typedef struct {
	char* code;
	program_pos_t pos;
} program_t;
#define PROGRAM_NEXT(program)	program->code[program->pos++]


/* Values returned by `run_instruction` */
enum exec_status {
	INSTRUCTION_UNKNOWN,
	INSTRUCTION_OK,
	LOOP_DONE
};

int run_instruction(program_t* program, tape_t* tape, char c);
void run_loop(program_t* program, tape_t* tape);

/* Initialize memory cells */
int tape_init(tape_t* tape) {
	tape->pos = 0;
	memset(tape->memory, 0, MEM_SIZE * sizeof(cell_t));
	return 0;
}

/* Top level instruction loop */
void run_main_loop(program_t* program, tape_t* tape) {
	register char c;

	while ( (c = PROGRAM_NEXT(program)) != 0 ) {
		if (run_instruction(program, tape, c) == LOOP_DONE) {
			fprintf(stderr, "Error: found ] @ %ld with unmatched [.\n", program->pos);
			free(program->code);
			exit(1);
		}
	}
}

/* Called upon reading a `[` instruction */
void run_loop(program_t* program, tape_t* tape) {
	register char c = program->code[program->pos];
	program_pos_t loop_beginning = program->pos;

	if (tape->memory[tape->pos] == 0) {

		/* Skip to matching ], while we are not at EOF */
		int_fast16_t loop = 1;
		while ( loop != 0 && (c = PROGRAM_NEXT(program)) != 0 ) {
			if (c == ']') {
				--loop;
			} else if (c == '[') {
				++loop;
			}
		}

	} else {

		/* Loop until counter is zero, while we are not at EOF */
		while (tape->memory[tape->pos] != 0 && c != 0) {
			program->pos = loop_beginning;
			while ( (c = PROGRAM_NEXT(program)) != 0 ) {
				if (run_instruction(program, tape, c) == LOOP_DONE) {
					break;
				}
			}
		}
	}

	/* If we reached EOF during a loop there was an error */
	if (c == 0) {
		fprintf(stderr, "Error: found [ @ %ld with unmatched ].\n", loop_beginning);
		free(program->code);
		exit(1);
	}
}

/* Execute a single instruction */
inline int run_instruction(program_t* program, tape_t* tape, char c) {
	switch(c) {
		case '<':
			--(tape->pos);
			break;
		case '>':
			++(tape->pos);
			break;
		case '+':
			++(tape->memory[tape->pos]);
			break;
		case '-':
			--(tape->memory[tape->pos]);
			break;
		case '.':
			putchar(tape->memory[tape->pos]);
			fflush(stdout);
			break;
		case ',':
			tape->memory[tape->pos] = (cell_t) getchar();
			break;
		case '[':
			run_loop(program, tape);
			break;
		case ']':
			return LOOP_DONE;
		default:
			return INSTRUCTION_UNKNOWN;
	}

	return INSTRUCTION_OK;
}

int main(int argc, const char* argv[]) {
	FILE* fp;
	size_t file_size;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s program_file.b\n", argv[0]);
		exit(0);
	}

	/* Init memory */
	tape_t tape;
	if (tape_init(&tape) != 0) {
		fprintf(stderr, "Error: could not init memory.\n");
		exit(1);
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

	/* Allocate program */
	program_t program = {
		.code = calloc(file_size + 1, sizeof(char)),
		.pos = 0
	};
	if (program.code == NULL) {
		fclose(fp);
		perror("malloc");
	}

	/* Load program */
	if (fread(program.code, sizeof(char), file_size, fp) != (file_size * sizeof(char))) {
		fprintf(stderr, "Error: could not read program %s.\n", argv[1]);
		exit(1);
	}
	fclose(fp);

	/* Run program and exit */
	run_main_loop(&program, &tape);
	free(program.code);
	return 0;
}
