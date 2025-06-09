#!/bin/bash

set -e  # Exit on error

case "$1" in
  mount)
    echo "Mounting the filesystem..."
    (cd example && ../src/bbfs ./rootdir ./mountdir)
    ;;

  unmount)
    echo "Unmounting the filesystem..."
    (cd example && fusermount -u ./mountdir)
    ;;

  test)
    echo "Running tests..."
    python3 ./test/test.py
    ;;

  *)
    echo "Usage: $0 {mount|unmount|test}"
    exit 1
    ;;
esac
