-- WallForge Library Database Schema
-- SQLite3

-- 壁紙メタデータテーブル
CREATE TABLE IF NOT EXISTS wallpapers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    steam_id TEXT UNIQUE,                    -- Steam Workshop ID (nullable)
    name TEXT NOT NULL,
    type TEXT NOT NULL CHECK(type IN ('scene', 'video', 'web')),
    path TEXT NOT NULL UNIQUE,               -- 壁紙ディレクトリへのパス
    preview_path TEXT,                       -- プレビュー画像パス
    description TEXT DEFAULT '',
    author TEXT DEFAULT '',
    rating INTEGER DEFAULT 0 CHECK(rating BETWEEN 0 AND 5),
    resolution_width INTEGER DEFAULT 0,
    resolution_height INTEGER DEFAULT 0,
    file_size_bytes INTEGER DEFAULT 0,
    added_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_used_at TIMESTAMP,
    use_count INTEGER DEFAULT 0,
    is_favorite BOOLEAN DEFAULT 0
);

-- タグテーブル
CREATE TABLE IF NOT EXISTS tags (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE
);

-- 壁紙-タグ中間テーブル
CREATE TABLE IF NOT EXISTS wallpaper_tags (
    wallpaper_id INTEGER NOT NULL,
    tag_id INTEGER NOT NULL,
    PRIMARY KEY (wallpaper_id, tag_id),
    FOREIGN KEY (wallpaper_id) REFERENCES wallpapers(id) ON DELETE CASCADE,
    FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE
);

-- プレイリストテーブル
CREATE TABLE IF NOT EXISTS playlists (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    description TEXT DEFAULT '',
    is_shuffle BOOLEAN DEFAULT 0,
    transition_type TEXT DEFAULT 'crossfade' CHECK(transition_type IN ('none', 'fade', 'crossfade', 'slide')),
    transition_duration_ms INTEGER DEFAULT 1000,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- プレイリスト-壁紙中間テーブル
CREATE TABLE IF NOT EXISTS playlist_wallpapers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    playlist_id INTEGER NOT NULL,
    wallpaper_id INTEGER NOT NULL,
    position INTEGER NOT NULL,
    duration_seconds INTEGER DEFAULT 300,    -- この壁紙の表示時間（秒）
    FOREIGN KEY (playlist_id) REFERENCES playlists(id) ON DELETE CASCADE,
    FOREIGN KEY (wallpaper_id) REFERENCES wallpapers(id) ON DELETE CASCADE
);

-- スケジュールトリガーテーブル
CREATE TABLE IF NOT EXISTS schedule_triggers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    playlist_id INTEGER NOT NULL,
    trigger_type TEXT NOT NULL CHECK(trigger_type IN ('interval', 'time_of_day', 'day_of_week', 'on_login', 'on_app_launch', 'on_app_close', 'on_resume')),
    trigger_value TEXT NOT NULL,              -- JSONエンコードされたトリガー設定
    is_enabled BOOLEAN DEFAULT 1,
    FOREIGN KEY (playlist_id) REFERENCES playlists(id) ON DELETE CASCADE
);

-- パフォーマンスプロファイルテーブル
CREATE TABLE IF NOT EXISTS performance_profiles (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE,
    max_fps INTEGER DEFAULT 60,
    effects_enabled BOOLEAN DEFAULT 1,
    audio_enabled BOOLEAN DEFAULT 1,
    is_builtin BOOLEAN DEFAULT 0
);

-- アプリルールテーブル
CREATE TABLE IF NOT EXISTS app_rules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    app_name TEXT NOT NULL,                  -- WM_CLASS or process name
    condition TEXT NOT NULL CHECK(condition IN ('fullscreen', 'focused', 'running')),
    action TEXT NOT NULL CHECK(action IN ('pause', 'lower_fps', 'mute', 'change_profile')),
    action_value TEXT,                       -- action-specific value (e.g., profile name)
    is_enabled BOOLEAN DEFAULT 1
);

-- 設定テーブル（キーバリュー）
CREATE TABLE IF NOT EXISTS settings (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL
);

-- デフォルトパフォーマンスプロファイル挿入
INSERT OR IGNORE INTO performance_profiles (name, max_fps, effects_enabled, audio_enabled, is_builtin)
VALUES 
    ('high', 60, 1, 1, 1),
    ('balanced', 30, 1, 1, 1),
    ('low', 15, 0, 0, 1);

-- デフォルト設定の挿入
INSERT OR IGNORE INTO settings (key, value)
VALUES
    ('default_profile', 'balanced'),
    ('battery_auto_switch', 'true'),
    ('battery_profile', 'low'),
    ('battery_critical_threshold', '15'),
    ('battery_critical_action', 'pause'),
    ('display_backend', 'x11');

-- インデックス
CREATE INDEX IF NOT EXISTS idx_wallpapers_type ON wallpapers(type);
CREATE INDEX IF NOT EXISTS idx_wallpapers_name ON wallpapers(name);
CREATE INDEX IF NOT EXISTS idx_wallpapers_rating ON wallpapers(rating);
CREATE INDEX IF NOT EXISTS idx_wallpapers_last_used ON wallpapers(last_used_at);
CREATE INDEX IF NOT EXISTS idx_playlist_wallpapers_position ON playlist_wallpapers(playlist_id, position);
CREATE INDEX IF NOT EXISTS idx_app_rules_app ON app_rules(app_name);
