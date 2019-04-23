CFLAGS=-Wall -ggdb -I ~/mosq/mosquitto-1.4.5/lib -I ~/mosq/mosquitto-1.4.5/lib/cpp
LDFLAGS=-L ~/mosq/mosquitto-1.4.5/lib  ~/mosq/mosquitto-1.4.5/lib/cpp/libmosquittopp.so.1  ~/mosq/mosquitto-1.4.5/lib/libmosquitto.so.1

.PHONY: all clean

all : servocontrol

#Executables

servocontrol : servocontrol.o mqtt_connection.o
	${CXX} $^ -o $@ ${LDFLAGS}

connection_test : connection_test.o mqtt_connection.o
	${CXX} $^ -o $@ ${LDFLAGS}

#Objects

servocontrol.o : servocontrol.cpp
	${CXX} -c $^ -o $@ ${CFLAGS}

connection_test.o : connection_test.cpp
	${CXX} -c $^ -o $@ ${CFLAGS}

mqtt_connection.o : mqtt_connection.cpp
	${CXX} -c $^ -o $@ ${CFLAGS}

####

clean : 
	-rm -f *.o connection_test servocontrol
