# Testing

This directory contains the `test.py` test file. The test expects its own directories for the filesystem, the `mountdir` and the `rootdir` in the `fs/` folder, so please make sure that bbfs is not mounted before running the tests.

To run the tests Python version 3 should be installed in the system.

The test uses [`unittest`](https://docs.python.org/3/library/unittest.html) python library, which is a testing framework. Each class contains different types of tests. The test classes a) mount the filesystem, b) execute the tests, c) delete all files in the root directory and d) unmount the filesystem.

> Note: the test expects the `bbfs` executable to be in `../filesystem/bbfs` to run.

First, run `make` in the `../filesystem` directory and then create the two folders if they do not exist:

```cmd
mkdir -p ./fs/mountdir
mkdir -p ./fs/rootdir
```

To run the tests run:

```cmd
python3 test.py
# or
python3 -m unittest test
```

To execute one specific test, run:

```cmd
python3 -m unittest test.TestClass.test_case_funciton_name
# for example:
python3 -m unittest test.TestBlocks.test_compression
```

After the the tests are done, logs will be generated in the current folder.
