# Installation

To run and mount this file system, run the following commands:

```cmd
sudo apt update
sudo apt install pkg-config libssl-dev libfuse-dev
```

In the directory the `Makefile` resides, run clean and make:

```cmd
make clean
make
```

Once the source code has been compiled, create the mount and root directory of the filesystems if they do not exist:

```cmd
mkdir -p rootdir
mkdir -p mountdir
```

Now run the executable with the root directory as the first argument and the mount directory as the second:

```cmd
./bbfs ./rootdir ./mountdir
```

> Note: the system is non-volatile, so the root directory will keep the files after the unmount.

To unmount the filesystem, run the following command:

```cmd
fusermount -u ./mountdir
```
