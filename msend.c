#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#define TRUE 1
#define FALSE 0
#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif
#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif
#define LOOPMAX   20
#define BUFSIZE   1024

//Global Variables
char *TEST_ADDR = "225.3.2.1";
int TEST_PORT = 4444;
int TTL_VALUE = 3;
int SLEEP_TIME = 1000;
unsigned long IP = INADDR_ANY;
int NUM = 0, j;

int join_flag = 0;		// not join

//Strukturen
typedef struct timerhandler_s {
    int s;
    char *achOut;
    char *achIn;
    int len;
    int n;
    struct sockaddr *stTo;
//    int addr_size;
} timerhandler_t;
timerhandler_t handler_par;
//void timerhandler();

void printHelp(void)
{
    printf("msend version %s\n\
           Usage:  msend [-g GROUP] [-p PORT] [-join] [-i ADDRESS] [-t TTL] [-P PERIOD]\n\
           [-text \"text\"|-n]\n\
           msend [-v | -h]\n\
           \n\
           -g GROUP     IP multicast group address to send to.  Default: 224.1.1.1\n\
           -p PORT      UDP port number used in the multicast packets.  Default: 4444\n\
           -i ADDRESS   IP address of the interface to use to send the packets.\n\
           The default is to use the system default interface.\n\
           -join        Multicast sender will join the multicast group.\n\
           By default a sender never joins the group.\n\
           -P PERIOD    Interval in milliseconds between packets.  Default 1000 msec\n\
           -t TTL       The TTL value (1-255) used in the packets.  You must set\n\
           this higher if you want to route the traffic, otherwise\n\
           the first router will drop the packets!  Default: 1\n\
           -text \"text\" Specify a string to use as payload in the packets, also\n\
           displayed by the mreceive command.  Default: empty\n\
           -n           Encode -text argument as a number instead of a string.\n\
           -v           Print version information.\n\
           -h           Print the command usage.\n\n", VERSION);
}

int main(int argc, char *argv[])
{
    struct sockaddr_in stLocal, stTo; //,stFrom
    char achOut[BUFSIZE] = "";
//    unsigned char achIn[BUFSIZE];
    int s,i;
    struct ip_mreq stMreq;
    int iTmp, iRet;
    int ii = 1;
//    int addr_size = sizeof(struct sockaddr_in);
//    int rcvCountOld = 0;
//    int rcvCountNew = 1;
//    struct timeval tv;
    //struct itimerval times;
    //sigset_t sigset;
    //struct sigaction act;
    
    if ((argc == 2) && (strcmp(argv[ii], "-v") == 0)) {
        printf("msend version 2.2\n");
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
        } else if (strcmp(argv[ii], "-join") == 0) {
            join_flag++;;
            ii++;
        } else if (strcmp(argv[ii], "-i") == 0) {
            ii++;
            if ((ii < argc) && !(strchr(argv[ii], '-'))) {
                IP = inet_addr(argv[ii]);
                ii++;
            }
        } else if (strcmp(argv[ii], "-t") == 0) {
            ii++;
            if ((ii < argc) && !(strchr(argv[ii], '-'))) {
                TTL_VALUE = atoi(argv[ii]);
                ii++;
            }
        } else if (strcmp(argv[ii], "-P") == 0) {
            ii++;
            if ((ii < argc) && !(strchr(argv[ii], '-'))) {
                SLEEP_TIME = atoi(argv[ii]);
                ii++;
            }
        } else if (strcmp(argv[ii], "-n") == 0) {
            ii++;
            NUM = 1;
            ii++;
        } else if (strcmp(argv[ii], "-text") == 0) {
            ii++;
            if ((ii < argc) && !(strchr(argv[ii], '-'))) {
                strcpy(achOut, argv[ii]);
                ii++;
            }
        } else {
            printf("wrong parameters!\n\n");
            printHelp();
            return 1;
        }
    }
    
    // get a datagram socket
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == INVALID_SOCKET) {
        printf("socket() failed.\n");
        exit(1);
    }
    
    // avoid EADDRINUSE error on bind()
    iTmp = TRUE;
    iRet = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&iTmp, sizeof(iTmp));
    if (iRet == SOCKET_ERROR) {
        printf("setsockopt() SO_REUSEADDR failed.\n");
        exit(1);
    }
    
    // name the socket
    stLocal.sin_family = AF_INET;
    stLocal.sin_addr.s_addr = htonl(INADDR_ANY); //IP
    stLocal.sin_port = htons(TEST_PORT);
    iRet = bind(s, (struct sockaddr *)&stLocal, sizeof(stLocal));
    if (iRet == SOCKET_ERROR) {
        printf("bind() failed.\n");
        exit(1);
    }

    //Non-blocking socket
	int flags = fcntl(s, F_GETFL, 0);
	if(flags < 0){
		printf("\nfcntl() non-blocking flag failed !\n");
		exit(1);
	}
	flags = (flags | O_NONBLOCK);
	fcntl(s, F_SETFL, flags);

    // join the multicast group.
    stMreq.imr_multiaddr.s_addr = inet_addr(TEST_ADDR);
    stMreq.imr_interface.s_addr = IP;
    if (join_flag == 1) {
        iRet = setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&stMreq, sizeof(stMreq));
        if (iRet == SOCKET_ERROR) {
            printf("setsockopt() IP_ADD_MEMBERSHIP failed.\n");
            exit(1);
        }
    }
    
    //set TTL to traverse up to multiple routers
    iTmp = TTL_VALUE;
    iRet = setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&iTmp, sizeof(iTmp));
    if (iRet == SOCKET_ERROR) {
        printf("setsockopt() IP_MULTICAST_TTL failed.\n");
        exit(1);
    }
    
    //enable loopback
    iTmp = TRUE;
    iRet = setsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&iTmp, sizeof(iTmp));
    if (iRet == SOCKET_ERROR) {
        printf("setsockopt() IP_MULTICAST_LOOP failed.\n");
        exit(1);
    }
    
    // assign our destination address
    stTo.sin_family = AF_INET;
    stTo.sin_addr.s_addr = inet_addr(TEST_ADDR);
    stTo.sin_port = htons(TEST_PORT);
    printf("\n\nNow sending to multicast group: %s\n\n", TEST_ADDR);
    
        // convert to milliseconds
        // block SIGALRM
        //set up handler for SIGALRM
        //set up interval timer
    
        handler_par.s = s;
        handler_par.achOut = achOut;
	//andler_par.achIn = achIn;
        handler_par.len = strlen(achOut) + 1;
        handler_par.n = 0;
        handler_par.stTo = (struct sockaddr *)&stTo;
//        handler_par.addr_size = addr_size;
        
        // now wait for the alarms
  
        for (i=0;i<10;i++) {
            int addr_size = sizeof(struct sockaddr_in);
            
            if (NUM) {
                achOut[3] = (unsigned char)(i >> 24);
                achOut[2] = (unsigned char)(i >> 16);
                achOut[1] = (unsigned char)(i >> 8);
                achOut[0] = (unsigned char)(i);
                printf("Send out msg %d to %s:%d\n", i+1, TEST_ADDR, TEST_PORT);
            } else {
                printf("Send out msg %d with TTL=%i to %s:%d: %s\n", i+1,TTL_VALUE, TEST_ADDR, TEST_PORT, achOut);
            }
            
            iRet = sendto(s, achOut, (NUM ? 4 : strlen(achOut) + 1), 0, (struct sockaddr *)&stTo, addr_size);
            if (iRet < 0) {
                printf("sendto() failed.\n");
                exit(1);
            }
            sleep(1);
        }
    //////// end for() /////////

//    printf("Now receiving from multicast group: %s\n", TEST_ADDR);
    
//    time_t exitTime = time(0) + 10;
//    for (j = 0; time(0) <= exitTime; j++) { // time(0) <= exitTime
//        socklen_t addr_size = sizeof(struct sockaddr_in);
/*
        iRet = recvfrom(s, achIn, BUFSIZE, 0, (struct sockaddr *)&stFrom, &addr_size);
        
        if (iRet < 0) {
            continue;
            printf("recvfrom() failed.\n");
            exit(1);
        }
        
        if (NUM) {
            gettimeofday(&tv, NULL);
            
            fflush(stdout);
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
                   j+1, inet_ntoa(stFrom.sin_addr), ntohs(stFrom.sin_port), achIn);
            
        }
*/	system("mreceive");
//	printf("\nHALLO\n");
//	sleep(1);
//    }
    
    return 0;
}				/* end main() */
