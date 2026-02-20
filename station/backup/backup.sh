#!/bin/sh
set -e

DB_PATH="/data/pechka-journal.db"
BACKUP_DIR="/backups"
DATE=$(date +"%F_%s")

sqlite3 "$DB_PATH" ".backup $BACKUP_DIR/pechka-journal_$DATE.db"