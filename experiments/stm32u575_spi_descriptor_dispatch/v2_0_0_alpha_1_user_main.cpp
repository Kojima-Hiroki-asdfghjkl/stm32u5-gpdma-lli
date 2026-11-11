#include "user_main.hpp"
#include "user_main.h"

#include "stm32u575xx.h"
#include "stm32u5xx_ll_utils.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "gpdma_lli.hpp"
#include "original_init.hpp"
#include "stm32u575_spi_t.hpp"

namespace spi_lli
{
    constexpr std::uint32_t CLEAR_IFCR = 0x0000'FFFF;
    constexpr std::uint32_t START_CR1  = 0x0000'1001; // SSI=1, SPE=1
    constexpr std::uint32_t STOP_CR1   = 0x0000'1000; // SSI=1, SPE=0
    constexpr std::uint32_t DATA_LEN   = 10;

    constexpr auto make_test_data()
    {
        std::array<std::uint8_t, 256> data{};

        for (std::size_t i = 0; i < data.size(); ++i)
        {
            data[i] = static_cast<std::uint8_t>(i);
        }

        return data;
    }

    constexpr auto TEST_DATA = make_test_data();

    constexpr LLI_12BSD_t SET_DATA_LEN_NODE = {
        .CTR1 = 0x0002'4002,
        .CTR2 = 0x0000'0200,
        .CBR1 = 4,
        .CSAR = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&DATA_LEN)
        ),
        .CDAR = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&(SPI1->CR2))
        )
    };

    constexpr LLI_SD_t CLEAR_IFCR_NODE = {
        .CSAR = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&CLEAR_IFCR)
        ),
        .CDAR = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&(SPI1->IFCR))
        )
    };

    constexpr LLI_SD_t START_NODE = {
        .CSAR = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&START_CR1)
        ),
        .CDAR = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&(SPI1->CR1))
        )
    };

    constexpr auto SET_DATA_LEN = make_lli(SET_DATA_LEN_NODE);
    constexpr auto CLEAR_FLAGS  = make_lli(CLEAR_IFCR_NODE);
    constexpr auto START        = make_lli(START_NODE);

    // STM32U575のSPI単方向送信モードでは、SPIを無効から有効にした直後の
    // 最初の送信完了時、TXC付近でSPI_RXのDMA要求が1回だけ発生することを
    // 実機で確認したため、ここではその要求を終端トリガーとして利用する。
    // この挙動はRM/DSに記載がなく、仕様保証された動作ではない。
    // SPI設定、シリコンリビジョン、初期化手順を変更した場合は再検証すること。
    constexpr LLI_12BSD_t STOP_NODE = {
        .CTR1 = 0x0002'4002,
        .CTR2 = 0x0000'0406,
        .CBR1 = 4,
        .CSAR = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&STOP_CR1)
        ),
        .CDAR = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&(SPI1->CR1))
        )
    };

    constexpr auto STOP = make_lli(STOP_NODE);
}

namespace gpio_lli
{
    constexpr std::uint32_t HIGH_C = 0x0000'00C0;
    constexpr std::uint32_t LOW_C  = 0x00C0'0000;

    constexpr LLI_12BSD_t HIGH_NODE = {
        .CTR1 = 0x0002'4002,
        .CTR2 = 0x0000'0200,
        .CBR1 = 4,
        .CSAR = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&HIGH_C)
        ),
        .CDAR = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&(GPIOC->BSRR))
        )
    };

    constexpr LLI_2S_t LOW_NODE = {
        .CTR2 = 0x010B'0200,
        .CSAR = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&LOW_C)
        )
    };

    constexpr LLI_SD_t LOW_SOFTWARE_NODE = {
        .CSAR = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&LOW_C)
        ),
        .CDAR = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&(GPIOC->BSRR))
        )
    };

    constexpr auto HIGH         = make_lli(HIGH_NODE);
    constexpr auto LOW          = make_lli(LOW_NODE);
    constexpr auto LOW_SOFTWARE = make_lli(LOW_SOFTWARE_NODE);
}

// ============================================================================
// SPI transfer descriptors
// ============================================================================

constexpr DESC_12BSD_t SPI_TRANSFER_0 = {
    .CTR1 = 0x0000'4008,
    .CTR2 = 0x0000'0407,
    .CBR1 = 10,
    .CSAR = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(&spi_lli::TEST_DATA[10])
    ),
    .CDAR = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(&(SPI1->TXDR))
    )
};

constexpr DESC_12BSD_t SPI_TRANSFER_1 = {
    .CTR1 = 0x0000'4008,
    .CTR2 = 0x0000'0407,
    .CBR1 = 10,
    .CSAR = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(&spi_lli::TEST_DATA[0])
    ),
    .CDAR = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(&(SPI1->TXDR))
    )
};

constexpr DESC_12BSD_t SPI_TRANSFER_2 = {
    .CTR1 = 0x0000'4008,
    .CTR2 = 0x0000'0407,
    .CBR1 = 10,
    .CSAR = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(&spi_lli::TEST_DATA[40])
    ),
    .CDAR = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(&(SPI1->TXDR))
    )
};

constexpr DESC_12BSD_t SPI_TRANSFER_3 = {
    .CTR1 = 0x0000'4008,
    .CTR2 = 0x0000'0407,
    .CBR1 = 10,
    .CSAR = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(&spi_lli::TEST_DATA[12])
    ),
    .CDAR = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(&(SPI1->TXDR))
    )
};

// selector.CSARから32バイト転送すると、先頭12バイトがselectorの
// CSAR/CDAR/CLLRへ、後続20バイトがexecutorのCTR1..CDARへ入る。
// executor.CLLRは転送範囲外なのでROOTの固定リンクを保持する。
struct DescriptorStep
{
    std::uintptr_t nextSelectorSource;
    std::uintptr_t nextSelectorDestination;
    std::uint32_t nextSelectorLink;
    DESC_12BSD_t executor;
};

static_assert(sizeof(void*) == sizeof(std::uint32_t));
static_assert(sizeof(std::uintptr_t) == sizeof(std::uint32_t));
static_assert(std::is_standard_layout_v<DESC_12BSD_t>);
static_assert(std::is_standard_layout_v<DescriptorStep>);
static_assert(sizeof(DESC_12BSD_t) == 20);
static_assert(sizeof(DescriptorStep) == 32);
static_assert(offsetof(DescriptorStep, nextSelectorSource) == 0);
static_assert(offsetof(DescriptorStep, nextSelectorDestination) == 4);
static_assert(offsetof(DescriptorStep, nextSelectorLink) == 8);
static_assert(offsetof(DescriptorStep, executor) == 12);

constexpr LLI_12BSD_t DESCRIPTOR_SELECTOR_NODE = {
    .CTR1 = 0x000A'400A,
    .CTR2 = 0x0000'0200,
    .CBR1 = sizeof(DescriptorStep),
    .CSAR = 0,
    .CDAR = 0,
    .CLLR = 0
};

constexpr LLI_12BSD_t DESCRIPTOR_EXECUTOR_NODE = {
    .CTR1 = 0,
    .CTR2 = 0,
    .CBR1 = 0,
    .CSAR = 0,
    .CDAR = 0,
    .CLLR = 0
};

constexpr auto DESCRIPTOR_SELECTOR_EXECUTOR = make_lli(
    DESCRIPTOR_SELECTOR_NODE,
    DESCRIPTOR_EXECUTOR_NODE
);

// ============================================================================
// ROOT template
// ============================================================================

constexpr auto SPI_GPIO_ROOT_TEMPLATE = [] {
    auto root = make_lli_group(
        spi_lli::SET_DATA_LEN,             // 0
        gpio_lli::LOW_SOFTWARE,            // 1
        spi_lli::START,                    // 2

        spi_lli::SET_DATA_LEN,             // 3
        spi_lli::CLEAR_FLAGS,              // 4
        spi_lli::START,                    // 5

        DESCRIPTOR_SELECTOR_EXECUTOR,      // 6: selector=0, executor=1

        spi_lli::STOP,                     // 7
        gpio_lli::HIGH,                    // 8
        gpio_lli::LOW                      // 9
    );

    patch_jump<node_at<0, 0>, node_at<1, 0>>(root);
    patch_jump<node_at<1, 0>, node_at<2, 0>>(root);
    patch_jump<node_at<2, 0>, node_at<6, 0>>(root);

    patch_jump<node_at<3, 0>, node_at<4, 0>>(root);
    patch_jump<node_at<4, 0>, node_at<5, 0>>(root);
    patch_jump<node_at<5, 0>, node_at<6, 0>>(root);

    // selector.CLLRはdescriptor自身がexecutorへのリンク値へ書き換える。
    patch_jump<node_at<6, 1>, node_at<7, 0>>(root);
    patch_jump<node_at<7, 0>, node_at<8, 0>>(root);
    patch_jump<node_at<8, 0>, node_at<9, 0>>(root);
    patch_jump<node_at<9, 0>, node_at<3, 0>>(root);

    return root;
}();

static_assert(validate_root(SPI_GPIO_ROOT_TEMPLATE));

constexpr std::uint32_t SELECTOR_TO_EXECUTOR_CLLR =
    start_cllr<node_at<6, 1>>(SPI_GPIO_ROOT_TEMPLATE);

using root_t = std::remove_cv_t<decltype(SPI_GPIO_ROOT_TEMPLATE)>;
using descriptor_chain_t = typename group_chain_type<6, root_t>::type;

constexpr std::size_t DESCRIPTOR_CHAIN_OFFSET =
    group_chain_offset<6, root_t>();

constexpr std::size_t SELECTOR_OFFSET =
    DESCRIPTOR_CHAIN_OFFSET +
    chain_node_offset<0, descriptor_chain_t>();

constexpr std::size_t EXECUTOR_OFFSET =
    DESCRIPTOR_CHAIN_OFFSET +
    chain_node_offset<1, descriptor_chain_t>();

constexpr std::size_t DESCRIPTOR_PATCH_BEGIN =
    SELECTOR_OFFSET + offsetof(LLI_12BSD_t, CSAR);

constexpr std::size_t DESCRIPTOR_PATCH_END =
    EXECUTOR_OFFSET + offsetof(LLI_12BSD_t, CLLR);

static_assert((SELECTOR_OFFSET & 0x3u) == 0);
static_assert((EXECUTOR_OFFSET & 0x3u) == 0);
static_assert((DESCRIPTOR_PATCH_BEGIN & 0x3u) == 0);
static_assert(
    DESCRIPTOR_PATCH_END - DESCRIPTOR_PATCH_BEGIN ==
    sizeof(DescriptorStep)
);

// 相互参照用の前方宣言。
extern root_t SPI_GPIO_ROOT;
extern DescriptorStep descriptorRing[4];

// ============================================================================
// Descriptor ring
//
// アドレスはコンパイラ/リンカの静的初期化とリロケーションで解決する。
// ============================================================================

alignas(4)
constinit DescriptorStep descriptorRing[4] = {
    {
        reinterpret_cast<std::uintptr_t>(&descriptorRing[1]),
        reinterpret_cast<std::uintptr_t>(&SPI_GPIO_ROOT) +
            DESCRIPTOR_PATCH_BEGIN,
        SELECTOR_TO_EXECUTOR_CLLR,
        SPI_TRANSFER_0
    },
    {
        reinterpret_cast<std::uintptr_t>(&descriptorRing[2]),
        reinterpret_cast<std::uintptr_t>(&SPI_GPIO_ROOT) +
            DESCRIPTOR_PATCH_BEGIN,
        SELECTOR_TO_EXECUTOR_CLLR,
        SPI_TRANSFER_1
    },
    {
        reinterpret_cast<std::uintptr_t>(&descriptorRing[3]),
        reinterpret_cast<std::uintptr_t>(&SPI_GPIO_ROOT) +
            DESCRIPTOR_PATCH_BEGIN,
        SELECTOR_TO_EXECUTOR_CLLR,
        SPI_TRANSFER_2
    },
    {
        reinterpret_cast<std::uintptr_t>(&descriptorRing[0]),
        reinterpret_cast<std::uintptr_t>(&SPI_GPIO_ROOT) +
            DESCRIPTOR_PATCH_BEGIN,
        SELECTOR_TO_EXECUTOR_CLLR,
        SPI_TRANSFER_3
    }
};

// ============================================================================
// SRAM2 execution ROOT
//
// ROOTの構造、リンク、CBR1、CLLRは静的に完成している。
// FlashからSRAM2へのコピーは完成済みロードイメージの展開であり、
// ROOTの実行時構築ではない。
//
// alpha.1ではselector.CSAR/CDARのみ起動後に接続する。
// 正式版ではリンカリロケーションで解決し、実行時パッチを廃止する。
// ============================================================================

[[gnu::used]]
[[gnu::section(".SRAM2")]]
constinit root_t SPI_GPIO_ROOT = [] {
    auto root = SPI_GPIO_ROOT_TEMPLATE;

    auto& selector = chain_node_at<0>(
        group_chain_at<6>(root)
    ).current;

    selector.CBR1 = sizeof(DescriptorStep);
    selector.CLLR = SELECTOR_TO_EXECUTOR_CLLR;

    return root;
}();

inline LLI_12BSD_t& descriptor_selector()
{
    return chain_node_at<0>(
        group_chain_at<6>(SPI_GPIO_ROOT)
    ).current;
}

inline void connect_descriptor_selector()
{
    auto& selector = descriptor_selector();

    selector.CSAR = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(&descriptorRing[0])
    );

    selector.CDAR = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(&SPI_GPIO_ROOT) +
        DESCRIPTOR_PATCH_BEGIN
    );
}

// ============================================================================
// Application entry
// ============================================================================

namespace user_main
{
    void userMain()
    {
        init::setup();
        UpdateSysTick(80'000'000);

        // alpha.1で残る実行時構築はselectorの絶対アドレス2個だけ。
        connect_descriptor_selector();

        GPDMA1_Channel0->CCR &= ~0x0000'0001;
        GPDMA1_Channel0->CTR2 |= 0x0000'0200;

        load_lli<node_at<0, 0>>(
            *GPDMA1_Channel0,
            SPI_GPIO_ROOT
        );

        GPDMA1_Channel0->CCR |= 0x0000'0001;

        while (1)
        {
        }
    }
}

extern "C" void runUserMain()
{
    user_main::userMain();
}

// ============================================================================
// v2.0.0-alpha.1 provisional SRAM2 startup copy
//
// Flash上の.SRAM2ロードイメージを実行アドレスへコピーする。
// 正式版ではconstructor属性を廃止し、Reset_Handlerから
// __libc_init_array()より前に専用初期化処理を呼び出す。
// ============================================================================

extern "C"
{
    extern std::uint32_t __sram2_load__[];
    extern std::uint32_t __sram2_start__[];
    extern std::uint32_t __sram2_end__[];
}

[[gnu::constructor(101)]]
static void initialize_sram2_alpha_1()
{
    const std::uint32_t* source = __sram2_load__;
    std::uint32_t* destination = __sram2_start__;

    while (destination < __sram2_end__)
    {
        *destination++ = *source++;
    }
}
