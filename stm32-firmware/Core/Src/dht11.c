#include "dht11.h"


#define DT GPIO_Pin_0
      
//��λDHT11
void DHT11_Rst(void)	   
{                 
	DHT11_IO_OUT(); 	//SET OUTPUT
	DHT11_DQ_OUT((GPIO_PinState)0); 	//����DQ
	HAL_Delay(20);    	//��������18ms
	DHT11_DQ_OUT((GPIO_PinState)1); 	//DQ=1 
	delay_us_systick(30);     	//��������20~40us
}

//�ȴ�DHT11�Ļ�Ӧ
//����1:δ��⵽DHT11�Ĵ���
//����0:����
uint8_t DHT11_Check(void) 	   
{   
	uint8_t retry=0;
	DHT11_IO_IN();//SET INPUT	 
    while (DHT11_DQ_IN&&retry<100)//DHT11������40~80us
	{
		retry++;
		delay_us_systick(1);
	};	 
	if(retry>=100)return 1;
	else retry=0;
    while (!DHT11_DQ_IN&&retry<100)//DHT11���ͺ���ٴ�����40~80us
	{
		retry++;
		delay_us_systick(1);
	};
	if(retry>=100)return 1;	    
	return 0;
}

//��DHT11��ȡһ��λ
//����ֵ��1/0
uint8_t DHT11_Read_Bit(void) 			 
{
 	uint8_t retry=0;
	while(DHT11_DQ_IN&&retry<100)//�ȴ���Ϊ�͵�ƽ
	{
		retry++;
		delay_us_systick(1);
	}
	retry=0;
	while(!DHT11_DQ_IN&&retry<100)//�ȴ���ߵ�ƽ
	{
		retry++;
		delay_us_systick(1);
	}
	delay_us_systick(40);//�ȴ�40us
	if(DHT11_DQ_IN)return 1;
	else return 0;		   
}

//��DHT11��ȡһ���ֽ�
//����ֵ������������
uint8_t DHT11_Read_Byte(void)    
{        
	uint8_t i,dat;
	dat=0;
	for (i=0;i<8;i++) 
	{
		dat<<=1; 
		dat|=DHT11_Read_Bit();
	}						    
	return dat;
}

//��DHT11��ȡһ������
//temp:�¶�ֵ(��Χ:0~50��)
//humi:ʪ��ֵ(��Χ:20%~90%)
//����ֵ��0,����;1,��ȡʧ��
/* return: 0=OK, 1=no response, 2=checksum error */
uint8_t DHT11_Read_Data(uint8_t *temp,uint8_t *humi)
{
    uint8_t buf[5];
    uint8_t i;

    if ((temp == NULL) || (humi == NULL))
    {
        return DHT11_ERROR_NO_RESPONSE;
    }

    DHT11_Rst();
    if (DHT11_Check() != 0U)
    {
        return DHT11_ERROR_NO_RESPONSE;
    }

    for (i = 0U; i < 5U; i++)
    {
        buf[i] = DHT11_Read_Byte();
    }

    if ((uint8_t)(buf[0] + buf[1] + buf[2] + buf[3]) != buf[4])
    {
        return DHT11_ERROR_CHECKSUM;
    }

    *humi = buf[0];
    *temp = buf[2];

    return DHT11_OK;
}

//��ʼ��DHT11��IO�� DQ ͬʱ���DHT11�Ĵ���
//����1:������
//����0:����    	 
uint8_t DHT11_Init(void)
{	 

	DHT11_Rst();  //��λDHT11
	return DHT11_Check();//�ȴ�DHT11�Ļ�Ӧ
} 

