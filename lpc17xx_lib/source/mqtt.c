#include "uip/uip.h"
#include "uip/ethernet.h"
#include "uip/uiptimer.h"
#include "memb.h"
#include "GPIO.h"
#include "timer.h"
#include <stdio.h>
#include <string.h>
#include "mqtt.h"
#include "opentherm.h"
#include "leds.h"

MEMB(publbuf, struct mqtt_pubmessage, MQTT_NUMLINES);

#define STATE_DISCONNECTED 0
#define STATE_CONNECTED 1

static struct mqtt_state s;
struct timer ping_timer;

//---------------------------------------------------------------------------
char* addChar(char* p, char v) {
  char* q = (char*) p;
  *q++ = v;
  return (char*) q;
}
//------------------------------------------------------------------------------
char* addUTF8(char* p, char* buf)
{
  char pos = 2;
  char* bufp = buf;
  char* q = (char*) p;

  while (*bufp) q[pos++] = *bufp++;
  q[0] = 0;
  q[1] = pos-2;

  return (char *)q+pos;
}
//------------------------------------------------------------------------------
char* addStr(char* p, char* buf)
{
  int pos = 0;
  char* bufp = buf;
  char* q = (char*) p;

  while (*bufp) q[pos++] = *bufp++;

  return (char *)q+pos;
}
//---------------------------------------------------------------------------
static char * alloc_msg(void)
{
  return memb_alloc(&publbuf);
}
//---------------------------------------------------------------------------
static void dealloc_msg(struct mqtt_pubmessage *msg)
{
  memb_free(&publbuf, msg);
}
//---------------------------------------------------------------------------
static void sendmsg(struct mqtt_pubmessage *msg)
{
  static unsigned int i;

  for(i = 0; i < MQTT_NUMLINES; ++i)
  {
    if(s.msgs[i] == NULL) {
      s.msgs[i] = msg;
      break;
    }
  }

  if(i == MQTT_NUMLINES) dealloc_msg(msg);

}
//---------------------------------------------------------------------------
void mqtt_init(void)
{
  s.connstate = STATE_DISCONNECTED;
  s.mqttstate = CONNECTING;
  s.msgid = 0;

  OT_init();

  uip_ipaddr_t ipaddr;  /* local IP address */
  uip_ipaddr(ipaddr,HISIP_1,HISIP_2,HISIP_3,HISIP_4);
  uip_connect(&ipaddr, HTONS(HISPORT));

  GPIOSetDir(1, 18, OUTPUT);
  GPIOSetDir(1, 19, OUTPUT);
  GPIOSetDir(1, 24, OUTPUT);
  GPIOSetDir(1, 25, OUTPUT);
  GPIOSetDir(0, 26, OUTPUT);

  UARTLED_OFF;
  APPLED_OFF;
  CONNLED_OFF;
  LED9_OFF;

  memb_init(&publbuf);
}
//---------------------------------------------------------------------------
static void acked(void)
{
  static unsigned int i;

  while(s.numsent > 0) {
    dealloc_msg(s.msgs[0]);
    for(i = 1; i < MQTT_NUMLINES; ++i) {
      s.msgs[i - 1] = s.msgs[i];
    }
    s.msgs[MQTT_NUMLINES - 1] = NULL;
    --s.numsent;
  }
}
//---------------------------------------------------------------------------
static void senddata(void)
{
  static char *bufptr;
  struct mqtt_pubmessage *msgptr;
  static int buflen, msglen;

  bufptr = uip_appdata;
  buflen = 0;
  for(s.numsent = 0; s.numsent < MQTT_NUMLINES && s.msgs[s.numsent] != NULL ; ++s.numsent)
  {
    msgptr = s.msgs[s.numsent];
    msglen = (* msgptr).len;
    if(msglen > MQTT_MAX_PACKET_SIZE) msglen = MQTT_MAX_PACKET_SIZE;
    if(buflen + msglen < uip_mss())
    {
      memcpy(bufptr, (* msgptr).msg, msglen);
      bufptr += msglen;
      buflen += msglen;
    }
    else break;
  }
  uip_send(uip_appdata, buflen);
}
//---------------------------------------------------------------------------
static void cleanup(void)
{
  static unsigned int i;

  for(i = 0; i < MQTT_NUMLINES; ++i) {
    if(s.msgs[i] != NULL) {
      dealloc_msg(s.msgs[i]);
    }
  }
}
//---------------------------------------------------------------------------
uint16_t rl(char* data) {
   uint16_t i = 1;
   uint8_t multiplier = 1;
   uint16_t length = 0;
   uint8_t digit;
   do {
      digit = data[i++];
      length += (digit & 127) * multiplier;
      multiplier *= 128;
   } while ((digit & 128) != 0);
   return length;
}
//---------------------------------------------------------------------------
static void newdata(void)
{
	u16_t len;
  u8_t c;
  char *dataptr;

  len = uip_datalen();
  dataptr = (char *)uip_appdata;

  if(len > 0) {
    switch(s.mqttstate){
    case CONNECTING:
      if((((dataptr[0]&0xf0)>>4) == MQTT_CONNACK)
      &&(dataptr[3]==0))
      {
      	s.mqttstate=CONNECTED;
      }
      break;

    case CONNECTED:
    	s.mqttstate=SUBSCRIBING;
    	mqtt_subscribe(CPF"#");
    	break;

    case SUBSCRIBING:
      if(((dataptr[0]&0xf0)>>4) == MQTT_SUBACK)
      {
      	s.mqttstate=ACTIVE;
      }
      break;

    case ACTIVE:
    	switch(((dataptr[0]&0xf0)>>4)){
    	case MQTT_PUBLISH:
    		if(mqtt_appcallback){
    			uint16_t tl=(dataptr[2]<<8)+dataptr[3];
    			uint32_t pl=rl(dataptr)-2-tl;
          char topic[tl+1];
          char payload[pl+1];
          uint16_t i=0;
          for (i=0;i<tl;i++) {
             topic[i] = dataptr[4+i];
          }
          topic[tl] = 0;
          for (i=0;i<pl;i++) {
             payload[i] = dataptr[4+tl+i];
          }
          payload[pl] = 0;
          mqtt_appcallback(topic, payload, pl);
    		}
    		break;
    	}
    	break;
    }
  }
}
//------------------------------------------------------------------------------
uint8_t write(uint8_t fh, uint8_t* vh, uint16_t length)
{
  static char *bufptr;
  uint8_t rl[4];
  uint8_t rllen = 0;
  uint8_t digit;
  uint8_t pos = 0;
  uint8_t len = length;
  uint8_t i = 0;

  do {
     digit = len % 128;
     len = len / 128;
     if (len > 0) {
        digit |= 0x80;
     }
     rl[pos++] = digit;
     rllen++;
  } while(len>0);

  bufptr = uip_appdata;

  pos = 0;
  bufptr[pos] = fh;
  pos++;

  if(rllen>0)
  {
    for(i=0;i<rllen;i++)
    {
      bufptr[pos] = rl[i];
      pos++;
    }
  }
  memcpy(bufptr+pos, vh, length);

  uip_send(uip_appdata, pos+length);
  return 0;
}

//---------------------------------------------------------------------------
void mqtt_appcall(void)
{

  APPLED_ON;
  delayMs(0,1);
  APPLED_OFF;

  static unsigned int i;

  //s = uip_conn->appstate;

  if(uip_connected())
  {
  	CONNLED_ON;
  	s.connstate = STATE_CONNECTED;
  	s.mqttstate = CONNECTING;
    timer_set(&ping_timer, CLOCK_SECOND * (MQTT_KEEPALIVE / 3));  /* 10s */

    for(i = 0; i < MQTT_NUMLINES; ++i) s.msgs[i] = NULL;
    mqtt_connect(MQTT_CLIENTID, "", "", "lwt", 0, 0, MQTT_CLIENTID" died");
  }

  if(uip_closed() || uip_aborted() || uip_timedout())
  {
  	CONNLED_OFF;
    s.connstate = STATE_DISCONNECTED;
    cleanup();
    uip_close();

    return;
  }

  if(uip_acked())
  {
    acked();
  }

  if(uip_newdata())
  {
    newdata();
  }

  if(uip_poll()&&timer_expired(&ping_timer))
  {
    timer_reset(&ping_timer);
    mqtt_ping();
  }
  else
  {
    if(uip_rexmit() || uip_newdata() || uip_acked() || uip_connected() || uip_poll())
    {
       senddata();
    }
  }
}
//---------------------------------------------------------------------------
uint8_t fixedheader(mqtt_msg_t type, uint8_t dup, uint8_t qos, uint8_t retain)
{
  /* Fixed Header
    bit    |7 6 5 4     | |3       | |2 1      |  |  0   |
    byte 1 |Message Type| |DUP flag| |QoS level|  |RETAIN|
  */
  return (type << 4) | (dup << 3) | (qos << 1) | retain;
}   
//------------------------------------------------------------------------------
uint8_t mqtt_connect(char *clientid, char* user, char* pass, char* willTopic, uint8_t willQoS, uint8_t willRetain, char* willMessage)
{
  //  example CONNECT message
  //  10 2E 00 06 4D 51 49 73 64 70 03 06 00 28 00 0A  |....MQIsdp...(..
  //  73 6D 61 72 74 6D 65 74 65 72 00 03 6C 77 74 00  |smartmeter..lwt.
  //  0F 73 6D 61 72 74 6D 65 74 65 72 20 64 69 65 64  |.smartmeter died

  char vh[] = {0x00,0x06,'M','Q','I','s','d','p',0x03};
  int i;

  char* p = s.buf;

  // variable header
  for (i=0;i<sizeof(vh);i++) p = addChar(p, vh[i]);
  p = addChar(p,(willQoS<<3)|(willRetain<<5)|(1<<2)|(1<<1));
  p = addChar(p,MQTT_KEEPALIVE >> 8);
  p = addChar(p,MQTT_KEEPALIVE & 0xff);
  p = addUTF8(p, clientid);
  p = addUTF8(p, willTopic);
  p = addUTF8(p, willMessage);

  s.len = p-s.buf;
  write(fixedheader(MQTT_CONNECT,0,0,0), s.buf, s.len);

  return 0;
}
//------------------------------------------------------------------------------
void mqtt_ping(void)
{
  if(s.connstate == STATE_CONNECTED)
  {
    s.len = 0;
    s.buf[0]=0;
    s.buf[1]=0;
    write(fixedheader(MQTT_PINGREQ,0,0,0), s.buf, s.len);
  }
}
//------------------------------------------------------------------------------
void mqtt_publish(char *topic, char *payload, size_t pllen, uint8_t retained)
{
  // TODO:inc msgid 1..ffff (for QoS > 0, later...)
  if(s.connstate == STATE_CONNECTED)
  {
    uint8_t rl[4];
    uint8_t rllen = 0;
    uint8_t digit;
    uint8_t pos = 0;
    size_t len;
    uint8_t i = 0;
    static struct mqtt_pubmessage *line;

    char* p = s.buf;

    len = strlen(topic)+2+pllen;
    do {
       digit = len % 128;
       len = len / 128;
       if (len > 0) digit |= 0x80;
       rl[pos++] = digit;
       rllen++;
    } while(len>0);

    p = addChar(p, fixedheader(MQTT_PUBLISH,0,0,0));
    for(i=0;i<rllen;i++) p = addChar(p, rl[i]);
    p = addUTF8(p, topic);
    p = addStr(p, payload);
    s.len = p-s.buf;

    line = alloc_msg();
    if(line != NULL) {
      (* line).len = s.len;
      memcpy((* line).msg, s.buf, s.len);
      sendmsg(line);
    }
  }
}
//------------------------------------------------------------------------------
// must be called from mqtt_appcall
void mqtt_subscribe(char *topic)
{
  if(s.connstate == STATE_CONNECTED)
  {
  	//static struct mqtt_pubmessage *line;
  	char* p = s.buf;
    s.msgid++;

    p = addChar(p, s.msgid>>8);
    p = addChar(p, s.msgid&0xff);
    p = addUTF8(p, topic);
    p = addChar(p, 0);

    s.len = p-s.buf;
    write(fixedheader(MQTT_SUBSCRIBE,0,0,0), s.buf, s.len);
  }
}
//------------------------------------------------------------------------------
// will probably never be used..
void mqtt_disconnect()
{
  if(s.connstate == STATE_CONNECTED)
  {
    s.buf[0]=0;
    s.len=1;
    write(fixedheader(MQTT_DISCONNECT,0,0,0), s.buf, s.len);
  }
}
