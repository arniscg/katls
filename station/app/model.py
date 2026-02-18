from enum import Enum
from pydantic import BaseModel


class EventType(str, Enum):
    Temp = "set-temp"
    Clean = "clean"
    Off = "off"
    Restock = "restock"
    AddBags = "add-bags"


class Event(BaseModel):
    id: str
    time: int
    event: EventType
    data: str = ""
    deleted: int = 0
