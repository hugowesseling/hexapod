#ifndef TEMPERATURE_CONVERSION_H
#define TEMPERATURE_CONVERSION_H

#include <mosquittopp.h>

class MqttConnection : public mosqpp::mosquittopp
{
    private:
        const char *topic;
        void (*on_message_func)(const struct mosquitto_message *);

		void on_connect(int rc);
		void on_message(const struct mosquitto_message *message);
		void on_subscribe(int mid, int qos_count, const int *granted_qos);

	public:
		MqttConnection(const char *id, const char *host, int port, const char *topic, void (*on_message_func)(const struct mosquitto_message *));
		~MqttConnection();

		void publish_string(const char *s);

};

#endif
