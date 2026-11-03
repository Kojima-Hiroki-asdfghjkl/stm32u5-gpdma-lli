#include "gpdma_lli.hpp"

constexpr auto SPI = make_lli(
    LLI_12BSD_t{},
    LLI_1BSD_t{},
    LLI_2D_t{}
);

constexpr auto GPIO = make_lli(
    LLI_12BSD_t{},
    LLI_1BSD_t{}
);

constexpr auto ROOT = [] {
    auto root = make_lli_group(SPI, GPIO);
    patch_jump<node_at<0, 2>, node_at<1, 0>>(root);
    patch_jump<node_at<1, 1>, node_at<0, 0>>(root);
    return root;
}();

static_assert(validate_root(ROOT));
static_assert(
    start_cllr<node_at<0, 0>>(ROOT) ==
    start_cllr<LLI_12BSD_t>(lli_offset<0, decltype(ROOT)>())
);
