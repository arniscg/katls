from fastapi import FastAPI, Depends
from contextlib import asynccontextmanager
from .db import db_init, db_get, db_add, db_mark_deleted, db_getall
import sqlite3
from .model import Event


@asynccontextmanager
async def lifespan(app: FastAPI):
    db_init()
    yield


app = FastAPI(lifespan=lifespan)


@app.post("/events")
def post_event(event: Event, db: sqlite3.Connection = Depends(db_get)):
    db_add(db, event)
    return {}


@app.delete("/events/{event_id}")
def delete_event(event_id: str, db: sqlite3.Connection = Depends(db_get)):
    db_mark_deleted(db, event_id)
    return {}


@app.get("/events")
def get_events(db: sqlite3.Connection = Depends(db_get)):
    return db_getall(db)
