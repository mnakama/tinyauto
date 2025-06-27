#include <err.h>
#include <fcntl.h> // open
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <bsd/string.h>
#include <MQTTClient.h>

const char topicPrefix[] = "zigbee2mqtt/";

// light switches
const char LivingRoomSwitchDoorSide[]    = "Living room switch - door side";
const char HallSwitch[]                  = "Hall switch";
const char BedroomSwitch[]               = "Bedroom switch";
const char MattsOfficeSwitch[]           = "Matt's Office switch";
const char KitchenStoveSwitch[]          = "Kitchen Stove switch";

const char ActionSingle[]  = R"("action":"single")";
const char ActionDouble[]  = R"("action":"double")";
const char ActionHold[]    = R"("action":"hold")";
const char ActionRelease[] = R"("action":"release")";

// windows
const char BedroomWindowLeft[]     = "Bedroom window left";
const char BedroomWindowRight[]    = "Bedroom window right";
const char KitchenWindowRight[]    = "Kitchen window right";
const char LivingRoomWindowLeft[]  = "Living room window left";
const char LivingRoomWindowRight[] = "Living room window right";

const char WindowOpen[]   = R"("contact":false)";
const char WindowClosed[] = R"("contact":true)";

const short BedroomLeft     = 0b00001;
const short BedroomRight    = 0b00010;
const short KitchenRight    = 0b00100;
const short LivingRoomLeft  = 0b01000;
const short LivingRoomRight = 0b10000;
const short AllWindows      = 0b11111;

MQTTClient client;
short WindowState = 0; // all open

void cleanup() {
	int fh = creat("window_state", 0664);
	if (fh < 0) {
		err(EXIT_FAILURE, "could not open 'window_state'");
	}

	size_t ret = write(fh, &WindowState, sizeof(WindowState));
	if (ret < sizeof(WindowState)) {
		err(EXIT_FAILURE, "could not write to 'window_state'. Bytes written: %zu", ret);
	}

	if (close(fh) < 0) {
		err(EXIT_FAILURE, "could not close 'window_state'");
	}

	exit(0);
}

void signalHandler(int signum) {
	switch (signum) {
	case SIGINT:
		printf("Received SIGINT. Exitting.\n");
		cleanup();
		break;
	case SIGTERM:
		printf("Received SIGTERM. Exitting.\n");
		cleanup();
		break;
	default:
		printf("Unhandled signal: %d\n", signum);
	}
}

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

void zigbeeSet(const char *partialTopic, char* message) {
	const char suffix[] = "/set";

	int totalLen = sizeof(topicPrefix)-1 + sizeof(suffix)-1 + strlen(partialTopic);

	char *topic = alloca(totalLen+1);

	char *dest = stpcpy(topic, topicPrefix);
	dest = stpcpy(dest, partialTopic);
	dest = stpcpy(dest, suffix);

	sendMessage(topic, message);
}

void lightSwitchPressed(const char *target, MQTTClient_message *message) {
	if (strnstr(message->payload, ActionSingle, message->payloadlen) != NULL) {
		zigbeeSet(target, R"({"state":"TOGGLE"})");

	} else if (strnstr(message->payload, ActionDouble, message->payloadlen) != NULL) {
		zigbeeSet(target, R"({"state":"ON","brightness":"25"})");

	} else if (strnstr(message->payload, ActionHold, message->payloadlen) != NULL) {
		//zigbeeSet(target, R"({"state":"ON","brightness":"255"})");
		zigbeeSet(target, R"({"brightness_move_onoff":100})");

	} else if (strnstr(message->payload, ActionRelease, message->payloadlen) != NULL) {
		zigbeeSet(target, R"({"brightness_move_onoff":0})");
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
		short savedWindowState = WindowState;
		WindowState |= window;

		if (WindowState == AllWindows && savedWindowState != WindowState) {
			printf("All windows are closed. Turning filters on.\n");
			zigbeeSet("Bedroom air filter",     R"({"state":"ON"})");
			zigbeeSet("Living room air filter", R"({"state":"ON"})");
		}
	} else {
		// open
		short savedWindowState = WindowState;
		WindowState &= ~window;

		if (savedWindowState != WindowState) {
			printf("Window opened. Turning filters off.\n");
			zigbeeSet("Bedroom air filter",     R"({"state":"OFF"})");
			zigbeeSet("Living room air filter", R"({"state":"OFF"})");
		}
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

	if (strncmp(topicName, topicPrefix, sizeof(topicPrefix)-1) != 0) {
		printf("prefix mismatch. Expected '%s'", topicPrefix);
		goto messageArrived_cleanup;
	}

	char* deviceName = topicName + sizeof(topicPrefix)-1;

	// switches
	if (strcmp(deviceName, LivingRoomSwitchDoorSide) == 0 ||
		strcmp(deviceName, HallSwitch) == 0) {
		lightSwitchPressed("Hall Light", message);
	} else if (strcmp(deviceName, BedroomSwitch) == 0) {
		lightSwitchPressed("Bedroom lights", message);
	} else if (strcmp(deviceName, MattsOfficeSwitch) == 0) {
		lightSwitchPressed("Matt's Office Light", message);
	} else if (strcmp(deviceName, KitchenStoveSwitch) == 0) {
		lightSwitchPressed("Kitchen Stove", message);

		// windows
	} else if (strcmp(deviceName, BedroomWindowLeft) == 0) {
		windowStateChanged(BedroomLeft, message);
	} else if (strcmp(deviceName, BedroomWindowRight) == 0) {
		windowStateChanged(BedroomRight, message);
	} else if (strcmp(deviceName, KitchenWindowRight) == 0) {
		windowStateChanged(KitchenRight, message);
	} else if (strcmp(deviceName, LivingRoomWindowLeft) == 0) {
		windowStateChanged(LivingRoomLeft, message);
	} else if (strcmp(deviceName, LivingRoomWindowRight) == 0) {
		windowStateChanged(LivingRoomRight, message);

	} else {
		printf("Unrecognized topic: %s\n", topicName);
	}

messageArrived_cleanup:
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);

	return 1;
}

void subscribe(const char* device) {
	char* topic = alloca(sizeof(topicPrefix)-1 + strlen(device) + 1);
	char* dest = stpcpy(topic, topicPrefix);
	stpcpy(dest, device);

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
	subscribe(HallSwitch);
	subscribe(MattsOfficeSwitch);
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

void loadWindowState() {
	int fh = open("window_state", O_RDONLY);
	if (fh < 0) {
		warn("could not open 'window_state'");
		return;
	}

	size_t ret = read(fh, &WindowState, sizeof(WindowState));
	if (ret < sizeof(WindowState)) {
		warn("could not read 'window_state'. Bytes read: %zu", ret);
	}

	if (close(fh) < 0) {
		warn("could not close 'window_state'");
	}

	printf("WindowState: 0x%x\n", WindowState);
}

int main(/*int argc, char* argv[]*/) {
    int rc;

	// set line buffering so we can watch the logs in journald
	setlinebuf(stdout);

    // Create an MQTT client instance
    MQTTClient_create(&client, "tcp://[::1]:1883", "ExampleClientSub", MQTTCLIENT_PERSISTENCE_NONE, NULL);

    // Set the callback function to handle incoming messages
    //MQTTClient_setCallbacks(client, NULL, NULL, messageArrived, NULL);

	loadWindowState();

	rc = mconnect();
	if (rc != MQTTCLIENT_SUCCESS) exit(1);

	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);

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
