import sqlite3
from pathlib import Path
from .model import Event
import json, os
import logging
import shutil

logger = logging.getLogger("uvicorn.info")


def get_db_path():
    return Path(os.getenv("DB_DIR")).joinpath("pechka-journal.db")


def db_init() -> None:
    db_dir = os.getenv("DB_DIR")
    backup_dir = os.getenv("BACKUP_DIR")

    if not db_dir or not backup_dir:
        raise Exception(".env not set")

    db_dir = Path(db_dir)
    backup_dir = Path(backup_dir)

    db_path = get_db_path()

    if not db_path.exists():
        logger.info("db doesn't exist")

        backups = None
        if backup_dir.exists():
            backups = sorted(backup_dir.glob("pechka-journal_*.db"), reverse=True)

        db_path.parent.mkdir(parents=True, exist_ok=True)

        if not backups:
            logger.info("no backups found")
        else:
            logger.info(f"restoring backup {backups[0]}")
            shutil.copy2(backups[0], db_path)
    else:
        logger.info("db already exists")

    conn = sqlite3.connect(db_path)
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
        get_db_path(), timeout=5, isolation_level=None, check_same_thread=False
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


def db_getall(db: sqlite3.Connection, count: int = 0):
    db.row_factory = sqlite3.Row
    cur = db.cursor()

    qry = "SELECT id, time, event, data, deleted FROM events ORDER BY time DESC"
    if count:
        qry += f" LIMIT {count}"

    cur.execute(qry)
    rows = cur.fetchall()

    data = [dict(row) for row in rows]

    return data
