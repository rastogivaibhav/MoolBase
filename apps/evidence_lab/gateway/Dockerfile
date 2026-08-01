FROM python:3.12-slim

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    PIP_NO_CACHE_DIR=1 \
    EVIDENCE_LAB_DATA_DIR=/var/lib/evidence-lab \
    EVIDENCE_LAB_PUBLIC_MODE=true

RUN groupadd --gid 10001 evidence-lab \
    && useradd --create-home --uid 10001 --gid 10001 evidence-lab
WORKDIR /app

COPY pyproject.toml /app/pyproject.toml
COPY evidence_lab /app/evidence_lab
RUN pip install --no-cache-dir '.[kubernetes]'

RUN mkdir -p /var/lib/evidence-lab \
    && chown -R evidence-lab:evidence-lab /var/lib/evidence-lab /app \
    && chmod -R go-w /app
USER 10001:10001

EXPOSE 8080
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
  CMD ["python", "-c", "import urllib.request; urllib.request.urlopen('http://127.0.0.1:8080/v1/public/health', timeout=3).read()"]

CMD ["uvicorn", "evidence_lab.main:app", "--host", "0.0.0.0", "--port", "8080", "--proxy-headers", "--forwarded-allow-ips", "*"]
