from fastapi import APIRouter, Request, Depends
from fastapi.responses import HTMLResponse
from fastapi.templating import Jinja2Templates
from ..db import *
from datetime import datetime
from ..parser import db_entries_to_txt

router = APIRouter()
templates = Jinja2Templates(directory="app/templates")


@router.get("/events", response_class=HTMLResponse)
def events(request: Request, db: sqlite3.Connection = Depends(db_get)):
    all = db_getall(db, 100)

    # evs: list[dict] = []
    # for e in all:
    #     if e["deleted"]:
    #         continue
    #     time_str = datetime.fromtimestamp(e["time"]).strftime("%d.%m %H:%M")
    #     evs.append({"time": time_str, "event": e["event"], "data": str(e["data"])})

    evs = db_entries_to_txt(all)

    return templates.TemplateResponse(
        request, "htmx/events.html", context={"events": evs}
    )
