#ifndef _UTILS_H
#define _UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/ethernet.h>
#include <linux/if.h>
#include <linux/if_tun.h>

#define ERROR_PRINT_THEN_EXIT(...) do { \
    fprintf(stderr, __VA_ARGS__);       \
    exit(EXIT_FAILURE);                 \
} while(0)

int tap_alloc(char *dev);  // Move tap_alloc function here

#endif
