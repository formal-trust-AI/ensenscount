#!/bin/bash

# SSH Key Setup Script - Reads configuration from experiment_config.yaml
# Usage: ./add_ssh_key.sh [email]

# Set script directory and config path
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_FILE="$SCRIPT_DIR/../../config/experiment_config.yaml"

# Check if config file exists
if [ ! -f "$CONFIG_FILE" ]; then
    echo "Error: Configuration file not found at $CONFIG_FILE"
    echo "Please ensure experiment_config.yaml exists in the config directory"
    exit 1
fi

# Function to extract value from YAML (simple grep-based parser)
get_config_value() {
    local key="$1"
    grep "^$key:" "$CONFIG_FILE" | sed 's/.*: *//' | tr -d '"'"'"
}

# Read configuration values
USER=$(get_config_value "user")
SERVER=$(get_config_value "server")
HOST=$(get_config_value "host")

# Validate required values
if [ -z "$USER" ] || [ -z "$SERVER" ] || [ -z "$HOST" ]; then
    echo "Error: Missing required configuration values"
    echo "Required: user, server, host in $CONFIG_FILE"
    exit 1
fi

# Get email from command line or prompt
EMAIL="$1"
if [ -z "$EMAIL" ]; then
    read -p "Enter your email for SSH key: " EMAIL
fi

if [ -z "$EMAIL" ]; then
    echo "Error: Email is required for SSH key generation"
    exit 1
fi

echo "=== SSH Key Setup ==="
echo "User: $USER"
echo "Jump Server: $SERVER"
echo "Target Host: $HOST"
echo "Email: $EMAIL"
echo

# Generate SSH key
echo "Generating SSH key..."
ssh-keygen -t ed25519 -C "$EMAIL" -f ~/.ssh/id_ed25519

# Setup SSH config
echo "Configuring SSH..."
touch ~/.ssh/config && chmod 600 ~/.ssh/config

cat >> ~/.ssh/config << EOF

# Jump server configuration (added by xgboost-counting setup)
Host jumpserver
    HostName $SERVER
    User $USER
    IdentityFile ~/.ssh/id_ed25519
    ForwardAgent yes

# Target server through jump server (added by xgboost-counting setup)
Host targetserver
    HostName $HOST
    User $USER
    IdentityFile ~/.ssh/id_ed25519
    ProxyJump jumpserver
    ForwardAgent yes

EOF

echo "✓ SSH configuration added"

# Copy keys to servers
echo "Copying SSH key to jump server..."
ssh-copy-id -i ~/.ssh/id_ed25519.pub "$USER@$SERVER"

echo "Copying SSH key to target server..."
ssh-copy-id -i ~/.ssh/id_ed25519.pub -o ProxyJump="$USER@$SERVER" "$USER@$HOST"

echo
echo "✓ SSH setup complete!"
echo "✓ You can now run experiments using: ./xcount_experiments"