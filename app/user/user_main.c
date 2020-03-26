#include "ets_sys.h"
#include "os_type.h"
#include "c_types.h"
#include "osapi.h"
#include "mem.h"
#include "gpio.h"
#include "user_interface.h"

#include "mqtt/mqtt.h"
#include "driver/uart.h"
#include "driver/wifi.h"
#include "driver/ota.h"
#include "driver/dc1.h"
#include "driver/md5.h"

#define MAIN_DEBUG_ON
#if defined(MAIN_DEBUG_ON)
#define INFO( format, ... ) os_printf( format, ## __VA_ARGS__ )
#else
#define INFO( format, ... )
#endif

bool isRegister = false;

#define SENDINFO_TIME							10000

u8 md5_password[36];

u8 mac_str[18];									//mac地址

u8 spi_size_map=0;

MQTT_Client mqttClient;
bool smartconfig_mode = false;
os_timer_t OS_Timer_LED;
os_timer_t OS_Timer_SendInfo;

os_timer_t OS_Timer_Heartbeat; //登录成功心跳

char pinNumStatus[] = {false, false, false, false};

bool isLogoLed = true;
bool isWifiLed = true;

/*
 * function: hmac_md5
 * description: hmacmd5测试，成功
 */
void ICACHE_FLASH_ATTR
hmac_md5(void) {

	#define DAXUNYUN_KEY	"daxunyun_secret"

	u8 input[8];

	os_sprintf(input,"%08d", os_random() % 99999999);
	INFO("input:%s\n", input);

	int i;
	u8 md5output[17];
	HMAC_MD5(input, os_strlen(input), DAXUNYUN_KEY, md5output);

	INFO("Digest: ");
	for (i = 0; i < 16; i++) {
		if (md5output[i] < 0x10) {
			INFO("%d", 0);
		}
		INFO("%x", md5output[i]);

		os_sprintf(&md5_password[i * 2], "%02x", (unsigned int)md5output[i]);
	}
	INFO("\n");

	INFO("md5_password:%s\n",md5_password);
}

char ICACHE_FLASH_ATTR *substr(char* srcstr, int offset, int length)
{
    int total_length = os_strlen(srcstr);
    int real_length = ((total_length - offset) >= length ? length : (total_length - offset)) + 1;
    char *tmp;
    if (NULL == (tmp=(char*) os_malloc(real_length * sizeof(char))))
    {
        INFO("Memory overflow. \n");
        return "";
    }
    os_strncpy(tmp, srcstr+offset, real_length - 1);
    tmp[real_length - 1] = '\0';
    return tmp;
}

void ICACHE_FLASH_ATTR substring(char *dest, const char *src, int start, int end)
{
    int i = start;
    
    if (start > os_strlen(src))
    {
        return;
    }
    
    if (end > os_strlen(src))
    {
        end = os_strlen(src);
    }
    
    while (i < end)
    {
        dest[i - start] = src[i];
        i ++;
    }
    
    dest[i - start] = '\0';
}

void ICACHE_FLASH_ATTR InvertUint8(unsigned char *DesBuf, unsigned char *SrcBuf)
{
    int i;
    unsigned char temp = 0;
    
    for(i = 0; i < 8; i++)
    {
        if(SrcBuf[0] & (1 << i))
        {
            temp |= 1<<(7-i);
        }
    }
    DesBuf[0] = temp;
}

void ICACHE_FLASH_ATTR InvertUint16(unsigned short *DesBuf, unsigned short *SrcBuf)
{
    int i;
    unsigned short temp = 0;
    
    for(i = 0; i < 16; i++)
    {
        if(SrcBuf[0] & (1 << i))
        {
            temp |= 1<<(15 - i);
        }
    }
    DesBuf[0] = temp;
}

unsigned short ICACHE_FLASH_ATTR Crc16_modbus(unsigned char *puchMsg, unsigned int usDataLen)
{
    unsigned short wCRCin = 0xFFFF;
    unsigned short wCPoly = 0x8005;
    unsigned char wChar = 0;
    
    while (usDataLen--)
    {
        wChar = *(puchMsg++);
        InvertUint8(&wChar, &wChar);
        wCRCin ^= (wChar << 8);
        
        int i;
        for(i = 0; i < 8; i++)
        {
            if(wCRCin & 0x8000)
            {
                wCRCin = (wCRCin << 1) ^ wCPoly;
            }
            else
            {
                wCRCin = wCRCin << 1;
            }
        }
    }
    InvertUint16(&wCRCin, &wCRCin);
    return (wCRCin) ;
}

int ICACHE_FLASH_ATTR getbitvalue(int n,int _Mask)
{
    _Mask = _Mask -1;
    int a=1;
    a<<=_Mask;
    n&=a;
    if(n == a)
    {
        return 1;
    }
    return 0;
}

uint32 ICACHE_FLASH_ATTR user_rf_cal_sector_set(void) {
	enum flash_size_map size_map = system_get_flash_size_map();
	uint32 rf_cal_sec = 0;
    
    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            priv_param_start_sec = 0x3C;
            break;
            
        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            priv_param_start_sec = 0x7C;
            spi_size_map=2;
            break;
            
        case FLASH_SIZE_16M_MAP_512_512:
            rf_cal_sec = 512 - 5;
            priv_param_start_sec = 0x7C;
            spi_size_map=3;
            break;
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            priv_param_start_sec = 0xFC;
            spi_size_map=5;
            break;
            
        case FLASH_SIZE_32M_MAP_512_512:
            rf_cal_sec = 1024 - 5;
            priv_param_start_sec = 0x7C;
            spi_size_map=4;
            break;
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            priv_param_start_sec = 0xFC;
            spi_size_map=6;
            break;
            
        case FLASH_SIZE_64M_MAP_1024_1024:
            rf_cal_sec = 2048 - 5;
            priv_param_start_sec = 0xFC;
            break;
        case FLASH_SIZE_128M_MAP_1024_1024:
            rf_cal_sec = 4096 - 5;
            priv_param_start_sec = 0xFC;
            break;
        default:
            rf_cal_sec = 0;
            priv_param_start_sec = 0;
            break;
    }
    
    return rf_cal_sec;
}


/**
 * 	smartconfig配置回调
 */
void smartconfig_cd(sm_status status){

	switch (status)
	{
		case SM_STATUS_FINISH:
			INFO("smartconfig_finish\n");

			int manner = 1; //注册模式

			spi_flash_erase_sector(dev_manner_sec);
			if(spi_flash_write(dev_manner_sec * SPI_FLASH_SEC_SIZE, (uint32 *)&manner, 4) == SPI_FLASH_RESULT_OK){
				INFO("manner successfully\r\n");
			}

			hmac_md5();
			
			isRegister = true;

			//system_restart();
			break;
	
		case SM_STATUS_GETINFO:
			INFO("wifiinfo_error\n");
			break;
		case SM_STATUS_TIMEOUT:
			INFO("smartconfig_timeout\n");
			break;
	}
	smartconfig_mode = false;
}

/**
 * 	ota升级回调
 */
void ota_finished_callback(void * arg) {
    struct upgrade_server_info *update = arg;
    os_timer_disarm(&OS_Timer_LED);
    if (update->upgrade_flag == true) {
        INFO("OTA  Success ! rebooting!\n");
        system_upgrade_reboot();
    } else {
        INFO("OTA Failed!\n");
    }
}

void ICACHE_FLASH_ATTR device_status(void){
    char status[64];

     os_strcpy(status, "ffff");
     os_strcat(status, "01");
     os_strcat(status, "0026");
     os_strcat(status, "03");
     os_strcat(status, "aa");
     os_strcat(status, "0b");
     os_strcat(status, "073b");
    
    char logo[3];
    os_sprintf(logo, "%02x",isLogoLed);
    os_strcat(status, logo);
    
    char wifi[3];
    os_sprintf(wifi, "%02x",isWifiLed);
    os_strcat(status, wifi);
    
    os_strcat(status, "00");
    
    char one[3];
    os_sprintf(one, "%02x",pinNumStatus[1]);
    os_strcat(status, one);
    
    char two[3];
    os_sprintf(one, "%02x",pinNumStatus[2]);
    os_strcat(status, one);
    
    char three[3];
    os_sprintf(three, "%02x",pinNumStatus[3]);
    os_strcat(status, three);
    
    os_strcat(status, "00");
    os_strcat(status, "00");
    
    uint16_t electric_data[3];
    get_electric_data(electric_data);
    
    char voltage[5];
    os_sprintf(voltage, "%04x",electric_data[0]);
    os_strcat(status, voltage);
    
    char current[5];
    os_sprintf(current, "%04x",electric_data[1]);
    os_strcat(status, current);
    
    char power[5];
    os_sprintf(power, "%04x",electric_data[2]);
    os_strcat(status, power);
    
    INFO("status : %s - len:%d\n", status,os_strlen(status));
    
    unsigned short crc16_result = Crc16_modbus((unsigned char *)status,os_strlen(status));
    char crc16[5];
    os_sprintf(crc16, "%04x",crc16_result);
    os_strcat(status, crc16);
    
    bool ret = MQTT_Publish(&mqttClient, "devices/msgbound", status, os_strlen(status), 0, 0);
    
    INFO("device_status : %s\n", status);
}

/**
 * 	WIFI连接回调
 */
void wifi_connect_cb(void){

    INFO("wifi connect! version:%s spi_size_map:%d\r\n",FIRMWARE_VERSION,spi_size_map);
	os_timer_disarm(&OS_Timer_LED);
	wifi_led_switch(1);
	MQTT_Connect(&mqttClient);
    
    logo_led_switch(isLogoLed);
    
    //device_status();
}

/**
 * 	WIFI断开回调
 */
void wifi_disconnect_cb(void){
	INFO("wifi disconnect!\r\n");
    os_timer_disarm(&OS_Timer_Heartbeat);//取消定时器
	if(smartconfig_mode != true){
		os_timer_arm(&OS_Timer_LED, 500, 1);
	}
	MQTT_Disconnect(&mqttClient);
}

void ICACHE_FLASH_ATTR heartbeat(void){
    bool ret = MQTT_Publish(&mqttClient, "devices/heartbeat", "ffff01000109", os_strlen("ffff01000109"), 0, 0);
    
    //INFO("devices/heartbeat - ret : %d\n", ret);
}

bool ICACHE_FLASH_ATTR device_login(void) {
    char login[128];
    
    uint8 devUsername[36]={0};
    spi_flash_read(dev_username_sec * SPI_FLASH_SEC_SIZE, (uint32 *)&devUsername, 32);
    
    uint8 devPassword[36]={0};
    spi_flash_read(dev_password_sec * SPI_FLASH_SEC_SIZE, (uint32 *)&devPassword, 32);
    
    os_sprintf(login, "{\"username\":\"%s\",\"password\":\"%s\"}",devUsername,devPassword);
    
    INFO("login : %s\n", login);
    
    bool ret = MQTT_Publish(&mqttClient, "login/device", login, os_strlen(login), 0,0);
    
    INFO("ret : %d\n", ret);
}

bool ICACHE_FLASH_ATTR device_register(void) {
	uint8_t devreg[300];
	uint8 devUsername[36]={0};
    spi_flash_read(dev_username_sec * SPI_FLASH_SEC_SIZE, (uint32 *)&devUsername, 32);
    INFO("devUsername : %s - %d - %d\n", devUsername,sizeof(devUsername),os_strlen(devUsername));

	if (os_strlen(devUsername) != 32)
	{
		os_sprintf(devUsername,"");
	}

	INFO("devUsername : %s - %d - %d\n", devUsername,sizeof(devUsername),os_strlen(devUsername));

	uint8 devPassword[36]={0};
    spi_flash_read(dev_password_sec * SPI_FLASH_SEC_SIZE, (uint32 *)&devPassword, 32);
    INFO("devPassword : %s - %d - %d\n", devPassword,sizeof(devPassword),os_strlen(devPassword));

	if (os_strlen(devPassword) != 32)
	{
		os_sprintf(devPassword,"");
	}

	INFO("devPassword : %s - %d - %d\n", devPassword,sizeof(devPassword),os_strlen(devPassword));
    
    uint8 devSn[24]={0};
    spi_flash_read(dev_sn_sec * SPI_FLASH_SEC_SIZE, (uint32 *)&devSn, 20);
    INFO("devSn :%s - %d,%d,%d,%d\r\n",devSn,os_strlen(devSn),isspace(devSn[0]),ispunct(devSn[0]),devSn[0]);

	os_sprintf(devreg, "{\"accessKey\":\"%s\",\"secretKey\": \"%s\",\"sn\":\"%s\",\"macAddress\":\"%s\",\"password\":\"%s\",\"oldUsername\":\"%s\", \"oldPassword\": \"%s\"}", ACCESSKEY, SECRETKEY, devSn, mac_str, md5_password, devUsername, devPassword);

	INFO("devreg : %s\n", devreg);

	bool ret = MQTT_Publish(&mqttClient, "register/device", devreg, os_strlen(devreg), 0,0);

	INFO("ret : %d\n", ret);
}

bool ICACHE_FLASH_ATTR device_bind(MQTT_Client* client) {
    char bind[300];
    
	int manner = 0;//登录模式

	spi_flash_erase_sector(dev_manner_sec);
	if(spi_flash_write(dev_manner_sec * SPI_FLASH_SEC_SIZE, (uint32 *)&manner, 4) == SPI_FLASH_RESULT_OK){
		INFO("manner successfully\r\n");
	}

	struct station_config config[5];
	wifi_station_get_ap_info(config);

	INFO("ssid : %s\n", config[0].ssid);
	INFO("password : %s\n", config[0].password);

	uint8 phoneip[16]={0};
	spi_flash_read(phone_ip_sec * SPI_FLASH_SEC_SIZE, (uint32 *)&phoneip, sizeof(phoneip));
	INFO("PHONEIP : %s\n", phoneip);

	uint8 devip[16]={0};
	spi_flash_read(dev_ip_sec, (uint32 *)&devip, sizeof(devip));
	INFO("DEVIP : %s\n", devip);
    
    os_sprintf(bind, "{\"ssid\": \"%s\",\"password\":\"%s\",\"deviceip\":\"%s\",\"phoneip\":\"%s\"}",config[0].ssid,config[0].password,devip,phoneip);
    
    INFO("bind : %s\n", bind);
    
    bool ret = MQTT_Publish(&mqttClient, "devices/bind", bind, os_strlen(bind), 0,0);
    
    INFO("ret : %d\n", ret);
}

void ICACHE_FLASH_ATTR device_version(char *useruuid){
    char version[120];
    
    os_sprintf(version, "{\"userUuid\":\"%s\",\"version\":\"%s\"}",useruuid,FIRMWARE_VERSION);
    
    bool ret = MQTT_Publish(&mqttClient, "devices/version", version, os_strlen(version), 0, 0);
    
    //INFO("ret : %d\n", ret);
}



void ICACHE_FLASH_ATTR device_firmware(void){
    char version[64];
    os_sprintf(version, "{\"version\":\"%s\",\"full_url\":\"2\"}",FIRMWARE_VERSION);
    
    bool ret = MQTT_Publish(&mqttClient, "devices/firmware", version, os_strlen(version), 0, 0);
    
    //INFO("ret : %d\n", ret);
}

/**
 * 	MQTT连接回调
 */
void mqttConnectedCb(uint32_t *args) {
	u8 i;
	uint8_t gpio_ret;
	uint8_t switch_bit=0x80;
	MQTT_Client* client = (MQTT_Client*) args;
	INFO("MQTT: Connected\r\n");

	if (isRegister)
	{
		isRegister = false;

		device_register();
		return;
	}

    int manner;
    spi_flash_read(dev_manner_sec * SPI_FLASH_SEC_SIZE, (uint32 *)&manner, 4);
    INFO("manner :%d\r\n",manner);
	if (manner == 0 || manner == 2)
	{
		device_login();
		return;
	}
}


/**
 * 	MQTT断开连接回调
 */
void mqttDisconnectedCb(uint32_t *args) {
    os_timer_disarm(&OS_Timer_Heartbeat);//取消定时器
    os_timer_disarm(&OS_Timer_SendInfo);
	MQTT_Client* client = (MQTT_Client*) args;
	INFO("MQTT: Disconnected\r\n");
}


/**
 * 	MQTT发布消息回调
 */
void mqttPublishedCb(uint32_t *args) {
	MQTT_Client* client = (MQTT_Client*) args;
	//INFO("MQTT: Published\r\n");
}

/**
 * 	MQTT接收数据回调
 */
void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len,
		const char *data, uint32_t data_len) {
	char *topicBuf = (char*) os_zalloc(topic_len + 1), *dataBuf =
			(char*) os_zalloc(data_len + 1);
	
	u8 i;		

	MQTT_Client* client = (MQTT_Client*) args;

	os_memcpy(topicBuf, topic, topic_len);
	topicBuf[topic_len] = 0;

	os_memcpy(dataBuf, data, data_len);
	dataBuf[data_len] = 0;

	INFO("Receive topic: %s, data: %s len:%d\r\n", topicBuf, dataBuf,os_strlen(dataBuf));

	if (os_strcmp(topicBuf,"register/device") == 0) {
		if (os_strlen(dataBuf) == 32)
		{
            INFO("priv_param_start_sec :%02x , %02x , %02x\r\n",priv_param_start_sec,dev_username_sec,dev_sn_sec);
            
			uint8 devUsername[36];
			os_strcpy(devUsername,dataBuf);
			spi_flash_erase_sector(dev_username_sec);
			if(spi_flash_write(dev_username_sec * SPI_FLASH_SEC_SIZE, (uint32 *)&devUsername, 32) == SPI_FLASH_RESULT_OK){
                INFO("dev_username successfully : %s\r\n",devUsername);
			}

			uint8 devUsernames[36]={0};
    		spi_flash_read(dev_username_sec * SPI_FLASH_SEC_SIZE, (uint32 *)&devUsernames, 32);
			INFO("devUsernames :%s - %d\r\n",devUsernames,os_strlen(devUsernames));

			spi_flash_erase_sector(dev_password_sec);
			if(spi_flash_write(dev_password_sec * SPI_FLASH_SEC_SIZE, (uint32 *)&md5_password, 32) == SPI_FLASH_RESULT_OK){
			INFO("dev_password successfully\r\n");
			}

			int manner = 2;//绑定模式

			spi_flash_erase_sector(dev_manner_sec);
			if(spi_flash_write(dev_manner_sec * SPI_FLASH_SEC_SIZE, (uint32 *)&manner, 4) == SPI_FLASH_RESULT_OK){
			INFO("manner successfully\r\n");
			}

			system_restart();

		}

	} else if (os_strcmp(topicBuf,"login/device") == 0) {
		if (os_strcmp(dataBuf,"200") == 0) {
            //heartbeat();
            
            os_timer_disarm(&OS_Timer_Heartbeat);//取消定时器
            os_timer_arm(&OS_Timer_Heartbeat, 20000, true);//使能定时器
            
			int manner;
			spi_flash_read(dev_manner_sec * SPI_FLASH_SEC_SIZE, (uint32 *)&manner, 4);
			
            if (manner == 2)
            //if (os_strcmp(manner,"2") == 0)
            {
                device_bind(client);
            } else {
                os_timer_arm(&OS_Timer_SendInfo, SENDINFO_TIME, 1);
            }
		}

    } else if (os_strcmp(topicBuf,"devices/heartbeat") == 0) {
        
    } else if (os_strcmp(topicBuf,"devices/bind") == 0) {
        if (os_strcmp(dataBuf,"200") == 0) {
            os_timer_arm(&OS_Timer_SendInfo, SENDINFO_TIME, 1);
        }
    } else if (os_strcmp(topicBuf,"devices/version") == 0) {
        if (os_strlen(dataBuf) >= 32) {
            device_version(dataBuf);
        }
    } else if (os_strcmp(topicBuf,"devices/firmware") == 0) {
        if (os_strcmp(dataBuf,"999") == 0) {
            device_firmware();
        } else if (os_strcmp(dataBuf,"104") == 0 || os_strcmp(dataBuf,"105") == 0) {
            //mqtt_stop();
            //esp_restart();
        } else if (os_strlen(dataBuf) > 10) {
            os_timer_disarm(&OS_Timer_SendInfo);
            os_timer_disarm(&OS_Timer_Heartbeat);
            
            ota_upgrade(dataBuf,ota_finished_callback);
        }
    } else if (os_strcmp(topicBuf,"devices/msgevents") == 0 && os_strlen(dataBuf) <= 22) {
        INFO("DATA POINT ERROR!\r\n");
    } else if (os_strstr(topic,"devices/msgevents")) {
        char *endptr;
        
        char crc16[5];
        substring(crc16, dataBuf, os_strlen(dataBuf)-4, os_strlen(dataBuf));
        unsigned short iCrc16 = strtoll(crc16, &endptr, 16);
        
        char msgData[300];
        substring(msgData, dataBuf, 0, os_strlen(dataBuf)-4);
        
        unsigned short crc16_result = Crc16_modbus((unsigned char *)msgData,os_strlen(msgData));
        
        INFO("~~~CRC16_MODBUS:%s,%d,%d \r\n",msgData,crc16_result,iCrc16);

        if (crc16_result != iCrc16) {

            return;
        }
        
        char header[5];
        char ver[3];
        char len[5];
        char cmd[3];
        char msgSn[3];
        char num[3];
        char flags[17];
        char payload[257];
        
        substring(header, dataBuf, 0, 4);
        substring(ver, dataBuf, 4, 6);
        substring(len, dataBuf, 6, 10);
        int iLen = strtol(len, &endptr, 16);
        
        substring(cmd, dataBuf, 10, 12);
        
        //cmd:03是app请求查询状态，只要是03直接返回当前状态即可。当然要根据自身设备指令组合，还要crc16校验
        if (strcmp(cmd,"03") == 0) {
            device_status();
            
            return ;
        }
        
        substring(msgSn, dataBuf, 12, 14);
        substring(num, dataBuf, 14, 16);
        
        int iNum = strtol(num, &endptr, 16);
        int iDataNum = iNum / 4;

        if (iNum % 4 != 0) {
            iDataNum ++;
        }
        if (iDataNum % 2 != 0) {
            iDataNum ++;
        }
        
        substring(flags, dataBuf, 16, 16+iDataNum);
        long long int iFlags = strtoll(flags, &endptr, 16);
        
        substring(payload, dataBuf, 16+iDataNum, os_strlen(dataBuf)-4);
        
        char *bits = (char *)os_malloc(iNum);
        
        INFO("~~iNum:%d iFlags:%lld\r\n", iNum,iFlags);
        
        int i;
        for (i=iNum; i>0; --i) {
            INFO("~~getbitvalue:%d\n", getbitvalue(iFlags, i));

            if (i == iNum) {
                os_sprintf(bits, "%d", getbitvalue(iFlags, i));
            } else {
                os_sprintf(bits, "%s%d", bits, getbitvalue(iFlags, i));
            }
            //00 00 00 00 00 00 00 00 0000 0000 0001
            if (getbitvalue(iFlags, i)) {
                switch (i-1) {
                    case 0://logo led
                    {
                        INFO("~~~case:%d \r\n",i-1);
                        if (os_strcmp(substr(payload, (i-1)*2, 2),"00") == 0) {
                            //关闭
                            INFO("~~~close \r\n");
                            logo_led_switch(false);
                            isLogoLed = false;
                        } else if (os_strcmp(substr(payload, (i-1)*2, 2),"01") == 0) {
                            //打开
                            INFO("~~~open \r\n");
                            logo_led_switch(true);
                            isLogoLed = true;
                        }
                    }
                        break;
                    case 1://wifi led
                    {
                        INFO("~~~case:%d \r\n",i-1);
                        if (os_strcmp(substr(payload, (i-1)*2, 2),"00") == 0) {
                            //关闭
                            INFO("~~~close \r\n");
                            wifi_led_switch(false);
                            isWifiLed = false;
                            isLogoLed = false;
                        } else if (os_strcmp(substr(payload, (i-1)*2, 2),"01") == 0) {
                            //打开
                            INFO("~~~open \r\n");
                            wifi_led_switch(true);
                            isWifiLed = true;
                        }
                    }
                        break;
                    case 2://总开关
                    {
                        INFO("~~~case:%d \r\n",i-1);
                        int y;
                        if (os_strcmp(substr(payload, (i-1)*2, 2),"00") == 0) {
                            //关闭
                            INFO("~~~close \r\n");
                            
                            for (y=0; y<4; y++) {
                                pinNumStatus[y] = false;
                            }
                            set_switch(0,false);
                        } else if (os_strcmp(substr(payload, (i-1)*2, 2),"01") == 0) {
                            //打开
                            INFO("~~~open \r\n");
                            
                            for (y=0; y<4; y++) {
                                pinNumStatus[y] = true;
                            }
                            set_switch(1,true);
                            set_switch(2,true);
                            set_switch(3,true);
                        }
                    }
                        break;
                    case 3://第一开关
                    {
                        INFO("~~~case:%d \r\n",i-1);
                        if (os_strcmp(substr(payload, (i-1)*2, 2),"00") == 0) {
                            //关闭
                            INFO("~~~close \r\n");
                            
                            pinNumStatus[1] = false;
                            set_switch(1,false);
                        } else if (os_strcmp(substr(payload, (i-1)*2, 2),"01") == 0) {
                            //打开
                            INFO("~~~open \r\n");
                            
                            pinNumStatus[1] = true;
                            set_switch(1,true);
                        }
                    }
                        break;
                    case 4://第二个开关
                    {
                        INFO("~~~case:%d \r\n",i-1);
                        if (os_strcmp(substr(payload, (i-1)*2, 2),"00") == 0) {
                            //关闭
                            INFO("~~~close \r\n");
                            
                            pinNumStatus[2] = false;
                            set_switch(2,false);
                        } else if (os_strcmp(substr(payload, (i-1)*2, 2),"01") == 0) {
                            //打开
                            INFO("~~~open \r\n");
                            
                            pinNumStatus[2] = true;
                            set_switch(2,true);
                        }
                    }
                        break;
                    case 5://第三个开关
                    {
                        INFO("~~~case:%d \r\n",i-1);
                        if (os_strcmp(substr(payload, (i-1)*2, 2),"00") == 0) {
                            //关闭
                            INFO("~~~close \r\n");
                            
                            pinNumStatus[3] = false;
                            set_switch(3,false);
                        } else if (os_strcmp(substr(payload, (i-1)*2, 2),"01") == 0) {
                            //打开
                            INFO("~~~open \r\n");
                            
                            pinNumStatus[3] = true;
                            set_switch(3,true);
                        }
                    }
                        break;
                    case 6://小时
                    {
                        INFO("~~~case:%d \r\n",i-1);
                        
                    }
                        break;
                    case 7://分钟
                    {
                        INFO("~~~case:%d \r\n",i-1);
                        
                    }
                        break;
                    default:
                        break;
                }
            }
        }
        
        INFO("header:%s ver:%s len:%s(%d) cmd:%s msgSn:%s num:%s(%d) flags:%s(%s) payload:%s crc16:%s \r\n", header, ver, len, iLen, cmd, msgSn, num, iNum, flags, bits, payload, crc16);
        
        os_free(bits);
    }
 
	os_free(topicBuf);
	os_free(dataBuf);
}

/**
 * 	MQTT初始化
 */
void ICACHE_FLASH_ATTR mqtt_init(void) {

	MQTT_InitConnection(&mqttClient, MQTT_HOST, MQTT_PORT, DEFAULT_SECURITY);
	MQTT_InitClient(&mqttClient, "dc1", MQTT_USER,MQTT_PASS, MQTT_KEEPALIVE, 1);
	MQTT_OnConnected(&mqttClient, mqttConnectedCb);
	MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
	MQTT_OnPublished(&mqttClient, mqttPublishedCb);
	MQTT_OnData(&mqttClient, mqttDataCb);
}


/**
 * 获取MAC
 */
void ICACHE_FLASH_ATTR get_mac(void) {

	u8 i;
	u8 mac[6];
	wifi_get_macaddr(STATION_IF, mac);
	os_sprintf(mac_str,"%02x:%02x:%02x:%02x:%02x:%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
	INFO("mac:%s\n", mac_str);
}


void ICACHE_FLASH_ATTR led_flash(void){
    LOCAL bool status=0;
    wifi_led_switch(status);
    status=~status;
}

void ICACHE_FLASH_ATTR key0_short(bool status){

    u8 i=0;
	if(status){
		//MQTT_Publish(&mqttClient, status_topic[0], "1", os_strlen("1"), 0,1);
	}else{
		for(i = 0; i < 4; i++){
			//MQTT_Publish(&mqttClient, status_topic[i], "0", os_strlen("0"), 0,1);
            pinNumStatus[i] = status;
		}
	}
	
}

void ICACHE_FLASH_ATTR key0_long(void){
    INFO("keyLongPress\n");
	smartconfig_mode = true;
    os_timer_arm(&OS_Timer_LED, 100, 1);
    start_smartconfig(smartconfig_cd);
}

void ICACHE_FLASH_ATTR key1_short(bool status){
    pinNumStatus[1] = status;
    device_status();
}

void ICACHE_FLASH_ATTR key2_short(bool status){
    pinNumStatus[2] = status;
    device_status();
}

void ICACHE_FLASH_ATTR key3_short(bool status){
    pinNumStatus[3] = status;
    device_status();
}

void user_init(void) {

    phone_ip_sec = priv_param_start_sec; //16
    dev_ip_sec = phone_ip_sec  * SPI_FLASH_SEC_SIZE + 64; //16
    dev_username_sec = priv_param_start_sec + 1; //32
    dev_password_sec = priv_param_start_sec + 2; //32
    dev_sn_sec = priv_param_start_sec + 3; //128
    dev_manner_sec = priv_param_start_sec + 4; //1

	// 串口初始化
    UART_SetPrintPort(1);
    uart_init(4800, 115200);
    os_delay_us(60000);
    UART_SetParity(0,EVEN_BITS);
    system_uart_swap();
    set_uart_cb(dc1_uart_data_handler);

	userbin_check();
	get_mac();

	wifi_set_opmode(STATION_MODE); 

	//设置wifi信息存储数量
	wifi_station_ap_number_set(1);
	
	mqtt_init();
	set_wifistate_cb(wifi_connect_cb, wifi_disconnect_cb);

	//按键初始化
	dc1_init(key0_short,key0_long,key1_short,NULL,key2_short,NULL,key3_short,NULL);

    os_timer_disarm(&OS_Timer_LED);
    os_timer_setfn(&OS_Timer_LED, (os_timer_func_t *)led_flash, NULL);
    os_timer_arm(&OS_Timer_LED, 500, 1);

    os_timer_disarm(&OS_Timer_SendInfo);
    os_timer_setfn(&OS_Timer_SendInfo, (os_timer_func_t *)device_status, NULL);
    
    os_timer_disarm(&OS_Timer_Heartbeat);
    os_timer_setfn(&OS_Timer_Heartbeat, (os_timer_func_t *)heartbeat, NULL);
    
    
    uint8 devSn[24]={0};
    spi_flash_read(dev_sn_sec * SPI_FLASH_SEC_SIZE, (uint32 *)&devSn, 20);
    INFO("devSn :%s - %d,%d,%d\r\n",devSn,os_strlen(devSn),isspace(devSn[0]),ispunct(devSn[0]));
    
    if ((os_strlen(devSn) == 0) || devSn[0] == 255) {
        INFO("devSn is null\n");
        
        uint8 devSn[] = SN;
        spi_flash_erase_sector(dev_sn_sec);
        if(spi_flash_write(dev_sn_sec * SPI_FLASH_SEC_SIZE, (uint32 *)&devSn, 20) == SPI_FLASH_RESULT_OK){
            INFO("dev_sn successfully\r\n");
        }
    }
}

