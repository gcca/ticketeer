const BASE = '/ticketeer/api/v1'

export type Profile = {
  id: number
  user_id: number
  department_id: number
  role: string
  created_at: string
}

export type Department = {
  id: number
  name: string
  description?: string
  created_at?: string
}

export type Priority = {
  id: number
  name: string
  display_name: string
}

export type RequestType = {
  id: number
  name: string
  category_id: number
  default_priority_id: number
  description: string
  created_at?: string
}

export type Ticket = {
  id: number
  request_type_id: number
  requester_id?: number
  assigned_to_id: number | null
  department_id: number
  priority_id: number
  status_id: number
  description: string
  due_date: string | null
  created_at: string
  updated_at: string
}

export type AdminDashboardTicket = {
  id: number
  description: string
  priority: string
  status: string
  created_at: string
  assigned_to: string | null
  created_by: string
}

export type SupervisorTicketDetail = {
  id: number
  request_type_id: number
  requester_id: number
  assigned_to_id: number | null
  department_id: number
  priority_id: number
  status_id: number
  description: string
  due_date: string | null
  created_at: string
  updated_at: string
}

export type TechnicianProfile = {
  id: number
  user_id: number
  department_id: number
  username: string
}

type SupervisorTechnicianResponse = {
  id: number
  user_id: number
  department_id: number
  role: string
  created_at: string
  user: {
    id: number
    username: string
    created_at: string
  }
}

export type TicketStatus = {
  id: number
  name: string
  display_name: string
  trait: string
}

export type TechnicianTicket = {
  id: number
  description: string
  priority: string
  status: string
  created_at: string
  created_by: string
}

export type DashboardContent = {
  profile: Profile
  departments: Department[]
  priorities: Priority[]
  request_types: RequestType[]
  ticket_statuses: TicketStatus[]
}

type RequestOptions = {
  method?: string
  body?: unknown
}

async function request<T>(path: string, token: string, options: RequestOptions = {}): Promise<T> {
  const headers: Record<string, string> = {
    Authorization: `Bearer ${token}`,
  }
  if (options.body !== undefined) {
    headers['Content-Type'] = 'application/json'
  }

  const res = await fetch(`${BASE}${path}`, {
    method: options.method,
    headers,
    body: options.body !== undefined ? JSON.stringify(options.body) : undefined,
  })
  if (!res.ok) {
    const body = await res.json().catch(() => ({})) as { error?: string }
    throw Object.assign(new Error(body.error ?? res.statusText), { status: res.status })
  }
  if (res.status === 204) {
    return undefined as T
  }
  const text = await res.text()
  if (!text.trim()) {
    return undefined as T
  }
  return JSON.parse(text) as T
}

export async function Signin(username: string, password: string): Promise<string> {
  const res = await fetch(`${BASE}/auth/signin`, {
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

export function GetDashboardContent(token: string) {
  return request<DashboardContent>('/helpdesk/roles/content', token)
}

export function GetTicketsRequestedBy(token: string, profileId: number) {
  return request<{ items: Ticket[] }>(`/helpdesk/roles/requester/requested_by/${profileId}`, token)
    .then(response => response.items)
}

export function ListTickets(token: string) {
  return request<Ticket[]>('/helpdesk/tickets', token)
}

export function GetAdministratorDashboardTickets(token: string) {
  return request<AdminDashboardTicket[]>('/helpdesk/roles/administrator/tickets', token)
}

export function GetSupervisorTickets(token: string, statusId?: number) {
  const query = statusId ? `?ticket_status_id=${statusId}` : ''
  return request<AdminDashboardTicket[]>(`/helpdesk/roles/supervisor/tickets${query}`, token)
}

export function GetSupervisorTechnicians(token: string) {
  return request<SupervisorTechnicianResponse[]>('/helpdesk/roles/supervisor/technicians', token)
    .then(response => response.map(technician => ({
      id: technician.id,
      user_id: technician.user_id,
      department_id: technician.department_id,
      username: technician.user.username,
    })))
}

export function GetSupervisorTicket(token: string, id: number) {
  return request<SupervisorTicketDetail>(`/helpdesk/roles/supervisor/tickets/${id}`, token)
}

export function UpdateSupervisorTicket(
  token: string,
  id: number,
  payload: {
    request_type_id: number
    requester_id: number
    assigned_to_id: number | null
    department_id: number
    priority_id: number
    status_id: number
    description: string
    due_date: string | null
  },
) {
  return request<void>(`/helpdesk/roles/supervisor/tickets/${id}`, token, {
    method: 'PUT',
    body: payload,
  })
}

export function GetAdministratorTechnicians(token: string) {
  return request<TechnicianProfile[]>('/helpdesk/administrator/technicians', token)
}

export function GetTechnicianTickets(token: string) {
  return request<TechnicianTicket[]>('/helpdesk/roles/technician/tickets', token)
}

export function AssignSupervisorTickets(
  token: string,
  payload: Array<{ ticket_id: number; technician_id: number | null }>,
) {
  return request<{ updated: number }>('/helpdesk/roles/supervisor/assign-tickets', token, {
    method: 'POST',
    body: payload,
  })
}

export function CreateTicketRequest(
  token: string,
  payload: {
    request_type_id: number
    department_id: number
    priority_id: number
    description: string
  },
) {
  return request<{ id: number }>('/helpdesk/roles/requester/request', token, {
    method: 'POST',
    body: payload,
  })
}

export function CreateAdminTicket(
  token: string,
  payload: {
    request_type_id: number
    requester_id: number
    department_id: number
    priority_id: number
    description: string
  },
) {
  return request<{ id: number }>('/helpdesk/tickets', token, {
    method: 'POST',
    body: payload,
  })
}

export function PatchTicket(
  token: string,
  id: number,
  payload: {
    assigned_to_id?: number | null
    priority_id?: number
    status_id?: number
  },
) {
  return request<void>(`/helpdesk/tickets/${id}`, token, {
    method: 'PATCH',
    body: payload,
  })
}

export function ListDepartments(token: string) {
  return request<{ items: Department[] }>('/helpdesk/departments', token)
    .then(response => response.items)
}

export function GetDepartment(token: string, id: number) {
  return request<Department>(`/helpdesk/departments/${id}`, token)
}

export function CreateDepartment(token: string, payload: { name: string; description: string }) {
  return request<{ id: number }>('/helpdesk/departments', token, {
    method: 'POST',
    body: payload,
  })
}

export function UpdateDepartment(token: string, id: number, payload: { name: string; description: string }) {
  return request<void>(`/helpdesk/departments/${id}`, token, {
    method: 'PUT',
    body: payload,
  })
}

export function DeleteDepartment(token: string, id: number) {
  return request<void>(`/helpdesk/departments/${id}`, token, {
    method: 'DELETE',
  })
}

export function ListTicketStatuses(token: string) {
  return request<{ items: TicketStatus[] }>('/helpdesk/ticket-statuses', token)
}

export function GetTicketStatus(token: string, id: number) {
  return request<TicketStatus>(`/helpdesk/ticket-statuses/${id}`, token)
}

export function CreateTicketStatus(token: string, payload: { name: string; display_name: string; trait: string }) {
  return request<{ id: number }>('/helpdesk/ticket-statuses', token, {
    method: 'POST',
    body: payload,
  })
}

export function UpdateTicketStatus(token: string, id: number, payload: { name: string; display_name: string; trait: string }) {
  return request<void>(`/helpdesk/ticket-statuses/${id}`, token, {
    method: 'PUT',
    body: payload,
  })
}

export function DeleteTicketStatus(token: string, id: number) {
  return request<void>(`/helpdesk/ticket-statuses/${id}`, token, {
    method: 'DELETE',
  })
}

export function ListPriorities(token: string) {
  return request<{ items: Priority[] }>('/helpdesk/priorities', token)
    .then(response => response.items)
}

export function GetPriority(token: string, id: number) {
  return request<Priority>(`/helpdesk/priorities/${id}`, token)
}

export function CreatePriority(token: string, payload: { name: string; display_name: string }) {
  return request<{ id: number }>('/helpdesk/priorities', token, {
    method: 'POST',
    body: payload,
  })
}

export function UpdatePriority(token: string, id: number, payload: { name: string; display_name: string }) {
  return request<void>(`/helpdesk/priorities/${id}`, token, {
    method: 'PUT',
    body: payload,
  })
}

export function DeletePriority(token: string, id: number) {
  return request<void>(`/helpdesk/priorities/${id}`, token, {
    method: 'DELETE',
  })
}

export function ListRequestTypes(token: string) {
  return request<{ items: RequestType[] }>('/helpdesk/request-types', token)
    .then(response => response.items)
}

export function GetRequestType(token: string, id: number) {
  return request<RequestType>(`/helpdesk/request-types/${id}`, token)
}

export function CreateRequestType(
  token: string,
  payload: {
    name: string
    category_id: number
    default_priority_id: number
    description: string
  },
) {
  return request<{ id: number }>('/helpdesk/request-types', token, {
    method: 'POST',
    body: payload,
  })
}

export function UpdateRequestType(
  token: string,
  id: number,
  payload: {
    name: string
    category_id: number
    default_priority_id: number
    description: string
  },
) {
  return request<void>(`/helpdesk/request-types/${id}`, token, {
    method: 'PUT',
    body: payload,
  })
}

export function DeleteRequestType(token: string, id: number) {
  return request<void>(`/helpdesk/request-types/${id}`, token, {
    method: 'DELETE',
  })
}
