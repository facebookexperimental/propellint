# PropelLint

The goal of this tool is to detect bad patterns which are not detectable via
only dynamic profiling, or only static analysis. To achieve this, the tool uses
a profile as input, and then inspects the source code to determine if a warning
should be issued.

The current focus is to detect unintentional map inserts with `operator[]`. In
C++, `map[key]` always inserts a default value. Sometimes, these are not
intended and can cause bugs and/or performance regressions.

> **Warning**
> This project is a work in progress.

## How it works

### Profile

The profile must be a JSON file with the following format.

```json
[
    {
        "stack_combined": [
            "someFunction@some_source_file.cpp:13",
            "someOtherFunction@some_other_source_file.cpp:31",
            <...>
        ],
        "total_weight": 458188117
    },
    <...>
]
```

Internally, we use a profiler called Strobelight, and pre-filter the data to
only contain stacks with `operator[]`.

### Analysis

 1. The profile is parsed, and only `operator[]` locations that insert are kept.
    We can find those by looking for specific sub-functions that we know are
    signs of insertion.
 2. For each file, we need to file the necessary compile commands. The tool is
    currently based on Buck, which it queries to find the compilation database
    of each file.
 3. The AST is generated using Clang, and inspected to filter out cases with a
    high-likelihood of intentional.

## Getting started

```bash
[~] git clone https://github.com/facebookexperimental/propellint.git
[~] cd propellint
[~/propellint] mkdir build
[~/propellint] cd build
[~/propellint/build] cmake ..
[~/propellint/build] make
```

## License

This work is licensed under the Apache License 2.0.
