#pragma once

#include "stm32u575xx.h"
#include "stm32u5xx_hal_cortex.h"

// レジスタの特定ビットをクリアしつつ、新しい値を書き込む汎用マクロ
#define REG_MODIFY(REG, CLEAR_MASK, SET_MASK) \
    ((REG) = (((REG) & ~(CLEAR_MASK)) | (SET_MASK)))

namespace init {
    inline void coreSetup(){
        /*
        メモ
        PLL1のみどうしようもないので，coreSteupで初期化する
        一回起動してしまったらどうしようもないので
        */
        
        //FLASH SETTING --------------------------------------------------------
        //いったん80mhzに設定する
        FLASH -> ACR = 0x0000'0102;//Fclk<=90MHzなので2でいい．プリフェッチ有効化
        
        //RCC SETTING 1 --------------------------------------------------------
        //ここではPWRのみクロックを有効にする
        RCC -> AHB3ENR |= 0x0000'0004;//PWR

        //ここでは，メインクロックの設定だけする
        while ((RCC -> CR & 0x0000'0004) == 0);//Wait for MSISRDY
        REG_MODIFY(RCC -> ICSCR1, 0xF000'0000, 0x2080'0000);//MSIS:16MHz, MSIRGSEL:1

        RCC -> PLL1CFGR = 0x0004'1101;//PLL1R:EN, PLL1MBOOST:1/2, PLL1M:1/2, PLL1RGE:4-8MHz(refx_ck), PLL1<-MSIS
        
        //PWR SETTING ----------------------------------------------------------
        //レンジは2として設定する
        REG_MODIFY(PWR -> VOSR, 0x0001'0000, 0x0006'0000);//BOOSTEN:EN, VOS:Range2
        while ((PWR -> VOSR & 0x0000'C000) != 0x0000'C000);//Wait for VOSRDY, BOOSTRDY
        
        //RCC SETTING 2 --------------------------------------------------------
        //PLL1 = MSIS(16MHz) / PLL1M
        //PLL1VCO = 20(8*20 = 160MHz)
        //PLL1R = 160 / 2 = 80MHzz
        RCC -> PLL1DIVR = 0x0100'0013;//PLL1R:1/2, PLL1N:20
        RCC -> CR |= 0x0100'0000;//PLL1:EN
        while ((RCC -> CR & 0x0200'0000) == 0);//Wait for PLL1RDY
        
        RCC -> CFGR1 |= 0x0000'0003;//SYSCLK <- PLL1R
        while ((RCC -> CFGR1 & 0x0000'000C) != 0x0000'000C);//Wait for SWS(PLL1R)

        //ICACHE SETTING -------------------------------------------------------
        // ICACHE -> CRR0 = 0x0040'8040;//CACHE0:EN, WRAP, MASTER1, 2MByte, 0x0800'0000 - 0x0820'0000
        //↑リファレンスにいらないって書いてあったから，無効化しておく
        ICACHE -> CR = 0x0000'0005;//n-Ways:EN, ICACHE:EN
        //n-Waysキャッシュを切るとパフォーマンス落ちるけど，省消費電力になる

        //DCACHE SETTING -------------------------------------------------------
        DCACHE1 -> CR = 0x0000'0001;//WRAP, DCACHE:EN

        //CORTEX SETTING -------------------------------------------------------
        // DWTサイクルカウンタの有効化
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

        /*
        メモ

        フラッシュ領域は一応メモリタイプはNORMALでWTになってるので，そのままでキャッシュ効くと思う
        
        OSPI領域もNORMALでWTになってるから，いけるとは思うけど，念のため，OSPIの部分で変更しておく
        フラッシュなので，write throughか，読み込みのやつでいいと思う
        メモリはnormalで，nonshareableにする

        ソース:
        PM0264 STM32 Cortex®-M33 MCUs and MPUs programming manual - Programming manual
        2.3.5から
        */
    }
    
    inline void rccSetup() {
        //普通にRCCの初期化
        RCC -> CR |= 0x0000'0100;//HSI16:EN
        while ((RCC -> CR & 0x0000'0400) == 0);//Wait for HSIRDY

        //ペリフェラルにクロックを入れる
        RCC -> AHB1ENR |= 0x0000'0001;//GPDMA1
        RCC -> AHB2ENR1 |= 0x0000'000F;//GPIOA, B, C, D
        RCC -> APB3ENR |= 0x0000'1800;//LPTIM3, 1
        RCC -> APB2ENR |= 0x0000'1000;//SPI1
        RCC -> APB1ENR1 |= 0x0000'0004;//TIM4

        //クロックソースを変更する
        //リセット値が0x0000'0000なので，ビットを立てるだけで完了する
        RCC -> CCIPR1 |= 0x0020'0000;//SPI1:HSI16
        RCC -> CCIPR3 |= 0x0000'0A00;//LPTIM1, 3/4:HSI16
    }

    inline void gpioSetup() {
        //GPIOA
        REG_MODIFY(GPIOA -> MODER, 0x0000'4500, 0x0000'8A00);
        //PC7, 5, 4:AF
        GPIOA -> AFR[0] = 0x5055'0000;
        //PA7:SPI1_MOSI, PA5:SPI1_SCK, PA4:SPI1_SS

        REG_MODIFY(GPIOB -> MODER, 0x0000'4040, 0x0000'8080);
        // REG_MODIFY(GPIOB -> MODER, 0x0000'0000, 0x0000'8000);

        GPIOB -> AFR[0] = 0x2000'2000;//TIM4_CH2, LPTIM1_CH1

        //GPIOC
        GPIOC -> BSRR = 0x0000'00C0;
        REG_MODIFY(GPIOC -> MODER, 0x0000'F0C0, 0x0000'5080);
        //PC3:AF, PC7, 6:Digital_Output
        GPIOC -> AFR[0] |= 0x0000'2000;//PC3:AF2(LPTIM3_CH1)

        //GPIOD
        GPIOD -> BSRR = 0x0000'0800;
        REG_MODIFY(GPIOD -> MODER, 0x0080'0000, 0x0040'0000);//PD11

    }

    inline void timerSetup() {
        //TIM4
        // TIM4 -> CR2 = 0x0000'0000;
        // TIM4 -> SMCR = 0x0000'0000;
        // TIM4 -> DIER = 0x0000'0000;
        // TIM4 -> CCMR1 = 0x0000'0000;//CH2, 1:OUTPUT, CH2:(PWM_Mode1, PE:EN)
        // TIM4 -> CCER = 0x0010;//CH2:(Active_LO, OUTPUT:EN)
        // TIM4 -> PSC = 79;//80MHz -> 1MHz
        // TIM4 -> CNT = 0;
        // TIM4 -> ARR = 999;//1KHz
        // TIM4 -> CCR2 = 499;//50%
        // TIM4 -> EGR = 0x0001;//UG
        // TIM4 -> CR1 = 0x0001;//TIM4:EN

        //LPTIM1
        LPTIM1 -> CFGR = 0x0040'0800;//PRELOAD:EN, 1/16(HSI16 -> 1MHz)
        LPTIM1 -> CR = 0x0000'0001;
        LPTIM1 -> CCR1 = 999;//100%
        LPTIM1 -> ARR = 999;//1KHz
        LPTIM1 -> CNT = 0;
        LPTIM1 -> CCMR1 |= 0x0000'0002;//CH1:Active_Hi, OUTPUT:EN, PWM
        LPTIM1 -> CR |= 0x0000'0004;//CNTSTRT

        //LPTIM3
        LPTIM3 -> CFGR = 0x0040'0800;//PRELOAD:EN, 1/16(HSI16 -> 1MHz)
        LPTIM3 -> CR = 0x0000'0001;
        LPTIM3 -> CCR1 = 499;//50%
        LPTIM3 -> ARR = 999;//1KHz
        LPTIM3 -> CNT = 0;
        LPTIM3 -> CCMR1 |= 0x0000'0002;//CH1:Active_Hi, OUTPUT:EN, PWM
        LPTIM3 -> CR |= 0x0000'0004;//CNTSTRT


        //TIM7
        //80MHz->1KHz
        // TIM7 -> PSC = 79;
        // TIM7 -> ARR = 999;
        // TIM7 -> EGR = 0x0001;//UG
        // TIM7 -> CR1 = 0x0001;//TIM7:EN


        //TIM4
    }

    inline void spiSetup() {
        //SPI1 (APB2)
        SPI1 -> CR1 = 0x0000'1000;//SSI:HI, SPIx:OFF
        SPI1 -> CR2 = 0;
        SPI1 -> CFG1 = 0x1000'C0E7;//Fclk:1/4, T/RXDMAEN, FTHLV:8, 8bit
        SPI1 -> CFG2 = 0x8442'0000;//AFCNTR, SS_OUT:DISABLE, SSM:SSI, MASTER, TX_Only
        SPI1 -> AUTOCR = 0x0020'0000;//dma1_ch0_tc
    }

    inline void gpdmaSetup() {
        GPDMA1 -> PRIVCFGR = 0x0000'FFFF;//全て特権に設定する
        GPDMA1_Channel0_S -> CCR = 0x00C0'0000;//PRIO:HI, LAP:Port0, GPDMA:EN
    }

    void interruptSetup() {
        __enable_irq();
        NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_3);
        
        //SPI1
        NVIC_SetPriority(SPI1_IRQn, NVIC_EncodePriority(NVIC_PRIORITYGROUP_3, 7, 0));
        NVIC_EnableIRQ(SPI1_IRQn);
    }

    inline void setup(){
        coreSetup();
        rccSetup();
        gpioSetup();
        timerSetup();

        spiSetup();
        gpdmaSetup();
        // interruptSetup();
    }
}