#ifndef JOURNAL_H
#define JOURNAL_H

#include <stdint.h>
#include <stdio.h>

#define ENTRY_TEMP 1
#define ENTRY_STOP 2
#define ENTRY_BAGS 3
#define ENTRY_RESTOCK 4
#define ENTRY_CLEAN 5

typedef struct {
  uint8_t type;
  uint64_t time;
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
void journal_add(BaseEntry *);
void journal_store();
unsigned journal_entry_serialize(BaseEntry *, uint8_t *);
FILE *journal_read_init();
bool journal_next_entry(FILE *, BaseEntry **);
void journal_entry_to_str(BaseEntry *, char *, size_t);

#endif