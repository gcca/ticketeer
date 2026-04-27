const AUTH = '/ticketeer/api/auth/v1'
const DASHBOARD = '/ticketeer/api/dashboard/v1'
const REQUESTER = '/ticketeer/api/role/requester/v1'
const SUPERVISOR = '/ticketeer/api/role/supervisor/v1'

export type LandingProfile = {
  id: string
  department_id: string
  role: string
  name: string
  user: {
    id: string
    username: string
  }
}

export type Department = {
  id: string
  name: string
  description: string
}

export type TicketStatus = {
  id: string
  name: string
  display_name: string
  trait: string
}

export type RequestType = {
  id: string
  name: string
  description: string
  category: string
  default_priority: string
}

export type Priority = {
  id: string
  name: string
  display_name: string
}

export type LandingData = {
  profile: LandingProfile
  departments: Department[]
  ticket_statuses: TicketStatus[]
  request_types: RequestType[]
  priorities: Priority[]
  defaults: { status_id: string }
}

export async function Signin(username: string, password: string): Promise<string> {
  const res = await fetch(`${AUTH}/signin`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ username, password }),
  })
  if (!res.ok) {
    const body = await res.json().catch(() => ({})) as { message?: string }
    throw new Error(body.message ?? 'Credenciales incorrectas.')
  }
  const data = await res.json() as { token: string }
  return data.token
}

export type TicketActivity = {
  body: string
  created_at: string
}

export type Ticket = {
  id: string
  description: string
  status_id: string
  created_at: string
  activities: TicketActivity[]
}

export async function GetLanding(token: string): Promise<LandingData> {
  const res = await fetch(`${DASHBOARD}/landing`, {
    headers: { Authorization: `Bearer ${token}` },
  })
  if (!res.ok) {
    const body = await res.json().catch(() => ({})) as { message?: string }
    throw Object.assign(
      new Error(body.message ?? res.statusText),
      { status: res.status },
    )
  }
  return res.json() as Promise<LandingData>
}

export type CreateTicketInput = {
  request_type_id: string
  department_id: string
  priority_id: string
  description: string
  due_date?: string
}

export type CreatedTicket = {
  id: string
  description: string
  created_at: string
}

export async function TicketCreate(token: string, input: CreateTicketInput): Promise<CreatedTicket> {
  const body: Record<string, string> = {
    request_type_id: input.request_type_id,
    department_id: input.department_id,
    priority_id: input.priority_id,
    description: input.description,
  }
  if (input.due_date) body.due_date = input.due_date

  const res = await fetch(`${REQUESTER}/ticket/create`, {
    method: 'POST',
    headers: {
      Authorization: `Bearer ${token}`,
      'Content-Type': 'application/json',
    },
    body: JSON.stringify(body),
  })
  if (!res.ok) {
    const err = await res.json().catch(() => ({})) as { message?: string }
    throw Object.assign(
      new Error(err.message ?? res.statusText),
      { status: res.status },
    )
  }
  return res.json() as Promise<CreatedTicket>
}

export type TicketActivityDetail = {
  id: string
  kind: string
  body: string
  created_at: string
  profile_name: string
}

export type TicketDetail = {
  id: string
  description: string
  created_at: string
  due_date?: string
  assigned_to?: { id: string; name: string; user: { id: string; username: string } }
  department: { id: string; name: string }
  priority: { id: string; display_name: string }
  status: { id: string; display_name: string }
  request_type: { id: string; name: string; description: string }
  requester: { name: string; user: { id: string; username: string } }
  activities: TicketActivityDetail[]
}

export async function TicketDetails(token: string, id: string): Promise<TicketDetail> {
  const res = await fetch(`${REQUESTER}/ticket/${id}/details`, {
    headers: { Authorization: `Bearer ${token}` },
  })
  if (!res.ok) {
    const err = await res.json().catch(() => ({})) as { message?: string }
    throw Object.assign(
      new Error(err.message ?? res.statusText),
      { status: res.status },
    )
  }
  return res.json() as Promise<TicketDetail>
}

export async function ActivityCreateMessage(
  token: string,
  ticket_id: string,
  message: string,
): Promise<TicketActivityDetail> {
  const res = await fetch(`${REQUESTER}/ticket/${ticket_id}/activity/create/message`, {
    method: 'POST',
    headers: {
      Authorization: `Bearer ${token}`,
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({ message }),
  })
  if (!res.ok) {
    const err = await res.json().catch(() => ({})) as { message?: string }
    throw Object.assign(
      new Error(err.message ?? res.statusText),
      { status: res.status },
    )
  }
  return res.json() as Promise<TicketActivityDetail>
}

export async function TicketList(token: string, search?: string): Promise<Ticket[]> {
  const url = new URL(`${REQUESTER}/ticket/list`, window.location.origin)
  if (search) url.searchParams.set('s', search)
  const res = await fetch(url.toString(), {
    headers: { Authorization: `Bearer ${token}` },
  })
  if (!res.ok) {
    const body = await res.json().catch(() => ({})) as { message?: string }
    throw Object.assign(
      new Error(body.message ?? res.statusText),
      { status: res.status },
    )
  }
  return res.json() as Promise<Ticket[]>
}

export async function SupervisorTicketList(token: string, search?: string): Promise<Ticket[]> {
  const url = new URL(`${SUPERVISOR}/ticket/list`, window.location.origin)
  if (search) url.searchParams.set('s', search)
  const res = await fetch(url.toString(), {
    headers: { Authorization: `Bearer ${token}` },
  })
  if (!res.ok) {
    const body = await res.json().catch(() => ({})) as { message?: string }
    throw Object.assign(new Error(body.message ?? res.statusText), { status: res.status })
  }
  return res.json() as Promise<Ticket[]>
}

export async function SupervisorTicketCreate(token: string, input: CreateTicketInput): Promise<CreatedTicket> {
  const body: Record<string, string> = {
    request_type_id: input.request_type_id,
    department_id: input.department_id,
    priority_id: input.priority_id,
    description: input.description,
  }
  if (input.due_date) body.due_date = input.due_date
  const res = await fetch(`${SUPERVISOR}/ticket/create`, {
    method: 'POST',
    headers: { Authorization: `Bearer ${token}`, 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  })
  if (!res.ok) {
    const err = await res.json().catch(() => ({})) as { message?: string }
    throw Object.assign(new Error(err.message ?? res.statusText), { status: res.status })
  }
  return res.json() as Promise<CreatedTicket>
}

export async function SupervisorTicketDetails(token: string, id: string): Promise<TicketDetail> {
  const res = await fetch(`${SUPERVISOR}/ticket/${id}/details`, {
    headers: { Authorization: `Bearer ${token}` },
  })
  if (!res.ok) {
    const err = await res.json().catch(() => ({})) as { message?: string }
    throw Object.assign(new Error(err.message ?? res.statusText), { status: res.status })
  }
  return res.json() as Promise<TicketDetail>
}

export async function SupervisorActivityCreateMessage(
  token: string,
  ticket_id: string,
  message: string,
): Promise<TicketActivityDetail> {
  const res = await fetch(`${SUPERVISOR}/ticket/${ticket_id}/activity/create/message`, {
    method: 'POST',
    headers: { Authorization: `Bearer ${token}`, 'Content-Type': 'application/json' },
    body: JSON.stringify({ message }),
  })
  if (!res.ok) {
    const err = await res.json().catch(() => ({})) as { message?: string }
    throw Object.assign(new Error(err.message ?? res.statusText), { status: res.status })
  }
  return res.json() as Promise<TicketActivityDetail>
}
