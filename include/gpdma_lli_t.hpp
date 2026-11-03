// ==========================================
// 静的サイズ用構造体 (2Dなし / 2Dあり)
// ==========================================
struct LLI_Static_t       { uint32_t CTR1, CTR2, CBR1, CSAR, CDAR, CLLR, reserved[2]; };
struct LLI_Static_2D_t    { uint32_t CTR1, CTR2, CBR1, CSAR, CDAR, CTR3, CBR2, CLLR, reserved[2]; };

// ==========================================
// GPDMA LLI 全128パターン構造体定義 (命名規則統一版)
// 物理メモリレイアウト順: CTR1 -> CTR2 -> CBR1 -> CSAR -> CDAR -> CTR3 -> CBR2 -> CLLR
// サフィックス規則: CBR2のみ=_B2, CTR3のみ=_3, 両方=_3B2
// ==========================================

// --- グループ0: 2Dレジスタなし (32パターン) ---
struct LLI__t               { uint32_t CLLR; };
struct LLI_D_t              { uint32_t CDAR, CLLR; };
struct LLI_S_t              { uint32_t CSAR, CLLR; };
struct LLI_SD_t             { uint32_t CSAR, CDAR, CLLR; };
struct LLI_B_t              { uint32_t CBR1, CLLR; };
struct LLI_BD_t             { uint32_t CBR1, CDAR, CLLR; };
struct LLI_BS_t             { uint32_t CBR1, CSAR, CLLR; };
struct LLI_BSD_t            { uint32_t CBR1, CSAR, CDAR, CLLR; };
struct LLI_2_t              { uint32_t CTR2, CLLR; };
struct LLI_2D_t             { uint32_t CTR2, CDAR, CLLR; };
struct LLI_2S_t             { uint32_t CTR2, CSAR, CLLR; };
struct LLI_2SD_t            { uint32_t CTR2, CSAR, CDAR, CLLR; };
struct LLI_2B_t             { uint32_t CTR2, CBR1, CLLR; };
struct LLI_2BD_t            { uint32_t CTR2, CBR1, CDAR, CLLR; };
struct LLI_2BS_t            { uint32_t CTR2, CBR1, CSAR, CLLR; };
struct LLI_2BSD_t           { uint32_t CTR2, CBR1, CSAR, CDAR, CLLR; };
struct LLI_1_t              { uint32_t CTR1, CLLR; };
struct LLI_1D_t             { uint32_t CTR1, CDAR, CLLR; };
struct LLI_1S_t             { uint32_t CTR1, CSAR, CLLR; };
struct LLI_1SD_t            { uint32_t CTR1, CSAR, CDAR, CLLR; };
struct LLI_1B_t             { uint32_t CTR1, CBR1, CLLR; };
struct LLI_1BD_t            { uint32_t CTR1, CBR1, CDAR, CLLR; };
struct LLI_1BS_t            { uint32_t CTR1, CBR1, CSAR, CLLR; };
struct LLI_1BSD_t           { uint32_t CTR1, CBR1, CSAR, CDAR, CLLR; };
struct LLI_12_t             { uint32_t CTR1, CTR2, CLLR; };
struct LLI_12D_t            { uint32_t CTR1, CTR2, CDAR, CLLR; };
struct LLI_12S_t            { uint32_t CTR1, CTR2, CSAR, CLLR; };
struct LLI_12SD_t           { uint32_t CTR1, CTR2, CSAR, CDAR, CLLR; };
struct LLI_12B_t            { uint32_t CTR1, CTR2, CBR1, CLLR; };
struct LLI_12BD_t           { uint32_t CTR1, CTR2, CBR1, CDAR, CLLR; };
struct LLI_12BS_t           { uint32_t CTR1, CTR2, CBR1, CSAR, CLLR; };
struct LLI_12BSD_t          { uint32_t CTR1, CTR2, CBR1, CSAR, CDAR, CLLR; };

// --- グループ1: CBR2 のみ追加 (_B2) (32パターン) ---
struct LLI__B2_t            { uint32_t CBR2, CLLR; };
struct LLI_D_B2_t           { uint32_t CDAR, CBR2, CLLR; };
struct LLI_S_B2_t           { uint32_t CSAR, CBR2, CLLR; };
struct LLI_SD_B2_t          { uint32_t CSAR, CDAR, CBR2, CLLR; };
struct LLI_B_B2_t           { uint32_t CBR1, CBR2, CLLR; };
struct LLI_BD_B2_t          { uint32_t CBR1, CDAR, CBR2, CLLR; };
struct LLI_BS_B2_t          { uint32_t CBR1, CSAR, CBR2, CLLR; };
struct LLI_BSD_B2_t         { uint32_t CBR1, CSAR, CDAR, CBR2, CLLR; };
struct LLI_2_B2_t           { uint32_t CTR2, CBR2, CLLR; };
struct LLI_2D_B2_t          { uint32_t CTR2, CDAR, CBR2, CLLR; };
struct LLI_2S_B2_t          { uint32_t CTR2, CSAR, CBR2, CLLR; };
struct LLI_2SD_B2_t         { uint32_t CTR2, CSAR, CDAR, CBR2, CLLR; };
struct LLI_2B_B2_t          { uint32_t CTR2, CBR1, CBR2, CLLR; };
struct LLI_2BD_B2_t         { uint32_t CTR2, CBR1, CDAR, CBR2, CLLR; };
struct LLI_2BS_B2_t         { uint32_t CTR2, CBR1, CSAR, CBR2, CLLR; };
struct LLI_2BSD_B2_t        { uint32_t CTR2, CBR1, CSAR, CDAR, CBR2, CLLR; };
struct LLI_1_B2_t           { uint32_t CTR1, CBR2, CLLR; };
struct LLI_1D_B2_t          { uint32_t CTR1, CDAR, CBR2, CLLR; };
struct LLI_1S_B2_t          { uint32_t CTR1, CSAR, CBR2, CLLR; };
struct LLI_1SD_B2_t         { uint32_t CTR1, CSAR, CDAR, CBR2, CLLR; };
struct LLI_1B_B2_t          { uint32_t CTR1, CBR1, CBR2, CLLR; };
struct LLI_1BD_B2_t         { uint32_t CTR1, CBR1, CDAR, CBR2, CLLR; };
struct LLI_1BS_B2_t         { uint32_t CTR1, CBR1, CSAR, CBR2, CLLR; };
struct LLI_1BSD_B2_t        { uint32_t CTR1, CBR1, CSAR, CDAR, CBR2, CLLR; };
struct LLI_12_B2_t          { uint32_t CTR1, CTR2, CBR2, CLLR; };
struct LLI_12D_B2_t         { uint32_t CTR1, CTR2, CDAR, CBR2, CLLR; };
struct LLI_12S_B2_t         { uint32_t CTR1, CTR2, CSAR, CBR2, CLLR; };
struct LLI_12SD_B2_t        { uint32_t CTR1, CTR2, CSAR, CDAR, CBR2, CLLR; };
struct LLI_12B_B2_t         { uint32_t CTR1, CTR2, CBR1, CBR2, CLLR; };
struct LLI_12BD_B2_t        { uint32_t CTR1, CTR2, CBR1, CDAR, CBR2, CLLR; };
struct LLI_12BS_B2_t        { uint32_t CTR1, CTR2, CBR1, CSAR, CBR2, CLLR; };
struct LLI_12BSD_B2_t       { uint32_t CTR1, CTR2, CBR1, CSAR, CDAR, CBR2, CLLR; };

// --- グループ2: CTR3 のみ追加 (_3) (32パターン) ---
struct LLI__3_t             { uint32_t CTR3, CLLR; };
struct LLI_D_3_t            { uint32_t CDAR, CTR3, CLLR; };
struct LLI_S_3_t            { uint32_t CSAR, CTR3, CLLR; };
struct LLI_SD_3_t           { uint32_t CSAR, CDAR, CTR3, CLLR; };
struct LLI_B_3_t            { uint32_t CBR1, CTR3, CLLR; };
struct LLI_BD_3_t           { uint32_t CBR1, CDAR, CTR3, CLLR; };
struct LLI_BS_3_t           { uint32_t CBR1, CSAR, CTR3, CLLR; };
struct LLI_BSD_3_t          { uint32_t CBR1, CSAR, CDAR, CTR3, CLLR; };
struct LLI_2_3_t            { uint32_t CTR2, CTR3, CLLR; };
struct LLI_2D_3_t           { uint32_t CTR2, CDAR, CTR3, CLLR; };
struct LLI_2S_3_t           { uint32_t CTR2, CSAR, CTR3, CLLR; };
struct LLI_2SD_3_t          { uint32_t CTR2, CSAR, CDAR, CTR3, CLLR; };
struct LLI_2B_3_t           { uint32_t CTR2, CBR1, CTR3, CLLR; };
struct LLI_2BD_3_t          { uint32_t CTR2, CBR1, CDAR, CTR3, CLLR; };
struct LLI_2BS_3_t          { uint32_t CTR2, CBR1, CSAR, CTR3, CLLR; };
struct LLI_2BSD_3_t         { uint32_t CTR2, CBR1, CSAR, CDAR, CTR3, CLLR; };
struct LLI_1_3_t            { uint32_t CTR1, CTR3, CLLR; };
struct LLI_1D_3_t           { uint32_t CTR1, CDAR, CTR3, CLLR; };
struct LLI_1S_3_t           { uint32_t CTR1, CSAR, CTR3, CLLR; };
struct LLI_1SD_3_t          { uint32_t CTR1, CSAR, CDAR, CTR3, CLLR; };
struct LLI_1B_3_t           { uint32_t CTR1, CBR1, CTR3, CLLR; };
struct LLI_1BD_3_t          { uint32_t CTR1, CBR1, CDAR, CTR3, CLLR; };
struct LLI_1BS_3_t          { uint32_t CTR1, CBR1, CSAR, CTR3, CLLR; };
struct LLI_1BSD_3_t         { uint32_t CTR1, CBR1, CSAR, CDAR, CTR3, CLLR; };
struct LLI_12_3_t           { uint32_t CTR1, CTR2, CTR3, CLLR; };
struct LLI_12D_3_t          { uint32_t CTR1, CTR2, CDAR, CTR3, CLLR; };
struct LLI_12S_3_t          { uint32_t CTR1, CTR2, CSAR, CTR3, CLLR; };
struct LLI_12SD_3_t         { uint32_t CTR1, CTR2, CSAR, CDAR, CTR3, CLLR; };
struct LLI_12B_3_t          { uint32_t CTR1, CTR2, CBR1, CTR3, CLLR; };
struct LLI_12BD_3_t         { uint32_t CTR1, CTR2, CBR1, CDAR, CTR3, CLLR; };
struct LLI_12BS_3_t         { uint32_t CTR1, CTR2, CBR1, CSAR, CTR3, CLLR; };
struct LLI_12BSD_3_t        { uint32_t CTR1, CTR2, CBR1, CSAR, CDAR, CTR3, CLLR; };

// --- グループ3: CTR3 と CBR2 の両方あり (_3B2) (32パターン) ---
struct LLI__3B2_t           { uint32_t CTR3, CBR2, CLLR; };
struct LLI_D_3B2_t          { uint32_t CDAR, CTR3, CBR2, CLLR; };
struct LLI_S_3B2_t          { uint32_t CSAR, CTR3, CBR2, CLLR; };
struct LLI_SD_3B2_t         { uint32_t CSAR, CDAR, CTR3, CBR2, CLLR; };
struct LLI_B_3B2_t          { uint32_t CBR1, CTR3, CBR2, CLLR; };
struct LLI_BD_3B2_t         { uint32_t CBR1, CDAR, CTR3, CBR2, CLLR; };
struct LLI_BS_3B2_t         { uint32_t CBR1, CSAR, CTR3, CBR2, CLLR; };
struct LLI_BSD_3B2_t        { uint32_t CBR1, CSAR, CDAR, CTR3, CBR2, CLLR; };
struct LLI_2_3B2_t          { uint32_t CTR2, CTR3, CBR2, CLLR; };
struct LLI_2D_3B2_t         { uint32_t CTR2, CDAR, CTR3, CBR2, CLLR; };
struct LLI_2S_3B2_t         { uint32_t CTR2, CSAR, CTR3, CBR2, CLLR; };
struct LLI_2SD_3B2_t        { uint32_t CTR2, CSAR, CDAR, CTR3, CBR2, CLLR; };
struct LLI_2B_3B2_t         { uint32_t CTR2, CBR1, CTR3, CBR2, CLLR; };
struct LLI_2BD_3B2_t        { uint32_t CTR2, CBR1, CDAR, CTR3, CBR2, CLLR; };
struct LLI_2BS_3B2_t        { uint32_t CTR2, CBR1, CSAR, CTR3, CBR2, CLLR; };
struct LLI_2BSD_3B2_t       { uint32_t CTR2, CBR1, CSAR, CDAR, CTR3, CBR2, CLLR; };
struct LLI_1_3B2_t          { uint32_t CTR1, CTR3, CBR2, CLLR; };
struct LLI_1D_3B2_t         { uint32_t CTR1, CDAR, CTR3, CBR2, CLLR; };
struct LLI_1S_3B2_t         { uint32_t CTR1, CSAR, CTR3, CBR2, CLLR; };
struct LLI_1SD_3B2_t        { uint32_t CTR1, CSAR, CDAR, CTR3, CBR2, CLLR; };
struct LLI_1B_3B2_t         { uint32_t CTR1, CBR1, CTR3, CBR2, CLLR; };
struct LLI_1BD_3B2_t        { uint32_t CTR1, CBR1, CDAR, CTR3, CBR2, CLLR; };
struct LLI_1BS_3B2_t        { uint32_t CTR1, CBR1, CSAR, CTR3, CBR2, CLLR; };
struct LLI_1BSD_3B2_t       { uint32_t CTR1, CBR1, CSAR, CDAR, CTR3, CBR2, CLLR; };
struct LLI_12_3B2_t         { uint32_t CTR1, CTR2, CTR3, CBR2, CLLR; };
struct LLI_12D_3B2_t        { uint32_t CTR1, CTR2, CDAR, CTR3, CBR2, CLLR; };
struct LLI_12S_3B2_t        { uint32_t CTR1, CTR2, CSAR, CTR3, CBR2, CLLR; };
struct LLI_12SD_3B2_t       { uint32_t CTR1, CTR2, CSAR, CDAR, CTR3, CBR2, CLLR; };
struct LLI_12B_3B2_t        { uint32_t CTR1, CTR2, CBR1, CTR3, CBR2, CLLR; };
struct LLI_12BD_3B2_t       { uint32_t CTR1, CTR2, CBR1, CDAR, CTR3, CBR2, CLLR; };
struct LLI_12BS_3B2_t       { uint32_t CTR1, CTR2, CBR1, CSAR, CTR3, CBR2, CLLR; };
struct LLI_12BSD_3B2_t      { uint32_t CTR1, CTR2, CBR1, CSAR, CDAR, CTR3, CBR2, CLLR; };