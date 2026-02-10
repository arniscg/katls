#ifndef JOURNAL_H
#define JOURNAL_H

#include <freertos/ringbuf.h>
#include <stdint.h>
#include <stdio.h>

#define ENTRY_TYPE_UNKNOWN 0
#define ENTRY_TYPE_TEMP 1
#define ENTRY_TYPE_STOP 2
#define ENTRY_TYPE_BAGS 3
#define ENTRY_TYPE_RESTOCK 4
#define ENTRY_TYPE_CLEAN 5

#define JOURNAL_SIZE 16

extern SemaphoreHandle_t journal_mutex;

typedef enum {
  ENTRY_STATE_INIT,
  ENTRY_STATE_STORING,
  ENTRY_STATE_STORED,
  ENTRY_STATE_STORE_FAILED,
} EntryState;

typedef struct {
  unsigned id;
  uint8_t type;
  uint64_t time;
  // state
  EntryState state;
  uint8_t storetry;
  TimerHandle_t timer;
} BaseEntry;

typedef struct {
  BaseEntry b;
  uint8_t temp;
} TempEntry;

typedef struct {
  BaseEntry b;
} StopEntry;

typedef struct {
  BaseEntry b;
  uint8_t count;
} BagsEntry;

typedef struct {
  BaseEntry b;
  uint16_t count;
} RestockEntry;

typedef struct {
  BaseEntry b;
} CleanEntry;

void journal_init();
bool journal_add(BaseEntry *);
BaseEntry *journal_find_entry(unsigned);
unsigned journal_get(BaseEntry **, unsigned);

void entry_to_str(BaseEntry *, char *, size_t);
void entry_init(BaseEntry *);
uint8_t entry_create(BaseEntry *, uint8_t *, size_t);
void entry_store(BaseEntry *);
void entry_state_update(BaseEntry *, EntryState);

#endif