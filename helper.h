//
// Created by mateusz on 28.05.19.
//
#include <stdio.h>

#ifndef SIKI_HELPER_H
#define SIKI_HELPER_H

constexpr size_t CMD_SIZE = (1 << 16) - 31; // cos pomiedzy 31-25

union CMD {

    struct SIMPL {
        char cmd[10];
        uint64_t cmd_seq;
        char data[CMD_SIZE - 10 * sizeof(char) - sizeof(uint64_t)];
    } SIMPL;

    struct CMPLX {
        char cmd[10];
        uint64_t cmd_seq;
        uint64_t param;
        char data[CMD_SIZE - 10 * sizeof(char) - 2 * sizeof(uint64_t)];
    } CMPLX;
};
#endif //SIKI_HELPER_H
