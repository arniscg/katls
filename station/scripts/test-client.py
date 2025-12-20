import json
import uuid
import time
import requests

URL = "http://localhost:8000"


def req(data: dict):
    print(data)
    r = requests.post(f"{URL}/events", json=data)
    print(r.status_code)
    if r.status_code != 200:
        print(r.json())


def send_temp():
    req(
        {
            "id": str(uuid.uuid4()),
            "time": int(time.time()),
            "event": "set-temp",
            "data": json.dumps({"value": 50}),
        }
    )


def send_clean():
    req({"id": str(uuid.uuid4()), "time": int(time.time()), "event": "clean"})


def send_off():
    req({"id": str(uuid.uuid4()), "time": int(time.time()), "event": "off"})


def send_restock():
    req(
        {
            "id": str(uuid.uuid4()),
            "time": int(time.time()),
            "event": "restock",
            "data": json.dumps({"value": 200}),
        }
    )


def send_add_bags():
    req(
        {
            "id": str(uuid.uuid4()),
            "time": int(time.time()),
            "event": "add-bags",
            "data": json.dumps({"value": 9}),
        }
    )


def main():
    while True:
        i = input("choose action: ")
        if i == "temp":
            send_temp()
        elif i == "clean":
            send_clean()
        elif i == "off":
            send_off()
        elif i == "restock":
            send_restock()
        elif i == "add-bags":
            send_add_bags()


if __name__ == "__main__":
    main()
