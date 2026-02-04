#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_spi.h"

/* ======================== HID游戏手柄描述符 ======================== */
/* USB Device Descriptor */
const uint8_t DEVICE_DESCRIPTOR[18] = {
    0x12,                  /* bLength */
    0x01,                  /* bDescriptorType = DEVICE */
    0x00, 0x02,            /* bcdUSB = 2.00 */
    0x00,                  /* bDeviceClass = 0 (Defined at Interface level) */
    0x00,                  /* bDeviceSubClass */
    0x00,                  /* bDeviceProtocol */
    0x40,                  /* bMaxPacketSize0 = 64 */
    0x83, 0x04,            /* idVendor = 0x0483 (STMicroelectronics) */
    0x40, 0x57,            /* idProduct = 0x5740 */
    0x00, 0x02,            /* bcdDevice = 2.00 */
    0x01,                  /* iManufacturer = 1 */
    0x02,                  /* iProduct = 2 */
    0x03,                  /* iSerialNumber = 3 */
    0x01                   /* bNumConfigurations = 1 */
};

/* USB Configuration Descriptor */
const uint8_t CONFIG_DESCRIPTOR[34] = {
    0x09,                  /* bLength */
    0x02,                  /* bDescriptorType = CONFIGURATION */
    0x22, 0x00,            /* wTotalLength = 34 */
    0x01,                  /* bNumInterfaces = 1 */
    0x01,                  /* bConfigurationValue = 1 */
    0x00,                  /* iConfiguration = 0 */
    0xA0,                  /* bmAttributes = BUS_POWERED | REMOTE_WAKEUP */
    0x32,                  /* bMaxPower = 100mA */
    
    /* Interface Descriptor */
    0x09,                  /* bLength */
    0x04,                  /* bDescriptorType = INTERFACE */
    0x00,                  /* bInterfaceNumber = 0 */
    0x00,                  /* bAlternateSetting = 0 */
    0x01,                  /* bNumEndpoints = 1 */
    0x03,                  /* bInterfaceClass = HID */
    0x01,                  /* bInterfaceSubClass = Boot Interface Subclass */
    0x05,                  /* bInterfaceProtocol = Gamepad */
    0x00,                  /* iInterface = 0 */
    
    /* HID Descriptor */
    0x09,                  /* bLength */
    0x21,                  /* bDescriptorType = HID */
    0x10, 0x01,            /* bcdHID = 1.10 */
    0x00,                  /* bCountryCode = 0 */
    0x01,                  /* bNumDescriptors = 1 */
    0x22,                  /* bDescriptorType = REPORT */
    0x32, 0x00,            /* wItemLength = 50 */
    
    /* Endpoint Descriptor */
    0x07,                  /* bLength */
    0x05,                  /* bDescriptorType = ENDPOINT */
    0x81,                  /* bEndpointAddress = IN | 1 */
    0x03,                  /* bmAttributes = Interrupt */
    0x08, 0x00,            /* wMaxPacketSize = 8 */
    0x0A                   /* bInterval = 10ms */
};

/* HID Report Descriptor - 游戏手柄 */
const uint8_t HID_REPORT_DESCRIPTOR[50] = {
    0x05, 0x01,            /* USAGE_PAGE (Generic Desktop) */
    0x09, 0x05,            /* USAGE (Game Pad) */
    0xA1, 0x01,            /* COLLECTION (Application) */
    
    /* 6个按键 */
    0x05, 0x09,            /* USAGE_PAGE (Button) */
    0x19, 0x01,            /* USAGE_MINIMUM (Button 1) */
    0x29, 0x06,            /* USAGE_MAXIMUM (Button 6) */
    0x15, 0x00,            /* LOGICAL_MINIMUM (0) */
    0x25, 0x01,            /* LOGICAL_MAXIMUM (1) */
    0x75, 0x01,            /* REPORT_SIZE (1) */
    0x95, 0x06,            /* REPORT_COUNT (6) */
    0x81, 0x02,            /* INPUT (Data, Variable, Absolute) */
    
    /* 2位填充 */
    0x75, 0x02,            /* REPORT_SIZE (2) */
    0x95, 0x01,            /* REPORT_COUNT (1) */
    0x81, 0x01,            /* INPUT (Constant) */
    
    /* X轴 (0-255) */
    0x05, 0x01,            /* USAGE_PAGE (Generic Desktop) */
    0x09, 0x30,            /* USAGE (X) */
    0x15, 0x00,            /* LOGICAL_MINIMUM (0) */
    0x26, 0xFF, 0x00,      /* LOGICAL_MAXIMUM (255) */
    0x75, 0x08,            /* REPORT_SIZE (8) */
    0x95, 0x01,            /* REPORT_COUNT (1) */
    0x81, 0x02,            /* INPUT (Data, Variable, Absolute) */
    
    /* Y轴 (0-255) */
    0x09, 0x31,            /* USAGE (Y) */
    0x15, 0x00,            /* LOGICAL_MINIMUM (0) */
    0x26, 0xFF, 0x00,      /* LOGICAL_MAXIMUM (255) */
    0x75, 0x08,            /* REPORT_SIZE (8) */
    0x95, 0x01,            /* REPORT_COUNT (1) */
    0x81, 0x02,            /* INPUT (Data, Variable, Absolute) */
    
    0xC0                   /* END_COLLECTION */
};

/* ======================== HID游戏设备数据结构 ======================== */
typedef struct {
    uint8_t buttons;       /* 按键状态: bit0-5=按键, bit6-7=保留 */
    uint8_t x_axis;        /* X轴 (0-255) */
    uint8_t y_axis;        /* Y轴 (0-255) */
} HID_Gamepad_Report_t;

/* ======================== 全局变量 ======================== */
HID_Gamepad_Report_t gamepad_report;
volatile uint8_t usb_ready = 0;
volatile uint8_t report_ready = 0;

/* ======================== USB核心函数 ======================== */

/**
  * @brief  初始化USB设备
  */
void USB_Init(void)
{
    /* 使能USB时钟 */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USB, ENABLE);
    
    /* 禁用USB上拉电阻 */
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_SetBits(GPIOA, GPIO_Pin_12);
    
    /* 延迟100ms，让主机检测到设备断开 */
    uint32_t i = 72000 * 100;
    while (i--);
    
    /* 使能USB上拉电阻 */
    GPIO_ResetBits(GPIOA, GPIO_Pin_12);
    
    usb_ready = 1;
}

/**
  * @brief  获取设备描述符
  */
uint8_t* USB_GetDeviceDescriptor(uint16_t *pLen)
{
    *pLen = sizeof(DEVICE_DESCRIPTOR);
    return (uint8_t*)DEVICE_DESCRIPTOR;
}

/**
  * @brief  获取配置描述符
  */
uint8_t* USB_GetConfigDescriptor(uint16_t *pLen)
{
    *pLen = sizeof(CONFIG_DESCRIPTOR);
    return (uint8_t*)CONFIG_DESCRIPTOR;
}

/**
  * @brief  获取HID报表描述符
  */
uint8_t* USB_GetHIDReportDescriptor(uint16_t *pLen)
{
    *pLen = sizeof(HID_REPORT_DESCRIPTOR);
    return (uint8_t*)HID_REPORT_DESCRIPTOR;
}

/**
  * @brief  发送HID报告（游戏设备数据）
  * @param  Report: 指向HID报告的指针
  */
void HID_SendReport(HID_Gamepad_Report_t *Report)
{
    if (usb_ready && Report)
    {
        gamepad_report = *Report;
        report_ready = 1;
    }
}

/**
  * @brief  获取当前HID报告
  */
HID_Gamepad_Report_t* HID_GetReport(void)
{
    return &gamepad_report;
}

/**
  * @brief  处理USB请求（标准请求）
  */
void USB_HandleRequest(void)
{
    /* 此函数应处理来自主机的HID请求 */
    /* 包括GET_REPORT、SET_REPORT、GET_IDLE等 */
}

/* ======================== 外设输入处理函数 ======================== */

/**
  * @brief  读取摇杆输入（模拟量）
  * @note   需要配置ADC来读取模拟摇杆
  */
uint8_t ADC_ReadAnalog(uint8_t channel)
{
    /* 
    这里应该包含ADC初始化和读取代码
    返回值范围：0-255
    */
    return 128;  /* 中点值 */
}

/**
  * @brief  读取按键状态
  * @note   需要配置GPIO来读取按键
  */
uint8_t GPIO_ReadButtons(void)
{
    uint8_t buttons = 0;
    
    /* 
    这里应该读取GPIO引脚的电平
    bit0-5 对应6个按键
    按键按下时对应位为1
    */
    
    return buttons;
}

/**
  * @brief  更新游戏设备报告
  */
void HID_UpdateReport(void)
{
    /* 读取按键状态 */
    gamepad_report.buttons = GPIO_ReadButtons();
    
    /* 读取X轴和Y轴 */
    gamepad_report.x_axis = ADC_ReadAnalog(0);   /* PA0_ADC */
    gamepad_report.y_axis = ADC_ReadAnalog(1);   /* PA1_ADC */
    
    /* 发送报告 */
    HID_SendReport(&gamepad_report);
}

/**
  * @brief  系统初始化
  */
void System_Init(void)
{
    /* 使能GPIOA和GPIOB时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
    
    /* 使能AFIO时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    
    /* 初始化USB */
    USB_Init();
}

/**
  * @brief  GPIO初始化
  */
void GPIO_Init_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    /* 初始化所有GPIO为输入（用于按键） */
    GPIO_InitStructure.GPIO_Pin = 0xFFFF;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/* ======================== 主函数 ======================== */

/**
  * @brief  主函数
  */
int main(void)
{
    /* 初始化系统 */
    System_Init();
    
    /* 初始化GPIO */
    GPIO_Init_Config();
    
    /* 初始化HID报告 */
    gamepad_report.buttons = 0;
    gamepad_report.x_axis = 128;
    gamepad_report.y_axis = 128;
    
    /* 主循环 */
    while (1)
    {
        /* 更新游戏设备报告 */
        HID_UpdateReport();
        
        /* 延迟10ms */
        uint32_t i = 72000 * 10;
        while (i--);
    }
    
    return 0;
}
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