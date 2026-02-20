from parser import *
from pathlib import Path
import argparse
import sqlite3
import json


def main():
    args = parse_args()

    db = sqlite3.connect(
        args.db_path, timeout=5, isolation_level=None, check_same_thread=False
    )

    db.row_factory = sqlite3.Row
    cur = db.cursor()

    cur.execute("SELECT id, time, event, data, deleted FROM events")
    rows = cur.fetchall()

    data = [dict(row) for row in rows]

    entries = db_entries_to_txt(data)

    args.dst_path.write_text("\n".join(entries))


def parse_args():
    cli = argparse.ArgumentParser()
    cli.add_argument("db_path", type=Path)
    cli.add_argument("dst_path", type=Path)
    args = cli.parse_args()

    if not args.db_path.exists():
        print("db file doesn't exist")
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
