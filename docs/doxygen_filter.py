#!/usr/bin/python
"""
Python 3 script
doxygen_filter.py

Pilot's API is written in terms of macros, chiefly so that the caller's filename
and line number can be covertly incorporated into the call to the real function.
For example, when the user calls PI_Write, the PI_Write macro is expanded to call
the real API function PI_Write_ (with trailing underscore).

Generating man pages based on macro definitions would be confusing and unhelpful,
therefore this filter script performs a couple transformations:  it filters out
the #defined macros that start with PI_, which the user does not need to see, and
it changes the names of the real API functions from the underscore form to the name
that the user expects to call. In this way, there will be a man page for PI_Write
but not for PI_Write_.
"""

import re

def filter_function_macros(input_stream, output_stream):
    """
    Write all data from input_stream to output_stream except function
    macro definitions that begin with PI_.
    """
    pattern = re.compile(r'\s*#define\s+PI_\w+\([a-zA-Z0-9_,.\s]*\)')
    in_fun_macro = False
    for line in input_stream:
        if pattern.match(line):
            in_fun_macro = True
        if in_fun_macro:
            if not line.endswith('\\\n'):
                in_fun_macro = False
        else:
            output_stream.write(line)
    return output_stream

def filter_trailing_underscore(input_stream, output_stream):
    """
    Write all data from input_stream to output_stream except function
    names that start with ``PI_`` and end in ``_`` will have the
    trailing ``_`` removed.
    """
    pattern = re.compile(r'(.*?PI_[a-zA-Z0-9]+)_(\()')
    for line in input_stream:
        match = pattern.match(line)
        if match:
            line = match.group(1) + match.group(2) + line[len(match.group(0)):]
        output_stream.write(line)
    return output_stream

if __name__ == '__main__':
    import sys

    class MockOStream(list):
        def write(self, data):
            self.append(data)

    if len(sys.argv) == 2:
        if sys.argv[1] == "-":
            f = sys.stdin
        else:
            f = open(sys.argv[1], "r")
        s = MockOStream()
        filter_trailing_underscore(filter_function_macros(f, s), sys.stdout)
        sys.exit(0)
    elif len(sys.argv) > 2:
        import os
        print("Usage: %s [input_file]" % os.path.basename(__file__))
        print("If input_file is not specified, then the unit tests are run.")
        print("If input_filt is \"-\", then stdin is used.")
        sys.exit(1)

    ## Unit Tests ##
    import unittest

    class TestFilters(unittest.TestCase):
        def setUp(self):
            self.ostream = MockOStream()

        def test_filter_function_macros(self):
            input_data = ["int i;\n", "#define PI_foo(a, b) \\\n",
                          "  do { something(); \\\n", "  } while (0);\n",
                          "void foo(int c, int b);"]
            filter_function_macros(input_data, self.ostream)
            self.assertEqual(self.ostream, [input_data[0], input_data[4]])

        def test_filter_trailing_underscore(self):
            input_data = ["int PI_Foobar_(int a, int b);\n", "int t;"]
            filter_trailing_underscore(input_data, self.ostream)
            self.assertEqual(
                self.ostream,
                ["int PI_Foobar(int a, int b);\n", input_data[1]]
            )
    unittest.main()
