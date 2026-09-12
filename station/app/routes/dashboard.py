import os
import time
from datetime import datetime

from fastapi import APIRouter, Depends, Request
from fastapi.responses import HTMLResponse
from fastapi.templating import Jinja2Templates

from .. import analytics as A
from ..db import db_get, db_get_events_for_analytics

page_router = APIRouter()
router = APIRouter()
templates = Jinja2Templates(directory="app/templates")


def _hours_str(hours: float | None) -> str:
    if hours is None:
        return "unknown"
    if hours < 24:
        return f"{hours:.1f}h"
    return f"{int(hours // 24)}d {hours % 24:.1f}h"


templates.env.filters["hours_str"] = _hours_str


class Snapshot:
    def __init__(self, events: list[dict]):
        self.events = events
        # KATLS_SIMULATED_NOW lets a dev instance pretend "now" is some other
        # moment (e.g. to demo the dashboard mid-cycle, off-season). Unset in
        # normal/production use, where it's just real wall-clock time.
        simulated_now = os.environ.get("KATLS_SIMULATED_NOW")
        self.now = float(simulated_now) if simulated_now else time.time()
        self.cycles, self.open_cycle = A.build_cycles(events, self.now)
        self.all_cycles = self.cycles + ([self.open_cycle] if self.open_cycle else [])
        self.batches = A.build_batches(events)
        A.compute_batch_rates(self.batches, self.all_cycles, self.now)
        self.seasons = A.detect_seasons(events, self.now)
        start = events[0]["time"] if events else self.now
        self.segments = A.build_segments(self.all_cycles, self.batches, start, self.now)


def _snapshot(db=Depends(db_get)) -> Snapshot:
    return Snapshot(db_get_events_for_analytics(db))


@page_router.get("/dashboard", response_class=HTMLResponse)
def dashboard(request: Request):
    return templates.TemplateResponse(request, "pages/dashboard.html")


@router.get("/status", response_class=HTMLResponse)
def status(request: Request, s: Snapshot = Depends(_snapshot)):
    status = A.current_status(s.events, s.open_cycle)
    stock = A.current_stock(s.events)
    hopper = A.hopper_level(s.batches, s.all_cycles, s.now)
    cleaning = A.cleaning_status(s.events, s.now)
    consumption = A.current_consumption(s.batches)
    last_event_age_days = (s.now - s.events[-1]["time"]) / 86400 if s.events else None
    return templates.TemplateResponse(
        request,
        "dashboard/status.html",
        context={
            "status": status,
            "stock": stock,
            "hopper": hopper,
            "cleaning": cleaning,
            "consumption": consumption,
            "last_event_age_days": last_event_age_days,
        },
    )


@router.get("/stock", response_class=HTMLResponse)
def stock(request: Request, period: str = "month", s: Snapshot = Depends(_snapshot)):
    stock = A.current_stock(s.events)
    recent = A.recent_rate_per_day(s.segments, s.now)
    seasonal = A.seasonal_rate_per_day(s.seasons, s.segments, s.now)
    estimate = A.estimated_days_remaining(stock, recent, seasonal)
    window_start = A.range_window_start(period, s.seasons, s.now)
    timeline = [p for p in A.stock_timeline(s.events) if p["time"] >= window_start]
    return templates.TemplateResponse(
        request,
        "dashboard/stock.html",
        context={
            "stock": stock,
            "recent_rate": recent,
            "seasonal_rate": seasonal,
            "estimate": estimate,
            "timeline": timeline,
            "period": period,
        },
    )


@router.get("/seasons", response_class=HTMLResponse)
def seasons(request: Request, s: Snapshot = Depends(_snapshot)):
    series = A.cumulative_by_season_day(s.segments, s.seasons)
    return templates.TemplateResponse(request, "dashboard/seasons.html", context={"series": series})


@router.get("/consumption", response_class=HTMLResponse)
def consumption(request: Request, period: str = "month", s: Snapshot = Depends(_snapshot)):
    window_start = A.range_window_start(period, s.seasons, s.now)
    series = [p for p in A.consumption_series(s.batches) if p["start"] >= window_start]
    return templates.TemplateResponse(
        request, "dashboard/consumption.html", context={"series": series, "period": period}
    )


@router.get("/cleaning", response_class=HTMLResponse)
def cleaning(request: Request, period: str = "month", s: Snapshot = Depends(_snapshot)):
    cleans = sorted(e["time"] for e in s.events if e["event"] == "clean")
    status = A.cleaning_status(s.events, s.now)
    window_start = A.range_window_start(period, s.seasons, s.now)
    gaps = [
        {"time": cleans[i], "gap_days": (cleans[i] - cleans[i - 1]) / 86400}
        for i in range(1, len(cleans))
        if cleans[i] >= window_start
    ]
    return templates.TemplateResponse(
        request, "dashboard/cleaning.html", context={"gaps": gaps, "status": status, "period": period}
    )


@router.get("/quality", response_class=HTMLResponse)
def quality(request: Request, s: Snapshot = Depends(_snapshot)):
    flags = A.quality_flags(s.cycles, s.open_cycle, s.batches)
    for f in flags:
        f["start_str"] = datetime.fromtimestamp(f["start"]).strftime("%Y-%m-%d %H:%M")
        f["end_str"] = datetime.fromtimestamp(f["end"]).strftime("%Y-%m-%d %H:%M") if f.get("end") else None
    return templates.TemplateResponse(request, "dashboard/quality.html", context={"flags": flags})
