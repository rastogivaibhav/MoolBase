# GrapheneDB Server Deployment Security

GrapheneDB's built-in server intentionally speaks plain HTTP. It must not be exposed directly to an untrusted network. Terminate TLS at a trusted reverse proxy and place GrapheneDB on a private network.

## Secure-by-default server behaviour

- Non-loopback binds are rejected unless an API key is configured.
- Non-loopback binds are also rejected unless `--behind-tls-proxy` is acknowledged.
- `--require-forwarded-https` optionally rejects application requests unless the trusted proxy supplies `X-Forwarded-Proto: https`.
- `--trust-proxy` must only be enabled when the server is reachable exclusively through a trusted proxy; it uses the first `X-Forwarded-For` address for rate limiting and logs.
- `/v1/health` and `/v1/ready` are unauthenticated and reveal no database counts. Other endpoints require the API key when configured.

Recommended command behind a same-host proxy:

```bash
export GRAPHENEDB_API_KEY='replace-with-a-random-secret'
./graphenedb_server /var/lib/graphenedb 64 8080 \
  --bind-address 127.0.0.1 \
  --physical-lattice-primary \
  --physical-lattice-radius 600 \
  --expected-max-nodes 1000000 \
  --workers 8 --queue-capacity 1024 \
  --rate-limit-rps 200 --rate-limit-burst 400
```

For a private container network, use `--bind-address 0.0.0.0 --behind-tls-proxy`. Add `--trust-proxy --require-forwarded-https` only when direct access to the GrapheneDB container is blocked.

## Caddy

The repository includes `docker-compose.secure.yml` and `deploy/Caddyfile`. Configure a real hostname and set the API key before startup:

```bash
export GRAPHENEDB_API_KEY="$(openssl rand -hex 32)"
export GRAPHENEDB_HOST=db.example.com
docker compose -f docker-compose.secure.yml up --build
```

Do not publish the GrapheneDB container's port. Only the reverse proxy should publish port 443.

## NGINX equivalent

```nginx
server {
    listen 443 ssl http2;
    server_name db.example.com;

    ssl_certificate     /etc/letsencrypt/live/db.example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/db.example.com/privkey.pem;

    client_max_body_size 4m;
    proxy_connect_timeout 5s;
    proxy_read_timeout 35s;
    proxy_send_timeout 35s;

    location / {
        proxy_pass http://127.0.0.1:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-Proto https;
        proxy_set_header X-Forwarded-For $remote_addr;
    }
}
```

## Container runtime controls

Run as UID/GID 10001, drop all capabilities, enable `no-new-privileges`, use a read-only root filesystem, and mount only `/var/lib/graphenedb` read-write. The secure Compose file applies these controls.

Release CI should pin base images by digest and scan the built image with the organisation's approved scanner, for example Trivy or Grype. The repository's static validator checks structural hardening but cannot replace a current CVE scan.
