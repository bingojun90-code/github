12
#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_misc.h"

/* 定义引脚 */
#define LED_RED_PIN      GPIO_Pin_0     /* LED红色 - PA0 */
#define LED_GREEN_PIN    GPIO_Pin_1     /* LED绿色 - PA1 */
#define LED_BLUE_PIN     GPIO_Pin_2     /* LED蓝色 - PA2 */
#define LED_PORT         GPIOA

#define KEY_PIN          GPIO_Pin_3     /* 按键 - PA3 */
#define KEY_PORT         GPIOA

/* 状态定义 */
#define LED_OFF          0              /* LED关闭 */
#define LED_ON_WAIT      1              /* LED点亮等待 */
#define LED_BLINK        2              /* LED闪烁 */

/* 全局变量 */
uint8_t led_state = LED_OFF;           /* LED当前状态 */
uint32_t led_timer = 0;                /* LED计时器（毫秒） */
uint8_t blink_color = 0;               /* 闪烁颜色：0=红色，1=黄色，2=蓝色 */
uint8_t blink_flag = 0;                /* 闪烁标志 */

/**
  * @brief  系统初始化
  */
void System_Init(void)
{
    /* 使能GPIOA时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    /* 使能AFIO时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
}

/**
  * @brief  GPIO初始化
  */
void GPIO_Init_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    /* LED引脚配置：输出模式，推挽输出 */
    GPIO_InitStructure.GPIO_Pin = LED_RED_PIN | LED_GREEN_PIN | LED_BLUE_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED_PORT, &GPIO_InitStructure);
    
    /* 按键引脚配置：输入模式 */
    GPIO_InitStructure.GPIO_Pin = KEY_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;      /* 上拉输入 */
    GPIO_Init(KEY_PORT, &GPIO_InitStructure);
    
    /* 初始状态：LED全灭 */
    GPIO_SetBits(LED_PORT, LED_RED_PIN | LED_GREEN_PIN | LED_BLUE_PIN);
}

/**
  * @brief  EXTI中断配置
  */
void EXTI_Config(void)
{
    EXTI_InitTypeDef EXTI_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    
    /* 连接PA3到EXTI3 */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource3);
    
    /* EXTI3配置：下降沿触发 */
    EXTI_InitStructure.EXTI_Line = EXTI_Line3;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);
    
    /* NVIC配置 */
    NVIC_InitStructure.NVIC_IRQChannel = EXTI3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

/**
  * @brief  SysTick初始化，用于产生1ms中断
  */
void SysTick_Init(void)
{
    /* 1ms产生一次中断 */
    if (SysTick_Config(SystemCoreClock / 1000))
    {
        while (1);
    }
}

/**
  * @brief  LED亮红色
  */
void LED_Red_On(void)
{
    GPIO_ResetBits(LED_PORT, LED_RED_PIN);      /* 红色亮 */
    GPIO_SetBits(LED_PORT, LED_GREEN_PIN);      /* 绿色灭 */
    GPIO_SetBits(LED_PORT, LED_BLUE_PIN);       /* 蓝色灭 */
}

/**
  * @brief  LED亮黄色（红+绿）
  */
void LED_Yellow_On(void)
{
    GPIO_ResetBits(LED_PORT, LED_RED_PIN);      /* 红色亮 */
    GPIO_ResetBits(LED_PORT, LED_GREEN_PIN);    /* 绿色亮 */
    GPIO_SetBits(LED_PORT, LED_BLUE_PIN);       /* 蓝色灭 */
}

/**
  * @brief  LED亮蓝色
  */
void LED_Blue_On(void)
{
    GPIO_SetBits(LED_PORT, LED_RED_PIN);        /* 红色灭 */
    GPIO_SetBits(LED_PORT, LED_GREEN_PIN);      /* 绿色灭 */
    GPIO_ResetBits(LED_PORT, LED_BLUE_PIN);     /* 蓝色亮 */
}

/**
  * @brief  LED全灭
  */
void LED_All_Off(void)
{
    GPIO_SetBits(LED_PORT, LED_RED_PIN | LED_GREEN_PIN | LED_BLUE_PIN);
}

/**
  * @brief  EXTI3中断处理函数（按键中断）
  */
void EXTI3_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line3) != RESET)
    {
        /* 切换LED状态 */
        if (led_state == LED_OFF)
        {
            led_state = LED_ON_WAIT;
            led_timer = 0;
            LED_Red_On();           /* 先亮红色 */
        }
        else
        {
            led_state = LED_OFF;
            led_timer = 0;
            blink_flag = 0;
            LED_All_Off();
        }
        
        EXTI_ClearITPendingBit(EXTI_Line3);
    }
}

/**
  * @brief  SysTick中断处理函数（1ms）
  */
void SysTick_Handler(void)
{
    led_timer++;
    
    if (led_state == LED_ON_WAIT)
    {
        /* LED持续亮5秒 */
        if (led_timer < 5000)
        {
            /* 保持红色亮 */
            LED_Red_On();
        }
        else
        {
            /* 5秒后进入闪烁状态 */
            led_state = LED_BLINK;
            led_timer = 0;
            blink_color = 0;
            blink_flag = 0;
        }
    }
    else if (led_state == LED_BLINK)
    {
        /* LED闪烁，每500ms切换一次颜色 */
        if (led_timer % 500 == 0)
        {
            blink_flag = 1 - blink_flag;
            
            if (blink_flag)
            {
                /* 显示当前颜色 */
                if (blink_color == 0)
                    LED_Red_On();
                else if (blink_color == 1)
                    LED_Yellow_On();
                else
                    LED_Blue_On();
            }
            else
            {
                /* 熄灭LED */
                LED_All_Off();
                
                /* 准备下一个颜色 */
                blink_color++;
                if (blink_color > 2)
                    blink_color = 0;
            }
        }
    }
}

/**
  * @brief  主函数
  */
int main(void)
{
    /* 系统初始化 */
    System_Init();
    
    /* GPIO初始化 */
    GPIO_Init_Config();
    
    /* EXTI中断配置 */
    EXTI_Config();
    
    /* SysTick初始化 */
    SysTick_Init();
    
    /* 全局中断使能 */
    __enable_irq();
    
    /* 主循环 */
    while (1)
    {
        /* 可以在这里添加其他处理逻辑 */
    }
    
    return 0;
}