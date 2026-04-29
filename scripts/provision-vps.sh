#!/usr/bin/env bash
# Auto-KJ VPS Full Provision — one-shot rebuild
# Run this on a freshly installed Ubuntu 24.04 VPS as root
#
# Usage: curl -sS https://raw.githubusercontent.com/pkircher29/auto-kj/master/scripts/provision-vps.sh | bash
# Or: scp this to the VPS then run it

set -euo pipefail

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
log()  { echo -e "${GREEN}[✓]${NC} $1"; }
warn() { echo -e "${YELLOW}[!]${NC} $1"; }
fail() { echo -e "${RED}[✗]${NC} $1"; exit 1; }

# ---------- Configuration ----------
DOMAIN="auto-kj.com"
ADMIN_EMAIL="pkircher@gmail.com"
REPO_URL="https://github.com/pkircher29/auto-kj.git"
WEBSITE_REPO="https://github.com/pkircher29/auto-kj-website.git"
SERVER_REPO="https://github.com/pkircher29/auto-kj-server.git"
STRIKE_SECRET_KEY="${STRIKE_SECRET_KEY:-}"
STRIKE_PUBLISHABLE_KEY="${STRIKE_PUBLISHABLE_KEY:-}"
STRIKE_WEBHOOK_SECRET="${STRIKE_WEBHOOK_SECRET:-}"
RESEND_API_KEY="${RESEND_API_KEY:-}"
ADMIN_API_KEY="${ADMIN_API_KEY:-79bf9762214c2f1c021614160e73e48a4cc0c629b3e7747aa731f112565bb75d}"

# ---------- Prerequisites ----------
log "Updating system packages..."
apt-get update -qq && apt-get upgrade -y -qq
apt-get install -y -qq curl wget git nginx certbot python3-certbot-nginx nodejs npm python3 python3-pip python3-venv build-essential

# ---------- Node.js (ensure latest LTS via nodesource) ----------
if ! node --version | grep -q "^v2[0-9]"; then
    log "Installing Node.js LTS..."
    curl -fsSL https://deb.nodesource.com/setup_lts.x | bash -
    apt-get install -y -qq nodejs
fi
log "Node: $(node --version) | npm: $(npm --version)"

# ---------- PM2 (process manager) ----------
log "Installing PM2..."
npm install -g pm2

# ---------- nginx config ----------
log "Writing nginx configuration..."
cat > /etc/nginx/sites-available/auto-kj.conf << 'NGINX'
upstream autokj_license {
    server 127.0.0.1:3001;
    keepalive 64;
}

# HTTP → HTTPS redirect (ALL subdomains)
server {
    listen 80;
    listen [::]:80;
    server_name auto-kj.com www.auto-kj.com api.auto-kj.com singer.auto-kj.com dj.auto-kj.com kiosk.auto-kj.com;
    return 301 https://$host$request_uri;
}

# auto-kj.com — Static Astro site
server {
    listen 443 ssl http2;
    listen [::]:443 ssl http2;
    server_name auto-kj.com www.auto-kj.com;

    ssl_certificate /etc/letsencrypt/live/auto-kj.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/auto-kj.com/privkey.pem;

    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384;
    ssl_prefer_server_ciphers off;
    ssl_session_timeout 1d;
    ssl_session_cache shared:SSL:50m;

    add_header Strict-Transport-Security "max-age=63072000; includeSubDomains; preload" always;

    root /var/www/auto-kj/dist;
    index index.html;

    location / {
        try_files $uri $uri/index.html $uri.html /index.html;
    }

    location ~* \.(js|css|png|jpg|jpeg|gif|ico|svg|woff|woff2|ttf|eot)$ {
        expires 30d;
        add_header Cache-Control "public, immutable";
    }

    add_header X-Frame-Options "SAMEORIGIN" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header Referrer-Policy "strict-origin-when-cross-origin" always;

    if ($host = www.auto-kj.com) {
        return 301 https://auto-kj.com$request_uri;
    }

    access_log /var/log/nginx/auto-kj.access.log;
    error_log /var/log/nginx/auto-kj.error.log;
}

# api.auto-kj.com — License server API
server {
    listen 443 ssl http2;
    listen [::]:443 ssl http2;
    server_name api.auto-kj.com;

    ssl_certificate /etc/letsencrypt/live/auto-kj.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/auto-kj.com/privkey.pem;

    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384;
    ssl_prefer_server_ciphers off;

    add_header Strict-Transport-Security "max-age=63072000; includeSubDomains; preload" always;
    add_header X-Frame-Options "DENY" always;
    add_header X-Content-Type-Options "nosniff" always;

    location / {
        proxy_pass http://autokj_license;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection 'upgrade';
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_cache_bypass $http_upgrade;
        proxy_read_timeout 30s;
        proxy_send_timeout 30s;
        client_max_body_size 2m;
    }

    access_log /var/log/nginx/api-auto-kj.access.log;
    error_log /var/log/nginx/api-auto-kj.error.log;
}

# singer.auto-kj.com — Singer PWA
server {
    listen 443 ssl http2;
    listen [::]:443 ssl http2;
    server_name singer.auto-kj.com;

    ssl_certificate /etc/letsencrypt/live/auto-kj.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/auto-kj.com/privkey.pem;

    ssl_protocols TLSv1.2 TLSv1.3;
    add_header Strict-Transport-Security "max-age=63072000; includeSubDomains; preload" always;

    root /var/www/auto-kj-server/singer-app;
    index index.html;

    location / {
        try_files $uri $uri/ /index.html;
    }

    location /api/ {
        proxy_pass http://autokj_license;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection 'upgrade';
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }

    access_log /var/log/nginx/singer-auto-kj.access.log;
    error_log /var/log/nginx/singer-auto-kj.error.log;
}

# dj.auto-kj.com — KJ Dashboard PWA
server {
    listen 443 ssl http2;
    listen [::]:443 ssl http2;
    server_name dj.auto-kj.com;

    ssl_certificate /etc/letsencrypt/live/auto-kj.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/auto-kj.com/privkey.pem;

    ssl_protocols TLSv1.2 TLSv1.3;
    add_header Strict-Transport-Security "max-age=63072000; includeSubDomains; preload" always;

    root /var/www/auto-kj-server/dj-dashboard;
    index index.html;

    location / {
        try_files $uri $uri/ /index.html;
    }

    location /api/ {
        proxy_pass http://autokj_license;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection 'upgrade';
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }

    access_log /var/log/nginx/dj-auto-kj.access.log;
    error_log /var/log/nginx/dj-auto-kj.error.log;
}

# kiosk.auto-kj.com — Kiosk PWA
server {
    listen 443 ssl http2;
    listen [::]:443 ssl http2;
    server_name kiosk.auto-kj.com;

    ssl_certificate /etc/letsencrypt/live/auto-kj.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/auto-kj.com/privkey.pem;

    ssl_protocols TLSv1.2 TLSv1.3;
    add_header Strict-Transport-Security "max-age=63072000; includeSubDomains; preload" always;

    root /var/www/auto-kj-server/kiosk;
    index index.html;

    location / {
        try_files $uri $uri/ /index.html;
    }

    access_log /var/log/nginx/kiosk-auto-kj.access.log;
    error_log /var/log/nginx/kiosk-auto-kj.error.log;
}
NGINX

rm -f /etc/nginx/sites-enabled/default
ln -sf /etc/nginx/sites-available/auto-kj.conf /etc/nginx/sites-enabled/auto-kj.conf

# ---------- SSL via Let's Encrypt ----------
log "Obtaining SSL certificates (must be online, DNS correctly pointed)..."
certbot --nginx -d auto-kj.com -d www.auto-kj.com -d api.auto-kj.com -d singer.auto-kj.com -d dj.auto-kj.com -d kiosk.auto-kj.com --non-interactive --agree-tos --email "$ADMIN_EMAIL" || warn "certbot failed. Run manually after DNS propagates."

# ---------- Directory structure ----------
log "Creating directory structure..."
mkdir -p /var/www/auto-kj/dist
mkdir -p /var/www/auto-kj/server
mkdir -p /var/www/auto-kj-server/{singer-app,dj-dashboard,kiosk,backend}

# ---------- Website (Astro static site) ----------
log "Building website..."
if [ -d /tmp/auto-kj-website ]; then rm -rf /tmp/auto-kj-website; fi
git clone --depth=1 "$WEBSITE_REPO" /tmp/auto-kj-website
cd /tmp/auto-kj-website
npm ci
npm run build
cp -r dist/* /var/www/auto-kj/dist/
log "Website built and deployed."

# ---------- License Server (Node.js, port 3001) ----------
log "Setting up license server..."
cp -r /tmp/auto-kj-website/server/* /var/www/auto-kj/server/
cd /var/www/auto-kj/server
npm ci --omit=dev

# Create .env for license server
cat > /var/www/auto-kj/server/.env << ENV
PORT=3001
NODE_ENV=production
DB_PATH=./data/licenses.db
ADMIN_API_KEY=${ADMIN_API_KEY}
ALLOWED_ORIGINS=https://auto-kj.com,https://www.auto-kj.com,https://api.auto-kj.com,https://singer.auto-kj.com,https://dj.auto-kj.com,https://kiosk.auto-kj.com
APP_URL=https://auto-kj.com
SUCCESS_URL=https://auto-kj.com/success
CANCEL_URL=https://auto-kj.com/cancel
MANAGE_URL=https://auto-kj.com/manage
STRIPE_SECRET_KEY=${STRIKE_SECRET_KEY}
STRIPE_PUBLISHABLE_KEY=${STRIKE_PUBLISHABLE_KEY}
STRIPE_WEBHOOK_SECRET=${STRIKE_WEBHOOK_SECRET}
STRIPE_PRICE_ADVANCED_MONTHLY=price_1TMbqs3ZYLz2LK0Li5ISeWuy
STRIPE_PRICE_ADVANCED_YEARLY=price_1TMbqs3ZYLz2LK0LCVG9QAwt
STRIPE_PRICE_UNLIMITED_MONTHLY=price_1TMbqq3ZYLz2LK0LpMflm7p2
STRIPE_PRICE_UNLIMITED_YEARLY=price_1TMbqs3ZYLz2LK0LsoDcmtV4
STRIPE_PRICE_LIFETIME=price_1TMbqs3ZYLz2LK0Lo18naB4Q
RESEND_API_KEY=${RESEND_API_KEY}
LIFETIME_MAX=10
ENV

mkdir -p /var/www/auto-kj/server/data

# ---------- Auto-KJ Server (Python FastAPI backend) ----------
log "Setting up Auto-KJ Server backend..."
git clone --depth=1 "$SERVER_REPO" /tmp/auto-kj-server-clone
cp -r /tmp/auto-kj-server-clone/backend/* /var/www/auto-kj-server/backend/

cd /var/www/auto-kj-server/backend
python3 -m venv venv
source venv/bin/activate
pip install -q -r requirements.txt
deactivate

# Create .env for backend
cat > /var/www/auto-kj-server/backend/.env << ENV
APP_ENV=production
SECRET_KEY=$(openssl rand -hex 32)
DEBUG=false
DATABASE_URL=sqlite+aiosqlite:///./autokj.db
ENV

# ---------- Frontend apps (singer, dj, kiosk) ----------
log "Building frontend apps..."
cd /tmp/auto-kj-server-clone/frontend
npm ci
npm run build 2>&1 || warn "Frontend build had issues"

# Copy built frontends to web root
if [ -d packages/singer-app/dist ]; then
    cp -r packages/singer-app/dist/* /var/www/auto-kj-server/singer-app/
fi
if [ -d apps/singer-app/dist ]; then
    cp -r apps/singer-app/dist/* /var/www/auto-kj-server/singer-app/
fi

if [ -d packages/dj-dashboard/dist ]; then
    cp -r packages/dj-dashboard/dist/* /var/www/auto-kj-server/dj-dashboard/
fi
if [ -d apps/dj-dashboard/dist ]; then
    cp -r apps/dj-dashboard/dist/* /var/www/auto-kj-server/dj-dashboard/
fi

if [ -d packages/kiosk/dist ]; then
    cp -r packages/kiosk/dist/* /var/www/auto-kj-server/kiosk/
fi
if [ -d apps/kiosk/dist ]; then
    cp -r apps/kiosk/dist/* /var/www/auto-kj-server/kiosk/
fi

# ---------- PM2 start ----------
log "Starting PM2 processes..."
cd /var/www/auto-kj/server
pm2 start index.js --name "auto-kj-license" -i max

cd /var/www/auto-kj-server/backend
pm2 start venv/bin/uvicorn --name "auto-kj-backend" -- app.main:app --host 127.0.0.1 --port 3002

pm2 save
pm2 startup systemd -u root 2>/dev/null || true

# ---------- nginx restart ----------
log "Testing and restarting nginx..."
nginx -t && systemctl restart nginx

# ---------- SSL auto-renewal ----------
log "Setting up certbot auto-renewal..."
systemctl enable certbot.timer 2>/dev/null || true
(crontab -l 2>/dev/null; echo "0 3 * * * /usr/bin/certbot renew --quiet --deploy-hook 'systemctl reload nginx'") | crontab -

# ---------- Firewall ----------
log "Configuring UFW..."
ufw allow 22/tcp
ufw allow 80/tcp
ufw allow 443/tcp
ufw --force enable

# ---------- Done ----------
log "========================================="
log "  Auto-KJ VPS Provision Complete!"
log "========================================="
log "  Website:  https://auto-kj.com"
log "  API:      https://api.auto-kj.com"
log "  Singer:   https://singer.auto-kj.com"
log "  DJ:       https://dj.auto-kj.com"
log "  Kiosk:    https://kiosk.auto-kj.com"
log "========================================="
echo ""
warn "IMPORTANT: Set these environment variables when running this script:"
echo "  STRIKE_SECRET_KEY=sk_live_..."
echo "  STRIKE_PUBLISHABLE_KEY=pk_live_..."
echo "  STRIKE_WEBHOOK_SECRET=whsec_..."
echo "  RESEND_API_KEY=re_..."
echo ""
warn "If certbot failed, run manually after DNS propagates:"
echo "  certbot --nginx -d auto-kj.com -d www.auto-kj.com -d api.auto-kj.com -d singer.auto-kj.com -d dj.auto-kj.com -d kiosk.auto-kj.com"
echo ""
log "Check status with: pm2 list"
