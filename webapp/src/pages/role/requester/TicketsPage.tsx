import { Fragment, useEffect, useRef, useState } from 'react'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import {
  ActivityCreateMessage,
  TicketCreate,
  TicketDetails,
  TicketList,
  type CreateTicketInput,
  type CreatedTicket,
  type Department,
  type Priority,
  type RequestType,
  type Ticket,
  type TicketActivityDetail,
  type TicketDetail,
  type TicketStatus,
} from '@/lib/api'

export type TicketsPageApi = {
  listTickets: (token: string, search?: string) => Promise<Ticket[]>
  createTicket: (token: string, input: CreateTicketInput) => Promise<CreatedTicket>
  getDetails: (token: string, id: string) => Promise<TicketDetail>
  createMessage: (token: string, ticketId: string, message: string) => Promise<TicketActivityDetail>
}

const defaultApi: TicketsPageApi = {
  listTickets: TicketList,
  createTicket: TicketCreate,
  getDetails: TicketDetails,
  createMessage: ActivityCreateMessage,
}

type Props = {
  token: string
  onSignout: () => void
  requestTypes: RequestType[]
  departments: Department[]
  priorities: Priority[]
  ticketStatuses: TicketStatus[]
  api?: TicketsPageApi
}

type ListState =
  | { status: 'loading' }
  | { status: 'error'; message: string }
  | { status: 'ready'; tickets: Ticket[] }

type DetailState =
  | { status: 'loading' }
  | { status: 'error'; message: string }
  | { status: 'ready'; detail: TicketDetail }

type FormState =
  | { status: 'idle' }
  | { status: 'submitting' }
  | { status: 'error'; message: string }

type MessageState =
  | { status: 'idle' }
  | { status: 'submitting' }
  | { status: 'error'; message: string }

const selectClass =
  'h-8 w-full min-w-0 rounded-lg border border-input bg-transparent px-2.5 py-1 text-sm transition-colors outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50 disabled:opacity-50'

const textareaClass =
  'w-full min-w-0 rounded-lg border border-input bg-transparent px-2.5 py-1.5 text-sm transition-colors outline-none placeholder:text-muted-foreground focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50 disabled:opacity-50 resize-none'

function formatDate(raw: string) {
  const d = new Date(raw)
  return d.toLocaleDateString('es-PE', { day: '2-digit', month: 'short', year: 'numeric' })
}

function formatDateTime(raw: string) {
  const d = new Date(raw)
  return d.toLocaleString('es-PE', { day: '2-digit', month: 'short', year: 'numeric', hour: '2-digit', minute: '2-digit' })
}

const activityKindLabel: Record<string, string> = {
  message: 'Mensaje',
  status_changed: 'Cambio de estado',
  assigned_changed: 'Cambio de asignación',
  department_changed: 'Cambio de departamento',
  priority_changed: 'Cambio de prioridad',
  due_date_changed: 'Cambio de fecha límite',
}


function DetailField({ label, value }: { label: string; value: string }) {
  return (
    <div>
      <p className="text-xs text-muted-foreground mb-0.5">{label}</p>
      <p className="text-sm font-medium">{value}</p>
    </div>
  )
}

const DEFAULT_PAGE_SIZE = 12

export function TicketsPage({ token, onSignout, requestTypes, departments, priorities, ticketStatuses, api = defaultApi }: Props) {
  const [listState, setListState] = useState<ListState>({ status: 'loading' })
  const [expandedId, setExpandedId] = useState<string | null>(null)
  const [detailId, setDetailId] = useState<string | null>(null)
  const [detailState, setDetailState] = useState<DetailState | null>(null)
  const [formOpen, setFormOpen] = useState(false)
  const [formState, setFormState] = useState<FormState>({ status: 'idle' })
  const formRef = useRef<HTMLFormElement>(null)
  const [messageOpen, setMessageOpen] = useState(false)
  const [messageState, setMessageState] = useState<MessageState>({ status: 'idle' })
  const messageRef = useRef<HTMLTextAreaElement>(null)
  const [search, setSearch] = useState('')
  const [searchInput, setSearchInput] = useState('')
  const [page, setPage] = useState(1)
  const [pageSize, setPageSize] = useState(DEFAULT_PAGE_SIZE)
  const searchTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null)

  function loadTickets(s?: string) {
    setListState({ status: 'loading' })
    api.listTickets(token, s)
      .then(tickets => setListState({ status: 'ready', tickets }))
      .catch(err => {
        if ((err as { status?: number }).status === 401) { onSignout(); return }
        const message = err instanceof Error ? err.message : 'Error al cargar los tickets.'
        setListState({ status: 'error', message })
      })
  }

  useEffect(() => { loadTickets(search || undefined) }, [token, onSignout, search])

  function handleSearchChange(e: React.ChangeEvent<HTMLInputElement>) {
    const val = e.target.value
    setSearchInput(val)
    if (searchTimerRef.current) clearTimeout(searchTimerRef.current)
    searchTimerRef.current = setTimeout(() => {
      setSearch(val)
      setPage(1)
    }, 400)
  }

  function handlePageSizeChange(e: React.ChangeEvent<HTMLInputElement>) {
    const n = parseInt(e.target.value, 10)
    if (!isNaN(n) && n > 0) {
      setPageSize(n)
      setPage(1)
    }
  }

  function handleRowClick(id: string) {
    setExpandedId(prev => prev === id ? null : id)
  }

  function handleViewDetail(id: string) {
    setDetailId(id)
    setDetailState({ status: 'loading' })
    api.getDetails(token, id)
      .then(detail => setDetailState({ status: 'ready', detail }))
      .catch(err => {
        if ((err as { status?: number }).status === 401) { onSignout(); return }
        const message = err instanceof Error ? err.message : 'Error al cargar el detalle.'
        setDetailState({ status: 'error', message })
      })
  }

  function handleBackToList() {
    setDetailId(null)
    setDetailState(null)
  }

  function handleOpenForm() {
    setFormOpen(true)
    setFormState({ status: 'idle' })
  }

  function handleCancel() {
    setFormOpen(false)
    setFormState({ status: 'idle' })
  }

  async function handleSubmit(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault()
    const data = new FormData(e.currentTarget)
    const request_type_id = data.get('request_type_id') as string
    const department_id = data.get('department_id') as string
    const priority_id = data.get('priority_id') as string
    const description = (data.get('description') as string).trim()
    const due_date = (data.get('due_date') as string) || undefined

    setFormState({ status: 'submitting' })
    try {
      await api.createTicket(token, { request_type_id, department_id, priority_id, description, due_date })
      setFormOpen(false)
      setFormState({ status: 'idle' })
      loadTickets(search || undefined)
    } catch (err) {
      if ((err as { status?: number }).status === 401) { onSignout(); return }
      const message = err instanceof Error ? err.message : 'Error al crear el ticket.'
      setFormState({ status: 'error', message })
    }
  }

  async function handleMessageSubmit(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault()
    if (detailState?.status !== 'ready') return
    const message = messageRef.current?.value.trim() ?? ''
    if (!message) return
    setMessageState({ status: 'submitting' })
    try {
      const activity = await api.createMessage(token, detailState.detail.id, message)
      setDetailState({
        status: 'ready',
        detail: { ...detailState.detail, activities: [activity, ...detailState.detail.activities] },
      })
      setMessageOpen(false)
      setMessageState({ status: 'idle' })
    } catch (err) {
      if ((err as { status?: number }).status === 401) { onSignout(); return }
      const message = err instanceof Error ? err.message : 'Error al enviar el mensaje.'
      setMessageState({ status: 'error', message })
    }
  }

  const isSubmitting = formState.status === 'submitting'

  // ── Detail view ──────────────────────────────────────────────────────────────
  if (detailId !== null) {
    return (
      <div className="space-y-4">
        <div className="flex items-center gap-3">
          <Button variant="ghost" size="sm" onClick={handleBackToList} type="button" className="text-muted-foreground">
            <svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true" className="mr-1">
              <path d="m15 18-6-6 6-6" />
            </svg>
            Volver
          </Button>
          <h2 className="text-xl font-semibold tracking-tight">Detalle del ticket</h2>
        </div>

        {detailState?.status === 'loading' && (
          <div className="flex items-center justify-center py-16 text-muted-foreground text-sm">Cargando…</div>
        )}

        {detailState?.status === 'error' && (
          <div className="flex items-center justify-center py-16 px-4">
            <div role="alert" className="text-sm text-destructive bg-destructive/10 border border-destructive/20 rounded-md px-4 py-3 max-w-sm text-center">
              {detailState.message}
            </div>
          </div>
        )}

        {detailState?.status === 'ready' && (() => {
          const { detail } = detailState
          return (
            <div className="rounded-lg border bg-background p-5 space-y-5">
              <div>
                <p className="text-xs text-muted-foreground mb-1">Descripción</p>
                <p className="text-base font-medium leading-snug">{detail.description}</p>
              </div>
              <div className="grid grid-cols-2 gap-x-8 gap-y-4 sm:grid-cols-3">
                <DetailField label="Estado" value={detail.status.display_name} />
                <DetailField label="Prioridad" value={detail.priority.display_name} />
                <DetailField label="Departamento" value={detail.department.name} />
                <DetailField label="Tipo de solicitud" value={detail.request_type.description} />
                <DetailField label="Solicitante" value={detail.requester.name} />
                {detail.assigned_to && <DetailField label="Asignado a" value={detail.assigned_to.name} />}
                <DetailField label="Creado" value={formatDateTime(detail.created_at)} />
                {detail.due_date && <DetailField label="Fecha límite" value={formatDate(detail.due_date)} />}
              </div>
              <div className="border-t pt-5">
                <div className="flex items-center justify-between mb-4">
                  <p className="text-xs font-medium text-muted-foreground uppercase tracking-wide">Actividad</p>
                  {!messageOpen && (
                    <Button size="sm" variant="outline" type="button" onClick={() => { setMessageOpen(true); setMessageState({ status: 'idle' }) }}>
                      Añadir mensaje
                    </Button>
                  )}
                </div>
                {messageOpen && (
                  <form onSubmit={handleMessageSubmit} className="mb-5 space-y-3 rounded-lg border bg-muted/20 p-4">
                    <p className="text-xs font-medium">Nuevo mensaje</p>
                    <textarea
                      ref={messageRef}
                      className={textareaClass}
                      rows={3}
                      placeholder="Escribe tu mensaje…"
                      required
                      disabled={messageState.status === 'submitting'}
                      autoFocus
                    />
                    {messageState.status === 'error' && (
                      <p role="alert" className="text-sm text-destructive">{messageState.message}</p>
                    )}
                    <div className="flex gap-2">
                      <Button type="submit" size="sm" disabled={messageState.status === 'submitting'}>
                        {messageState.status === 'submitting' ? 'Enviando…' : 'Enviar'}
                      </Button>
                      <Button type="button" size="sm" variant="ghost" onClick={() => { setMessageOpen(false); setMessageState({ status: 'idle' }) }} disabled={messageState.status === 'submitting'}>
                        Cancelar
                      </Button>
                    </div>
                  </form>
                )}
                {detail.activities.length === 0 ? (
                  <p className="text-sm text-muted-foreground">Sin actividad registrada.</p>
                ) : (
                  <div className="relative">
                    <div className="absolute left-[6px] top-2 bottom-2 w-px bg-border" />
                    <div className="space-y-5">
                      {detail.activities.map(a => (
                        <div key={a.id} className="flex gap-4">
                          <div className="mt-1 h-3 w-3 shrink-0 rounded-full border-2 border-muted-foreground/40 bg-background z-10" />
                          <div className="flex-1">
                            <div className="flex items-baseline gap-2 mb-1">
                              <span className="text-xs font-medium italic">{activityKindLabel[a.kind] ?? a.kind}</span>
                              <span className="text-xs text-muted-foreground">{a.profile_name}</span>
                              <span className="text-xs text-muted-foreground">{formatDateTime(a.created_at)}</span>
                            </div>
                            <p className="text-sm leading-relaxed">{a.body}</p>
                          </div>
                        </div>
                      ))}
                    </div>
                  </div>
                )}
              </div>
            </div>
          )
        })()}
      </div>
    )
  }

  // ── List view ─────────────────────────────────────────────────────────────────
  const allTickets = listState.status === 'ready' ? listState.tickets : []
  const totalPages = Math.max(1, Math.ceil(allTickets.length / pageSize))
  const safePage = Math.min(page, totalPages)
  const pageTickets = allTickets.slice((safePage - 1) * pageSize, safePage * pageSize)

  return (
    <div className="space-y-4">
      <div className="flex items-center justify-between gap-4">
        <div>
          <h2 className="text-xl font-semibold tracking-tight">Tickets</h2>
          {listState.status === 'ready' && (
            <p className="text-sm text-muted-foreground mt-1">
              {allTickets.length === 0
                ? (search ? 'Sin resultados para la búsqueda.' : 'No hay tickets registrados.')
                : `${allTickets.length} ticket${allTickets.length === 1 ? '' : 's'} · página ${safePage} de ${totalPages}`}
            </p>
          )}
        </div>
        {!formOpen && (
          <Button size="sm" onClick={handleOpenForm} type="button">
            <svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true" className="mr-1.5">
              <line x1="12" x2="12" y1="5" y2="19" />
              <line x1="5" x2="19" y1="12" y2="12" />
            </svg>
            Nuevo ticket
          </Button>
        )}
      </div>

      <div className="flex items-center gap-3">
        <div className="relative flex-1 max-w-sm">
          <svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true" className="absolute left-2.5 top-1/2 -translate-y-1/2 text-muted-foreground">
            <circle cx="11" cy="11" r="8" /><path d="m21 21-4.3-4.3" />
          </svg>
          <Input
            type="search"
            placeholder="Buscar tickets…"
            value={searchInput}
            onChange={handleSearchChange}
            className="pl-8 h-8 text-sm"
          />
        </div>
        <div className="flex items-center gap-1.5 text-sm text-muted-foreground shrink-0">
          <span>Por página</span>
          <input
            type="number"
            min={1}
            value={pageSize}
            onChange={handlePageSizeChange}
            className="w-14 h-8 rounded-md border border-input bg-transparent px-2 text-sm text-center outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50"
          />
        </div>
      </div>

      {formOpen && (
        <div className="rounded-lg border bg-background p-5">
          <h3 className="text-sm font-semibold mb-4">Nuevo ticket</h3>
          <form ref={formRef} onSubmit={handleSubmit} className="space-y-4">
            <div className="grid grid-cols-1 gap-4 sm:grid-cols-3">
              <div className="space-y-1.5">
                <Label htmlFor="request_type_id">Tipo de solicitud</Label>
                <select id="request_type_id" name="request_type_id" className={selectClass} required disabled={isSubmitting} defaultValue="">
                  <option value="" disabled>Selecciona un tipo</option>
                  {requestTypes.map(rt => (
                    <option key={rt.id} value={rt.id}>{rt.description}</option>
                  ))}
                </select>
              </div>
              <div className="space-y-1.5">
                <Label htmlFor="department_id">Departamento</Label>
                <select id="department_id" name="department_id" className={selectClass} required disabled={isSubmitting} defaultValue="">
                  <option value="" disabled>Selecciona un departamento</option>
                  {departments.map(d => (
                    <option key={d.id} value={d.id}>{d.name}</option>
                  ))}
                </select>
              </div>
              <div className="space-y-1.5">
                <Label htmlFor="priority_id">Prioridad</Label>
                <select id="priority_id" name="priority_id" className={selectClass} required disabled={isSubmitting} defaultValue="">
                  <option value="" disabled>Selecciona una prioridad</option>
                  {priorities.map(p => (
                    <option key={p.id} value={p.id}>{p.display_name}</option>
                  ))}
                </select>
              </div>
            </div>
            <div className="space-y-1.5">
              <Label htmlFor="description">Descripción</Label>
              <textarea id="description" name="description" className={textareaClass} rows={3} placeholder="Describe el problema o solicitud…" required disabled={isSubmitting} />
            </div>
            <div className="space-y-1.5 w-48">
              <Label htmlFor="due_date">Fecha límite <span className="text-muted-foreground font-normal">(opcional)</span></Label>
              <Input id="due_date" name="due_date" type="date" disabled={isSubmitting} />
            </div>
            {formState.status === 'error' && (
              <p role="alert" className="text-sm text-destructive">{formState.message}</p>
            )}
            <div className="flex gap-2 pt-1">
              <Button type="submit" size="sm" disabled={isSubmitting}>
                {isSubmitting ? 'Creando…' : 'Crear ticket'}
              </Button>
              <Button type="button" size="sm" variant="ghost" onClick={handleCancel} disabled={isSubmitting}>
                Cancelar
              </Button>
            </div>
          </form>
        </div>
      )}

      {listState.status === 'loading' && (
        <div className="flex items-center justify-center py-16 text-muted-foreground text-sm">Cargando…</div>
      )}

      {listState.status === 'error' && (
        <div className="flex items-center justify-center py-16 px-4">
          <div role="alert" className="text-sm text-destructive bg-destructive/10 border border-destructive/20 rounded-md px-4 py-3 max-w-sm text-center">
            {listState.message}
          </div>
        </div>
      )}

      {listState.status === 'ready' && allTickets.length > 0 && (
        <div className="rounded-md border bg-background overflow-hidden">
          <table className="w-full text-sm">
            <thead>
              <tr className="border-b bg-muted/50">
                <th className="px-4 py-3 text-left font-medium text-muted-foreground">Título</th>
                <th className="w-8" />
              </tr>
            </thead>
            <tbody>
              {pageTickets.map((ticket, i) => {
                const isExpanded = expandedId === ticket.id
                const isLast = i === pageTickets.length - 1
                const statusName = ticketStatuses.find(s => s.id === ticket.status_id)?.display_name ?? ticket.status_id
                return (
                  <Fragment key={ticket.id}>
                    <tr
                      className={`cursor-pointer hover:bg-muted/40 transition-colors${!isExpanded && !isLast ? ' border-b' : ''}`}
                      onClick={() => handleRowClick(ticket.id)}
                    >
                      <td className="px-4 py-3">
                        {!isExpanded && <p className="text-xs text-muted-foreground mb-0.5">Ticket #{ticket.id}</p>}
                        <p className="font-medium">{ticket.description}</p>
                      </td>
                      <td className="px-3 py-3 text-muted-foreground">
                        <svg
                          xmlns="http://www.w3.org/2000/svg"
                          width="14"
                          height="14"
                          viewBox="0 0 24 24"
                          fill="none"
                          stroke="currentColor"
                          strokeWidth="2"
                          strokeLinecap="round"
                          strokeLinejoin="round"
                          aria-hidden="true"
                          className={`transition-transform ${isExpanded ? 'rotate-180' : ''}`}
                        >
                          <path d="m6 9 6 6 6-6" />
                        </svg>
                      </td>
                    </tr>

                    {isExpanded && (
                      <tr className={!isLast ? 'border-b' : undefined}>
                        <td colSpan={2} className="px-4 py-3 bg-muted/20">
                          <div className="space-y-2">
                            <div className="flex items-center justify-between gap-4">
                              <div className="flex items-center gap-6 text-sm">
                                <div>
                                  <span className="text-xs text-muted-foreground mr-1.5">Estado</span>
                                  <span className="inline-flex items-center rounded-full bg-secondary px-2.5 py-0.5 text-xs font-medium text-secondary-foreground">
                                    {statusName}
                                  </span>
                                </div>
                                <div>
                                  <span className="text-xs text-muted-foreground mr-1.5">Creado</span>
                                  <span className="text-xs">{formatDateTime(ticket.created_at)}</span>
                                </div>
                              </div>
                              <button
                                type="button"
                                onClick={e => { e.stopPropagation(); handleViewDetail(ticket.id) }}
                                className="inline-flex items-center gap-1.5 rounded-md px-2.5 py-1.5 text-xs font-medium text-muted-foreground hover:bg-accent hover:text-accent-foreground transition-colors"
                                title="Ver detalle"
                              >
                                <svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">
                                  <path d="M2 12s3-7 10-7 10 7 10 7-3 7-10 7-10-7-10-7Z" />
                                  <circle cx="12" cy="12" r="3" />
                                </svg>
                                Ver detalle
                              </button>
                            </div>
                            {ticket.activities.length > 0 && (
                              <div className="max-h-32 overflow-y-auto space-y-1.5 border-t border-border/50 pt-2">
                                {ticket.activities.map((a, i) => (
                                  <div key={i} className="flex gap-3 text-xs">
                                    <span className="text-muted-foreground shrink-0">{formatDateTime(a.created_at)}</span>
                                    <span>{a.body}</span>
                                  </div>
                                ))}
                              </div>
                            )}
                          </div>
                        </td>
                      </tr>
                    )}
                  </Fragment>
                )
              })}
            </tbody>
          </table>
        </div>
      )}

      {listState.status === 'ready' && totalPages > 1 && (
        <div className="flex items-center justify-center gap-1">
          <button
            type="button"
            onClick={() => setPage(1)}
            disabled={safePage === 1}
            className="inline-flex items-center justify-center h-8 w-8 rounded-md text-sm text-muted-foreground hover:bg-accent hover:text-accent-foreground disabled:opacity-40 disabled:pointer-events-none transition-colors"
            aria-label="Primera página"
          >
            <svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="m11 17-5-5 5-5"/><path d="m18 17-5-5 5-5"/></svg>
          </button>
          <button
            type="button"
            onClick={() => setPage(p => Math.max(1, p - 1))}
            disabled={safePage === 1}
            className="inline-flex items-center justify-center h-8 w-8 rounded-md text-sm text-muted-foreground hover:bg-accent hover:text-accent-foreground disabled:opacity-40 disabled:pointer-events-none transition-colors"
            aria-label="Página anterior"
          >
            <svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="m15 18-6-6 6-6"/></svg>
          </button>
          <span className="px-3 text-sm text-muted-foreground">{safePage} / {totalPages}</span>
          <button
            type="button"
            onClick={() => setPage(p => Math.min(totalPages, p + 1))}
            disabled={safePage === totalPages}
            className="inline-flex items-center justify-center h-8 w-8 rounded-md text-sm text-muted-foreground hover:bg-accent hover:text-accent-foreground disabled:opacity-40 disabled:pointer-events-none transition-colors"
            aria-label="Página siguiente"
          >
            <svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="m9 18 6-6-6-6"/></svg>
          </button>
          <button
            type="button"
            onClick={() => setPage(totalPages)}
            disabled={safePage === totalPages}
            className="inline-flex items-center justify-center h-8 w-8 rounded-md text-sm text-muted-foreground hover:bg-accent hover:text-accent-foreground disabled:opacity-40 disabled:pointer-events-none transition-colors"
            aria-label="Última página"
          >
            <svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="m6 17 5-5-5-5"/><path d="m13 17 5-5-5-5"/></svg>
          </button>
        </div>
      )}
    </div>
  )
}
