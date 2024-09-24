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

void cleanup() {
	int fh = creat("window_state", 0664);
	if (fh < 0) {
		err(EXIT_FAILURE, "could not open 'window_state'");
	}

	size_t ret = write(fh, &WindowState, sizeof(WindowState));
	if (ret < sizeof(WindowState)) {
		err(EXIT_FAILURE, "could not write to 'window_state'. Bytes written: %lu", ret);
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

void lightSwitchPressed(const char *topic, MQTTClient_message *message) {
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
			sendMessage("zigbee2mqtt/Bedroom air filter/set",     R"({"state":"ON"})");
			sendMessage("zigbee2mqtt/Living room air filter/set", R"({"state":"ON"})");
		}
	} else {
		// open
		short savedWindowState = WindowState;
		WindowState &= ~window;

		if (savedWindowState != WindowState) {
			printf("Window opened. Turning filters off.\n");
			sendMessage("zigbee2mqtt/Bedroom air filter/set",     R"({"state":"OFF"})");
			sendMessage("zigbee2mqtt/Living room air filter/set", R"({"state":"OFF"})");
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

	char *topic = NULL;
	// switches
	if (strcmp(topicName, LivingRoomSwitchDoorSide) == 0 ||
		strcmp(topicName, LivingRoomSwitchBedroomSide) == 0) {
		topic = "zigbee2mqtt/Living room lights/set";
		lightSwitchPressed(topic, message);
	} else if (strcmp(topicName, BedroomSwitch) == 0) {
		topic = "zigbee2mqtt/Bedroom lights/set";
		lightSwitchPressed(topic, message);
	} else if (strcmp(topicName, KitchenHallSwitch) == 0) {
		topic = "zigbee2mqtt/Kitchen Hall/set";
		lightSwitchPressed(topic, message);
	} else if (strcmp(topicName, KitchenStoveSwitch) == 0) {
		topic = "zigbee2mqtt/Kitchen Stove/set";
		lightSwitchPressed(topic, message);

		// windows
	} else if (strcmp(topicName, BedroomWindowLeft) == 0) {
		windowStateChanged(BedroomLeft, message);
	} else if (strcmp(topicName, BedroomWindowRight) == 0) {
		windowStateChanged(BedroomRight, message);
	} else if (strcmp(topicName, KitchenWindowRight) == 0) {
		windowStateChanged(KitchenRight, message);
	} else if (strcmp(topicName, LivingRoomWindowLeft) == 0) {
		windowStateChanged(LivingRoomLeft, message);
	} else if (strcmp(topicName, LivingRoomWindowRight) == 0) {
		windowStateChanged(LivingRoomRight, message);

	} else {
		printf("Unrecognized topic: %s\n", topicName);
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

void loadWindowState() {
	int fh = open("window_state", O_RDONLY);
	if (fh < 0) {
		warn("could not open 'window_state'");
		return;
	}

	size_t ret = read(fh, &WindowState, sizeof(WindowState));
	if (ret < sizeof(WindowState)) {
		warn("could not read 'window_state'. Bytes read: %lu", ret);
	}

	if (close(fh) < 0) {
		warn("could not close 'window_state'");
	}

	printf("WindowState: 0x%x\n", WindowState);
}

int main(/*int argc, char* argv[]*/) {
    int rc;

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
