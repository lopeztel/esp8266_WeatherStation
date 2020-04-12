#ifndef _USER_CONFIG_H_
#define _USER_CONFIG_H_

#define HTTP_DEBUG	true
#define DHT22_DEBUG	true

#define WIFI_CLIENTSSID		"<your_wifi_client_SSID>"
#define WIFI_CLIENTPASSWORD	"<your_wifi_client_password>"

#define DATA_SEND_DELAY 5000//900000	/* milliseconds */ //15 min wait time
#define WIFI_CHECK_DELAY 1000	/* milliseconds */ //check every 5 minutes

// Thingspeak server address

#define THINGSPEAK_SERVER	"api.thingspeak.com"

// Thingspeak channel write API key
#define THINGSPEAK_API_KEY	"<Thinkspeak_API_key>"

/*MQTT stuff*/
#define MQTT_HOST			"<MQTT_broker_host IP>" //Host IP address, Raspberry Pi or "mqtt.yourdomain.com"
#define MQTT_PORT			1883
#define MQTT_BUF_SIZE		1024
#define MQTT_KEEPALIVE		120	 /*second*/

#define MQTT_CLIENT_ID		"ESP_WeatherStation" //Name for your client, no duplicates
#define MQTT_USER			"<MQTT_broker_user>"
#define MQTT_PASS			"<MQTT_broker_password>"
#define MQTT_RECONNECT_TIMEOUT 	5	/*second*/
#define DEFAULT_SECURITY	0
#define QUEUE_BUFFER_SIZE		 		2048
#define PROTOCOL_NAMEv31	/*MQTT version 3.1 compatible with Mosquitto v0.15*/
//PROTOCOL_NAMEv311			/*MQTT version 3.11 compatible with https://eclipse.org/paho/clients/testing/*/
#define CLIENT_SSL_ENABLE
#endif
