//
// Created by mateusz on 28.05.19.
//
#include <stdio.h>
#include <string>
#include "err.h"

using namespace std;

#ifndef SIKI_HELPER_H
#define SIKI_HELPER_H

constexpr size_t CMD_SIZE = 65489;
constexpr size_t CMD_SIMPL_DATA_SIZE = CMD_SIZE - 10 * sizeof(char) - sizeof(uint64_t);
constexpr size_t CMD_CMPLX_DATA_SIZE = CMD_SIZE - 10 * sizeof(char) - 2 * sizeof(uint64_t);


union CMD {
    //TODO poczytac o packach by kompilator nie zmienił kolejności plików
    struct SIMPL {
        char cmd[10];
        uint64_t cmd_seq;
        char data[CMD_SIMPL_DATA_SIZE];

    } SIMPL;

    struct CMPLX {
        char cmd[10];
        uint64_t cmd_seq;
        uint64_t param;
        char data[CMD_CMPLX_DATA_SIZE];
    } CMPLX;
};

/* wypisuje informacje o blednym zakonczeniu funkcji systemowej
i konczy dzialanie */
extern void syserr(const char *fmt, ...);

/* wypisuje informacje o bledzie i konczy dzialanie */
extern void fatal(const char *fmt, ...);

void pckg_error(const struct sockaddr_in &addr, const string &info);

void send_simpl_cmd(int sock, struct sockaddr_in &addr, const string &cmd, uint64_t cmd_seq, const string &data);

void send_cmplx_cmd(int sock, struct sockaddr_in &addr, const string &cmd, uint64_t cmd_seq, uint64_t param,
                    const string &data);

#endif //SIKI_HELPER_H
