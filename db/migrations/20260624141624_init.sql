-- migrate:up
CREATE TABLE auth_user (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    username       TEXT    NOT NULL UNIQUE,
    password       TEXT    NOT NULL,
    name           TEXT    NOT NULL DEFAULT '',
    email          TEXT    NOT NULL DEFAULT '',
    last_logged_in TEXT,
    created_at     TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE auth_session (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id    INTEGER NOT NULL REFERENCES auth_user(id),
    token      TEXT    NOT NULL UNIQUE,
    created_at TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    expires_at TEXT    NOT NULL
);

CREATE TABLE auth_app_provider (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    client_id     TEXT    NOT NULL,
    client_secret TEXT    NOT NULL,
    tenant_id     TEXT    NOT NULL,
    domain        TEXT    NOT NULL UNIQUE,
    created_at    TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE helpdesk_ticket_status (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    name         TEXT    NOT NULL UNIQUE,
    display_name TEXT    NOT NULL,
    trait        TEXT    NOT NULL CHECK (trait IN ('open', 'in_progress', 'closed'))
);

CREATE TABLE helpdesk_priority (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    name         TEXT    NOT NULL UNIQUE,
    display_name TEXT    NOT NULL
);

CREATE TABLE helpdesk_profile (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id    INTEGER NOT NULL UNIQUE REFERENCES auth_user(id),
    role       TEXT    NOT NULL CHECK (role IN ('system', 'administrator', 'supervisor', 'technician', 'requester')),
    created_at TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE helpdesk_ticket (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    requester_id   INTEGER NOT NULL REFERENCES helpdesk_profile(id),
    assigned_to_id INTEGER          REFERENCES helpdesk_profile(id),
    priority_id    INTEGER NOT NULL REFERENCES helpdesk_priority(id) ON DELETE RESTRICT ON UPDATE RESTRICT,
    status_id      INTEGER NOT NULL REFERENCES helpdesk_ticket_status(id) ON DELETE RESTRICT ON UPDATE RESTRICT,
    body           TEXT    NOT NULL,
    due_date       TEXT,
    created_at     TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at     TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX ticket_requester_created_at_idx ON helpdesk_ticket (requester_id, created_at DESC);
CREATE INDEX ticket_created_at_idx ON helpdesk_ticket (created_at DESC);
CREATE INDEX ticket_body_idx ON helpdesk_ticket (body);
CREATE INDEX ticket_status_created_at_idx ON helpdesk_ticket (status_id, created_at DESC);
CREATE INDEX ticket_priority_created_at_idx ON helpdesk_ticket (priority_id, created_at DESC);
CREATE INDEX ticket_assigned_to_created_at_idx ON helpdesk_ticket (assigned_to_id, created_at DESC);
CREATE INDEX ticket_due_date_idx ON helpdesk_ticket (due_date);

CREATE TRIGGER set_ticket_updated_at
AFTER UPDATE ON helpdesk_ticket
FOR EACH ROW
WHEN NEW.updated_at = OLD.updated_at
BEGIN
    UPDATE helpdesk_ticket SET updated_at = CURRENT_TIMESTAMP WHERE id = NEW.id;
END;

CREATE TABLE helpdesk_ticket_activity (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    ticket_id  INTEGER NOT NULL REFERENCES helpdesk_ticket(id),
    profile_id INTEGER NOT NULL REFERENCES helpdesk_profile(id),
    kind       TEXT    NOT NULL CHECK (kind IN ('message', 'status_changed', 'assigned_changed', 'department_changed', 'priority_changed', 'due_date_changed')),
    body       TEXT    NOT NULL DEFAULT '',
    metadata   TEXT    NOT NULL DEFAULT '{}',
    created_at TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX ticket_activity_body_idx ON helpdesk_ticket_activity (body);
CREATE INDEX ticket_activity_ticket_created_at_idx ON helpdesk_ticket_activity (ticket_id, created_at DESC, id DESC);

CREATE TABLE helpdesk_ticket_activity_attachment (
    id                 INTEGER PRIMARY KEY AUTOINCREMENT,
    ticket_activity_id INTEGER NOT NULL REFERENCES helpdesk_ticket_activity(id),
    file_path          TEXT    NOT NULL,
    file_name          TEXT    NOT NULL,
    file_size          INTEGER NOT NULL,
    mime_type          TEXT    NOT NULL,
    created_at         TEXT    NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX ticket_activity_attachment_ticket_activity_id_idx
ON helpdesk_ticket_activity_attachment (ticket_activity_id);

CREATE TABLE helpdesk_setting (
    name                           TEXT    PRIMARY KEY DEFAULT 'default',
    default_status_id              INTEGER NOT NULL REFERENCES helpdesk_ticket_status(id) ON DELETE RESTRICT ON UPDATE RESTRICT,
    default_assigned_to_id         INTEGER NOT NULL REFERENCES helpdesk_profile(id),
    default_ticket_priority_id     INTEGER NOT NULL REFERENCES helpdesk_priority(id) ON DELETE RESTRICT ON UPDATE RESTRICT,
	   assigned_status_id             INTEGER NOT NULL REFERENCES helpdesk_ticket_status(id) ON DELETE RESTRICT ON UPDATE RESTRICT,
    system_profile_id              INTEGER NOT NULL REFERENCES helpdesk_profile(id),
    ticket_body_maxlength          INTEGER NOT NULL DEFAULT 750,
    ticket_activity_body_maxlength INTEGER NOT NULL DEFAULT 500,
    ticket_due_delta               INTEGER NOT NULL DEFAULT 172800
);

-- migrate:down
DROP TABLE IF EXISTS helpdesk_setting;
DROP INDEX IF EXISTS ticket_activity_attachment_ticket_activity_id_idx;
DROP TABLE IF EXISTS helpdesk_ticket_activity_attachment;
DROP INDEX IF EXISTS ticket_activity_ticket_created_at_idx;
DROP INDEX IF EXISTS ticket_activity_body_idx;
DROP TABLE IF EXISTS helpdesk_ticket_activity;
DROP TRIGGER IF EXISTS set_ticket_updated_at;
DROP INDEX IF EXISTS ticket_due_date_idx;
DROP INDEX IF EXISTS ticket_assigned_to_created_at_idx;
DROP INDEX IF EXISTS ticket_priority_created_at_idx;
DROP INDEX IF EXISTS ticket_status_created_at_idx;
DROP INDEX IF EXISTS ticket_body_idx;
DROP INDEX IF EXISTS ticket_created_at_idx;
DROP INDEX IF EXISTS ticket_requester_created_at_idx;
DROP TABLE IF EXISTS helpdesk_ticket;
DROP TABLE IF EXISTS helpdesk_profile;
DROP TABLE IF EXISTS helpdesk_priority;
DROP TABLE IF EXISTS helpdesk_ticket_status;
DROP TABLE IF EXISTS auth_app_provider;
DROP TABLE IF EXISTS auth_session;
DROP TABLE IF EXISTS auth_user;
