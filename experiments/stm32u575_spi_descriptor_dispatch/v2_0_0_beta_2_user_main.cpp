#include "user_main.hpp"
#include "user_main.h"

#include "stm32u575xx.h"
#include "stm32u5xx_ll_utils.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "gpdma_lli.hpp"
#include "stm32u575_spi_t.hpp"

namespace init
{
    inline void modify_register(
        volatile uint32_t& reg,
        uint32_t clear_mask,
        uint32_t set_mask)
    {
        reg = (reg & ~clear_mask) | set_mask;
    }

    void setup()
    {
        //Flash latency
        FLASH->ACR = 0x0000'0102;

        //RCC
        RCC->AHB3ENR |= 0x0000'0004;
        while ((RCC->CR & 0x0000'0004) == 0);
        modify_register(RCC->ICSCR1, 0xF000'0000, 0x2080'0000);
        RCC->PLL1CFGR = 0x0004'1101;

        //PWR Range-2
        modify_register(PWR->VOSR, 0x0001'0000, 0x0006'0000);
        while ((PWR->VOSR & 0x0000'C000) != 0x0000'C000);

        //80MHz
        RCC->PLL1DIVR = 0x0100'0013;
        RCC->CR |= 0x0100'0000;
        while ((RCC->CR & 0x0200'0000) == 0);

        RCC->CFGR1 |= 0x0000'0003;
        while ((RCC->CFGR1 & 0x0000'000C) != 0x0000'000C);

        //CACHE
        ICACHE->CR = 0x0000'0005;

        //HSI-16
        RCC->CR |= 0x0000'0100;
        while ((RCC->CR & 0x0000'0400) == 0);

        //RCC
        RCC->AHB1ENR |= 0x0000'0001;
        RCC->AHB2ENR1 |= 0x0000'000D;
        RCC->APB3ENR |= 0x0000'0800;
        RCC->APB2ENR |= 0x0000'1000;
        RCC->CCIPR1 |= 0x0020'0000;
        RCC->CCIPR3 |= 0x0000'0800;

        //GPIO
        modify_register(GPIOA->MODER, 0x0000'4500, 0x0000'8A00);
        GPIOA->AFR[0] = 0x5055'0000;

        GPIOC->BSRR = 0x0000'00C0;
        modify_register(GPIOC->MODER, 0x0000'F0C0, 0x0000'5080);
        GPIOC->AFR[0] |= 0x0000'2000;
        
        GPIOD -> BSRR = 0x0000'0800;
        modify_register(GPIOD -> MODER, 0x0080'0000, 0x0040'0000);//PD11

        //LPTIM1(GPDMA_trigger)
        LPTIM1 -> CFGR = 0x0040'0800;//PRELOAD:EN, 1/16(HSI16 -> 1MHz)
        LPTIM1 -> CR = 0x0000'0001;
        LPTIM1 -> CCR1 = 999;//100%
        LPTIM1 -> ARR = 999;//1KHz
        LPTIM1 -> CNT = 0;
        LPTIM1 -> CCMR1 |= 0x0000'0002;//CH1:Active_Hi, OUTPUT:EN, PWM
        LPTIM1 -> CR |= 0x0000'0004;//CNTSTRT

        //SPI
        SPI1->CR1 = 0x0000'1000;
        SPI1->CR2 = 0;
        SPI1->CFG1 = 0x1000'C0E7;
        SPI1->CFG2 = 0x8442'0000;
        SPI1->AUTOCR = 0x0020'0000;

        //GPDMA
        GPDMA1->PRIVCFGR = 0x0000'FFFF;
        GPDMA1_Channel0_S->CCR = 0x00C0'0000;
    }
}

namespace spiLliSpace
{
    constexpr uint32_t SPI_CLR_IFCR    = 0x0000'FFFF;
    constexpr uint32_t SPI_START_CR1   = 0x0000'1001; // SSI:HI, SPIx:EN
    constexpr uint32_t SPI_DISABLE_CR1 = 0x0000'1000; // SSI:HI, SPIx:OFF
    constexpr uint32_t SPI_DAT_LEN     = 10;

    constexpr auto make_dummy()
    {
        std::array<uint8_t, 256> data{};

        for (std::size_t i = 0; i < data.size(); ++i)
        {
            data[i] = static_cast<uint8_t>(i);
        }

        return data;
    }

    constexpr auto DUMMY = make_dummy();

    constexpr LLI_12BSD_t SPI_SET_DAT_LEN_NODE = {
        .CTR1 = 0x0002'4002,
        .CTR2 = 0x0000'0200,
        .CBR1 = 4,
        .CSAR = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&SPI_DAT_LEN)),
        .CDAR = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&(SPI1->CR2)))
    };

    constexpr LLI_SD_t SPI_SET_IFCR_NODE = {
        .CSAR = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&SPI_CLR_IFCR)),
        .CDAR = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&(SPI1->IFCR)))
    };

    constexpr LLI_SD_t SPI_SEND_START_SD_NODE = {
        .CSAR = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&SPI_START_CR1)),
        .CDAR = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&(SPI1->CR1)))
    };

    constexpr auto SPI_SET_DAT_LEN = make_lli(SPI_SET_DAT_LEN_NODE);
    constexpr auto SPI_SET_IFCR = make_lli(SPI_SET_IFCR_NODE);
    constexpr auto SPI_SEND_START_SD = make_lli(SPI_SEND_START_SD_NODE);

    // STM32U575のSPI単方向送信モードでは、SPIを無効から有効にした直後の
    // 最初の送信完了時、TXC付近でSPI_RXのDMA要求が1回だけ発生することを
    // 実機で確認したため、ここではその要求を終端トリガーとして利用する。
    // この挙動はRM/DSに記載がなく、仕様保証された動作ではない。
    // SPI設定、シリコンリビジョン、初期化手順を変更した場合は再検証すること。
    constexpr LLI_12BSD_t SPI_DISABLE_NODE = {
        .CTR1 = 0x0002'4002,
        .CTR2 = 0x0000'0406,
        .CBR1 = 4,
        .CSAR = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&SPI_DISABLE_CR1)),
        .CDAR = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&(SPI1->CR1)))
    };

    constexpr auto SPI_DISABLE = make_lli(SPI_DISABLE_NODE);
}

namespace gpioLliSpace
{
    constexpr uint32_t GPIO_HIGH_C = 0x0000'00C0;
    constexpr uint32_t GPIO_LOW_C  = 0x00C0'0000;
    
    constexpr uint32_t GPIO_HIGH_D = 0x0000'0800;
    constexpr uint32_t GPIO_LOW_D  = 0x0800'0000;

    constexpr LLI_12BSD_t GPIO_HI_NODE = {
        .CTR1 = 0x0002'4002,
        .CTR2 = 0x0000'0200,
        .CBR1 = 4,
        .CSAR = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&GPIO_HIGH_C)),
        .CDAR = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&(GPIOC->BSRR)))
    };

    constexpr LLI_2S_t GPIO_LO_NODE = {
        .CTR2 = 0x010B'0200,
        .CSAR = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&GPIO_LOW_C))
    };

    constexpr LLI_12BSD_t GPIO_D_HI_NODE = {
        .CTR1 = 0x0002'4002,
        .CTR2 = 0x0000'0200,//software
        .CBR1 = 4,
        .CSAR = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&GPIO_HIGH_D)),
        .CDAR = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&(GPIOD->BSRR)))
    };

    constexpr LLI_2S_t GPIO_D_LO_NODE = {
        .CTR2 = 0x010B'0200,//LPTIM
        .CSAR = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&GPIO_LOW_D))
    };

    constexpr LLI_SD_t GPIO_LO_SOFT_NODE = {
        .CSAR = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&GPIO_LOW_C)),
        .CDAR = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&(GPIOC->BSRR)))
    };

    constexpr auto GPIO_HI = make_lli(GPIO_HI_NODE);
    constexpr auto GPIO_LO = make_lli(GPIO_LO_NODE);
    constexpr auto GPIO_LO_SOFT = make_lli(GPIO_LO_SOFT_NODE);


    // タグ
    struct GpioD;
    struct GpioDInstance1;
    struct GpioDInstance2;

    // append_lli_group動作テスト
    constexpr auto gpioDChain = make_lli(GPIO_D_HI_NODE, GPIO_D_LO_NODE);
    constexpr auto gpioDList = make_list<GpioD, GpioD>(gpioDChain);
    constexpr auto gpioDProgramTemplate = make_lli_program(gpioDList);
    constexpr auto gpioDGroup = make_lli_group(
        instantiate<GpioDInstance1>(gpioDProgramTemplate),
        instantiate<GpioDInstance2>(gpioDProgramTemplate)
    );
    // constexpr auto gpioDGroup = []{
    //     auto group = make_lli_group(
    //         instantiate<GpioDInstance1>(gpioDProgramTemplate),
    //         instantiate<GpioDInstance2>(gpioDProgramTemplate)
    //     );
    //     patch_jump<
    //         list_tail<instanced_tag<GpioDInstance1, GpioD>, GpioD>,
    //         list_entry<instanced_tag<GpioDInstance2, GpioD>, GpioD>
    //         >(group);
        
    //     return group;
    // }();
    using Exit = list_tail<
        instanced_tag<GpioDInstance2, GpioD>,
        GpioD
    >;
}


// ============================================================================
// SPI transfer descriptors
// ============================================================================

constexpr DESC_12BSD_t test_spi_dat1 = {
    .CTR1 = 0x0000'4008,
    .CTR2 = 0x0000'0407,
    .CBR1 = 10,
    .CSAR = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&spiLliSpace::DUMMY[10])),
    .CDAR = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&(SPI1->TXDR)))
};

constexpr DESC_12BSD_t test_spi_dat2 = {
    .CTR1 = 0x0000'4008,
    .CTR2 = 0x0000'0407,
    .CBR1 = 10,
    .CSAR = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&spiLliSpace::DUMMY[0])),
    .CDAR = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&(SPI1->TXDR)))
};

constexpr DESC_12BSD_t test_spi_dat3 = {
    .CTR1 = 0x0000'4008,
    .CTR2 = 0x0000'0407,
    .CBR1 = 10,
    .CSAR = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&spiLliSpace::DUMMY[40])),
    .CDAR = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&(SPI1->TXDR)))
};

constexpr DESC_12BSD_t test_spi_dat4 = {
    .CTR1 = 0x0000'4008,
    .CTR2 = 0x0000'0407,
    .CBR1 = 10,
    .CSAR = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&spiLliSpace::DUMMY[12])),
    .CDAR = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&(SPI1->TXDR)))
};

// selector.CSARから32バイト転送すると、先頭12バイトがselectorの
// CSAR/CDAR/CLLRへ、後続20バイトがexecutorのCTR1..CDARへ入る。
// executor.CLLRは転送範囲外なのでROOTの固定リンクを保持する。
using descriptor_step_t = descriptor_step<DESC_12BSD_t>;

static_assert(sizeof(void*) == sizeof(uint32_t));
static_assert(sizeof(std::uintptr_t) == sizeof(uint32_t));
static_assert(std::is_standard_layout_v<DESC_12BSD_t>);
static_assert(sizeof(DESC_12BSD_t) == 20);

constexpr LLI_12BSD_t DESC_SELECTOR_NODE = {
    .CTR1 = 0x000A'400A,
    .CTR2 = 0x0000'0200,
    .CBR1 = sizeof(descriptor_step_t),
    .CSAR = 0,
    .CDAR = 0,
    .CLLR = 0
};

constexpr LLI_12BSD_t DESC_EXECUTOR_NODE = {
    .CTR1 = 0,
    .CTR2 = 0,
    .CBR1 = 0,
    .CSAR = 0,
    .CDAR = 0,
    .CLLR = 0
};

constexpr auto DESC_SEL_EXE = make_lli(
    DESC_SELECTOR_NODE,
    DESC_EXECUTOR_NODE
);

// ============================================================================
// ROOT template
// ============================================================================

namespace SpiGpioProgram
{
    struct Program;
    struct Initial;
    struct LoopSetup;
    struct Dispatch;
    struct Finish;

    using Entry = list_entry<Program, Initial>;
    using DispatchSelector = list_entry<Program, Dispatch>;
    using DispatchExecutor = list_tail<Program, Dispatch>;

    constexpr auto spiGpioProgram = make_lli_program(
        make_list<Program, Initial>(
            make_lli(
                spiLliSpace::SPI_SET_DAT_LEN_NODE,
                gpioLliSpace::GPIO_LO_SOFT_NODE,
                spiLliSpace::SPI_SEND_START_SD_NODE
            )
        ),
        make_list<Program, LoopSetup>(
            make_lli(
                spiLliSpace::SPI_SET_DAT_LEN_NODE,
                spiLliSpace::SPI_SET_IFCR_NODE,
                spiLliSpace::SPI_SEND_START_SD_NODE
            )
        ),
        make_list<Program, Dispatch>(DESC_SEL_EXE),
        make_list<Program, Finish>(
            make_lli(
                spiLliSpace::SPI_DISABLE_NODE,
                gpioLliSpace::GPIO_HI_NODE,
                gpioLliSpace::GPIO_LO_NODE
            )
        )
    );
}

constexpr auto SPI_GPIO_ROOT_TEMPLATE = [] {
    constexpr auto spiGpioGroup =
        make_lli_group(SpiGpioProgram::spiGpioProgram);
    auto root =
        append_lli_group(gpioLliSpace::gpioDGroup, spiGpioGroup);

    patch_jump<
        list_tail<instanced_tag<gpioLliSpace::GpioDInstance1, gpioLliSpace::GpioD>, gpioLliSpace::GpioD>,
        list_entry<instanced_tag<gpioLliSpace::GpioDInstance2, gpioLliSpace::GpioD>, gpioLliSpace::GpioD>
    >(root);
    
    patch_jump<gpioLliSpace::Exit, SpiGpioProgram::Entry>(root);

    {
        using namespace SpiGpioProgram;
        patch_jump<
            list_tail<Program, Initial>,
            DispatchSelector
        >(root);

        patch_jump<
            list_tail<Program, Finish>,
            list_entry<Program, LoopSetup>
        >(root);
    }

    return root;
}();

static_assert(validate_root(SPI_GPIO_ROOT_TEMPLATE));

using root_t = std::remove_cv_t<decltype(SPI_GPIO_ROOT_TEMPLATE)>;
using descriptor_dispatch_t = descriptor_dispatch_layout<
    SpiGpioProgram::DispatchSelector,
    SpiGpioProgram::DispatchExecutor,
    DESC_12BSD_t,
    root_t
>;

// 相互参照用の前方宣言。
extern root_t SPI_GPIO_ROOT;

// ============================================================================
// Descriptor ring
//
// アドレスはコンパイラ/リンカの静的初期化とリロケーションで解決する。
// ============================================================================

constinit descriptor_step_t descList[4] = {
    descriptor_dispatch_t::make_step(
        &descList[1], SPI_GPIO_ROOT, test_spi_dat1),
    descriptor_dispatch_t::make_step(
        &descList[2], SPI_GPIO_ROOT, test_spi_dat2),
    descriptor_dispatch_t::make_step(
        &descList[3], SPI_GPIO_ROOT, test_spi_dat3),
    descriptor_dispatch_t::make_step(
        &descList[0], SPI_GPIO_ROOT, test_spi_dat4)
};

// ============================================================================
// SRAM execution ROOT
//
// selector.CSAR/CDARを含め、コンパイラ/リンカの静的初期化と
// リロケーションで完成させる。
// ============================================================================

[[gnu::used]]
[[gnu::section(".SRAM2")]]
constinit root_t SPI_GPIO_ROOT = [] {
    auto root = SPI_GPIO_ROOT_TEMPLATE;

    descriptor_dispatch_t::initialize_selector(
        root,
        SPI_GPIO_ROOT,
        descList,
        start_at<0>{}
    );

    return root;
}();

// ============================================================================
// userMain
// ============================================================================

namespace user_main
{
    void userMain()
    {
        init::setup();

        GPDMA1_Channel0->CCR |= 0x0000'0002;
        GPDMA1_Channel0->CBR1 = 0;

        load_lli<list_entry<
            instanced_tag<gpioLliSpace::GpioDInstance2, gpioLliSpace::GpioD>,
            gpioLliSpace::GpioD>>(
            *GPDMA1_Channel0,
            SPI_GPIO_ROOT
        );
        // load_lli<SpiGpioProgram::Entry>(
        //     *GPDMA1_Channel0,
        //     SPI_GPIO_ROOT
        // );

        GPDMA1_Channel0->CCR |= 0x0000'0001;

        while (1);
    }
}

extern "C" void runUserMain()
{
    user_main::userMain();
}