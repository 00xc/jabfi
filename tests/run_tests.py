import os
import subprocess
import shlex

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

	TestResult = namedtuple("TestResult", ["stdout", "stderr"])
	expected = {
		"hello_world.b": TestResult("Hello World!", ""),
		"h.b": TestResult("H", ""),
		"pound_symbol.b": TestResult("#", ""),
		"squares.b": TestResult("\n".join(map(str, all_squares())), ""),
		"unmatched1.b": TestResult("#", "Error: found ] @ 26 with unmatched [."),
		"unmatched2.b": TestResult("#", "Error: found [ @ 26 with unmatched ].")
	}

	current_directory = os.path.dirname(os.path.realpath(__file__))
	files = [os.path.join(current_directory, file) for file in os.listdir(current_directory) if file.endswith(".b")]

	upper_directory = os.path.dirname(current_directory)
	binary = os.path.join(upper_directory, "jabfi")


	print("[*] Running {} tests".format(len(files)))
	print("-"*100)

	for file in files:
		res = subprocess.run(shlex.split("{} {}".format(binary, file)), stdout=subprocess.PIPE, stderr=subprocess.PIPE)

		stdout = res.stdout.decode().rstrip()
		stderr = res.stderr.decode().rstrip()

		if stdout == expected[os.path.basename(file)].stdout and stderr == expected[os.path.basename(file)].stderr:
			print("[+]", file, ": TEST OK")
		else:
			print("[-]", file, ": TEST FAILED")
