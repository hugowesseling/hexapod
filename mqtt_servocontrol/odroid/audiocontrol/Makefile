CFLAGS=-Wall -ggdb -I ~/mosq/mosquitto-1.4.5/lib -I ~/mosq/mosquitto-1.4.5/lib/cpp
LDFLAGS=-L ~/mosq/mosquitto-1.4.5/lib  ~/mosq/mosquitto-1.4.5/lib/cpp/libmosquittopp.so.1  ~/mosq/mosquitto-1.4.5/lib/libmosquitto.so.1

.PHONY: all clean

all : audiocontrol

#Executables

audiocontrol : audiocontrol.o mqtt_connection.o
	${CXX} $^ -o $@ ${LDFLAGS}

#Objects

audiocontrol.o : audiocontrol.cpp
	${CXX} -c $^ -o $@ ${CFLAGS}

mqtt_connection.o : mqtt_connection.cpp
	${CXX} -c $^ -o $@ ${CFLAGS}

####

clean : 
	-rm -f *.o audiocontrol
