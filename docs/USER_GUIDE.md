# Auto-KJ User Guide

This guide is for KJs installing Auto-KJ, running a local show, and connecting the optional Auto-KJ server features for singer phone requests, venue tools, and web displays.

## What Auto-KJ Does

Auto-KJ is professional karaoke hosting software built around a Rotation Fairness Engine. The desktop app runs the show: karaoke playback, singer rotation, song queues, break music, search, history, key changes, tempo control, and KJ override tools.

The desktop app is free and open source. An Auto-KJ account is only needed when you want the hosted server features, such as phone requests, check-ins, venue management, web dashboard controls, cloud-backed show state, and kiosk or singer-facing web views.

## Install Auto-KJ

### Windows

1. Go to the Auto-KJ releases page: https://github.com/pkircher29/auto-kj/releases
2. Download the latest Windows installer artifact or release asset.
3. Run the installer.
4. Launch Auto-KJ from the Start menu or desktop shortcut.

If Windows SmartScreen warns that the app is from an unknown publisher, choose the option to view more details and run it only if the file came from the official GitHub releases page.

### Portable Windows Build

Some releases also include a portable ZIP bundle.

1. Download the portable Windows ZIP from the releases page.
2. Extract it to a folder you control, such as `C:\Auto-KJ`.
3. Run `auto-kj.exe` from the extracted folder.

Use the installer for normal users. Use the portable ZIP when you want to test a build without changing the machine-wide install.

### macOS and Linux

Auto-KJ is built with Qt 5 and GStreamer. Release assets may vary by platform. If a packaged build is not available for your platform, build from source using the platform-specific instructions in [BUILD.md](../BUILD.md).

### Build From Source

Use source builds when you are developing, testing a branch, or packaging for an unsupported platform.

1. Install Qt 5.15 or newer, GStreamer 1.x, CMake 3.16 or newer, and a C++17 compiler.
2. Clone the repository:

   ```bash
   git clone https://github.com/pkircher29/auto-kj.git
   cd auto-kj
   ```

3. Follow [BUILD.md](../BUILD.md) for Windows, macOS, or Linux build commands.

## First Launch Setup

### Add Your Karaoke Library

1. Open Auto-KJ.
2. Open `Tools > Settings`.
3. Go to the `Karaoke` settings page.
4. Use `Add Directory` to add the folders that contain your karaoke files.
5. Save settings.
6. Open `Tools > Manage Karaoke DB`.
7. Update or scan the database so Auto-KJ can index your songs.

Auto-KJ is designed for common karaoke media such as CDG/MP3+G, MP4, MKV, and other formats supported by the installed media stack.

### Add Singers

1. Stay on the `Karaoke` tab.
2. Click `Add singer` to add a singer to the rotation.
3. Add regular singers when you want Auto-KJ to remember them as regulars.
4. Select a singer in the rotation before adding songs to that singer's queue.

The fairness engine watches the rotation and warns when a singer may be getting an unfair duplicate turn or when a song was already performed during the show.

### Add and Play Songs

1. Search the karaoke database by artist, title, or all fields.
2. Select the singer in the rotation.
3. Select a song in the database.
4. Add it to the singer's queue, or play it directly if that is how you run your show.
5. Use the playback controls for play, pause, stop, volume, channel selection, key change, and tempo.

### Use Break Music

1. Go to the `Break Music` tab.
2. Open `Tools > Manage Break DB` if you need to build or refresh the break music database.
3. Add tracks to the break music playlist.
4. Start break music between karaoke tracks.

Auto-KJ can fade break music around karaoke playback so you do not have dead air between singers.

## Daily Show Workflow

1. Start Auto-KJ before the show.
2. Confirm your karaoke database is current.
3. Add early singers to the rotation.
4. Add songs to each singer's queue.
5. Keep the rotation moving from top to bottom.
6. Let the fairness warnings guide you when a singer already performed, is using another name, or requests a repeated song.
7. Use KJ override when the room needs an exception.
8. End the show by stopping playback, clearing or saving the rotation as needed, and closing Auto-KJ.

## Auto-KJ Accounts and Plans

### What Is Free

The desktop application is free. You can host karaoke locally with:

- Karaoke playback
- Rotation management
- Song search and local database tools
- Fairness engine warnings and KJ override
- Break music
- Streaming controls
- Local show workflow tools

No account is required for normal desktop hosting.

### What an Account Adds

An Auto-KJ account connects the desktop app to the hosted Auto-KJ server. This unlocks the server-backed features that make the show easier for singers and reduce interruption at the KJ station:

- Singer phone requests through the Singer PWA
- Venue QR code for singers to scan
- Remote request queue in Auto-KJ
- Singer check-ins
- Rotation display for singers
- Now-playing and up-next updates
- Venue and showtime management
- DJ profile and venue-aware show state
- Web dashboard or tablet commands wired back into the desktop app
- Kiosk-style display features, when available for your plan
- Plan details loaded directly in the Settings dialog

### Plan Summary

Current public plan information is shown on https://auto-kj.com. At the time this guide was written, the public site listed:

| Plan | Public price | Best for |
|------|--------------|----------|
| Desktop App | Free | KJs who only need the local desktop app |
| Advanced | $14.99/month or $129.99/year | KJs who want singer phone requests, check-ins, rotation display, venue tools, gig history, and cloud sync for up to 3 venues |
| Unlimited | $29.99/month or $249.99/year | KJs running more venues who want unlimited venues, deeper analytics, priority support, and early access |
| Lifetime | $599.99 once | KJs who want Unlimited features without a recurring subscription, while lifetime slots are available |

Prices and plan contents can change. The checkout page on https://auto-kj.com is the source of truth before purchase.

## Sign Up for an Account

You can create an account from the website or from inside the desktop app.

### Sign Up on the Website

1. Go to https://auto-kj.com.
2. Choose the plan that matches your show.
3. Enter the email address you want tied to the KJ account.
4. Complete checkout if you selected a paid plan.
5. Save the license or account email you receive after checkout.
6. Open Auto-KJ and connect the desktop app using the steps in the next section.

### Sign Up in the Desktop App

1. Open `Tools > Settings`.
2. Go to the `Network` tab.
3. Check `Use request server`.
4. Leave `Server URL` as `https://api.auto-kj.com` unless you are self-hosting.
5. Click `No account? Create one...`.
6. Enter your display name, email, password, and password confirmation.
7. Use a password with at least 8 characters, including uppercase, lowercase, and a number.
8. Create the account.
9. Auto-KJ fills the email and password fields after a successful registration.
10. Choose or purchase a plan on https://auto-kj.com if the account does not already have the server features you need.

## Enable Account Features in Auto-KJ

### Connect the Desktop App

1. Open `Tools > Settings`.
2. Go to `Network`.
3. Check `Use request server`.
4. Set `Server URL` to `https://api.auto-kj.com` for the hosted Auto-KJ service.
5. Enter your Auto-KJ account email.
6. Enter your Auto-KJ account password.
7. Enter your API key or license key if your account email included one and the app requests it.
8. Click `Test`.
9. Confirm that the test reports a successful request server connection.
10. Save settings.

When the test succeeds, Auto-KJ signs in, loads plan details, and enables the venue toolbar in the main window.

### Create or Select a Venue

After the request server is enabled, the main window shows a `Venue` toolbar.

1. Use the `Venue` dropdown to select the venue for tonight's show.
2. Click the refresh button if the venue list is stale.
3. Click the manage venue button, or open `Tools > Manage Venues & Gigs...`, to add or edit venues.
4. Add showtimes for venues if you want the app to track recurring or one-time gigs.

If you have no venues yet, Auto-KJ prompts you to create the first venue when the server connection succeeds.

### Start Accepting Requests

1. Select the active venue.
2. Confirm the correct DJ profile in the toolbar.
3. Check `Accept Requests`.
4. Open `Tools > Incoming Requests` or click the `Requests` button.
5. Leave the Incoming Requests dialog open during the show if you want to watch requests arrive live.

Use `End Show` when the venue's active show is finished. Ending the show also helps prevent singers from sending requests to an old event.

### Show the Venue QR Code

1. Open `Tools > Show Venue QR`, or use the venue QR toolbar button.
2. Display or print the QR code where singers can scan it.
3. Singers scan the code, open the Singer PWA in their phone browser, and request songs.

No app store installation is required for singers.

### Upload or Refresh the Remote Songbook

Singers can only request songs that the server knows about.

1. Open `Incoming Requests`.
2. Click `Update Remote DB`.
3. Wait for the update to finish.
4. Reopen the singer page or refresh the singer phone browser if old song data is still showing.

Run this again after you add a large batch of new karaoke files.

### Handle Incoming Requests

1. Open `Incoming Requests`.
2. Select a request.
3. Match it to an existing singer or choose a new singer.
4. Confirm any co-singers.
5. Choose where to add the singer in the rotation.
6. Click `Add Song`.

Auto-KJ can remove the request from the request queue after adding it to the rotation, depending on your settings.

### Use Singer Check-ins

1. Open `Tools > Singer Check-ins...`.
2. Click a pending singer.
3. Click `Add to Rotation`.

Check-ins help the KJ know who is actually present before adding them to the live rotation.

### Tune Request Rules

Open `Tools > Settings > Network` and review the server-related options available in your build and plan, such as:

- Maximum singers per request
- One song per singer limits
- Repeat-song blocking
- Line jump or fairness priority controls
- No-show timeout
- KJ buffer
- Venue geofence and radius

Use stricter rules for busy shows and looser rules for private parties.

## Account Management

### Forgot Password

1. Open `Tools > Settings > Network`.
2. Click `Forgot password?`.
3. Enter your account email.
4. Follow the reset instructions.
5. Paste the reset token if the app asks for it.
6. Save the new password.
7. Click `Test` again in Network settings.

### Change Password

1. Open `Tools > Settings`.
2. Use the `Change Password` control in the settings dialog.
3. Save the new password.
4. Update the password field in `Network`.
5. Click `Test` to refresh the saved login token.

### Manage Billing

Use https://auto-kj.com to select or change a plan. If your plan changes while Auto-KJ is open, return to `Tools > Settings > Network` and click `Test` so the desktop app reloads plan details.

## Troubleshooting

### The Request Server Test Fails

Check these first:

- `Use request server` is checked.
- `Server URL` is `https://api.auto-kj.com` for the hosted service.
- Email and password are correct.
- Your account has a plan that includes the server features you are trying to use.
- Your internet connection is working.
- The system clock on the computer is correct.

Do not enable `Ignore HTTPS certificate errors` for the hosted Auto-KJ service. That setting is only for self-hosted or test servers with self-signed certificates.

### Plan Details Do Not Show

1. Reopen `Tools > Settings > Network`.
2. Confirm email and password.
3. Click `Test`.
4. Restart Auto-KJ if the plan label still does not refresh.

### Singers Cannot See Songs

1. Confirm the local karaoke database has songs.
2. Open `Incoming Requests`.
3. Click `Update Remote DB`.
4. Wait for the upload/update to finish.
5. Ask the singer to refresh their phone browser.

### Singers Cannot Submit Requests

1. Confirm the correct venue is selected.
2. Confirm `Accept Requests` is checked.
3. Confirm there is an active show for the venue.
4. Confirm the singer scanned the current venue QR code.
5. Check the Incoming Requests dialog for new requests.

### Wrong Venue Opens From QR Code

1. Select the correct venue in the toolbar.
2. Refresh the venue list.
3. Open `Show Venue QR` again.
4. Replace any old printed or displayed QR code.

### Requests Arrive But Do Not Go Into the Rotation

Requests are not automatically forced into the live show. The KJ chooses when to add them.

1. Open `Incoming Requests`.
2. Select the request.
3. Match it to a singer.
4. Click `Add Song`.

This keeps the KJ in control of fairness, duplicates, co-singers, and room-specific exceptions.

## Self-Hosting Notes

Most users should use `https://api.auto-kj.com`.

Advanced users may self-host compatible server components. In that case, set `Server URL` to your own HTTPS endpoint, confirm WebSocket support, and only use `Ignore HTTPS certificate errors` for trusted test environments. Self-hosting is for technical operators who are comfortable maintaining the API, database, TLS certificates, and web apps.

## Release Checklist for KJs

Before a paid or public show:

1. Install the latest stable Auto-KJ build.
2. Confirm playback works with your media files.
3. Confirm your karaoke database is current.
4. Confirm break music works.
5. If using server features, click `Test` in Network settings.
6. Confirm the correct venue is selected.
7. Upload or refresh the remote songbook.
8. Display the current venue QR code.
9. Send one test request from a phone.
10. Add that request to the rotation and verify it appears correctly.

That dry run catches almost every setup issue before singers are in front of you.
