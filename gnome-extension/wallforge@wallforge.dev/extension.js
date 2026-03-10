/**
 * WallForge GNOME Shell Extension
 *
 * Uses Clutter.Clone to inject a wallpaper renderer window's content
 * into the GNOME desktop background layer. Based on techniques from
 * gnome-ext-hanabi (jeffshee/gnome-ext-hanabi).
 *
 * How it works:
 * 1. WallForge launches linux-wallpaperengine in window mode
 * 2. The engine renders the wallpaper to a regular window
 * 3. This extension finds that window by its WM_CLASS
 * 4. Creates a Clutter.Clone of the WindowActor
 * 5. Inserts the clone into the background layer (behind all windows)
 * 6. Hides/minimizes the original window
 *
 * Compatible with GNOME Shell 46 (Ubuntu 24.04)
 */

import Meta from 'gi://Meta';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import Clutter from 'gi://Clutter';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';

const WALLFORGE_WM_CLASSES = [
    'linux-wallpaperengine',
    'wallpaperengine',
    'wallforge',
    'glfw',
];
const POLL_INTERVAL_MS = 2000;

export default class WallForgeExtension {
    constructor() {
        this._clone = null;
        this._sourceWindow = null;
        this._sourceActor = null;
        this._pollTimerId = null;
        this._windowCreatedId = null;
        this._windowRemovedId = null;
        this._monitorsChangedId = null;
        this._sizeChangedId1 = null;
        this._sizeChangedId2 = null;
        this._surfaceContainerSignalId = null;
        this._surfaceContainer = null;
        this._lastGeometryKey = '';
        this._geometryUpdateTimerId = null;
    }

    enable() {
        log('[WallForge] Enabling...');

        this._windowCreatedId = global.display.connect(
            'window-created',
            (_display, metaWindow) => this._onWindowCreated(metaWindow)
        );

        this._windowRemovedId = global.window_manager.connect(
            'destroy',
            (_wm, actor) => this._onWindowDestroyed(actor)
        );

        this._monitorsChangedId = Main.layoutManager.connect(
            'monitors-changed',
            () => this._onMonitorsChanged()
        );

        this._startPolling();
        this._findRendererWindow();

        log('[WallForge] Enabled');
    }

    disable() {
        log('[WallForge] Disabling...');

        this._stopPolling();

        if (this._windowCreatedId) {
            global.display.disconnect(this._windowCreatedId);
            this._windowCreatedId = null;
        }
        if (this._windowRemovedId) {
            global.window_manager.disconnect(this._windowRemovedId);
            this._windowRemovedId = null;
        }
        if (this._monitorsChangedId) {
            Main.layoutManager.disconnect(this._monitorsChangedId);
            this._monitorsChangedId = null;
        }

        this._detach();
        log('[WallForge] Disabled');
    }

    // ---- Polling ----

    _startPolling() {
        if (this._pollTimerId) return;
        this._pollTimerId = GLib.timeout_add(
            GLib.PRIORITY_DEFAULT,
            POLL_INTERVAL_MS,
            () => {
                this._findRendererWindow();
                if (this._sourceWindow) {
                    this._pollTimerId = null;
                    return GLib.SOURCE_REMOVE;
                }
                return GLib.SOURCE_CONTINUE;
            }
        );
    }

    _stopPolling() {
        if (this._pollTimerId) {
            GLib.source_remove(this._pollTimerId);
            this._pollTimerId = null;
        }
    }

    // ---- Window detection ----

    _findRendererWindow() {
        const windowActors = global.get_window_actors();
        for (const actor of windowActors) {
            const metaWindow = actor.get_meta_window();
            if (metaWindow && this._isWallForgeWindow(metaWindow)) {
                this._attach(metaWindow, actor);
                return;
            }
        }
    }

    _isWallForgeWindow(metaWindow) {
        if (!metaWindow) return false;
        const wmClass = (metaWindow.get_wm_class() || '').toLowerCase();
        const wmInstance = (metaWindow.get_wm_class_instance() || '').toLowerCase();
        const title = (metaWindow.get_title() || '').toLowerCase();

        for (const cls of WALLFORGE_WM_CLASSES) {
            if (wmClass.includes(cls) || wmInstance.includes(cls)) return true;
        }
        if (title.includes('wallpaperengine') || title.includes('wallforge')) return true;
        return false;
    }

    _onWindowCreated(metaWindow) {
        GLib.timeout_add(GLib.PRIORITY_DEFAULT, 500, () => {
            if (this._isWallForgeWindow(metaWindow)) {
                const actor = metaWindow.get_compositor_private();
                if (actor) this._attach(metaWindow, actor);
            }
            return GLib.SOURCE_REMOVE;
        });
    }

    _onWindowDestroyed(actor) {
        if (this._sourceActor === actor) {
            log('[WallForge] Renderer window destroyed');
            this._detach();
            this._startPolling();
        }
    }

    _onMonitorsChanged() {
        if (this._clone) this._updateCloneGeometry();
    }

    // ---- Attach / Detach ----

    _attach(metaWindow, actor) {
        if (this._sourceWindow === metaWindow) return;

        log(`[WallForge] Attaching: class='${metaWindow.get_wm_class()}' pid=${metaWindow.get_pid()}`);
        log(`[WallForge]   actor size: ${actor.width}x${actor.height}`);

        this._detach();
        this._sourceWindow = metaWindow;
        this._sourceActor = actor;

        // Clone the full WindowActor (like Hanabi)
        this._clone = new Clutter.Clone({
            source: actor,
            reactive: false,
        });

        // Insert into background layer
        const bgGroup = Main.layoutManager._backgroundGroup;
        if (!bgGroup) {
            log('[WallForge] ERROR: _backgroundGroup not found');
            return;
        }

        bgGroup.add_child(this._clone);
        bgGroup.set_child_above_sibling(this._clone, null);

        // Fit clone to primary monitor
        this._updateCloneGeometry();

        // Hide the source window
        this._hideSourceWindow(metaWindow, actor);

        // Track actor size changes (debounced)
        this._sizeChangedId1 = actor.connect('notify::width', () =>
            this._scheduleGeometryUpdate()
        );
        this._sizeChangedId2 = actor.connect('notify::height', () =>
            this._scheduleGeometryUpdate()
        );

        log(`[WallForge] Clone attached (bgGroup children: ${bgGroup.get_n_children()})`);
    }

    _detach() {
        if (this._geometryUpdateTimerId) {
            GLib.source_remove(this._geometryUpdateTimerId);
            this._geometryUpdateTimerId = null;
        }
        this._disconnectSizeSignals();
        this._disconnectSurfaceContainerSignal();
        this._showSourceWindow();
        this._removeClone();
        this._sourceWindow = null;
        this._sourceActor = null;
        this._lastGeometryKey = '';
    }

    // ---- Clone geometry ----

    _scheduleGeometryUpdate() {
        if (this._geometryUpdateTimerId) return; // Already scheduled
        this._geometryUpdateTimerId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 100, () => {
            this._geometryUpdateTimerId = null;
            this._updateCloneGeometry();
            return GLib.SOURCE_REMOVE;
        });
    }

    _updateCloneGeometry() {
        if (!this._clone || !this._sourceActor || !this._sourceWindow) return;
        const mon = Main.layoutManager.primaryMonitor;
        if (!mon) return;

        // The window's frame_rect is the actual content area (excluding decorations).
        // The actor includes compositor shadows around the content.
        const frameRect = this._sourceWindow.get_frame_rect();
        const contentW = frameRect.width;
        const contentH = frameRect.height;
        if (contentW <= 0 || contentH <= 0) return;

        // Shadow offset: where the content starts within the actor.
        // actor.x/y is the top-left of the full actor (including shadows),
        // frameRect.x/y is the top-left of the content area.
        const shadowL = frameRect.x - this._sourceActor.x;
        const shadowT = frameRect.y - this._sourceActor.y;

        // Scale content to fill monitor exactly (1:1 when content == monitor).
        // "Contain" scaling: fit entirely within the monitor, no cropping.
        const scaleX = mon.width / contentW;
        const scaleY = mon.height / contentH;
        const scale = Math.min(scaleX, scaleY);

        // Position the clone so the content area (after scaling) is centered
        // on the monitor. The content starts at (shadowL, shadowT) in the actor.
        const offsetX = (mon.width - contentW * scale) / 2 - shadowL * scale;
        const offsetY = (mon.height - contentH * scale) / 2 - shadowT * scale;

        this._clone.set_position(mon.x + offsetX, mon.y + offsetY);
        this._clone.set_size(
            this._sourceActor.width * scale,
            this._sourceActor.height * scale
        );

        // Clip to monitor bounds (hide shadows and any overflow)
        this._clone.set_clip(
            -offsetX,
            -offsetY,
            mon.width,
            mon.height
        );

        // Only log when geometry actually changes
        const geoKey = `${mon.width}x${mon.height}_${contentW}x${contentH}_${shadowL},${shadowT}_${this._sourceActor.width}x${this._sourceActor.height}`;
        if (geoKey !== this._lastGeometryKey) {
            this._lastGeometryKey = geoKey;
            log(`[WallForge] Geometry: monitor=${mon.width}x${mon.height} content=${contentW}x${contentH} shadow=${shadowL},${shadowT} actor=${this._sourceActor.width}x${this._sourceActor.height} scale=${scale.toFixed(3)}`);
        }
    }

    // ---- Window hiding ----

    _hideSourceWindow(metaWindow, actor) {
        // Step 1: Minimize (most important - keeps texture updates active)
        // IMPORTANT: Do NOT move offscreen as compositor stops rendering
        try {
            metaWindow.minimize();
            log('[WallForge] Window minimized');
        } catch (e) {
            log(`[WallForge] minimize() failed: ${e.message}`);
        }

        // Step 2: Skip taskbar (optional, may not exist in all GNOME versions)
        try {
            if (typeof metaWindow.set_skip_taskbar === 'function') {
                metaWindow.set_skip_taskbar(true);
                log('[WallForge] set_skip_taskbar applied');
            }
        } catch (_e) {
            // Non-critical
        }

        // Step 3: Fix surface container position (GNOME 45+/Wayland workaround)
        // When minimized, MetaSurfaceContainerActorWayland shifts surface position
        this._fixSurfaceContainerPosition(actor);
    }

    _fixSurfaceContainerPosition(actor) {
        const children = actor.get_children();
        const surfaceContainer = children.find(child => {
            const typeName = GObject.type_name(child);
            return (
                typeName === 'MetaSurfaceContainerActorWayland' ||
                typeName === 'MetaSurfaceActorWayland' ||
                typeName === 'MetaSurfaceActorX11'
            );
        });

        if (surfaceContainer) {
            surfaceContainer.set_position(0, 0);

            this._surfaceContainer = surfaceContainer;
            this._surfaceContainerSignalId = surfaceContainer.connect(
                'notify::position',
                () => surfaceContainer.set_position(0, 0)
            );

            log(`[WallForge] Surface workaround: ${GObject.type_name(surfaceContainer)}`);
        } else {
            const types = children.map(c => GObject.type_name(c)).join(', ');
            log(`[WallForge] No surface container. Actor children: [${types}]`);
        }
    }

    _showSourceWindow() {
        if (!this._sourceWindow) return;
        try {
            this._sourceWindow.unminimize();
        } catch (_e) {
            // Window may already be gone
        }
    }

    // ---- Cleanup ----

    _removeClone() {
        if (this._clone) {
            const parent = this._clone.get_parent();
            if (parent) parent.remove_child(this._clone);
            this._clone.destroy();
            this._clone = null;
        }
    }

    _disconnectSizeSignals() {
        if (this._sizeChangedId1 && this._sourceActor) {
            this._sourceActor.disconnect(this._sizeChangedId1);
        }
        this._sizeChangedId1 = null;
        if (this._sizeChangedId2 && this._sourceActor) {
            this._sourceActor.disconnect(this._sizeChangedId2);
        }
        this._sizeChangedId2 = null;
    }

    _disconnectSurfaceContainerSignal() {
        if (this._surfaceContainerSignalId && this._surfaceContainer) {
            this._surfaceContainer.disconnect(this._surfaceContainerSignalId);
        }
        this._surfaceContainerSignalId = null;
        this._surfaceContainer = null;
    }
}
