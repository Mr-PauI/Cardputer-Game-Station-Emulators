#include "snes_save.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "share/game_save.h"

extern "C" {
  #include "snes9x/snes9x.h"
}

/* ============================ Config ============================ */

#define SNES_SAVE_DIR "/sd/snes_saves"
#define SAVE_CHECK_MS 2000
#define SAVE_GAP_MS   15000
#define SNES_SRAM_MAX_BYTES 2048

/* ============================= Etat ============================= */

static char*      g_save_path   = nullptr;
static TickType_t g_next_check  = 0;
static TickType_t g_next_allow  = 0;
static TickType_t g_first_dirty = 0;
static TickType_t g_last_save   = 0;

#ifndef SNES_NO_THREADED_SAVE
static TaskHandle_t   g_task        = nullptr;
static volatile bool  g_flag_check  = false;
static volatile bool  g_flag_flush  = false;
#endif

/* ========================== Utils FS =========================== */

static void make_save_path(const char* romPathOrName) {
  const char* base = share::gameSaveBasename(romPathOrName);
  char name[160] = {0};

  if (base && *base) {
    strncpy(name, base, sizeof(name) - 1);

    char* dot = strrchr(name, '.');
    if (dot) {
      *dot = '\0';
    }

    strncat(name, ".srm", sizeof(name) - strlen(name) - 1);
  } else {
    strcpy(name, "snes_autosave.srm");
  }

  int n = snprintf(g_save_path, PATH_MAX, SNES_SAVE_DIR "/%s", name);
  if (n < 0 || (size_t)n >= PATH_MAX) {
    g_save_path[PATH_MAX - 1] = '\0';
  }
}

static size_t get_sram_size(void) {
  if (!Memory.SRAM) return 0;
  if (Memory.SRAMMask == 0) return 0;

  size_t size = (size_t)Memory.SRAMMask + 1;
  if (size > SNES_SRAM_MAX_BYTES) return 0;
  return size;
}

/* ======================= SRAM setup ===================== */

extern "C" void snes_save_prepare_sram(void) {
  uint32_t sram_bytes = 0;

  if (Memory.SRAMSize > 0) {
    sram_bytes = ((uint32_t)1 << (Memory.SRAMSize + 3)) * 128;
  }

  if (Memory.SRAM != NULL) {
    free(Memory.SRAM);
    Memory.SRAM = NULL;
  }

  if (sram_bytes == 0 || sram_bytes > SNES_SRAM_MAX_BYTES) {
    printf("[SNES][SRAM] disabled: requested %u bytes\n", (unsigned)sram_bytes);
    Memory.SRAMSize = 0;
    Memory.SRAMMask = 0;
    return;
  }

  Memory.SRAM = (uint8_t*)calloc(1, sram_bytes);
  if (Memory.SRAM == NULL) {
    printf("[SNES][SRAM] alloc failed for %u bytes\n", (unsigned)sram_bytes);
    Memory.SRAMSize = 0;
    Memory.SRAMMask = 0;
    return;
  }

  Memory.SRAMMask = sram_bytes - 1;
  CPU.SRAMModified = false;

  printf("[SNES][SRAM] allocated: %u bytes, mask=0x%X\n",
         (unsigned)sram_bytes,
         (unsigned)Memory.SRAMMask);
}

/* ============================ Save ============================= */

static bool save_now(void) {
  if (!g_save_path) return false;
  if (!Memory.SRAM) return false;

  size_t sram_size = get_sram_size();
  if (sram_size == 0) {
    printf("[SNES][SAVE] SRAM disabled or size=0, skip save\n");
    return false;
  }

  if (!share::gameSaveEnsureParentReady(SNES_SAVE_DIR)) {
    printf("[SNES][SAVE] storage path not ready, skip save\n");
    return false;
  }

  share::setGameIsSaving(true);

  FILE* f = fopen(g_save_path, "wb");
  if (!f) {
    share::setGameIsSaving(false);
    printf("[SNES][SAVE] open failed for %s\n", g_save_path);
    return false;
  }

  size_t n = fwrite(Memory.SRAM, 1, sram_size, f);
  fclose(f);

  share::setGameIsSaving(false);

  if (n != sram_size) {
    printf("[SNES][SAVE] fwrite failed: wrote %u / %u bytes\n",
           (unsigned)n, (unsigned)sram_size);
    return false;
  }

  g_last_save   = xTaskGetTickCount();
  g_first_dirty = 0;
  CPU.SRAMModified = false;

  printf("[SNES][SAVE] SRAM saved to %s (%u bytes)\n",
         g_save_path, (unsigned)sram_size);
  return true;
}

#ifdef SNES_NO_THREADED_SAVE

static void process_save_logic(bool force_flush) {
  if (!g_save_path) return;
  if (!Memory.SRAM) return;

  TickType_t now = xTaskGetTickCount();

  if (force_flush) {
    if (now >= g_next_allow) {
      bool ok = save_now();
      if (ok) {
        g_next_allow = xTaskGetTickCount() + pdMS_TO_TICKS(SAVE_GAP_MS);
      } else {
        printf("[SNES][SAVE] save failed, will retry later\n");
      }
    }
    return;
  }

  if (now < g_next_check) return;
  g_next_check = now + pdMS_TO_TICKS(SAVE_CHECK_MS);

  if (CPU.SRAMModified) {
    if (g_first_dirty == 0) g_first_dirty = now;

    if (now >= g_next_allow) {
      bool ok = save_now();
      if (ok) {
        g_next_allow = xTaskGetTickCount() + pdMS_TO_TICKS(SAVE_GAP_MS);
      } else {
        printf("[SNES][SAVE] save failed, will retry later\n");
      }
    }
  }
}

#else

/* ============================= Task ============================ */

static void SaveTask(void* /*arg*/) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    bool do_check = g_flag_check; g_flag_check = false;
    bool do_flush = g_flag_flush; g_flag_flush = false;
    TickType_t now = xTaskGetTickCount();

    if (do_check) {
      if (CPU.SRAMModified) {
        if (g_first_dirty == 0) g_first_dirty = now;
        if (now >= g_next_allow) {
          do_flush = true;
        }
      }
    }

    if (do_flush && now >= g_next_allow) {
      bool ok = save_now();
      if (ok) {
        g_next_allow = xTaskGetTickCount() + pdMS_TO_TICKS(SAVE_GAP_MS);
      } else {
        printf("[SNES][SAVE] save failed, will retry on next tick\n");
      }
    }
  }
}

#endif

/* ============================== API ============================ */

extern "C" void snes_save_init(const char* romPathOrName) {
  if (!Memory.SRAM || Memory.SRAMMask == 0) {
    printf("[SNES][SAVE] not started (no SRAM)\n");
    return;
  }

  if (!g_save_path) {
    g_save_path = (char*)malloc(PATH_MAX);
    if (!g_save_path) {
      printf("[SNES][SAVE] OOM on path alloc, autosave disabled\n");
      return;
    }
  }

  g_save_path[0] = '\0';
  make_save_path(romPathOrName);

  g_next_check  = 0;
  g_next_allow  = 0;
  g_first_dirty = 0;
  g_last_save   = 0;

#ifndef SNES_NO_THREADED_SAVE
  g_flag_check  = false;
  g_flag_flush  = false;

  if (!g_task) {
    xTaskCreatePinnedToCore(
      SaveTask,
      "SNES_SaveTask",
      2048,
      nullptr,
      6,
      &g_task,
      0
    );
  }
#endif

#ifdef SNES_NO_THREADED_SAVE
  printf("[SNES][SAVE] path=%s (non-threaded)\n", g_save_path);
#else
  printf("[SNES][SAVE] path=%s (threaded)\n", g_save_path);
#endif
}

extern "C" void snes_save_load(void) {
  if (!g_save_path) return;
  if (!Memory.SRAM) {
    printf("[SNES][SAVE] skip load (SRAM disabled)\n");
    return;
  }

  if (!share::gameSaveEnsureParentReady(SNES_SAVE_DIR)) {
    printf("[SNES][SAVE] skip load (storage not ready)\n");
    return;
  }

  struct stat st;
  if (stat(g_save_path, &st) != 0) {
    printf("[SNES][SAVE] no existing save file for %s\n", g_save_path);
    return;
  }

  size_t sram_size = get_sram_size();
  if (sram_size == 0) {
    printf("[SNES][SAVE] skip load (SRAM size=0)\n");
    return;
  }

  FILE* f = fopen(g_save_path, "rb");
  if (!f) {
    printf("[SNES][SAVE] load open failed for %s\n", g_save_path);
    return;
  }

  size_t n = fread(Memory.SRAM, 1, sram_size, f);
  fclose(f);

  printf("[SNES][SAVE] existing save size: %ld bytes\n", (long)st.st_size);
  printf("[SNES][SAVE] SRAM loaded from %s (%u bytes)\n",
         g_save_path, (unsigned)n);

  CPU.SRAMModified = false;

  FILE* dbg = fopen(g_save_path, "rb");
  if (dbg) {
    uint8_t buf[16];
    size_t got = fread(buf, 1, sizeof(buf), dbg);
    fclose(dbg);
    printf("[SNES][SAVE] first bytes: ");
    for (size_t i = 0; i < got; ++i) printf("%02X ", buf[i]);
    printf("\n");
  }
}

extern "C" void snes_save_tick(void) {
#ifdef SNES_NO_THREADED_SAVE
  process_save_logic(false);
#else
  if (!g_save_path) return;

  TickType_t now = xTaskGetTickCount();
  if (now < g_next_check) return;

  g_next_check = now + pdMS_TO_TICKS(SAVE_CHECK_MS);
  g_flag_check = true;

  if (g_task) xTaskNotifyGive(g_task);
#endif
}

extern "C" void snes_save_request_flush(void) {
#ifdef SNES_NO_THREADED_SAVE
  process_save_logic(true);
#else
  g_flag_flush = true;
  if (g_task) xTaskNotifyGive(g_task);
#endif
}

extern "C" void snes_save_force_flush(void) {
  save_now();
}