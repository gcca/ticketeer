import { Fragment, useEffect, useMemo, useRef, useState } from 'react'
import { Button } from '@/components/ui/button'
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card'
import { Input } from '@/components/ui/input'
import {
  Table,
  TableBody,
  TableCell,
  TableHead,
  TableHeader,
  TableRow,
} from '@/components/ui/table'
import type { AdminDashboardTicket, TechnicianProfile, TicketStatus } from '@/lib/api'

type Props = {
  tickets: AdminDashboardTicket[]
  technicians: TechnicianProfile[]
  ticketStatuses: TicketStatus[]
  statusFilter: string
  onStatusFilterChange: (value: string) => void
  activeTicketId: number | null
  onTicketClick: (ticketId: number) => void
  onOpenDetails: (ticketId: number) => void
  page: number
  onPageChange: (page: number) => void
  isLoading: boolean
  assigningTicketId: number | null
  onAssignmentChange: (ticketId: number, technicianId: string) => void
  formatDate: (value?: string) => string
}

type AssignmentFieldProps = {
  ticketId: number
  assignedTo: string | null
  technicians: TechnicianProfile[]
  isBusy: boolean
  onAssignmentChange: (ticketId: number, technicianId: string) => void
}

function AssignmentField({
  ticketId,
  assignedTo,
  technicians,
  isBusy,
  onAssignmentChange,
}: AssignmentFieldProps) {
  const containerRef = useRef<HTMLDivElement | null>(null)
  const [isOpen, setIsOpen] = useState(false)
  const [dropdownRect, setDropdownRect] = useState({ top: 0, left: 0, width: 0 })
  const [query, setQuery] = useState(assignedTo ?? '')

  useEffect(() => {
    setQuery(assignedTo ?? '')
  }, [assignedTo])

  useEffect(() => {
    if (!isOpen) {
      return
    }

    function handlePointerDown(event: MouseEvent) {
      if (!containerRef.current?.contains(event.target as Node)) {
        setIsOpen(false)
      }
    }

    document.addEventListener('mousedown', handlePointerDown)
    return () => document.removeEventListener('mousedown', handlePointerDown)
  }, [isOpen])

  const filteredTechnicians = useMemo(() => {
    const normalizedQuery = query.trim().toLowerCase()
    if (!normalizedQuery) {
      return technicians
    }
    return technicians.filter(technician => technician.username.toLowerCase().includes(normalizedQuery))
  }, [query, technicians])

  function openDropdown() {
    if (containerRef.current) {
      const rect = containerRef.current.getBoundingClientRect()
      setDropdownRect({ top: rect.bottom, left: rect.left, width: rect.width })
    }
    setIsOpen(true)
  }

  function handleSelect(technician: TechnicianProfile) {
    setQuery(technician.username)
    setIsOpen(false)
    onAssignmentChange(ticketId, String(technician.id))
  }

  return (
    <div className="min-w-44" ref={containerRef}>
      <Input
        value={query}
        onChange={event => {
          setQuery(event.target.value)
          openDropdown()
        }}
        onFocus={() => openDropdown()}
        placeholder="Buscar técnico"
        disabled={isBusy}
      />
      {isOpen ? (
        <div
          className="z-50 max-h-56 overflow-y-auto rounded-lg border bg-background p-1 shadow-md"
          style={{ position: 'fixed', top: dropdownRect.top, left: dropdownRect.left, width: dropdownRect.width }}
        >
          {(query.trim() === '' || 'sin asignar'.includes(query.trim().toLowerCase())) ? (
            <button
              type="button"
              className="block w-full rounded-md px-3 py-2 text-left text-sm hover:bg-accent"
              onClick={() => {
                setQuery('')
                setIsOpen(false)
                onAssignmentChange(ticketId, '')
              }}
            >
              Sin asignar
            </button>
          ) : null}
          {filteredTechnicians.length === 0 ? (
            <div className="px-3 py-2 text-sm text-muted-foreground">Sin coincidencias.</div>
          ) : (
            filteredTechnicians.map(technician => (
              <button
                key={technician.id}
                type="button"
                className="block w-full rounded-md px-3 py-2 text-left text-sm hover:bg-accent"
                onClick={() => handleSelect(technician)}
              >
                {technician.username}
              </button>
            ))
          )}
        </div>
      ) : null}
    </div>
  )
}

export function SupervisorTicketsView({
  tickets,
  technicians,
  ticketStatuses,
  statusFilter,
  onStatusFilterChange,
  activeTicketId,
  onTicketClick,
  onOpenDetails,
  page,
  onPageChange,
  isLoading,
  assigningTicketId,
  onAssignmentChange,
  formatDate,
}: Props) {
  const pageSize = 10
  const totalPages = Math.max(1, Math.ceil(tickets.length / pageSize))
  const paginatedTickets = tickets.slice((page - 1) * pageSize, page * pageSize)

  return (
    <Card>
      <CardHeader className="flex flex-col gap-4 md:flex-row md:items-center md:justify-between">
        <div>
          <CardTitle className="text-base">
            Tickets
            <span className="ml-2 text-muted-foreground font-normal text-sm">
              ({tickets.length})
            </span>
          </CardTitle>
          <CardDescription>Tickets visibles para el supervisor.</CardDescription>
        </div>
        <div className="flex flex-wrap items-center gap-2">
          <label className="text-sm text-muted-foreground" htmlFor="supervisor-status-filter">
            Estado
          </label>
          <select
            id="supervisor-status-filter"
            className="flex h-8 min-w-44 rounded-lg border border-input bg-transparent px-2.5 py-1 text-sm outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50"
            value={statusFilter}
            onChange={event => onStatusFilterChange(event.target.value)}
            disabled={isLoading}
          >
            <option value="">Todos los estados</option>
            {ticketStatuses.map(option => (
              <option key={option.id} value={option.id}>
                {option.display_name}
              </option>
            ))}
          </select>
        </div>
      </CardHeader>
      <CardContent className="px-4 pb-4">
        {isLoading ? (
          <p className="py-10 text-center text-sm text-muted-foreground">Cargando tickets…</p>
        ) : tickets.length === 0 ? (
          <p className="py-10 text-center text-sm text-muted-foreground">No hay tickets para ese filtro.</p>
        ) : (
          <>
            <Table>
              <TableHeader>
                <TableRow>
                  <TableHead className="w-16">#</TableHead>
                  <TableHead>Descripción</TableHead>
                  <TableHead className="w-36">Solicitante</TableHead>
                  <TableHead className="w-52">Asignado a</TableHead>
                  <TableHead className="w-28">Prioridad</TableHead>
                  <TableHead className="w-28">Estado</TableHead>
                  <TableHead className="w-36 hidden sm:table-cell">Fecha</TableHead>
                </TableRow>
              </TableHeader>
              <TableBody>
                {paginatedTickets.map(ticket => (
                  <Fragment key={ticket.id}>
                    <TableRow
                      className="cursor-pointer"
                      onClick={() => onTicketClick(ticket.id)}
                    >
                      <TableCell className="text-muted-foreground">{ticket.id}</TableCell>
                      <TableCell className="max-w-xs truncate">{ticket.description}</TableCell>
                      <TableCell>{ticket.created_by}</TableCell>
                      <TableCell>
                        <div onClick={event => event.stopPropagation()}>
                          <AssignmentField
                            ticketId={ticket.id}
                            assignedTo={ticket.assigned_to}
                            technicians={technicians}
                            isBusy={assigningTicketId === ticket.id}
                            onAssignmentChange={onAssignmentChange}
                          />
                        </div>
                      </TableCell>
                      <TableCell>{ticket.priority}</TableCell>
                      <TableCell>{ticket.status}</TableCell>
                      <TableCell className="text-muted-foreground text-xs hidden sm:table-cell">
                        {formatDate(ticket.created_at)}
                      </TableCell>
                    </TableRow>
                    {activeTicketId === ticket.id ? (
                      <TableRow>
                        <TableCell colSpan={7} className="bg-muted/30">
                          <div className="flex gap-2 py-1">
                            <Button
                              type="button"
                              variant="outline"
                              size="sm"
                              onClick={() => onOpenDetails(ticket.id)}
                            >
                              Ver detalles
                            </Button>
                          </div>
                        </TableCell>
                      </TableRow>
                    ) : null}
                  </Fragment>
                ))}
              </TableBody>
            </Table>

            {totalPages > 1 ? (
              <div className="flex items-center justify-between px-4 py-4">
                <p className="text-sm text-muted-foreground">
                  Página {page} de {totalPages}
                </p>
                <div className="flex gap-2">
                  <Button
                    type="button"
                    variant="outline"
                    size="sm"
                    onClick={() => onPageChange(Math.max(1, page - 1))}
                    disabled={page === 1}
                  >
                    Anterior
                  </Button>
                  <Button
                    type="button"
                    variant="outline"
                    size="sm"
                    onClick={() => onPageChange(Math.min(totalPages, page + 1))}
                    disabled={page === totalPages}
                  >
                    Siguiente
                  </Button>
                </div>
              </div>
            ) : null}
          </>
        )}
      </CardContent>
    </Card>
  )
}
