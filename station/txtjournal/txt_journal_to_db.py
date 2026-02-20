from parser import *
from pathlib import Path
import argparse
import sqlite3
import json


# python3 txt_journal_to_db.py ./journal.txt /mnt/pechka-backups/pechka-journal_$(date +"%F_%s")_fromtxt.db

def main():
    args = parse_args()

    conn = sqlite3.connect(
        args.dst_path, timeout=5, isolation_level=None, check_same_thread=False
    )
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

    entries = parse_entries(args.journal_path)

    id = None
    time = None
    event = None
    data = None

    for i, e in enumerate(entries):
        id = f"txt_{i}"
        time = int(e.time.timestamp())
        if isinstance(e, BagsEntry):
            event = "add-bags"
            data = {"value": e.count}
        elif isinstance(e, TempEntry):
            event = "set-temp"
            data = {"value": e.temp}
        elif isinstance(e, StopEntry):
            event = "off"
            data = None
        elif isinstance(e, CleanEntry):
            event = "clean"
            data = None
        elif isinstance(e, RestockEntry):
            event = "restock"
            data = {"value": e.bag_count}

        print(f"insert {id} {time} {event} {data}")
        conn.execute(
            "INSERT INTO events (id, time, event, data, deleted) VALUES (?, ?, ?, ?, ?)",
            (id, time, event, json.dumps(data) if data else "", 0),
        )

    conn.close()


def parse_args():
    cli = argparse.ArgumentParser()
    cli.add_argument("journal_path", type=Path)
    cli.add_argument("dst_path", type=Path)
    args = cli.parse_args()

    if not args.journal_path.exists():
        print("journal file doesn't exist")
        exit(1)

    if args.dst_path.exists():
        x = input("dst exists, overwrite? (y/n): ")
        if x == "y":
            args.dst_path.unlink()
        else:
            exit(1)

    return args


if __name__ == "__main__":
    main()
