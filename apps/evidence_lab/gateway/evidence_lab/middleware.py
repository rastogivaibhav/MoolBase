from __future__ import annotations

from collections import defaultdict, deque
import asyncio
import secrets
import time
from typing import Callable

from fastapi import Request
from fastapi.responses import JSONResponse
from starlette.middleware.base import BaseHTTPMiddleware
from starlette.types import ASGIApp


class RequestSecurityMiddleware(BaseHTTPMiddleware):
    def __init__(
        self,
        app: ASGIApp,
        *,
        max_content_length: int,
        allowed_hosts: tuple[str, ...],
    ) -> None:
        super().__init__(app)
        self.max_content_length = max_content_length
        self.allowed_hosts = tuple(item.casefold() for item in allowed_hosts)

    def _host_allowed(self, host: str) -> bool:
        candidate = host.split(":", 1)[0].strip("[]").casefold()
        for allowed in self.allowed_hosts:
            if allowed == "*" or candidate == allowed:
                return True
            if allowed.startswith("*.") and candidate.endswith(allowed[1:]):
                return True
        return False

    async def dispatch(self, request: Request, call_next: Callable):
        request_id = request.headers.get("X-Request-ID") or f"req_{secrets.token_urlsafe(12)}"
        request.state.request_id = request_id

        if not request.url.path.endswith("/health"):
            host = request.headers.get("host", "")
            if not host or not self._host_allowed(host):
                return JSONResponse(
                    status_code=400,
                    content={
                        "error": "invalid_host",
                        "detail": "request Host is not allowed",
                        "request_id": request_id,
                    },
                    headers={"X-Request-ID": request_id},
                )

        content_length = request.headers.get("content-length")
        if content_length:
            try:
                if int(content_length) > self.max_content_length:
                    return JSONResponse(
                        status_code=413,
                        content={
                            "error": "payload_too_large",
                            "detail": "request body exceeds public limit",
                            "request_id": request_id,
                        },
                    )
            except ValueError:
                return JSONResponse(
                    status_code=400,
                    content={
                        "error": "invalid_content_length",
                        "detail": "invalid Content-Length header",
                        "request_id": request_id,
                    },
                )
        response = await call_next(request)
        response.headers["X-Request-ID"] = request_id
        response.headers["X-Content-Type-Options"] = "nosniff"
        response.headers["X-Frame-Options"] = "DENY"
        response.headers["Referrer-Policy"] = "no-referrer"
        response.headers["Permissions-Policy"] = "camera=(), microphone=(), geolocation=(), payment=(), usb=()"
        response.headers["Cross-Origin-Resource-Policy"] = "same-site"
        response.headers["Cache-Control"] = "no-store"
        return response


class SlidingWindowRateLimitMiddleware(BaseHTTPMiddleware):
    def __init__(self, app: ASGIApp, *, requests: int, window_seconds: int) -> None:
        super().__init__(app)
        self.requests = requests
        self.window_seconds = window_seconds
        self._events: dict[str, deque[float]] = defaultdict(deque)
        self._lock = asyncio.Lock()

    @staticmethod
    def _key(request: Request) -> str:
        forwarded = request.headers.get("x-forwarded-for", "").split(",", 1)[0].strip()
        client = forwarded or (request.client.host if request.client else "unknown")
        session = request.headers.get("x-session-id", "anonymous")[:96]
        return f"{client}:{session}"

    async def dispatch(self, request: Request, call_next: Callable):
        if request.url.path.endswith("/health") or request.method == "OPTIONS":
            return await call_next(request)
        now = time.monotonic()
        key = self._key(request)
        async with self._lock:
            queue = self._events[key]
            cutoff = now - self.window_seconds
            while queue and queue[0] <= cutoff:
                queue.popleft()
            if len(queue) >= self.requests:
                retry_after = max(1, int(self.window_seconds - (now - queue[0])))
                return JSONResponse(
                    status_code=429,
                    content={"error": "rate_limited", "detail": "public request limit exceeded"},
                    headers={"Retry-After": str(retry_after)},
                )
            queue.append(now)
            if len(self._events) > 10000:
                stale = [item for item, values in self._events.items() if not values or values[-1] <= cutoff]
                for item in stale[:1000]:
                    self._events.pop(item, None)
        return await call_next(request)
