#!/bin/bash

COPYFILE_DISABLE=1 \
tar --dereference --exclude='build' \
	--exclude='cmake-build-debug' \
	--exclude='cmake-build-release' \
	--exclude='submission.tar.gz' \
    --exclude='workloads' -czf submission.tar.gz *
