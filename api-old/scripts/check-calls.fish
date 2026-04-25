#!/usr/bin/env fish

set -l reset (set_color normal)
set -l bold (set_color --bold)
set -l dim (set_color brblack)

function _header
    set -l width 56
    set -l line (string repeat -n $width -)
    printf "\n%s%s%s\n" (set_color --bold cyan) $line $reset
    printf "%s%s%s\n" (set_color --bold cyan) "$argv" $reset
    printf "%s%s%s\n" (set_color --bold cyan) $line $reset
end

function _step
    printf "\n%s[%s]%s %s\n" (set_color --bold blue) STEP $reset "$argv"
end

function _ok
    printf "%s[OK]%s %s\n" (set_color green) $reset "$argv"
end

function _error
    printf "%s[ERROR]%s %s\n" (set_color --bold red) $reset "$argv" >&2
end

function _info
    printf "%s%s%s\n" $dim "$argv" $reset
end

function _require_command --argument-names name
    if not command -q $name
        _error "Missing required command: $name"
        exit 127
    end
end

function _psql
    psql -X -v ON_ERROR_STOP=1 $argv
end

set -g base_url http://127.0.0.1:8000
set -g last_response_file

if test (count $argv) -ge 1
    set base_url $argv[1]
end

set base_url (string replace -r '/$' '' -- $base_url)

if not set -q PGDATABASE
    set -gx PGDATABASE ticketeer
end
if not set -q PGUSER
    set -gx PGUSER postgres
end

_require_command http
_require_command psql
_require_command python3

set -x PGPASSWORD gcca;set -x PGUSER gcca;set -x PGDATABASE ticketeer;set -x PGHOST localhost

function _seed_database
    _step "Applying migration and seeding fixtures in PostgreSQL"
    set -l schema_ready (_psql -At -c "SELECT to_regclass('ticketeer.auth_user') IS NOT NULL")
    if test "$schema_ready" != "t"
        if not _psql -f migrations/001-up_initial.sql >/dev/null
            _error "Failed to apply migrations with psql"
            exit 1
        end
    end

    set -l sql "
TRUNCATE TABLE
    ticketeer.auth_session,
    ticketeer.helpdesk_ticket,
    ticketeer.helpdesk_setting,
    ticketeer.helpdesk_request_type,
    ticketeer.helpdesk_request_category,
    ticketeer.helpdesk_priority,
    ticketeer.helpdesk_ticket_status,
    ticketeer.helpdesk_profile,
    ticketeer.helpdesk_department,
    ticketeer.auth_user
RESTART IDENTITY CASCADE;

INSERT INTO ticketeer.auth_user (username, password) VALUES
    ('administrator', crypt('t801', gen_salt('bf'))),
    ('technician', crypt('t801', gen_salt('bf'))),
    ('requester', crypt('t801', gen_salt('bf'))),
    ('selfservice', crypt('t801', gen_salt('bf')));

INSERT INTO ticketeer.helpdesk_department (name, description) VALUES
    ('IT Support', 'End-user technical support');

INSERT INTO ticketeer.helpdesk_profile (user_id, department_id, role) VALUES
    (1, 1, 'administrator'),
    (2, 1, 'technician'),
    (3, 1, 'requester');

INSERT INTO ticketeer.helpdesk_ticket_status (name, display_name, trait) VALUES
    ('new', 'New', 'open'),
    ('in_progress', 'In Progress', 'in_progress'),
    ('resolved', 'Resolved', 'closed');

INSERT INTO ticketeer.helpdesk_priority (name, display_name) VALUES
    ('low', 'Low'),
    ('medium', 'Medium'),
    ('high', 'High'),
    ('urgent', 'Urgent');

INSERT INTO ticketeer.helpdesk_request_category (name) VALUES
    ('incident');

INSERT INTO ticketeer.helpdesk_request_type
    (name, category_id, default_priority_id, description)
VALUES
    ('account_access', 1, 2, 'Account access issue'),
    ('hardware_support', 1, 2, 'Hardware support request');

INSERT INTO ticketeer.helpdesk_setting (name, default_status_id, assigned_status_id) VALUES
    ('default', 1, 1);

INSERT INTO ticketeer.helpdesk_ticket
    (request_type_id, requester_id, assigned_to_id, department_id, priority_id, status_id, description)
VALUES
    (2, 3, NULL, 1, 2, 1, 'Printer not working');

INSERT INTO ticketeer.auth_session (user_id, token, expires_at) VALUES
    (1, 'expired-token', now() - interval '1 hour');
"

    if not _psql -c "$sql" >/dev/null
        _error "Failed to seed PostgreSQL fixtures"
        exit 1
    end
    _ok "Database fixtures loaded into $PGDATABASE"
end

function _request --argument-names expected_status label method path
    set -e argv[1..4]

    if test -n "$last_response_file"
        command rm -f $last_response_file
    end

    set -g last_response_file (mktemp)
    http --ignore-stdin --print=hb $method "$base_url$path" $argv >$last_response_file 2>&1
    set -l http_exit $status
    set -l status_code (sed -n 's/^HTTP\/[^ ]* \([0-9][0-9][0-9]\).*/\1/p' $last_response_file | head -n 1)

    if test -z "$status_code"
        _error "$label did not return an HTTP status"
        cat $last_response_file >&2
        return 1
    end

    if test "$status_code" != "$expected_status"
        _error "$label returned $status_code, expected $expected_status"
        cat $last_response_file >&2
        return 1
    end

    if test $http_exit -ne 0
        if test "$expected_status" = 200 -o "$expected_status" = 201 -o "$expected_status" = 204
            _error "$label exited with status $http_exit"
            cat $last_response_file >&2
            return 1
        end
    end

    _ok "$label returned $status_code"
    return 0
end

function _response_body
    python3 -c "import pathlib, sys
text = pathlib.Path(sys.argv[1]).read_text()
sep = '\r\n\r\n' if '\r\n\r\n' in text else '\n\n'
body = text.split(sep, 1)[1] if sep in text else ''
print(body.strip())" $last_response_file
end

function _response_json_field --argument-names field
    _response_body | python3 -c "import json, sys
body = sys.stdin.read().strip()
value = json.loads(body).get(sys.argv[1], '')
print(value)" $field
end

function _assert_non_empty --argument-names value label
    if test -z "$value"
        _error "$label is empty"
        if test -n "$last_response_file"
            cat $last_response_file >&2
        end
        exit 1
    end
    _ok "$label captured"
end

function _assert_selfservice_profile_created
    set -l count (_psql -At -c "SELECT count(*) FROM ticketeer.helpdesk_profile WHERE user_id = 4 AND role = 'requester'::ticketeer.helpdesk_role")
    if test "$count" != "1"
        _error "Self-service requester profile was not created"
        exit 1
    end
    _ok "Self-service requester profile created"
end

_header "ticketeer-api call checks"
_info "Base URL: $base_url"
_info "Database: $PGDATABASE"
_info "User: $PGUSER"

_seed_database

_step "GET /"
_request 200 "GET /" GET "/"; or exit 1

_header "Auth"

_request 200 "POST /auth/signin administrator" POST "/ticketeer/api/v1/auth/signin" \
    username=administrator \
    password=t801; or exit 1
set -l admin_token (_response_json_field token)
_assert_non_empty "$admin_token" "administrator token"

_request 200 "POST /auth/signin requester" POST "/ticketeer/api/v1/auth/signin" \
    username=requester \
    password=t801; or exit 1
set -l requester_token (_response_json_field token)
_assert_non_empty "$requester_token" "requester token"

_request 200 "POST /auth/signin selfservice" POST "/ticketeer/api/v1/auth/signin" \
    username=selfservice \
    password=t801; or exit 1
set -l selfservice_token (_response_json_field token)
_assert_non_empty "$selfservice_token" "selfservice token"

_request 400 "POST /auth/signin invalid body" POST "/ticketeer/api/v1/auth/signin"; or exit 1
_request 401 "POST /auth/signin wrong credentials" POST "/ticketeer/api/v1/auth/signin" \
    username=administrator \
    password=wrong; or exit 1

_header "Departments"

_request 200 "GET /helpdesk/departments" GET "/ticketeer/api/v1/helpdesk/departments" \
    Authorization:"Bearer $admin_token"; or exit 1

_request 401 "GET /helpdesk/departments without token" GET "/ticketeer/api/v1/helpdesk/departments"; or exit 1

_request 201 "POST /helpdesk/departments" POST "/ticketeer/api/v1/helpdesk/departments" \
    Authorization:"Bearer $admin_token" \
    name="Security" \
    description="Physical and digital security"; or exit 1
set -l created_department_id (_response_json_field id)
_assert_non_empty "$created_department_id" "created department id"

_request 400 "POST /helpdesk/departments missing name" POST "/ticketeer/api/v1/helpdesk/departments" \
    Authorization:"Bearer $admin_token" \
    description="No name provided"; or exit 1

_request 200 "GET /helpdesk/departments/1" GET "/ticketeer/api/v1/helpdesk/departments/1" \
    Authorization:"Bearer $admin_token"; or exit 1

_request 404 "GET /helpdesk/departments/9999" GET "/ticketeer/api/v1/helpdesk/departments/9999" \
    Authorization:"Bearer $admin_token"; or exit 1

_request 200 "PUT /helpdesk/departments/1" PUT "/ticketeer/api/v1/helpdesk/departments/1" \
    Authorization:"Bearer $admin_token" \
    name="IT Support" \
    description="Updated description"; or exit 1

_request 403 "PUT /helpdesk/departments/1 non-admin" PUT "/ticketeer/api/v1/helpdesk/departments/1" \
    Authorization:"Bearer $requester_token" \
    name="IT Support" \
    description="Requester cannot update"; or exit 1

_request 200 "PATCH /helpdesk/departments/1" PATCH "/ticketeer/api/v1/helpdesk/departments/1" \
    Authorization:"Bearer $admin_token" \
    description="Only updating description"; or exit 1

_request 204 "DELETE /helpdesk/departments/$created_department_id" DELETE "/ticketeer/api/v1/helpdesk/departments/$created_department_id" \
    Authorization:"Bearer $admin_token"; or exit 1

_request 401 "DELETE /helpdesk/departments/1 expired token" DELETE "/ticketeer/api/v1/helpdesk/departments/1" \
    Authorization:"Bearer expired-token"; or exit 1

_header "Tickets Admin"

_request 200 "GET /helpdesk/tickets" GET "/ticketeer/api/v1/helpdesk/tickets" \
    Authorization:"Bearer $admin_token"; or exit 1

_request 403 "GET /helpdesk/tickets non-admin" GET "/ticketeer/api/v1/helpdesk/tickets" \
    Authorization:"Bearer $requester_token"; or exit 1

_request 201 "POST /helpdesk/tickets" POST "/ticketeer/api/v1/helpdesk/tickets" \
    Authorization:"Bearer $admin_token" \
    request_type_id:=2 \
    requester_id:=3 \
    department_id:=1 \
    priority_id:=2 \
    description="Screen flickering on workstation 4"; or exit 1
set -l created_ticket_id (_response_json_field id)
_assert_non_empty "$created_ticket_id" "created ticket id"

_request 400 "POST /helpdesk/tickets missing fields" POST "/ticketeer/api/v1/helpdesk/tickets" \
    Authorization:"Bearer $admin_token" \
    description="Missing required fields"; or exit 1

_request 200 "GET /helpdesk/tickets/1" GET "/ticketeer/api/v1/helpdesk/tickets/1" \
    Authorization:"Bearer $admin_token"; or exit 1

_request 404 "GET /helpdesk/tickets/9999" GET "/ticketeer/api/v1/helpdesk/tickets/9999" \
    Authorization:"Bearer $admin_token"; or exit 1

_request 200 "PUT /helpdesk/tickets/1" PUT "/ticketeer/api/v1/helpdesk/tickets/1" \
    Authorization:"Bearer $admin_token" \
    request_type_id:=2 \
    department_id:=1 \
    priority_id:=3 \
    status_id:=2 \
    description="Printer not working - escalated"; or exit 1

_request 200 "PATCH /helpdesk/tickets/1" PATCH "/ticketeer/api/v1/helpdesk/tickets/1" \
    Authorization:"Bearer $admin_token" \
    priority_id:=4 \
    status_id:=3; or exit 1

_request 204 "DELETE /helpdesk/tickets/1" DELETE "/ticketeer/api/v1/helpdesk/tickets/1" \
    Authorization:"Bearer $admin_token"; or exit 1

_header "Dashboard"

_request 200 "GET /helpdesk/roles/content" GET "/ticketeer/api/v1/helpdesk/roles/content" \
    Authorization:"Bearer $admin_token"; or exit 1

_request 200 "GET /helpdesk/roles/content requester" GET "/ticketeer/api/v1/helpdesk/roles/content" \
    Authorization:"Bearer $requester_token"; or exit 1

_request 404 "GET /helpdesk/roles/content no profile" GET "/ticketeer/api/v1/helpdesk/roles/content" \
    Authorization:"Bearer $selfservice_token"; or exit 1

_request 401 "GET /helpdesk/roles/content without token" GET "/ticketeer/api/v1/helpdesk/roles/content"; or exit 1

_header "Profiles"

_request 200 "GET /helpdesk/profiles/me" GET "/ticketeer/api/v1/helpdesk/profiles/me" \
    Authorization:"Bearer $requester_token"; or exit 1

_request 404 "GET /helpdesk/profiles/me no profile" GET "/ticketeer/api/v1/helpdesk/profiles/me" \
    Authorization:"Bearer $selfservice_token"; or exit 1

_request 401 "GET /helpdesk/profiles/me without token" GET "/ticketeer/api/v1/helpdesk/profiles/me"; or exit 1

_header "Ticket Request"

_request 201 "POST /helpdesk/roles/requester/request" POST "/ticketeer/api/v1/helpdesk/roles/requester/request" \
    Authorization:"Bearer $selfservice_token" \
    request_type_id:=2 \
    department_id:=1 \
    priority_id:=2 \
    description="My keyboard is not working"; or exit 1
set -l requested_ticket_id (_response_json_field id)
_assert_non_empty "$requested_ticket_id" "requested ticket id"
_assert_selfservice_profile_created
set -l selfservice_profile_id (_psql -At -c "SELECT id FROM ticketeer.helpdesk_profile WHERE user_id = 4")
_assert_non_empty "$selfservice_profile_id" "selfservice profile id"

_request 200 "GET /helpdesk/roles/requester/requested_by/$selfservice_profile_id" GET "/ticketeer/api/v1/helpdesk/roles/requester/requested_by/$selfservice_profile_id" \
    Authorization:"Bearer $selfservice_token"; or exit 1

_request 403 "GET /helpdesk/roles/requester/requested_by/$selfservice_profile_id forbidden" GET "/ticketeer/api/v1/helpdesk/roles/requester/requested_by/$selfservice_profile_id" \
    Authorization:"Bearer $admin_token"; or exit 1

_request 404 "GET /helpdesk/roles/requester/requested_by/9999" GET "/ticketeer/api/v1/helpdesk/roles/requester/requested_by/9999" \
    Authorization:"Bearer $selfservice_token"; or exit 1

_request 401 "GET /helpdesk/roles/requester/requested_by/$selfservice_profile_id without token" GET "/ticketeer/api/v1/helpdesk/roles/requester/requested_by/$selfservice_profile_id"; or exit 1

_request 400 "POST /helpdesk/roles/requester/request missing fields" POST "/ticketeer/api/v1/helpdesk/roles/requester/request" \
    Authorization:"Bearer $selfservice_token" \
    description="Forgot required fields"; or exit 1

_request 401 "POST /helpdesk/roles/requester/request without token" POST "/ticketeer/api/v1/helpdesk/roles/requester/request" \
    request_type_id:=2 \
    department_id:=1 \
    priority_id:=2 \
    description="Not authenticated"; or exit 1

_info "The documented 503 path is exercised with a database constraint error."
_request 503 "POST /helpdesk/roles/requester/request 503 path" POST "/ticketeer/api/v1/helpdesk/roles/requester/request" \
    Authorization:"Bearer $selfservice_token" \
    request_type_id:=9999 \
    department_id:=1 \
    priority_id:=2 \
    description="Triggering a database error"; or exit 1

_header "Completed"
_ok "All documented calls in docs/calls.md were exercised successfully"

if test -n "$last_response_file"
    command rm -f $last_response_file
end
