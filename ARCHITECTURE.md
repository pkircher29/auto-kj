# Auto-KJ System Architecture

This document describes the full stack as of v0.5.1, including the request server, authentication flow, license system, and website.

---

## Overview

Auto-KJ is composed of four parts that work together:

```
┌──────────────────────┐       ┌────────────────────────────────────┐
│   Desktop App        │       │         api.auto-kj.com            │
│   (C++/Qt)           │──────▶│  ┌──────────────────────────────┐  │
│                      │       │  │  FastAPI backend  (port 8001) │  │
│  - Rotation engine   │       │  │  Singer requests, queue mgmt  │  │
│  - Fairness engine   │       │  │  OAuth2 Bearer token auth     │  │
│  - Karaoke playback  │       │  └──────────────────────────────┘  │
│  - Settings dialog   │       │  ┌──────────────────────────────┐  │
│  - Registration dlg  │       │  │  License server   (port 3001) │  │
└──────────────────────┘       │  │  Stripe checkout, license mgmt│  │
                               │  │  Coupon code redemption       │  │
                               │  └──────────────────────────────┘  │
                               └────────────────────────────────────┘
                                             ▲
                               ┌─────────────┴───────────────────────┐
                               │   auto-kj.com  (Astro static site)  │
                               │   Registration, checkout, pricing   │
                               └─────────────────────────────────────┘
```

Nginx reverse-proxies both ports behind HTTPS at `api.auto-kj.com`.

---

## 1. Desktop App (`src/`)

**Language:** C++17, Qt 5.15.2  
**Build:** CMake → MSVC (Windows CI via GitHub Actions), GCC/Clang (Linux/macOS)

### Request Server Authentication

The desktop app authenticates to the FastAPI backend using **email + password → Bearer token** flow, replacing the legacy API key approach.

**Key files:**

| File | Purpose |
|------|---------|
| `src/settings.h/.cpp` | Stores email, password, and cached token in QSettings. On email/password change, clears the cached token to force re-login. Legacy URL migration (`api.okjsongbook.com` → `api.auto-kj.com`) applied on read. |
| `src/autokjserverapi.h/.cpp` | HTTP client for the FastAPI backend. `ensureToken()` checks in-memory cache → QSettings → calls `login()`. `setAuthHeader()` adds `Authorization: Bearer <token>`. Auto-retries with fresh login on 401. |
| `src/dlgsettings.h/.cpp/.ui` | Settings dialog — Network tab now has Email and Password fields instead of API Key. Includes "Create Account" button. |
| `src/dlgregister.h/.cpp/.ui` | In-app account registration dialog. POSTs `{email, password, display_name}` to `/api/v1/auth/register`. On success, fills email/password in settings. |

**Auth flow (desktop):**
```
1. User enters email + password in Settings → Network
2. Click "Test" → ensureToken() → login() → POST /api/v1/auth/login
3. Token stored in memory (m_token) and QSettings
4. All API calls use Bearer token header
5. On 401: clear token, re-login, retry once
```

### Key Changes from v0.5.0

- `requestServerApiKey()` removed — replaced with `requestServerEmail()` / `requestServerPassword()` / `requestServerToken()`
- `hasCredentials()` replaces `hasApiKey()` throughout
- Update checker now uses HTTPS and validates version string format (prevents 301 redirect body being parsed as a version number)

---

## 2. FastAPI Backend (`/opt/autokjpro/backend/`)

**Runtime:** Python, uvicorn on port 8001, managed by PM2 (process name: `api`)  
**Auth:** OAuth2PasswordBearer — `POST /api/v1/auth/login` returns `{access_token}` (JWT)

### Relevant Endpoints

| Method | Path | Description |
|--------|------|-------------|
| `POST` | `/api/v1/auth/login` | Email + password → access token |
| `POST` | `/api/v1/auth/register` | Create KJ account `{email, password, display_name}` |
| `GET`  | `/api/v1/kj/venues` | List venues for authenticated KJ (used as connection test) |

---

## 3. License Server (`/var/www/auto-kj/server/`)

**Runtime:** Node.js, Express on port 3001, managed by PM2 (process name: `auto-kj-license`)  
**Database:** SQLite via sql.js (file: `data/licenses.db`)

### Routes

| Method | Path | Description |
|--------|------|-------------|
| `POST` | `/api/v1/checkout` | Create Stripe Checkout session |
| `POST` | `/api/v1/coupon/redeem` | Redeem coupon code for free license |
| `GET`  | `/api/v1/verify` | Verify a license key |
| `POST` | `/api/v1/activate/:apiKey` | Activate license on a device |
| `DELETE` | `/api/v1/activate/:apiKey` | Deactivate device |
| `POST` | `/api/v1/portal` | Stripe Customer Portal redirect |
| `GET`  | `/api/v1/inventory` | Check lifetime tier availability |
| `POST` | `/api/v1/webhook` | Stripe webhook (fulfillment) |

### Coupon System

Coupons are stored in the `coupons` table. Each has a `max_uses` limit and a `use_count`. Redemption is atomic — the UPDATE increments `use_count WHERE use_count < max_uses` and checks `getRowsModified()` to confirm the claim.

**Note:** `getRowsModified()` must be called **before** `saveDbSync()` — calling `db.export()` inside saveDbSync resets the internal changes counter.

**Current coupon:** `karaoke1976` — 10 uses, grants a free lifetime license.

### Database Schema Summary

```sql
customers        -- KJ accounts (Stripe or coupon-created)
licenses         -- License keys with tier, expiry, venue/singer limits
activations      -- Per-device license activations (max 3 per key)
coupons          -- Coupon codes with use_count / max_uses
coupon_redemptions -- Audit log of coupon redemptions
lifetime_inventory -- Tracks remaining lifetime slots
webhook_events   -- Stripe webhook idempotency log
```

---

## 4. Website (`auto-kj-website-review/`)

**Framework:** Astro (static site)  
**Deployed:** `/var/www/auto-kj/dist/` via `scp` after `npm run build`

### Pages

| Page | Purpose |
|------|---------|
| `/` (index.astro) | Landing page with pricing and checkout modal |
| `/register` | Web-based KJ account registration (calls FastAPI `/api/v1/auth/register`) |
| `/register-success` | Post-registration instructions for connecting the desktop app |
| `/success` | Post-purchase success page; shows license key directly for coupon redemptions |
| `/cancel` | Stripe checkout cancelled |
| `/manage` | License management |

### Checkout Flow

```
User clicks pricing button
  → Modal opens (email + optional coupon field)
  → If coupon entered:
       POST /api/v1/coupon/redeem {code, email}
       → Success: redirect to /success?apiKey=AKJ-XXXX&coupon=1 (key shown on page)
       → Invalid/expired: show error, stay in modal
  → If no coupon (or after coupon error):
       POST /api/v1/checkout {tier, billingCycle, customerEmail}
       → Redirect to Stripe Checkout URL
       → Stripe webhook fulfills → license key emailed
```

---

## Deployment

### Desktop App
GitHub Actions (`.github/workflows/build-windows.yml`) builds on push to `master` or version tags. Release artifacts: `Auto-KJ-Windows-x64-Setup.exe` only (Inno Setup installer).

### Website
```bash
cd auto-kj-website-review
npm run build
scp -i ~/.ssh/id_rsa_autokj -r dist/* root@api.auto-kj.com:/var/www/auto-kj/dist/
```

### Backend Services
```bash
ssh -i ~/.ssh/id_rsa_autokj root@api.auto-kj.com
pm2 restart api              # FastAPI backend
pm2 restart auto-kj-license  # License server
```
