from fastapi import FastAPI, Depends
from contextlib import asynccontextmanager
from .db import db_init, db_get, db_add, db_mark_deleted, db_getall
import sqlite3
from .model import Event
from dotenv import load_dotenv
from fastapi.staticfiles import StaticFiles
from fastapi.routing import Mount, APIRouter

from .routes.api import router as api
from .routes.pages import router as pages
from .routes.htmx import router as htmx


@asynccontextmanager
async def lifespan(app: FastAPI):
    load_dotenv()
    db_init()
    yield


app = FastAPI(lifespan=lifespan)

app.include_router(pages)

app.include_router(htmx, prefix="/hx")

app.include_router(api, prefix="/api/v1")

app.mount("/static", StaticFiles(directory="app/static"), name="static")
