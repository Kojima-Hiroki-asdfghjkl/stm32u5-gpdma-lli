#pragma once

#include <cstdint>
#include <cstddef>
#include <concepts>
#include <type_traits>

#include "gpdma_lli_t.hpp"

// CLLRの各レジスタ更新フラグ

namespace GPDMA_Flag
{
    constexpr uint32_t ULL = (1UL << 16);

    constexpr uint32_t UDA = (1UL << 27);
    constexpr uint32_t USA = (1UL << 28);
    constexpr uint32_t UB1 = (1UL << 29);
    constexpr uint32_t UT2 = (1UL << 30);
    constexpr uint32_t UT1 = (1UL << 31);

    constexpr uint32_t UB2 = (1UL << 26);
    constexpr uint32_t UT3 = (1UL << 25);
}

// LLI型が各レジスタを持つか判定する

template<typename, typename = std::void_t<>>
struct has_ctr1 : std::false_type {};
template<typename T>
struct has_ctr1<T, std::void_t<decltype(T::CTR1)>> : std::true_type {};

template<typename, typename = std::void_t<>>
struct has_ctr2 : std::false_type {};
template<typename T>
struct has_ctr2<T, std::void_t<decltype(T::CTR2)>> : std::true_type {};

template<typename, typename = std::void_t<>>
struct has_cbr1 : std::false_type {};
template<typename T>
struct has_cbr1<T, std::void_t<decltype(T::CBR1)>> : std::true_type {};

template<typename, typename = std::void_t<>>
struct has_csar : std::false_type {};
template<typename T>
struct has_csar<T, std::void_t<decltype(T::CSAR)>> : std::true_type {};

template<typename, typename = std::void_t<>>
struct has_cdar : std::false_type {};
template<typename T>
struct has_cdar<T, std::void_t<decltype(T::CDAR)>> : std::true_type {};

template<typename, typename = std::void_t<>>
struct has_ctr3 : std::false_type {};
template<typename T>
struct has_ctr3<T, std::void_t<decltype(T::CTR3)>> : std::true_type {};

template<typename, typename = std::void_t<>>
struct has_cbr2 : std::false_type {};
template<typename T>
struct has_cbr2<T, std::void_t<decltype(T::CBR2)>> : std::true_type {};

// LLIのメンバーがGPDMAのレジスタ順と32ビット幅に並んでいるか検査する。
template<typename T>
consteval bool valid_lli_layout()
{
    if constexpr (
        !std::is_standard_layout_v<T> ||
        !requires(T value) {
            { value.CLLR } -> std::convertible_to<uint32_t>;
        }
    )
    {
        return false;
    }
    else
    {
        std::size_t expected_offset = 0;

        if constexpr (has_ctr1<T>::value)
        {
            if (offsetof(T, CTR1) != expected_offset ||
                sizeof(decltype(T::CTR1)) != sizeof(uint32_t))
                return false;
            expected_offset += sizeof(uint32_t);
        }

        if constexpr (has_ctr2<T>::value)
        {
            if (offsetof(T, CTR2) != expected_offset ||
                sizeof(decltype(T::CTR2)) != sizeof(uint32_t))
                return false;
            expected_offset += sizeof(uint32_t);
        }

        if constexpr (has_cbr1<T>::value)
        {
            if (offsetof(T, CBR1) != expected_offset ||
                sizeof(decltype(T::CBR1)) != sizeof(uint32_t))
                return false;
            expected_offset += sizeof(uint32_t);
        }

        if constexpr (has_csar<T>::value)
        {
            if (offsetof(T, CSAR) != expected_offset ||
                sizeof(decltype(T::CSAR)) != sizeof(uint32_t))
                return false;
            expected_offset += sizeof(uint32_t);
        }

        if constexpr (has_cdar<T>::value)
        {
            if (offsetof(T, CDAR) != expected_offset ||
                sizeof(decltype(T::CDAR)) != sizeof(uint32_t))
                return false;
            expected_offset += sizeof(uint32_t);
        }

        if constexpr (has_ctr3<T>::value)
        {
            if (offsetof(T, CTR3) != expected_offset ||
                sizeof(decltype(T::CTR3)) != sizeof(uint32_t))
                return false;
            expected_offset += sizeof(uint32_t);
        }

        if constexpr (has_cbr2<T>::value)
        {
            if (offsetof(T, CBR2) != expected_offset ||
                sizeof(decltype(T::CBR2)) != sizeof(uint32_t))
                return false;
            expected_offset += sizeof(uint32_t);
        }

        return
            offsetof(T, CLLR) == expected_offset &&
            sizeof(decltype(T::CLLR)) == sizeof(uint32_t) &&
            sizeof(T) == expected_offset + sizeof(uint32_t);
    }
}

// LLI型に含まれるレジスタに対応する更新フラグを生成する

template<typename T>
constexpr uint32_t _calcCLLR()
{
    uint32_t flags = GPDMA_Flag::ULL;

    if constexpr (has_ctr1<T>::value) flags |= GPDMA_Flag::UT1;
    if constexpr (has_ctr2<T>::value) flags |= GPDMA_Flag::UT2;
    if constexpr (has_cbr1<T>::value) flags |= GPDMA_Flag::UB1;
    if constexpr (has_csar<T>::value) flags |= GPDMA_Flag::USA;
    if constexpr (has_cdar<T>::value) flags |= GPDMA_Flag::UDA;
    if constexpr (has_ctr3<T>::value) flags |= GPDMA_Flag::UT3;
    if constexpr (has_cbr2<T>::value) flags |= GPDMA_Flag::UB2;

    return flags;
}

template<typename T>
constexpr uint32_t lli_flags()
{
    return
        _calcCLLR<T>()
        & 0xFFFF0000UL;
}

// CLLRを持つ標準レイアウト型だけをLLIとして受け付ける
template<typename T>
concept LLIType =
    valid_lli_layout<T>();

// 異なるLLI型をそのまま保持する単方向チェーン
template<LLIType... Ts>
struct LLI_Node;

template<LLIType Head>
struct LLI_Node<Head>
{
    Head current;
};

template<
    LLIType Head,
    LLIType Next,
    LLIType... Tail
>
struct LLI_Node<Head, Next, Tail...>
{
    Head current;
    LLI_Node<Next, Tail...> next;
};

// LLI値からチェーンを組み立てる。CLLRはグループ生成時に設定する
template<LLIType Head>
constexpr auto make_lli(Head head)
{
    head.CLLR = 0;

    return LLI_Node<Head>
    {
        head
    };
}

template<
    LLIType Head,
    LLIType Next,
    LLIType... Tail
>
constexpr auto make_lli(
    Head head,
    Next next,
    Tail... tail
)
{
    head.CLLR = 0;

    return LLI_Node<
        Head,
        Next,
        Tail...
    >
    {
        head,
        make_lli(next, tail...)
    };
}

// CLLBAR基準のオフセットとLLI型の更新フラグから次のCLLR値を作る
template<typename Entry>
constexpr uint32_t start_cllr(
    uint32_t offset
)
{
    return
        lli_flags<Entry>()
        |
        (offset & 0xFFFCU);
}

// 複数チェーンをhead/tailの入れ子で保持する
template<typename... CHAINS>
struct lli_group;

template<>
struct lli_group<>
{
};

template<typename HEAD, typename... TAIL>
struct lli_group<HEAD, TAIL...>
{
    HEAD head;
    lli_group<TAIL...> tail;
};

// チェーンの現在ノード型と、末尾かどうかを取得する
template<typename NODE>
struct node_traits;

template<LLIType HEAD>
struct node_traits<
    LLI_Node<HEAD>
>
{
    static constexpr bool last = true;
    static constexpr std::size_t size = 1;

    using current_type = HEAD;
};

template<
    LLIType HEAD,
    LLIType NEXT,
    LLIType... TAIL
>
struct node_traits<
    LLI_Node<
        HEAD,
        NEXT,
        TAIL...
    >
>
{
    static constexpr bool last = false;
    static constexpr std::size_t size =
        1 + node_traits<LLI_Node<NEXT, TAIL...>>::size;

    using current_type = HEAD;

    using next_type =
        LLI_Node<
            NEXT,
            TAIL...
        >;
};

template<typename T>
concept LLIChainType =
    requires {
        typename node_traits<T>::current_type;
    };

// グループ内のチェーン数と、次のtail型を取得する。
template<typename GROUP>
struct group_traits
{
    static constexpr std::size_t chain_count = 0;
};

template<>
struct group_traits<lli_group<>>
{
    static constexpr std::size_t chain_count = 0;
};

template<typename HEAD, typename... TAIL>
struct group_traits<lli_group<HEAD, TAIL...>>
{
    static constexpr std::size_t chain_count =
        1 + group_traits<lli_group<TAIL...>>::chain_count;

    using tail_type = lli_group<TAIL...>;
};

// ROOT内のチェーン番号と、そのチェーン内のLLI番号を0始まりで表す。
template<std::size_t ChainIndex, std::size_t NodeIndex>
struct node_at
{
    static constexpr std::size_t chain_index = ChainIndex;
    static constexpr std::size_t node_index = NodeIndex;
};

template<typename POSITION>
struct node_position_traits
{
    static constexpr bool valid = false;
    static constexpr std::size_t chain_index = 0;
    static constexpr std::size_t node_index = 0;
};

template<std::size_t ChainIndex, std::size_t NodeIndex>
struct node_position_traits<node_at<ChainIndex, NodeIndex>>
{
    static constexpr bool valid = true;
    static constexpr std::size_t chain_index = ChainIndex;
    static constexpr std::size_t node_index = NodeIndex;
};

// 指定したチェーン番号に対応するチェーン型を取得する。
template<std::size_t Index, typename GROUP>
struct group_chain_type;

template<typename HEAD, typename... TAIL>
struct group_chain_type<0, lli_group<HEAD, TAIL...>>
{
    using type = HEAD;
};

template<std::size_t Index, typename HEAD, typename... TAIL>
    requires (Index > 0)
struct group_chain_type<Index, lli_group<HEAD, TAIL...>>
    : group_chain_type<Index - 1, lli_group<TAIL...>>
{
};

template<std::size_t ChainIndex, std::size_t NodeIndex, typename GROUP>
concept ValidGroupNode =
    (ChainIndex <
        group_traits<std::remove_cvref_t<GROUP>>::chain_count) &&
    requires {
        typename group_chain_type<
            ChainIndex,
            std::remove_cvref_t<GROUP>
        >::type;
        requires (
            NodeIndex <
            node_traits<
                typename group_chain_type<
                    ChainIndex,
                    std::remove_cvref_t<GROUP>
                >::type
            >::size
        );
    };

template<typename POSITION, typename GROUP>
concept ValidNodePosition =
    node_position_traits<POSITION>::valid &&
    ValidGroupNode<
        node_position_traits<POSITION>::chain_index,
        node_position_traits<POSITION>::node_index,
        GROUP
    >;

// 指定したチェーンの実体をROOTから取得する。
template<std::size_t Index, typename HEAD, typename... TAIL>
constexpr decltype(auto) group_chain_at(
    lli_group<HEAD, TAIL...>& group
)
{
    if constexpr (Index == 0)
    {
        return (group.head);
    }
    else
    {
        return group_chain_at<Index - 1>(group.tail);
    }
}

template<std::size_t Index, typename HEAD, typename... TAIL>
constexpr decltype(auto) group_chain_at(
    const lli_group<HEAD, TAIL...>& group
)
{
    if constexpr (Index == 0)
    {
        return (group.head);
    }
    else
    {
        return group_chain_at<Index - 1>(group.tail);
    }
}

// チェーン内の指定したノードを取得する。
template<std::size_t Index, typename NODE>
constexpr decltype(auto) chain_node_at(
    NODE& node
)
{
    if constexpr (Index == 0)
    {
        return (node);
    }
    else
    {
        return chain_node_at<Index - 1>(node.next);
    }
}

template<std::size_t Index, typename NODE>
constexpr decltype(auto) chain_node_at(
    const NODE& node
)
{
    if constexpr (Index == 0)
    {
        return (node);
    }
    else
    {
        return chain_node_at<Index - 1>(node.next);
    }
}

// 指定したチェーンの開始位置をROOT先頭からのオフセットで求める。
template<std::size_t Index, typename GROUP>
constexpr std::size_t group_chain_offset()
{
    using group_t = std::remove_cvref_t<GROUP>;

    if constexpr (Index == 0)
    {
        return offsetof(group_t, head);
    }
    else
    {
        using tail_t = typename group_traits<group_t>::tail_type;

        return
            offsetof(group_t, tail) +
            group_chain_offset<Index - 1, tail_t>();
    }
}

// チェーン内の指定ノード位置をチェーン先頭からのオフセットで求める。
template<std::size_t Index, typename NODE>
constexpr std::size_t chain_node_offset()
{
    if constexpr (Index == 0)
    {
        return offsetof(NODE, current);
    }
    else
    {
        using next_t = typename node_traits<NODE>::next_type;

        return
            offsetof(NODE, next) +
            chain_node_offset<Index - 1, next_t>();
    }
}

// チェーン内の指定ノード型を取得する。
template<std::size_t Index, typename NODE>
struct chain_node_type;

template<typename HEAD, typename... TAIL>
struct chain_node_type<0, LLI_Node<HEAD, TAIL...>>
{
    using type = LLI_Node<HEAD, TAIL...>;
};

template<std::size_t Index, typename HEAD, typename NEXT, typename... TAIL>
    requires (Index > 0)
struct chain_node_type<Index, LLI_Node<HEAD, NEXT, TAIL...>>
    : chain_node_type<Index - 1, LLI_Node<NEXT, TAIL...>>
{
};

// ROOT内のチェーン先頭LLIのオフセットを型から求める。
template<std::size_t Index, typename GROUP>
    requires (
        Index <
        group_traits<std::remove_cvref_t<GROUP>>::chain_count
    )
constexpr std::size_t lli_offset()
{
    using group_t = std::remove_cvref_t<GROUP>;
    using chain_t =
        typename group_chain_type<Index, group_t>::type;

    return
        group_chain_offset<Index, group_t>() +
        chain_node_offset<0, chain_t>();
}

// チェーン番号からLLI型とROOT基準のオフセットを求め、開始用CLLRを生成する。
template<std::size_t Index, typename... CHAINS>
    requires ValidGroupNode<Index, 0, lli_group<CHAINS...>>
constexpr uint32_t start_cllr(
    const lli_group<CHAINS...>&
)
{
    using chain_t =
        typename group_chain_type<Index, lli_group<CHAINS...>>::type;
    using lli_t = typename node_traits<chain_t>::current_type;

    return start_cllr<lli_t>(
        static_cast<uint32_t>(
            lli_offset<Index, lli_group<CHAINS...>>()
        )
    );
}

// node_atで指定した位置のLLI型とROOT基準オフセットから開始用CLLRを生成する。
template<typename POSITION, typename... CHAINS>
    requires ValidNodePosition<POSITION, lli_group<CHAINS...>>
constexpr uint32_t start_cllr(
    const lli_group<CHAINS...>&
)
{
    using root_t = lli_group<CHAINS...>;
    using position_t = node_position_traits<POSITION>;
    using chain_t =
        typename group_chain_type<position_t::chain_index, root_t>::type;
    using node_t =
        typename chain_node_type<position_t::node_index, chain_t>::type;
    using lli_t = typename node_traits<node_t>::current_type;

    constexpr std::size_t offset =
        group_chain_offset<position_t::chain_index, root_t>() +
        chain_node_offset<position_t::node_index, chain_t>();

    static_assert(
        (offset & 0x3U) == 0,
        "LLI start target must be 4-byte aligned"
    );
    static_assert(
        offset <= 0xFFFCU,
        "LLI start target exceeds the 16-bit CLLR.LA offset range"
    );

    return start_cllr<lli_t>(static_cast<uint32_t>(offset));
}

template<
    std::size_t ChainIndex,
    std::size_t NodeIndex,
    typename... CHAINS
>
constexpr bool cllr_target_exists(
    const lli_group<CHAINS...>& root,
    uint32_t cllr
)
{
    using root_t = lli_group<CHAINS...>;
    using chain_t =
        typename group_chain_type<ChainIndex, root_t>::type;
    using node_t =
        typename chain_node_type<NodeIndex, chain_t>::type;
    using lli_t = typename node_traits<node_t>::current_type;

    constexpr std::size_t target_offset =
        group_chain_offset<ChainIndex, root_t>() +
        chain_node_offset<NodeIndex, chain_t>();

    if constexpr (
        target_offset <= 0xFFFCU &&
        (target_offset & 0x3U) == 0
    )
    {
        if (cllr == start_cllr<lli_t>(
                static_cast<uint32_t>(target_offset)
            ))
        {
            return true;
        }
    }

    if constexpr (NodeIndex + 1 < node_traits<chain_t>::size)
    {
        return cllr_target_exists<ChainIndex, NodeIndex + 1>(root, cllr);
    }
    else if constexpr (ChainIndex + 1 < sizeof...(CHAINS))
    {
        return cllr_target_exists<ChainIndex + 1, 0>(root, cllr);
    }
    else
    {
        return false;
    }
}

template<typename... CHAINS>
constexpr bool valid_cllr_target(
    const lli_group<CHAINS...>& root,
    uint32_t cllr
)
{
    if (cllr == 0)
    {
        return true;
    }

    const uint32_t target_offset = cllr & 0xFFFCU;
    return
        (cllr & 0x3U) == 0 &&
        target_offset < sizeof(root) &&
        cllr_target_exists<0, 0>(root, cllr);
}

template<
    std::size_t ChainIndex,
    std::size_t NodeIndex,
    typename... CHAINS
>
constexpr bool chain_targets_valid(
    const lli_group<CHAINS...>& root
)
{
    using root_t = lli_group<CHAINS...>;
    using chain_t =
        typename group_chain_type<ChainIndex, root_t>::type;

    constexpr std::size_t node_offset =
        group_chain_offset<ChainIndex, root_t>() +
        chain_node_offset<NodeIndex, chain_t>();
    const auto& node =
        chain_node_at<NodeIndex>(
            group_chain_at<ChainIndex>(root)
        );

    if constexpr (
        node_offset > 0xFFFCU ||
        (node_offset & 0x3U) != 0
    )
    {
        return false;
    }
    else if (!valid_cllr_target(root, node.current.CLLR))
    {
        return false;
    }
    else if constexpr (NodeIndex + 1 < node_traits<chain_t>::size)
    {
        return chain_targets_valid<ChainIndex, NodeIndex + 1>(root);
    }
    else if constexpr (ChainIndex + 1 < sizeof...(CHAINS))
    {
        return chain_targets_valid<ChainIndex + 1, 0>(root);
    }
    else
    {
        return true;
    }
}

// ROOT内の全LLIのCLLRが終端か、有効なROOT内ノードを指すことを検証する。
template<LLIChainType... CHAINS>
    requires (sizeof...(CHAINS) > 0)
constexpr bool validate_root(
    const lli_group<CHAINS...>& root
)
{
    using root_t = lli_group<CHAINS...>;

    if constexpr (
        !std::is_standard_layout_v<root_t> ||
        sizeof(root_t) > 0x10000U
    )
    {
        return false;
    }
    else
    {
        return chain_targets_valid<0, 0>(root);
    }
}

template<typename CHANNEL>
concept GPDMAChannel =
    requires(CHANNEL& channel, uint32_t value) {
        channel.CLBAR = value;
        channel.CLLR = value;
        channel.CCR |= value;
    };

// ROOTの配置先をCLBAR/CLLRへ設定する。64KB境界配置はリンカ側で保証する。
template<typename POSITION, GPDMAChannel CHANNEL, typename... CHAINS>
    requires ValidNodePosition<POSITION, lli_group<CHAINS...>>
inline void dma_set_lli_start(
    CHANNEL& channel,
    const lli_group<CHAINS...>& root
)
{
    const uintptr_t root_address =
        reinterpret_cast<uintptr_t>(&root);

    channel.CLBAR =
        static_cast<uint32_t>(root_address);
    channel.CLLR = start_cllr<POSITION>(root);
}

// LLI開始位置を設定してチャネルを有効化する。
// 停止要求やトリガ設定は呼び出し側で行う。
template<typename POSITION, GPDMAChannel CHANNEL, typename... CHAINS>
    requires ValidNodePosition<POSITION, lli_group<CHAINS...>>
inline void dma_start(
    CHANNEL& channel,
    const lli_group<CHAINS...>& root
)
{
    dma_set_lli_start<POSITION>(channel, root);
    channel.CCR |= 0x00000001U;
}

// チェーン内を再帰し、次のLLIへのCLLRを設定する。
// current_offsetとnext_deltaは、どちらもROOT先頭からのバイトオフセット。
template<typename NODE>
constexpr void fixup_chain(
    NODE& node,
    uint32_t current_offset
)
{
    if constexpr (node_traits<NODE>::last)
    {
        node.current.CLLR = 0;
    }
    else
    {
        using next_node_t =
            typename node_traits<NODE>::next_type;

        using next_lli_t =
            typename node_traits<
                next_node_t
            >::current_type;

        using node_t = NODE;

        constexpr uint32_t next_delta =
            offsetof(node_t, next.current);

        const uint32_t next_offset =
            current_offset +
            next_delta;

        node.current.CLLR =
            start_cllr<next_lli_t>(
                next_offset
            );

        fixup_chain(
            node.next,
            next_offset
        );
    }
}

// 直線リンクのCLLRが、チェーン内の実際の配置と一致するか検査する。
template<typename NODE>
constexpr bool chain_links_valid(
    const NODE& node,
    uint32_t current_offset
)
{
    if constexpr (node_traits<NODE>::last)
    {
        return node.current.CLLR == 0;
    }
    else
    {
        using next_node_t =
            typename node_traits<NODE>::next_type;
        using next_lli_t =
            typename node_traits<next_node_t>::current_type;

        constexpr uint32_t next_delta =
            offsetof(NODE, next.current);
        const uint32_t next_offset =
            current_offset + next_delta;

        return
            node.current.CLLR == start_cllr<next_lli_t>(next_offset) &&
            chain_links_valid(node.next, next_offset);
    }
}

// 直線リンクをROOT内の任意のLLIへ向かうリンクに置き換える。
// リンク先LLIの型から更新フラグを選び、ROOT基準のオフセットを設定する。
template<typename FROM, typename TO, typename... CHAINS>
    requires
        ValidNodePosition<FROM, lli_group<CHAINS...>> &&
        ValidNodePosition<TO, lli_group<CHAINS...>>
constexpr void patch_jump(
    lli_group<CHAINS...>& root
)
{
    using root_t = lli_group<CHAINS...>;
    using from_t = node_position_traits<FROM>;
    using to_t = node_position_traits<TO>;

    using target_chain_t =
        typename group_chain_type<
            to_t::chain_index,
            root_t
        >::type;

    using target_node_t = std::remove_cvref_t<
        decltype(
            chain_node_at<to_t::node_index>(
                group_chain_at<to_t::chain_index>(root)
            )
        )
    >;

    using target_lli_t =
        typename node_traits<target_node_t>::current_type;

    constexpr std::size_t target_offset =
        group_chain_offset<to_t::chain_index, root_t>() +
        chain_node_offset<to_t::node_index, target_chain_t>();

    static_assert(
        (target_offset & 0x3U) == 0,
        "LLI jump target must be 4-byte aligned"
    );
    static_assert(
        target_offset <= 0xFFFCU,
        "LLI jump target exceeds the 16-bit CLLR.LA offset range"
    );

    auto& from_node = chain_node_at<from_t::node_index>(
        group_chain_at<from_t::chain_index>(root)
    );
    const uint32_t target_cllr =
        start_cllr<target_lli_t>(
            static_cast<uint32_t>(target_offset)
        );
    from_node.current.CLLR = target_cllr;
}

// チェーンがなくなったら再帰を終了する
constexpr void fixup_group(
    lli_group<>&,
    uint32_t
)
{
}

// グループ内の各チェーンを検証する。base_offsetはROOT先頭からの位置。
constexpr bool group_links_valid(
    const lli_group<>&,
    uint32_t
)
{
    return true;
}

template<
    typename HEAD,
    typename... TAIL
>
constexpr void fixup_group(
    lli_group<
        HEAD,
        TAIL...
    >& group,
    uint32_t base_offset
)
{
    using group_t =
        lli_group<
            HEAD,
            TAIL...
        >;

    // このチェーンの開始位置を、入れ子グループのオフセットに加算する。
    fixup_chain(
        group.head,
        base_offset +
        offsetof(
            group_t,
            head
        )
    );

    // tailの実オフセットを次の再帰に渡し、ROOT基準の位置を保つ。
    const uint32_t next_group_offset =
        base_offset +
        offsetof(
            group_t,
            tail
        );

    fixup_group(
        group.tail,
        next_group_offset
    );
}

template<
    typename HEAD,
    typename... TAIL
>
constexpr bool group_links_valid(
    const lli_group<HEAD, TAIL...>& group,
    uint32_t base_offset
)
{
    using group_t = lli_group<HEAD, TAIL...>;
    const uint32_t chain_offset =
        base_offset + offsetof(group_t, head);
    const uint32_t next_group_offset =
        base_offset + offsetof(group_t, tail);

    return
        chain_links_valid(group.head, chain_offset) &&
        group_links_valid(group.tail, next_group_offset);
}

// チェーンをグループに格納し、各CLLRをROOT基準の位置で設定する
template<LLIChainType... CHAINS>
    requires (sizeof...(CHAINS) > 0)
constexpr auto make_lli_group(
    CHAINS... chains
)
{
    using root_t = lli_group<CHAINS...>;

    static_assert(
        std::is_standard_layout_v<root_t>,
        "LLI group must have a standard memory layout"
    );
    static_assert(
        sizeof(root_t) <= 0x10000,
        "LLI group must fit within the 16-bit CLLR.LA offset range"
    );

    auto root =
        root_t
        {
            chains...
        };

    fixup_group(root, 0);

    // 定数評価中に不整合があればコンパイルエラーにする。
    // 実行時生成でも不整合を検出できるよう、失敗時は停止する。
    if (!group_links_valid(root, 0))
    {
        __builtin_trap();
    }

    return root;
}
