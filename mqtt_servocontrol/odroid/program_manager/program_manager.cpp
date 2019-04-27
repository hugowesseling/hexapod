// First: Add single leg gate to gracefully get into stand4 mode
// To do that: Create mapping from leg to leg group (tripodgait has two groups, ripple three and single leg six)
// Create seqMaps for all and link them all in a single pointer array
// Next step: create long temporal command: walkfor <steps>
// Command will execute, and send "Command End" upon completion


#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include "mqtt_connection.h"

using namespace std;

int received = 0;
char received_msg[1000] = {0};

void on_message_func(const struct mosquitto_message *message)
{
    printf("on_message_func: %s\n", (char *)(message->payload));
    if(!received && message->payload != NULL)
    {
        strcpy(received_msg,(char *)(message->payload));
        received = 1;
    }
}

typedef struct strstrmap{
	const char *key, *value;
} StrStrMap;

#define ALIAS_COUNT 3
StrStrMap processNames[ALIAS_COUNT] = {
	"pm", "program_manager", 
	"sc", "servocontrol",
	"sv", "ffmpeg"};

StrStrMap cmds[ALIAS_COUNT] = {
	"pm", "~/github/hexapod/mqtt_servocontrol/odroid/program_manager/program_manager &",
	"sc", "~/github/hexapod/mqtt_servocontrol/odroid/servocontrol &",
	"sv", "~/stream_video.sh &"};

int main(int argc,char *argv[])
{
  class MqttConnection *mqtt_connection;
  char received_copy[1000] = {0};

  printf("Mqtt init\n");
  mosqpp::lib_init();
  printf("Connect to synology\n");
  mqtt_connection = new MqttConnection("program_manager", "localhost", 1883, "programcontrol", on_message_func);
  printf("Starting loop\n");
  mqtt_connection->loop_start();
  printf("Mqtt setup complete\n");
  
  usleep(100000);

  while(true)
  {
    if(received)
    {
      strcpy(received_copy, received_msg);
      received = 0;

      printf("Received1:'%s'\n",received_msg);
      printf("Received2:'%s'\n",received_copy);
      if(received_copy[0]=='K')
      {
        // Toggle servo power
        char *alias = received_copy+2;
//	system("killall program_manager &");
	int i;
	for(i = 0; i < ALIAS_COUNT; i++){
          if(strcmp(alias, processNames[i].key) == 0){
            char systemString[256];
            sprintf(systemString, "killall %s", processNames[i].value);
            printf("Executing \"%s\"\n", systemString);
	    system(systemString);
            break;
          }
	}
	if(i == ALIAS_COUNT){
	  printf("Alias \"%s\" not known\n", alias);
	}
      }
      if(received_copy[0]=='S')
      {
        char *alias = received_copy+2;
        int i;
        for(i = 0; i < ALIAS_COUNT; i++){
          if(strcmp(alias, cmds[i].key) == 0){
            printf("Executing \"%s\"\n", cmds[i].value);
            system(cmds[i].value);
            break;
          }
        }
        if(i == ALIAS_COUNT){
          printf("Alias \"%s\" not known\n", alias);
        }
      }
      if(received_copy[0]=='C')
      {
        char *alias = received_copy+2;
        int i;
        for(i = 0; i < ALIAS_COUNT; i++){
          if(strcmp(alias, processNames[i].key) == 0){
	    const char *pn = processNames[i].value;
	    char bufstring[256];
            sprintf(bufstring, "ps -Af | grep \"[%c]%s\"", pn[0], pn+1);
            printf("Executing \"%s\"\n", bufstring);
	    int retval = system(bufstring);
            printf("retval: %d\n", retval);
	    sprintf(bufstring, "R %s %d", alias, !retval);
	    mqtt_connection->publish_string(bufstring);
            break;
          }
        }
        if(i == ALIAS_COUNT){
          printf("Alias \"%s\" not known\n", alias);
        }
      }

    }
    usleep(50000);
  }
  mqtt_connection->loop_stop();
  mosqpp::lib_cleanup();
}
