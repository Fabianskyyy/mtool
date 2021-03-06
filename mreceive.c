#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#define TRUE 1
#define FALSE 0
#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif
#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif
#define BUFSIZE   1024
#define TTL_VALUE 3
#define LOOPMAX   20
#define MAXIP     16

char *TEST_ADDR = "225.3.2.1";
int TEST_PORT = 4444;
unsigned long IP[MAXIP];
int NUM = 0;
char command[1128];
int SLEEP_TIME = 1000;
static int iCounter = 1;
int j=0,z=0;

typedef struct timerhandler_s {
    int s;
    char *achOut;
    int len;
    int n;
    struct sockaddr *stTo;
    int addr_size;
} timerhandler_t;
timerhandler_t handler_par;

void printHelp(void)
{
    printf("mreceive version %s\n\
           Usage: mreceive [-g GROUP] [-p PORT] [-i ADDRESS ] ... [-i ADDRESS] [-n]\n\
           mreceive [-v | -h]\n\
           \n\
           -g GROUP     IP multicast group address to listen to.  Default: 224.1.1.1\n\
           -p PORT      UDP port number used in the multicast packets.  Default: 4444\n\
           -i ADDRESS   IP addresses of one or more interfaces to listen for the given\n\
           multicast group.  Default: the system default interface.\n\
           -n           Interpret the contents of the message as a number instead of\n\
           a string of characters.  Use this with `msend -n`\n\
           -v           Print version information.\n\
           -h           Print the command usage.\n\n", VERSION);
}


int main(int argc, char *argv[])
{
    struct sockaddr_in stLocal, stFrom, stTo;
    char achIn[BUFSIZE];
    int addr_size = sizeof(struct sockaddr_in);
    int s, i;
    struct ip_mreq stMreq;
    int iTmp, iRet;
    int ipnum = 0;
    int ii;
    int rcvCountOld = 0;
    int rcvCountNew = 1;
    struct timeval tv;
    
    ii = 1;
    
    if ((argc == 2) && (strcmp(argv[ii], "-v") == 0)) {
        printf("mreceive version 2.2\n");
        return 0;
    }
    if ((argc == 2) && (strcmp(argv[ii], "-h") == 0)) {
        printHelp();
        return 0;
    }
    
    
    while (ii < argc) {
        if (strcmp(argv[ii], "-g") == 0) {
            ii++;
            if ((ii < argc) && !(strchr(argv[ii], '-'))) {
                TEST_ADDR = argv[ii];
                ii++;
            }
        } else if (strcmp(argv[ii], "-p") == 0) {
            ii++;
            if ((ii < argc) && !(strchr(argv[ii], '-'))) {
                TEST_PORT = atoi(argv[ii]);
                ii++;
            }
        } else if (strcmp(argv[ii], "-i") == 0) {
            ii++;
            if ((ii < argc) && !(strchr(argv[ii], '-'))) {
                IP[ipnum] = inet_addr(argv[ii]);
                ii++;
                ipnum++;
            }
        } else if (strcmp(argv[ii], "-n") == 0) {
            ii++;
            NUM = 1;
        } else {
            printf("wrong parameters!\n\n");
            printHelp();
            return 1;
        }
    }
    
    /* get a datagram socket */
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == INVALID_SOCKET) {
        printf("socket() failed.\n");
        exit(1);
    }
    
    /* avoid EADDRINUSE error on bind() */
    iTmp = TRUE;
    iRet = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&iTmp, sizeof(iTmp));
    if (iRet == SOCKET_ERROR) {
        printf("setsockopt() SO_REUSEADDR failed.\n");
        exit(1);
    }
    
    /* name the socket */
    stLocal.sin_family = AF_INET;
    stLocal.sin_addr.s_addr = htonl(INADDR_ANY);
    stLocal.sin_port = htons(TEST_PORT);
    iRet = bind(s, (struct sockaddr *)&stLocal, sizeof(stLocal));
    if (iRet == SOCKET_ERROR) {
        printf("bind() failed.\n");
        exit(1);
    }
    
    /* join the multicast group. */
    if (!ipnum) {                   /* single interface */
        stMreq.imr_multiaddr.s_addr = inet_addr(TEST_ADDR);
        stMreq.imr_interface.s_addr = INADDR_ANY;
        iRet = setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&stMreq, sizeof(stMreq));
        if (iRet == SOCKET_ERROR) {
            printf("setsockopt() IP_ADD_MEMBERSHIP failed.\n");
            exit(1);
        }
    } else {
        for (i = 0; i < ipnum; i++) {
            stMreq.imr_multiaddr.s_addr = inet_addr(TEST_ADDR);
            stMreq.imr_interface.s_addr = IP[i];
            iRet = setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&stMreq, sizeof(stMreq));
            if (iRet == SOCKET_ERROR) {
                printf("setsockopt() IP_ADD_MEMBERSHIP failed.\n");
                exit(1);
            }
        }
    }
    
    /* set TTL to traverse up to multiple routers */
    iTmp = TTL_VALUE;
    iRet = setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&iTmp, sizeof(iTmp));
    if (iRet == SOCKET_ERROR) {
        printf("setsockopt() IP_MULTICAST_TTL failed.\n");
        exit(1);
    }
    
    /* nonblocking socket */
    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0) {
        printf("fcntl() nonblocking flag failed.\n");
        exit(1);
    }
    flags = (flags | O_NONBLOCK);
    fcntl(s, F_SETFL, flags);
    
    /* disable loopback */
    /* iTmp = TRUE; */
    iTmp = FALSE;
    iRet = setsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&iTmp, sizeof(iTmp));
    if (iRet == SOCKET_ERROR) {
        printf("setsockopt() IP_MULTICAST_LOOP failed.\n");
        exit(1);
    }
    
    printf("Now receiving from multicast group: %s\n", TEST_ADDR);
    
    time_t exitTime = time(0) + 12;
    
    for (j = 0;time(0) <= exitTime ; j++) { // time(0) <= exitTime
        socklen_t addr_size = sizeof(struct sockaddr_in);
        static int iCounter = 1;
        //printf("\n %i \n",j);
        /* receive from the multicast address */
        
        iRet = recvfrom(s, achIn, BUFSIZE, 0, (struct sockaddr *)&stFrom, &addr_size);
        if (iRet < 0) {
            continue;
            printf("recvfrom() failed.\n");
            exit(1);
        }
        
        if (NUM) {
            gettimeofday(&tv, NULL);
            if (rcvCountNew > rcvCountOld + 1) {
                if (rcvCountOld + 1 == rcvCountNew - 1)
                    printf("****************\nMessage not received: %d\n****************\n", rcvCountOld + 1);
                else
                    printf("****************\nMessages not received: %d to %d\n****************\n",
                           rcvCountOld + 1, rcvCountNew - 1);
            }
            if (rcvCountNew == rcvCountOld) {
                printf("Duplicate message received: %d\n", rcvCountNew);
            }
            if (rcvCountNew < rcvCountOld) {
                printf("****************\nGap detected: %d from %d\n****************\n", rcvCountNew, rcvCountOld);
            }
            rcvCountOld = rcvCountNew;
        } else {
            printf("Receive msg %d from %s:%d: %s\n",
                   iCounter, inet_ntoa(stFrom.sin_addr), ntohs(stFrom.sin_port), achIn);
        }
        iCounter++;
    }
    
    printf("\nStopping receive traffic from Source . . .");
    
    // Assign our destination address
    stTo.sin_family = AF_INET;
    stTo.sin_addr.s_addr = inet_addr(TEST_ADDR);
    stTo.sin_port = htons(TEST_PORT);
    
    
    printf("\nStart sending from new Source . . .\n\n\n");
    
    for(z = 0 ; z < 10 ; z++){
    handler_par.s = s;
    handler_par.achOut = achIn;
    handler_par.len = strlen(achIn) + 1;
    handler_par.n = 0;
    handler_par.stTo = (struct sockaddr *)&stTo;
    handler_par.addr_size = addr_size;
    
    if (NUM) {
        handler_par.achOut = (char *)(&iCounter);
        handler_par.len = sizeof(iCounter);
        printf("Sending msg %d, TTL %d, to %s:%d\n", z+1, TTL_VALUE, TEST_ADDR, TEST_PORT);
    } else {
        printf("Sending msg %d, TTL %d, to %s:%d: %s\n", z+1, TTL_VALUE, TEST_ADDR, TEST_PORT, handler_par.achOut);
    }
    iRet = sendto(handler_par.s, handler_par.achOut, handler_par.len, handler_par.n, handler_par.stTo, handler_par.addr_size);
    if (iRet < 0) {
        printf("sendto() failed.\n");
        exit(1);
      }
	sleep(1);
    }
    return 0;
}
