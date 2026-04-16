<div align="center">

# 🎤 Auto-KJ

### Professional Karaoke Hosting with Intelligent Fairness

*Built by a KJ with 25 years in the trenches.*

[![License: GPL-3.0](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-lightgrey.svg)](https://github.com/pkircher29/auto-kj/releases)
[![Price](https://img.shields.io/badge/price-FREE-success.svg)](https://auto-kj.com)

[Download](https://github.com/pkircher29/auto-kj/releases) · [Website](https://auto-kj.com) · [License Server](https://api.auto-kj.com) · [Report Bug](https://github.com/pkircher29/auto-kj/issues)

</div>

---

## Why Auto-KJ?

Every KJ knows the problem: singers game the rotation. They sign up under different names, request the same song twice, or just muscle their way to the front. Existing software doesn't stop it. You have to manually police the list all night.

**Auto-KJ fixes this.**

The **Rotation Fairness Engine** automatically enforces fair turn order, detects duplicate singers, blocks repeat songs, and tracks rounds — so you can focus on hosting, not babysitting the list.

## ⚖️ Rotation Fairness Engine

The #1 reason KJs switch to Auto-KJ. No other karaoke software enforces fairness.

| Feature | What It Does |
|---------|-------------|
| **Round-Based Turn Tracking** | Tracks when every singer completes their turn. New round starts when the last singer performs. Nobody gets a second turn before everyone gets their first. |
| **Duplicate Turn Prevention** | Warns you instantly when a singer already performed this round — "4 singers still waiting." You can still allow it, but you'll never accidentally let someone cut the line. |
| **Smart Song Blocking** | Once a song is performed tonight, it's blocked — even different versions. "Don't Stop Believin' [Karaoke Version]" and "Don't Stop Believin' (In the Style of Journey)"? Same song. The engine knows. |
| **Singer Alias Detection** | "Mike," "Michael," and "Mike S." — all the same singer. The alias system resolves alternate names to a single identity. No gaming the rotation. |
| **DJ Override** | Fairness rules are enforced, not imposed. Override any block with a click — birthday requests, encores, whatever. Overrides are tracked per-singer, per-song. You're still the boss. |

## 🎛️ Features

- **Every Format You Need** — CDG, MP3+G, MP4, mkv. Play them all with key changer, tempo control, and EQ built in.
- **Built-in Streaming Controls** — Spotify, YouTube Music, and iTunes control right from the UI. Manage break music without switching apps.
- **Break Music Auto-Fade** — Break music fades out when the singer starts, fades back in when they finish. No dead air.
- **Automatic Backups** — Timestamped database backups with automatic pruning. Your show data survives hardware failures.
- **Singer PWA** — Singers scan a QR code, request songs from their phone, see the rotation, check in for the night. No app to install. *(Coming soon)*
- **Now-Playing Push** — Singers get notified when they're up next. No more "when's my turn??" interruptions. *(Coming soon)*
- **SingLink** — QR-based social graph. Connect with singers, discover duet partners, request directly to the queue. *(Coming soon)*

## 💰 Pricing

| Tier | Price | What You Get |
|------|-------|-------------|
| **Desktop App** | Free, forever | Full hosting, fairness engine, streaming controls, all formats. No limits. No catch. |
| **Advanced** | $14.99/mo or $129.99/yr | Singer PWA, remote requests, rotation display, now-playing push, check-ins, 3 venues, gig history, cloud sync |
| **Unlimited** | $29.99/mo or $249.99/yr | Everything in Advanced + unlimited venues, team accounts, analytics, priority support, early access |
| **Lifetime** | $599.99 once | Everything in Unlimited, forever. All future features, all future updates. Extremely limited offer. |

The desktop app is **free and open source** under GPL-3.0. Server features (Singer PWA, remote requests, etc.) are paid add-ons.

## 🚀 Getting Started

### Download

Grab the latest release for your platform:

- **Windows:** [Auto-KJ-Setup.exe](https://github.com/pkircher29/auto-kj/releases)
- **macOS:** [Auto-KJ.dmg](https://github.com/pkircher29/auto-kj/releases)
- **Linux:** [Auto-KJ.AppImage](https://github.com/pkircher29/auto-kj/releases)

Or [build from source](#building-from-source).

### First Show

1. Open Auto-KJ
2. Add your karaoke library (CDG, MP3+G, MP4, mkv folders)
3. Create a rotation — add singers
4. Start the show. The Fairness Engine handles the rest.

No account needed. No trial period. No watermarks.

## 🔧 Building from Source

Auto-KJ is built on Qt 5 and GStreamer. You'll need:

- Qt 5.15+ (Core, Gui, Widgets, Network, Sql, Multimedia, MultimediaWidgets)
- GStreamer 1.x (core, plugins-good, plugins-bad)
- CMake 3.16+
- C++17 compatible compiler

```bash
git clone https://github.com/pkircher29/auto-kj.git
cd auto-kj
mkdir build && cd build
cmake ..
make -j$(nproc)
```

See [BUILDING.md](BUILDING.md) for platform-specific instructions.

## 🗺️ Roadmap

- [x] Rotation Fairness Engine — round tracking, duplicate prevention, alias detection
- [x] Streaming controls — Spotify, YouTube Music, iTunes
- [x] Automatic backups
- [ ] **Singer PWA & Remote Requests** — QR code → phone → song request, rotation display, check-ins
- [ ] **AI Karaoke Track Generation** — generate real karaoke tracks from any MP3
- [ ] **SingLink** — QR social graph, duet discovery, direct-to-queue requests
- [ ] **Band** — group singing via SingLink, up to 4 singers, approval flow
- [ ] **Offline Resilience** — cached license validation, local P2P server when internet is down
- [ ] **Multi-KJ Teams** — shared venues, coordinated schedules

## 🤝 Contributing

Auto-KJ is open source because karaoke software should be. PRs welcome:

1. Fork the repo
2. Create a feature branch (`git checkout -b feature/my-feature`)
3. Commit your changes (`git commit -am 'Add my feature'`)
4. Push to the branch (`git push origin feature/my-feature`)
5. Open a Pull Request

See [CONTRIBUTING.md](CONTRIBUTING.md) for coding standards and guidelines.

## 📜 License

Auto-KJ is licensed under the [GNU General Public License v3.0](https://www.gnu.org/licenses/gpl-3.0.en.html). 

The desktop app is free. The source is open. It stays that way.

Server-side features (Singer PWA, remote requests, etc.) are offered as paid services under separate terms. See [auto-kj.com/license](https://auto-kj.com/license) for details.

## 🎤 About

Auto-KJ was born from Kircher Entertainment — 25 years running karaoke shows in the Philadelphia area. Every feature exists because a real KJ needed it on a real stage.

This isn't software built in a vacuum by developers who've never hosted a show. This is software built by someone who's been in the trenches, dealt with the drunks, managed the rotation, and knows exactly what a KJ needs at 11 PM on a Friday when the room is packed and the requests won't stop.

---

<div align="center">

**[auto-kj.com](https://auto-kj.com)** · Built with ❤️ for KJs everywhere

</div>