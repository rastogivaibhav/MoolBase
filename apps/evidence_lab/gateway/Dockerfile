FROM python:3.12-slim

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    EVIDENCE_LAB_DATA_DIR=/var/lib/evidence-lab

RUN useradd --create-home --uid 10001 evidence-lab
WORKDIR /app

COPY pyproject.toml /app/pyproject.toml
COPY evidence_lab /app/evidence_lab
RUN pip install --no-cache-dir .

RUN mkdir -p /var/lib/evidence-lab && chown -R evidence-lab:evidence-lab /var/lib/evidence-lab /app
USER evidence-lab

EXPOSE 8080
CMD ["uvicorn", "evidence_lab.main:app", "--host", "0.0.0.0", "--port", "8080", "--proxy-headers"]
