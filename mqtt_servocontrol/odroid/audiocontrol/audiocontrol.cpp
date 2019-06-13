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

#define AUDIOFILE_FOLDER_PREFIX "/home/odroid/github/hexapod/sounds/"


int main(int argc,char *argv[])
{
  class MqttConnection *mqtt_connection;
  char received_copy[1000] = {0};
  int counter = 1000000;

  printf("Mqtt init\n");
  fflush(stdout);
  mosqpp::lib_init();
  printf("Connect to synology\n");
  mqtt_connection = new MqttConnection("audiocontrol", "localhost", 1883, "audiocontrol", on_message_func);
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
      if(received_copy[0]=='P')
      {
        char *audiofile = received_copy+2;
        char systemString[256];
        sprintf(systemString, "mplayer -ao alsa:device=hw=0.0 -volume 10 \"%s%s\"", AUDIOFILE_FOLDER_PREFIX, audiofile);
        printf("Executing \"%s\"\n", systemString);
        fflush(stdout);
        system(systemString);
      }
    }
    usleep(SLEEP_MICROS);
    counter++;
    if(counter>(10000000/SLEEP_MICROS)) { //Every 10 seconds
      counter = 0;
      mqtt_connection->publish_string("R ac 1");
    }
  }
  mqtt_connection->loop_stop();
  mosqpp::lib_cleanup();
}
