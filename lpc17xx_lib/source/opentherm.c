#include "lpc_types.h"
#include "leds.h"
#include "mqtt.h"
#include "opentherm.h"
#include "uart.h"

#define OTGW_LINELEN 11      //
typedef struct {
  uint8_t len;
  // B40192CC7\r\n = 11
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

typedef union {
	OTdata data;
} OTFrameInfo;

typedef enum {Read, Write, Both} CommandType;
typedef enum {both, highbyte, lowbyte} ByteType;
typedef enum {flag8, u8, s8, f8_8, u16, s16, dowtod} PayloadType;

typedef struct{
	uint8_t ID;
	CommandType rw;
	ByteType whichbyte;
	PayloadType format;
	uint8_t bitpos;
	char *topic;
} OTInformation;

OTInformation OTInfos[] = {
		{0x00,	Read, 	highbyte,	flag8,	0,	"ch_enable"},
		{0x00,	Read, 	highbyte, flag8, 	1,  "dhw_enable"},
		{0x00,	Read, 	highbyte, flag8, 	2,  "cooling_enabled"},
		{0x00,	Read, 	highbyte, flag8, 	3,  "otc_active"},
		{0x00, 	Read, 	highbyte, flag8, 	4,  "ch2_enable"},
		{0x00, 	Read, 	highbyte, flag8, 	5,  "0x00:5"},
		{0x00, 	Read,		highbyte, flag8, 	6,  "0x00:6"},
		{0x00, 	Read, 	highbyte, flag8, 	7,  "0x00:7"},
		{0x00, 	Read, 	lowbyte,  flag8, 	0,  "fault"},
		{0x00, 	Read, 	lowbyte,  flag8, 	1,  "ch_mode"},
		{0x00, 	Read, 	lowbyte,  flag8, 	2,  "dhw_mode"},
		{0x00, 	Read, 	lowbyte,  flag8, 	3,  "flame"},
		{0x00, 	Read, 	lowbyte,  flag8, 	4,  "cooling"},
		{0x00, 	Read, 	lowbyte,  flag8, 	5,  "ch2E"},
		{0x00, 	Read, 	lowbyte,  flag8, 	6,  "diag"},
		{0x00, 	Read, 	lowbyte,  flag8, 	7,  "0x00:7"},
		{0x01, 	Write,	both,  		f8_8, 	0,  "controlsetpoint"},
		{0x02, 	Write, 	highbyte, flag8, 	0,  "0x02:0"},
		{0x02, 	Write, 	highbyte, flag8, 	1,  "0x02:1"},
		{0x02, 	Write, 	highbyte, flag8, 	2,  "0x02:2"},
		{0x02, 	Write, 	highbyte, flag8, 	3,  "0x02:3"},
		{0x02, 	Write, 	highbyte, flag8, 	4,  "0x02:4"},
		{0x02, 	Write, 	highbyte, flag8, 	5,  "0x02:5"},
		{0x02, 	Write, 	highbyte, flag8, 	6,  "0x02:6"},
		{0x02, 	Write, 	highbyte, flag8,	7,  "0x02:7"},
		{0x02, 	Write, 	lowbyte, 	u8, 		0,  "mastermemberid"},
		{0x03, 	Read, 	highbyte, flag8, 	0,  "dhwpresent"},
		{0x03, 	Read, 	highbyte, flag8, 	1,  "controltype"},
		{0x03, 	Read, 	highbyte, flag8, 	2,  "coolingsupport"},
		{0x03, 	Read, 	highbyte, flag8, 	3,  "dhwconfig"},
		{0x03, 	Read, 	highbyte, flag8, 	4,  "masterlowoff"},
		{0x03, 	Read, 	highbyte, flag8, 	5,  "ch2present"},
		{0x03, 	Read, 	highbyte, flag8, 	6,  "0x03:6"},
		{0x03, 	Read, 	highbyte, flag8, 	7,  "0x03:7"},
		{0x03, 	Read, 	lowbyte, 	u8, 		0,  "slavememberid"},
		{0x04, 	Write, 	highbyte, u8, 		0,  "commandcode"},
		{0x04, 	Read, 	lowbyte,  u8, 		0,  "commandresponse"},
		{0x05, 	Read, 	highbyte, flag8, 	0,  "servicerequest"},
		{0x05, 	Read, 	highbyte, flag8, 	1,  "lockout-reset"},
		{0x05, 	Read, 	highbyte, flag8, 	2,  "lowwaterpress"},
		{0x05, 	Read, 	highbyte, flag8, 	3,  "gasflamefault"},
		{0x05, 	Read, 	highbyte, flag8, 	4,  "airpressfault"},
		{0x05, 	Read, 	highbyte, flag8, 	5,  "waterovtemp"},
		{0x05, 	Read, 	highbyte, flag8, 	6,  "0x05:6"},
		{0x05, 	Read, 	highbyte, flag8, 	7,  "0x05:7"},
		{0x05, 	Read, 	lowbyte, 	u8, 		0,  "oemfaultcode"},
		{0x06, 	Read, 	lowbyte,  flag8,  0,  "0x06:l0"},
		{0x06, 	Read, 	lowbyte,  flag8,  1,  "0x06:l1"},
		{0x06, 	Read, 	lowbyte,  flag8,  2,  "0x06:l2"},
		{0x06, 	Read, 	lowbyte,  flag8,  3,  "0x06:l3"},
		{0x06, 	Read, 	lowbyte,  flag8,  4,  "0x06:l4"},
		{0x06, 	Read, 	lowbyte,  flag8,  5,  "0x06:l5"},
		{0x06, 	Read, 	lowbyte,  flag8,  6,  "0x06:l6"},
		{0x06, 	Read, 	lowbyte,  flag8,  7,  "0x06:l7"},
		{0x06, 	Read, 	highbyte, flag8,  0,  "0x06:h0"},
		{0x06, 	Read, 	highbyte, flag8,  1,  "0x06:h1"},
		{0x06, 	Read, 	highbyte, flag8,  2,  "0x06:h2"},
		{0x06, 	Read, 	highbyte, flag8,  3,  "0x06:h3"},
		{0x06, 	Read, 	highbyte, flag8,  4,  "0x06:h4"},
		{0x06,	Read, 	highbyte, flag8,  5,  "0x06:h5"},
		{0x06, 	Read, 	highbyte, flag8,  6,  "0x06:h6"},
		{0x06, 	Read, 	highbyte, flag8,  7,  "0x06:h7"},
		{0x07, 	Write, 	both,     f8_8,   0,  "0x07"},
		{0x08, 	Write, 	both, 		f8_8,  	0,  "controlsetpoint2"},
		{0x09, 	Read, 	both,     f8_8, 	0,  "overridesetpoint"},
		{0x0a, 	Write, 	highbyte, u8, 		0,  "0x0a:h"},
		{0x0a, 	Write, 	lowbyte, 	u8, 		0,  "0x0a:l"},
		{0x0b, 	Both, 	highbyte, u8, 		0,  "tspindex"},
		{0x0b, 	Both, 	lowbyte,  u8, 		0,  "tspvalue"},
		{0x0c, 	Read, 	highbyte, u8, 		0,  "0x0c:h"},
		{0x0c, 	Read, 	lowbyte,  u8, 		0,  "0x0c:l"},
		{0x0d, 	Read, 	highbyte, u8, 		0,  "0x0d:h"},
		{0x0d, 	Read, 	lowbyte,  u8, 		0,  "0x0d:l"},
		{0x0e, 	Read, 	lowbyte,  f8_8, 	0,  "maxrelmdulevel"},
		{0x0f, 	Read, 	highbyte, u8, 		0,  "maxcapkw"},
		{0x0f, 	Read, 	lowbyte,  u8, 		0,  "maxcapprc"},
		{0x10, 	Write, 	both,     f8_8, 	0,  "roomsetpoint"},
		{0x11, 	Read, 	both,     f8_8, 	0,  "modulevel"},
		{0x12, 	Read, 	both,     f8_8, 	0,  "waterpressure"},
		{0x13, 	Read, 	both,     f8_8, 	0,  "dhwflow"},
		{0x14, 	Both, 	both,     dowtod,	0,  "dowtod"},
		{0x15, 	Both, 	highbyte, u8, 		0,  "month"},
		{0x15, 	Both, 	lowbyte,  u8, 		0,  "dom"},
		{0x16, 	Both, 	lowbyte,  u8, 		0,  "year"},
		{0x17, 	Write, 	both,     f8_8, 	0,  "setpointch2"},
		{0x18, 	Write, 	both,     f8_8, 	0,  "roomtemp"},
		{0x19, 	Read,  	both, 		f8_8, 	0,  "flowtemp"},
		{0x1a, 	Read,  	both, 		f8_8, 	0,  "dhwtemp"},
		{0x1b, 	Read,  	both, 		f8_8, 	0,  "outsidetemp"},
		{0x1c, 	Read,  	both, 		f8_8, 	0,  "returntemp"},
		{0x1d, 	Read,  	both, 		f8_8, 	0,  "solstortemp"},
		{0x1e, 	Read,  	both, 		f8_8, 	0,  "solcolltemp"},
		{0x1f, 	Read,  	both, 		f8_8, 	0,  "flowtemp2"},
		{0x20, 	Read,  	both, 		f8_8, 	0,  "dhw2temp"},
		{0x21, 	Read,  	both, 		s16, 		0,  "exhausttemp"},
		{0x30, 	Read, 	highbyte, s8, 		0, 	"tdhwsetu"},
		{0x30, 	Read, 	lowbyte, 	s8, 		0,  "tdhwsetl"},
		{0x31, 	Read, 	highbyte, s8, 		0, 	"maxchu"},
		{0x31, 	Read, 	lowbyte, 	s8, 		0,  "maxchl"},
		{0x32, 	Read, 	highbyte, s8, 		0, 	"otcu"},
		{0x32, 	Read, 	lowbyte, 	s8, 		0,  "otcl"},
		{0x38, 	Both, 	both, 		f8_8, 	0, 	"tdhwset"},
		{0x39, 	Both, 	both, 		f8_8, 	0, 	"tchmax"},
		{0x3a, 	Both, 	both, 		f8_8, 	0, 	"otchcratio"},
		{0x64, 	Read, 	highbyte, flag8, 	0,  "rof0"},
		{0x64, 	Read, 	highbyte, flag8, 	1,  "rof1"},
		{0x64, 	Read, 	highbyte, flag8, 	2,  "rof2"},
		{0x64, 	Read, 	highbyte, flag8, 	3,  "rof3"},
		{0x64, 	Read, 	highbyte, flag8, 	4,  "rof4"},
		{0x64, 	Read, 	highbyte, flag8, 	5,  "rof5"},
		{0x64, 	Read, 	highbyte, flag8, 	6,  "rof6"},
		{0x64, 	Read, 	highbyte, flag8, 	7,  "rof7"},
		{0x73, 	Read, 	both, 		u16, 		0,  "oemdiagcode"}, //check!!!
		{0x74, 	Read,  	both, 		u16, 		0,  "burnerstarts"},
		{0x75, 	Read,  	both, 		u16, 		0,  "chpumpstarts"},
		{0x76, 	Read,  	both, 		u16, 		0,  "dhwpvstarts"},
		{0x77, 	Read,  	both, 		u16, 		0,  "dhwburnerstarts"},
		{0x78, 	Read,  	both, 		u16, 		0,  "burnerhours"},
		{0x79, 	Read,  	both, 		u16, 		0,  "chpumphours"},
		{0x7a, 	Read,  	both, 		u16, 		0,  "dhwpvhours"},
		{0x7b, 	Read,  	both, 		u16, 		0,  "dhwburnerhours"},
		{0x7c, 	Write, 	both, 		f8_8, 	0,  "masterotversion"},
		{0x7d, 	Read, 	both, 		f8_8, 	0,  "slaveotversion"},
		{0x7e, 	Write, 	highbyte, u8, 		0,  "masterproducttype"},
		{0x7e, 	Write, 	lowbyte, 	u8, 		0,  "masterproductversion"},
		{0x7f, 	Read, 	highbyte, u8, 		0,  "slaveproducttype"},
		{0x7f, 	Read, 	lowbyte, 	u8, 		0,  "slaveproductversion"}
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

static Command localcommandlist[] = {
  {CPF"r"},   // raw frames
  {CPF"a"},   // all values
  {CPF"i"}    // init
};

volatile int sendraw=0;
volatile int sendall=0;

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
	char *tpf = TPF;
	size_t lenp = strlen(tpf);
	size_t lens = strlen(str1);
	char *const buf[lenp+lens+1];
	register char *p = buf;
	memcpy(p,tpf,lenp); p += lenp;
	memcpy(p,str1,lens);
	p[lens] = 0;
  mqtt_publish(buf, str2, strlen(str2), 1 /* (retain) */);
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
	if(sendall==1){
		return 1;
	}
	else
	{
		if((IDHist[index].sent!=1)||(IDHist[index].value!=value)){
			IDHist[index].sent=1;
			IDHist[index].value=value;
			return 1;
		}
		return 0;
	}
}
//---------------------------------------------------------------------------
void parseline()
{
  uint8_t validhex;
	uint8_t i;

	if(sendraw==1){
		send("raw", gwdata.line);
	}


	// check on responses/error codes and forward those.
  Response t;
  for(i=0;(i<sizeof(responselist)/sizeof(Response));i++){
    t = responselist[i];
    if(strncmp(t.response, gwdata.line, strlen(t.response)) == 0){
    	send("response", gwdata.line);
    	return;
    }
  }

	// filter on B(oiler),T(hermostat),A(nswer),R(equest)
	if(!((gwdata.line[0]=='B')|(gwdata.line[0]=='T')|(gwdata.line[0]=='A')|(gwdata.line[0]=='R')|(gwdata.line[0]=='E'))){
  	send("incomplete", gwdata.line);
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

		OTFrameInfo f;
		f.data.byte[0] = (hextoint(gwdata.line[1])<<4) + hextoint(gwdata.line[2]);
		f.data.byte[1] = (hextoint(gwdata.line[3])<<4) + hextoint(gwdata.line[4]);
		f.data.byte[2] = (hextoint(gwdata.line[5])<<4) + hextoint(gwdata.line[6]);
		f.data.byte[3] = (hextoint(gwdata.line[7])<<4) + hextoint(gwdata.line[8]);

		uint8_t dataid = f.data.byte[1];
		int found=0;
		// assume OTInfos is sorted on Data ID
		for(i=0; (i < (sizeof(OTInfos)/sizeof(OTInformation)))&&(OTInfos[i].ID<=dataid);i++){
			if((OTInfos[i].ID==dataid)&&(OTInfos[i].topic!="")){
				found=1;
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
						PutUnsignedInt(payload, '0', 2, (int)((f.data.byte[3]*100)/255));
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
		if(found==0) send("unknown", gwdata.line);
	} else send("invalid", gwdata.line);
}
//---------------------------------------------------------------------------
// character received will be stored in linebuffer, which will be
// processed when a line-feed (0x0a) is encountered.
void gw_char(uint8_t c)
{
	if(c == 0x0d) return;
  gwdata.line[gwdata.len] = c;
  if((gwdata.line[gwdata.len] == 0x0a) || (gwdata.len == OTGW_LINELEN - 1)){
    // discard newline, close string, parse line and clear it.
    gwdata.line[gwdata.len] = 0;
    parseline();
    gwdata.len = 0;
  }
  else ++gwdata.len;
}
//---------------------------------------------------------------------------
void OT_callback(char* topic, char* payload,int length) {
  // handle message arrived

	if(length>0){
		// check on valid command and forward those to the gateway
		uint8_t i;
		Command t;
		for(i=0;(i<sizeof(commandlist)/sizeof(Command));i++){
			t = commandlist[i];
			if(strncmp(t.cmd, topic, strlen(t.cmd)) == 0){
				// send it to the gateway
				UART0_PrintString(payload);
				UART0_Sendchar(0x0d);
				return;
			}
		}

		for(i=0;(i<sizeof(localcommandlist)/sizeof(Command));i++){
			t = localcommandlist[i];
			if(strncmp(t.cmd, topic, strlen(topic)) == 0){
				switch(i){
				case 0: // d
					sendraw=(strncmp(payload, "1", 1) == 0?1:0);
					break;
				case 1:	// c
					sendall=(strncmp(payload, "1", 1) == 0?1:0);
					break;
				case 2:	// i
					for(i=0; i < (sizeof(IDHist)/sizeof(OTHistory));i++){
						IDHist[i].sent=0;
					}
					break;
				}
				return;
			}
		}
	}
}
//---------------------------------------------------------------------------
void OT_init(void)
{
	mqtt_appcallback = OT_callback;

	int i;
	for(i=0; i < (sizeof(IDHist)/sizeof(OTHistory));i++){
		IDHist[i].sent=0;
	}
	gwdata.len=0;
}
