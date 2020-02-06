/**
 * Copyright (c) 2015, Martin Roth (mhroth@gmail.com)
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */


#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "tinyosc.h"

#pragma comment(lib, "ws2_32.lib")

#define OSC_BUFFER_SIZE 2048
#define OSC_PORT 8000

int bufSize = OSC_BUFFER_SIZE;
char oscBuffer[OSC_BUFFER_SIZE];
SOCKET udpSocket;
WSADATA wsaData;

static volatile bool keepRunning = true;

// handle Ctrl+C
static void sigintHandler(int x) {
    keepRunning = false;
}

/**
 * A basic program to listen to port 8000 and print received OSC packets.
 */
int main(int argc, char* argv[]) {

    char buffer[2048]; // declare a 2Kb buffer to read packet data into

    printf("Starting write tests:\n");
    int len = 0;
    char blob[8] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF };
    len = tosc_writeMessage(buffer, sizeof(buffer), "/address", "fsibTFNI",
        1.0f, "hello world", -1, sizeof(blob), blob);
    tosc_printOscBuffer(buffer, len);
    printf("done.\n");

    // open a socket to listen for datagrams (i.e. UDP packets) on port 8000
    if (WSAStartup(MAKEWORD(1, 1), &wsaData) == SOCKET_ERROR) {
        printf("Error initialising WSA.\n");
    }
    udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    printf("made socket. result: %d\n", udpSocket);

    struct sockaddr_in sin;
    int sin_len = sizeof(struct sockaddr_in);
    sin.sin_family = AF_INET;
    hostent* localHost = gethostbyname("");
    char* localIP = inet_ntoa(*(struct in_addr*) * localHost->h_addr_list);
    sin.sin_port = htons(OSC_PORT);
    sin.sin_addr.s_addr = INADDR_ANY;
    if (bind(udpSocket, (struct sockaddr*) & sin, sizeof(sin)) == -1) {
        printf("bind failure. error: %d,\n", WSAGetLastError());
    }

    char broadcast = '1';
    if (setsockopt(udpSocket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        printf("Error in setting Broadcast option\n");
    }

    printf("tinyosc is now listening on port %d.\n", OSC_PORT);
    printf("Press Ctrl+C to stop.\n");

    while (keepRunning) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(udpSocket, &readSet);
        struct timeval timeout = { 1, 0 }; // select times out after 1 second
        if (select(udpSocket + 1, &readSet, NULL, NULL, &timeout) > 0) {
            struct sockaddr sa; // can be safely cast to sockaddr_in
            int sa_len = sizeof(struct sockaddr_in);
            int len = 0;
            while ((len = (int)recvfrom(udpSocket, buffer, sizeof(buffer), 0, (struct sockaddr*) &sa, &sa_len)) > 0) {
                if (tosc_isBundle(buffer)) {
                    tosc_bundle bundle;
                    tosc_parseBundle(&bundle, buffer, len);
                    const uint64_t timetag = tosc_getTimetag(&bundle);
                    tosc_message osc;
                    while (tosc_getNextMessage(&bundle, &osc)) {
                        tosc_printMessage(&osc);
                    }
                }
                else {
                    tosc_message osc;
                    tosc_parseMessage(&osc, buffer, len);
                    tosc_printMessage(&osc);
                }
            }
        }
    }

    // close the UDP socket
    closesocket(udpSocket);
    WSACleanup();

    return 0;
}
