#include <stdio.h>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <cstdarg>
#include "helper.h"
#include "err.h"

using namespace std;


void syserr(const char *fmt, ...)
{
    va_list fmt_args;
    int err = errno;

    fprintf(stderr, "ERROR: ");

    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end (fmt_args);
    fprintf(stderr," (%d; %s)\n", err, strerror(err));
    exit(EXIT_FAILURE);
}



void fatal(const char *fmt, ...)
{
    va_list fmt_args;

    fprintf(stderr, "ERROR: ");

    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end (fmt_args);

    fprintf(stderr,"\n");
    exit(EXIT_FAILURE);
}


void
send_cmd(int sock, struct sockaddr_in &addr, const string &cmd, uint64_t cmd_seq, uint64_t param, const string &data,
         bool isSimple) {
    socklen_t addr_len;
    ssize_t snd_len;
    int flag = 0;
    CMD mess;
    memset(mess.CMPLX.cmd, 0, 10);
    strncpy(mess.CMPLX.cmd, cmd.c_str(), 10);
    mess.CMPLX.cmd_seq = htobe64(cmd_seq);
    if (isSimple) {
        strncpy(mess.SIMPL.data, data.c_str(), CMD_SIMPL_DATA_SIZE);
    } else {
        mess.CMPLX.param = htobe64(param);
        strncpy(mess.CMPLX.data, data.c_str(), CMD_CMPLX_DATA_SIZE);
    }

    addr_len = (socklen_t) sizeof(addr);
    snd_len = sendto(sock, &mess, sizeof(CMD), flag,
                     (struct sockaddr *) &addr, addr_len);

    if (snd_len != (ssize_t) sizeof(CMD)) {
        syserr("partial / failed write");
    }
    if (isSimple) {
        cout << "Wyslałem SIMPL:\n { cmd:" << mess.SIMPL.cmd << "; bitow:" << snd_len << "; cmd_seq:" << cmd_seq
             << ";\n";
        cout << "data:\n" << mess.SIMPL.data << "\n}\n";
    } else {
        cout << "Wyslałem CMPLX:\n { cmd:" << mess.CMPLX.cmd << "; bitow:" << snd_len << "; cmd_seq:" << cmd_seq
             << ";\n";
        cout << "param:" << param << "\n";
        cout << "data:\n" << mess.CMPLX.data << "\n}\n";
    }
}

void send_simpl_cmd(int sock, struct sockaddr_in &addr, const string &cmd, uint64_t cmd_seq, const string &data) {
    send_cmd(sock, addr, cmd, cmd_seq, 0, data, true);
}

void send_cmplx_cmd(int sock, struct sockaddr_in &addr, const string &cmd, uint64_t cmd_seq, uint64_t param,
                    const string &data) {
    send_cmd(sock, addr, cmd, cmd_seq, param, data, false);

}
