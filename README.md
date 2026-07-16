# ECE318 Operating Systems

![C](https://img.shields.io/badge/C-systems-blue)
![Linux Kernel](https://img.shields.io/badge/Linux-kernel-black)
![FUSE](https://img.shields.io/badge/FUSE-filesystem-purple)
![Scheduling](https://img.shields.io/badge/CPU-scheduling-orange)
![Coursework](https://img.shields.io/badge/UTH-ECE318-green)

Operating systems coursework focused on Linux kernel interfaces, custom scheduling behavior, and a FUSE filesystem implementation. The projects move from kernel modules and system-call experiments to CPU scheduling simulation and finally to a user-space filesystem with persistent block storage.

## Standout work

The highlight of this repository is **Project 3**, a FUSE filesystem that stores duplicate file blocks only once instead of writing repeated data multiple times. The implementation goes beyond the simplified version suggested by the handout: rather than relying on the easier shortcuts, it keeps the full filesystem behavior working with custom metadata, block lookup, node tracking, persistence, and test coverage.

Key pieces of the Project 3 implementation include:

- **Duplicate-block storage:** repeated file blocks are represented once in the backing store and referenced through filesystem metadata.
- **Custom block and node metadata:** the filesystem tracks both stored blocks and filesystem nodes through dedicated hash-table structures.
- **Persistent backing storage:** filesystem state survives unmounts through metadata and block repository files in the root directory.
- **Full FUSE operation path:** file creation, deletion, reads, writes, truncation, directory operations, and rename behavior are handled in the filesystem layer.
- **End-to-end stress testing:** the filesystem was pushed beyond small synthetic tests by running a Minecraft server on top of it, until multithreaded world generation became the limiting factor.

That final test is the main story worth surfacing: this was not only a toy block-deduplication exercise, but a working filesystem that could support a real application workload far past the basic assignment examples.

## Course contents

| Path | Description |
| --- | --- |
| `Project1/` | Linux kernel module and system-call work. |
| `Project1/project1_find_roots/` | User-space wrapper and test files for the `find_roots` system-call work. |
| `Project1/project1_module/` | Kyber/elevator kernel module changes and build files. |
| `Project1/sysfs_module/` | sysfs kernel module implementation. |
| `Project2/` | CPU scheduling simulator, workload configs, plotting scripts, and report. |
| `Project2/Scheduler_VM/` | Scheduler VM implementation with SJF and modified goodness-based scheduling behavior. |
| `Project3/` | FUSE filesystem project, development tree, final packaged source layout, experiments, and report. |
| `Project3/project_3_3672_3771_3796/filesystem/` | Final Project 3 filesystem source and Makefile. |
| `Project3/project_3_3672_3771_3796/experiments/` | Python unittest workflow for Project 3 filesystem behavior. |
| `Project3/project_3_3672_3771_3796/report/` | Final Project 3 report. |

### Project 1 — kernel interfaces

Project 1 contains Linux kernel-facing work: module builds, syscall-side experiments, sysfs exposure, and the final patch artifact used for the kernel changes. The project is organized into separate folders for each component so the module and user-space pieces can be inspected independently.

### Project 2 — CPU scheduling

Project 2 implements and evaluates scheduling behavior in a simulator. The `Scheduler_VM` tree includes workload configuration files, scheduler source code, scripts for running experiments, and plotting support for comparing scheduling behavior across workloads.

### Project 3 — FUSE filesystem

Project 3 is the largest implementation in the repository. The final source layout is under:

```text
Project3/project_3_3672_3771_3796/
```

The final filesystem source is in:

```text
Project3/project_3_3672_3771_3796/filesystem/
```

The tests and experiment workflow are in:

```text
Project3/project_3_3672_3771_3796/experiments/
```

The older `fuse-compressed-fs/` and `fuse-tutorial-2018-02-04/` trees show the development path from the base FUSE tutorial code toward the final filesystem implementation.

## Requirements

General tools:

- Linux environment
- `gcc`
- `make`
- Python 3

Project 3 additionally needs FUSE and OpenSSL development headers:

```bash
sudo apt update
sudo apt install pkg-config libssl-dev libfuse-dev
```

Project 1 targets Linux kernel/module workflows, so it should be built in an environment with the appropriate kernel headers/source setup for the assignment.

## Quick validation

Build and run a small Project 2 scheduler example:

```bash
cd Project2/Scheduler_VM/src
make
./sjf_sched ../confs/simple.conf
make clean
```

Build the final Project 3 filesystem:

```bash
cd Project3/project_3_3672_3771_3796/filesystem
make
```

Run Project 3 tests after building `bbfs`:

```bash
cd ../experiments
mkdir -p ./fs/mountdir ./fs/rootdir
python3 -m unittest test
```

## Full setup explanation

### Project 2 scheduler simulator

From the repository root:

```bash
cd Project2/Scheduler_VM/src
make
```

Run one of the included workload configurations:

```bash
./sjf_sched ../confs/simple.conf
```

The project also includes additional configurations under:

```text
Project2/Scheduler_VM/confs/
```

and plotting/experiment helpers under:

```text
Project2/Scheduler_VM/run.sh
Project2/Scheduler_VM/plot.py
```

### Project 3 filesystem

Install dependencies first:

```bash
sudo apt update
sudo apt install pkg-config libssl-dev libfuse-dev
```

Build the filesystem:

```bash
cd Project3/project_3_3672_3771_3796/filesystem
make clean
make
```

Create the root and mount directories:

```bash
mkdir -p rootdir mountdir
```

Run the filesystem:

```bash
./bbfs ./rootdir ./mountdir
```

Unmount it when finished:

```bash
fusermount -u ./mountdir
```

Run the experiment tests:

```bash
cd ../experiments
mkdir -p ./fs/mountdir ./fs/rootdir
python3 -m unittest test
```

### Project 1 kernel work

Project 1 contains source and patch artifacts for kernel-facing assignment components. Inspect the relevant folder for each part:

```text
Project1/project1_find_roots/
Project1/project1_module/
Project1/sysfs_module/
```

Build commands depend on the kernel/module environment used for the assignment, but the source layout keeps each component separate for easier inspection and reuse.
