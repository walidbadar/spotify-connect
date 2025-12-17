#!/bin/sh

# Spotify API Configuration
CLIENT_ID="9258522c44d14324969a8ef9dbe545f2"
CLIENT_SECRET="5f073e0b9c6a4aa1aa7643eecaf335df"

# Token file (will be stored in /tmp on OpenWrt)
TOKEN_FILE="/tmp/spotify_token"

# Function to refresh access token
refresh_token() {
    if [ ! -f "$TOKEN_FILE" ]; then
        echo "Error: Token file not found. Run initial setup first."
        exit 1
    fi
    
    REFRESH_TOKEN=$(grep "refresh_token" "$TOKEN_FILE" | cut -d'=' -f2)
    
    RESPONSE=$(curl -k -s -X POST "https://accounts.spotify.com/api/token" \
        -H "Content-Type: application/x-www-form-urlencoded" \
        -d "grant_type=refresh_token" \
        -d "refresh_token=$REFRESH_TOKEN" \
        -d "client_id=$CLIENT_ID" \
        -d "client_secret=$CLIENT_SECRET")
    
    ACCESS_TOKEN=$(echo "$RESPONSE" | grep -o '"access_token":"[^"]*' | cut -d'"' -f4)
    
    if [ -z "$ACCESS_TOKEN" ]; then
        echo "Error refreshing token"
        exit 1
    fi
    
    echo "access_token=$ACCESS_TOKEN" > "$TOKEN_FILE"
    echo "refresh_token=$REFRESH_TOKEN" >> "$TOKEN_FILE"
    echo "Token refreshed successfully"
}

# Function to extract JSON value (works with both compact and pretty JSON)
get_json_value() {
    local json="$1"
    local key="$2"
    echo "$json" | sed -n "s/.*\"$key\" *: *\"\([^\"]*\)\".*/\1/p" | head -1
}

# Function to get currently playing track
get_current_track() {
    if [ ! -f "$TOKEN_FILE" ]; then
        echo "Error: Token file not found. Run initial setup first."
        exit 1
    fi
    
    ACCESS_TOKEN=$(grep "access_token" "$TOKEN_FILE" | cut -d'=' -f2)
    
    RESPONSE=$(curl -k -s -X GET "https://api.spotify.com/v1/me/player/currently-playing" \
        -H "Authorization: Bearer $ACCESS_TOKEN")
    
    # Check if token expired
    if echo "$RESPONSE" | grep -q "token expired"; then
        echo "Token expired, refreshing..."
        refresh_token
        ACCESS_TOKEN=$(grep "access_token" "$TOKEN_FILE" | cut -d'=' -f2)
        RESPONSE=$(curl -k -s -X GET "https://api.spotify.com/v1/me/player/currently-playing" \
            -H "Authorization: Bearer $ACCESS_TOKEN")
    fi
    
    # Parse response
    if [ -z "$RESPONSE" ] || echo "$RESPONSE" | grep -q "\"item\" *: *null"; then
        echo "Nothing is currently playing"
        exit 0
    fi
    
    # Extract values by counting "name" field occurrences
    # Based on JSON structure:
    # 1st name: album artist
    # 2nd name: album name  
    # 3rd+ name: track artists
    # last name: track name
    
    # Get all "name" values in order
    ALL_NAMES=$(echo "$RESPONSE" | grep '"name"' | sed 's/.*"name" *: *"\([^"]*\)".*/\1/')
    
    # Album is 2nd name
    ALBUM=$(echo "$ALL_NAMES" | sed -n '2p')
    
    # Artist is 3rd name (first track artist)
    ARTIST=$(echo "$ALL_NAMES" | sed -n '3p')
    
    # Track name is the last occurrence
    TRACK_NAME=$(echo "$ALL_NAMES" | tail -1)
    
    # Get playing status
    IS_PLAYING=$(echo "$RESPONSE" | grep -o '"is_playing" *: *[^,}]*' | sed 's/.*: *\([a-z]*\).*/\1/')
    
    # Get progress and duration
    PROGRESS_MS=$(echo "$RESPONSE" | grep -o '"progress_ms" *: *[0-9]*' | grep -o '[0-9]*')
    DURATION_MS=$(echo "$RESPONSE" | awk '/"item"/{flag=1} flag && /"duration_ms"/{print; exit}' | grep -o '[0-9]*')
    
    # Convert to seconds
    if [ -n "$PROGRESS_MS" ] && [ -n "$DURATION_MS" ]; then
        PROGRESS_SEC=$((PROGRESS_MS / 1000))
        DURATION_SEC=$((DURATION_MS / 1000))
        PROGRESS_MIN=$((PROGRESS_SEC / 60))
        PROGRESS_SEC_REM=$((PROGRESS_SEC % 60))
        DURATION_MIN=$((DURATION_SEC / 60))
        DURATION_SEC_REM=$((DURATION_SEC % 60))
        TIME_INFO=$(printf "%d:%02d / %d:%02d" $PROGRESS_MIN $PROGRESS_SEC_REM $DURATION_MIN $DURATION_SEC_REM)
    fi
    
    echo "Now Playing:"
    echo "  Track:  $TRACK_NAME"
    echo "  Artist: $ARTIST"
    echo "  Album:  $ALBUM"
    echo "  Status: $([ "$IS_PLAYING" = "true" ] && echo "Playing" || echo "Paused")"
    [ -n "$TIME_INFO" ] && echo "  Time:   $TIME_INFO"
}

# Function for initial setup
setup() {
    echo "=== Spotify API Setup ==="
    echo ""
    echo "Step 1: Open this URL in your browser:"
    echo "https://accounts.spotify.com/authorize?client_id=$CLIENT_ID&response_type=code&redirect_uri=https://127.0.0.1:8888/callback&scope=user-read-currently-playing%20user-read-playback-state"
    echo ""
    echo "Step 2: After logging in, copy the 'code' parameter from the redirect URL"
    echo ""
    read -p "Enter the code: " CODE
    
    echo "Exchanging code for tokens..."
    RESPONSE=$(curl -k -s -X POST "https://accounts.spotify.com/api/token" \
        -H "Content-Type: application/x-www-form-urlencoded" \
        -d "grant_type=authorization_code" \
        -d "code=$CODE" \
        -d "redirect_uri=https://127.0.0.1:8888/callback" \
        -d "client_id=$CLIENT_ID" \
        -d "client_secret=$CLIENT_SECRET")
    
    ACCESS_TOKEN=$(echo "$RESPONSE" | grep -o '"access_token":"[^"]*' | cut -d'"' -f4)
    REFRESH_TOKEN=$(echo "$RESPONSE" | grep -o '"refresh_token":"[^"]*' | cut -d'"' -f4)
    
    if [ -z "$ACCESS_TOKEN" ] || [ -z "$REFRESH_TOKEN" ]; then
        echo "Error: Failed to get tokens"
        echo "Response: $RESPONSE"
        exit 1
    fi
    
    echo "access_token=$ACCESS_TOKEN" > "$TOKEN_FILE"
    echo "refresh_token=$REFRESH_TOKEN" >> "$TOKEN_FILE"
    
    echo "Setup complete! Tokens saved to $TOKEN_FILE"
}

# Main
case "$1" in
    setup)
        setup
        ;;
    now)
        get_current_track
        ;;
    refresh)
        refresh_token
        ;;
    *)
        echo "Usage: $0 {setup|now|refresh}"
        echo ""
        echo "  setup   - Initial setup (run once)"
        echo "  now - Get currently playing track"
        echo "  refresh - Manually refresh access token"
        exit 1
        ;;
esac
