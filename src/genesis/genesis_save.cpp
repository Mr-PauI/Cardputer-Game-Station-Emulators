#include "genesis_save.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "share/game_save.h"

extern "C" {
  #include "genesis/gwenesis/bus/gwenesis_bus.h"
}

/* ============================ Config ============================ */

#define GENESIS_SAVE_DIR "/sd/genesis_saves"
#define SAVE_CHECK_MS 2000
#define SAVE_GAP_MS   15000

/* ============================= Etat ============================= */

static char*        g_save_path   = nullptr;
static TaskHandle_t g_task        = nullptr;
static TickType_t   g_next_check  = 0;
static TickType_t   g_next_allow  = 0;
static TickType_t   g_first_dirty = 0;
static TickType_t   g_last_save   = 0;

static volatile bool g_flag_check = false;
static volatile bool g_flag_flush = false;
static volatile bool g_sram_dirty = false;

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
    strcpy(name, "genesis_autosave.srm");
  }

  int n = snprintf(g_save_path, PATH_MAX, GENESIS_SAVE_DIR "/%s", name);
  if (n < 0 || (size_t)n >= PATH_MAX) {
    g_save_path[PATH_MAX - 1] = '\0';
  }
}

static size_t get_sram_size() {
  if (!SRAM_ENABLED || !SRAM) return 0;
  if (SRAM_END < SRAM_START) return 0;

  size_t size = (size_t)(SRAM_END - SRAM_START + 1);
  if (size > 0x2000) size = 0x2000;
  return size;
}

/* ============================ Save ============================= */

static bool save_now() {
  if (!g_save_path) return false;
  if (!SRAM_ENABLED || !SRAM) return false;

  size_t sram_size = get_sram_size();
  if (sram_size == 0) {
    printf("[GEN][SAVE] SRAM disabled or size=0, skip save\n");
    return false;
  }

  if (!share::gameSaveEnsureParentReady(GENESIS_SAVE_DIR)) {
    printf("[GEN][SAVE] storage path not ready, skip save\n");
    return false;
  }

  share::setGameIsSaving(true);

  FILE* f = fopen(g_save_path, "wb");
  if (!f) {
    share::setGameIsSaving(false);
    printf("[GEN][SAVE] open failed for %s\n", g_save_path);
    return false;
  }

  size_t n = fwrite(SRAM, 1, sram_size, f);
  fclose(f);

  share::setGameIsSaving(false);

  if (n != sram_size) {
    printf("[GEN][SAVE] fwrite failed: wrote %u / %u bytes\n",
           (unsigned)n, (unsigned)sram_size);
    return false;
  }

  g_last_save   = xTaskGetTickCount();
  g_first_dirty = 0;
  g_sram_dirty  = false;

  printf("[GEN][SAVE] SRAM saved to %s (%u bytes)\n",
         g_save_path, (unsigned)sram_size);
  return true;
}

/* ============================= Task ============================ */

static void SaveTask(void* /*arg*/) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    bool do_check = g_flag_check; g_flag_check = false;
    bool do_flush = g_flag_flush; g_flag_flush = false;
    TickType_t now = xTaskGetTickCount();

    if (do_check) {
      if (g_sram_dirty) {
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
        printf("[GEN][SAVE] save failed, will retry on next tick\n");
      }
    }
  }
}

/* ============================== API ============================ */

extern "C" void genesis_save_init(const char* romPathOrName) {
  if (!g_save_path) {
    g_save_path = (char*)malloc(PATH_MAX);
    if (!g_save_path) {
      printf("[GEN][SAVE] OOM on path alloc, autosave disabled\n");
      return;
    }
  }

  g_save_path[0] = '\0';
  make_save_path(romPathOrName);

  g_next_check  = 0;
  g_next_allow  = 0;
  g_first_dirty = 0;
  g_last_save   = 0;
  g_flag_check  = false;
  g_flag_flush  = false;
  g_sram_dirty  = false;

  if (!g_task) {
    xTaskCreatePinnedToCore(
      SaveTask,
      "GEN_SaveTask",
      3072,
      nullptr,
      6,
      &g_task,
      0
    );
  }

  printf("[GEN][SAVE] path=%s\n", g_save_path);
}

extern "C" void genesis_save_load(void) {
  if (!g_save_path) return;
  if (!SRAM_ENABLED || !SRAM) {
    printf("[GEN][SAVE] skip load (SRAM disabled)\n");
    return;
  }

  if (!share::gameSaveEnsureParentReady(GENESIS_SAVE_DIR)) {
    printf("[GEN][SAVE] skip load (storage not ready)\n");
    return;
  }

  struct stat st;
  if (stat(g_save_path, &st) != 0) {
    printf("[GEN][SAVE] no existing save file for %s\n", g_save_path);
    return;
  }

  size_t sram_size = get_sram_size();
  if (sram_size == 0) {
    printf("[GEN][SAVE] skip load (SRAM size=0)\n");
    return;
  }

  FILE* f = fopen(g_save_path, "rb");
  if (!f) {
    printf("[GEN][SAVE] load open failed for %s\n", g_save_path);
    return;
  }

  size_t n = fread(SRAM, 1, sram_size, f);
  fclose(f);

  printf("[GEN][SAVE] existing save size: %ld bytes\n", (long)st.st_size);
  printf("[GEN][SAVE] SRAM loaded from %s (%u bytes)\n",
         g_save_path, (unsigned)n);

  g_sram_dirty = false;

  FILE* dbg = fopen(g_save_path, "rb");
  if (dbg) {
    uint8_t buf[16];
    size_t got = fread(buf, 1, sizeof(buf), dbg);
    fclose(dbg);
    printf("[GEN][SAVE] first bytes: ");
    for (size_t i = 0; i < got; ++i) printf("%02X ", buf[i]);
    printf("\n");
  }
}

extern "C" void genesis_save_tick(void) {
  if (!g_save_path) return;

  TickType_t now = xTaskGetTickCount();
  if (now < g_next_check) return;

  g_next_check = now + pdMS_TO_TICKS(SAVE_CHECK_MS);
  g_flag_check = true;

  if (g_task) xTaskNotifyGive(g_task);
}

extern "C" void genesis_save_request_flush(void) {
  g_flag_flush = true;
  if (g_task) xTaskNotifyGive(g_task);
}

extern "C" void genesis_save_force_flush(void) {
  save_now();
}

extern "C" void genesis_save_mark_dirty_c(void) {
  g_sram_dirty = true;
}