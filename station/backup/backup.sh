#!/bin/sh
set -e

DB_PATH="/pechka-journal.db"
BACKUP_DIR="/backups"
DATE=$(date +%F)

sqlite3 "$DB_PATH" ".backup $BACKUP_DIR/app-$DATE.db"