"""
Pellet/bag consumption model.

Vocabulary:
  cycle  - one continuous burn: starts at a set-temp (only if the stove was
           off before), ends at the next off. Repeated set-temp events with
           no off between them are just temperature changes, not new cycles.
  batch  - the fuel window between two consecutive add-bags events. A batch
           can span multiple cycles (top up once, run several on/off cycles
           before the next top-up).

Consumption rate for a closed batch = bags_added / on-seconds within the
batch window (only "on" time counts - idle time between cycles doesn't burn
fuel). The most recent batch has no closing add-bags event yet, so its true
rate is unknown; we use a provisional rate (weighted average of the last few
closed batches) for it and flag every number derived from it as provisional.

A cycle longer than ANOMALY_CYCLE_SECONDS almost certainly means a missing
`off` event rather than a real multi-week burn; such cycles are excluded
from rate calculations and surfaced as data-quality flags instead.
"""

import json
import statistics
from dataclasses import dataclass, field
from datetime import datetime, timedelta

ANOMALY_CYCLE_SECONDS = 10 * 86400
SEASON_GAP_SECONDS = 45 * 86400
PROVISIONAL_BATCH_LOOKBACK = 3
CLEANING_OVERDUE_FACTOR = 1.5


def _value(data: str) -> int:
    return json.loads(data)["value"]


@dataclass
class Cycle:
    start: float
    end: float
    anomalous: bool = False

    @property
    def duration(self) -> float:
        return self.end - self.start


@dataclass
class Batch:
    start: float
    end: float | None  # None = still open (no closing add-bags event yet)
    qty: int
    rate: float | None = None  # bags/second, None if unknown
    denominator_seconds: float = 0.0
    provisional: bool = False


@dataclass
class Segment:
    start: float
    end: float
    is_on: bool
    batch: Batch | None
    cycle: Cycle | None
    amount: float | None  # bags consumed in this segment, None if unknown

    @property
    def duration(self) -> float:
        return self.end - self.start


def build_cycles(events: list[dict], now: float) -> tuple[list[Cycle], Cycle | None]:
    """Returns (closed cycles, open cycle or None if currently idle)."""
    cycles: list[Cycle] = []
    state = "off"
    start = None
    for e in events:
        if e["event"] == "set-temp":
            if state == "off":
                start = e["time"]
                state = "on"
        elif e["event"] == "off":
            if state == "on":
                c = Cycle(start, e["time"])
                c.anomalous = c.duration > ANOMALY_CYCLE_SECONDS
                cycles.append(c)
                state = "off"

    if state == "on":
        c = Cycle(start, now)
        c.anomalous = c.duration > ANOMALY_CYCLE_SECONDS
        return cycles, c
    return cycles, None


def build_batches(events: list[dict]) -> list[Batch]:
    adds = [e for e in events if e["event"] == "add-bags"]
    batches: list[Batch] = []
    for i, e in enumerate(adds):
        end = adds[i + 1]["time"] if i + 1 < len(adds) else None
        batches.append(Batch(start=e["time"], end=end, qty=_value(e["data"])))
    return batches


def _on_seconds_in_window(
    cycles: list[Cycle], win_start: float, win_end: float, exclude_anomalous: bool = True
) -> float:
    total = 0.0
    for c in cycles:
        if exclude_anomalous and c.anomalous:
            continue
        lo = max(c.start, win_start)
        hi = min(c.end, win_end)
        if hi > lo:
            total += hi - lo
    return total


def compute_batch_rates(
    batches: list[Batch], all_cycles: list[Cycle], now: float
) -> None:
    """Fills in .rate, .denominator_seconds, .provisional on each batch, in place."""
    closed_with_rate: list[Batch] = []
    for b in batches:
        window_end = b.end if b.end is not None else now
        b.denominator_seconds = _on_seconds_in_window(all_cycles, b.start, window_end)
        if b.end is not None:
            b.rate = b.qty / b.denominator_seconds if b.denominator_seconds > 0 else None
            if b.rate is not None:
                closed_with_rate.append(b)

    open_batch = next((b for b in batches if b.end is None), None)
    if open_batch is not None:
        recent = closed_with_rate[-PROVISIONAL_BATCH_LOOKBACK:]
        total_qty = sum(b.qty for b in recent)
        total_secs = sum(b.denominator_seconds for b in recent)
        open_batch.provisional = True
        open_batch.rate = total_qty / total_secs if total_secs > 0 else None


def _find_batch(batches: list[Batch], t: float) -> Batch | None:
    for b in batches:
        if b.start <= t and (b.end is None or t < b.end):
            return b
    return None


def _find_cycle(cycles_sorted: list[Cycle], t: float) -> Cycle | None:
    for c in cycles_sorted:
        if c.start <= t < c.end:
            return c
    return None


def build_segments(
    cycles: list[Cycle],
    batches: list[Batch],
    overall_start: float,
    overall_end: float,
    extra_cutpoints: list[float] | None = None,
) -> list[Segment]:
    """
    Sweep-line partition of [overall_start, overall_end) into elementary
    segments cut at every cycle boundary, batch boundary, and any extra
    cutpoints supplied (e.g. calendar month starts). Each segment is tagged
    with which cycle/batch (if any) it belongs to and how many bags it
    represents - so any grouping (by month, by cycle, by batch) is just a
    sum over the segments in that group, and a cycle or bags total that
    spans a cutpoint is automatically split proportionally by duration.
    """
    cuts = {overall_start, overall_end}
    for c in cycles:
        cuts.add(max(c.start, overall_start))
        cuts.add(min(c.end, overall_end))
    for b in batches:
        cuts.add(max(b.start, overall_start))
        if b.end is not None:
            cuts.add(min(b.end, overall_end))
    if extra_cutpoints:
        for t in extra_cutpoints:
            if overall_start <= t <= overall_end:
                cuts.add(t)

    points = sorted(t for t in cuts if overall_start <= t <= overall_end)

    segments: list[Segment] = []
    for t0, t1 in zip(points, points[1:]):
        if t1 <= t0:
            continue
        mid = (t0 + t1) / 2
        cycle = _find_cycle(cycles, mid)
        batch = _find_batch(batches, mid)

        if cycle is None:
            amount = 0.0
        elif cycle.anomalous or batch is None or batch.rate is None:
            amount = None
        else:
            amount = batch.rate * (t1 - t0)

        segments.append(Segment(t0, t1, cycle is not None, batch, cycle, amount))

    return segments


def month_start(ts: float) -> float:
    d = datetime.fromtimestamp(ts)
    return datetime(d.year, d.month, 1).timestamp()


def next_month_start(ts: float) -> float:
    d = datetime.fromtimestamp(ts)
    y, m = (d.year, d.month + 1) if d.month < 12 else (d.year + 1, 1)
    return datetime(y, m, 1).timestamp()


def month_key(ts: float) -> str:
    d = datetime.fromtimestamp(ts)
    return f"{d.year:04d}-{d.month:02d}"


def bags_by_month(segments: list[Segment]) -> dict[str, dict]:
    """
    Returns {month_key: {"known": float, "provisional": float, "unknown_seconds": float}}
    """
    out: dict[str, dict] = {}
    for s in segments:
        if s.amount is None and not s.is_on:
            continue
        key = month_key(s.start)
        bucket = out.setdefault(key, {"known": 0.0, "provisional": 0.0, "unknown_seconds": 0.0})
        if s.amount is None:
            bucket["unknown_seconds"] += s.duration
        elif s.batch and s.batch.provisional:
            bucket["provisional"] += s.amount
        else:
            bucket["known"] += s.amount
    return out


def detect_seasons(events: list[dict], now: float) -> list[dict]:
    """A season boundary is any gap of SEASON_GAP_SECONDS+ with no events at all."""
    if not events:
        return []
    seasons = []
    season_start = events[0]["time"]
    prev_time = events[0]["time"]
    for e in events[1:]:
        if e["time"] - prev_time >= SEASON_GAP_SECONDS:
            seasons.append({"start": season_start, "end": prev_time})
            season_start = e["time"]
        prev_time = e["time"]
    seasons.append({"start": season_start, "end": max(prev_time, now)})
    return seasons


def cumulative_by_season_day(segments: list[Segment], seasons: list[dict]) -> list[dict]:
    """
    For each season, a list of (day_of_season, cumulative_bags) points,
    counting provisional consumption but not fully-unknown segments.

    Each point also carries `calendar_ts`: the same day-of-season, replayed
    onto a shared synthetic year anchored at that season's actual start
    month/day. That lets seasons starting on different real dates be
    compared by calendar date (e.g. "bags used by Dec 1") rather than by
    day-of-season, while still handling the Dec->Jan rollover correctly.
    """
    result = []
    for season in seasons:
        points = []
        running = 0.0
        season_start_dt = datetime.fromtimestamp(season["start"])
        calendar_anchor = datetime(2000, season_start_dt.month, season_start_dt.day)
        for s in sorted(segments, key=lambda s: s.start):
            if s.start < season["start"] or s.start >= season["end"]:
                continue
            if s.amount:
                running += s.amount
                day = (s.end - season["start"]) / 86400
                calendar_ts = (calendar_anchor + timedelta(days=day)).timestamp()
                points.append({"day": day, "cumulative": running, "calendar_ts": calendar_ts})
        result.append(
            {
                "start": season["start"],
                "end": season["end"],
                "label": datetime.fromtimestamp(season["start"]).strftime("%Y-%m-%d"),
                "points": points,
            }
        )
    return result


def consumption_series(batches: list[Batch]) -> list[dict]:
    """Bags/day (of runtime) for each batch that has a known rate, in time order."""
    out = []
    for b in batches:
        if b.rate is None:
            continue
        out.append(
            {
                "start": b.start,
                "label": datetime.fromtimestamp(b.start).strftime("%Y-%m-%d"),
                "bags_per_day": b.rate * 86400,
                "provisional": b.provisional,
            }
        )
    return out


def current_consumption(batches: list[Batch]) -> dict:
    """Bags/day at the current (most recent) batch's rate - the same rate the fuel consumption chart plots."""
    if not batches or batches[-1].rate is None:
        return {"bags_per_day": None, "provisional": False}
    last = batches[-1]
    return {"bags_per_day": last.rate * 86400, "provisional": last.provisional}


def stock_timeline(events: list[dict]) -> list[dict]:
    """Running storage stock (restock adds, add-bags subtracts) over time."""
    points = []
    running = 0
    for e in events:
        if e["event"] == "restock":
            running += _value(e["data"])
            points.append({"time": e["time"], "stock": running, "event": "restock", "value": _value(e["data"])})
        elif e["event"] == "add-bags":
            running -= _value(e["data"])
            points.append({"time": e["time"], "stock": running, "event": "add-bags", "value": _value(e["data"])})
    return points


def current_stock(events: list[dict]) -> int:
    total = 0
    for e in events:
        if e["event"] == "restock":
            total += _value(e["data"])
        elif e["event"] == "add-bags":
            total -= _value(e["data"])
    return total


def recent_rate_per_day(segments: list[Segment], now: float, window_days: int = 21) -> float:
    window_start = now - window_days * 86400
    total = sum(s.amount for s in segments if s.amount and s.start >= window_start and s.start < now)
    return total / window_days


def seasonal_rate_per_day(
    seasons: list[dict], segments: list[Segment], now: float, window_days: int = 10
) -> float | None:
    """Average daily consumption in previous seasons around the same day-of-season as `now`."""
    if len(seasons) < 2:
        return None
    current = seasons[-1]
    day_of_season = (now - current["start"]) / 86400

    total = 0.0
    total_days = 0.0
    for season in seasons[:-1]:
        lo = max(season["start"] + (day_of_season - window_days) * 86400, season["start"])
        hi = min(season["start"] + (day_of_season + window_days) * 86400, season["end"])
        if hi <= lo:
            continue
        total += sum(s.amount for s in segments if s.amount and lo <= s.start < hi)
        total_days += (hi - lo) / 86400
    return total / total_days if total_days > 0 else None


def estimated_days_remaining(
    stock: int, recent_rate_per_day: float | None, seasonal_rate_per_day: float | None
) -> dict:
    """
    Blends the recent consumption pace with the historical pace for this
    point in the season. Falls back to whichever rate is available if the
    other is missing (e.g. only one season of history so far).
    """
    rates = [r for r in (recent_rate_per_day, seasonal_rate_per_day) if r and r > 0]
    if not rates:
        return {"days": None, "blended_rate_per_day": None}
    blended = sum(rates) / len(rates)
    return {"days": stock / blended, "blended_rate_per_day": blended}


def hopper_level(batches: list[Batch], cycles: list[Cycle], now: float) -> dict:
    """
    Estimated bags remaining in the actively-fed hopper right now, plus how
    many more hours of *burn time* (not wall-clock time) that's good for at
    the current batch's rate - i.e. if it kept running continuously.
    """
    if not batches:
        return {"bags_remaining": None, "provisional": False, "hours_remaining": None}
    last = batches[-1]
    if last.rate is None:
        return {"bags_remaining": None, "provisional": last.provisional, "hours_remaining": None}
    on_seconds = _on_seconds_in_window(cycles, last.start, now)
    remaining = max(0.0, last.qty - last.rate * on_seconds)
    hours_remaining = remaining / (last.rate * 3600) if last.rate > 0 else None
    return {"bags_remaining": remaining, "provisional": last.provisional, "hours_remaining": hours_remaining}


def current_status(events: list[dict], open_cycle: Cycle | None) -> dict:
    last = events[-1] if events else None
    running = open_cycle is not None
    last_temp = None
    for e in reversed(events):
        if e["event"] == "set-temp":
            last_temp = _value(e["data"])
            break
    return {
        "running": running,
        "target_temp": last_temp if running else None,
        "cycle_started": open_cycle.start if open_cycle else None,
        "cycle_duration_hours": open_cycle.duration / 3600 if open_cycle else None,
        "last_event_time": last["time"] if last else None,
    }


def cleaning_status(events: list[dict], now: float) -> dict:
    cleans = [e["time"] for e in events if e["event"] == "clean"]
    if not cleans:
        return {"last_clean": None, "days_since": None, "typical_interval_days": None, "overdue": False}
    cleans.sort()
    gaps = [cleans[i + 1] - cleans[i] for i in range(len(cleans) - 1)]
    typical = statistics.median(gaps) / 86400 if gaps else None
    days_since = (now - cleans[-1]) / 86400
    overdue = typical is not None and days_since > typical * CLEANING_OVERDUE_FACTOR
    return {
        "last_clean": cleans[-1],
        "days_since": days_since,
        "typical_interval_days": typical,
        "overdue": overdue,
    }


RANGE_DAYS = {"day": 1, "week": 7, "month": 30}


def range_window_start(range_key: str, seasons: list[dict], now: float) -> float:
    """Start of the requested display window; 0.0 (i.e. no filtering) for an unknown key."""
    if range_key in RANGE_DAYS:
        return now - RANGE_DAYS[range_key] * 86400
    if range_key == "season" and seasons:
        return seasons[-1]["start"]
    return 0.0


def quality_flags(cycles: list[Cycle], open_cycle: Cycle | None, batches: list[Batch]) -> list[dict]:
    flags = []
    all_cycles = cycles + ([open_cycle] if open_cycle else [])
    for c in all_cycles:
        if c.anomalous:
            flags.append(
                {
                    "type": "long_cycle",
                    "start": c.start,
                    "end": c.end,
                    "duration_days": c.duration / 86400,
                    "message": "Cycle longer than 10 days - likely a missing 'off' event, excluded from consumption calculations.",
                }
            )
    for b in batches:
        if b.end is not None and b.rate is None:
            flags.append(
                {
                    "type": "no_runtime",
                    "start": b.start,
                    "end": b.end,
                    "message": "No 'on' time observed between these two add-bags events - consumption rate for this batch is undefined.",
                }
            )
    return sorted(flags, key=lambda f: f["start"])
