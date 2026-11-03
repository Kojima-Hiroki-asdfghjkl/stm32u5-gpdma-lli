add_library(gpdma_lli INTERFACE)

target_include_directories(gpdma_lli
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(gpdma_lli INTERFACE cxx_std_20)
