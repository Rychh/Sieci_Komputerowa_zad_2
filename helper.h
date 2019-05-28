//
// Created by mateusz on 28.05.19.
//
#include <stdio.h>

#ifndef SIKI_HELPER_H
#define SIKI_HELPER_H

constexpr size_t CMD_SIZE = 1 << (1 << (1 << (1 << 1)));

struct SIMPL_CMD {
    char cmd[10];
    uint64_t cmd_seq;
    char data[CMD_SIZE - 10 * sizeof(char) - sizeof(uint64_t)];
};

struct CMPLX_CMD {
    char cmd[10];
    uint64_t cmd_seq;
    uint64_t param;
    char data[CMD_SIZE - 10 * sizeof(char) - 2 * sizeof(uint64_t)];
};

#endif //SIKI_HELPER_H
