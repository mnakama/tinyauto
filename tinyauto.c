#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <bsd/string.h>
#include <MQTTClient.h>

MQTTClient client;

const char LivingRoomSwitchDoorSide[] = "zigbee2mqtt/Living room switch - door side";
const char LivingRoomSwitchBedroomSide[] = "zigbee2mqtt/Living room switch - bedroom side";
const char BedroomSwitch[] = "zigbee2mqtt/Bedroom switch";
const char KitchenHallSwitch[] = "zigbee2mqtt/Kitchen Hall switch";
const char KitchenStoveSwitch[] = "zigbee2mqtt/Kitchen Stove switch";

const char ActionSingle[] = R"("action":"single")";
const char ActionDouble[] = R"("action":"double")";
const char ActionHold[] = R"("action":"hold")";
//const char ActionRelease[] = R"("action":"release")";

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

	char *topic = NULL;
	if (strcmp(topicName, LivingRoomSwitchDoorSide) == 0 || strcmp(topicName, LivingRoomSwitchBedroomSide) == 0) {
		topic = "zigbee2mqtt/Living room lights/set";
	} else if (strcmp(topicName, BedroomSwitch) == 0) {
		topic = "zigbee2mqtt/Bedroom lights/set";
	} else if (strcmp(topicName, KitchenHallSwitch) == 0) {
		topic = "zigbee2mqtt/Kitchen Hall/set";
	} else if (strcmp(topicName, KitchenStoveSwitch) == 0) {
		topic = "zigbee2mqtt/Kitchen Stove/set";
	} else {
		printf("Unrecognized topic: %s\n", topicName);
		goto cleanup;
	}

	if (strnstr(message->payload, ActionSingle, message->payloadlen) != NULL) {
		sendMessage(topic, R"({"state":"TOGGLE"})");
	} else if (strnstr(message->payload, ActionDouble, message->payloadlen) != NULL) {
		sendMessage(topic,  R"({"state":"ON","brightness":"25"})");
	} else if (strnstr(message->payload, ActionHold, message->payloadlen) != NULL) {
		sendMessage(topic, R"({"state":"ON","brightness":"255"})");
	}

 cleanup:
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

	subscribe(LivingRoomSwitchDoorSide);
	subscribe(LivingRoomSwitchBedroomSide);
	subscribe(KitchenHallSwitch);
	subscribe(KitchenStoveSwitch);
	subscribe(BedroomSwitch);

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
