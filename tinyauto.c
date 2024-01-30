#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <bsd/string.h>
#include <MQTTClient.h>

MQTTClient client;

void sendMessage(const char *topicName, char* message) {
	MQTTClient_message pubmsg = MQTTClient_message_initializer;
	pubmsg.qos = 1;
	pubmsg.retained = 0;
	pubmsg.payload = message;
	pubmsg.payloadlen = strlen(pubmsg.payload);

	// Publish the message
	int rc;
	if ((rc = MQTTClient_publishMessage(client, topicName, &pubmsg, NULL)) != MQTTCLIENT_SUCCESS) {
		printf("Failed to publish message, return code %d\n", rc);
	}
}

int messageArrived(__attribute__((unused)) void *context,
				   char *topicName,
				   __attribute__((unused)) int topicLen,
				   MQTTClient_message *message) {
    int i;
    char* payloadptr;

    printf("Message arrived\n");
    printf("     topic: %s\n", topicName);
    printf("   message: ");

    payloadptr = message->payload;
    for(i=0; i<message->payloadlen; i++) {
        putchar(*payloadptr++);
    }
    putchar('\n');

	char topic[] = "zigbee2mqtt/Living room lights/set";

	if (strnstr(message->payload, "\"action\":\"single\"", message->payloadlen) != NULL) {
		sendMessage(topic, "{\"state\":\"TOGGLE\"}");
	} else if (strnstr(message->payload, "\"action\":\"double\"", message->payloadlen) != NULL) {
		sendMessage(topic,  "{\"state\":\"ON\",\"brightness\":\"25\"}");
	} else if (strnstr(message->payload, "\"action\":\"hold\"", message->payloadlen) != NULL) {
		sendMessage(topic, "{\"state\":\"ON\",\"brightness\":\"255\"}");
	}

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);

	return 1;
}

void subscribe(const char* topic) {
    // Subscribe to the topic
	static const int qos = 1;

	int rc;
	if ((rc = MQTTClient_subscribe(client, topic, qos)) != MQTTCLIENT_SUCCESS) {
		printf("Failed to subscribe, return code %d\n", rc);
		exit(-1);
	}
}

int mconnect() {
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    // Connect to the MQTT broker
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect, return code %d\n", rc);
        return rc;
    }

	subscribe("zigbee2mqtt/Living room switch - door side");
	subscribe("zigbee2mqtt/Living room switch - bedroom side");

	return rc;
}

int main(/*int argc, char* argv[]*/) {
    int rc;

    // Create an MQTT client instance
    MQTTClient_create(&client, "tcp://[::1]:1883", "ExampleClientSub", MQTTCLIENT_PERSISTENCE_NONE, NULL);

    // Set the callback function to handle incoming messages
    //MQTTClient_setCallbacks(client, NULL, NULL, messageArrived, NULL);

	rc = mconnect();
	if (rc != MQTTCLIENT_SUCCESS) exit(1);

	while (1) {
		printf("Waiting for messages...\n");

		char *topicName = NULL;
		int topicLen = 0;
		MQTTClient_message *message = NULL;

		int ret = MQTTClient_receive(client, &topicName, &topicLen, &message, -1);
		switch (ret) {
		case MQTTCLIENT_SUCCESS:
		case MQTTCLIENT_TOPICNAME_TRUNCATED:
			if (message == NULL) continue;
			messageArrived(NULL, topicName, topicLen, message);

			break;
		case MQTTCLIENT_DISCONNECTED:
			printf("MQTT receive failed: disconnected\n");
			rc = mconnect();
			while (rc != MQTTCLIENT_SUCCESS) {
				sleep(1);
				rc = mconnect();
			}

			printf("connected\n");
			break;
		default:
			printf("MQTT receive failed: %d\n", ret);
			sleep(1);
			break;
		}
	}

    // Disconnect from the MQTT broker
    MQTTClient_disconnect(client, 10000);

    // Destroy the MQTT client instance
    MQTTClient_destroy(&client);

    return rc;
}
