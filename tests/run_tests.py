import os
import sys
import subprocess
import shlex
import time

from collections import namedtuple

def all_squares(lim=10000):
	out = []
	j = 0
	for i in range(0, lim+1):
		while (j*j) <= i:
			if j*j == i:
				out.append(i)
				break
			j += 1
	return out

if __name__ == "__main__":

	TestResult = namedtuple("TestResult", ["stdin", "stdout", "stderr"])
	tests = {
		"hello_world.b": TestResult(None, "Hello World!", ""),
		"h.b": TestResult(None, "H", ""),
		"pound_symbol.b": TestResult(None, "#", ""),
		"squares.b": TestResult(None, "\n".join(map(str, all_squares())), ""),
		"unmatched1.b": TestResult(None, "#", "Error: found ] @ 14 with unmatched [."),
		"unmatched2.b": TestResult(None, "#", "Error: found [ @ 14 with unmatched ]."),
		"collatz.b": TestResult(b"5492280743\x0d\x0a\x00", "403", "")
	}

	# Retrieve test programs
	current_directory = os.path.dirname(os.path.realpath(__file__))
	files = [os.path.join(current_directory, file) for file in os.listdir(current_directory) if file.endswith(".b")]

	# Retrieve binary
	upper_directory = os.path.dirname(current_directory)
	binary = os.path.join(upper_directory, "jabfi")

	print("[*] Running {} tests".format(len(files)))
	print("-"*60)

	# Run tests
	for file in files:

		filename = os.path.basename(file)

		if filename not in tests:
			print("WARNING: test for file '{}' not found".format(filename), file=sys.stderr)
			continue
		expected_test = tests[filename]

		t0 = time.perf_counter()
		res = subprocess.run(shlex.split("{} {}".format(binary, file)), input=expected_test.stdin , stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		elapsed = time.perf_counter() - t0

		# Collect test outputs
		stdout = res.stdout.decode().rstrip()
		stderr = res.stderr.decode().rstrip()

		if stdout == expected_test.stdout and stderr == expected_test.stderr:
			print("[+]", file, ": TEST OK ({} ms)".format(round(elapsed*1000, 3)))
		else:
			print("[-]", file, ": TEST FAILED")
