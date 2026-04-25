CREATE EXTENSION IF NOT EXISTS pgcrypto;

CREATE SCHEMA IF NOT EXISTS ticketeer;

CREATE TABLE ticketeer.auth_user (
    id         BIGSERIAL    PRIMARY KEY,
    username   TEXT         NOT NULL UNIQUE,
    password   TEXT         NOT NULL,
    created_at TIMESTAMPTZ  NOT NULL DEFAULT now()
);

CREATE TYPE ticketeer.helpdesk_role AS ENUM (
    'administrator',
    'supervisor',
    'technician',
    'requester'
);

CREATE TABLE ticketeer.helpdesk_department (
    id          BIGSERIAL    PRIMARY KEY,
    name        TEXT         NOT NULL UNIQUE,
    description TEXT         NOT NULL DEFAULT '',
    created_at  TIMESTAMPTZ  NOT NULL DEFAULT now()
);

CREATE TABLE ticketeer.helpdesk_profile (
    id            BIGSERIAL                PRIMARY KEY,
    user_id       BIGINT                   NOT NULL UNIQUE REFERENCES ticketeer.auth_user(id),
    department_id BIGINT                   NOT NULL REFERENCES ticketeer.helpdesk_department(id),
    role          ticketeer.helpdesk_role   NOT NULL,
    created_at    TIMESTAMPTZ              NOT NULL DEFAULT now()
);

CREATE TYPE ticketeer.helpdesk_ticket_trait AS ENUM (
    'open',
    'in_progress',
    'closed'
);

CREATE TABLE ticketeer.helpdesk_ticket_status (
    id           BIGSERIAL                      PRIMARY KEY,
    name         TEXT                           NOT NULL UNIQUE,
    display_name TEXT                           NOT NULL,
    trait        ticketeer.helpdesk_ticket_trait NOT NULL
);

CREATE TABLE ticketeer.helpdesk_priority (
    id           BIGSERIAL PRIMARY KEY,
    name         TEXT      NOT NULL UNIQUE,
    display_name TEXT      NOT NULL
);

CREATE TABLE ticketeer.helpdesk_request_category (
    id   BIGSERIAL PRIMARY KEY,
    name TEXT      NOT NULL UNIQUE
);

CREATE TABLE ticketeer.helpdesk_request_type (
    id                  BIGSERIAL   PRIMARY KEY,
    name                TEXT        NOT NULL UNIQUE,
    category_id         BIGINT      NOT NULL REFERENCES ticketeer.helpdesk_request_category(id),
    default_priority_id BIGINT      NOT NULL REFERENCES ticketeer.helpdesk_priority(id),
    description         TEXT        NOT NULL DEFAULT '',
    created_at          TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE ticketeer.helpdesk_setting (
    name               TEXT   PRIMARY KEY DEFAULT 'default',
    default_status_id  BIGINT NOT NULL REFERENCES ticketeer.helpdesk_ticket_status(id),
    assigned_status_id BIGINT NOT NULL REFERENCES ticketeer.helpdesk_ticket_status(id)
);

CREATE TABLE ticketeer.helpdesk_ticket (
    id              BIGSERIAL                      PRIMARY KEY,
    request_type_id BIGINT                         NOT NULL REFERENCES ticketeer.helpdesk_request_type(id),
    requester_id    BIGINT                         NOT NULL REFERENCES ticketeer.helpdesk_profile(id),
    assigned_to_id  BIGINT                                  REFERENCES ticketeer.helpdesk_profile(id),
    department_id   BIGINT                         NOT NULL REFERENCES ticketeer.helpdesk_department(id),
    priority_id     BIGINT                         NOT NULL REFERENCES ticketeer.helpdesk_priority(id),
    status_id       BIGINT                         NOT NULL REFERENCES ticketeer.helpdesk_ticket_status(id),
    description     TEXT                           NOT NULL,
    due_date        TIMESTAMPTZ,
    created_at      TIMESTAMPTZ                    NOT NULL DEFAULT now(),
    updated_at      TIMESTAMPTZ                    NOT NULL DEFAULT now()
);

CREATE TABLE ticketeer.auth_session (
    id         BIGSERIAL    PRIMARY KEY,
    user_id    BIGINT       NOT NULL REFERENCES ticketeer.auth_user(id),
    token      TEXT         NOT NULL UNIQUE,
    created_at TIMESTAMPTZ  NOT NULL DEFAULT now(),
    expires_at TIMESTAMPTZ  NOT NULL
);
