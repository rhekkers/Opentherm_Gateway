#ifndef __OPENTHERM_H__

#define __OPENTHERM_H__

#include "lpc_types.h"

#define DEVICEID	"OTGW"

#define TPF     "/sensor/"DEVICEID"/"    // MQTT topic prefix. temporary.
#define CPF     "/command/"DEVICEID"/"   // MQTT topic prefix. temporary.

//#define OTGWCommand   "/OTGW/cmd"     // command topic

void gw_char(uint8_t c);
void OT_init(void);
void OT_callback(char* topic, char* payload,int length);

//#define LED6_ON         GPIOSetValue(1, 18, 1)
//#define LED6_OFF        GPIOSetValue(1, 18, 0)
//#define LED7_ON         GPIOSetValue(1, 19, 1)
//#define LED7_OFF        GPIOSetValue(1, 19, 0)
//#define LED8_ON         GPIOSetValue(1, 24, 1)
//#define LED8_OFF        GPIOSetValue(1, 24, 0)
//#define LED9_ON         GPIOSetValue(1, 25, 1)
//#define LED9_OFF        GPIOSetValue(1, 25, 0)
//
//#define LEDX_ON         GPIOSetValue(0, 26, 1)
//#define LEDX_OFF        GPIOSetValue(0, 26, 0)
//

//typedef enum { RESPONSE, COMMAND} MatchType;
typedef struct {
  char* response;
} Response;


typedef struct {
  char* cmd;
} Command;


#endif /* __OPENTHERM_H__ */
