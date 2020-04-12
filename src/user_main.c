/*
 *  Example of working sensor DHT22 (temperature and humidity) and send data on the service thingspeak.com
 *  https://thingspeak.com
 *
 *  For a single device, connect as follows:
 *  DHT22 1 (Vcc) to Vcc (3.3 Volts)
 *  DHT22 2 (DATA_OUT) to ESP Pin GPIO2
 *  DHT22 3 (NC)
 *  DHT22 4 (GND) to GND
 *
 *  Between Vcc and DATA_OUT need to connect a pull-up resistor of 10 kOh.
 *
 *  (c) 2015 by Mikhail Grigorev <sleuthhound@gmail.com>
 *
 */

#include <user_config.h>
#include <user_interface.h>
#include <osapi.h>
#include <c_types.h>
#include <mem.h>
#include <os_type.h>
#include "httpclient.h"
#include "driver/uart.h"
#include "dht22.h"
#include <mqtt.h>
#include <mem.h>


//typedef enum {
//	WIFI_CONNECTING,
//	WIFI_CONNECTING_ERROR,
//	WIFI_CONNECTED,
//} tConnState;

//unsigned char *default_certificate;
//unsigned int default_certificate_len = 0;
//unsigned char *default_private_key;
//unsigned int default_private_key_len = 0;

typedef void (*WifiCallback)(uint8_t);

extern int ets_uart_printf(const char *fmt, ...);
int (*console_printf)(const char *fmt, ...) = ets_uart_printf;

// Debug output.
#ifdef DHT22_DEBUG
#undef DHT22_DEBUG
#define DHT22_DEBUG(...) console_printf(__VA_ARGS__);
#else
#define DHT22_DEBUG(...)
#endif

LOCAL os_timer_t dht22_timer;
MQTT_Client mqttClient;
LOCAL bool ICACHE_FLASH_ATTR setup_wifi_st_mode();
static void ICACHE_FLASH_ATTR wifi_check_ip(void *arg);
static struct ip_info ipConfig;
static ETSTimer WiFiLinker;
static tConnState connState = WIFI_CONNECTING;
bool firstTime = true;

const char *FlashSizeMap[] =
{
		"512 KB (256 KB + 256 KB)",	// 0x00
		"256 KB",			// 0x01
		"1024 KB (512 KB + 512 KB)", 	// 0x02
		"2048 KB (512 KB + 512 KB)"	// 0x03
		"4096 KB (512 KB + 512 KB)"	// 0x04
		"2048 KB (1024 KB + 1024 KB)"	// 0x05
		"4096 KB (1024 KB + 1024 KB)"	// 0x06
};

const char *WiFiMode[] =
{
		"NULL",		// 0x00
		"STATION",	// 0x01
		"SOFTAP", 	// 0x02
		"STATIONAP"	// 0x03
};

const char *WiFiStatus[] =
{
	    "STATION_IDLE", 			// 0x00
	    "STATION_CONNECTING", 		// 0x01
	    "STATION_WRONG_PASSWORD", 	// 0x02
	    "STATION_NO_AP_FOUND", 		// 0x03
	    "STATION_CONNECT_FAIL", 	// 0x04
	    "STATION_GOT_IP" 			// 0x05
};

const char *statusMapping[] =
{
	    "OK",
	    "FAIL",
	    "PENDING",
	    "BUSY",
	    "CANCEL"
};

const char *authMapping[]=
{
	    "AUTH_OPEN",
	    "AUTH_WEP",
	    "AUTH_WPA_PSK",
	    "AUTH_WPA2_PSK",
	    "AUTH_WPA_WPA2_PSK",
	    "AUTH_MAX"
};

const char *MQTT_state[]=
{
	"WIFI_INIT",
	"WIFI_CONNECTING",
	"WIFI_CONNECTING_ERROR",
	"WIFI_CONNECTED",
	"DNS_RESOLVE",
	"TCP_DISCONNECTED",
	"TCP_RECONNECT_REQ",
	"TCP_RECONNECT",
	"TCP_CONNECTING",
	"TCP_CONNECTING_ERROR",
	"TCP_CONNECTED",
	"MQTT_CONNECT_SEND",
	"MQTT_CONNECT_SENDING",
	"MQTT_SUBSCIBE_SEND",
	"MQTT_SUBSCIBE_SENDING",
	"MQTT_DATA",
	"MQTT_PUBLISH_RECV",
	"MQTT_PUBLISHING"
};

/*void wifiConnectCb(uint8_t status)
{
	if(status == STATION_GOT_IP){
		MQTT_Connect(&mqttClient);
	} else {
		MQTT_Disconnect(&mqttClient);
	}
}*/
void mqttConnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	DHT22_DEBUG("MQTT: Connected\r\n");
	MQTT_Subscribe(client, "/mqtt/relay", 2);
	//MQTT_Subscribe(client, "/mqtt/topic/1", 1);
	//MQTT_Subscribe(client, "/mqtt/topic/2", 2);

	//MQTT_Publish(client, "temp", "00.00", 5, 0, 0); //limit to nn.nn strings
	//MQTT_Publish(client, "humty", "00.00", 5, 1, 0);
	//MQTT_Publish(client, "heatIdx", "00.00", 5, 2, 0);

}

void mqttDisconnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	DHT22_DEBUG("MQTT: Disconnected\r\n");
}

void mqttPublishedCb(uint32_t *args)
{
	static uint8_t counter = 0;
	MQTT_Client* client = (MQTT_Client*)args;
	DHT22_DEBUG("MQTT: Published %u \r\n", counter);
	if (counter != 5){
		counter++;
	}else{
	    system_soft_wdt_feed();
		DHT22_DEBUG("published, going to sleep...\r\n")
		system_deep_sleep(15*60*1000*1000); //sleep for  15 mins
	}
}

void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
	char *topicBuf = (char*)os_zalloc(topic_len+1),
			*dataBuf = (char*)os_zalloc(data_len+1);

	MQTT_Client* client = (MQTT_Client*)args;

	os_memcpy(topicBuf, topic, topic_len);
	topicBuf[topic_len] = 0;

	os_memcpy(dataBuf, data, data_len);
	dataBuf[data_len] = 0;

	DHT22_DEBUG("Receive topic: %s, data: %s \r\n", topicBuf, dataBuf);
	os_free(topicBuf);
	os_free(dataBuf);
}

LOCAL void ICACHE_FLASH_ATTR thingspeak_http_callback(char * response, int http_status, char * full_response)
{
	DHT22_DEBUG("Answers: \r\n");
	if (http_status == 200)
	{
		//DHT22_DEBUG("strlen(response)=%d\r\n", strlen(response));
		//DHT22_DEBUG("strlen(full_response)=%d\r\n", strlen(full_response));
		DHT22_DEBUG("response=%s<EOF>\n", response);
		//DHT22_DEBUG("full_response=%s\r\n", full_response);
		DHT22_DEBUG("---------------------------\r\n");
	}
	else
	{
		DHT22_DEBUG("http_status=%d\r\n", http_status);
		DHT22_DEBUG("strlen(response)=%d\r\n", strlen(response));
		DHT22_DEBUG("strlen(full_response)=%d\r\n", strlen(full_response));
		DHT22_DEBUG("response=%s<EOF>\n", response);
		DHT22_DEBUG("---------------------------\r\n");
	}
	DHT22_DEBUG("Free heap size = %d\r\n", system_get_free_heap_size());
}

LOCAL void ICACHE_FLASH_ATTR dht22_cb(void *arg)
{
	static char data[256];
	static char temp[10];
	static char hum[10];
	static char heatIdx[10];
	struct dht_sensor_data* r;
	float lastTemp, lastHum, lastHeatIdx;

	os_timer_disarm(&dht22_timer);
	os_timer_disarm(&WiFiLinker);
	switch(connState)
	{
		case WIFI_CONNECTED:
			r = DHTRead();
			lastTemp = r->temperature;
			lastHum = r->humidity;
			lastHeatIdx = r->heatindex;
			if(r->success)
			{
					wifi_get_ip_info(STATION_IF, &ipConfig);
					os_sprintf(temp, "%d.%d",(int)(lastTemp),(int)((lastTemp - (int)lastTemp)*100));
					os_sprintf(hum, "%d.%d",(int)(lastHum),(int)((lastHum - (int)lastHum)*100));
					os_sprintf(heatIdx, "%d.%d",(int)(lastHeatIdx),(int)((lastHeatIdx - (int)lastHeatIdx)*100));
					DHT22_DEBUG("Temperature: %s °C, Humidity: %s %%, Heat Index: %s °C\r\n", temp, hum, heatIdx);
					// Start the connection process
					//os_sprintf(data, "http://%s/update?key=%s&field1=%s&field2=%s&status=dev_ip:%d.%d.%d.%d", THINGSPEAK_SERVER, THINGSPEAK_API_KEY, temp, hum, IP2STR(&ipConfig.ip));
					//DHT22_DEBUG("Request: %s\r\n", data);
					//http_get(data, "", thingspeak_http_callback);

					/*os_sprintf(data, "key=%s&field1=%s&field2=%s&status=dev_ip:%d.%d.%d.%d", THINGSPEAK_SERVER, THINGSPEAK_API_KEY, temp, hum, IP2STR(&ipconfig.ip));
					DHT22_DEBUG("Request: http://184.106.153.149/update?%s\r\n", data);
					http_post("http://184.106.153.149/update", data, "Content-Type: application/x-www-form-urlencoded\r\n", thingspeak_http_callback);*/

					if (MQTT_state[mqttClient.connState] == "MQTT_DATA"){
						bool no_error = true;
						no_error &= MQTT_Publish(&mqttClient, "/mqtt/temperature", temp, 5, 2, 1); //retain message so that if someone subcribes after publication, value is known
						no_error &= MQTT_Publish(&mqttClient, "/mqtt/humidity", hum, 5, 2, 1);
						no_error &= MQTT_Publish(&mqttClient, "/mqtt/heatIndex", heatIdx, 5, 2, 1);
						if (no_error){
							DHT22_DEBUG("No errors initiating publish \r\n");
						}
					} else{
						DHT22_DEBUG("MQTT has a problem, state is: %s\r\n", MQTT_state[mqttClient.connState]);
					}

			} else {
				DHT22_DEBUG("Error reading temperature and humidity.\r\n");
			}
			break;
		default:
			DHT22_DEBUG("WiFi not connected...\r\n");
	}
	os_timer_setfn(&dht22_timer, (os_timer_func_t *)dht22_cb, (void *)0);
	os_timer_arm(&dht22_timer, DATA_SEND_DELAY, 1);

	os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
	os_timer_arm(&WiFiLinker, WIFI_CHECK_DELAY, 0);
}

static void ICACHE_FLASH_ATTR wifi_check_ip(void *arg)
{
	os_timer_disarm(&WiFiLinker);
	switch(wifi_station_get_connect_status())
	{
		case STATION_GOT_IP:
			wifi_get_ip_info(STATION_IF, &ipConfig);
			if(ipConfig.ip.addr != 0) {
				connState = WIFI_CONNECTED;
				//DHT22_DEBUG("WiFi connected, wait DHT22 timer...\r\n");
				if (firstTime){
					MQTT_Connect(&mqttClient);
					//while (MQTT_state[mqttClient.connState] != "TCP_CONNECTED"){system_soft_wdt_feed();} //wait for tcp connection
					//DHT22_DEBUG("MQTT client connection state: %s\r\n", MQTT_state[mqttClient.connState]);
					firstTime = false;
				}
			} else {
				connState = WIFI_CONNECTING_ERROR;
				DHT22_DEBUG("WiFi connected, ip.addr is null\r\n");
			}
			break;
		case STATION_WRONG_PASSWORD:
			connState = WIFI_CONNECTING_ERROR;
			DHT22_DEBUG("WiFi connecting error, wrong password\r\n");
			break;
		case STATION_NO_AP_FOUND:
			connState = WIFI_CONNECTING_ERROR;
			DHT22_DEBUG("WiFi connecting error, ap not found\r\n");
			break;
		case STATION_CONNECT_FAIL:
			connState = WIFI_CONNECTING_ERROR;
			DHT22_DEBUG("WiFi connecting fail\r\n");
			break;
		default:
			connState = WIFI_CONNECTING;
			DHT22_DEBUG("WiFi connecting...\r\n");
	}
	os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
	os_timer_arm(&WiFiLinker, WIFI_CHECK_DELAY, 0);
}

LOCAL void ICACHE_FLASH_ATTR wifi_show_scan_info(void *arg, STATUS status){
	DHT22_DEBUG("\n==== Avaliable Networks: ====\n");
	if (status == OK){
		struct bss_info *bssInfo;
		bssInfo = (struct bss_info *)arg;
		//skip first in chain as it is invalid
		bssInfo = STAILQ_NEXT(bssInfo, next);
		while (bssInfo != NULL){
			DHT22_DEBUG("SSID: %s\r\n", bssInfo->ssid);
			DHT22_DEBUG("SECURITY: %s\r\n", authMapping[bssInfo->authmode]);
			DHT22_DEBUG("RSSI: %d dB\r\n\n", bssInfo->rssi);
			bssInfo = STAILQ_NEXT(bssInfo, next);
		}

		DHT22_DEBUG("Scan done, setting DHT11 and WiFi check timers...\r\n");
		// Wait for Wi-Fi connection
		os_timer_disarm(&WiFiLinker);
		os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
		os_timer_arm(&WiFiLinker, WIFI_CHECK_DELAY, 0);

		// Set up a timer to send the message
		os_timer_disarm(&dht22_timer);
		os_timer_setfn(&dht22_timer, (os_timer_func_t *)dht22_cb, (void *)0);
		os_timer_arm(&dht22_timer, DATA_SEND_DELAY, 1);
	}else{
		DHT22_DEBUG("There is a problem scanning nearby networks \r\n");
		DHT22_DEBUG("Status is: %s \r\n", statusMapping[status]);
	}
}

LOCAL void ICACHE_FLASH_ATTR to_scan(void) { wifi_station_scan(NULL,wifi_show_scan_info); }

LOCAL bool ICACHE_FLASH_ATTR setup_wifi_st_mode()
{
	//wifi_set_opmode(STATION_MODE);
	//WifiCallback wifiCb = cb;
	struct station_config stconfig;
	wifi_station_disconnect();
	wifi_station_dhcpc_stop();
	if(wifi_station_get_config(&stconfig))
	{
		os_memset(stconfig.ssid, 0, sizeof(stconfig.ssid));
		os_memset(stconfig.password, 0, sizeof(stconfig.password));
		os_sprintf(stconfig.ssid, "%s", WIFI_CLIENTSSID);
		os_sprintf(stconfig.password, "%s", WIFI_CLIENTPASSWORD);
		if(!wifi_station_set_config(&stconfig))
		{
			DHT22_DEBUG("ESP8266 not set station config!\r\n");
			return false;
		}
	}
	wifi_station_connect();
	wifi_station_dhcpc_start();
	//wifi_station_set_auto_connect(1);
	DHT22_DEBUG("ESP8266 in STA mode configured.\r\n");
	return true;
}

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABBBCDDD
 *                A : rf cal
 *                B : at parameters
 *                C : rf init data
 *                D : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
*******************************************************************************/
 uint32 ICACHE_FLASH_ATTR user_rf_cal_sector_set(void)
{
    enum flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 8;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;

        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
} 

void ICACHE_FLASH_ATTR user_rf_pre_init(void)
{
}

void ICACHE_FLASH_ATTR user_init(void)
{
	// Configure the UART
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	// Enable system messages
	system_set_os_print(1);

	//To print available networks
	wifi_set_opmode(STATION_MODE);
	system_init_done_cb(to_scan);
	//os_delay_us(20000);

	DHT22_DEBUG("\n==== System info: ====\n");
	DHT22_DEBUG("SDK version:%s rom %d\n", system_get_sdk_version(), system_upgrade_userbin_check());
	DHT22_DEBUG("Time = %ld\n", system_get_time());
	DHT22_DEBUG("Chip id = 0x%x\n", system_get_chip_id());
	DHT22_DEBUG("CPU freq = %d MHz\n", system_get_cpu_freq());
	//DHT22_DEBUG("Flash size map = %s\n", system_get_flash_size_map()); //doesn't work for some reason
	DHT22_DEBUG("Free heap size = %d\n", system_get_free_heap_size());
	DHT22_DEBUG("==== End System info ====\n");
	DHT22_DEBUG("==== MQTT client setup ====\n");
	MQTT_InitConnection(&mqttClient, MQTT_HOST, MQTT_PORT, DEFAULT_SECURITY);
	DHT22_DEBUG("MQTT settings:\r\n Host: %s\r\n Port: %d\r\n Security: %d\r\n", MQTT_HOST, MQTT_PORT, DEFAULT_SECURITY);
	MQTT_InitClient(&mqttClient, MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS, MQTT_KEEPALIVE, 1);
	DHT22_DEBUG("MQTT client settings:\r\n Device ID: %s\r\n MQTT_User: %s\r\n MQTT_Password: %s\r\n MQTT_Keepalive: %d\r\n Uses clean session\r\n", MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS, MQTT_KEEPALIVE);
	MQTT_InitLWT(&mqttClient, "lwt", "offline", 0, 0); //last will topic
	MQTT_OnConnected(&mqttClient, mqttConnectedCb);
	MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
	MQTT_OnPublished(&mqttClient, mqttPublishedCb);
	MQTT_OnData(&mqttClient, mqttDataCb);
	DHT22_DEBUG("==== End MQTT client setup ====\n");
	os_delay_us(10000);
	DHT22_DEBUG("System init...\r\n");
	if(setup_wifi_st_mode()){
		if(wifi_get_phy_mode() != PHY_MODE_11N)
			wifi_set_phy_mode(PHY_MODE_11N);
		if(wifi_station_get_auto_connect() == 0)
			wifi_station_set_auto_connect(1);
		wifi_station_set_reconnect_policy(TRUE);
	}
	// Init DHT22 sensor
	DHT22_DEBUG("Initializing DHT11 sensor...\r\n");
	DHTInit(DHT11);

	// Wait for Wi-Fi connection
//	os_timer_disarm(&WiFiLinker);
//	os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
//	os_timer_arm(&WiFiLinker, WIFI_CHECK_DELAY, 0);
//
//	// Set up a timer to send the message
//	os_timer_disarm(&dht22_timer);
//	os_timer_setfn(&dht22_timer, (os_timer_func_t *)dht22_cb, (void *)0);
//	os_timer_arm(&dht22_timer, DATA_SEND_DELAY, 1);

	DHT22_DEBUG("System init done.\n");
}
