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

#define SLEEP_MICROS 50000

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
	"pm", "/home/odroid/github/hexapod/mqtt_servocontrol/odroid/program_manager/program_manager &",
	"sc", "/home/odroid/github/hexapod/mqtt_servocontrol/odroid/servocontrol &",
	"sv", "/home/odroid/stream_video.sh"};

int main(int argc,char *argv[])
{
  class MqttConnection *mqtt_connection;
  char received_copy[1000] = {0};
  int counter = 1000000;

  printf("Mqtt init\n");
  fflush(stdout);
  mosqpp::lib_init();
  printf("Connect to synology\n");
  mqtt_connection = new MqttConnection("program_manager", "localhost", 1883, "programcontrol", on_message_func);
  printf("Starting loop\n");
  fflush(stdout);
  mqtt_connection->loop_start();
  printf("Mqtt setup complete\n");
  fflush(stdout);
  
  usleep(100000);

  while(true)
  {
    if(received)
    {
      strcpy(received_copy, received_msg);
      received = 0;

      printf("Received1:'%s'\n",received_msg);
      fflush(stdout);
      printf("Received2:'%s'\n",received_copy);
      fflush(stdout);
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
            fflush(stdout);
	    system(systemString);
            //Directly do a check
            char bufstring[256];
            sprintf(bufstring, "C %s", alias);
            mqtt_connection->publish_string(bufstring);
            break;
          }
	}
	if(i == ALIAS_COUNT){
	  printf("Alias \"%s\" not known\n", alias);
          fflush(stdout);
	}
      }
      if(received_copy[0]=='S')
      {
        char *alias = received_copy+2;
        int i;
        for(i = 0; i < ALIAS_COUNT; i++){
          if(strcmp(alias, cmds[i].key) == 0){
            printf("Executing \"%s\"\n", cmds[i].value);
            fflush(stdout);
            system(cmds[i].value);
            //Directly do a check
            char bufstring[256];
            sprintf(bufstring, "C %s", alias);
            mqtt_connection->publish_string(bufstring);
            break;
          }
        }
        if(i == ALIAS_COUNT){
          printf("Alias \"%s\" not known\n", alias);
          fflush(stdout);
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
            fflush(stdout);
	    int retval = system(bufstring);
            printf("retval: %d\n", retval);
            fflush(stdout);
	    sprintf(bufstring, "R %s %d", alias, !retval);
	    mqtt_connection->publish_string(bufstring);
            break;
          }
        }
        if(i == ALIAS_COUNT){
          printf("Alias \"%s\" not known\n", alias);
          fflush(stdout);
        }
      }

    }
    usleep(SLEEP_MICROS);
    counter++;
    if(counter>(10000000/SLEEP_MICROS)) { //Every 10 seconds
      counter = 0;
      mqtt_connection->publish_string("R pm 1");
    }
  }
  mqtt_connection->loop_stop();
  mosqpp::lib_cleanup();
}
