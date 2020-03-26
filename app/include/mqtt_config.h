#ifndef __MQTT_CONFIG_H__
#define __MQTT_CONFIG_H__

/*DEFAULT CONFIGURATIONS*/

#define MQTT_HOST			"192.168.1.101"       //HOST or "mqtt.yourdomain.com"
#define MQTT_PORT			3863
#define MQTT_USER            "user123"
#define MQTT_PASS            "pass123"

#define MQTT_BUF_SIZE		2048
#define MQTT_KEEPALIVE		120	            /*second*/

//产品信息
//后台产品管理中产品信息里获取 Access Key
#define ACCESSKEY "xxx"
//后台产品管理中产品信息里获取 Secret Key
#define SECRETKEY "xxx"
//后台产品序列号自行生成，这个sn是每台设备都不同的，所以设备第一次启动时都存入flash，之后一直都是读flash里的sn，这样OTA升级都不会做成影响
#define SN "xxx" //DC1

#define MQTT_RECONNECT_TIMEOUT 	5	        /*second*/

#define DEFAULT_SECURITY	0
#define QUEUE_BUFFER_SIZE		 		2048

#define PROTOCOL_NAMEv31	                /*MQTT version 3.1 compatible with Mosquitto v0.15*/
//#define PROTOCOL_NAMEv311			        /*MQTT version 3.11 compatible with https://eclipse.org/paho/clients/testing/*/

#endif // __MQTT_CONFIG_H__
