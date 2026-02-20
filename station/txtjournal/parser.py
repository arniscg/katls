from datetime import datetime
from dataclasses import dataclass
from pathlib import Path
import struct, json


@dataclass
class Entry:
    time: datetime


@dataclass
class BagsEntry(Entry):
    count: int


@dataclass
class TempEntry(Entry):
    temp: int


@dataclass
class StopEntry(Entry):
    pass


@dataclass
class CleanEntry(Entry):
    pass


@dataclass
class RestockEntry(Entry):
    bag_count: int
    reset: bool


def type_to_num(e: Entry):
    if isinstance(e, TempEntry):
        return 1
    elif isinstance(e, StopEntry):
        return 2
    elif isinstance(e, BagsEntry):
        return 3
    elif isinstance(e, RestockEntry):
        return 4
    elif isinstance(e, CleanEntry):
        return 5


# If multiple entries entered on same time, read them in predefined order
# Having BagEntry before TempEntry (ON) makes the processing easier later on
entry_order = [StopEntry, RestockEntry, BagsEntry, CleanEntry, TempEntry]


def parse_entries(journal_path: Path) -> list[Entry]:
    if not journal_path.exists():
        raise Exception("journal file doesn't exist")

    def failed(err: str, line_num: int, line: str):
        raise Exception(f"{err} ({line_num}: '{line.strip()}')")

    entries: list[Entry] = []

    with journal_path.open() as f:
        year = None

        prev_time = None
        for i, line in enumerate(f.readlines()):
            line = line.strip()
            if line.startswith("#"):
                continue
            if not len(line):
                continue

            parts = line.split(" ")

            if len(parts) == 1:
                try:
                    year = int(parts[0])
                except:
                    failed("invalid year", i, line)
                if year < 2024 or year > 2100:
                    failed(f"invalid year", i, line)
                continue

            if year is None:
                failed(f"year unknown", i, line)

            if len(parts) < 2:
                failed("invalid", i, line)

            time = None
            try:
                time = datetime.strptime(f"{parts[0]};{year}", "%d.%mT%H:%M;%Y")
            except ValueError:
                try:
                    time = datetime.strptime(f"{parts[0]};{year}", "%d.%m;%Y")
                except:
                    failed("invalid date", i, line)

            if prev_time is not None and prev_time > time:
                failed("decreasing time", i, line)
            prev_time = time

            en: list[Entry] = []
            for part in parts[1:]:
                part = part.strip()
                if not len(part):
                    continue

                if part[0] == "+":
                    if len(part) < 2 or len(part) > 3:
                        failed("invalid bags entry", i, line)
                    en.append(BagsEntry(time, int(part[1:])))
                elif "C" in part:
                    if len(part) != 3 or part[2] != "C":
                        failed("invalid temperature entry", i, line)
                    en.append(TempEntry(time, int(part[0:2])))
                elif part == "X":
                    en.append(StopEntry(time))
                elif part == "P":
                    en.append(CleanEntry(time))
                elif part[0] == "G":
                    en.append(RestockEntry(time, int(part.replace("G", "")), False))
                elif part[0] == "R":
                    en.append(RestockEntry(time, int(part.replace("R", "")), True))
                else:
                    failed("unknown entry", i, line)

            entries += sorted(en, key=lambda e: entry_order.index(type(e)))

    return entries


def serialize(entry: Entry) -> bytearray:
    t = type_to_num(entry)
    print(t)
    a = bytearray()
    a.append(t)
    a.extend(struct.pack("<Q", int(entry.time.timestamp())))

    if isinstance(entry, TempEntry):
        a.append(entry.temp)
    elif isinstance(entry, StopEntry):
        pass
    elif isinstance(entry, BagsEntry):
        a.append(entry.count)
    elif isinstance(entry, RestockEntry):
        a.extend(struct.pack("<H", entry.bag_count))
    elif isinstance(entry, CleanEntry):
        pass

    a.append(t)  # for reverse reading

    return a


def db_entries_to_txt(entries: list[dict]) -> list[str]:
    res: list[str] = []
    last_time: int | None = None
    last_year: int | None = None
    for ent in entries:
        t = datetime.fromtimestamp(ent["time"])

        if t.year != last_year:
            res.append(str(t.year))
        last_year = t.year

        s = entry_str(ent["event"], ent["data"])

        if last_time == t:
            assert len(res)
            res[-1] += f" {s}"
        else:
            res.append(f"{t.strftime('%d.%mT%H:%M')} {s}")
        last_time = t

    return res


def entry_str(event: str, data: str) -> str:
    d = json.loads(data) if data else {}
    if event == "set-temp":
        return f"{d['value']}C"
    elif event == "clean":
        return "P"
    elif event == "off":
        return "X"
    elif event == "restock":
        return f"G{d['value']}"
    elif event == "add-bags":
        return f"+{d['value']}"
