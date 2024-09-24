#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <bsd/string.h>
#include <MQTTClient.h>

MQTTClient client;

// light switches
const char LivingRoomSwitchDoorSide[]    = "zigbee2mqtt/Living room switch - door side";
const char LivingRoomSwitchBedroomSide[] = "zigbee2mqtt/Living room switch - bedroom side";
const char BedroomSwitch[]               = "zigbee2mqtt/Bedroom switch";
const char KitchenHallSwitch[]           = "zigbee2mqtt/Kitchen Hall switch";
const char KitchenStoveSwitch[]          = "zigbee2mqtt/Kitchen Stove switch";

const char ActionSingle[]  = R"("action":"single")";
const char ActionDouble[]  = R"("action":"double")";
const char ActionHold[]    = R"("action":"hold")";
const char ActionRelease[] = R"("action":"release")";

// windows
const char BedroomWindowLeft[]     = "zigbee2mqtt/Bedroom window left";
const char BedroomWindowRight[]    = "zigbee2mqtt/Bedroom window right";
const char KitchenWindowRight[]    = "zigbee2mqtt/Kitchen window right";
const char LivingRoomWindowLeft[]  = "zigbee2mqtt/Living room window left";
const char LivingRoomWindowRight[] = "zigbee2mqtt/Living room window right";

const char WindowOpen[]   = R"("contact":false)";
const char WindowClosed[] = R"("contact":true)";

const short BedroomLeft     = 0b00001;
const short BedroomRight    = 0b00010;
const short KitchenRight    = 0b00100;
const short LivingRoomLeft  = 0b01000;
const short LivingRoomRight = 0b10000;
const short AllWindows      = 0b11111;

short WindowState = 0; // all open

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


void windowStateChanged(int window, MQTTClient_message *message) {
	// false is open; true is closed; same as contact field
	bool state = false;
	if (strnstr(message->payload, WindowOpen, message->payloadlen) != NULL) {
		state = false;
	} else if (strnstr(message->payload, WindowClosed, message->payloadlen) != NULL) {
		state = true;
	} else {
		printf("Unhandled window message: %s\n", (char*)message->payload);
		return;
	}

	if (state == true) {
		// closed
		WindowState |= window;

		// TODO: save and restore state persistently.
		if (WindowState == AllWindows) {
			printf("All windows are closed. Turning filters on.\n");
			sendMessage("zigbee2mqtt/Bedroom air filter/set",     R"({"state":"ON"})");
			sendMessage("zigbee2mqtt/Living room air filter/set", R"({"state":"ON"})");
		}
	} else {
		// open
		WindowState &= ~window;

		printf("Turning filters off.\n");
		sendMessage("zigbee2mqtt/Bedroom air filter/set",     R"({"state":"OFF"})");
		sendMessage("zigbee2mqtt/Living room air filter/set", R"({"state":"OFF"})");
	}

	printf("WindowState: 0x%x\n", WindowState);
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
	// switches
	if (strcmp(topicName, LivingRoomSwitchDoorSide) == 0 ||
		strcmp(topicName, LivingRoomSwitchBedroomSide) == 0) {
		topic = "zigbee2mqtt/Living room lights/set";
	} else if (strcmp(topicName, BedroomSwitch) == 0) {
		topic = "zigbee2mqtt/Bedroom lights/set";
	} else if (strcmp(topicName, KitchenHallSwitch) == 0) {
		topic = "zigbee2mqtt/Kitchen Hall/set";
	} else if (strcmp(topicName, KitchenStoveSwitch) == 0) {
		topic = "zigbee2mqtt/Kitchen Stove/set";

		// windows
	} else if (strcmp(topicName, BedroomWindowLeft) == 0) {
		windowStateChanged(BedroomLeft, message);
		goto cleanup;
	} else if (strcmp(topicName, BedroomWindowRight) == 0) {
		windowStateChanged(BedroomRight, message);
		goto cleanup;
	} else if (strcmp(topicName, KitchenWindowRight) == 0) {
		windowStateChanged(KitchenRight, message);
		goto cleanup;
	} else if (strcmp(topicName, LivingRoomWindowLeft) == 0) {
		windowStateChanged(LivingRoomLeft, message);
		goto cleanup;
	} else if (strcmp(topicName, LivingRoomWindowRight) == 0) {
		windowStateChanged(LivingRoomRight, message);
		goto cleanup;

	} else {
		printf("Unrecognized topic: %s\n", topicName);
		goto cleanup;
	}

	if (strnstr(message->payload, ActionSingle, message->payloadlen) != NULL) {
		sendMessage(topic, R"({"state":"TOGGLE"})");

	} else if (strnstr(message->payload, ActionDouble, message->payloadlen) != NULL) {
		sendMessage(topic, R"({"state":"ON","brightness":"25"})");

	} else if (strnstr(message->payload, ActionHold, message->payloadlen) != NULL) {
		//sendMessage(topic, R"({"state":"ON","brightness":"255"})");
		sendMessage(topic, R"({"brightness_move_onoff":100})");

	} else if (strnstr(message->payload, ActionRelease, message->payloadlen) != NULL) {
		sendMessage(topic, R"({"brightness_move_onoff":0})");
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

	// switches
	subscribe(LivingRoomSwitchDoorSide);
	subscribe(LivingRoomSwitchBedroomSide);
	subscribe(KitchenHallSwitch);
	subscribe(KitchenStoveSwitch);
	subscribe(BedroomSwitch);

	// windows
	subscribe(BedroomWindowLeft);
	subscribe(BedroomWindowRight);
	subscribe(KitchenWindowRight);
	subscribe(LivingRoomWindowLeft);
	subscribe(LivingRoomWindowRight);

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
