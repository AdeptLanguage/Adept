
# ---------------------------------------------------------
# Minimal E2E Testing Framework for Command-Line Programs
#     by Isaac Shelton
# ---------------------------------------------------------

import os
import re
import sys
from subprocess import CalledProcessError, Popen, PIPE

RED = "\x1B[31m"
NORMAL = "\x1B[0m"
GREEN = "\x1B[32m"

all_good = True

if sys.version_info.major != 3:
    print(RED + "ERROR: e2e-runner.py expects to be run with Python 3" + NORMAL)
    sys.exit(1)

if len(sys.argv) != 2:
    print(RED + "ERROR: e2e-runner.py requires executable location!" + NORMAL)
    print(RED + "  e2e-runner.py <executable>" + NORMAL)
    sys.exit(1)

def e2e_framework_run(run_all_tests_function):
    global all_good

    try:
        run_all_tests_function()
    except CalledProcessError as e:
        print(RED + "INCORRECT EXITCODE " + str(e.returncode) + ": A test command exited with incorrect status" + NORMAL)
        print(RED + "Shell Command: " + str(e.cmd) + NORMAL)
        print(RED + "Output ---------------------------" + NORMAL)
        print(e.stdout)
        print(RED + "Stderr ---------------------------" + NORMAL)
        print(e.stderr)
        all_good = False

    if not all_good:
        print(RED + "Exiting E2E testbed with status of 1..." + NORMAL)
        sys.exit(1)
    else:
        print(GREEN + "All tests passed..." + NORMAL)
        sys.exit(0)

def test(name, args, predicate, expected_exitcode="zero", only_on=None):
    if (only_on == "windows" and os.name != 'nt') or (only_on == "unix" and os.name != 'nt'):
        print("Skipped test `" + name + "` (not applicable)")
        return

    print("Running test `" + name + "`")

    global all_good
    res = Popen(args, stdout=PIPE, stderr=PIPE)
    stdout, stderr = res.communicate()

    # Remove ANSI excape sequences
    # https://stackoverflow.com/questions/14693701/how-can-i-remove-the-ansi-escape-sequences-from-a-string-in-python
    # 7-bit and 8-bit C1 ANSI sequences
    ansi_escape_8bit = re.compile(
        br'(?:\x1B[@-Z\\-_]|[\x80-\x9A\x9C-\x9F]|(?:\x1B\[|\x9B)[0-?]*[ -/]*[@-~])'
    )
    actual_output = ansi_escape_8bit.sub(b'', stderr.replace(b'\r\n', b'\n') + stdout.replace(b'\r\n', b'\n'))
    
    if not predicate(actual_output):
        print(RED + "TEST `" + name + "` FAILED: Output from command " + str(args) + " does not meet predicate." + NORMAL)
        print(RED + "Actual...\n" + NORMAL + str(actual_output))
        all_good = False
    
    miss = False

    if expected_exitcode is None or expected_exitcode == "zero":
        miss = res.returncode != 0
    elif expected_exitcode == "non-zero":
        miss = res.returncode == 0
    else:
        miss = res.returncode != expected_exitcode
    
    if miss:
        raise CalledProcessError(res.returncode, res.args, stdout, stderr)
