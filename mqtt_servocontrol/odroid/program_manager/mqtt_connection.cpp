#include <cstdio>
#include <cstring>

#include "mqtt_connection.h"
#include <mosquittopp.h>

MqttConnection::MqttConnection(const char *id, const char *host, int port, const char *topic, void (*on_message_func)(const struct mosquitto_message *))
    :mosquittopp(id)
{
	int keepalive = 60;

	/* Connect immediately. This could also be done by calling
	 * MqttConnection->connect(). */
	connect(host, port, keepalive);
    this->on_message_func = on_message_func;
    this->topic = topic;
};

MqttConnection::~MqttConnection()
{
}

void MqttConnection::on_connect(int rc)
{
	printf("Connected with code %d.\n", rc);
	if(rc == 0){
		/* Only attempt to subscribe on a successful connect. */
		subscribe(NULL, topic);
	}
}

void MqttConnection::publish_string(char *s)
{
    int len = strlen(s);
    publish(NULL, topic, len, s);
}

void MqttConnection::on_message(const struct mosquitto_message *message)
{
    printf("on_message: %s\n", (char *)(message->payload));
    (*on_message_func)(message);
    
	/*
	double temp_celsius, temp_farenheit;
	char buf[51];
    
    if(!strcmp(message->topic, "temperature/celsius")){
		memset(buf, 0, 51*sizeof(char));
		// Copy N-1 bytes to ensure always 0 terminated.
		memcpy(buf, message->payload, 50*sizeof(char));
		temp_celsius = atof(buf);
		temp_farenheit = temp_celsius*9.0/5.0 + 32.0;
		snprintf(buf, 50, "%f", temp_farenheit);
		publish(NULL, "temperature/farenheit", strlen(buf), buf);
	}*/
}

void MqttConnection::on_subscribe(int mid, int qos_count, const int *granted_qos)
{
	printf("Subscription succeeded.\n");
}

