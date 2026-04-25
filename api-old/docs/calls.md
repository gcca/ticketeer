# API Calls Reference

Base URL: `http://localhost:8000`

All examples use [HTTPie](https://httpie.io/).  
Authenticated endpoints require `Authorization: Bearer <token>`.

---

## Auth

### POST /ticketeer/api/v1/auth/signin

No authentication required.

**Success — 200**
```sh
http POST :8000/ticketeer/api/v1/auth/signin \
  username=administrator \
  password=t801
```
```json
{ "token": "aB3xZ..." }
```

**Fail — 400** missing fields
```sh
http POST :8000/ticketeer/api/v1/auth/signin
```
```
400 Bad Request
Invalid request body
```

**Fail — 401** wrong credentials
```sh
http POST :8000/ticketeer/api/v1/auth/signin \
  username=administrator \
  password=wrong
```
```json
{ "error": "invalid_credentials", "message": "Username or password is incorrect" }
```

---

## Roles

### GET /ticketeer/api/v1/helpdesk/roles/content

Permission: `LoginRequired`.  
Returns the authenticated user's profile together with lookup data needed to render the roles view.

**Success — 200**
```sh
http GET :8000/ticketeer/api/v1/helpdesk/roles/content \
  Authorization:"Bearer <token>"
```
```json
{
  "profile": {
    "id": 3,
    "user_id": 5,
    "department_id": 1,
    "role": "requester",
    "created_at": "2026-04-04T12:00:00.000000+00"
  },
  "departments": [
    { "id": 1, "name": "IT Support" }
  ],
  "priorities": [
    { "id": 1, "name": "low",      "display_name": "Baja" },
    { "id": 2, "name": "medium",   "display_name": "Media" },
    { "id": 3, "name": "high",     "display_name": "Alta" },
    { "id": 4, "name": "critical", "display_name": "Crítica" }
  ],
  "request_types": [
    {
      "id": 1,
      "name": "hardware_failure",
      "category_id": 1,
      "default_priority_id": 3,
      "description": "Physical hardware malfunction"
    }
  ],
  "ticket_statuses": [
    {
      "id": 1,
      "name": "new",
      "display_name": "New",
      "trait": "open"
    },
    {
      "id": 2,
      "name": "in_progress",
      "display_name": "In Progress",
      "trait": "in_progress"
    },
    {
      "id": 3,
      "name": "resolved",
      "display_name": "Resolved",
      "trait": "closed"
    }
  ]
}
```

**Fail — 404** user has no helpdesk profile
```sh
http GET :8000/ticketeer/api/v1/helpdesk/roles/content \
  Authorization:"Bearer <noprofile-token>"
```
```json
{ "error": "not_found" }
```

**Fail — 401** no token
```sh
http GET :8000/ticketeer/api/v1/helpdesk/roles/content
```
```json
{ "error": "unauthorized" }
```

---

## Departments

All department endpoints require a valid session token.  
`POST`, `PUT`, `PATCH`, `DELETE` additionally require `role = administrator`.

### GET /ticketeer/api/v1/helpdesk/departments

Permission: `LoginRequired`

Query params: `page` (default 1), `page_size` (default 20, max 100).

**Success — 200**
```sh
http GET :8000/ticketeer/api/v1/helpdesk/departments \
  Authorization:"Bearer <token>"
```
```json
{
  "items": [
    { "id": 1, "name": "IT Support", "description": "End-user technical support", "created_at": "2026-04-04T12:00:00.000000+00" }
  ],
  "total": 1,
  "page": 1,
  "page_size": 20
}
```

**Fail — 401** no token
```sh
http GET :8000/ticketeer/api/v1/helpdesk/departments
```
```json
{ "error": "unauthorized" }
```

---

### POST /ticketeer/api/v1/helpdesk/departments

Permission: `LoginRequired` + `administrator`

**Success — 201**
```sh
http POST :8000/ticketeer/api/v1/helpdesk/departments \
  Authorization:"Bearer <token>" \
  name="Security" \
  description="Physical and digital security"
```
```json
{ "id": 13 }
```

**Fail — 400** missing name
```sh
http POST :8000/ticketeer/api/v1/helpdesk/departments \
  Authorization:"Bearer <token>" \
  description="No name provided"
```
```
400 Bad Request
Invalid request body
```

---

### GET /ticketeer/api/v1/helpdesk/departments/<id>

Permission: `LoginRequired`

**Success — 200**
```sh
http GET :8000/ticketeer/api/v1/helpdesk/departments/1 \
  Authorization:"Bearer <token>"
```
```json
{ "id": 1, "name": "IT Support", "description": "End-user technical support", "created_at": "2026-04-04T12:00:00.000000+00" }
```

**Fail — 404**
```sh
http GET :8000/ticketeer/api/v1/helpdesk/departments/9999 \
  Authorization:"Bearer <token>"
```
```json
{ "error": "not_found" }
```

---

### PUT /ticketeer/api/v1/helpdesk/departments/<id>

Permission: `LoginRequired` + `administrator`

**Success — 200**
```sh
http PUT :8000/ticketeer/api/v1/helpdesk/departments/1 \
  Authorization:"Bearer <token>" \
  name="IT Support" \
  description="Updated description"
```
```
200 OK
```

**Fail — 403** insufficient role
```sh
http PUT :8000/ticketeer/api/v1/helpdesk/departments/1 \
  Authorization:"Bearer <requester-token>" \
  name="IT Support" \
  description="..."
```
```json
{ "error": "forbidden" }
```

---

### PATCH /ticketeer/api/v1/helpdesk/departments/<id>

Permission: `LoginRequired` + `administrator`

**Success — 200** partial update
```sh
http PATCH :8000/ticketeer/api/v1/helpdesk/departments/1 \
  Authorization:"Bearer <token>" \
  description="Only updating description"
```
```
200 OK
```

---

### DELETE /ticketeer/api/v1/helpdesk/departments/<id>

Permission: `LoginRequired` + `administrator`

**Success — 204**
```sh
http DELETE :8000/ticketeer/api/v1/helpdesk/departments/13 \
  Authorization:"Bearer <token>"
```
```
204 No Content
```

**Fail — 401** expired token
```sh
http DELETE :8000/ticketeer/api/v1/helpdesk/departments/13 \
  Authorization:"Bearer expired-token"
```
```json
{ "error": "unauthorized" }
```

---

## Administrator

### GET /ticketeer/api/v1/helpdesk/administrator/technicians

Permission: `LoginRequired` + `administrator`.  
Returns all technician profiles with their username, for use in ticket assignment.

**Success — 200**
```sh
http GET :8000/ticketeer/api/v1/helpdesk/administrator/technicians \
  Authorization:"Bearer <admin-token>"
```
```json
[
  { "id": 2, "user_id": 2, "department_id": 1, "username": "technician" }
]
```

**Fail — 403** non-admin token
```sh
http GET :8000/ticketeer/api/v1/helpdesk/administrator/technicians \
  Authorization:"Bearer <requester-token>"
```
```json
{ "error": "forbidden" }
```

**Fail — 401** no token
```sh
http GET :8000/ticketeer/api/v1/helpdesk/administrator/technicians
```
```json
{ "error": "unauthorized" }
```

---

## Roles (administrator)

### GET /ticketeer/api/v1/helpdesk/roles/administrator/tickets

Permission: `LoginRequired` + `administrator`.  
Returns up to 100 most recent tickets with human-readable priority, status, and usernames.

**Success — 200**
```sh
http GET :8000/ticketeer/api/v1/helpdesk/roles/administrator/tickets \
  Authorization:"Bearer <admin-token>"
```
```json
[
  {
    "id": 1,
    "description": "Printer not working",
    "priority": "Media",
    "status": "New",
    "created_at": "2026-04-04T12:00:00.000000+00",
    "assigned_to": null,
    "created_by": "requester"
  }
]
```

**Fail — 403** non-admin token
```sh
http GET :8000/ticketeer/api/v1/helpdesk/roles/administrator/tickets \
  Authorization:"Bearer <requester-token>"
```
```json
{ "error": "forbidden" }
```

**Fail — 401** no token
```sh
http GET :8000/ticketeer/api/v1/helpdesk/roles/administrator/tickets
```
```json
{ "error": "unauthorized" }
```

---

## Roles (supervisor)

### GET /ticketeer/api/v1/helpdesk/roles/supervisor/tickets

Permission: `LoginRequired` + `supervisor`.  
Query params: `ticket_status_id` optional.  
Returns up to 50 most recent tickets. When `ticket_status_id` is provided, only tickets with that status are returned.

**Success — 200**
```sh
http GET :8000/ticketeer/api/v1/helpdesk/roles/supervisor/tickets \
  Authorization:"Bearer <supervisor-token>"
```
```json
[
  {
    "id": 1,
    "description": "Printer not working",
    "priority": "Media",
    "status": "New",
    "created_at": "2026-04-04T12:00:00.000000+00",
    "assigned_to": null,
    "created_by": "requester"
  }
]
```

**Success — 200** filtered by status
```sh
http GET :8000/ticketeer/api/v1/helpdesk/roles/supervisor/tickets ticket_status_id==1 \
  Authorization:"Bearer <supervisor-token>"
```

**Fail — 400** invalid `ticket_status_id`
```sh
http GET :8000/ticketeer/api/v1/helpdesk/roles/supervisor/tickets ticket_status_id==abc \
  Authorization:"Bearer <supervisor-token>"
```
```
400 Bad Request
Invalid ticket_status_id
```

**Fail — 403** non-supervisor token
```sh
http GET :8000/ticketeer/api/v1/helpdesk/roles/supervisor/tickets \
  Authorization:"Bearer <requester-token>"
```
```json
{ "error": "forbidden" }
```

**Fail — 401** no token
```sh
http GET :8000/ticketeer/api/v1/helpdesk/roles/supervisor/tickets
```
```json
{ "error": "unauthorized" }
```

---

### GET /ticketeer/api/v1/helpdesk/roles/supervisor/tickets/<id>

Permission: `LoginRequired` + `supervisor`.  
Returns the full detail of a ticket.

**Success — 200**
```sh
http GET :8000/ticketeer/api/v1/helpdesk/roles/supervisor/tickets/1 \
  Authorization:"Bearer <supervisor-token>"
```
```json
{
  "id": 1,
  "request_type_id": 1,
  "requester_id": 3,
  "assigned_to_id": 2,
  "department_id": 1,
  "priority_id": 2,
  "status_id": 1,
  "description": "Printer not working",
  "due_date": null,
  "created_at": "2026-04-04T12:00:00.000000+00",
  "updated_at": "2026-04-04T12:00:00.000000+00"
}
```

**Fail — 404**
```sh
http GET :8000/ticketeer/api/v1/helpdesk/roles/supervisor/tickets/9999 \
  Authorization:"Bearer <supervisor-token>"
```
```json
{ "error": "not_found" }
```

---

### PUT /ticketeer/api/v1/helpdesk/roles/supervisor/tickets/<id>

Permission: `LoginRequired` + `supervisor`.  
Full replacement. `assigned_to_id` and `due_date` may be `null`. If `assigned_to_id` is not `null`, it must reference a technician profile. When `assigned_to_id` changes from `null` to a technician, the ticket status is automatically replaced with the configured `assigned_status_id` from `helpdesk_setting`.

**Success — 200**
```sh
http PUT :8000/ticketeer/api/v1/helpdesk/roles/supervisor/tickets/1 \
  Authorization:"Bearer <supervisor-token>" \
  request_type_id:=1 \
  requester_id:=3 \
  assigned_to_id:=2 \
  department_id:=1 \
  priority_id:=2 \
  status_id:=2 \
  description="Assigned to technician" \
  due_date:=null
```
```
200 OK
```

**Success — 200** unassign
```sh
http PUT :8000/ticketeer/api/v1/helpdesk/roles/supervisor/tickets/1 \
  Authorization:"Bearer <supervisor-token>" \
  request_type_id:=1 \
  requester_id:=3 \
  assigned_to_id:=null \
  department_id:=1 \
  priority_id:=2 \
  status_id:=1 \
  description="Returned to queue" \
  due_date:=null
```

**Fail — 400** invalid technician
```sh
http PUT :8000/ticketeer/api/v1/helpdesk/roles/supervisor/tickets/1 \
  Authorization:"Bearer <supervisor-token>" \
  request_type_id:=1 \
  requester_id:=3 \
  assigned_to_id:=9999 \
  department_id:=1 \
  priority_id:=2 \
  status_id:=2 \
  description="Assigned to technician" \
  due_date:=null
```
```json
{ "error": "invalid_technician", "technician_id": 9999 }
```

**Fail — 404**
```sh
http PUT :8000/ticketeer/api/v1/helpdesk/roles/supervisor/tickets/9999 \
  Authorization:"Bearer <supervisor-token>" \
  request_type_id:=1 \
  requester_id:=3 \
  assigned_to_id:=null \
  department_id:=1 \
  priority_id:=2 \
  status_id:=1 \
  description="Returned to queue" \
  due_date:=null
```
```json
{ "error": "not_found" }
```

---

### POST /ticketeer/api/v1/helpdesk/roles/supervisor/assign-tickets

Permission: `LoginRequired` + `supervisor`.  
Receives a JSON array of objects containing `ticket_id` and `technician_id` (`technician_id` may be `null` to unassign).  
Items with missing or malformed fields are silently skipped. When a ticket transitions from unassigned to assigned, its status is automatically updated using `assigned_status_id` from the `helpdesk_setting` table.

**Success — 200**
```sh
http POST :8000/ticketeer/api/v1/helpdesk/roles/supervisor/assign-tickets \
  Authorization:"Bearer <supervisor-token>" \
  <<< '[{"ticket_id":1,"technician_id":2},{"ticket_id":2,"technician_id":2}]'
```
```json
{ "updated": 2 }
```

**Success — 200** unassign tickets
```sh
http POST :8000/ticketeer/api/v1/helpdesk/roles/supervisor/assign-tickets \
  Authorization:"Bearer <supervisor-token>" \
  <<< '[{"ticket_id":1,"technician_id":null}]'
```
```json
{ "updated": 1 }
```

**Fail — 400** invalid body
```sh
http POST :8000/ticketeer/api/v1/helpdesk/roles/supervisor/assign-tickets \
  Authorization:"Bearer <supervisor-token>" \
  <<< '{}'
```
```json
{ "error": "Expected array of assignments" }
```

**Fail — 400** empty array
```sh
http POST :8000/ticketeer/api/v1/helpdesk/roles/supervisor/assign-tickets \
  Authorization:"Bearer <supervisor-token>" \
  <<< '[]'
```
```json
{ "error": "No assignments provided" }
```

---

### GET /ticketeer/api/v1/helpdesk/roles/supervisor/technicians

Permission: `LoginRequired` + `supervisor`.  
Returns technician profiles with nested user information.

**Success — 200**
```sh
http GET :8000/ticketeer/api/v1/helpdesk/roles/supervisor/technicians \
  Authorization:"Bearer <supervisor-token>"
```
```json
[
  {
    "id": 2,
    "user_id": 2,
    "department_id": 1,
    "role": "technician",
    "created_at": "2026-04-04T12:00:00.000000+00",
    "user": {
      "id": 2,
      "username": "technician",
      "created_at": "2026-04-04T12:00:00.000000+00"
    }
  }
]
```

**Fail — 403** non-supervisor token
```sh
http GET :8000/ticketeer/api/v1/helpdesk/roles/supervisor/technicians \
  Authorization:"Bearer <requester-token>"
```
```json
{ "error": "forbidden" }
```

---

## Technician

### GET /ticketeer/api/v1/helpdesk/roles/technician/tickets

Permission: `LoginRequired` + `technician`.  
Returns up to 50 tickets assigned to the current technician profile.

**Success — 200**
```sh
http GET :8000/ticketeer/api/v1/helpdesk/roles/technician/tickets \
  Authorization:"Bearer <technician-token>"
```
```json
[
  {
    "id": 1,
    "description": "Printer not working",
    "priority": "Media",
    "status": "New",
    "created_at": "2026-04-04T12:00:00.000000+00",
    "created_by": "requester"
  }
]
```

**Fail — 403** non-technician token
```sh
http GET :8000/ticketeer/api/v1/helpdesk/roles/technician/tickets \
  Authorization:"Bearer <requester-token>"
```
```json
{ "error": "forbidden" }
```

**Fail — 401** no token
```sh
http GET :8000/ticketeer/api/v1/helpdesk/technician/tickets
```
```json
{ "error": "unauthorized" }
```

---
## Ticket Statuses

`POST`, `GET /<id>`, `PUT`, `PATCH`, `DELETE` require `LoginRequired` + `administrator`.  
`GET` (list) requires `LoginRequired` only.

### GET /ticketeer/api/v1/helpdesk/ticket-statuses

Permission: `LoginRequired`

Query params: `page` (default 1), `page_size` (default 20, max 100).

**Success — 200**
```sh
http GET :8000/ticketeer/api/v1/helpdesk/ticket-statuses \
  Authorization:"Bearer <token>"
```
```json
{
  "items": [
    { "id": 1, "name": "new",         "display_name": "New",         "trait": "open" },
    { "id": 2, "name": "in_progress", "display_name": "In Progress", "trait": "in_progress" },
    { "id": 3, "name": "resolved",    "display_name": "Resolved",    "trait": "closed" }
  ],
  "total": 3,
  "page": 1,
  "page_size": 20
}
```

**Fail — 401** no token
```sh
http GET :8000/ticketeer/api/v1/helpdesk/ticket-statuses
```
```json
{ "error": "unauthorized" }
```

---

### POST /ticketeer/api/v1/helpdesk/ticket-statuses

**Success — 201**
```sh
http POST :8000/ticketeer/api/v1/helpdesk/ticket-statuses \
  Authorization:"Bearer <admin-token>" \
  name="on_hold" \
  display_name="On Hold" \
  trait="in_progress"
```
```json
{ "id": 4 }
```

**Fail — 400** missing fields
```sh
http POST :8000/ticketeer/api/v1/helpdesk/ticket-statuses \
  Authorization:"Bearer <admin-token>" \
  name="on_hold"
```
```
400 Bad Request
Invalid request body
```

---

### GET /ticketeer/api/v1/helpdesk/ticket-statuses/<id>

**Success — 200**
```sh
http GET :8000/ticketeer/api/v1/helpdesk/ticket-statuses/1 \
  Authorization:"Bearer <admin-token>"
```
```json
{ "id": 1, "name": "new", "display_name": "New", "trait": "open" }
```

**Fail — 404**
```sh
http GET :8000/ticketeer/api/v1/helpdesk/ticket-statuses/9999 \
  Authorization:"Bearer <admin-token>"
```
```json
{ "error": "not_found" }
```

---

### PUT /ticketeer/api/v1/helpdesk/ticket-statuses/<id>

**Success — 200**
```sh
http PUT :8000/ticketeer/api/v1/helpdesk/ticket-statuses/1 \
  Authorization:"Bearer <admin-token>" \
  name="new" \
  display_name="New ticket" \
  trait="open"
```
```
200 OK
```

---

### PATCH /ticketeer/api/v1/helpdesk/ticket-statuses/<id>

**Success — 200** partial update
```sh
http PATCH :8000/ticketeer/api/v1/helpdesk/ticket-statuses/1 \
  Authorization:"Bearer <admin-token>" \
  display_name="New ticket"
```
```
200 OK
```

---

### DELETE /ticketeer/api/v1/helpdesk/ticket-statuses/<id>

**Success — 204**
```sh
http DELETE :8000/ticketeer/api/v1/helpdesk/ticket-statuses/4 \
  Authorization:"Bearer <admin-token>"
```
```
204 No Content
```

---

## Priorities

All priority endpoints require `LoginRequired` + `administrator`.

### GET /ticketeer/api/v1/helpdesk/priorities

Query params: `page` (default 1), `page_size` (default 20, max 100).

**Success — 200**
```sh
http GET :8000/ticketeer/api/v1/helpdesk/priorities \
  Authorization:"Bearer <admin-token>"
```
```json
{
  "items": [
    { "id": 1, "name": "low",      "display_name": "Baja" },
    { "id": 2, "name": "medium",   "display_name": "Media" },
    { "id": 3, "name": "high",     "display_name": "Alta" },
    { "id": 4, "name": "critical", "display_name": "Crítica" }
  ],
  "total": 4,
  "page": 1,
  "page_size": 20
}
```

**Fail — 403** non-admin token
```sh
http GET :8000/ticketeer/api/v1/helpdesk/priorities \
  Authorization:"Bearer <requester-token>"
```
```json
{ "error": "forbidden" }
```

---

### POST /ticketeer/api/v1/helpdesk/priorities

**Success — 201**
```sh
http POST :8000/ticketeer/api/v1/helpdesk/priorities \
  Authorization:"Bearer <admin-token>" \
  name="urgent" \
  display_name="Urgente"
```
```json
{ "id": 5 }
```

**Fail — 400** missing fields
```sh
http POST :8000/ticketeer/api/v1/helpdesk/priorities \
  Authorization:"Bearer <admin-token>" \
  name="urgent"
```
```
400 Bad Request
Invalid request body
```

---

### GET /ticketeer/api/v1/helpdesk/priorities/<id>

**Success — 200**
```sh
http GET :8000/ticketeer/api/v1/helpdesk/priorities/1 \
  Authorization:"Bearer <admin-token>"
```
```json
{ "id": 1, "name": "low", "display_name": "Baja" }
```

**Fail — 404**
```sh
http GET :8000/ticketeer/api/v1/helpdesk/priorities/9999 \
  Authorization:"Bearer <admin-token>"
```
```json
{ "error": "not_found" }
```

---

### PUT /ticketeer/api/v1/helpdesk/priorities/<id>

**Success — 200**
```sh
http PUT :8000/ticketeer/api/v1/helpdesk/priorities/1 \
  Authorization:"Bearer <admin-token>" \
  name="low" \
  display_name="Baja prioridad"
```
```
200 OK
```

---

### PATCH /ticketeer/api/v1/helpdesk/priorities/<id>

**Success — 200** partial update
```sh
http PATCH :8000/ticketeer/api/v1/helpdesk/priorities/1 \
  Authorization:"Bearer <admin-token>" \
  display_name="Baja prioridad"
```
```
200 OK
```

---

### DELETE /ticketeer/api/v1/helpdesk/priorities/<id>

**Success — 204**
```sh
http DELETE :8000/ticketeer/api/v1/helpdesk/priorities/5 \
  Authorization:"Bearer <admin-token>"
```
```
204 No Content
```

---

## Request Types

All request-type endpoints require `LoginRequired` + `administrator`.

### GET /ticketeer/api/v1/helpdesk/request-types

Query params: `page` (default 1), `page_size` (default 20, max 100).

**Success — 200**
```sh
http GET :8000/ticketeer/api/v1/helpdesk/request-types \
  Authorization:"Bearer <admin-token>"
```
```json
{
  "items": [
    {
      "id": 1,
      "name": "account_access",
      "category_id": 1,
      "default_priority_id": 2,
      "description": "Account access issue",
      "created_at": "2026-04-04T12:00:00.000000+00"
    }
  ],
  "total": 2,
  "page": 1,
  "page_size": 20
}
```

**Fail — 403** non-admin token
```sh
http GET :8000/ticketeer/api/v1/helpdesk/request-types \
  Authorization:"Bearer <requester-token>"
```
```json
{ "error": "forbidden" }
```

---

### POST /ticketeer/api/v1/helpdesk/request-types

**Success — 201**
```sh
http POST :8000/ticketeer/api/v1/helpdesk/request-types \
  Authorization:"Bearer <admin-token>" \
  name="network_issue" \
  category_id:=1 \
  default_priority_id:=3 \
  description="Network connectivity problem"
```
```json
{ "id": 3 }
```

**Fail — 400** missing required fields
```sh
http POST :8000/ticketeer/api/v1/helpdesk/request-types \
  Authorization:"Bearer <admin-token>" \
  name="network_issue"
```
```
400 Bad Request
Invalid request body
```

---

### GET /ticketeer/api/v1/helpdesk/request-types/<id>

**Success — 200**
```sh
http GET :8000/ticketeer/api/v1/helpdesk/request-types/1 \
  Authorization:"Bearer <admin-token>"
```
```json
{
  "id": 1,
  "name": "account_access",
  "category_id": 1,
  "default_priority_id": 2,
  "description": "Account access issue",
  "created_at": "2026-04-04T12:00:00.000000+00"
}
```

**Fail — 404**
```sh
http GET :8000/ticketeer/api/v1/helpdesk/request-types/9999 \
  Authorization:"Bearer <admin-token>"
```
```json
{ "error": "not_found" }
```

---

### PUT /ticketeer/api/v1/helpdesk/request-types/<id>

**Success — 200**
```sh
http PUT :8000/ticketeer/api/v1/helpdesk/request-types/1 \
  Authorization:"Bearer <admin-token>" \
  name="account_access" \
  category_id:=1 \
  default_priority_id:=3 \
  description="Updated description"
```
```
200 OK
```

---

### PATCH /ticketeer/api/v1/helpdesk/request-types/<id>

**Success — 200** partial update
```sh
http PATCH :8000/ticketeer/api/v1/helpdesk/request-types/1 \
  Authorization:"Bearer <admin-token>" \
  default_priority_id:=2
```
```
200 OK
```

---

### DELETE /ticketeer/api/v1/helpdesk/request-types/<id>

**Success — 204**
```sh
http DELETE :8000/ticketeer/api/v1/helpdesk/request-types/3 \
  Authorization:"Bearer <admin-token>"
```
```
204 No Content
```

---

## Profiles

### GET /ticketeer/api/v1/helpdesk/profiles/me

Permission: `LoginRequired`.  
Returns the helpdesk profile associated with the authenticated user's token.

**Success — 200**
```sh
http GET :8000/ticketeer/api/v1/helpdesk/profiles/me \
  Authorization:"Bearer <token>"
```
```json
{
  "id": 3,
  "user_id": 5,
  "department_id": 1,
  "role": "requester",
  "created_at": "2026-04-04T12:00:00.000000+00"
}
```

**Fail — 404** user has no profile
```sh
http GET :8000/ticketeer/api/v1/helpdesk/profiles/me \
  Authorization:"Bearer <noprofile-token>"
```
```json
{ "error": "not_found" }
```

**Fail — 401** no token
```sh
http GET :8000/ticketeer/api/v1/helpdesk/profiles/me
```
```json
{ "error": "unauthorized" }
```

---

## Tickets (admin)

All `/helpdesk/tickets` endpoints require `LoginRequired` + `administrator`.

### GET /ticketeer/api/v1/helpdesk/tickets

Query params: `limit` (default 20, max 100), `cursor` (opaque token from previous response).

**Success — 200** first page
```sh
http GET :8000/ticketeer/api/v1/helpdesk/tickets \
  Authorization:"Bearer <admin-token>"
```
```json
{
  "items": [
    {
      "id": 1,
      "request_type_id": 2,
      "requester_id": 3,
      "assigned_to_id": null,
      "department_id": 1,
      "priority_id": 2,
      "status_id": 1,
      "description": "Printer not working",
      "due_date": null,
      "created_at": "2026-04-04T12:00:00.000000+00",
      "updated_at": "2026-04-04T12:00:00.000000+00"
    }
  ],
  "next": null
}
```

**Success — 200** next page using cursor
```sh
http GET ":8000/ticketeer/api/v1/helpdesk/tickets?cursor=MSwyMDI2LTA..." \
  Authorization:"Bearer <admin-token>"
```

**Fail — 403** non-admin token
```sh
http GET :8000/ticketeer/api/v1/helpdesk/tickets \
  Authorization:"Bearer <requester-token>"
```
```json
{ "error": "forbidden" }
```

---

### POST /ticketeer/api/v1/helpdesk/tickets

Permission: `LoginRequired` + `administrator`

**Success — 201**
```sh
http POST :8000/ticketeer/api/v1/helpdesk/tickets \
  Authorization:"Bearer <admin-token>" \
  request_type_id:=2 \
  requester_id:=3 \
  department_id:=1 \
  priority_id:=2 \
  description="Screen flickering on workstation 4"
```
```json
{ "id": 5 }
```

**Fail — 400** missing fields
```sh
http POST :8000/ticketeer/api/v1/helpdesk/tickets \
  Authorization:"Bearer <admin-token>" \
  description="Missing required fields"
```
```
400 Bad Request
Invalid request body
```

---

**Fail — 400** invalid cursor
```sh
http GET ":8000/ticketeer/api/v1/helpdesk/tickets?cursor=invalid" \
  Authorization:"Bearer <admin-token>"
```
```
400 Bad Request
Invalid cursor
```

---

### GET /ticketeer/api/v1/helpdesk/tickets/<id>

Permission: `LoginRequired` + `administrator`

**Success — 200**
```sh
http GET :8000/ticketeer/api/v1/helpdesk/tickets/1 \
  Authorization:"Bearer <admin-token>"
```
```json
{
  "id": 1,
  "request_type_id": 2,
  "requester_id": 3,
  "assigned_to_id": null,
  "department_id": 1,
  "priority_id": 2,
  "status_id": 1,
  "description": "Printer not working",
  "due_date": null,
  "created_at": "2026-04-04T12:00:00.000000+00",
  "updated_at": "2026-04-04T12:00:00.000000+00"
}
```

**Fail — 404**
```sh
http GET :8000/ticketeer/api/v1/helpdesk/tickets/9999 \
  Authorization:"Bearer <admin-token>"
```
```json
{ "error": "not_found" }
```

---

### PUT /ticketeer/api/v1/helpdesk/tickets/<id>

Permission: `LoginRequired` + `administrator`.  
Full replacement. `assigned_to_id` is optional: omit or send `null` to unassign; send an integer to assign.

**Success — 200** assign a technician
```sh
http PUT :8000/ticketeer/api/v1/helpdesk/tickets/1 \
  Authorization:"Bearer <admin-token>" \
  request_type_id:=2 \
  department_id:=1 \
  priority_id:=3 \
  status_id:=2 \
  assigned_to_id:=2 \
  description="Printer not working — escalated"
```
```
200 OK
```

**Success — 200** unassign
```sh
http PUT :8000/ticketeer/api/v1/helpdesk/tickets/1 \
  Authorization:"Bearer <admin-token>" \
  request_type_id:=2 \
  department_id:=1 \
  priority_id:=3 \
  status_id:=2 \
  assigned_to_id:=null \
  description="Printer not working — escalated"
```
```
200 OK
```

---

### PATCH /ticketeer/api/v1/helpdesk/tickets/<id>

Permission: `LoginRequired` + `administrator`.  
All fields are optional. `assigned_to_id` supports three states: omit (no change), integer (assign), `null` (unassign).

**Success — 200** assign a technician
```sh
http PATCH :8000/ticketeer/api/v1/helpdesk/tickets/1 \
  Authorization:"Bearer <admin-token>" \
  assigned_to_id:=2
```
```
200 OK
```

**Success — 200** unassign
```sh
http PATCH :8000/ticketeer/api/v1/helpdesk/tickets/1 \
  Authorization:"Bearer <admin-token>" \
  assigned_to_id:=null
```
```
200 OK
```

**Success — 200** update priority and status only
```sh
http PATCH :8000/ticketeer/api/v1/helpdesk/tickets/1 \
  Authorization:"Bearer <admin-token>" \
  priority_id:=4 \
  status_id:=3
```
```
200 OK
```

---

### DELETE /ticketeer/api/v1/helpdesk/tickets/<id>

Permission: `LoginRequired` + `administrator`

**Success — 204**
```sh
http DELETE :8000/ticketeer/api/v1/helpdesk/tickets/1 \
  Authorization:"Bearer <admin-token>"
```
```
204 No Content
```

---

## Ticket Request (self-service)

### POST /ticketeer/api/v1/helpdesk/roles/requester/request

Permission: `LoginRequired` (any authenticated user, no role required).  
If the user has no helpdesk profile, one is created automatically with `role = requester`.

**Success — 201**
```sh
http POST :8000/ticketeer/api/v1/helpdesk/roles/requester/request \\
  Authorization:"Bearer <token>" \
  request_type_id:=2 \
  department_id:=1 \
  priority_id:=2 \
  description="My keyboard is not working"
```
```json
{ "id": 6 }
```

**Fail — 400** missing fields
```sh
http POST :8000/ticketeer/api/v1/helpdesk/roles/requester/request \\
  Authorization:"Bearer <token>" \
  description="Forgot required fields"
```
```
400 Bad Request
Invalid request body
```

**Fail — 401** no token
```sh
http POST :8000/ticketeer/api/v1/helpdesk/roles/requester/request \\
  request_type_id:=2 \
  department_id:=1 \
  priority_id:=2 \
  description="Not authenticated"
```
```json
{ "error": "unauthorized" }
```

**Fail — 503** database unreachable
```
503 Service Unavailable
<pg error message>
```

---

### GET /ticketeer/api/v1/helpdesk/tickets/requested_by/<profile_id>

Permission: `LoginRequired`.  
Lists tickets created by the given profile. The profile's user must match the authenticated user's token.

Query params: `limit` (default 20, max 100), `cursor` (opaque token from previous response).

**Success — 200**
```sh
http GET :8000/ticketeer/api/v1/helpdesk/roles/requester/requested_by/3 \\
  Authorization:"Bearer <token>"
```
```json
{
  "items": [
    {
      "id": 6,
      "request_type_id": 2,
      "assigned_to_id": null,
      "department_id": 1,
      "priority_id": 2,
      "status_id": 1,
      "description": "My keyboard is not working",
      "due_date": null,
      "created_at": "2026-04-04T12:00:00.000000+00",
      "updated_at": "2026-04-04T12:00:00.000000+00"
    }
  ],
  "next": null
}
```

**Fail — 403** token does not belong to the profile's user
```sh
http GET :8000/ticketeer/api/v1/helpdesk/roles/requester/requested_by/3 \\
  Authorization:"Bearer <other-user-token>"
```
```json
{ "error": "forbidden" }
```

**Fail — 404** profile does not exist
```sh
http GET :8000/ticketeer/api/v1/helpdesk/roles/requester/requested_by/9999 \\
  Authorization:"Bearer <token>"
```
```json
{ "error": "not_found" }
```

**Fail — 401** no token
```sh
http GET :8000/ticketeer/api/v1/helpdesk/roles/requester/requested_by/3
```
```json
{ "error": "unauthorized" }
```
