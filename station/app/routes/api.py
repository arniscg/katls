from fastapi import APIRouter, Depends
import sqlite3
from ..model import Event
from ..db import *

router = APIRouter()


@router.post("/events")
def post_event(event: Event, db: sqlite3.Connection = Depends(db_get)):
    db_add(db, event)
    return {}


@router.delete("/events/{event_id}")
def delete_event(event_id: str, db: sqlite3.Connection = Depends(db_get)):
    db_mark_deleted(db, event_id)
    return {}


@router.get("/events")
def get_events(db: sqlite3.Connection = Depends(db_get)):
    return db_getall(db)
