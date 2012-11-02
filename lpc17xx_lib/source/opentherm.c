#include "lpc_types.h"
#include "leds.h"
#include "mqtt.h"
#include "opentherm.h"
//#include <string.h>
//#include <stdlib.h>
//#include <stdio.h>

#define OTGW_LINELEN 12      //
typedef struct {
  uint8_t len;
  char line[OTGW_LINELEN];
} gw_state;

gw_state gwdata;

// commands from master to slave
#define MSG_READ_DATA					0 //000
#define MSG_WRITE_DATA				1	//001
#define MSG_INVALID_DATA			2	//010
#define MSG_RESERVED					3	//011

// responses from slave to master
#define MSG_READ_ACK					4	//100
#define MSG_WRITE_ACK					5	//101
#define MSG_DATA_INVALID			6	//110
#define MSG_UNKNOWN_DATAID		7	//111

typedef struct{
	uint8_t byte[4];
} OTdata;

typedef struct{
	unsigned long parity:1;
	unsigned long msgtype:3;
	unsigned long spare:4;
	unsigned long dataid:8;
	unsigned long data1:8;
	unsigned long data2:8;
} OTframe;

typedef union {
	OTdata data;
	//OTframe frame;
} OTFrameInfo;


OTFrameInfo gwframe;
OTFrameInfo history[256];


//typedef enum {ReadData, WriteData, InvalidData, reserverd, ReadAck, WriteAck, DataInvalid, UnknowID} MessageType;
//typedef enum {MtoS, StoM} DirectionType;
typedef enum {Read, Write, Both} CommandType;
typedef enum {both, highbyte, lowbyte} ByteType;
typedef enum {flag8, u8, s8, f8_8, u16, s16, dowtod} PayloadType;

typedef struct{
	uint8_t ID;
	CommandType rw;
	ByteType whichbyte;
	PayloadType format;
	uint8_t bitpos;
	char* topic;
} OTInformation;

OTInformation OTInfos[] = {
		{0x00,	Read, 	highbyte,	flag8,	0,	TPF"ch_enable"},
		{0x00,	Read, 	highbyte, flag8, 	1,  TPF"dhw_enable"},
		{0x00,	Read, 	highbyte, flag8, 	2,  TPF"cooling_enabled"},
		{0x00,	Read, 	highbyte, flag8, 	3,  TPF"otc_active"},
		{0x00, 	Read, 	highbyte, flag8, 	4,  TPF"ch2_enable"},
		{0x00, 	Read, 	highbyte, flag8, 	5,  ""},
		{0x00, 	Read,		highbyte, flag8, 	6,  ""},
		{0x00, 	Read, 	highbyte, flag8, 	7,  ""},
		{0x00, 	Read, 	lowbyte,  flag8, 	0,  TPF"fault"},
		{0x00, 	Read, 	lowbyte,  flag8, 	1,  TPF"ch_mode"},
		{0x00, 	Read, 	lowbyte,  flag8, 	2,  TPF"dhw_mode"},
		{0x00, 	Read, 	lowbyte,  flag8, 	3,  TPF"flame"},
		{0x00, 	Read, 	lowbyte,  flag8, 	4,  TPF"cooling"},
		{0x00, 	Read, 	lowbyte,  flag8, 	5,  TPF"ch2E"},
		{0x00, 	Read, 	lowbyte,  flag8, 	6,  TPF"diag"},
		{0x00, 	Read, 	lowbyte,  flag8, 	7,  ""},
		{0x01, 	Write,	both,  		f8_8, 	0,  TPF"controlsetpoint"},
		{0x02, 	Write, 	highbyte, flag8, 	0,  "x"},
		{0x02, 	Write, 	highbyte, flag8, 	1,  "x"},
		{0x02, 	Write, 	highbyte, flag8, 	2,  "x"},
		{0x02, 	Write, 	highbyte, flag8, 	3,  "x"},
		{0x02, 	Write, 	highbyte, flag8, 	4,  "x"},
		{0x02, 	Write, 	highbyte, flag8, 	5,  "x"},
		{0x02, 	Write, 	highbyte, flag8, 	6,  "x"},
		{0x02, 	Write, 	highbyte, flag8,	7,  "x"},
		{0x02, 	Write, 	lowbyte, 	u8, 		0,  TPF"mastermemberid"},
		{0x03, 	Read, 	highbyte, flag8, 	0,  TPF"dhwpresent"},
		{0x03, 	Read, 	highbyte, flag8, 	1,  TPF"controltype"},
		{0x03, 	Read, 	highbyte, flag8, 	2,  TPF"coolingsupport"},
		{0x03, 	Read, 	highbyte, flag8, 	3,  TPF"dhwconfig"},
		{0x03, 	Read, 	highbyte, flag8, 	4,  TPF"masterlowoff"},
		{0x03, 	Read, 	highbyte, flag8, 	5,  TPF"ch2present"},
		{0x03, 	Read, 	highbyte, flag8, 	6,  ""},
		{0x03, 	Read, 	highbyte, flag8, 	7,  ""},
		{0x03, 	Read, 	lowbyte, 	u8, 		0,  TPF"slavememberid"},
		{0x04, 	Write, 	highbyte, u8, 		0,  TPF"commandcode"},
		{0x04, 	Read, 	lowbyte,  u8, 		0,  TPF"commandresponse"},
		{0x05, 	Read, 	highbyte, flag8, 	0,  TPF"servicerequest"},
		{0x05, 	Read, 	highbyte, flag8, 	1,  TPF"lockout-reset"},
		{0x05, 	Read, 	highbyte, flag8, 	2,  TPF"lowwaterpress"},
		{0x05, 	Read, 	highbyte, flag8, 	3,  TPF"gasflamefault"},
		{0x05, 	Read, 	highbyte, flag8, 	4,  TPF"airpressfault"},
		{0x05, 	Read, 	highbyte, flag8, 	5,  TPF"waterovtemp"},
		{0x05, 	Read, 	highbyte, flag8, 	6,  ""},
		{0x05, 	Read, 	highbyte, flag8, 	7,  ""},
		{0x05, 	Read, 	lowbyte, 	u8, 		0,  TPF"oemfaultcode"},
		{0x06, 	Read, 	lowbyte,  flag8,  0,  "xxx"},
		{0x06, 	Read, 	lowbyte,  flag8,  1,  "xxx"},
		{0x06, 	Read, 	lowbyte,  flag8,  2,  "xxx"},
		{0x06, 	Read, 	lowbyte,  flag8,  3,  "xxx"},
		{0x06, 	Read, 	lowbyte,  flag8,  4,  "xxx"},
		{0x06, 	Read, 	lowbyte,  flag8,  5,  "xxx"},
		{0x06, 	Read, 	lowbyte,  flag8,  6,  "xxx"},
		{0x06, 	Read, 	lowbyte,  flag8,  7,  "xxx"},
		{0x06, 	Read, 	highbyte, flag8,  0,  "xxx"},
		{0x06, 	Read, 	highbyte, flag8,  1,  "xxx"},
		{0x06, 	Read, 	highbyte, flag8,  2,  "xxx"},
		{0x06, 	Read, 	highbyte, flag8,  3,  "xxx"},
		{0x06, 	Read, 	highbyte, flag8,  4,  "xxx"},
		{0x06,	Read, 	highbyte, flag8,  5,  "xxx"},
		{0x06, 	Read, 	highbyte, flag8,  6,  "xxx"},
		{0x06, 	Read, 	highbyte, flag8,  7,  "xxx"},
		{0x07, 	Write, 	both,     f8_8,   0,  "xxx"},
		{0x08, 	Write, 	both, 		f8_8,  	0,  TPF"controlsetpoint2"},
		{0x09, 	Read, 	both,     f8_8, 	0,  TPF"overridesetpoint"},
		{0x0a, 	Write, 	highbyte, u8, 		0,  "xxx"},
		{0x0a, 	Write, 	lowbyte, 	u8, 		0,  "xxx"},
		{0x0b, 	Both, 	highbyte, u8, 		0,  TPF"tspindex"},
		{0x0b, 	Both, 	lowbyte,  u8, 		0,  TPF"tspvalue"},
		{0x0c, 	Read, 	highbyte, u8, 		0,  "xxx"},
		{0x0c, 	Read, 	lowbyte,  u8, 		0,  "xxx"},
		{0x0d, 	Read, 	highbyte, u8, 		0,  "xxx"},
		{0x0d, 	Read, 	lowbyte,  u8, 		0,  "xxx"},
		{0x0e, 	Read, 	lowbyte,  f8_8, 	0,  TPF"maxrelmdulevel"},
		{0x0f, 	Read, 	highbyte, u8, 		0,  TPF"maxcapkw"},
		{0x0f, 	Read, 	lowbyte,  u8, 		0,  TPF"maxcapprc"},
		{0x10, 	Write, 	both,     f8_8, 	0,  TPF"roomsetpoint"},
		{0x11, 	Read, 	both,     f8_8, 	0,  TPF"modulevel"},
		{0x12, 	Read, 	both,     f8_8, 	0,  TPF"waterpressure"},
		{0x13, 	Read, 	both,     f8_8, 	0,  TPF"dhwflow"},
		{0x14, 	Both, 	both,     dowtod,	0,  TPF"dowtod"},
		{0x15, 	Both, 	highbyte, u8, 		0,  TPF"month"},
		{0x15, 	Both, 	lowbyte,  u8, 		0,  TPF"dom"},
		{0x16, 	Both, 	lowbyte,  u8, 		0,  TPF"year"},
		{0x17, 	Write, 	both,     f8_8, 	0,  TPF"setpointch2"},
		{0x18, 	Write, 	both,     f8_8, 	0,  TPF"roomtemp"},
		{0x19, 	Read,  	both, 		f8_8, 	0,  TPF"flowtemp"},
		{0x1a, 	Read,  	both, 		f8_8, 	0,  TPF"dhwtemp"},
		{0x1b, 	Read,  	both, 		f8_8, 	0,  TPF"outsidetemp"},
		{0x1c, 	Read,  	both, 		f8_8, 	0,  TPF"returntemp"},
		{0x1d, 	Read,  	both, 		f8_8, 	0,  TPF"solstortemp"},
		{0x1e, 	Read,  	both, 		f8_8, 	0,  TPF"solcolltemp"},
		{0x1f, 	Read,  	both, 		f8_8, 	0,  TPF"flowtemp2"},
		{0x20, 	Read,  	both, 		f8_8, 	0,  TPF"dhw2temp"},
		{0x21, 	Read,  	both, 		s16, 		0,  TPF"exhausttemp"},
		{0x30, 	Read, 	highbyte, s8, 		0, 	TPF"tdhwsetu"},
		{0x30, 	Read, 	lowbyte, 	s8, 		0,  TPF"tdhwsetl"},
		{0x31, 	Read, 	highbyte, s8, 		0, 	TPF"maxchu"},
		{0x31, 	Read, 	lowbyte, 	s8, 		0,  TPF"maxchl"},
		{0x32, 	Read, 	highbyte, s8, 		0, 	TPF"otcu"},
		{0x32, 	Read, 	lowbyte, 	s8, 		0,  TPF"otcl"},
		{0x38, 	Both, 	both, 		f8_8, 	0, 	TPF"tdhwsetu"},  // check!!
		{0x38, 	Both, 	both, 		f8_8, 	0,  TPF"tdhwsetl"},
		{0x39, 	Both, 	both, 		f8_8, 	0, 	TPF"tchmax"},
		{0x39, 	Both, 	both, 		f8_8, 	0,  TPF"tchmax"},  // check!!
		{0x3a, 	Both, 	both, 		f8_8, 	0, 	TPF"otc"},
		{0x3a, 	Both, 	both, 		f8_8, 	0,  TPF"otc"},
		{0x64, 	Read, 	highbyte, flag8, 	0,  TPF"remote override 0func"},
		{0x64, 	Read, 	highbyte, flag8, 	1,  TPF"remote override 1func"},
		{0x64, 	Read, 	highbyte, flag8, 	2,  TPF"remote override 2func"},
		{0x64, 	Read, 	highbyte, flag8, 	3,  TPF"remote override 3func"},
		{0x64, 	Read, 	highbyte, flag8, 	4,  TPF"remote override 4func"},
		{0x64, 	Read, 	highbyte, flag8, 	5,  TPF"remote override 5func"},
		{0x64, 	Read, 	highbyte, flag8, 	6,  TPF"remote override 6func"},
		{0x64, 	Read, 	highbyte, flag8, 	7,  TPF"remote override 7func"},
		{0x73, 	Read, 	both, 		u16, 		0,  TPF"oemdiagcode"}, //check!!!
		{0x74, 	Read,  	both, 		u16, 		0,  TPF"burnerstarts"},
		{0x75, 	Read,  	both, 		u16, 		0,  TPF"chpumpstarts"},
		{0x76, 	Read,  	both, 		u16, 		0,  TPF"dhwpvstarts"},
		{0x77, 	Read,  	both, 		u16, 		0,  TPF"dhwburnerstarts"},
		{0x78, 	Read,  	both, 		u16, 		0,  TPF"burnerhours"},
		{0x79, 	Read,  	both, 		u16, 		0,  TPF"chpumphours"},
		{0x7a, 	Read,  	both, 		u16, 		0,  TPF"dhwpvhours"},
		{0x7b, 	Read,  	both, 		u16, 		0,  TPF"dhwburnerhours"},
		{0x7c, 	Write, 	both, 		f8_8, 	0,  TPF"masterotversion"},
		{0x7d, 	Read, 	both, 		f8_8, 	0,  TPF"slaveotversion"},
		{0x7e, 	Write, 	highbyte, u8, 		0,  TPF"masterproducttype"},
		{0x7e, 	Write, 	lowbyte, 	u8, 		0,  TPF"masterproductversion"},
		{0x7f, 	Read, 	highbyte, u8, 		0,  TPF"slaveproducttype"},
		{0x7f, 	Read, 	lowbyte, 	u8, 		0,  TPF"slaveproductversion"}
};

typedef struct{
	uint8_t sent;
	uint16_t value;
} OTHistory;

OTHistory IDHist[sizeof(OTInfos)/sizeof(OTInformation)];

static Response responselist[] = {
  {"OK"},
  {"NG"},
  {"SE"},
  {"BV"},
  {"OR"},
  {"NS"},
  {"NF"},
  {"IP"},
  {"PF"},
  {"Error 01"},
  {"Error 02"},
  {"Error 03"},
  {"Error 04"}
};

static Command commandlist[] = {
  {CPF"OT"},
  {CPF"TT"},
  {CPF"TC"},
  {CPF"TR"},
  {CPF"SH"},
  {CPF"SW"},
  {CPF"SC"},
  {CPF"HW"},
  {CPF"AA"},
  {CPF"DA"},
  {CPF"LA"},
  {CPF"LB"},
  {CPF"LC"},
  {CPF"LD"},
  {CPF"PR"},
  {CPF"VR"},
  {CPF"GW"},
  {CPF"PS"},
  {CPF"IT"},
  {CPF"RS"},
  {CPF"DP"},
  {CPF"AA"}
};


//typedef enum { ID, STD, LINE17, LINE18, EXCLMARK } MatchType;
//typedef struct {
//  MatchType type;
//  dataid key;
//  char* topic;
//  int start;
//  int width;
//} Match;

//Match matchlist[] = {
//  {ID, "/ISk5\\", "", 0, 0},
//  {STD, "1-0:1.8.1", TPF"/powerusage1", 10, 9},
//  {STD, "1-0:1.8.2", TPF"/powerusage2", 10, 9},
//  {STD, "1-0:2.8.1", TPF"/powerdeliv1", 10, 9},
//  {STD, "1-0:2.8.2", TPF"/powerdeliv2", 10, 9},
//  {STD, "0-0:96.14.0", TPF"/tariff", 12, 4},
//  {STD, "1-0:1.7.0", TPF"/powerusagec", 10, 7},
//  {STD, "1-0:2.7.0", TPF"/powerdelivc", 10, 7},
//  {LINE17, "1-0:2.7.0", TPF"/gastimestamp", 11, 12},
//  {LINE18, "(", TPF"/gasusage", 1, 9},
//  {EXCLMARK,"!","", 0, 0}
//};


//---------------------------------------------------------------------------
int isMasterframe(OTFrameInfo f)
{
	return (((f.data.byte[0]&0x70)>>4)<MSG_READ_ACK);
}
//---------------------------------------------------------------------------
int isReadframe(OTFrameInfo f)
{
	return (((f.data.byte[0]&0x70)>>4)==MSG_READ_ACK)?1:0;
}
//---------------------------------------------------------------------------
int isWriteframe(OTFrameInfo f)
{
	return (((f.data.byte[0]&0x70)>>4)==MSG_WRITE_DATA)?1:0;
}
//---------------------------------------------------------------------------
void send(char *str1, char *str2)
{
  mqtt_publish(str1, str2, strlen(str2), 1 /* (retain) */);
}
//---------------------------------------------------------------------------
int validframechar(char c)
{
	return ((c>='0') && (c<='9'))|((c>='A') && (c<='F'));
}
//---------------------------------------------------------------------------
uint8_t hextoint(char c){
	if((c>='0') && (c<='9')){
		return (c-0x30);
	}	else {
		return (c-0x37);
	}
}
//---------------------------------------------------------------------------
uint8_t valueChanged(uint8_t index, uint16_t value)
{
	if((IDHist[index].sent!=1)||(IDHist[index].value!=value)){
		IDHist[index].sent=1;
		IDHist[index].value=value;
		return 1;
	}
	return 0;
}
//---------------------------------------------------------------------------
void parseline()
{
  uint8_t validhex;
	uint8_t i;

	//if(gwdata.len<8) return;

	uint8_t valid = 0;

	// check on responses/error codes and forward those.
  Response t;
  for(i=0;(i<sizeof(responselist)/sizeof(Response));i++){
    t = responselist[i];
    if(strncmp(t.response, gwdata.line, strlen(t.response)) == 0){
    	send(TPF"response", gwdata.line);
    	return;
    }
  }

	// filter on B(oiler),T(hermostat),A(nswer),R(equest)
	if(!((gwdata.line[0]=='B')|(gwdata.line[0]=='T')|(gwdata.line[0]=='A')|(gwdata.line[0]=='R')|(gwdata.line[0]=='E'))){
  	send(TPF"unknown", gwdata.line);
  	return;
	}

	// from here the line is assumed to be an OT frame.
	validhex=1;
	for(i=1;i<gwdata.len;i++){
		validhex=validhex&&(((gwdata.line[i]>='0') && (gwdata.line[i]<='9'))|((gwdata.line[i]>='A') && (gwdata.line[i]<='F')));
    if(!validhex) break;
	}

	if(validhex){
		// valid frame.
		//send(TPF"raw", gwdata.line);
		OTFrameInfo f;
		f.data.byte[0] = (hextoint(gwdata.line[1])<<4) + hextoint(gwdata.line[2]);
		f.data.byte[1] = (hextoint(gwdata.line[3])<<4) + hextoint(gwdata.line[4]);
		f.data.byte[2] = (hextoint(gwdata.line[5])<<4) + hextoint(gwdata.line[6]);
		f.data.byte[3] = (hextoint(gwdata.line[7])<<4) + hextoint(gwdata.line[8]);

		uint8_t dataid = f.data.byte[1];
		char buf[100]="";
		char* p = buf;
		int found=0;
		// assume OTInfos is sorted on Data ID
		for(i=0; (i < (sizeof(OTInfos)/sizeof(OTInformation)))&&(OTInfos[i].ID<=dataid);i++){
			if((OTInfos[i].ID==dataid)&&(OTInfos[i].topic!="")){
				found=1;
				p=buf;
				memset(buf, 0, sizeof(buf));
				OTInformation info = OTInfos[i];
				if((info.rw==Both)||((info.rw==Write)&&(isWriteframe(f)))||((info.rw==Read)&&(isReadframe(f)))){
					uint8_t ui8;
					int8_t i8;
					uint16_t ui16;
					int16_t i16;
					char valuebuf[10]="";
					char* payload = valuebuf;
					memset(valuebuf, 0, sizeof(valuebuf));
					switch (info.format){
					case flag8:
						ui8 = (f.data.byte[2+(info.whichbyte==highbyte?0:1)]&(1<<info.bitpos))!=0?1:0;
						p=addChar(p,0x30+ui8);
						PutUnsignedInt(payload,'0',1, ui8);
						if(valueChanged(i, ui8)) send(info.topic, payload);
						break;
					case u8: //unsigned 8-bit integer 0 .. 255
						ui8 = f.data.byte[2+(info.whichbyte==highbyte?0:1)];
						PutUnsignedInt(payload,'0',3,ui8);
						if(valueChanged(i, ui8)) send(info.topic, payload);
						break;
					case s8: //signed 8-bit integer -128 .. 127 (two’s compliment)
						i8 = f.data.byte[2+(info.whichbyte==highbyte?0:1)];
						PutSignedInt(payload,'0',3,i8);
						if(valueChanged(i, i8)) send(info.topic, payload);
						break;
					case f8_8:
						payload=payload+PutUnsignedInt(payload, '0', 0, f.data.byte[2]);
						payload=payload+PutChar(payload, '.');
						PutUnsignedInt(payload, '0', 2, (int)((f.data.byte[3]*100)/256));
						if(valueChanged(i, (f.data.byte[2]<<8)|f.data.byte[3])) send(info.topic, valuebuf);
  					break;
					case u16: //unsigned 16-bit integer 0..65535
						ui16 = (f.data.byte[2]<<8)|(f.data.byte[3]);
						PutUnsignedInt(payload,'0',3, ui16);
						if(valueChanged(i, ui16)) send(info.topic, payload);
						break;
					case s16: //signed 16-bit integer -32768..32767
						i16 = (f.data.byte[2]<<8)|(f.data.byte[3]);
						PutSignedInt(payload,'0',0, i16);
						if(valueChanged(i, i16)) send(info.topic, payload);
						break;
					}
				}
			}
		}
		if(found==0) send(TPF"unknown", gwdata.line);
	} else send(TPF"invalid", gwdata.line);
}
//---------------------------------------------------------------------------
// character received will be stored in linebuffer, which will be
// processed when a line-feed (0x0a) is encountered.
void gw_char(uint8_t c)
{
  if(c == 0x0d) return;
  gwdata.line[gwdata.len] = c;
  if(gwdata.line[gwdata.len] == 0x0a || gwdata.len == sizeof(gwdata.line) - 1){
    // discard newline, close string, parse line and clear it.
    if(gwdata.len > 0) gwdata.line[gwdata.len] = 0;
    parseline();
    gwdata.len = 0;
  }else{
    ++gwdata.len;
  }
}
//---------------------------------------------------------------------------
void OT_callback(char* topic, char* payload,int length) {
  // handle message arrived

//	if(strncmp(CPF, topic, strlen(CPF)) == 0){
//		LED9_ON;
		// check on valid command and forward those to the gateway
		uint8_t i;
	  Command t;
	  for(i=0;(i<sizeof(commandlist)/sizeof(Command));i++){
	    t = commandlist[i];
	    if(strncmp(t.cmd, topic, strlen(t.cmd)) == 0){
	    	LED9_ON;
	    	send(TPF"cmd", payload);
	    	return;
	    }
	  }
//	}
}
//---------------------------------------------------------------------------
void OT_init(void)
{
	mqtt_appcallback = OT_callback;

	int i;
	for(i=0; i < (sizeof(IDHist)/sizeof(OTHistory));i++){
		IDHist[i].sent=0;
	}
}
