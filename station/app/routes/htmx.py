from fastapi import APIRouter, Request, Depends, Form, Response
from fastapi.responses import HTMLResponse
from fastapi.templating import Jinja2Templates
from ..db import *
from datetime import datetime
from ..parser import *
from typing import Annotated
from ..model import Event
from uuid import uuid4

router = APIRouter()
templates = Jinja2Templates(directory="app/templates")


@router.get("/events", response_class=HTMLResponse)
def events(request: Request, db: sqlite3.Connection = Depends(db_get)):
    all = db_getall(db, 100)
    evs_strs = db_entries_to_txt(all)

    evs = []
    for e, s in zip(all, evs_strs):
        evs.append({"event": s, "id": e["id"]})

    return templates.TemplateResponse(
        request, "htmx/events.html", context={"events": evs}
    )


@router.post("/events", response_class=HTMLResponse)
def events_add(
    request: Request,
    entry: Annotated[str, Form()],
    year: Annotated[str, Form()],
    db: sqlite3.Connection = Depends(db_get),
):
    msg: str = ""
    success = False

    entries: list[Entry] = []
    try:
        entries = parse_str_entry(entry, int(year))
        msg = f"Success"
        success = True
    except Exception as e:
        msg = f"Error: {e}"
        success = False

    for e in entries:
        d = {
            "id": str(uuid4()),
            "time": e.time.timestamp(),
        }
        if isinstance(e, BagsEntry):
            d["event"] = "add-bags"
            d["data"] = json.dumps({"value": e.count})
        elif isinstance(e, TempEntry):
            d["event"] = "set-temp"
            d["data"] = json.dumps({"value": e.temp})
        elif isinstance(e, RestockEntry):
            d["event"] = "restock"
            d["data"] = json.dumps({"value": e.bag_count})
        elif isinstance(e, CleanEntry):
            d["event"] = "clean"
        elif isinstance(e, StopEntry):
            d["event"] = "off"

        try:
            ev = Event(**d)
            db_add(db, ev)
        except Exception as e:
            msg = f"Error: {e}"
            success = False
            break

    return templates.TemplateResponse(
        request,
        "htmx/events_add_form.html",
        context={"success": success, "message": msg},
        headers={"HX-Trigger": "updEntries"},
    )


@router.get("/events/add", response_class=HTMLResponse)
def events_add_form(request: Request):
    return templates.TemplateResponse(request, "htmx/events_add_form.html")