#!/bin/bash
# WallForge GNOME Shell Extension installer

EXTENSION_UUID="wallforge@wallforge.dev"
EXTENSION_DIR="$HOME/.local/share/gnome-shell/extensions/$EXTENSION_UUID"
SOURCE_DIR="$(dirname "$(readlink -f "$0")")/$EXTENSION_UUID"

echo "=== WallForge GNOME Shell Extension Installer ==="
echo ""

# Copy extension files
echo "Installing extension to: $EXTENSION_DIR"
mkdir -p "$EXTENSION_DIR"
cp "$SOURCE_DIR/metadata.json" "$EXTENSION_DIR/"
cp "$SOURCE_DIR/extension.js" "$EXTENSION_DIR/"

echo "Extension files installed."
echo ""

# Enable extension
echo "Enabling extension..."
gnome-extensions enable "$EXTENSION_UUID" 2>/dev/null

echo ""
echo "Installation complete!"
echo ""
echo "IMPORTANT: You need to restart GNOME Shell for the extension to take effect."
echo "  - On Wayland: Log out and log back in"  
echo "  - On X11: Press Alt+F2, type 'r', press Enter"
echo ""
echo "After restart, you can verify with:"
echo "  gnome-extensions info $EXTENSION_UUID"
echo ""
echo "To use with WallForge:"
echo "  wallforge run <id>                     # Start in window mode"
echo "  # The extension will auto-detect the window and clone it to background"
