-- migrate:up
CREATE EXTENSION IF NOT EXISTS pgcrypto;
CREATE EXTENSION IF NOT EXISTS pg_trgm;

CREATE SCHEMA IF NOT EXISTS common;
CREATE SCHEMA IF NOT EXISTS auth;
CREATE SCHEMA IF NOT EXISTS helpdesk;

-- common schema
CREATE OR REPLACE FUNCTION common.set_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = now();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- auth schema
CREATE TABLE auth."user" (
    id         BIGSERIAL   PRIMARY KEY,
    username   TEXT        NOT NULL UNIQUE,
    password   TEXT        NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE auth.session (
    id         BIGSERIAL   PRIMARY KEY,
    user_id    BIGINT      NOT NULL REFERENCES auth."user"(id),
    token      TEXT        NOT NULL UNIQUE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    expires_at TIMESTAMPTZ NOT NULL
);

CREATE TYPE auth.provider AS ENUM (
    'overlord'
);

CREATE TABLE auth.useroauth (
    id          BIGSERIAL     PRIMARY KEY,
    user_id     BIGINT        NOT NULL UNIQUE REFERENCES auth."user"(id),
    provider_id BIGINT        NOT NULL UNIQUE,
    provider    auth.provider NOT NULL,
    created_at  TIMESTAMPTZ   NOT NULL DEFAULT now()
);

-- helpdesk schema
CREATE TYPE helpdesk.role AS ENUM (
    'system',
    'administrator',
    'supervisor',
    'technician',
    'requester'
);

CREATE TABLE helpdesk.department (
    id          BIGSERIAL   PRIMARY KEY,
    name        TEXT        NOT NULL UNIQUE,
    description TEXT        NOT NULL DEFAULT '',
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE helpdesk.profile (
    id            BIGSERIAL     PRIMARY KEY,
    user_id       BIGINT        NOT NULL UNIQUE REFERENCES auth."user"(id),
    department_id BIGINT        NOT NULL REFERENCES helpdesk.department(id),
    role          helpdesk.role NOT NULL,
    name          TEXT          NOT NULL,
    created_at    TIMESTAMPTZ  NOT NULL DEFAULT now()
);

CREATE TYPE helpdesk.ticket_trait AS ENUM (
    'open',
    'in_progress',
    'closed'
);

CREATE TABLE helpdesk.ticket_status (
    id           BIGSERIAL             PRIMARY KEY,
    name         TEXT                  NOT NULL UNIQUE,
    display_name TEXT                  NOT NULL,
    trait        helpdesk.ticket_trait NOT NULL
);

CREATE TABLE helpdesk.priority (
    id           BIGSERIAL PRIMARY KEY,
    name         TEXT      NOT NULL UNIQUE,
    display_name TEXT      NOT NULL
);

CREATE TABLE helpdesk.request_category (
    id   BIGSERIAL PRIMARY KEY,
    name TEXT      NOT NULL UNIQUE
);

CREATE TABLE helpdesk.request_type (
    id                  BIGSERIAL   PRIMARY KEY,
    name                TEXT        NOT NULL UNIQUE,
    category_id         BIGINT      NOT NULL REFERENCES helpdesk.request_category(id),
    default_priority_id BIGINT      NOT NULL REFERENCES helpdesk.priority(id),
    description         TEXT        NOT NULL DEFAULT '',
    created_at          TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE helpdesk.setting (
    name                   TEXT   PRIMARY KEY DEFAULT 'default',
    default_status_id      BIGINT NOT NULL REFERENCES helpdesk.ticket_status(id),
    default_department_id  BIGINT NOT NULL REFERENCES helpdesk.department(id),
    default_assigned_to_id BIGINT NOT NULL REFERENCES helpdesk.profile(id),
    assigned_status_id     BIGINT NOT NULL REFERENCES helpdesk.ticket_status(id),
    system_profile_id      BIGINT NOT NULL REFERENCES helpdesk.profile(id)
);

CREATE TABLE helpdesk.ticket (
    id              BIGSERIAL   PRIMARY KEY,
    request_type_id BIGINT      NOT NULL REFERENCES helpdesk.request_type(id),
    requester_id    BIGINT      NOT NULL REFERENCES helpdesk.profile(id),
    assigned_to_id  BIGINT               REFERENCES helpdesk.profile(id),
    department_id   BIGINT      NOT NULL REFERENCES helpdesk.department(id),
    priority_id     BIGINT      NOT NULL REFERENCES helpdesk.priority(id),
    status_id       BIGINT      NOT NULL REFERENCES helpdesk.ticket_status(id),
    description     TEXT        NOT NULL,
    due_date        TIMESTAMPTZ,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at      TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TYPE helpdesk.ticket_activity_kind AS ENUM (
    'message',
    'status_changed',
    'assigned_changed',
    'department_changed',
    'priority_changed',
    'due_date_changed'
);

CREATE TABLE helpdesk.ticket_activity (
    id         BIGSERIAL                     PRIMARY KEY,
    ticket_id  BIGINT                        NOT NULL REFERENCES helpdesk.ticket(id),
    profile_id BIGINT                        NOT NULL REFERENCES helpdesk.profile(id),
    kind       helpdesk.ticket_activity_kind NOT NULL,
    body       TEXT                          NOT NULL DEFAULT '',
    metadata   JSONB                         NOT NULL DEFAULT '{}'::jsonb,
    created_at TIMESTAMPTZ                   NOT NULL DEFAULT now()
);

CREATE TABLE helpdesk.ticket_activity_attachment (
    id                  BIGSERIAL PRIMARY KEY,
    ticket_activity_id  BIGINT      NOT NULL REFERENCES helpdesk.ticket_activity(id),
    file_path           TEXT        NOT NULL,
    file_name           TEXT        NOT NULL,
    file_size           BIGINT      NOT NULL,
    mime_type           TEXT        NOT NULL,
    created_at          TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX ticket_requester_created_at_idx
ON helpdesk.ticket (requester_id, created_at DESC);

CREATE INDEX ticket_created_at_idx
ON helpdesk.ticket (created_at DESC);

CREATE INDEX ticket_description_trgm_idx
ON helpdesk.ticket USING GIN (description gin_trgm_ops);

CREATE INDEX ticket_activity_body_trgm_idx
ON helpdesk.ticket_activity USING GIN (body gin_trgm_ops);

CREATE INDEX ticket_activity_attachment_ticket_activity_id_idx
ON helpdesk.ticket_activity_attachment (ticket_activity_id);

CREATE INDEX ticket_activity_ticket_created_at_idx
ON helpdesk.ticket_activity (ticket_id, created_at DESC, id DESC);

CREATE TRIGGER set_ticket_updated_at
BEFORE UPDATE ON helpdesk.ticket
FOR EACH ROW
EXECUTE FUNCTION common.set_updated_at();


-- migrate:down
-- helpdesk schema
DROP TRIGGER IF EXISTS set_ticket_updated_at ON helpdesk.ticket;
DROP TABLE IF EXISTS helpdesk.ticket_activity_attachment;
DROP TABLE IF EXISTS helpdesk.ticket_activity;
DROP TYPE  IF EXISTS helpdesk.ticket_activity_kind;
DROP TABLE IF EXISTS helpdesk.ticket;
DROP TABLE IF EXISTS helpdesk.setting;
DROP TABLE IF EXISTS helpdesk.request_type;
DROP TABLE IF EXISTS helpdesk.request_category;
DROP TABLE IF EXISTS helpdesk.priority;
DROP TABLE IF EXISTS helpdesk.ticket_status;
DROP TYPE  IF EXISTS helpdesk.ticket_trait;
DROP TABLE IF EXISTS helpdesk.profile;
DROP TABLE IF EXISTS helpdesk.department;
DROP TYPE  IF EXISTS helpdesk.role;

-- auth schema
DROP TABLE IF EXISTS auth.session;
DROP TABLE IF EXISTS auth.useroauth;
DROP TABLE IF EXISTS auth."user";
DROP TYPE  IF EXISTS auth.provider;

-- schemas
DROP SCHEMA IF EXISTS helpdesk;
DROP SCHEMA IF EXISTS auth;
DROP FUNCTION IF EXISTS common.set_updated_at();
DROP SCHEMA IF EXISTS common;

-- extensions
DROP EXTENSION IF EXISTS pg_trgm;
DROP EXTENSION IF EXISTS pgcrypto;
