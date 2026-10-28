#ifndef __BH1750_H
#define __BH1750_H

#include "main.h"

/*BH1750引脚定义*********************/

/*
 * BH1750使用独立的软件I2C总线（PB6/PB7）
 * 不与OLED共用I2C，避免总线冲突
 */
#define BH1750_SCL_Port     GPIOB
#define BH1750_SCL_Pin      GPIO_PIN_6
#define BH1750_SDA_Port     GPIOB
#define BH1750_SDA_Pin      GPIO_PIN_7

/*********************BH1750引脚定义*/


/*BH1750 I2C地址定义*********************/

/*
 * BH1750的7位地址为0x23（ADDR引脚接低电平）或0x5C（ADDR引脚接高电平）
 * 左移1位后得到8位写地址和读地址
 * 默认使用ADDR=Low模式，即0x23
 * 如果你的模块ADDR接高电平，请将BH1750_ADDR改为0x5C
 */
#define BH1750_ADDR             0x23    // 7位地址，ADDR=Low
//#define BH1750_ADDR           0x5C    // 7位地址，ADDR=High（取消注释使用）

#define BH1750_ADDR_W           (BH1750_ADDR << 1)       // 写地址：0x46
#define BH1750_ADDR_R           ((BH1750_ADDR << 1) | 1) // 读地址：0x47

/*********************BH1750 I2C地址定义*/


/*BH1750测量模式定义*********************/

/*
 * Continuous模式：持续测量，每次读取后自动开始下一次测量
 * One-Time模式：测量一次后进入掉电模式，节省功耗
 * H-Resolution：1 lx分辨率，测量时间约120ms
 * H-Resolution2：0.5 lx分辨率，测量时间约120ms
 * L-Resolution：4 lx分辨率，测量时间约16ms
 */
#define BH1750_CONT_H_MODE      0x10    // 连续高分辨率模式（1 lx，~120ms）
#define BH1750_CONT_H_MODE2     0x11    // 连续高分辨率模式2（0.5 lx，~120ms）
#define BH1750_CONT_L_MODE      0x13    // 连续低分辨率模式（4 lx，~16ms）
#define BH1750_ONE_H_MODE       0x20    // 单次高分辨率模式（1 lx，~120ms）
#define BH1750_ONE_H_MODE2      0x21    // 单次高分辨率模式2（0.5 lx，~120ms）
#define BH1750_ONE_L_MODE       0x23    // 单次低分辨率模式（4 lx，~16ms）

/*********************BH1750测量模式定义*/


/*函数声明*********************/

void BH1750_Init(void);                     // 初始化BH1750
float BH1750_ReadLight(void);               // 读取光照强度（lux）
float BH1750_ReadLightMode(uint8_t Mode);   // 指定模式读取光照强度

/*********************函数声明*/

#endif
