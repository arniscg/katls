FROM python:3.14.2-alpine

ENV TZ="Europe/Riga"
COPY requirements.txt requirements.txt
COPY .env.example .env
RUN pip install -r requirements.txt
COPY ./app /app

CMD ["fastapi", "run", "/app/main.py", "--port", "8000"]