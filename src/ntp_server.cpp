#include "Arduino.h"
#include "ntp_server.h"
#include <lwip/def.h>
#include "datastore.h"
#define NTP_TIMESTAMP_DELTA 2208988800ull

// this will try to get ms since PPS
/*
[
  {"IP":"192.168.179.26","syncs":70,"last":"10.08.2023  10:10:57"},
  {"IP":"192.168.179.29","syncs":4,"last":"10.08.2023  10:05:34"},
  {"IP":"192.168.179.52","syncs":1,"last":"10.08.2023  09:14:32"},
  {"IP":"192.168.179.46","syncs":1,"last":"10.08.2023  09:20:15"},
  {"IP":"192.168.179.50","syncs":1,"last":"10.08.2023  09:47:15"},
  {"IP":"192.168.179.34","syncs":1,"last":"10.08.2023  09:51:04"}
  ]
*/
client_settings_t ntp_clients[20];
int totalClients = 0;

typedef struct
{
  uint8_t mode : 3; // mode. Three bits. Client will pick mode 3 for client.
  uint8_t vn : 3;   // vn.   Three bits. Version number of the protocol.
  uint8_t li : 2;   // li.   Two bits.   Leap indicator.
} ntp_flags_t;

typedef union
{
  uint32_t data;
  uint8_t byte[4];
  char c_str[4];
} refID_t;

typedef struct
{

  ntp_flags_t flags;
  uint8_t stratum;   // Eight bits. Stratum level of the local clock.
  uint8_t poll;      // Eight bits. Maximum interval between successive messages.
  uint8_t precision; // Eight bits. Precision of the local clock.

  uint32_t rootDelay;      // 32 bits. Total round trip delay time.
  uint32_t rootDispersion; // 32 bits. Max error aloud from primary clock source.
  refID_t refId;           // 32 bits. Reference clock identifier.

  uint32_t refTm_s; // 32 bits. Reference time-stamp seconds.
  uint32_t refTm_f; // 32 bits. Reference time-stamp fraction of a second.

  uint32_t origTm_s; // 32 bits. Originate time-stamp seconds.
  uint32_t origTm_f; // 32 bits. Originate time-stamp fraction of a second.

  uint32_t rxTm_s; // 32 bits. Received time-stamp seconds.
  uint32_t rxTm_f; // 32 bits. Received time-stamp fraction of a second.

  uint32_t txTm_s; // 32 bits and the most important field the client cares about. Transmit time-stamp seconds.
  uint32_t txTm_f; // 32 bits. Transmit time-stamp fraction of a second.

} ntp_packet_t;

uint8_t __calloverhead = 0;

AsyncUDP udp;
uint32_t (*fnc_read_utc)(void) = NULL;
uint32_t (*fnc_read_subsecond)(void) = NULL;

NTP_Server::NTP_Server()
{
}

NTP_Server::~NTP_Server()
{
}

uint8_t DeterminePrecision(void)
{
  /* We need to compute the call overhead */
  uint32_t utc_read = 0;
  uint32_t mil_read = 0;
  if (fnc_read_utc != NULL)
  {
    uint32_t start = micros();
    for (uint32_t i = 0; i < 1024; i++)
    {
      utc_read = fnc_read_utc();
      mil_read = fnc_read_subsecond();
    }
    uint32_t end = micros();
    double runtime = (((double)(end) - (double)(start)) / ((double)(1000000.0) * (double)(1024))) + ((double)(1)); // One second as we don't keep track on the fractions
    __calloverhead = log2(runtime);
  }
  else
  {
    /* this is a bad one ! */
    configASSERT(fnc_read_utc != NULL);
  }
  return 0;
}

bool NTP_Server::begin(uint16_t port, uint32_t (*fnc_getutc_time)(void), uint32_t (*fnc_get_subsecond)(void))
{

  bool started = false;
  fnc_read_utc = fnc_getutc_time;
  fnc_read_subsecond = fnc_get_subsecond;
  if (udp.listen(port))
  {
    started = true;
    udp.onPacket(NTP_Server::processUDPPacket);
  }

  return started;
}

/* static function */
void NTP_Server::processUDPPacket(AsyncUDPPacket &packet)
{
  uint32_t processing_start = 0;
  uint32_t millisec = 0;
  ntp_packet_t ntp_req;

  if (NULL != fnc_read_subsecond)
  {
    millisec = fnc_read_subsecond();
  }

  if (fnc_read_utc != NULL)
  {
    processing_start = fnc_read_utc() + NTP_TIMESTAMP_DELTA;
  }
  else
  {
    return;
  }

  if (packet.length() != sizeof(ntp_packet_t))
  {
    /* this is not what we want ! */
    return;
  }

  memcpy(&ntp_req, packet.data(), sizeof(ntp_packet_t));

  /* Next is to swap the data arround */

  ntp_req.rootDelay = ntohl(ntp_req.rootDelay);
  ntp_req.rootDispersion = ntohl(ntp_req.rootDispersion);
  ntp_req.refId.data = ntohl(ntp_req.refId.data);

  ntp_req.refTm_s = ntohl(ntp_req.refTm_s);
  ntp_req.refTm_f = ntohl(ntp_req.refTm_f);

  ntp_req.origTm_s = ntohl(ntp_req.txTm_s);
  ntp_req.origTm_f = ntohl(ntp_req.txTm_f);

  ntp_req.rxTm_s = ntohl(ntp_req.rxTm_s);
  ntp_req.rxTm_f = ntohl(ntp_req.rxTm_f);

  ntp_req.txTm_s = ntohl(ntp_req.txTm_s);
  ntp_req.txTm_f = ntohl(ntp_req.txTm_f);

  ntp_req.flags.li = 0;   // NONE
  ntp_req.flags.vn = 4;   // NTP Version 4
  ntp_req.flags.mode = 4; // Server

  strncpy(ntp_req.refId.c_str, "PPS", sizeof(ntp_req.refId.c_str));
  ntp_req.stratum = 1;
  // We don't touch ntp_req.poll
  ntp_req.precision = __calloverhead;
  ntp_req.rootDelay = 1;
  ntp_req.rootDispersion = 1;

  /* We don't touch the originate timestamp */
  ntp_req.rxTm_s = processing_start;
  ntp_req.rxTm_f = 0;
  ntp_req.refTm_s = processing_start;
  ntp_req.refTm_f = 0;

  /* UNIX Start is 1.1.1970 and GPS Start is 1.1.1900 */
  // ntp_req.refTm_s =  ntp_req.refTm_s +  NTP_TIMESTAMP_DELTA; // We need to add 70 Years
  ntp_req.txTm_s = fnc_read_utc() + NTP_TIMESTAMP_DELTA;
  ntp_req.txTm_f = 0;

  ntp_req.rootDelay = htonl(ntp_req.rootDelay);
  ntp_req.rootDispersion = htonl(ntp_req.rootDispersion);

  ntp_req.refTm_s = htonl(ntp_req.refTm_s);
  ntp_req.refTm_f = htonl(ntp_req.refTm_f);
  ntp_req.origTm_s = htonl(ntp_req.origTm_s);
  ntp_req.origTm_f = htonl(ntp_req.origTm_f);

  ntp_req.rxTm_s = htonl(ntp_req.rxTm_s);
  // ntp_req.rxTm_f = htonl( ntp_req.rxTm_f );
  ntp_req.txTm_s = htonl(ntp_req.txTm_s);
  ntp_req.rxTm_f = htonl(millisec * 4294967);
  ntp_req.txTm_f = htonl(millisec * 4294967);

  ntp_req.txTm_f = htonl(ntp_req.txTm_f);

  packet.write((uint8_t *)&ntp_req, sizeof(ntp_packet_t));

  const time_t t = fnc_read_utc();
  struct tm *ti;
  ti = localtime(&t);

  IPAddress ipAdr = packet.remoteIP();
  String ip = ipAdr.toString();

  bool isIn = false;
  for (int i = 0; i < totalClients; i++)
  {
    if (ntp_clients[i].clientIP == ipAdr)
    {
      ntp_clients[i].last_stamp = t;
      ntp_clients[i].syncs++;
      isIn = true;
      break;
    }
  }

  if (!isIn && (totalClients < 15))
  {
    ntp_clients[totalClients].clientIP = ipAdr;
    ntp_clients[totalClients].last_stamp = t;
    ntp_clients[totalClients].syncs = 1;
//    Serial.printf(".IP.%s - Client %03d Time: %02d.%02d.%04d  %02d:%02d:%02d  %03d \n", ntp_clients[totalClients].clientIP.toString().c_str(), totalClients, ti->tm_mday, ti->tm_mon, ti->tm_year + 1900, ti->tm_hour, ti->tm_min, ti->tm_sec, millisec);

    totalClients++;
  }
  //else
  //  Serial.printf(" IP %s - Client %03d Time: %02d.%02d.%04d  %02d:%02d:%02d  %03d \n", ip.c_str(), totalClients, ti->tm_mday, ti->tm_mon, ti->tm_year + 1900, ti->tm_hour, ti->tm_min, ti->tm_sec, millisec);

  // Serial.print(ESP.getFreeHeap());
}