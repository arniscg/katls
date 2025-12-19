FROM python:3.14.2-alpine

COPY requirements.txt requirements.txt
RUN pip install -r requirements.txt
COPY ./app /app

CMD ["fastapi", "run", "/app/main.py", "--port", "8000"]