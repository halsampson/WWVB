#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#pragma comment(lib,"Ws2_32.lib")

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;

#define NTP_TIMESTAMP_70_YEARS 2208988800ull

// NTP packet struct
typedef struct {
    // LSB first on little-endian CPUs:
    uint8_t mode : 3;  // mode 3 for client.
    uint8_t vn : 3;    // protocol Version Number
    uint8_t li : 2;    // Leap year Indicator

    uint8_t stratum;         // Stratum level of the local clock.
    uint8_t poll;            // Maximum interval between successive messages.
    uint8_t precision;       // Precision of the local clock.

    uint32_t rootDelay;      // Total round trip delay time.
    uint32_t rootDispersion; // Max error allowed from primary clock source.
    uint32_t refId;          // Reference clock identifier.

    uint32_t refTm_s;        // Reference time-stamp seconds.
    uint32_t refTm_f;        // Reference time-stamp fraction of a second.

    uint32_t origTm_s;       // Originate time-stamp seconds.
    uint32_t origTm_f;       // Originate time-stamp fraction of a second.

    uint32_t rxTm_s;         // Received time-stamp seconds.
    uint32_t rxTm_f;         // Received time-stamp fraction of a second.

    uint32_t txTm_s;         // Transmit time-stamp seconds.
    uint32_t txTm_f;         // Transmit time-stamp fraction of a second.
} ntp_packet;                // Total: 48 bytes.

void err(const char *message) {
  printf("Error: %s %d\n", message, GetLastError());
  exit(1);
}

double ntpTime() {
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData)) 
    err("WSAStartup fail");

  ntp_packet ntp_p = {0};
  ntp_p.vn = 3;
  ntp_p.mode = 3; 

  SOCKET socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (socket_fd < 0)
      err("open socket");

  struct hostent *ip = gethostbyname("us.pool.ntp.org");
  if (ip == NULL)
      err("hostname");

  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  memcpy(&addr.sin_addr.s_addr, ip->h_addr_list[0], ip->h_length);
  addr.sin_port = htons(123); // UDP
  if (connect(socket_fd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
      err("connect");

  uint32_t t4 = (uint32_t)time(NULL) + NTP_TIMESTAMP_70_YEARS;
  ntp_p.txTm_s = ntohl(t4);
  if (send(socket_fd, (char *) &ntp_p, sizeof(ntp_packet), 0) < 0)
      err("send to socket");

  if (recv(socket_fd, (char *) &ntp_p, sizeof(ntp_packet), 0) < 0)
      err("read socket");

  closesocket(socket_fd);
  WSACleanup();

  ntp_p.txTm_s = htonl(ntp_p.txTm_s); // Seconds
  ntp_p.txTm_f = htonl(ntp_p.txTm_f);   // Fraction

// TODO: subtract estimated net delay (typically < 10 ms, so not critical)
// (also add radio propagation delay to Ft. Collins -- could ping NIST)
/*
  SYSTEMTIME st;
  GetSystemTime(&st); // millisec -- sufficient

  FILETIME ft;
  SystemTimeToFileTime(); // 100 ns + jitter

  QueryPerformanceCounter()
  QueryPerformanceFrequency()  // 10 MHz

  t0 is the client's timestamp of the request packet transmission,
  t1 is the server's timestamp of the request packet reception 
  t2 is the server's timestamp of the response packet transmission and
  t3 is the client's timestamp of the response packet reception
  Time Correction = ((t2+t1)/2) - ((t3+t0)/2)

  ntp_p.refTm_s = ntohl( ntp_p.refTm_s ); // Time-stamp seconds.
  ntp_p.refTm_f = ntohl( ntp_p.refTm_f ); // Time-stamp fraction of a second.
  ntp_p.origTm_s = ntohl( ntp_p.origTm_s ); // Time-stamp seconds.
  ntp_p.origTm_f = ntohl( ntp_p.origTm_f ); // Time-stamp fraction of a second.
  ntp_p.rxTm_s = ntohl( ntp_p.rxTm_s ); // Time-stamp seconds.
  ntp_p.rxTm_f = ntohl( ntp_p.rxTm_f ); // Time-stamp fraction of a second.

  int delta = (int)(((ntp_p.rxTm_s - ntp_p.origTm_s) + (ntp_p.txTm_s - t4)) / 2);
  int delta_f = (int)(ntp_p.rxTm_f - ntp_p.origTm_f);
*/
 
  return ntp_p.txTm_s - NTP_TIMESTAMP_70_YEARS + (double)ntp_p.txTm_f / (1LL << 32);  // TODO: correct for trip delay
}
