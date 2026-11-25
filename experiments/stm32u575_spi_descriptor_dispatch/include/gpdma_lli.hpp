#pragma once

#include <cstdint>
#include <cstddef>
#include <concepts>
#include <tuple>
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

template<typename ExecutorImage>
inline constexpr bool supported_descriptor_executor_image_v = false;

template<>
inline constexpr bool supported_descriptor_executor_image_v<DESC_12BSD_t> = true;

// selectorのCSAR/CDAR/CLLRとexecutor更新像を連続配置する。
// v2.0.0では完全な1D executor像であるDESC_12BSD_tだけを受け付ける。
template<typename ExecutorImage>
    requires supported_descriptor_executor_image_v<ExecutorImage>
struct descriptor_step
{
    union
    {
        uint32_t next_selector_csar;
        const descriptor_step* next_selector_csar_pointer;
    };
    union
    {
        uint32_t next_selector_cdar;
        uint32_t* next_selector_cdar_pointer;
    };
    uint32_t next_selector_cllr;
    ExecutorImage executor;
};

static_assert(std::is_standard_layout_v<descriptor_step<DESC_12BSD_t>>);
static_assert(sizeof(descriptor_step<DESC_12BSD_t>) == 32);
static_assert(offsetof(descriptor_step<DESC_12BSD_t>, executor) == 12);

// descriptor dispatcherの初期descriptor番号を型で指定する。
template<std::size_t Index>
struct start_at
{
    static constexpr std::size_t index = Index;
};

// Group/Listタグと物理チェーンを結び付ける。
// タグは型情報だけに使い、ROOTにはチェーンのレジスタ像だけを保持する。
template<
    typename GroupTag,
    typename ListTag,
    typename Chain,
    bool ExplicitEnd = false
>
struct lli_list
{
    using group_tag = GroupTag;
    using list_tag = ListTag;
    using chain_type = Chain;

    static constexpr bool explicit_end = ExplicitEnd;

    Chain chain;
};

template<typename GroupTag, typename ListTag, typename Chain>
constexpr auto make_list(Chain chain)
{
    return lli_list<GroupTag, ListTag, Chain>{chain};
}

// 単一Group用の簡易APIで内部補完するGroupタグ。
struct default_group_tag
{
};

// 完全タグ無しListを名前付き位置APIの対象外として識別する。
template<typename Chain>
struct anonymous_list_tag
{
};

// Groupタグだけを省略し、Listタグによる名前付き位置指定を維持する。
template<typename ListTag, typename Chain>
constexpr auto make_list(Chain chain)
{
    return make_list<default_group_tag, ListTag>(chain);
}

// Group/Listタグを両方省略する。
// Chain型が同じListを複数含めても、名前付き位置APIを使わないため許可する。
template<typename Chain>
constexpr auto make_list(Chain chain)
{
    return make_list<default_group_tag, anonymous_list_tag<Chain>>(chain);
}

// List末尾を同一Groupタグ内の次Listへ自動接続しない。
template<
    typename GroupTag,
    typename ListTag,
    typename Chain,
    bool ExplicitEnd
>
constexpr auto end_list(
    const lli_list<GroupTag, ListTag, Chain, ExplicitEnd>& list
)
{
    return lli_list<GroupTag, ListTag, Chain, true>{list.chain};
}

// 任意のタグに再利用時のインスタンス識別子を付加する。
template<typename InstanceTag, typename Tag>
struct instanced_tag
{
    using instance_tag = InstanceTag;
    using base_tag = Tag;
};

// ROOT内の位置をGroup/ListタグとList内ノード番号で表す。
template<typename GroupTag, typename ListTag, std::size_t NodeIndex>
struct node_at
{
    using group_tag = GroupTag;
    using list_tag = ListTag;

    static constexpr std::size_t node_index = NodeIndex;
};

// Listの先頭または末尾を、Node番号に依存せずに表す。
template<typename GroupTag, typename ListTag>
struct list_entry
{
    using group_tag = GroupTag;
    using list_tag = ListTag;
};

template<typename GroupTag, typename ListTag>
struct list_tail
{
    using group_tag = GroupTag;
    using list_tag = ListTag;
};

template<typename T>
struct is_node_position : std::false_type
{
};

template<typename GroupTag, typename ListTag, std::size_t NodeIndex>
struct is_node_position<node_at<GroupTag, ListTag, NodeIndex>> : std::true_type
{
};

template<typename GroupTag, typename ListTag>
struct is_node_position<list_entry<GroupTag, ListTag>> : std::true_type
{
};

template<typename GroupTag, typename ListTag>
struct is_node_position<list_tail<GroupTag, ListTag>> : std::true_type
{
};

template<typename T>
inline constexpr bool is_node_position_v =
    is_node_position<std::remove_cvref_t<T>>::value;

template<typename T>
concept NodePosition = is_node_position_v<T>;

// Group構築時に指定するノード間ジャンプ配線ルール。
template<NodePosition From, NodePosition To>
struct jump
{
    using from_position = std::remove_cvref_t<From>;
    using to_position = std::remove_cvref_t<To>;
};

// Group構築時に指定するノード終端配線ルール。
template<NodePosition Position>
struct end_node
{
    using position = std::remove_cvref_t<Position>;
};

template<typename T>
struct is_wiring_rule : std::false_type
{
};

template<NodePosition From, NodePosition To>
struct is_wiring_rule<jump<From, To>> : std::true_type
{
};

template<NodePosition Position>
struct is_wiring_rule<end_node<Position>> : std::true_type
{
};

template<typename T>
inline constexpr bool is_wiring_rule_v =
    is_wiring_rule<std::remove_cvref_t<T>>::value;

template<typename T>
concept WiringRule = is_wiring_rule_v<T>;

template<WiringRule... RULES>
struct wiring_rules
{
};

template<typename... PACKS>
struct concat_wiring_rules;

template<>
struct concat_wiring_rules<>
{
    using type = wiring_rules<>;
};

template<WiringRule... RULES>
struct concat_wiring_rules<wiring_rules<RULES...>>
{
    using type = wiring_rules<RULES...>;
};

template<WiringRule... LEFT, WiringRule... RIGHT, typename... REST>
struct concat_wiring_rules<wiring_rules<LEFT...>, wiring_rules<RIGHT...>, REST...>
    : concat_wiring_rules<wiring_rules<LEFT..., RIGHT...>, REST...>
{
};

template<typename... PACKS>
using concat_wiring_rules_t = typename concat_wiring_rules<PACKS...>::type;

template<typename T>
struct list_traits;

template<
    typename GroupTag,
    typename ListTag,
    typename Chain,
    bool ExplicitEnd
>
struct list_traits<lli_list<GroupTag, ListTag, Chain, ExplicitEnd>>
{
    using group_tag = GroupTag;
    using list_tag = ListTag;
    using chain_type = Chain;

    static constexpr bool explicit_end = ExplicitEnd;
};

template<typename T>
struct is_anonymous_list_tag : std::false_type
{
};

template<typename Chain>
struct is_anonymous_list_tag<anonymous_list_tag<Chain>> : std::true_type
{
};

template<typename T>
inline constexpr bool is_anonymous_list_tag_v =
    is_anonymous_list_tag<T>::value;

template<typename... LISTS>
struct unique_group_list_tags : std::true_type
{
};

template<typename HEAD, typename... TAIL>
struct unique_group_list_tags<HEAD, TAIL...>
    : std::bool_constant<
        (is_anonymous_list_tag_v<typename list_traits<HEAD>::list_tag> ||
         (!(std::is_same_v<
                typename list_traits<HEAD>::group_tag,
                typename list_traits<TAIL>::group_tag
            > &&
            std::is_same_v<
                typename list_traits<HEAD>::list_tag,
                typename list_traits<TAIL>::list_tag
            >) && ...)) &&
        unique_group_list_tags<TAIL...>::value
    >
{
};

template<typename... LISTS>
inline constexpr bool unique_group_list_tags_v =
    unique_group_list_tags<LISTS...>::value;

// 複数の名前付きListをhead/tailの入れ子で保持する。
template<typename Rules, typename... LISTS>
struct lli_group;

template<typename... LISTS>
struct finalized_lli_group;

template<typename T>
struct is_finalized_lli_group : std::false_type
{
};

template<typename... LISTS>
struct is_finalized_lli_group<finalized_lli_group<LISTS...>> : std::true_type
{
};

template<typename T>
inline constexpr bool is_finalized_lli_group_v =
    is_finalized_lli_group<std::remove_cvref_t<T>>::value;

template<WiringRule... RULES>
struct lli_group<wiring_rules<RULES...>>
{
    using rules_type = wiring_rules<RULES...>;
};

template<WiringRule... RULES, typename HEAD, typename... TAIL>
struct lli_group<wiring_rules<RULES...>, HEAD, TAIL...>
{
    using rules_type = wiring_rules<RULES...>;

    HEAD head;
    lli_group<wiring_rules<>, TAIL...> tail;
};

template<typename Rules>
constexpr auto make_raw_lli_group()
{
    return lli_group<Rules>{};
}

template<typename Rules, typename HEAD, typename... TAIL>
constexpr auto make_raw_lli_group(const HEAD& head, const TAIL&... tail)
{
    return lli_group<Rules, HEAD, TAIL...>{
        head,
        make_raw_lli_group<wiring_rules<>>(tail...)
    };
}

template<typename... LISTS>
struct finalized_lli_group
{
    using base_type = lli_group<wiring_rules<>, LISTS...>;

    base_type root;

    constexpr finalized_lli_group() = default;

    constexpr explicit finalized_lli_group(const base_type& value)
        : root(value)
    {
    }

    constexpr const base_type& data() const noexcept
    {
        return root;
    }

    constexpr base_type& data() noexcept
    {
        return root;
    }
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

template<WiringRule... RULES>
struct group_traits<lli_group<wiring_rules<RULES...>>>
{
    static constexpr std::size_t chain_count = 0;
};

template<WiringRule... RULES, typename HEAD, typename... TAIL>
struct group_traits<lli_group<wiring_rules<RULES...>, HEAD, TAIL...>>
{
    static constexpr std::size_t chain_count =
        1 + group_traits<lli_group<wiring_rules<>, TAIL...>>::chain_count;

    using tail_type = lli_group<wiring_rules<>, TAIL...>;
};

template<typename... LISTS>
struct group_traits<finalized_lli_group<LISTS...>>
    : group_traits<lli_group<wiring_rules<>, LISTS...>>
{
};

template<typename GroupTag, typename ListTag, typename GROUP>
struct group_list_index;

template<
    typename GroupTag,
    typename ListTag,
    WiringRule... RULES,
    typename HEAD,
    typename... TAIL
>
struct group_list_index<
    GroupTag,
    ListTag,
    lli_group<wiring_rules<RULES...>, HEAD, TAIL...>
>
{
private:
    static constexpr bool matches =
        std::is_same_v<GroupTag, typename list_traits<HEAD>::group_tag> &&
        std::is_same_v<ListTag, typename list_traits<HEAD>::list_tag>;

public:
    static constexpr std::size_t value = [] {
        if constexpr (matches)
        {
            return std::size_t{0};
        }
        else
        {
            return std::size_t{1} +
                group_list_index<
                    GroupTag,
                    ListTag,
                    lli_group<wiring_rules<>, TAIL...>
                >::value;
        }
    }();
};

template<typename GroupTag, typename ListTag, WiringRule... RULES>
struct group_list_index<GroupTag, ListTag, lli_group<wiring_rules<RULES...>>>
{
    static constexpr std::size_t value = 0;
};

template<typename GroupTag, typename ListTag, typename... LISTS>
struct group_list_index<
    GroupTag,
    ListTag,
    finalized_lli_group<LISTS...>
> : group_list_index<GroupTag, ListTag, lli_group<wiring_rules<>, LISTS...>>
{
};

template<typename GroupTag, typename ListTag, typename GROUP>
inline constexpr bool group_has_list_v =
    group_list_index<
        GroupTag,
        ListTag,
        std::remove_cvref_t<GROUP>
    >::value <
    group_traits<std::remove_cvref_t<GROUP>>::chain_count;

template<typename POSITION, typename GROUP>
struct node_position_traits
{
    static constexpr bool valid = false;
    static constexpr std::size_t chain_index = 0;
    static constexpr std::size_t node_index = 0;
};

// 指定したチェーン番号に対応するチェーン型を取得する。
template<std::size_t Index, typename GROUP>
struct group_chain_type;

template<WiringRule... RULES, typename HEAD, typename... TAIL>
struct group_chain_type<0, lli_group<wiring_rules<RULES...>, HEAD, TAIL...>>
{
    using type = typename list_traits<HEAD>::chain_type;
};

template<std::size_t Index, WiringRule... RULES, typename HEAD, typename... TAIL>
    requires (Index > 0)
struct group_chain_type<Index, lli_group<wiring_rules<RULES...>, HEAD, TAIL...>>
    : group_chain_type<Index - 1, lli_group<wiring_rules<>, TAIL...>>
{
};

template<std::size_t Index, typename... LISTS>
struct group_chain_type<Index, finalized_lli_group<LISTS...>>
    : group_chain_type<Index, lli_group<wiring_rules<>, LISTS...>>
{
};

template<
    typename GroupTag,
    typename ListTag,
    std::size_t NodeIndex,
    typename GROUP
>
struct node_position_traits<
    node_at<GroupTag, ListTag, NodeIndex>,
    GROUP
>
{
    static constexpr bool valid =
        group_has_list_v<GroupTag, ListTag, GROUP>;
    static constexpr std::size_t chain_index =
        group_list_index<
            GroupTag,
            ListTag,
            std::remove_cvref_t<GROUP>
        >::value;
    static constexpr std::size_t node_index = NodeIndex;
};

template<typename GroupTag, typename ListTag, typename GROUP>
struct node_position_traits<
    list_entry<GroupTag, ListTag>,
    GROUP
>
{
    static constexpr bool valid =
        group_has_list_v<GroupTag, ListTag, GROUP>;
    static constexpr std::size_t chain_index =
        group_list_index<
            GroupTag,
            ListTag,
            std::remove_cvref_t<GROUP>
        >::value;
    static constexpr std::size_t node_index = 0;
};

template<bool Valid, std::size_t ChainIndex, typename GROUP>
struct list_tail_node_index
{
    static constexpr std::size_t value = 0;
};

template<std::size_t ChainIndex, typename GROUP>
struct list_tail_node_index<true, ChainIndex, GROUP>
{
    using chain_type =
        typename group_chain_type<
            ChainIndex,
            GROUP
        >::type;

    static constexpr std::size_t value =
        node_traits<chain_type>::size - 1;
};

template<typename GroupTag, typename ListTag, typename GROUP>
struct node_position_traits<
    list_tail<GroupTag, ListTag>,
    GROUP
>
{
    static constexpr bool valid =
        group_has_list_v<GroupTag, ListTag, GROUP>;
    static constexpr std::size_t chain_index =
        group_list_index<
            GroupTag,
            ListTag,
            std::remove_cvref_t<GROUP>
        >::value;
    static constexpr std::size_t node_index =
        list_tail_node_index<
            valid,
            chain_index,
            std::remove_cvref_t<GROUP>
        >::value;
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
    node_position_traits<POSITION, GROUP>::valid &&
    ValidGroupNode<
        node_position_traits<POSITION, GROUP>::chain_index,
        node_position_traits<POSITION, GROUP>::node_index,
        GROUP
    >;

// 指定したチェーンの実体をROOTから取得する。
template<std::size_t Index, WiringRule... RULES, typename HEAD, typename... TAIL>
constexpr decltype(auto) group_chain_at(
    lli_group<wiring_rules<RULES...>, HEAD, TAIL...>& group
)
{
    if constexpr (Index == 0)
    {
        return (group.head.chain);
    }
    else
    {
        return group_chain_at<Index - 1>(group.tail);
    }
}

template<std::size_t Index, WiringRule... RULES, typename HEAD, typename... TAIL>
constexpr decltype(auto) group_chain_at(
    const lli_group<wiring_rules<RULES...>, HEAD, TAIL...>& group
)
{
    if constexpr (Index == 0)
    {
        return (group.head.chain);
    }
    else
    {
        return group_chain_at<Index - 1>(group.tail);
    }
}

template<std::size_t Index, typename... LISTS>
constexpr decltype(auto) group_chain_at(
    finalized_lli_group<LISTS...>& group
)
{
    return group_chain_at<Index>(group.root);
}

template<std::size_t Index, typename... LISTS>
constexpr decltype(auto) group_chain_at(
    const finalized_lli_group<LISTS...>& group
)
{
    return group_chain_at<Index>(group.root);
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
template<typename GROUP>
struct mutable_group_type
{
    using type = std::remove_cvref_t<GROUP>;
};

template<typename... LISTS>
struct mutable_group_type<finalized_lli_group<LISTS...>>
{
    using type = lli_group<wiring_rules<>, LISTS...>;
};

template<typename GROUP>
using mutable_group_type_t =
    typename mutable_group_type<std::remove_cvref_t<GROUP>>::type;

template<std::size_t Index, typename GROUP>
constexpr std::size_t group_chain_offset()
{
    using group_t = mutable_group_type_t<GROUP>;

    if constexpr (Index == 0)
    {
        using list_t =
            std::remove_cvref_t<decltype(group_t::head)>;

        return
            offsetof(group_t, head) +
            offsetof(list_t, chain);
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
template<std::size_t Index, WiringRule... RULES, typename... CHAINS>
    requires ValidGroupNode<Index, 0, lli_group<wiring_rules<RULES...>, CHAINS...>>
constexpr uint32_t start_cllr(
    const lli_group<wiring_rules<RULES...>, CHAINS...>&
)
{
    using group_t = lli_group<wiring_rules<RULES...>, CHAINS...>;
    using chain_t =
        typename group_chain_type<Index, group_t>::type;
    using lli_t = typename node_traits<chain_t>::current_type;

    return start_cllr<lli_t>(
        static_cast<uint32_t>(
            lli_offset<Index, group_t>()
        )
    );
}

// node_atで指定した位置のLLI型とROOT基準オフセットから開始用CLLRを生成する。
template<typename POSITION, typename ROOT>
    requires ValidNodePosition<POSITION, std::remove_cvref_t<ROOT>>
constexpr uint32_t start_cllr(
    const ROOT&
)
{
    using root_t = std::remove_cvref_t<ROOT>;
    using position_t = node_position_traits<POSITION, root_t>;
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

// descriptor dispatcherの物理配置とpatch範囲をROOT型から導出する。
// 呼び出し側にはselector/executorのオフセット計算を公開しない。
template<
    typename SelectorPosition,
    typename ExecutorPosition,
    typename ExecutorImage,
    typename ROOT
>
    requires (
        is_finalized_lli_group_v<ROOT> &&
        supported_descriptor_executor_image_v<ExecutorImage> &&
        ValidNodePosition<SelectorPosition, std::remove_cvref_t<ROOT>> &&
        ValidNodePosition<ExecutorPosition, std::remove_cvref_t<ROOT>>
    )
struct descriptor_dispatch_layout
{
    using root_type = std::remove_cvref_t<ROOT>;
    using step_type = descriptor_step<ExecutorImage>;

private:
    using selector_position =
        node_position_traits<SelectorPosition, root_type>;
    using executor_position =
        node_position_traits<ExecutorPosition, root_type>;
    using selector_chain =
        typename group_chain_type<
            selector_position::chain_index,
            root_type
        >::type;
    using executor_chain =
        typename group_chain_type<
            executor_position::chain_index,
            root_type
        >::type;
    using selector_node =
        typename chain_node_type<
            selector_position::node_index,
            selector_chain
        >::type;
    using executor_node =
        typename chain_node_type<
            executor_position::node_index,
            executor_chain
        >::type;

    using selector_image =
        typename node_traits<selector_node>::current_type;
    using executor_lli =
        typename node_traits<executor_node>::current_type;

    static constexpr std::size_t selector_offset =
        group_chain_offset<
            selector_position::chain_index,
            root_type
        >() +
        chain_node_offset<
            selector_position::node_index,
            selector_chain
        >();

    static constexpr std::size_t executor_offset =
        group_chain_offset<
            executor_position::chain_index,
            root_type
        >() +
        chain_node_offset<
            executor_position::node_index,
            executor_chain
        >();

    static constexpr std::size_t patch_begin =
        selector_offset + offsetof(selector_image, CSAR);

    static constexpr std::size_t patch_end =
        executor_offset + offsetof(executor_lli, CLLR);

    static_assert(
        patch_end - patch_begin == sizeof(step_type),
        "descriptor patch range must match descriptor_step"
    );

public:
    static constexpr uint32_t selector_to_executor_cllr =
        start_cllr<ExecutorPosition>(root_type{});

    template<std::size_t StepCount, std::size_t StartIndex = 0>
        requires (StartIndex < StepCount)
    static constexpr void initialize_selector(
        root_type& image,
        root_type& execution_root,
        const step_type (&steps)[StepCount],
        start_at<StartIndex> = {}
    )
    {
        auto& selector = chain_node_at<selector_position::node_index>(
            group_chain_at<selector_position::chain_index>(image)
        ).current;

        auto& execution_selector =
            chain_node_at<selector_position::node_index>(
                group_chain_at<selector_position::chain_index>(execution_root)
            ).current;

        selector.CSAR_pointer = &steps[StartIndex];
        selector.CDAR_pointer = &execution_selector.CSAR;
    }

    static constexpr step_type make_step(
        const step_type* next,
        root_type& root,
        ExecutorImage executor
    )
    {
        auto& selector = chain_node_at<selector_position::node_index>(
            group_chain_at<selector_position::chain_index>(root)
        ).current;

        return {
            .next_selector_csar_pointer = next,
            .next_selector_cdar_pointer = &selector.CSAR,
            .next_selector_cllr = selector_to_executor_cllr,
            .executor = executor
        };
    }

    template<std::size_t StepCount, std::size_t StartIndex = 0>
        requires (StartIndex < StepCount)
    static void connect_selector(
        root_type& root,
        const step_type (&steps)[StepCount],
        start_at<StartIndex> = {}
    )
    {
        auto& selector = chain_node_at<selector_position::node_index>(
            group_chain_at<selector_position::chain_index>(root)
        ).current;

        selector.CBR1 = sizeof(step_type);
        selector.CSAR_pointer = &steps[StartIndex];
        selector.CDAR_pointer = &selector.CSAR;
        selector.CLLR = selector_to_executor_cllr;
    }
};

template<
    std::size_t ChainIndex,
    std::size_t NodeIndex,
    WiringRule... RULES,
    typename... CHAINS
>
constexpr bool cllr_target_exists(
    const lli_group<wiring_rules<RULES...>, CHAINS...>& root,
    uint32_t cllr
)
{
    using root_t = lli_group<wiring_rules<RULES...>, CHAINS...>;
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

template<WiringRule... RULES, typename... CHAINS>
constexpr bool valid_cllr_target(
    const lli_group<wiring_rules<RULES...>, CHAINS...>& root,
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
    WiringRule... RULES,
    typename... CHAINS
>
constexpr bool chain_targets_valid(
    const lli_group<wiring_rules<RULES...>, CHAINS...>& root
)
{
    using root_t = lli_group<wiring_rules<RULES...>, CHAINS...>;
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
template<typename ROOT>
    requires (
        group_traits<std::remove_cvref_t<ROOT>>::chain_count > 0
    )
constexpr bool validate_root(
    const ROOT& root
)
{
    using root_t = std::remove_cvref_t<ROOT>;

    if constexpr (
        !std::is_standard_layout_v<root_t> ||
        sizeof(root_t) > 0x10000U
    )
    {
        return false;
    }
    else if constexpr (is_finalized_lli_group_v<root_t>)
    {
        return chain_targets_valid<0, 0>(root.root);
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
    };

// 停止中のDMAチャネルへROOTの開始位置を設定する。
// 本関数はチャネルを停止・開始しない。
// CCR.EN == 1の場合、CLBAR/CLLRはロックされるため設定は反映されない。
// 64KB境界配置はリンカ側で保証する。
template<typename POSITION, GPDMAChannel CHANNEL, typename... LISTS>
    requires ValidNodePosition<POSITION, finalized_lli_group<LISTS...>>
inline void load_lli(
    CHANNEL& channel,
    const finalized_lli_group<LISTS...>& root
)
{
    const uintptr_t root_address =
        reinterpret_cast<uintptr_t>(&root.root);

    channel.CLBAR =
        static_cast<uint32_t>(root_address);
    channel.CLLR = start_cllr<POSITION>(root);
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

template<typename RULE, typename GROUP>
struct valid_wiring_rule : std::false_type
{
};

template<typename FROM, typename TO, typename GROUP>
struct valid_wiring_rule<jump<FROM, TO>, GROUP>
    : std::bool_constant<
        ValidNodePosition<FROM, std::remove_cvref_t<GROUP>> &&
        ValidNodePosition<TO, std::remove_cvref_t<GROUP>>
    >
{
};

template<typename POSITION, typename GROUP>
struct valid_wiring_rule<end_node<POSITION>, GROUP>
    : std::bool_constant<
        ValidNodePosition<POSITION, std::remove_cvref_t<GROUP>>
    >
{
};

template<typename RULES, typename GROUP>
struct valid_wiring_rules : std::false_type
{
};

template<WiringRule... RULES, typename GROUP>
struct valid_wiring_rules<wiring_rules<RULES...>, GROUP>
    : std::bool_constant<(valid_wiring_rule<RULES, GROUP>::value && ...)>
{
};

template<typename RULES, typename GROUP>
inline constexpr bool valid_wiring_rules_v =
    valid_wiring_rules<RULES, GROUP>::value;

// 宣言されたノード間ジャンプをROOT内オフセットへ解決する。
template<typename FROM, typename TO, WiringRule... RULES, typename... CHAINS>
    requires
        ValidNodePosition<FROM, lli_group<wiring_rules<RULES...>, CHAINS...>> &&
        ValidNodePosition<TO, lli_group<wiring_rules<RULES...>, CHAINS...>>
constexpr void apply_wiring_rule(
    lli_group<wiring_rules<RULES...>, CHAINS...>& root,
    jump<FROM, TO>
)
{
    using root_t = lli_group<wiring_rules<RULES...>, CHAINS...>;
    using from_t = node_position_traits<FROM, root_t>;
    using to_t = node_position_traits<TO, root_t>;

    using target_chain_t =
        typename group_chain_type<
            to_t::chain_index,
            root_t
        >::type;

    using target_node_t =
        typename chain_node_type<
            to_t::node_index,
            target_chain_t
        >::type;

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

// 宣言されたノード終端指定を適用する。
template<typename POSITION, WiringRule... RULES, typename... CHAINS>
    requires ValidNodePosition<POSITION, lli_group<wiring_rules<RULES...>, CHAINS...>>
constexpr void apply_wiring_rule(
    lli_group<wiring_rules<RULES...>, CHAINS...>& root,
    end_node<POSITION>
)
{
    using root_t = lli_group<wiring_rules<RULES...>, CHAINS...>;
    using position_t = node_position_traits<POSITION, root_t>;

    auto& node = chain_node_at<position_t::node_index>(
        group_chain_at<position_t::chain_index>(root)
    );
    node.current.CLLR = 0;
}

template<typename Group, WiringRule... RULES>
constexpr void apply_wiring_rules(
    Group& root,
    wiring_rules<RULES...>
)
{
    (apply_wiring_rule(root, RULES{}), ...);
}

// チェーンがなくなったら再帰を終了する
template<WiringRule... RULES>
constexpr void fixup_group(
    lli_group<wiring_rules<RULES...>>&,
    uint32_t
)
{
}

template<WiringRule... RULES, typename HEAD>
constexpr void connect_group_lists(
    lli_group<wiring_rules<RULES...>, HEAD>&,
    uint32_t
)
{
}

template<WiringRule... RULES, typename HEAD, typename NEXT, typename... TAIL>
constexpr void connect_group_lists(
    lli_group<wiring_rules<RULES...>, HEAD, NEXT, TAIL...>& group,
    uint32_t base_offset
)
{
    using group_t = lli_group<wiring_rules<RULES...>, HEAD, NEXT, TAIL...>;
    using tail_group_t = lli_group<wiring_rules<>, NEXT, TAIL...>;

    if constexpr (
        std::is_same_v<
            typename list_traits<HEAD>::group_tag,
            typename list_traits<NEXT>::group_tag
        > &&
        !list_traits<HEAD>::explicit_end
    )
    {
        using next_chain_t = typename list_traits<NEXT>::chain_type;
        using next_lli_t = typename node_traits<next_chain_t>::current_type;

        constexpr std::size_t tail_index =
            node_traits<typename list_traits<HEAD>::chain_type>::size - 1;
        const uint32_t next_chain_offset =
            base_offset +
            offsetof(group_t, tail) +
            offsetof(tail_group_t, head) +
            offsetof(NEXT, chain);

        chain_node_at<tail_index>(group.head.chain).current.CLLR =
            start_cllr<next_lli_t>(next_chain_offset);
    }

    connect_group_lists(
        group.tail,
        base_offset + offsetof(group_t, tail)
    );
}

template<
    WiringRule... RULES,
    typename HEAD,
    typename... TAIL
>
constexpr void fixup_group(
    lli_group<
        wiring_rules<RULES...>,
        HEAD,
        TAIL...
    >& group,
    uint32_t base_offset
)
{
    using group_t =
        lli_group<
            wiring_rules<RULES...>,
            HEAD,
            TAIL...
        >;

    // このチェーンの開始位置を、入れ子グループのオフセットに加算する。
    fixup_chain(
        group.head.chain,
        base_offset +
        offsetof(group_t, head) +
        offsetof(HEAD, chain)
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

// lli_groupをList列へ戻す。連結後はROOT全体を再構築し、CLLRを再計算する。
template<WiringRule... RULES>
constexpr auto group_lists(const lli_group<wiring_rules<RULES...>>&)
{
    return std::tuple{};
}

template<WiringRule... RULES, typename HEAD, typename... TAIL>
constexpr auto group_lists(
    const lli_group<wiring_rules<RULES...>, HEAD, TAIL...>& group
)
{
    return std::tuple_cat(
        std::tuple{group.head},
        group_lists(group.tail)
    );
}

template<
    typename InstanceTag,
    typename GroupTag,
    typename ListTag,
    typename Chain,
    bool ExplicitEnd
>
constexpr auto instantiate_list(
    const lli_list<GroupTag, ListTag, Chain, ExplicitEnd>& list
)
{
    using instanced_list_t = lli_list<
        instanced_tag<InstanceTag, GroupTag>,
        ListTag,
        Chain,
        ExplicitEnd
    >;

    return instanced_list_t{list.chain};
}

template<typename InstanceTag, typename Position>
struct instantiate_position;

template<typename InstanceTag, typename GroupTag, typename ListTag, std::size_t NodeIndex>
struct instantiate_position<InstanceTag, node_at<GroupTag, ListTag, NodeIndex>>
{
    using type = node_at<instanced_tag<InstanceTag, GroupTag>, ListTag, NodeIndex>;
};

template<typename InstanceTag, typename GroupTag, typename ListTag>
struct instantiate_position<InstanceTag, list_entry<GroupTag, ListTag>>
{
    using type = list_entry<instanced_tag<InstanceTag, GroupTag>, ListTag>;
};

template<typename InstanceTag, typename GroupTag, typename ListTag>
struct instantiate_position<InstanceTag, list_tail<GroupTag, ListTag>>
{
    using type = list_tail<instanced_tag<InstanceTag, GroupTag>, ListTag>;
};

template<typename InstanceTag, typename Position>
using instantiate_position_t =
    typename instantiate_position<InstanceTag, Position>::type;

template<typename InstanceTag, typename Rule>
struct instantiate_rule;

template<typename InstanceTag, typename From, typename To>
struct instantiate_rule<InstanceTag, jump<From, To>>
{
    using type = jump<
        instantiate_position_t<InstanceTag, From>,
        instantiate_position_t<InstanceTag, To>
    >;
};

template<typename InstanceTag, typename Position>
struct instantiate_rule<InstanceTag, end_node<Position>>
{
    using type = end_node<
        instantiate_position_t<InstanceTag, Position>
    >;
};

template<typename InstanceTag, typename Rule>
using instantiate_rule_t =
    typename instantiate_rule<InstanceTag, Rule>::type;

template<typename InstanceTag, typename Rules>
struct instantiate_rules;

template<typename InstanceTag, WiringRule... RULES>
struct instantiate_rules<InstanceTag, wiring_rules<RULES...>>
{
    using type = wiring_rules<instantiate_rule_t<InstanceTag, RULES>...>;
};

template<typename InstanceTag, typename Rules>
using instantiate_rules_t =
    typename instantiate_rules<InstanceTag, Rules>::type;

template<typename T>
struct is_lli_list : std::false_type
{
};

template<typename GroupTag, typename ListTag, typename Chain, bool ExplicitEnd>
struct is_lli_list<lli_list<GroupTag, ListTag, Chain, ExplicitEnd>>
    : std::true_type
{
};

template<typename T>
inline constexpr bool is_lli_list_v =
    is_lli_list<std::remove_cvref_t<T>>::value;

template<typename T>
struct is_mutable_lli_group : std::false_type
{
};

template<WiringRule... RULES, typename HEAD, typename... TAIL>
struct is_mutable_lli_group<lli_group<wiring_rules<RULES...>, HEAD, TAIL...>>
    : std::true_type
{
};

template<typename T>
inline constexpr bool is_mutable_lli_group_v =
    is_mutable_lli_group<std::remove_cvref_t<T>>::value;

template<typename T>
concept GroupBuildPart =
    is_lli_list_v<T> ||
    is_mutable_lli_group_v<T>;

template<typename T>
struct item_inner_rules
{
    using type = wiring_rules<>;
};

template<WiringRule... RULES, typename HEAD, typename... TAIL>
struct item_inner_rules<lli_group<wiring_rules<RULES...>, HEAD, TAIL...>>
{
    using type = wiring_rules<RULES...>;
};

template<typename T>
using item_inner_rules_t =
    typename item_inner_rules<std::remove_cvref_t<T>>::type;

template<typename T>
struct item_outer_rules
{
    using type = wiring_rules<>;
};

template<WiringRule RULE>
struct item_outer_rules<RULE>
{
    using type = wiring_rules<RULE>;
};

template<typename T>
using item_outer_rules_t =
    typename item_outer_rules<std::remove_cvref_t<T>>::type;

template<typename T>
constexpr auto collect_build_lists(const T& item)
{
    if constexpr (is_lli_list_v<T>)
    {
        return std::tuple{item};
    }
    else if constexpr (is_mutable_lli_group_v<T>)
    {
        return group_lists(item);
    }
    else
    {
        return std::tuple{};
    }
}

// チェーンをグループに格納し、各CLLRをROOT基準の位置で設定した後、配線ルールを適用する。
template<typename CombinedRules, typename... LISTS>
constexpr auto build_lli_group_from_lists(const LISTS&... lists)
{
    using root_t = lli_group<CombinedRules, LISTS...>;

    static_assert(
        unique_group_list_tags_v<LISTS...>,
        "GroupTag/ListTag pair must be unique in one ROOT"
    );
    static_assert(
        std::is_standard_layout_v<root_t>,
        "LLI group must have a standard memory layout"
    );
    static_assert(
        sizeof(root_t) <= 0x10000,
        "LLI group must fit within the 16-bit CLLR.LA offset range"
    );

    if constexpr (unique_group_list_tags_v<LISTS...>)
    {
        static_assert(
            valid_wiring_rules_v<CombinedRules, root_t>,
            "Wiring rule target position must exist in one ROOT"
        );
    }

    auto root = make_raw_lli_group<CombinedRules>(lists...);

    fixup_group(root, 0);
    connect_group_lists(root, 0);

    if constexpr (
        unique_group_list_tags_v<LISTS...> &&
        valid_wiring_rules_v<CombinedRules, root_t>
    )
    {
        apply_wiring_rules(root, CombinedRules{});
    }

    // 定数評価中に不整合があればコンパイルエラーにする。
    // 実行時生成でも不整合を検出できるよう、失敗時は停止する。
    if (!validate_root(root))
    {
        __builtin_trap();
    }

    return root;
}

// 未finalizeのGroupに新しいインスタンスタグを付け、配線ルールも変換した新しいGroupを返す。
template<typename InstanceTag, WiringRule... RULES, typename HEAD, typename... TAIL>
[[nodiscard]]
constexpr auto instantiate(
    const lli_group<wiring_rules<RULES...>, HEAD, TAIL...>& group
)
{
    using instanced_rules_t =
        instantiate_rules_t<InstanceTag, wiring_rules<RULES...>>;

    return std::apply(
        []<typename... SOURCE_LISTS>(const SOURCE_LISTS&... lists)
        {
            return build_lli_group_from_lists<instanced_rules_t>(
                instantiate_list<InstanceTag>(lists)...
            );
        },
        group_lists(group)
    );
}

// List、未finalize Group、および配線ルールを統合して一つのGroupにする。
template<typename... ITEMS>
    requires (
        ((GroupBuildPart<ITEMS> || WiringRule<ITEMS>) && ...) &&
        (GroupBuildPart<ITEMS> || ...)
    )
constexpr auto make_lli_group(const ITEMS&... items)
{
    using inner_rules_t = concat_wiring_rules_t<item_inner_rules_t<ITEMS>...>;
    using outer_rules_t = concat_wiring_rules_t<item_outer_rules_t<ITEMS>...>;
    using combined_rules_t = concat_wiring_rules_t<inner_rules_t, outer_rules_t>;

    return std::apply(
        []<typename... LISTS>(const LISTS&... lists)
        {
            return build_lli_group_from_lists<combined_rules_t>(lists...);
        },
        std::tuple_cat(collect_build_lists(items)...)
    );
}

// 妥当性確認済みのROOTへ型を変え、配線ルール型を外してdescriptor/DMA APIへ渡せる状態にする。
template<WiringRule... RULES, typename HEAD, typename... TAIL>
[[nodiscard]]
constexpr auto finalize_lli_group(
    const lli_group<wiring_rules<RULES...>, HEAD, TAIL...>& root
)
{
    using raw_group_t = lli_group<wiring_rules<>, HEAD, TAIL...>;
    using finalized_t = finalized_lli_group<HEAD, TAIL...>;

    static_assert(
        sizeof(finalized_t) == sizeof(lli_group<wiring_rules<RULES...>, HEAD, TAIL...>),
        "finalized_lli_group must match lli_group size"
    );
    static_assert(
        offsetof(finalized_t, root) == 0,
        "finalized_lli_group root member must start at offset 0"
    );

    if (!validate_root(root))
    {
        __builtin_trap();
    }

    return finalized_t{raw_group_t{root.head, root.tail}};
}

template<
    WiringRule... LEFT_RULES,
    typename LEFT_HEAD,
    typename... LEFT_TAIL,
    WiringRule... RIGHT_RULES,
    typename RIGHT_HEAD,
    typename... RIGHT_TAIL,
    WiringRule... RULES
>
constexpr auto append_lli_group(
    const lli_group<wiring_rules<LEFT_RULES...>, LEFT_HEAD, LEFT_TAIL...>& left,
    const lli_group<wiring_rules<RIGHT_RULES...>, RIGHT_HEAD, RIGHT_TAIL...>& right,
    RULES... rules
)
{
    return make_lli_group(left, right, rules...);
}
