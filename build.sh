#!/bin/sh
gcc -o demo_ramdisk demo_ramdisk.c ublk.c -l ublksrv
gcc -o demo_disk demo_disk.c ublk.c utils.c -l ublksrv
