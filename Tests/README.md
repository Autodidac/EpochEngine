# Shared tests

Keep tests in this tree, grouped by their owning subsystem. Shared launch and
build helpers live in `Tools/`; test implementations do not live in `Tools/tests`.

Use `python Tools/run_tests.py` for all Python suites. Use the selected CMake
preset's CTest lane for registered C++ contracts. Engine contract checks remain
available through the built Editor's `--engine-contract-self-test` option.
