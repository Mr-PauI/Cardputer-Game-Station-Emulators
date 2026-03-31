// // rom_pager.cpp
// #include "rom_pager.h"
// #include <cstdio>   // pour fopen, fread, fclose, fseek
// #include <cstring>  // pour memcpy

// struct RomPagerState {
//     const uint8_t* xipBase = nullptr;
//     size_t xipSize = 0;        // taille XIP (ex: 1 Mo)
//     uint32_t romSize = 0;      // taille totale de la ROM

//     FILE* file = nullptr;      // handle vers le fichier sur SD

//     // Petit cache paginé
//     static constexpr size_t PAGE_SIZE = 4096; // à ajuster
//     uint8_t  pageBuf[PAGE_SIZE];
//     uint32_t pageBase = 0;     // adresse de début de page dans la ROM
//     size_t   pageLen  = 0;     // nb d’octets valides dans pageBuf
//     bool     pageValid = false;
// };

// static RomPagerState g_pager;

// bool rom_pager_init(const char* romPath,
//                     const uint8_t* xipBase,
//                     size_t xipSize)
// {
//     // Reset état
//     g_pager = RomPagerState{};

//     g_pager.xipBase = xipBase;
//     g_pager.xipSize = xipSize;

//     // Ouvrir le fichier sur SD
//     g_pager.file = std::fopen(romPath, "rb");
//     if (!g_pager.file) {
//         return false;
//     }

//     // Déterminer la taille totale de la ROM
//     if (std::fseek(g_pager.file, 0, SEEK_END) != 0) {
//         std::fclose(g_pager.file);
//         g_pager.file = nullptr;
//         return false;
//     }

//     long size = std::ftell(g_pager.file);
//     if (size < 0) {
//         std::fclose(g_pager.file);
//         g_pager.file = nullptr;
//         return false;
//     }
//     g_pager.romSize = static_cast<uint32_t>(size);

//     // Revenir au début (utile pour les lectures ultérieures)
//     std::fseek(g_pager.file, 0, SEEK_SET);

//     // Cache invalide au départ
//     g_pager.pageValid = false;
//     g_pager.pageBase  = 0;
//     g_pager.pageLen   = 0;

//     return true;
// }

// void rom_pager_deinit() {
//     if (g_pager.file) {
//         std::fclose(g_pager.file);
//         g_pager.file = nullptr;
//     }
//     g_pager = RomPagerState{};
// }

// uint32_t rom_pager_get_size() {
//     return g_pager.romSize;
// }

// // Charge dans le cache la page contenant addr
// static bool load_page(uint32_t addr) {
//     if (!g_pager.file) return false;
//     if (addr >= g_pager.romSize) return false;

//     // Aligner la base de page
//     uint32_t base = addr & ~(RomPagerState::PAGE_SIZE - 1);

//     if (std::fseek(g_pager.file, base, SEEK_SET) != 0) {
//         return false;
//     }

//     size_t toRead = RomPagerState::PAGE_SIZE;
//     if (base + toRead > g_pager.romSize) {
//         toRead = g_pager.romSize - base;
//     }

//     size_t n = std::fread(g_pager.pageBuf, 1, toRead, g_pager.file);
//     if (n == 0) {
//         return false;
//     }

//     g_pager.pageBase  = base;
//     g_pager.pageLen   = n;
//     g_pager.pageValid = true;
//     return true;
// }

// uint8_t rom_pager_read8(uint32_t addr) {
//     // 1) Dans la fenêtre XIP ?
//     if (addr < g_pager.xipSize && g_pager.xipBase) {
//         return g_pager.xipBase[addr];
//     }

//     // 2) Sinon, au-delà de la XIP → SD + cache
//     if (addr >= g_pager.romSize) {
//         // Hors ROM : à toi de décider → 0xFF par défaut
//         return 0xFF;
//     }

//     // L’adresse est-elle déjà dans la page en cache ?
//     if (!g_pager.pageValid ||
//         addr < g_pager.pageBase ||
//         addr >= g_pager.pageBase + g_pager.pageLen)
//     {
//         if (!load_page(addr)) {
//             return 0xFF;
//         }
//     }

//     uint32_t offset = addr - g_pager.pageBase;
//     if (offset >= g_pager.pageLen) {
//         return 0xFF;
//     }

//     return g_pager.pageBuf[offset];
// }

// bool rom_pager_read_block(uint32_t addr, void* dst, size_t len) {
//     uint8_t* out = static_cast<uint8_t*>(dst);

//     for (size_t i = 0; i < len; ++i) {
//         out[i] = rom_pager_read8(addr + i);
//     }
//     return true; // on pourrait vérifier les erreurs plus finement
// }
