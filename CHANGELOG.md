# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Supervisors can now edit a ticket's due date after it has been created, instead of it being fixed at creation time.
- Ticket timestamps (created, last updated, activity history) now display in the business's local time instead of UTC, so staff see accurate local times.
- Ticket lists now show and can be sorted by last activity time, making it easier to spot tickets that have gone stale.
- Every column in the ticket lists (detail, requester, status, priority, assignee, created, last activity) can now be sorted, for both requesters and supervisors.
- Requesters can now filter their own ticket list by status and priority, matching the supervisor experience.
- On mobile, the full ticket description can be viewed in a pop-up instead of being cut off in the table; tooltips over ticket descriptions also now show the complete text.
- Supervisors can fully manage the catalog of ticket statuses and priorities (create, edit, retire) instead of these being fixed values.

### Changed
- Deleting a ticket status or priority that is still in use now requires choosing a replacement first, so tickets are never left in an invalid state.
- Reduced the default maximum length for ticket descriptions and messages to encourage more concise tickets.
- Reorganized the supervisor ticket details view so labels and values line up consistently.
- Internal code reorganization to make future feature work faster to build and ship; no user-facing behavior change.

### Fixed
- Fixed the assignee field on ticket details overflowing or looking broken when a technician or supervisor has a long name.

## 2026-06-21
- Work on `feat/settings` and `feat/manage-ticket_status` branches for configuration modules.

## 2026-06-20

### Removed
- **PR [#13](https://github.com/plaza-san-miguel/ticketeer/pull/13)**: Remove request type from tickets, schema, and UI.
  - Completely removes `helpdesk_request_type`, `helpdesk_request_category`, `request_type_id` column from `helpdesk_ticket`, the `request_type_changed` activity kind, related supervisor config routes/handlers/templates, and all UI references.
  - Ticket creation now only requires `priority_id` + `body`.
  - Closes #11.
  - Net removal of substantial code and 2 DB tables.
  - Migration: `20260620000000_03_remove_request_type.sql`.

## 2026-06-19

### Removed
- **PR [#10](https://github.com/plaza-san-miguel/ticketeer/pull/10)**: Remove `helpdesk_department` table and all Area references.
  - Drops `helpdesk_department` table.
  - Removes `department_id` FKs/columns from `helpdesk_profile`, `helpdesk_setting`, `helpdesk_ticket`.
  - Removes all Area/department selectors, filters, columns, and handlers from requester and supervisor flows.
  - Closes #5.
  - Migration: `20260619000000_02_remove_department.sql`.
  - Schema and fixtures updated.

## 2026-06-18

### Added
- **PR [#2](https://github.com/plaza-san-miguel/ticketeer/pull/2)**: Add supervisor RequestType configuration.
  - New supervisor module for managing the request type catalog (CRUD for `helpdesk_request_type` with category and default priority).
  - Routes: config request-type list/create/update/void.
  - New template `supervisor_config_request_type.csp`.
  - Validation, usage checks (prevent delete if tickets reference), sidebar integration.
  - (Later fully removed in PR #13.)

## 2026-05-31 and earlier (Initial development)

- Initial project scaffolding and core features added in large commits ("Ticketeer 🎟️").
- Core stack: C++23 / Drogon, SQLite, htmx + daisyUI, quill logging.
- Authentication (local + Office 365 device code flow).
- Role-based access: requester and supervisor.
- Ticket system: create, list, details, activity stream (messages + attachments), priority/status/assignment changes with activity logging.
- File uploads for attachments (stored on disk under `UPLOAD_DIR`).
- Initial DB schema + fixtures + migrations.
- Docker / Fly.io deployment setup, CI workflows.
- Added LICENSE in later history replay.

### Issues
- Issues are tracked on https://github.com/plaza-san-miguel/ticketeer (issues disabled on gcca/ticketeer mirror).
- Several closed issues correspond to the removals above (#11, #5, #3, #4).
- Open issues include enhancements for ticket title field, last activity display, comment locking on closed tickets, status simplification, due date automation, etc.
