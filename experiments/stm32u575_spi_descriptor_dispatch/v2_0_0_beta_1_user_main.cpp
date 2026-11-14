#include "user_main.hpp"
#include "user_main.h"

#include "stm32u575xx.h"
#include "stm32u5xx_ll_utils.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "init.hpp"
#include "delay.hpp"
#include "gpdma_lli.hpp"
#include "stm32u575_spi_t.hpp"

constexpr auto LLI_DUMMY = make_lli(LLI_12BSD_t{});

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

    constexpr LLI_SD_t GPIO_LO_SOFT_NODE = {
        .CSAR = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&GPIO_LOW_C)),
        .CDAR = static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(&(GPIOC->BSRR)))
    };

    constexpr auto GPIO_HI = make_lli(GPIO_HI_NODE);
    constexpr auto GPIO_LO = make_lli(GPIO_LO_NODE);
    constexpr auto GPIO_LO_SOFT = make_lli(GPIO_LO_SOFT_NODE);
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
    struct InitialDataLength;
    struct InitialGpioLow;
    struct InitialSpiStart;
    struct LoopDataLength;
    struct LoopClearFlags;
    struct LoopSpiStart;
    struct Dispatch;
    struct SpiDisable;
    struct GpioHigh;
    struct GpioLow;

    using Entry = list_entry<Program, InitialDataLength>;
    using DispatchSelector = list_entry<Program, Dispatch>;
    using DispatchExecutor = list_tail<Program, Dispatch>;
}

constexpr auto SPI_GPIO_ROOT_TEMPLATE = [] {
    using namespace SpiGpioProgram;

    auto root = make_lli_group(
        make_list<Program, InitialDataLength>(
            spiLliSpace::SPI_SET_DAT_LEN),
        make_list<Program, InitialGpioLow>(
            gpioLliSpace::GPIO_LO_SOFT),
        make_list<Program, InitialSpiStart>(
            spiLliSpace::SPI_SEND_START_SD),

        make_list<Program, LoopDataLength>(
            spiLliSpace::SPI_SET_DAT_LEN),
        make_list<Program, LoopClearFlags>(
            spiLliSpace::SPI_SET_IFCR),
        make_list<Program, LoopSpiStart>(
            spiLliSpace::SPI_SEND_START_SD),

        make_list<Program, Dispatch>(DESC_SEL_EXE),

        make_list<Program, SpiDisable>(
            spiLliSpace::SPI_DISABLE),
        make_list<Program, GpioHigh>(
            gpioLliSpace::GPIO_HI),
        make_list<Program, GpioLow>(
            gpioLliSpace::GPIO_LO)
    );

    patch_jump<
        node_at<Program, InitialSpiStart, 0>,
        node_at<Program, Dispatch, 0>
    >(root);

    patch_jump<
        node_at<Program, GpioLow, 0>,
        node_at<Program, LoopDataLength, 0>
    >(root);

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
        UpdateSysTick(80'000'000);
        
        GPDMA1_Channel0->CCR |= 0x0000'0002;//RESET
        GPDMA1_Channel0->CBR1 = 0;//NULL転送

        load_lli<SpiGpioProgram::Entry>(
            *GPDMA1_Channel0,
            SPI_GPIO_ROOT
        );

        GPDMA1_Channel0->CCR |= 0x0000'0001;//EN


        while (1)
        {


            // delayMs(100);
        }
    }
}

extern "C" void runUserMain()
{
    user_main::userMain();
}