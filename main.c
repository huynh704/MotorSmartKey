#include "stm8s.h"
#include "stm8s_spi.h"
#include "stm8s_tim2.h"
#include <string.h>
#include "stm8s_flash.h"

//User Define
#define WRITE   TRUE
#define READ    FALSE
#define ON      TRUE
#define OFF     FALSE

#define BLE_Power GPIO_PIN_3
#define BLE_Power_Port GPIOD
#define BLE_PWC GPIO_PIN_5
#define BLE_PWC_Port GPIOC

#define Speaker_Pin GPIO_PIN_2
#define Speaker_Port GPIOD
#define Led_Pin GPIO_PIN_6
#define Led_Port GPIOC

#define Epp_WirteProtect_Pin GPIO_PIN_3
#define Epp_WirteProtect_Port GPIOC
#define Cps_Ready_Pin GPIO_PIN_3
#define Cps_Ready_Port GPIOA

#define Power_Pin GPIO_PIN_4
#define Power_Port GPIOC
#define Key_Pin GPIO_PIN_4
#define Key_Port GPIOD

#define FORCE_MODE_ADDR 0x4000
#define PASSWORD_ADDR   0x4010

//*********
//Funtion def
void rccConfig();
// void spiInit();
void uartInit(uint32_t baudRate);
void uartSend(char* message);
void ioInit();
void millis_Start(uint32_t TimePerStep);
void speak_Ring(int ring[], int size);
void power_IO(bool state);
void File_Operator(bool Option, uint32_t adress, uint8_t *data, uint16_t size);
//***********
//Variable def
int r_Start[7] = {10, 9, 8, 7, 6, 5, 4};
int r_Stop[7] = {4, 5, 6, 7, 8, 9, 10};
int r_Recive[2] = {2, 3};
int r_Error[2] = {30};

int n_Tone = 200;
bool b_Power = FALSE;
bool b_Logged = FALSE;
uint8_t i_ForceMode;

int n_AutoSeq = 0;
struct UART_Data
{
  bool ReadComplete;
  uint8_t index, LastRecive;
  uint8_t ReceiveData[50];
} uart1;
struct timer_count
{
  uint32_t millis;
  uint32_t Alarm;
  uint32_t Sleep;
  uint32_t Step;
} Time;
uint8_t pass[10];
//uint8_t pass[] = "HUYNH_ANH ";
//************
void main()
{
  rccConfig();
  ioInit();
  millis_Start(1);
  uartInit(115200);
  char BLE_Name[12];
  //  Password_Operator(WRITE, 0x4000, &pass[0]);
  //  memset(&pass[0], 0, sizeof(pass));
  File_Operator(READ, PASSWORD_ADDR, &pass[0], sizeof(pass));
  File_Operator(READ, FORCE_MODE_ADDR, &i_ForceMode, 1);
  while(TRUE)
  {
    if(n_AutoSeq != 1)
    {
      Time.Sleep = Time.millis;
      Time.Alarm = Time.millis;
    }
    
    switch(n_AutoSeq)
    {
    case 0: //Buzzer On, BLE Power On
      {
        b_Power = FALSE;
        speak_Ring(r_Start, 7);
        GPIO_WriteLow(Power_Port, Power_Pin);
        
        GPIO_WriteHigh(BLE_PWC_Port, BLE_PWC);
        GPIO_WriteHigh(BLE_Power_Port, BLE_Power);
        Time.Step = Time.millis;
        n_AutoSeq++;
        break;
      }
    case 1: // Wait connect
      {
        if(uart1.ReadComplete)
        {
          uart1.ReadComplete = FALSE;
          if( strcmp((char*)uart1.ReceiveData, "+CONNECTED") == 0 )
          {
            for(uint32_t i = 0; i < 640000; i++);
            uartSend("[Reply] Connected\n");
            speak_Ring(r_Recive, 2);
            Time.Step = Time.millis;
            n_AutoSeq++;
          }
          memset(uart1.ReceiveData, 0, strlen((char*)uart1.ReceiveData));
        }
        else if(GPIO_ReadInputPin(Key_Port, Key_Pin) == 0x10)
        {
          if(i_ForceMode == TRUE)
          {
            speak_Ring(r_Recive, 2);
            Time.Step = Time.millis;
            n_AutoSeq++;
          }
          else if(Time.millis - Time.Alarm > 10000) // wait 10 sec
          {
            Time.Step = Time.millis;
            n_AutoSeq = 6;
          }
        }
        else if (Time.millis - Time.Sleep >= 10800000) // after 15 hour 54,000,000ms
        {
          GPIO_WriteLow(BLE_Power_Port, BLE_Power);
          n_AutoSeq = 5;
        }
        else
        {
          Time.Alarm = Time.millis;
        }
        break;
      }
    case 2: // Wait key On
      {
        //        if(Time.millis - Time.Step > 1000)
        //        {
        //          Time.Step = Time.millis;
        //          GPIO_WriteReverse(Led_Port, Led_Pin);
        //        }
        if(GPIO_ReadInputPin(Key_Port, Key_Pin) == 0x10)
        {
          GPIO_WriteLow(Led_Port, Led_Pin);
          speak_Ring(r_Recive, 2);
          uartSend("Ready for start.\n");
          n_AutoSeq++;
        }
        else if(uart1.ReadComplete)
        {
          uart1.ReadComplete = FALSE;
          if(strcmp((char*)uart1.ReceiveData, "+FIND_DEVICE") == 0)
          {
            Time.Step = Time.millis;
            uartSend("[Reply] ");
            for(int i = 0; i < 10; i++)
            {
              speak_Ring(r_Recive, 2);
              while(Time.millis - Time.Step < 100);
              Time.Step = Time.millis;
              uartSend("*");
            }
            uartSend("\n");
          }
          else if(strcmp((char*)uart1.ReceiveData, "+DISCONNECT") == 0)
          {
            GPIO_WriteLow(Led_Port, Led_Pin);
            speak_Ring(r_Error, 1);
            n_AutoSeq = 1;
          }
          else
          {
            uartSend("Switch key to ON\n");
          }
          memset(uart1.ReceiveData, 0, strlen((char*)uart1.ReceiveData));
        }
        break;
      }
    case 3: // Wait turn On motor
      {
        Time.Step = Time.millis;
        if(GPIO_ReadInputPin(Key_Port, Key_Pin) == RESET)
        {
          speak_Ring(r_Error, 1);
          n_AutoSeq = 2;
        }
        else if(i_ForceMode == TRUE)
        {
          //          GPIO_WriteHigh(Power_Port, Power_Pin);
          power_IO(ON);
          n_AutoSeq++;
        }
        if(uart1.ReadComplete)
        {
          uart1.ReadComplete = FALSE;
          memset(BLE_Name, 0, strlen(BLE_Name));
          strncpy(BLE_Name, (char*)uart1.ReceiveData, 7);
          if(strcmp((char*)uart1.ReceiveData, "+MOTOR_ON") == 0)
          {
            //            uartSend("\nMotor is On");
            power_IO(ON);
            n_AutoSeq++;
          }
          else if (strcmp(BLE_Name, "AT+NAME") == 0)
          {
            //            memset(BLE_Name, 0, strlen(BLE_Name));
            //            strncpy(BLE_Name, (char*)uart1.ReceiveData + 7, 12);
            //            uartSend("\nChange name: ");
            //            uartSend(BLE_Name);
            //            
            //            while(Time.millis - Time.Step < 500);
            //            Time.Step = Time.millis;
            //            
            //            strcat(BLE_Name, "\r\n");
            //            GPIO_WriteLow(BLE_Power_Port, BLE_Power);
            //            GPIO_WriteLow(BLE_PWC_Port, BLE_PWC);
            //            uartInit(9600);
            //            while(Time.millis - Time.Step < 1000);
            //            Time.Step = Time.millis;
            //            GPIO_WriteHigh(BLE_Power_Port, BLE_Power);
            //            while(Time.millis - Time.Step < 3000);
            //            Time.Step = Time.millis;
            //            uartSend(BLE_Name);
            //            while(Time.millis - Time.Step < 1000);
            //            Time.Step = Time.millis;
            //            
            //            GPIO_WriteLow(BLE_Power_Port, BLE_Power);
            //            GPIO_WriteHigh(BLE_PWC_Port, BLE_PWC);
            //            while(Time.millis - Time.Step < 1000);
            //            Time.Step = Time.millis;
            //            GPIO_WriteHigh(BLE_Power_Port, BLE_Power);
            //            uartInit(115200);
            //            n_AutoSeq = 1;
          }
          else if(strcmp((char*)uart1.ReceiveData, "+DISCONNECT") == 0)
          {
            speak_Ring(r_Error, 1);
            n_AutoSeq = 1;
          }
          memset(uart1.ReceiveData, 0, strlen((char*)uart1.ReceiveData));
        }
        break;
      }
    case 4: //Running mode
      {
        Time.Step = Time.millis;
        if(uart1.ReadComplete)
        {
          uart1.ReadComplete = FALSE;
          if(strcmp((char*)uart1.ReceiveData, "+MOTOR_ON") == 0)
          {
            power_IO(OFF);
            n_AutoSeq = 2;
          }
          else if(strcmp((char*)uart1.ReceiveData, "+FORCE_MODE") == 0)
          {
            i_ForceMode = i_ForceMode == 0 ? 1 : 0;
            File_Operator(WRITE, FORCE_MODE_ADDR, &i_ForceMode, 1);
            if(i_ForceMode == TRUE) uartSend("Force Mode On\n");
            else uartSend("Force Mode Off\n");
          }
          else if(strcmp((char*)uart1.ReceiveData, "+ClearPass") == 0)
          {
            File_Operator(WRITE, PASSWORD_ADDR, &pass[0], sizeof(pass));
            uartSend("Password is clear\n");
          }
//          else if(strstr((char*)uart1.ReceiveData, "+PassChange:") != NULL)
//          {
//            memcpy(pass, strstr((char*)uart1.ReceiveData, ":") + 1, sizeof(pass));
//            uartSend((char*)pass);
//          }
          else if(strcmp((char*)uart1.ReceiveData, "+DISCONNECT") == 0)
          {
            n_AutoSeq = 7;
          }
          memset(uart1.ReceiveData, 0, strlen((char*)uart1.ReceiveData));
        }
        if(GPIO_ReadInputPin(Key_Port, Key_Pin) == RESET)
        {
          BitStatus bCheckKey = RESET;
          for(int i = 0; i  < 1000; i++)
          {
            Time.Step = Time.millis;
            while(Time.millis - Time.Step < 5);
            bCheckKey += GPIO_ReadInputPin(Key_Port, Key_Pin);
          }
          if(bCheckKey == RESET)
          {
            power_IO(OFF);
            n_AutoSeq = 8;
          }
        }
        break;
      }
    case 5: //Sleep mode
      {
        Time.Step = Time.millis;
        if(GPIO_ReadInputPin(Key_Port, Key_Pin) == 0x10) n_AutoSeq = 0;
        break;
      }
    case 6: //Alarm
      {
        speak_Ring(r_Recive, 2);
        while(Time.millis - Time.Step < 100);
        Time.Step = Time.millis;
        if(GPIO_ReadInputPin(Key_Port, Key_Pin) == RESET | uart1.ReadComplete) n_AutoSeq = 1;
        break;
      }
    case 7: // Conection loss
      {
        for(int i = 0; i < 10; i++)
        {
          if(uart1.ReadComplete)
          {
            uart1.ReadComplete = FALSE;
            if( strcmp((char*)uart1.ReceiveData, "+CONNECTED") == 0 )
            {
              n_AutoSeq = 4;
              break;
            }
            memset(uart1.ReceiveData, 0, strlen((char*)uart1.ReceiveData));
          }
          else if( GPIO_ReadInputPin(Key_Port, Key_Pin) == RESET )
          {
            break;
          }
          n_Tone = 500;
          speak_Ring(r_Error, 1);
          while(Time.millis - Time.Step < 2000);
          Time.Step = Time.millis;
        }
        if(n_AutoSeq != 4) n_AutoSeq = 0;
        n_Tone = 200;
        Time.Step = Time.millis;
        break;
      }
    case 8:
      {
        if(GPIO_ReadInputPin(Key_Port, Key_Pin) == 0x10)
        {
          power_IO(ON);
          n_AutoSeq = 4;
        }
        else if(uart1.ReadComplete | i_ForceMode == TRUE)
        {
          if(i_ForceMode == FALSE) uartSend("Motor locked!\n");
          speak_Ring(r_Error, 1);
          n_AutoSeq = 2;
        }
        else if(Time.millis - Time.Step > 600000)
        {
          uartSend("Key off time out (3'). Dissconect BLE, return step 0\n");
          speak_Ring(r_Error, 1);
          n_AutoSeq = 0;
          Time.Step = Time.millis;
        }
      }
    }
  }
}
//Funtion process
void rccConfig()
{
  CLK_DeInit();
  CLK_SYSCLKConfig(CLK_PRESCALER_HSIDIV1);
  CLK_ClockSwitchConfig(CLK_SWITCHMODE_AUTO, CLK_SOURCE_HSI, DISABLE, CLK_CURRENTCLOCKSTATE_DISABLE);
}
void uartInit(uint32_t baudRate)
{
  UART1_DeInit();
  UART1_Init(baudRate, UART1_WORDLENGTH_8D, UART1_STOPBITS_1, UART1_PARITY_NO, UART1_SYNCMODE_CLOCK_DISABLE, UART1_MODE_TXRX_ENABLE);
  UART1_ITConfig(UART1_IT_RXNE_OR, ENABLE);
  UART1_Cmd(ENABLE);
  enableInterrupts();
}
void uartSend(char* message)
{
  while(*message)
  {
    UART1_SendData8((uint8_t)*message++);
    while(UART1_GetFlagStatus(UART1_FLAG_TXE) == RESET);
  }
}
void ioInit()
{
  GPIO_DeInit(GPIOA);
  GPIO_DeInit(GPIOC);
  GPIO_DeInit(GPIOD);
  
  GPIO_Init(Key_Port, Key_Pin, GPIO_MODE_IN_FL_NO_IT);
  GPIO_Init(Speaker_Port, Speaker_Pin, GPIO_MODE_OUT_PP_HIGH_FAST);
  GPIO_Init(Led_Port, Led_Pin, GPIO_MODE_OUT_PP_LOW_SLOW);
  
  GPIO_Init(Cps_Ready_Port, Cps_Ready_Pin, GPIO_MODE_IN_FL_NO_IT);
  
  GPIO_Init(Epp_WirteProtect_Port, Epp_WirteProtect_Pin, GPIO_MODE_IN_FL_NO_IT);
  GPIO_Init(Power_Port, Power_Pin, GPIO_MODE_OUT_PP_LOW_SLOW);
  
  GPIO_Init(BLE_Power_Port, BLE_Power, GPIO_MODE_OUT_PP_LOW_SLOW);
  GPIO_Init(BLE_PWC_Port, BLE_PWC, GPIO_MODE_OUT_PP_LOW_SLOW);
}
void millis_Start(uint32_t TimePerStep)
{
  float TimerPeriod = 0;
  uint16_t i16Buff = 1;
  TIM2_Prescaler_TypeDef Prescale = TIM2_PRESCALER_16;
  
  for(int i = 0; i < Prescale; i++) i16Buff *= 2;
  TimerPeriod = (TimePerStep * (float)CLK_GetClockFreq()) / (i16Buff * 1000.0);
  if(TimerPeriod > 65535.0) TimerPeriod = 65535; 
  
  TIM2_DeInit();
  TIM2_TimeBaseInit(Prescale, (uint16_t)TimerPeriod);
  TIM2_ClearFlag(TIM2_FLAG_UPDATE);
  TIM2_ITConfig(TIM2_IT_UPDATE, ENABLE);
  TIM2_Cmd(ENABLE);
  enableInterrupts();
}
void speak_Ring(int ring[], int size)
{
  for(int j = 0; j < size; j++)
  {
    for(int i = 0; i < n_Tone; i++)
    {
      GPIO_WriteReverse(Speaker_Port, Speaker_Pin);
      for(int delay = 0; delay < ring[j]; delay++) for(int j = 0; j < 125; j++);
    }
    for(int k = 0; k < 125; k++);
  }
  GPIO_WriteLow(Speaker_Port, Speaker_Pin);
}
void power_IO(bool state)
{
  if(state == FALSE)
  {
    b_Power = FALSE;
    speak_Ring(r_Stop, 7);
    uartSend("Motor is Off\n");
    GPIO_WriteLow(Power_Port, Power_Pin);
  }
  else
  {
    b_Power = TRUE;
    speak_Ring(r_Start, 7);
    uartSend("Motor is On\n");
    GPIO_WriteHigh(Power_Port, Power_Pin);
  }
}
//EPPROM
void File_Operator(bool Option, uint32_t adress, uint8_t *data, uint16_t size)
{
  FLASH_SetProgrammingTime(FLASH_PROGRAMTIME_STANDARD);
  FLASH_Unlock(FLASH_MEMTYPE_DATA);
  //  if(Option == READ) memset(data, 0, size);
  for(int i = 0; i < size; i++)
  {
    if(Option == READ) *data = FLASH_ReadByte(adress);
    else FLASH_ProgramByte(adress, *data);
    adress++;
    data++;
  }
  FLASH_Lock(FLASH_MEMTYPE_DATA);
}
//***************
//Handle Funtion
INTERRUPT_HANDLER(UART1_RX_IRQHandler, 18)
{
  if(UART1_GetITStatus(UART1_IT_RXNE))
  {
    char RxData = UART1_ReceiveData8();
    if((RxData == 0x0A & uart1.LastRecive == 0x0D) | uart1.index >= sizeof(uart1.ReceiveData))
    {
      //      for(int i = uart1.index; i < sizeof(uart1.ReceiveData); i++) uart1.ReceiveData[i] = 0;
      uart1.ReceiveData[uart1.index] = 0;
      
      uart1.index = 0;
      uart1.ReadComplete = TRUE;
    }
    else if(RxData != 0x0D & RxData != 0x0A)
    {
      uart1.ReadComplete = FALSE;
      uart1.ReceiveData[uart1.index] = RxData;
      uart1.index++;
    }
    else uart1.LastRecive = RxData;
  }
}
INTERRUPT_HANDLER(TIM2_UPD_OVF_BRK_IRQHandler, 13)
{
  if(TIM2_GetITStatus(TIM2_IT_UPDATE) == SET)
  {
    //    if(Time.millis >= 4294967290) // 3 360 183 641
    //    {
    ////      if(Time.Sleep > 0) Time.Sleep -= Time.millis;
    //      Time.millis = 0;
    //    }
    Time.millis++;
    TIM2_ClearITPendingBit(TIM2_IT_UPDATE);
  }
}
//******************

//STM8 Def
#ifdef USE_FULL_ASSERT

/**
* @brief  Reports the name of the source file and the source line number
*   where the assert_param error has occurred.
* @param file: pointer to the source file name
* @param line: assert_param error line source number
* @retval None
*/
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
  ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* Infinite loop */
  //  uint8_t* s_file = file;
  //  uint32_t t_line = line;
  while (1)
  {
    
  }
}
#endif