// // rom_pager.h
// #pragma once

// #ifdef __cplusplus
// extern "C" {
// #endif

// #include <stdint.h>
// #include <stddef.h>

// // Initialise le pager.
// // romPath  : chemin du fichier ROM sur la SD (ex: "/sd/roms/game.nes")
// // xipBase  : pointeur vers la ROM mappée en XIP (premier "bank" : ex 1 Mo)
// // xipSize  : taille de la région XIP (en bytes), typiquement 1*1024*1024
// // Retourne true si OK.
// bool rom_pager_init(const char* romPath,
//                     const uint8_t* xipBase,
//                     size_t xipSize);

// // Libère les ressources (fermeture fichier, etc.)
// void rom_pager_deinit(void);

// // Lecture d’un octet à l’adresse "addr" dans la ROM complète.
// uint8_t rom_pager_read8(uint32_t addr);

// // Lecture d’un bloc (facultatif mais très utile).
// // Retourne false si on sort des limites ou erreur I/O.
// bool rom_pager_read_block(uint32_t addr, void* dst, size_t len);

// // Taille totale de la ROM
// uint32_t rom_pager_get_size(void);

// #ifdef __cplusplus
// } // extern "C"
// #endif
