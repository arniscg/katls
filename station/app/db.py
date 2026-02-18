import sqlite3
from pathlib import Path
from .model import Event
import json
import logging
import shutil

logger = logging.getLogger("uvicorn.info")

DB_PATH = Path("/data/pechka-journal.db")
BACKUP_PATH = Path("/backups")


def db_init() -> None:
    if not DB_PATH.exists():
        logger.info("db doesn't exist")
        backups = sorted(BACKUP_PATH.glob("pechka-journal_*.db"), reverse=True)

        if not backups:
            logger.info("no backups found")
        else:
            logger.info(f"restoring backup {backups[0]}")

            DB_PATH.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(backups[0], DB_PATH)
    else:
        logger.info("db already exists")

    conn = sqlite3.connect(DB_PATH)
    conn.execute("PRAGMA journal_mode=WAL;")
    conn.execute("PRAGMA synchronous=NORMAL;")
    conn.execute("PRAGMA foreign_keys=ON;")

    conn.execute(
        """
        CREATE TABLE IF NOT EXISTS events (
            id TEXT PRIMARY KEY,
            time INTEGER NOT NULL,
            event TEXT NOT NULL,
            data TEXT,
            deleted BOOLEAN NOT NULL CHECK (deleted IN (0, 1))
        );
    """
    )

    conn.close()


def db_get():
    conn = sqlite3.connect(
        DB_PATH, timeout=5, isolation_level=None, check_same_thread=False
    )

    try:
        yield conn
    finally:
        conn.close()


def db_add(db: sqlite3.Connection, ev: Event):
    db.execute(
        "INSERT INTO events (id, time, event, data, deleted) VALUES (?, ?, ?, ?, ?)",
        (ev.id, ev.time, ev.event, ev.data, ev.deleted),
    )
    db.commit()


def db_mark_deleted(db: sqlite3.Connection, event_id: str):
    db.execute("UPDATE events SET deleted = TRUE WHERE id = ?", (event_id,))
    db.commit()


def db_getall(db: sqlite3.Connection):
    db.row_factory = sqlite3.Row
    cur = db.cursor()

    cur.execute("SELECT id, time, event, data, deleted FROM events")
    rows = cur.fetchall()

    data = [dict(row) for row in rows]

    return data
