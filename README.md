# Spotify Connect

A simple C app to interact with the Spotify Web API using libcurl. 
It allows you to set up API tokens, refresh them, and retrieve your currently playing track directly from the command line. It is lightweight and works in any standard Linux environment with `bash` and `curl`.

---

## Features

- **Initial Setup:** Obtain and save Spotify API access and refresh tokens.
- **Refresh Token:** Automatically refresh your access token when expired.
- **Now Playing:** Retrieve the currently playing track, including:
  - Track name
  - Artist
  - Album
  - Playback status (Playing/Paused)
  - Track progress and duration

---

## Prerequisites

* System with `bash` and `curl` installed.
* A Spotify Developer account with a registered application:
* You will need the **Client ID** and **Client Secret**.

## Installation


1. Install OpenWrt packages
```bash
scp spotify-connect/packages/mips_24kc/base/*.ipk root@<OpenWrt-ip-address>:/tmp/
ssh root@<OpenWrt-ip-address>
opkg install -d ram /tmp/curl/*.ipk
export PATH=$PATH:/tmp/usr/bin
export LD_LIBRARY_PATH=/tmp/lib:/tmp/usr/lib
```

2. Edit the creds.h and add your Spotify Client ID and Client Secret:
```bash
CLIENT_ID="your_client_id"
CLIENT_SECRET="your_client_secret"
```

3. Build:

```bash
$ mkdir build && cd build
$ cmake ..
$ make
```

## Usage

1. Run the initial setup to obtain your tokens:
```bash
$ ./spotify-connect setup
```

* The script will print a Spotify authorization URL.
* Open the URL in a browser and log in.
* Copy the code parameter from the redirected URL and paste it into the terminal.
* Tokens will be saved to /tmp/spotify_token.

2. Get Currently Playing Track
```bash
$ ./spotify-connect now
```

3. Sample output:
```bash
Now Playing:
  Track:  Cure
  Artist: Marianne Beaulieu
  Album:  The Committee
  Status: Playing
  Time:   0:12 / 4:02
```
