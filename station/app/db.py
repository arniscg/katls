import sqlite3
from pathlib import Path
from .model import Event
import json

DB_PATH = Path("/pechka-journal.db")


def db_init() -> None:
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
            data TEXT NOT NULL
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
        "INSERT INTO events (id, time, event, data) VALUES (?, ?, ?, ?)",
        (ev.id, ev.time, ev.event, json.dumps(ev.data)),
    )
