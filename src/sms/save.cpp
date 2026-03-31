#pragma GCC optimize ("Os")

#include "save.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <unistd.h>
#include "share/game_save.h"  

static uint8_t* g_sram = NULL;
static size_t   g_sram_len = 0;
static char*    g_save_path = nullptr;
static uint32_t g_crc_last = 0;
static TickType_t g_next_check = 0;
static TickType_t g_next_allowed_write = 0;
static TaskHandle_t g_save_task = nullptr;
static uint8_t* g_sram_shadow = nullptr;
static volatile bool g_flush_req = false;     // ask for flush
static volatile bool g_check_req = false;     // ask for check (CRC)

static void ensure_dir(void){ 
  mkdir("/sd/sms_saves", 0777); 
}

static bool flush_now(void){
  if (!g_sram || !g_sram_len) return false;

  if (share::gameSaveIsTrivialSram(g_sram, g_sram_len)) {
    printf("SMS save: skip trivial SRAM, no write.\n");
    return false; 
  }

  const uint8_t* src = g_sram;
  if (g_sram_shadow) {
    memcpy(g_sram_shadow, g_sram, g_sram_len);
    src = g_sram_shadow;
  }

  share::setGameIsSaving(true);

  ensure_dir();

  char tmp_path[PATH_MAX];
  snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", g_save_path);
  tmp_path[sizeof(tmp_path)-1] = '\0';

  // Write to temp file
  FILE* f = fopen(tmp_path, "wb");
  if (!f) {
    printf("SMS save: fopen tmp fail %s\n", tmp_path);
    return false;
  }
  setvbuf(f, NULL, _IONBF, 0);

  size_t w = fwrite(src, 1, g_sram_len, f);
  fflush(f);
  fsync(fileno(f));
  fclose(f);

  if (w != g_sram_len) {
    printf("SMS save: short write %u/%u to %s\n",
           (unsigned)w, (unsigned)g_sram_len, tmp_path);
    return false;
  }

  // Unlink + rename
  unlink(g_save_path);
  if (rename(tmp_path, g_save_path) != 0) {
    printf("SMS save: rename failed %s -> %s\n", tmp_path, g_save_path);
    return false;
  }

  printf("SMS save: wrote %u/%u -> %s \n",
         (unsigned)w, (unsigned)g_sram_len, g_save_path);
         
  share::setGameIsSaving(false);
  return true;
}

static void SaveTask(void*){
  for(;;){
    // sleep until notified
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(CHECK_MS));

    bool do_check = g_check_req; g_check_req = false;
    bool do_flush = g_flush_req; g_flush_req = false;

    TickType_t now = xTaskGetTickCount();

    if (do_check) {
      uint32_t crc = share::gameSaveCrc32Update(0, g_sram, g_sram_len);
      if (crc != g_crc_last && now >= g_next_allowed_write){
        g_crc_last = crc;

        if (!share::gameSaveIsTrivialSram(g_sram, g_sram_len)) {
          do_flush = true;
        } else {
          printf("SMS save: trivial after change, skip write.\n");
        }
      }
    }

    // flush if requested and allowed
    if (do_flush && now >= g_next_allowed_write) {
      bool ok = flush_now();
      if (ok) {
        // save OK
        g_next_allowed_write = xTaskGetTickCount() + pdMS_TO_TICKS(GAP_MS);
      } else {
        // failed, dont delay next attempt
        g_crc_last = 0xFFFFFFFFu;
        printf("SMS save: flush failed, will retry on next tick.\n");
      }
    }
  }
}

void sms_save_init(const char* romName, uint8_t* sramPtr, size_t sramLen){
  if (!g_save_path) {
    g_save_path = (char*)malloc(PATH_MAX);
    if (!g_save_path) abort();
  }
  g_sram = sramPtr;
  g_sram_len = sramLen;
  share::gameSaveBuildPath(g_save_path, PATH_MAX, "/sd/sms_saves", romName, "rom.sms");
  g_crc_last = (g_sram && g_sram_len) ? share::gameSaveCrc32Update(0, g_sram, g_sram_len) : 0;
  g_next_check = g_next_allowed_write = 0;

  // snapshot SRAM
  if (!g_sram_shadow && g_sram_len) {
    g_sram_shadow = (uint8_t*)malloc(g_sram_len);
    if (!g_sram_shadow) {
      printf("SMS save: no shadow buffer, will write live.\n");
    }
  }

  // launch save task
  if (!g_save_task) {
    xTaskCreatePinnedToCore(SaveTask, "SaveTask", 4096, nullptr, 2, &g_save_task, 0);
  }
}

void sms_save_load(void){
  if (!g_sram || !g_sram_len) return;
  memset(g_sram, 0xFF, g_sram_len);

  ensure_dir(); 

  char tmp_path[PATH_MAX];
  snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", g_save_path);
  tmp_path[sizeof(tmp_path)-1] = '\0';

  const char* loaded_path = g_save_path;

  // Try to load .sav
  FILE* f = fopen(g_save_path, "rb");
  if (!f) {
    // Try to load .tmp
    f = fopen(tmp_path, "rb");
    if (!f) {
      printf("SMS load: no save, %s nor %s\n", g_save_path, tmp_path);
      return;
    }

    printf("SMS load: .sav missing, using tmp %s\n", tmp_path);
    loaded_path = tmp_path;
  }

  size_t n = fread(g_sram, 1, g_sram_len, f);
  fclose(f);

  if (n < g_sram_len)
    memset(g_sram + n, 0xFF, g_sram_len - n);

  g_crc_last = share::gameSaveCrc32Update(0, g_sram, g_sram_len);

  printf("SMS load: read %u/%u from %s\n",
         (unsigned)n, (unsigned)g_sram_len, loaded_path);
}

void sms_save_tick(void){
  if (!g_sram || !g_sram_len) return;
  TickType_t now = xTaskGetTickCount();
  if (now < g_next_check) return;
  g_next_check = now + pdMS_TO_TICKS(CHECK_MS);

  g_check_req = true;
  if (g_save_task) xTaskNotifyGive(g_save_task);
}

void sms_save_force_flush(void){
  flush_now();
}