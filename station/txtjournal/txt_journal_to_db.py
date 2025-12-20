from parser import *
from pathlib import Path
import argparse
import sqlite3
import json


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
            data TEXT NOT NULL
        );
    """
    )

    entries = parse_entries(args.journal_path)

    id = None
    time = None
    event = None
    data = None

    for i, e in enumerate(entries):
        id = f"backup_{i}"
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
            "INSERT INTO events (id, time, event, data) VALUES (?, ?, ?, ?)",
            (id, time, event, json.dumps(data)),
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
