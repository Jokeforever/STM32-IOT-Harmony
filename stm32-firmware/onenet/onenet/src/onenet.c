#include "app_config.h"
#include "app_sensors.h"
/*
 * MODULE OVERVIEW
 * 文件：onenet.c
 * 作用：OneNET MQTT 鉴权、属性上报、属性下发和 set_reply。
 * 关键函数：
 *   OneNet_DevLink()    MQTT 登录
 *   OneNet_FillBuf()    组装 OneJSON 属性
 *   OneNet_RevPro()     处理 property/set 下发
 * 注意：
 *   led、bump、阈值和 PID 参数变化后会触发 EEPROM 保存。
 *   sensor_status 只上报 DHT11 和 CO2 的有效性。
 */
/**
	************************************************************
	************************************************************
	************************************************************
	*	文件名： 	onenet.c
	*
	*	作者： 		张继瑞
	*
	*	日期： 		2017-05-08
	*
	*	版本： 		V1.1
	*
	*	说明： 		与onenet平台的数据交互接口层
	*
	*	修改记录：	V1.0：协议封装、返回判断都在同一个文件，并且不同协议接口不同。
	*				V1.1：提供统一接口供应用层使用，根据不同协议文件来封装协议相关的内容。
	************************************************************
	************************************************************
	************************************************************
**/

//单片机头文件
#include "stm32f1xx_hal.h"

//网络设备
#include "esp8266.h"

//协议文件
#include "onenet.h"
#include "onenet_command.h"
#include "mqttkit.h"

//算法
#include "base64.h"
#include "hmac_sha1.h"

//硬件驱动
#include "usart.h"
#include "myusart.h"
//#include "sht20.h"
#include "oled.h"
#include "delay.h"
//C库
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define PROID			"OCuh518nh5"


#define ACCESS_KEY		"YOUR_DEVICE_ACCESS_KEY"


#define DEVICE_NAME		"SA1"

#define HMAC_SHA1_SIZE 20
#define ONENET_JSON_BUF_SIZE 512
static unsigned int one_net_msg_id = 1;


char devid[16];

char key[48];

static _Bool one_net_connected = 0;


extern unsigned char esp8266_buf[512];


/*
************************************************************
*	函数名称：	OTA_UrlEncode
*
*	函数功能：	sign需要进行URL编码
*
*	入口参数：	sign：加密结果
*
*	返回参数：	0-成功	其他-失败
*
*	说明：		+			%2B
*				空格		%20
*				/			%2F
*				?			%3F
*				%			%25
*				#			%23
*				&			%26
*				=			%3D
************************************************************
*/
static unsigned char OTA_UrlEncode(char *sign)
{

	char sign_t[40];
	unsigned char i = 0, j = 0;
	unsigned char sign_len = strlen(sign);
	
	if(sign == (void *)0 || sign_len < 28)
		return 1;
	
	for(; i < sign_len; i++)
	{
		sign_t[i] = sign[i];
		sign[i] = 0;
	}
	sign_t[i] = 0;
	
	for(i = 0, j = 0; i < sign_len; i++)
	{
		switch(sign_t[i])
		{
			case '+':
				strcat(sign + j, "%2B");j += 3;
			break;
			
			case ' ':
				strcat(sign + j, "%20");j += 3;
			break;
			
			case '/':
				strcat(sign + j, "%2F");j += 3;
			break;
			
			case '?':
				strcat(sign + j, "%3F");j += 3;
			break;
			
			case '%':
				strcat(sign + j, "%25");j += 3;
			break;
			
			case '#':
				strcat(sign + j, "%23");j += 3;
			break;
			
			case '&':
				strcat(sign + j, "%26");j += 3;
			break;
			
			case '=':
				strcat(sign + j, "%3D");j += 3;
			break;
			
			default:
				sign[j] = sign_t[i];j++;
			break;
		}
	}
	
	sign[j] = 0;
	
	return 0;

}

/*
************************************************************
*	函数名称：	OTA_Authorization
*
*	函数功能：	计算Authorization
*
*	入口参数：	ver：参数组版本号，日期格式，目前仅支持格式"2018-10-31"
*				res：产品id
*				et：过期时间，UTC秒值
*				access_key：访问密钥
*				dev_name：设备名
*				authorization_buf：缓存token的指针
*				authorization_buf_len：缓存区长度(字节)
*
*	返回参数：	0-成功	其他-失败
*
*	说明：		当前仅支持sha1
************************************************************
*/
#define METHOD		"sha1"
//static unsigned char OneNET_Authorization(char *ver, char *res, unsigned int et, char *access_key, char *dev_name,
//											char *authorization_buf, unsigned short authorization_buf_len, _Bool flag)
//{
//	
//	size_t olen = 0;
	size_t key_len = 0;
//	
//	char sign_buf[64];								//保存签名的Base64编码结果 和 URL编码结果
//	char hmac_sha1_buf[64];							//保存签名
//	char access_key_base64[64];						//保存access_key的Base64编码结合
//	char string_for_signature[72];					//保存string_for_signature，这个是加密的key


////----------------------------------------------------参数合法性--------------------------------------------------------------------
//	if(ver == (void *)0 || res == (void *)0 || et < 1564562581 || access_key == (void *)0
//		|| authorization_buf == (void *)0 || authorization_buf_len < 120)
//		return 1;
//	
////----------------------------------------------------将access_key进行Base64解码----------------------------------------------------
//	memset(access_key_base64, 0, sizeof(access_key_base64));
//	
//	BASE64_Decode((unsigned char *)access_key_base64, sizeof(access_key_base64), &key_len, (unsigned char *)access_key, strlen(access_key));
//	//UsartPrintf(USART_DEBUG, "access_key_base64: %s\r\n", access_key_base64);
//															OLED_Clear();
//	OLED_ShowString(0,0," 2",16);
//	
////----------------------------------------------------计算string_for_signature-----------------------------------------------------
//	memset(string_for_signature, 0, sizeof(string_for_signature));
//	if(flag)
//		snprintf(string_for_signature, sizeof(string_for_signature), "%d\n%s\nproducts/%s\n%s", et, METHOD, res, ver);
//	else
//		snprintf(string_for_signature, sizeof(string_for_signature), "%d\n%s\nproducts/%s/devices/%s\n%s", et, METHOD, res, dev_name, ver);
//	//UsartPrintf(USART_DEBUG, "string_for_signature: %s\r\n", string_for_signature);
//	
////----------------------------------------------------加密-------------------------------------------------------------------------
//	memset(hmac_sha1_buf, 0, sizeof(hmac_sha1_buf));

//	hmac_sha1((unsigned char *)access_key_base64, key_len,
//				(unsigned char *)string_for_signature, strlen(string_for_signature),
//				(unsigned char *)hmac_sha1_buf);


//	UsartPrintf(USART_DEBUG, "hmac_sha1_buf: %s\r\n", hmac_sha1_buf);


////----------------------------------------------------将加密结果进行Base64编码------------------------------------------------------
//	olen = 0;
//	memset(sign_buf, 0, sizeof(sign_buf));

//	
//	BASE64_Encode((unsigned char *)sign_buf, sizeof(sign_buf), &olen, (unsigned char *)hmac_sha1_buf, HMAC_SHA1_SIZE);

////----------------------------------------------------将Base64编码结果进行URL编码---------------------------------------------------
//	OTA_UrlEncode(sign_buf);
//	UsartPrintf(USART_DEBUG, "sign_buf: %s\r\n", sign_buf);

//	
////----------------------------------------------------计算Token--------------------------------------------------------------------
//	if(flag)
//		snprintf(authorization_buf, authorization_buf_len, "version=%s&res=products%%2F%s&et=%d&method=%s&sign=%s", ver, res, et, METHOD, sign_buf);
//	else
//		snprintf(authorization_buf, authorization_buf_len, "version=%s&res=products%%2F%s%%2Fdevices%%2F%s&et=%d&method=%s&sign=%s", ver, res, dev_name, et, METHOD, sign_buf);
//	UsartPrintf(USART_DEBUG, "Token: %s\r\n", authorization_buf);

//	return 0;

//}
static unsigned char OneNET_Authorization(char *ver, char *res, unsigned int et, char *access_key, char *dev_name,
											char *authorization_buf, unsigned short authorization_buf_len, _Bool flag)
{
	
	size_t olen = 0;
	size_t key_len = 0;
	
	char sign_buf[64];								//淇濆瓨绛惧悕鐨凚ase64缂栫爜缁撴灉 鍜?URL缂栫爜缁撴灉
	char hmac_sha1_buf[64];							//淇濆瓨绛惧悕
	char access_key_base64[64];						//淇濆瓨access_key鐨凚ase64缂栫爜缁撳悎
	char string_for_signature[72];					//淇濆瓨string_for_signature锛岃繖涓槸鍔犲瘑鐨刱ey

//----------------------------------------------------鍙傛暟鍚堟硶鎬?-------------------------------------------------------------------
	if(ver == (void *)0 || res == (void *)0 || et < 1564562581 || access_key == (void *)0
		|| authorization_buf == (void *)0 || authorization_buf_len < 120)
		return 1;
	
//----------------------------------------------------灏哸ccess_key杩涜Base64瑙ｇ爜----------------------------------------------------
	memset(access_key_base64, 0, sizeof(access_key_base64));
	BASE64_Decode((unsigned char *)access_key_base64, sizeof(access_key_base64), &key_len, (unsigned char *)access_key, strlen(access_key));
//	printf("access_key_base64: %s\r\n", access_key_base64);
															
//----------------------------------------------------璁＄畻string_for_signature-----------------------------------------------------
	memset(string_for_signature, 0, sizeof(string_for_signature));
	if(flag)
		snprintf(string_for_signature, sizeof(string_for_signature), "%d\n%s\nproducts/%s\n%s", et, METHOD, res, ver);
	else
		snprintf(string_for_signature, sizeof(string_for_signature), "%d\n%s\nproducts/%s/devices/%s\n%s", et, METHOD, res, dev_name, ver);
//	printf("string_for_signature: %s\r\n", string_for_signature);
	
//----------------------------------------------------鍔犲瘑-------------------------------------------------------------------------
	memset(hmac_sha1_buf, 0, sizeof(hmac_sha1_buf));
	
	hmac_sha1((unsigned char *)access_key_base64, key_len,
				(unsigned char *)string_for_signature, strlen(string_for_signature),
				(unsigned char *)hmac_sha1_buf);

//	printf("hmac_sha1_buf: %s\r\n", hmac_sha1_buf);
	
//----------------------------------------------------灏嗗姞瀵嗙粨鏋滆繘琛孊ase64缂栫爜------------------------------------------------------
	olen = 0;
	memset(sign_buf, 0, sizeof(sign_buf));
	BASE64_Encode((unsigned char *)sign_buf, sizeof(sign_buf), &olen, (unsigned char *)hmac_sha1_buf, HMAC_SHA1_SIZE);
	
//----------------------------------------------------灏咮ase64缂栫爜缁撴灉杩涜URL缂栫爜---------------------------------------------------
	OTA_UrlEncode(sign_buf);
			
//	printf("sign_buf: %s\r\n", sign_buf);
	
//----------------------------------------------------璁＄畻Token--------------------------------------------------------------------
	if(flag)
		snprintf(authorization_buf, authorization_buf_len, "version=%s&res=products%%2F%s&et=%d&method=%s&sign=%s", ver, res, et, METHOD, sign_buf);
	else
		snprintf(authorization_buf, authorization_buf_len, "version=%s&res=products%%2F%s%%2Fdevices%%2F%s&et=%d&method=%s&sign=%s", ver, res, dev_name, et, METHOD, sign_buf);
	
	
//	printf("Token: %s\r\n", authorization_buf);
	
	return 0;

}

//==========================================================
//	函数名称：	OneNET_RegisterDevice
//
//	函数功能：	在产品中注册一个设备
//
//	入口参数：	access_key：访问密钥
//				pro_id：产品ID
//				serial：唯一设备号
//				devid：保存返回的devid
//				key：保存返回的key
//
//	返回参数：	0-成功		1-失败
//
//	说明：		
//==========================================================
_Bool OneNET_RegisterDevice(void)
{

	_Bool result = 1;
	unsigned short send_len = 11 + strlen(DEVICE_NAME);
	char *send_ptr = NULL, *data_ptr = NULL;
	
	char authorization_buf[144];													//加密的key
	
	send_ptr = malloc(send_len + 240);
	if(send_ptr == NULL)
		return result;
	
	while(ESP8266_SendCmd("AT+CIPSTART=\"TCP\",\"183.230.40.33\",80\r\n", "CONNECT"))
		delay_ms(500);
	
	OneNET_Authorization("2018-10-31", PROID, 1956499200, ACCESS_KEY, NULL,
							authorization_buf, sizeof(authorization_buf), 1);
	
	snprintf(send_ptr, 280 + send_len, "POST /mqtt/v1/devices/reg HTTP/1.1\r\n"
					"Authorization:%s\r\n"
					"Host:ota.heclouds.com\r\n"
					"Content-Type:application/json\r\n"
					"Content-Length:%d\r\n\r\n"
					"{\"name\":\"%s\"}",
	
					authorization_buf, 11 + strlen(DEVICE_NAME), DEVICE_NAME);
	
	ESP8266_SendData((unsigned char *)send_ptr, strlen(send_ptr));
	
	/*
	{
	  "request_id" : "f55a5a37-36e4-43a6-905c-cc8f958437b0",
	  "code" : "onenet_common_success",
	  "code_no" : "000000",
	  "message" : null,
	  "data" : {
		"device_id" : "589804481",
		"name" : "mcu_id_43057127",
		
	"pid" : 282932,
		"key" : "indu/peTFlsgQGL060Gp7GhJOn9DnuRecadrybv9/XY="
	  }
	}
	*/
	
	data_ptr = (char *)ESP8266_GetIPD(250);							//等待平台响应
	
	if(data_ptr)
	{
		data_ptr = strstr(data_ptr, "device_id");
	}
	
	if(data_ptr)
	{
		char name[16];
		int pid = 0;
		
		if(sscanf(data_ptr, "device_id\" : \"%[^\"]\",\r\n\"name\" : \"%[^\"]\",\r\n\r\n\"pid\" : %d,\r\n\"key\" : \"%[^\"]\"", devid, name, &pid, key) == 4)
		{
			UsartPrintf(USART_DEBUG, "create device: %s, %s, %d, %s\r\n", devid, name, pid, key);
			result = 0;
		}
	}
	
	free(send_ptr);
	ESP8266_SendCmd("AT+CIPCLOSE\r\n", "OK");
	
	return result;

}

//==========================================================
//	函数名称：	OneNet_DevLink
//
//	函数功能：	与onenet创建连接
//
//	入口参数：	无
//
//	返回参数：	1-成功	0-失败
//
//	说明：		与onenet平台建立连接
//==========================================================
_Bool OneNet_DevLink(void)
{
	
	MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};					//协议包

	unsigned char *dataPtr;
	
	char authorization_buf[160];
	
	_Bool status = 1;
	
	OneNET_Authorization("2018-10-31", PROID, 1956499200, ACCESS_KEY, DEVICE_NAME,
								authorization_buf, sizeof(authorization_buf), 0);

//	UsartPrintf(USART_DEBUG, "OneNET_DevLink\r\n"
//							"NAME: %s,	PROID: %s,	KEY:%s\r\n"
//                        , DEVICE_NAME, PROID, authorization_buf);
						
	if(MQTT_PacketConnect(PROID, authorization_buf, DEVICE_NAME, 256, 1, MQTT_QOS_LEVEL0, NULL, NULL, 0, &mqttPacket) == 0)
	{

		ESP8266_SendData(mqttPacket._data, mqttPacket._len);			//上传平台
	
		dataPtr = ESP8266_GetIPD(250);									//等待平台响应
		

		if(dataPtr != NULL)
		{
			if(MQTT_UnPacketRecv(dataPtr) == MQTT_PKT_CONNACK)
			{
				switch(MQTT_UnPacketConnectAck(dataPtr))
				{
					case 0:;status = 0;break;
					
					case 1:;break;
					case 2:;break;
					case 3:;break;
					case 4:;break;
					case 5:;break;
					
					default:;break;
//					case 0:UsartPrintf(USART_DEBUG, "Tips:	杩炴帴鎴愬姛\r\n");status = 0;break;
//					
//					case 1:UsartPrintf(USART_DEBUG, "WARN:	杩炴帴澶辫触锛氬崗璁敊璇痋r\n");break;
//					case 2:UsartPrintf(USART_DEBUG, "WARN:	杩炴帴澶辫触锛氶潪娉曠殑clientid\r\n");break;
//					case 3:UsartPrintf(USART_DEBUG, "WARN:	杩炴帴澶辫触锛氭湇鍔″櫒澶辫触\r\n");break;
//					case 4:UsartPrintf(USART_DEBUG, "WARN:	杩炴帴澶辫触锛氱敤鎴峰悕鎴栧瘑鐮侀敊璇痋r\n");break;
//					case 5:UsartPrintf(USART_DEBUG, "WARN:	杩炴帴澶辫触锛氶潪娉曢摼鎺?姣斿token闈炴硶)\r\n");break;
//					
//					default:UsartPrintf(USART_DEBUG, "ERR:	杩炴帴澶辫触锛氭湭鐭ラ敊璇痋r\n");break;
				}
			}
		}

		MQTT_DeleteBuffer(&mqttPacket);						//删包

	}
//	else
////		UsartPrintf(USART_DEBUG, "WARN:	MQTT_PacketConnect Failed\r\n");
	
	one_net_connected = (status == 0);
	return status;
	
}

extern float water_vol;

static int OneNet_Append(char *buf, int size, int offset, const char *fmt, ...)
{
	va_list ap;
	int remain;
	int n;

	if (offset < 0)
	{
		offset = 0;
	}

	if (offset >= size - 1)
	{
		return size - 1;
	}

	remain = size - offset;
	va_start(ap, fmt);
	n = vsnprintf(buf + offset, remain, fmt, ap);
	va_end(ap);

	if (n < 0)
	{
		return offset;
	}

	if (n >= remain)
	{
		return size - 1;
	}

	return offset + n;
}

unsigned short OneNet_FillBuf(char *buf)
{
	const SensorData *sensors = SensorService_GetData();
	int len = 0;

	len = OneNet_Append(buf, ONENET_JSON_BUF_SIZE, len,
		"{\"id\":\"%u\",\"version\":\"1.0\",\"params\":{", one_net_msg_id++);
	len = OneNet_Append(buf, ONENET_JSON_BUF_SIZE, len,
		"\"temp\":{\"value\":%d},", sensors->temperature_c);
	len = OneNet_Append(buf, ONENET_JSON_BUF_SIZE, len,
		"\"humi\":{\"value\":%d},", sensors->humidity_pct);
	len = OneNet_Append(buf, ONENET_JSON_BUF_SIZE, len,
		"\"light\":{\"value\":%d},", sensors->light_lux);
	len = OneNet_Append(buf, ONENET_JSON_BUF_SIZE, len,
		"\"ppm\":{\"value\":%d},", sensors->co2_ppm);
	len = OneNet_Append(buf, ONENET_JSON_BUF_SIZE, len,
		"\"water_vol\":{\"value\":%.1f},", water_vol);
	len = OneNet_Append(buf, ONENET_JSON_BUF_SIZE, len,
		"\"led\":{\"value\":%d},", (int)AppConfig_Get()->led_mode);
	len = OneNet_Append(buf, ONENET_JSON_BUF_SIZE, len,
		"\"bump\":{\"value\":%d},", (int)AppConfig_Get()->bump_mode);
	len = OneNet_Append(buf, ONENET_JSON_BUF_SIZE, len,
		"\"CO2threhold\":{\"value\":%d},", AppConfig_Get()->co2_threshold);
	len = OneNet_Append(buf, ONENET_JSON_BUF_SIZE, len,
		"\"droughtthrehold\":{\"value\":%d},", AppConfig_Get()->soil_low);
	len = OneNet_Append(buf, ONENET_JSON_BUF_SIZE, len,
		"\"moistthrehold\":{\"value\":%d},", AppConfig_Get()->soil_high);
	len = OneNet_Append(buf, ONENET_JSON_BUF_SIZE, len,
		"\"tempthrehold\":{\"value\":%d},", AppConfig_Get()->temp_threshold);
	len = OneNet_Append(buf, ONENET_JSON_BUF_SIZE, len,
		"\"sensor_status\":{\"value\":%d},", (int)sensors->valid_mask);
	len = OneNet_Append(buf, ONENET_JSON_BUF_SIZE, len,
		"\"moist\":{\"value\":%d}", sensors->soil_moisture_pct);
	len = OneNet_Append(buf, ONENET_JSON_BUF_SIZE, len, "}}");

	if (len <= 0 || len >= ONENET_JSON_BUF_SIZE)
	{
		return 0;
	}

	return (unsigned short)len;
}
//==========================================================
//	函数名称：	OneNet_SendData
//
//	函数功能：	上传数据到平台
//
//	入口参数：	type：发送数据的格式
//
//	返回参数：	无
//
//	说明：		
//==========================================================
_Bool OneNet_SendData(void)
{
	
	MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};
	char buf[ONENET_JSON_BUF_SIZE];
	short body_len = 0, i = 0;

	memset(buf, 0, sizeof(buf));

	body_len = OneNet_FillBuf(buf);
	if(body_len <= 0)
	{
		one_net_connected = 0;
		return 1;
	}

	if(MQTT_PacketSaveData(PROID, DEVICE_NAME, body_len, NULL, &mqttPacket) != 0)
	{
		one_net_connected = 0;
		return 1;
	}

	for(; i < body_len; i++)
	{
		mqttPacket._data[mqttPacket._len++] = buf[i];
	}

	if(ESP8266_SendData(mqttPacket._data, mqttPacket._len) != 0)
	{
		MQTT_DeleteBuffer(&mqttPacket);
		one_net_connected = 0;
		return 1;
	}

	MQTT_DeleteBuffer(&mqttPacket);
	return 0;
}
//==========================================================
//	函数名称：	OneNET_Publish
//
//	函数功能：	发布消息
//
//	入口参数：	topic：发布的主题
//				msg：消息内容
//
//	返回参数：	无
//
//	说明：		
//==========================================================
void OneNET_Publish(const char *topic, const char *msg)
{

	MQTT_PACKET_STRUCTURE mqtt_packet = {NULL, 0, 0, 0};						//协议包
	
	UsartPrintf(USART_DEBUG, "Publish Topic: %s, Msg: %s\r\n", topic, msg);
	
	if(MQTT_PacketPublish(MQTT_PUBLISH_ID, topic, msg, strlen(msg), MQTT_QOS_LEVEL0, 0, 1, &mqtt_packet) == 0)
	{
		if(ESP8266_SendData(mqtt_packet._data, mqtt_packet._len) != 0)
		{
			one_net_connected = 0;
		}
		
		MQTT_DeleteBuffer(&mqtt_packet);										//删包
	}

}

//==========================================================
//	函数名称：	OneNET_Subscribe
//
//	函数功能：	订阅
//
//	入口参数：	无
//
//	返回参数：	无
//
//	说明：		
//==========================================================
void OneNET_Subscribe(void)
{

	MQTT_PACKET_STRUCTURE mqtt_packet = {NULL, 0, 0, 0};						//协议包
		
	char topic_buf[60];
	const char *topic = topic_buf;
		
	snprintf(topic_buf, sizeof(topic_buf), "$sys/%s/%s/thing/property/set", PROID, DEVICE_NAME);
	
//	UsartPrintf(USART_DEBUG, "Subscribe Topic: %s\r\n", topic_buf);
	
	if(MQTT_PacketSubscribe(MQTT_SUBSCRIBE_ID, MQTT_QOS_LEVEL0, &topic, 1, &mqtt_packet) == 0)
	{
		if(ESP8266_SendData(mqtt_packet._data, mqtt_packet._len) != 0)
		{
			one_net_connected = 0;
		}
		
		MQTT_DeleteBuffer(&mqtt_packet);										//删包
	}


}

_Bool OneNet_IsConnected(void)
{
	return one_net_connected;
}

//==========================================================
//	函数名称：	OneNet_RevPro
//
//	函数功能：	平台返回数据检测
//
//	入口参数：	dataPtr：平台返回的数据
//
//	返回参数：	无
//
//	说明：		
//==========================================================
void OneNet_RevPro(unsigned char *cmd)
{
	
	char *req_payload = NULL;
	char *cmdid_topic = NULL;
	
	unsigned short topic_len = 0;
	unsigned short req_len = 0;
	
	unsigned char qos = 0;
	static unsigned short pkt_id = 0;
	
	unsigned char type = 0;
	
	short result = 0;

//	char *dataPtr = NULL;
//	char numBuf[10];
//	int num = 0;
	
	const char *reply_topic = "$sys/" PROID "/" DEVICE_NAME "/thing/property/set_reply";
	const char *expected_topic = "$sys/" PROID "/" DEVICE_NAME "/thing/property/set";
	char reply_payload[160];
	char topic_buf[80];
	char payload_buf[256];

	type = MQTT_UnPacketRecv(cmd);
	switch(type)
	{
		case MQTT_PKT_PUBLISH:
		
			result = MQTT_UnPacketPublish(cmd, &cmdid_topic, &topic_len, &req_payload, &req_len, &qos, &pkt_id);
			if(result == 0 && cmdid_topic != NULL && req_payload != NULL)
			{
                /* 拒绝超长完整帧，不把截断JSON当成控制命令。 */
                if (topic_len < sizeof(topic_buf) && req_len < sizeof(payload_buf) &&
                    memchr(cmdid_topic, '\0', topic_len) == NULL && memchr(req_payload, '\0', req_len) == NULL)
                {
                    memcpy(topic_buf, cmdid_topic, topic_len); topic_buf[topic_len] = '\0';
                    memcpy(payload_buf, req_payload, req_len); payload_buf[req_len] = '\0';
                    if (strcmp(topic_buf, expected_topic) == 0 &&
                        OneNetCommand_Handle(payload_buf, reply_payload, sizeof(reply_payload)))
                    {
                        OneNET_Publish(reply_topic, reply_payload);
                    }
                }
			}
			break;
case MQTT_PKT_PUBACK:														
		
			if(MQTT_UnPacketPublishAck(cmd) == 0)

			
		break;
		
		case MQTT_PKT_SUBACK:															
		
			if(MQTT_UnPacketSubscribe(cmd) == 0)
				;
			else
				;
		
		break;
		
		default:
			result = -1;
		break;
	}
	
	ESP8266_Clear();
	
	if(result == -1)
		return;
	



	
	if(type == MQTT_PKT_CMD || type == MQTT_PKT_PUBLISH)
	{
		MQTT_FreeBuffer(cmdid_topic);
		MQTT_FreeBuffer(req_payload);
	}

}
