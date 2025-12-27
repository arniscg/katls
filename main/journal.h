#ifndef JOURNAL_H
#define JOURNAL_H

#include <stdint.h>
#include <stdio.h>

#define ENTRY_TYPE_UNKNOWN 0
#define ENTRY_TYPE_TEMP 1
#define ENTRY_TYPE_STOP 2
#define ENTRY_TYPE_BAGS 3
#define ENTRY_TYPE_RESTOCK 4
#define ENTRY_TYPE_CLEAN 5

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

// void journal_init();
// void journal_add(BaseEntry *);
// void journal_store();
// unsigned journal_entry_serialize(BaseEntry *, uint8_t *);
// FILE *journal_read_init();
// bool journal_next_entry(FILE *, BaseEntry **);

void journal_init(unsigned);
bool journal_add(BaseEntry *);

void entry_to_str(BaseEntry *, char *, size_t);
uint8_t entry_create(BaseEntry *, uint8_t *, size_t);
void entry_data_str(BaseEntry *,  char *, size_t);
void entry_store(BaseEntry *);

#endif