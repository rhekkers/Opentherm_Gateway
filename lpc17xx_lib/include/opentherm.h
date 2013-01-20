#ifndef __OPENTHERM_H__

#define __OPENTHERM_H__

#include "lpc_types.h"

#define DEVICEID	"otgw"
#define TPF     	"/sensor/"DEVICEID"/"    // MQTT topic prefix. temporary.
#define CPF     	"/command/"DEVICEID"/"   // MQTT topic prefix. temporary.

void gw_char(uint8_t c);
void OT_init(void);
void OT_callback(char* topic, char* payload,int length);

typedef struct {
  char* response;
} Response;


typedef struct {
  char* cmd;
} Command;


#endif /* __OPENTHERM_H__ */
