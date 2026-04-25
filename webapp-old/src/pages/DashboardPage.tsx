import { type FormEvent, type ReactNode, useEffect, useState } from 'react'
import { Badge } from '@/components/ui/badge'
import { Button } from '@/components/ui/button'
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card'
import { Label } from '@/components/ui/label'
import { AdminContentView } from '@/pages/administrator/AdminContentView'
import { AdminTicketsView } from '@/pages/administrator/AdminTicketsView'
import { RequesterNewTicketView } from '@/pages/requestes/RequesterNewTicketView'
import { RequesterTicketsView } from '@/pages/requestes/RequesterTicketsView'
import {
  type SupervisorTicketFormState,
  SupervisorTicketDetailModal,
} from '@/pages/supervisor/SupervisorTicketDetailModal'
import { SupervisorTicketsView } from '@/pages/supervisor/SupervisorTicketsView'
import { TechnicianTicketsView } from '@/pages/technician/TechnicianTicketsView'
import {
  AssignSupervisorTickets,
  GetTechnicianTickets,
  GetAdministratorDashboardTickets,
  GetAdministratorTechnicians,
  CreateDepartment,
  CreateAdminTicket,
  CreatePriority,
  CreateRequestType,
  CreateTicketStatus,
  CreateTicketRequest,
  DeleteDepartment,
  DeletePriority,
  DeleteRequestType,
  DeleteTicketStatus,
  GetDashboardContent,
  GetDepartment,
  GetPriority,
  GetRequestType,
  GetSupervisorTicket,
  GetSupervisorTechnicians,
  GetSupervisorTickets,
  GetTicketStatus,
  GetTicketsRequestedBy,
  ListDepartments,
  ListPriorities,
  ListRequestTypes,
  ListTicketStatuses,
  PatchTicket,
  UpdateDepartment,
  UpdatePriority,
  UpdateRequestType,
  UpdateSupervisorTicket,
  UpdateTicketStatus,
  type AdminDashboardTicket,
  type DashboardContent,
  type Department,
  type Priority,
  type Profile,
  type RequestType,
  type SupervisorTicketDetail,
  type TechnicianProfile,
  type TechnicianTicket,
  type Ticket,
  type TicketStatus,
} from '@/lib/api'

type Props = {
  token: string
  onSignout: () => void
  path: string
  onNavigate: (path: string, replace?: boolean) => void
}

type State =
  | { status: 'loading' }
  | { status: 'error'; message: string }
  | {
      status: 'ready'
      dashboard: DashboardContent
      tickets: Ticket[]
      adminTickets: AdminDashboardTicket[]
      technicianTickets: TechnicianTicket[]
      technicians: TechnicianProfile[]
      ticketStatuses: TicketStatus[]
    }

type TicketFormState = {
  requestTypeId: string
  departmentId: string
  priorityId: string
  description: string
}

type View = 'dashboard' | 'tickets' | 'create-ticket' | 'content'
type ContentTab = 'departments' | 'priorities' | 'request-types' | 'ticket-statuses'
type ContentMode = 'details' | 'create' | 'edit'

type FlashMessage = {
  id: number
  type: 'success' | 'error'
  text: string
}

type ContentDetail = Department | Priority | RequestType | TicketStatus | null

type ContentFormState = {
  name: string
  description: string
  displayName: string
  trait: string
  categoryId: string
  defaultPriorityId: string
}

const ROLE_LABEL: Record<string, string> = {
  administrator: 'Administrador',
  requester: 'Solicitante',
  supervisor: 'Supervisor',
  technician: 'Técnico',
  agent: 'Agente',
}

const APP_BASE = '/ticketeer/app'
const ROUTES = {
  dashboard: `${APP_BASE}/dashboard`,
  tickets: `${APP_BASE}/tickets`,
  newTicket: `${APP_BASE}/tickets/new`,
  content: (tab: ContentTab) => `${APP_BASE}/content/${tab}`,
}

function isContentTab(value: string): value is ContentTab {
  return value === 'departments'
    || value === 'priorities'
    || value === 'request-types'
    || value === 'ticket-statuses'
}

function parseRoute(path: string, canSeeTickets: boolean, canCreateTickets: boolean, isAdministrator: boolean): { view: View; contentTab: ContentTab } {
  if (path === ROUTES.newTicket && canCreateTickets) {
    return { view: 'create-ticket', contentTab: 'departments' }
  }

  if (path === ROUTES.tickets && canSeeTickets) {
    return { view: 'tickets', contentTab: 'departments' }
  }

  if (path.startsWith(`${APP_BASE}/content/`) && isAdministrator) {
    const tab = path.replace(`${APP_BASE}/content/`, '')
    if (isContentTab(tab)) {
      return { view: 'content', contentTab: tab }
    }
  }

  return { view: 'dashboard', contentTab: 'departments' }
}

function PriorityBadge({ id }: { id: number }) {
  const variants: Record<number, { label: string; variant: 'destructive' | 'default' | 'secondary' | 'outline' }> = {
    1: { label: 'Baja', variant: 'secondary' },
    2: { label: 'Media', variant: 'outline' },
    3: { label: 'Alta', variant: 'default' },
    4: { label: 'Crítica', variant: 'destructive' },
  }
  const { label, variant } = variants[id] ?? { label: `P${id}`, variant: 'outline' as const }
  return <Badge variant={variant}>{label}</Badge>
}

function StatusBadge({ id }: { id: number }) {
  const labels: Record<number, string> = {
    1: 'Abierto',
    2: 'En progreso',
    3: 'Resuelto',
    4: 'Cerrado',
  }
  return <Badge variant="outline">{labels[id] ?? `Estado ${id}`}</Badge>
}

function createInitialTicketFormState(
  profile: Profile,
  requestTypes: RequestType[],
  departments: DashboardContent['departments'],
) {
  const selectedRequestType = requestTypes[0]
  return {
    requestTypeId: selectedRequestType ? String(selectedRequestType.id) : '',
    departmentId: String(departments[0]?.id ?? profile.department_id),
    priorityId: selectedRequestType ? String(selectedRequestType.default_priority_id) : '',
    description: '',
  }
}

function createEmptyContentFormState(): ContentFormState {
  return {
    name: '',
    description: '',
    displayName: '',
    trait: '',
    categoryId: '',
    defaultPriorityId: '',
  }
}

function toDateTimeLocal(value: string | null) {
  if (!value) {
    return ''
  }
  const date = new Date(value)
  if (Number.isNaN(date.getTime())) {
    return ''
  }
  const pad = (part: number) => String(part).padStart(2, '0')
  return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}T${pad(date.getHours())}:${pad(date.getMinutes())}`
}

function createSupervisorTicketFormState(detail: SupervisorTicketDetail): SupervisorTicketFormState {
  return {
    requestTypeId: String(detail.request_type_id),
    requesterId: String(detail.requester_id),
    assignedToId: detail.assigned_to_id ? String(detail.assigned_to_id) : '',
    departmentId: String(detail.department_id),
    priorityId: String(detail.priority_id),
    statusId: String(detail.status_id),
    description: detail.description,
    dueDate: toDateTimeLocal(detail.due_date),
  }
}

function createContentFormState(tab: ContentTab, detail: ContentDetail): ContentFormState {
  if (!detail) {
    return createEmptyContentFormState()
  }

  if (tab === 'departments') {
    const department = detail as Department
    return {
      name: department.name,
      description: department.description ?? '',
      displayName: '',
      trait: '',
      categoryId: '',
      defaultPriorityId: '',
    }
  }

  if (tab === 'priorities') {
    const priority = detail as Priority
    return {
      name: priority.name,
      description: '',
      displayName: priority.display_name,
      trait: '',
      categoryId: '',
      defaultPriorityId: '',
    }
  }

  if (tab === 'ticket-statuses') {
    const ticketStatus = detail as TicketStatus
    return {
      name: ticketStatus.name,
      description: '',
      displayName: ticketStatus.display_name,
      trait: ticketStatus.trait,
      categoryId: '',
      defaultPriorityId: '',
    }
  }

  const requestType = detail as RequestType
  return {
    name: requestType.name,
    description: requestType.description,
    displayName: '',
    trait: '',
    categoryId: String(requestType.category_id),
    defaultPriorityId: String(requestType.default_priority_id),
  }
}

function formatDate(value?: string) {
  if (!value) {
    return 'No disponible'
  }
  return new Date(value).toLocaleDateString('es-ES', {
    day: '2-digit',
    month: 'short',
    year: 'numeric',
  })
}

function NavItem({
  active,
  children,
  onClick,
}: {
  active: boolean
  children: ReactNode
  onClick: () => void
}) {
  return (
    <button
      className={`w-full flex items-center gap-2 rounded-md px-3 py-2 text-sm ${
        active
          ? 'bg-accent text-accent-foreground font-medium'
          : 'text-muted-foreground hover:bg-accent hover:text-accent-foreground'
      }`}
      onClick={onClick}
      type="button"
    >
      {children}
    </button>
  )
}

export function DashboardPage({ token, onSignout, path, onNavigate }: Props) {
  const [state, setState] = useState<State>({ status: 'loading' })
  const [view, setView] = useState<View>('dashboard')
  const [isUserMenuOpen, setIsUserMenuOpen] = useState(false)
  const [ticketForm, setTicketForm] = useState<TicketFormState>({
    requestTypeId: '',
    departmentId: '',
    priorityId: '',
    description: '',
  })
  const [isTicketSubmitting, setIsTicketSubmitting] = useState(false)
  const [flashMessages, setFlashMessages] = useState<FlashMessage[]>([])
  const [adminTicketsPage, setAdminTicketsPage] = useState(1)
  const [supervisorStatusFilter, setSupervisorStatusFilter] = useState('')
  const [supervisorTicketsPage, setSupervisorTicketsPage] = useState(1)
  const [isSupervisorTicketsLoading, setIsSupervisorTicketsLoading] = useState(false)
  const [isTechnicianTicketsLoading, setIsTechnicianTicketsLoading] = useState(false)
  const [activeSupervisorTicketId, setActiveSupervisorTicketId] = useState<number | null>(null)
  const [isSupervisorDetailOpen, setIsSupervisorDetailOpen] = useState(false)
  const [isSupervisorDetailLoading, setIsSupervisorDetailLoading] = useState(false)
  const [isSupervisorDetailSaving, setIsSupervisorDetailSaving] = useState(false)
  const [supervisorTicketDetail, setSupervisorTicketDetail] = useState<SupervisorTicketDetail | null>(null)
  const [supervisorTicketForm, setSupervisorTicketForm] = useState<SupervisorTicketFormState>({
    requestTypeId: '',
    requesterId: '',
    assignedToId: '',
    departmentId: '',
    priorityId: '',
    statusId: '',
    description: '',
    dueDate: '',
  })
  const [assigningTicketId, setAssigningTicketId] = useState<number | null>(null)
  const [isMutationPending, setIsMutationPending] = useState(false)
  const [cursorPosition, setCursorPosition] = useState({ x: 0, y: 0 })
  const [contentTab, setContentTab] = useState<ContentTab>('departments')
  const [contentMode, setContentMode] = useState<ContentMode>('details')
  const [selectedContentId, setSelectedContentId] = useState<number | null>(null)
  const [contentDetail, setContentDetail] = useState<ContentDetail>(null)
  const [contentForm, setContentForm] = useState<ContentFormState>(createEmptyContentFormState())
  const [isContentBusy, setIsContentBusy] = useState(false)

  useEffect(() => {
    GetDashboardContent(token)
      .then(async dashboard => {
        if (dashboard.profile.role === 'administrator') {
          const [adminTickets, technicians] = await Promise.all([
            GetAdministratorDashboardTickets(token),
            GetAdministratorTechnicians(token),
          ])
          return {
            dashboard,
            tickets: [] as Ticket[],
            adminTickets,
            technicianTickets: [] as TechnicianTicket[],
            technicians,
            ticketStatuses: dashboard.ticket_statuses,
          }
        }
        if (dashboard.profile.role === 'supervisor') {
          return {
            dashboard,
            tickets: [] as Ticket[],
            adminTickets: [] as AdminDashboardTicket[],
            technicianTickets: [] as TechnicianTicket[],
            technicians: [] as TechnicianProfile[],
            ticketStatuses: dashboard.ticket_statuses,
          }
        }
        if (dashboard.profile.role === 'technician') {
          return {
            dashboard,
            tickets: [] as Ticket[],
            adminTickets: [] as AdminDashboardTicket[],
            technicianTickets: [] as TechnicianTicket[],
            technicians: [] as TechnicianProfile[],
            ticketStatuses: [] as TicketStatus[],
          }
        }
        const tickets = await GetTicketsRequestedBy(token, dashboard.profile.id)
        return {
          dashboard,
          tickets,
          adminTickets: [] as AdminDashboardTicket[],
          technicianTickets: [] as TechnicianTicket[],
          technicians: [] as TechnicianProfile[],
          ticketStatuses: [] as TicketStatus[],
        }
      })
      .then(({ dashboard, tickets, adminTickets, technicianTickets, technicians, ticketStatuses }) => {
        setState({ status: 'ready', dashboard, tickets, adminTickets, technicianTickets, technicians, ticketStatuses })
        setTicketForm(createInitialTicketFormState(dashboard.profile, dashboard.request_types, dashboard.departments))
        return undefined
      })
      .catch(err => {
        const message = err instanceof Error ? err.message : 'Error al cargar los datos.'
        if ((err as { status?: number }).status === 401) {
          onSignout()
          return
        }
        setState({ status: 'error', message })
      })
  }, [token, onSignout])

  useEffect(() => {
    if (!isMutationPending) {
      return
    }
    setCursorPosition({
      x: window.innerWidth / 2,
      y: window.innerHeight / 2,
    })
  }, [isMutationPending])

  const isReady = state.status === 'ready'
  const dashboard = isReady ? state.dashboard : null
  const tickets = isReady ? state.tickets : []
  const adminTickets = isReady ? state.adminTickets : []
  const technicianTickets = isReady ? state.technicianTickets : []
  const technicians = isReady ? state.technicians : []
  const ticketStatuses = isReady ? state.ticketStatuses : []
  const profile = dashboard?.profile ?? null
  const departments = dashboard?.departments ?? []
  const priorities = dashboard?.priorities ?? []
  const requestTypes = dashboard?.request_types ?? []
  const isAdministrator = profile?.role === 'administrator'
  const isSupervisor = profile?.role === 'supervisor'
  const isTechnician = profile?.role === 'technician'
  const isRequester = profile?.role === 'requester'
  const route = parseRoute(path, isAdministrator || isSupervisor || isTechnician || isRequester, isAdministrator || isRequester, isAdministrator)
  const selectedRequestType = requestTypes.find(type => String(type.id) === ticketForm.requestTypeId)
  const departmentName = profile ? departments.find(department => department.id === profile.department_id)?.name : undefined
  const currentContentItems =
    contentTab === 'departments'
      ? departments
      : contentTab === 'priorities'
        ? priorities
        : contentTab === 'ticket-statuses'
          ? ticketStatuses
          : requestTypes

  useEffect(() => {
    setView(route.view)
    setContentTab(route.contentTab)
  }, [route.contentTab, route.view])

  useEffect(() => {
    const normalizedPath =
      route.view === 'dashboard'
        ? ROUTES.dashboard
        : route.view === 'tickets'
          ? ROUTES.tickets
          : route.view === 'create-ticket'
            ? ROUTES.newTicket
            : ROUTES.content(route.contentTab)

    if (path !== normalizedPath) {
      onNavigate(normalizedPath, true)
    }
  }, [onNavigate, path, route.contentTab, route.view])

  useEffect(() => {
    if (!isReady || !isAdministrator || route.view !== 'content') {
      return
    }

    const firstId =
      route.contentTab === 'departments'
        ? departments[0]?.id ?? null
        : route.contentTab === 'priorities'
          ? priorities[0]?.id ?? null
          : route.contentTab === 'ticket-statuses'
            ? ticketStatuses[0]?.id ?? null
            : requestTypes[0]?.id ?? null

    setContentMode('details')
    setSelectedContentId(firstId)

    if (firstId === null) {
      setContentDetail(null)
      setContentForm(createEmptyContentFormState())
      return
    }

    setIsContentBusy(true)
    void fetchContentDetail(route.contentTab, firstId)
      .catch(err => {
        if ((err as { status?: number }).status === 401) {
          onSignout()
          return
        }
        setFlash('error', err instanceof Error ? err.message : 'No se pudo cargar el detalle.')
      })
      .finally(() => setIsContentBusy(false))
  }, [isReady, route.contentTab, route.view, isAdministrator, departments, priorities, requestTypes, ticketStatuses])

  useEffect(() => {
    if (!isReady || !isSupervisor || route.view !== 'tickets' || technicians.length > 0) {
      return
    }
    void GetSupervisorTechnicians(token)
      .then(nextTechnicians => {
        setState(current =>
          current.status === 'ready'
            ? { ...current, technicians: nextTechnicians }
            : current,
        )
      })
      .catch(err => {
        if ((err as { status?: number }).status === 401) {
          onSignout()
        }
      })
  }, [isReady, isSupervisor, route.view, token, onSignout, technicians.length])

  useEffect(() => {
    if (!isReady || !isSupervisor || route.view !== 'tickets') {
      return
    }

    setIsSupervisorTicketsLoading(true)
    void GetSupervisorTickets(token, supervisorStatusFilter ? Number(supervisorStatusFilter) : undefined)
      .then(nextTickets => {
        setState(current =>
          current.status === 'ready' ? { ...current, adminTickets: nextTickets } : current,
        )
        setSupervisorTicketsPage(1)
      })
      .catch(err => {
        if ((err as { status?: number }).status === 401) {
          onSignout()
          return
        }
        setFlash('error', err instanceof Error ? err.message : 'No se pudieron cargar los tickets.')
      })
      .finally(() => setIsSupervisorTicketsLoading(false))
  }, [isReady, isSupervisor, onSignout, route.view, supervisorStatusFilter, token])

  useEffect(() => {
    setActiveSupervisorTicketId(null)
  }, [supervisorStatusFilter, supervisorTicketsPage])

  useEffect(() => {
    if (!isReady || !isTechnician || route.view !== 'tickets') {
      return
    }
    setIsTechnicianTicketsLoading(true)
    void GetTechnicianTickets(token)
      .then(nextTickets => {
        setState(current =>
          current.status === 'ready' ? { ...current, technicianTickets: nextTickets } : current,
        )
      })
      .catch(err => {
        if ((err as { status?: number }).status === 401) {
          onSignout()
          return
        }
        setFlash('error', err instanceof Error ? err.message : 'No se pudieron cargar los tickets.')
      })
      .finally(() => setIsTechnicianTicketsLoading(false))
  }, [isReady, isTechnician, route.view, token, onSignout])

  if (state.status === 'loading') {
    return (
      <div className="min-h-screen flex items-center justify-center text-muted-foreground text-sm">
        Cargando…
      </div>
    )
  }

  if (state.status === 'error') {
    return (
      <div className="min-h-screen flex items-center justify-center px-4">
        <div role="alert" className="text-sm text-destructive bg-destructive/10 border border-destructive/20 rounded-md px-4 py-3 max-w-sm text-center">
          {state.message}
        </div>
      </div>
    )
  }

  const readyProfile = state.dashboard.profile

  async function refreshCollection(tab: ContentTab) {
    if (!isAdministrator) {
      return
    }

    const list =
      tab === 'departments'
        ? await ListDepartments(token)
        : tab === 'priorities'
          ? await ListPriorities(token)
          : tab === 'ticket-statuses'
            ? (await ListTicketStatuses(token)).items
            : await ListRequestTypes(token)

    setState(current => {
      if (current.status !== 'ready') {
        return current
      }
      return {
        ...current,
        dashboard: {
          ...current.dashboard,
          departments: tab === 'departments' ? list as Department[] : current.dashboard.departments,
          priorities: tab === 'priorities' ? list as Priority[] : current.dashboard.priorities,
          request_types: tab === 'request-types' ? list as RequestType[] : current.dashboard.request_types,
        },
        ticketStatuses: tab === 'ticket-statuses' ? list as TicketStatus[] : current.ticketStatuses,
      }
    })
  }

  async function fetchContentDetail(tab: ContentTab, id: number) {
    const detail =
      tab === 'departments'
        ? await GetDepartment(token, id)
        : tab === 'priorities'
          ? await GetPriority(token, id)
          : tab === 'ticket-statuses'
            ? await GetTicketStatus(token, id)
            : await GetRequestType(token, id)

    setSelectedContentId(id)
    setContentDetail(detail)
    setContentMode('details')
    setContentForm(createContentFormState(tab, detail))
  }

  function setFlash(type: FlashMessage['type'], text: string) {
    const id = Date.now() + Math.random()
    setFlashMessages(current => [...current, { id, type, text }])
    window.setTimeout(() => {
      setFlashMessages(current => current.filter(item => item.id !== id))
    }, 5000)
  }

  function dismissFlash(id: number) {
    setFlashMessages(current => current.filter(item => item.id !== id))
  }

  function handleRequestTypeChange(requestTypeId: string) {
    const requestType = requestTypes.find(type => String(type.id) === requestTypeId)
    setTicketForm(current => ({
      ...current,
      requestTypeId,
      priorityId: requestType ? String(requestType.default_priority_id) : current.priorityId,
    }))
  }

  async function handleCreateTicket(event: FormEvent<HTMLFormElement>) {
    event.preventDefault()
    if (!ticketForm.requestTypeId || !ticketForm.departmentId || !ticketForm.priorityId || !ticketForm.description.trim()) {
      setFlash('error', 'Completa todos los campos del ticket.')
      return
    }

    setIsTicketSubmitting(true)
    setIsMutationPending(true)
    try {
      if (isAdministrator) {
        await CreateAdminTicket(token, {
          request_type_id: Number(ticketForm.requestTypeId),
          requester_id: readyProfile.id,
          department_id: Number(ticketForm.departmentId),
          priority_id: Number(ticketForm.priorityId),
          description: ticketForm.description.trim(),
        })
      } else {
        await CreateTicketRequest(token, {
          request_type_id: Number(ticketForm.requestTypeId),
          department_id: Number(ticketForm.departmentId),
          priority_id: Number(ticketForm.priorityId),
          description: ticketForm.description.trim(),
        })
      }

      const nextTickets = isAdministrator
        ? await GetAdministratorDashboardTickets(token)
        : await GetTicketsRequestedBy(token, readyProfile.id)
      setState(current => {
        if (current.status !== 'ready') {
          return current
        }
        return isAdministrator
          ? { ...current, adminTickets: nextTickets as AdminDashboardTicket[] }
          : { ...current, tickets: nextTickets as Ticket[] }
      })
      setTicketForm(createInitialTicketFormState(readyProfile, requestTypes, departments))
      onNavigate(ROUTES.tickets)
      if (isAdministrator) {
        setAdminTicketsPage(1)
      }
      setFlash('success', 'Ticket creado correctamente.')
    } catch (err) {
      if ((err as { status?: number }).status === 401) {
        onSignout()
        return
      }
      setFlash('error', err instanceof Error ? err.message : 'No se pudo crear el ticket.')
    } finally {
      setIsTicketSubmitting(false)
      setIsMutationPending(false)
    }
  }

  async function handleChangeContentTab(tab: ContentTab) {
    onNavigate(ROUTES.content(tab))
  }

  async function handleSelectContentItem(id: number) {
    setIsContentBusy(true)
    try {
      await fetchContentDetail(contentTab, id)
    } catch (err) {
      if ((err as { status?: number }).status === 401) {
        onSignout()
        return
      }
      setFlash('error', err instanceof Error ? err.message : 'No se pudo cargar el detalle.')
    } finally {
      setIsContentBusy(false)
    }
  }

  function handleCreateContent() {
    setContentMode('create')
    setSelectedContentId(null)
    setContentDetail(null)
    setContentForm(
        contentTab === 'request-types'
        ? {
            ...createEmptyContentFormState(),
            defaultPriorityId: priorities[0] ? String(priorities[0].id) : '',
          }
        : contentTab === 'ticket-statuses'
          ? {
              ...createEmptyContentFormState(),
              trait: 'open',
            }
        : createEmptyContentFormState(),
    )
  }

  function handleEditContent() {
    setContentMode('edit')
    setContentForm(createContentFormState(contentTab, contentDetail))
  }

  async function handleDeleteContent() {
    if (!selectedContentId) {
      return
    }
    const confirmed = window.confirm('Esta acción eliminará el registro seleccionado. ¿Deseas continuar?')
    if (!confirmed) {
      return
    }

    setIsContentBusy(true)
    setIsMutationPending(true)
    try {
      if (contentTab === 'departments') {
        await DeleteDepartment(token, selectedContentId)
      } else if (contentTab === 'priorities') {
        await DeletePriority(token, selectedContentId)
      } else if (contentTab === 'ticket-statuses') {
        await DeleteTicketStatus(token, selectedContentId)
      } else {
        await DeleteRequestType(token, selectedContentId)
      }

      await refreshCollection(contentTab)

      const nextItems =
        contentTab === 'departments'
          ? departments.filter(item => item.id !== selectedContentId)
          : contentTab === 'priorities'
            ? priorities.filter(item => item.id !== selectedContentId)
            : contentTab === 'ticket-statuses'
              ? ticketStatuses.filter(item => item.id !== selectedContentId)
              : requestTypes.filter(item => item.id !== selectedContentId)

      const nextId = nextItems[0]?.id ?? null
      setSelectedContentId(nextId)
      setContentDetail(null)
      setContentForm(createEmptyContentFormState())
      setContentMode('details')

      if (nextId !== null) {
        await fetchContentDetail(contentTab, nextId)
      }

      setFlash('success', 'Registro eliminado correctamente.')
    } catch (err) {
      if ((err as { status?: number }).status === 401) {
        onSignout()
        return
      }
      setFlash('error', err instanceof Error ? err.message : 'No se pudo eliminar el registro.')
    } finally {
      setIsContentBusy(false)
      setIsMutationPending(false)
    }
  }

  async function handleSubmitContent(event: FormEvent<HTMLFormElement>) {
    event.preventDefault()

    if (!contentForm.name.trim()) {
      setFlash('error', 'El campo nombre es obligatorio.')
      return
    }
    if (contentTab === 'departments' && !contentForm.description.trim()) {
      setFlash('error', 'La descripción es obligatoria.')
      return
    }
    if (contentTab === 'priorities' && !contentForm.displayName.trim()) {
      setFlash('error', 'El nombre visible es obligatorio.')
      return
    }
    if (contentTab === 'ticket-statuses' && (!contentForm.displayName.trim() || !contentForm.trait.trim())) {
      setFlash('error', 'Completa nombre visible y trait del estado.')
      return
    }
    if (contentTab === 'request-types' && (!contentForm.description.trim() || !contentForm.categoryId || !contentForm.defaultPriorityId)) {
      setFlash('error', 'Completa todos los campos del tipo de solicitud.')
      return
    }

    setIsContentBusy(true)
    try {
      let createdId: number | null = null

      if (contentTab === 'departments') {
        const payload = {
          name: contentForm.name.trim(),
          description: contentForm.description.trim(),
        }
        if (contentMode === 'create') {
          const result = await CreateDepartment(token, payload)
          createdId = result.id
        } else if (selectedContentId) {
          await UpdateDepartment(token, selectedContentId, payload)
        }
      } else if (contentTab === 'priorities') {
        const payload = {
          name: contentForm.name.trim(),
          display_name: contentForm.displayName.trim(),
        }
        if (contentMode === 'create') {
          const result = await CreatePriority(token, payload)
          createdId = result.id
        } else if (selectedContentId) {
          await UpdatePriority(token, selectedContentId, payload)
        }
      } else if (contentTab === 'ticket-statuses') {
        const payload = {
          name: contentForm.name.trim(),
          display_name: contentForm.displayName.trim(),
          trait: contentForm.trait.trim(),
        }
        if (contentMode === 'create') {
          const result = await CreateTicketStatus(token, payload)
          createdId = result.id
        } else if (selectedContentId) {
          await UpdateTicketStatus(token, selectedContentId, payload)
        }
      } else {
        const payload = {
          name: contentForm.name.trim(),
          category_id: Number(contentForm.categoryId),
          default_priority_id: Number(contentForm.defaultPriorityId),
          description: contentForm.description.trim(),
        }
        if (contentMode === 'create') {
          const result = await CreateRequestType(token, payload)
          createdId = result.id
        } else if (selectedContentId) {
          await UpdateRequestType(token, selectedContentId, payload)
        }
      }

      await refreshCollection(contentTab)

      const detailId = createdId ?? selectedContentId
      if (detailId) {
        await fetchContentDetail(contentTab, detailId)
      } else {
        setContentMode('details')
      }

      setFlash('success', contentMode === 'create' ? 'Registro creado correctamente.' : 'Registro actualizado correctamente.')
    } catch (err) {
      if ((err as { status?: number }).status === 401) {
        onSignout()
        return
      }
      setFlash('error', err instanceof Error ? err.message : 'No se pudo guardar el registro.')
    } finally {
      setIsContentBusy(false)
    }
  }

  function renderProfileView() {
    return (
      <Card>
        <CardHeader>
          <CardTitle className="text-base">Mi perfil</CardTitle>
          <CardDescription>Información disponible desde `roles/content`.</CardDescription>
        </CardHeader>
        <CardContent className="grid grid-cols-2 gap-4 text-sm sm:grid-cols-4">
          <div>
            <p className="text-muted-foreground mb-1">ID de perfil</p>
            <p className="font-medium">{readyProfile.id}</p>
          </div>
          <div>
            <p className="text-muted-foreground mb-1">Rol</p>
            <Badge variant="secondary">{ROLE_LABEL[readyProfile.role] ?? readyProfile.role}</Badge>
          </div>
          <div>
            <p className="text-muted-foreground mb-1">Departamento</p>
            <p className="font-medium">{departmentName ?? `#${readyProfile.department_id}`}</p>
          </div>
          <div>
            <p className="text-muted-foreground mb-1">Miembro desde</p>
            <p className="font-medium">{formatDate(readyProfile.created_at)}</p>
          </div>
        </CardContent>
      </Card>
    )
  }

  function renderCreateTicketView() {
    if (!isAdministrator) {
      return (
        <RequesterNewTicketView
          form={ticketForm}
          priorities={priorities}
          departments={departments}
          requestTypes={requestTypes}
          selectedRequestType={selectedRequestType}
          isSubmitting={isTicketSubmitting}
          onRequestTypeChange={handleRequestTypeChange}
          onDepartmentChange={departmentId => setTicketForm(current => ({ ...current, departmentId }))}
          onPriorityChange={priorityId => setTicketForm(current => ({ ...current, priorityId }))}
          onDescriptionChange={description => setTicketForm(current => ({ ...current, description }))}
          onCancel={() => onNavigate(ROUTES.tickets)}
          onSubmit={handleCreateTicket}
        />
      )
    }

    return (
      <Card>
        <CardHeader>
          <CardTitle className="text-base">Nuevo ticket</CardTitle>
          <CardDescription>
            {isAdministrator
              ? 'Crea un ticket usando los endpoints administrativos.'
              : 'Completa la solicitud y guárdala.'}
          </CardDescription>
        </CardHeader>
        <CardContent>
          <form className="space-y-4" onSubmit={handleCreateTicket}>
            <div className="grid gap-4 md:grid-cols-3">
              <div className="space-y-2">
                <Label htmlFor="requestType">Tipo de solicitud</Label>
                <select
                  id="requestType"
                  className="flex h-8 w-full rounded-lg border border-input bg-transparent px-2.5 py-1 text-sm outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50"
                  value={ticketForm.requestTypeId}
                  onChange={event => handleRequestTypeChange(event.target.value)}
                  disabled={isTicketSubmitting}
                >
                  <option value="" disabled>Selecciona un tipo</option>
                  {requestTypes.map(requestType => (
                    <option key={requestType.id} value={requestType.id}>
                      {requestType.description}
                    </option>
                  ))}
                </select>
              </div>

              <div className="space-y-2">
                <Label htmlFor="department">Departamento</Label>
                <select
                  id="department"
                  className="flex h-8 w-full rounded-lg border border-input bg-transparent px-2.5 py-1 text-sm outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50"
                  value={ticketForm.departmentId}
                  onChange={event => setTicketForm(current => ({ ...current, departmentId: event.target.value }))}
                  disabled={isTicketSubmitting}
                >
                  <option value="" disabled>Selecciona un departamento</option>
                  {departments.map(department => (
                    <option key={department.id} value={department.id}>
                      {department.name}
                    </option>
                  ))}
                </select>
              </div>

              <div className="space-y-2">
                <Label htmlFor="priority">Prioridad</Label>
                <select
                  id="priority"
                  className="flex h-8 w-full rounded-lg border border-input bg-transparent px-2.5 py-1 text-sm outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50"
                  value={ticketForm.priorityId}
                  onChange={event => setTicketForm(current => ({ ...current, priorityId: event.target.value }))}
                  disabled={isTicketSubmitting}
                >
                  <option value="" disabled>Selecciona una prioridad</option>
                  {priorities.map(priority => (
                    <option key={priority.id} value={priority.id}>
                      {priority.display_name}
                    </option>
                  ))}
                </select>
              </div>
            </div>

            <div className="space-y-2">
              <Label htmlFor="description">Descripción</Label>
              <textarea
                id="description"
                className="flex min-h-28 w-full rounded-lg border border-input bg-transparent px-3 py-2 text-sm outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50 disabled:cursor-not-allowed disabled:opacity-50"
                value={ticketForm.description}
                onChange={event => setTicketForm(current => ({ ...current, description: event.target.value }))}
                placeholder={selectedRequestType?.description ?? 'Describe el problema'}
                disabled={isTicketSubmitting}
              />
            </div>

            <div className="flex flex-col gap-3 sm:flex-row sm:items-center sm:justify-between">
              <div className="text-sm text-muted-foreground">
                {selectedRequestType
                  ? `Prioridad sugerida: ${priorities.find(priority => priority.id === selectedRequestType.default_priority_id)?.display_name ?? selectedRequestType.default_priority_id}`
                  : 'Selecciona un tipo de solicitud.'}
              </div>
              <div className="flex gap-2">
                <Button type="button" variant="outline" onClick={() => onNavigate(ROUTES.tickets)} disabled={isTicketSubmitting}>
                  Cancelar
                </Button>
                <Button type="submit" disabled={isTicketSubmitting}>
                  {isTicketSubmitting ? 'Guardando…' : 'Guardar'}
                </Button>
              </div>
            </div>
          </form>
        </CardContent>
      </Card>
    )
  }

  async function openSupervisorTicketDetail(ticketId: number) {
    setIsSupervisorDetailOpen(true)
    setIsSupervisorDetailLoading(true)
    try {
      const detail = await GetSupervisorTicket(token, ticketId)
      setSupervisorTicketDetail(detail)
      setSupervisorTicketForm(createSupervisorTicketFormState(detail))
    } catch (err) {
      if ((err as { status?: number }).status === 401) {
        onSignout()
        return
      }
      setIsSupervisorDetailOpen(false)
      setFlash('error', err instanceof Error ? err.message : 'No se pudo cargar el detalle del ticket.')
    } finally {
      setIsSupervisorDetailLoading(false)
    }
  }

  async function handleSaveSupervisorTicketDetail() {
    if (!supervisorTicketDetail) {
      return
    }
    if (
      !supervisorTicketForm.requestTypeId
      || !supervisorTicketForm.requesterId
      || !supervisorTicketForm.departmentId
      || !supervisorTicketForm.priorityId
      || !supervisorTicketForm.statusId
      || !supervisorTicketForm.description.trim()
    ) {
      setFlash('error', 'Completa los campos obligatorios del ticket.')
      return
    }

    setIsSupervisorDetailSaving(true)
    setIsMutationPending(true)
    try {
      await UpdateSupervisorTicket(token, supervisorTicketDetail.id, {
        request_type_id: Number(supervisorTicketForm.requestTypeId),
        requester_id: Number(supervisorTicketForm.requesterId),
        assigned_to_id: supervisorTicketForm.assignedToId ? Number(supervisorTicketForm.assignedToId) : null,
        department_id: Number(supervisorTicketForm.departmentId),
        priority_id: Number(supervisorTicketForm.priorityId),
        status_id: Number(supervisorTicketForm.statusId),
        description: supervisorTicketForm.description.trim(),
        due_date: supervisorTicketForm.dueDate ? new Date(supervisorTicketForm.dueDate).toISOString() : null,
      })
      const [detail, nextSupervisorTickets] = await Promise.all([
        GetSupervisorTicket(token, supervisorTicketDetail.id),
        GetSupervisorTickets(token, supervisorStatusFilter ? Number(supervisorStatusFilter) : undefined),
      ])
      setSupervisorTicketDetail(detail)
      setSupervisorTicketForm(createSupervisorTicketFormState(detail))
      setState(current =>
        current.status === 'ready' ? { ...current, adminTickets: nextSupervisorTickets } : current,
      )
      setFlash('success', 'Ticket actualizado correctamente.')
    } catch (err) {
      if ((err as { status?: number }).status === 401) {
        onSignout()
        return
      }
      setFlash('error', err instanceof Error ? err.message : 'No se pudo actualizar el ticket.')
    } finally {
      setIsSupervisorDetailSaving(false)
      setIsMutationPending(false)
    }
  }

  function renderTicketsView() {
    async function handleAssignmentChange(ticketId: number, assignedToId: string) {
      setAssigningTicketId(ticketId)
      setIsMutationPending(true)
      try {
        await PatchTicket(token, ticketId, {
          assigned_to_id: assignedToId ? Number(assignedToId) : null,
        })
        const nextAdminTickets = await GetAdministratorDashboardTickets(token)
        setState(current =>
          current.status === 'ready' ? { ...current, adminTickets: nextAdminTickets } : current,
        )
        setFlash('success', 'Asignación actualizada correctamente.')
      } catch (err) {
        if ((err as { status?: number }).status === 401) {
          onSignout()
          return
        }
        setFlash('error', err instanceof Error ? err.message : 'No se pudo actualizar la asignación.')
      } finally {
        setAssigningTicketId(null)
        setIsMutationPending(false)
      }
    }

    async function handleSupervisorAssignmentChange(ticketId: number, technicianId: string) {
      setAssigningTicketId(ticketId)
      setIsMutationPending(true)
      try {
        await AssignSupervisorTickets(token, [{
          ticket_id: ticketId,
          technician_id: technicianId ? Number(technicianId) : null,
        }])
        const nextSupervisorTickets = await GetSupervisorTickets(
          token,
          supervisorStatusFilter ? Number(supervisorStatusFilter) : undefined,
        )
        setState(current =>
          current.status === 'ready' ? { ...current, adminTickets: nextSupervisorTickets } : current,
        )
        setFlash('success', 'Asignación actualizada correctamente.')
      } catch (err) {
        if ((err as { status?: number }).status === 401) {
          onSignout()
          return
        }
        setFlash('error', err instanceof Error ? err.message : 'No se pudo actualizar la asignación.')
      } finally {
        setAssigningTicketId(null)
        setIsMutationPending(false)
      }
    }

    if (isAdministrator) {
      return (
        <AdminTicketsView
          adminTickets={adminTickets}
          technicians={technicians}
          assigningTicketId={assigningTicketId}
          adminTicketsPage={adminTicketsPage}
          onAdminTicketsPageChange={setAdminTicketsPage}
          onAssignmentChange={(ticketId, assignedToId) => {
            void handleAssignmentChange(ticketId, assignedToId)
          }}
          onCreateTicket={() => onNavigate(ROUTES.newTicket)}
          formatDate={formatDate}
        />
      )
    }

    if (isSupervisor) {
      return (
        <SupervisorTicketsView
          tickets={adminTickets}
          technicians={technicians}
          ticketStatuses={ticketStatuses}
          statusFilter={supervisorStatusFilter}
          onStatusFilterChange={value => setSupervisorStatusFilter(value)}
          activeTicketId={activeSupervisorTicketId}
          onTicketClick={ticketId => setActiveSupervisorTicketId(current => current === ticketId ? null : ticketId)}
          onOpenDetails={ticketId => {
            void openSupervisorTicketDetail(ticketId)
          }}
          page={supervisorTicketsPage}
          onPageChange={setSupervisorTicketsPage}
          isLoading={isSupervisorTicketsLoading}
          assigningTicketId={assigningTicketId}
          onAssignmentChange={(ticketId, technicianId) => {
            void handleSupervisorAssignmentChange(ticketId, technicianId)
          }}
          formatDate={formatDate}
        />
      )
    }

    if (isTechnician) {
      return (
        <TechnicianTicketsView
          tickets={technicianTickets}
          isLoading={isTechnicianTicketsLoading}
          formatDate={formatDate}
        />
      )
    }

    return (
      <RequesterTicketsView
        tickets={tickets}
        onCreateTicket={() => onNavigate(ROUTES.newTicket)}
        formatDate={formatDate}
        renderPriority={id => <PriorityBadge id={id} />}
        renderStatus={id => <StatusBadge id={id} />}
      />
    )
  }

  function renderContentList() {
    return (
      <Card>
        <CardHeader>
          <CardTitle className="text-base">Registros</CardTitle>
          <CardDescription>
            {contentTab === 'departments'
              ? 'Departamentos disponibles.'
              : contentTab === 'priorities'
                ? 'Prioridades disponibles.'
                : contentTab === 'ticket-statuses'
                  ? 'Estados de ticket disponibles.'
                  : 'Tipos de solicitud disponibles.'}
          </CardDescription>
        </CardHeader>
        <CardContent className="space-y-2">
          {currentContentItems.length === 0 ? (
            <p className="text-sm text-muted-foreground">No hay registros.</p>
          ) : (
            currentContentItems.map(item => (
              <button
                key={item.id}
                className={`w-full rounded-lg border px-3 py-3 text-left ${
                  selectedContentId === item.id
                    ? 'border-primary bg-primary/5'
                    : 'border-input hover:bg-accent'
                }`}
                onClick={() => void handleSelectContentItem(item.id)}
                type="button"
              >
                <div className="font-medium">
                  {contentTab === 'departments'
                    ? (item as Department).name
                    : contentTab === 'priorities'
                      ? (item as Priority).display_name
                      : contentTab === 'ticket-statuses'
                        ? (item as TicketStatus).display_name
                        : (item as RequestType).name}
                </div>
                <div className="text-xs text-muted-foreground">
                  ID {item.id}
                </div>
              </button>
            ))
          )}
        </CardContent>
      </Card>
    )
  }

  function renderContentDetail() {
    if (contentMode === 'create' || contentMode === 'edit') {
      return (
        <Card>
          <CardHeader>
            <CardTitle className="text-base">
              {contentMode === 'create' ? 'Crear registro' : 'Editar registro'}
            </CardTitle>
            <CardDescription>
              {contentTab === 'departments'
                ? 'Gestiona el catálogo de departamentos.'
                : contentTab === 'priorities'
                  ? 'Gestiona el catálogo de prioridades.'
                  : contentTab === 'ticket-statuses'
                    ? 'Gestiona el catálogo de estados de ticket.'
                    : 'Gestiona el catálogo de tipos de solicitud.'}
            </CardDescription>
          </CardHeader>
          <CardContent>
            <form className="space-y-4" onSubmit={handleSubmitContent}>
              <div className="space-y-2">
                <Label htmlFor="content-name">Nombre</Label>
                <input
                  id="content-name"
                  className="flex h-8 w-full rounded-lg border border-input bg-transparent px-2.5 py-1 text-sm outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50"
                  value={contentForm.name}
                  onChange={event => setContentForm(current => ({ ...current, name: event.target.value }))}
                  disabled={isContentBusy}
                />
              </div>

              {contentTab === 'departments' ? (
                <div className="space-y-2">
                  <Label htmlFor="content-description">Descripción</Label>
                  <textarea
                    id="content-description"
                    className="flex min-h-28 w-full rounded-lg border border-input bg-transparent px-3 py-2 text-sm outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50"
                    value={contentForm.description}
                    onChange={event => setContentForm(current => ({ ...current, description: event.target.value }))}
                    disabled={isContentBusy}
                  />
                </div>
              ) : null}

              {contentTab === 'priorities' ? (
                <div className="space-y-2">
                  <Label htmlFor="content-display-name">Nombre visible</Label>
                  <input
                    id="content-display-name"
                    className="flex h-8 w-full rounded-lg border border-input bg-transparent px-2.5 py-1 text-sm outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50"
                    value={contentForm.displayName}
                    onChange={event => setContentForm(current => ({ ...current, displayName: event.target.value }))}
                    disabled={isContentBusy}
                  />
                </div>
              ) : null}

              {contentTab === 'ticket-statuses' ? (
                <>
                  <div className="space-y-2">
                    <Label htmlFor="content-display-name">Nombre visible</Label>
                    <input
                      id="content-display-name"
                      className="flex h-8 w-full rounded-lg border border-input bg-transparent px-2.5 py-1 text-sm outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50"
                      value={contentForm.displayName}
                      onChange={event => setContentForm(current => ({ ...current, displayName: event.target.value }))}
                      disabled={isContentBusy}
                    />
                  </div>
                  <div className="space-y-2">
                    <Label htmlFor="content-trait">Trait</Label>
                    <select
                      id="content-trait"
                      className="flex h-8 w-full rounded-lg border border-input bg-transparent px-2.5 py-1 text-sm outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50"
                      value={contentForm.trait}
                      onChange={event => setContentForm(current => ({ ...current, trait: event.target.value }))}
                      disabled={isContentBusy}
                    >
                      <option value="open">open</option>
                      <option value="in_progress">in_progress</option>
                      <option value="closed">closed</option>
                    </select>
                  </div>
                </>
              ) : null}

              {contentTab === 'request-types' ? (
                <>
                  <div className="space-y-2">
                    <Label htmlFor="content-category">Category ID</Label>
                    <input
                      id="content-category"
                      type="number"
                      className="flex h-8 w-full rounded-lg border border-input bg-transparent px-2.5 py-1 text-sm outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50"
                      value={contentForm.categoryId}
                      onChange={event => setContentForm(current => ({ ...current, categoryId: event.target.value }))}
                      disabled={isContentBusy}
                    />
                  </div>
                  <div className="space-y-2">
                    <Label htmlFor="content-default-priority">Prioridad por defecto</Label>
                    <select
                      id="content-default-priority"
                      className="flex h-8 w-full rounded-lg border border-input bg-transparent px-2.5 py-1 text-sm outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50"
                      value={contentForm.defaultPriorityId}
                      onChange={event => setContentForm(current => ({ ...current, defaultPriorityId: event.target.value }))}
                      disabled={isContentBusy}
                    >
                      <option value="" disabled>Selecciona una prioridad</option>
                      {priorities.map(priority => (
                        <option key={priority.id} value={priority.id}>
                          {priority.display_name}
                        </option>
                      ))}
                    </select>
                  </div>
                  <div className="space-y-2">
                    <Label htmlFor="content-request-description">Descripción</Label>
                    <textarea
                      id="content-request-description"
                      className="flex min-h-28 w-full rounded-lg border border-input bg-transparent px-3 py-2 text-sm outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50"
                      value={contentForm.description}
                      onChange={event => setContentForm(current => ({ ...current, description: event.target.value }))}
                      disabled={isContentBusy}
                    />
                  </div>
                </>
              ) : null}

              <div className="flex gap-2">
                <Button type="button" variant="outline" onClick={() => {
                  setContentMode('details')
                  setContentForm(createContentFormState(contentTab, contentDetail))
                }} disabled={isContentBusy}>
                  Cancelar
                </Button>
                <Button type="submit" disabled={isContentBusy}>
                  {isContentBusy ? 'Guardando…' : 'Guardar'}
                </Button>
              </div>
            </form>
          </CardContent>
        </Card>
      )
    }

    if (!contentDetail) {
      return (
        <Card>
          <CardHeader>
            <CardTitle className="text-base">Detalle</CardTitle>
            <CardDescription>Selecciona un registro o crea uno nuevo.</CardDescription>
          </CardHeader>
          <CardContent>
            <Button type="button" onClick={handleCreateContent}>
              Crear
            </Button>
          </CardContent>
        </Card>
      )
    }

    return (
      <Card>
        <CardHeader>
          <CardTitle className="text-base">Detalle</CardTitle>
          <CardDescription>Consulta, edita o elimina el registro seleccionado.</CardDescription>
        </CardHeader>
        <CardContent className="space-y-4 text-sm">
          {contentTab === 'departments' ? (
            <>
              <div><span className="text-muted-foreground">ID:</span> {(contentDetail as Department).id}</div>
              <div><span className="text-muted-foreground">Nombre:</span> {(contentDetail as Department).name}</div>
              <div><span className="text-muted-foreground">Descripción:</span> {(contentDetail as Department).description ?? 'Sin descripción'}</div>
              <div><span className="text-muted-foreground">Creado:</span> {formatDate((contentDetail as Department).created_at)}</div>
            </>
          ) : null}

          {contentTab === 'priorities' ? (
            <>
              <div><span className="text-muted-foreground">ID:</span> {(contentDetail as Priority).id}</div>
              <div><span className="text-muted-foreground">Nombre interno:</span> {(contentDetail as Priority).name}</div>
              <div><span className="text-muted-foreground">Nombre visible:</span> {(contentDetail as Priority).display_name}</div>
            </>
          ) : null}

          {contentTab === 'request-types' ? (
            <>
              <div><span className="text-muted-foreground">ID:</span> {(contentDetail as RequestType).id}</div>
              <div><span className="text-muted-foreground">Nombre:</span> {(contentDetail as RequestType).name}</div>
              <div><span className="text-muted-foreground">Category ID:</span> {(contentDetail as RequestType).category_id}</div>
              <div><span className="text-muted-foreground">Prioridad por defecto:</span> {(contentDetail as RequestType).default_priority_id}</div>
              <div><span className="text-muted-foreground">Descripción:</span> {(contentDetail as RequestType).description}</div>
              <div><span className="text-muted-foreground">Creado:</span> {formatDate((contentDetail as RequestType).created_at)}</div>
            </>
          ) : null}

          {contentTab === 'ticket-statuses' ? (
            <>
              <div><span className="text-muted-foreground">ID:</span> {(contentDetail as TicketStatus).id}</div>
              <div><span className="text-muted-foreground">Nombre interno:</span> {(contentDetail as TicketStatus).name}</div>
              <div><span className="text-muted-foreground">Nombre visible:</span> {(contentDetail as TicketStatus).display_name}</div>
              <div><span className="text-muted-foreground">Trait:</span> {(contentDetail as TicketStatus).trait}</div>
            </>
          ) : null}

          <div className="flex gap-2">
            <Button type="button" variant="outline" onClick={handleEditContent} disabled={isContentBusy}>
              Editar
            </Button>
            <Button type="button" variant="destructive" onClick={() => void handleDeleteContent()} disabled={isContentBusy}>
              Eliminar
            </Button>
          </div>
        </CardContent>
      </Card>
    )
  }

  function renderContentView() {
    return (
      <AdminContentView
        contentTab={contentTab}
        onChangeContentTab={tab => {
          void handleChangeContentTab(tab)
        }}
        onCreate={handleCreateContent}
        listContent={renderContentList()}
        detailContent={renderContentDetail()}
      />
    )
  }

  function renderMainContent() {
    if (view === 'dashboard') {
      return renderProfileView()
    }
    if (view === 'tickets') {
      return renderTicketsView()
    }
    if (view === 'create-ticket') {
      return renderCreateTicketView()
    }
    return renderContentView()
  }

  return (
    <div className="flex h-full overflow-hidden bg-muted/40">
      {isMutationPending ? (
        <div
          aria-hidden="true"
          className="fixed inset-0 z-50 bg-transparent"
          onMouseMove={event => setCursorPosition({ x: event.clientX, y: event.clientY })}
        >
          <div
            className="pointer-events-none fixed"
            style={{
              left: cursorPosition.x + 14,
              top: cursorPosition.y + 14,
            }}
          >
            <div className="size-5 animate-spin rounded-full border-2 border-primary/25 border-t-primary" />
          </div>
        </div>
      ) : null}

      <aside className="w-56 shrink-0 flex flex-col bg-background border-r">
        <div className="flex items-center gap-2 px-4 py-5 border-b">
          <div className="inline-flex items-center justify-center size-8 rounded-lg bg-primary text-primary-foreground">
            <svg
              xmlns="http://www.w3.org/2000/svg"
              width="16"
              height="16"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              strokeWidth="2"
              strokeLinecap="round"
              strokeLinejoin="round"
              aria-hidden="true"
            >
              <path d="M15 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V7Z" />
              <path d="M14 2v4a2 2 0 0 0 2 2h4" />
              <path d="M10 9H8" />
              <path d="M16 13H8" />
              <path d="M16 17H8" />
            </svg>
          </div>
          <span className="font-semibold text-sm">Ticketeer</span>
        </div>

        <nav className="flex-1 px-3 py-4 space-y-1">
          <NavItem active={view === 'dashboard'} onClick={() => onNavigate(ROUTES.dashboard)}>
            <svg xmlns="http://www.w3.org/2000/svg" width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">
              <rect width="7" height="9" x="3" y="3" rx="1" />
              <rect width="7" height="5" x="14" y="3" rx="1" />
              <rect width="7" height="9" x="14" y="12" rx="1" />
              <rect width="7" height="5" x="3" y="16" rx="1" />
            </svg>
            Dashboard
          </NavItem>
          {isAdministrator || isSupervisor || isTechnician || isRequester ? (
            <NavItem active={view === 'tickets' || view === 'create-ticket'} onClick={() => onNavigate(ROUTES.tickets)}>
              <svg xmlns="http://www.w3.org/2000/svg" width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">
                <path d="M15 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V7Z" />
                <path d="M14 2v4a2 2 0 0 0 2 2h4" />
                <path d="M10 9H8" />
                <path d="M16 13H8" />
                <path d="M16 17H8" />
              </svg>
              {isAdministrator || isSupervisor ? 'Tickets' : 'Mis tickets'}
            </NavItem>
          ) : null}
          {isAdministrator ? (
            <NavItem active={view === 'content'} onClick={() => onNavigate(ROUTES.content(contentTab))}>
              <svg xmlns="http://www.w3.org/2000/svg" width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">
                <path d="M21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16z" />
                <path d="M3.3 7l8.7 5 8.7-5" />
                <path d="M12 22V12" />
              </svg>
              Contenido
            </NavItem>
          ) : null}
        </nav>

        <div className="relative px-3 py-4 border-t">
          <Button
            variant="ghost"
            size="sm"
            className="w-full justify-between text-muted-foreground"
            onClick={() => setIsUserMenuOpen(current => !current)}
            type="button"
          >
            <span className="flex items-center gap-2">
              <span className="inline-flex size-7 items-center justify-center rounded-full bg-primary/10 text-xs font-semibold text-primary">
                {readyProfile.role.slice(0, 1).toUpperCase()}
              </span>
              Opciones de usuario
            </span>
            <svg xmlns="http://www.w3.org/2000/svg" width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">
              <path d="m6 9 6 6 6-6" />
            </svg>
          </Button>

          {isUserMenuOpen ? (
            <div className="absolute inset-x-3 bottom-16 rounded-lg border bg-background p-1 shadow-sm">
              <button
                className="flex w-full items-center gap-2 rounded-md px-3 py-2 text-sm text-left text-destructive hover:bg-destructive/10"
                onClick={onSignout}
                type="button"
              >
                <svg xmlns="http://www.w3.org/2000/svg" width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">
                  <path d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4" />
                  <polyline points="16 17 21 12 16 7" />
                  <line x1="21" x2="9" y1="12" y2="12" />
                </svg>
                Cerrar sesión
              </button>
            </div>
          ) : null}
        </div>
      </aside>

      <main className="flex-1 overflow-y-auto px-8 py-8">
        <div className="mx-auto flex w-full max-w-5xl flex-col space-y-6">
          {renderMainContent()}
        </div>
      </main>

      <SupervisorTicketDetailModal
        open={isSupervisorDetailOpen}
        detail={supervisorTicketDetail}
        form={supervisorTicketForm}
        requestTypes={requestTypes}
        departments={departments}
        priorities={priorities}
        ticketStatuses={ticketStatuses}
        technicians={technicians}
        isLoading={isSupervisorDetailLoading}
        isSaving={isSupervisorDetailSaving}
        onClose={() => {
          setIsSupervisorDetailOpen(false)
          setSupervisorTicketDetail(null)
        }}
        onSubmit={() => {
          void handleSaveSupervisorTicketDetail()
        }}
        onFieldChange={(field, value) => {
          setSupervisorTicketForm(current => ({ ...current, [field]: value }))
        }}
      />

      {flashMessages.length > 0 ? (
        <div className="fixed right-6 top-6 z-40 flex w-[22rem] flex-col gap-3">
          {flashMessages.map(message => (
            <div
              key={message.id}
              role="status"
              className={`relative rounded-md border px-4 py-3 pr-10 text-sm shadow-lg ${
                message.type === 'success'
                  ? 'border-emerald-200 bg-emerald-50 text-emerald-700'
                  : 'border-destructive/20 bg-destructive/10 text-destructive'
              }`}
            >
              <button
                type="button"
                aria-label="Cerrar notificación"
                className="absolute right-2 top-2 rounded p-1 text-current/70 hover:bg-black/5 hover:text-current"
                onClick={() => dismissFlash(message.id)}
              >
                x
              </button>
              {message.text}
            </div>
          ))}
        </div>
      ) : null}
    </div>
  )
}
