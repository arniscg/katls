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
from .routes.dashboard import page_router as dashboard_page, router as dashboard_hx


@asynccontextmanager
async def lifespan(app: FastAPI):
    load_dotenv()
    db_init()
    yield


app = FastAPI(lifespan=lifespan)


@app.middleware("http")
async def no_cache_dashboard(request, call_next):
    response = await call_next(request)
    if request.url.path == "/dashboard" or request.url.path.startswith("/hx/dashboard"):
        response.headers["Cache-Control"] = "no-store"
    return response


app.include_router(pages)
app.include_router(dashboard_page)

app.include_router(htmx, prefix="/hx")
app.include_router(dashboard_hx, prefix="/hx/dashboard")

app.include_router(api, prefix="/api/v1")

app.mount("/static", StaticFiles(directory="app/static"), name="static")
