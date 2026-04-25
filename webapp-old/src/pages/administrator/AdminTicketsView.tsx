import { Button } from '@/components/ui/button'
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card'
import {
  Table,
  TableBody,
  TableCell,
  TableHead,
  TableHeader,
  TableRow,
} from '@/components/ui/table'
import type { AdminDashboardTicket, TechnicianProfile } from '@/lib/api'

type Props = {
  adminTickets: AdminDashboardTicket[]
  technicians: TechnicianProfile[]
  assigningTicketId: number | null
  adminTicketsPage: number
  onAdminTicketsPageChange: (page: number) => void
  onAssignmentChange: (ticketId: number, assignedToId: string) => void
  onCreateTicket: () => void
  formatDate: (value?: string) => string
}

export function AdminTicketsView({
  adminTickets,
  technicians,
  assigningTicketId,
  adminTicketsPage,
  onAdminTicketsPageChange,
  onAssignmentChange,
  onCreateTicket,
  formatDate,
}: Props) {
  const pageSize = 10
  const totalPages = Math.max(1, Math.ceil(adminTickets.length / pageSize))
  const paginatedAdminTickets = adminTickets.slice((adminTicketsPage - 1) * pageSize, adminTicketsPage * pageSize)

  return (
    <Card>
      <CardHeader className="flex flex-row items-center justify-between">
        <div>
          <CardTitle className="text-base">
            Tickets
            <span className="ml-2 text-muted-foreground font-normal text-sm">
              ({adminTickets.length})
            </span>
          </CardTitle>
          <CardDescription>Listado administrativo de tickets.</CardDescription>
        </div>
        <Button type="button" onClick={onCreateTicket}>
          + Agregar
        </Button>
      </CardHeader>
      <CardContent className="px-4 pb-4">
        {adminTickets.length === 0 ? (
          <p className="text-sm text-muted-foreground text-center py-10">
            No hay tickets registrados.
          </p>
        ) : (
          <>
            <Table>
              <TableHeader>
                <TableRow>
                  <TableHead className="w-16">#</TableHead>
                  <TableHead>Descripción</TableHead>
                  <TableHead className="w-36">Solicitante</TableHead>
                  <TableHead className="w-36">Asignado a</TableHead>
                  <TableHead className="w-28">Prioridad</TableHead>
                  <TableHead className="w-28">Estado</TableHead>
                  <TableHead className="w-36 hidden sm:table-cell">Fecha</TableHead>
                </TableRow>
              </TableHeader>
              <TableBody>
                {paginatedAdminTickets.map(ticket => (
                  <TableRow key={ticket.id}>
                    <TableCell className="text-muted-foreground">{ticket.id}</TableCell>
                    <TableCell className="max-w-xs truncate">{ticket.description}</TableCell>
                    <TableCell>{ticket.created_by}</TableCell>
                    <TableCell>
                      <select
                        className="flex h-8 w-full min-w-36 rounded-lg border border-input bg-transparent px-2.5 py-1 text-sm outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50"
                        value={technicians.find(technician => technician.username === ticket.assigned_to)?.id?.toString() ?? ''}
                        onChange={event => onAssignmentChange(ticket.id, event.target.value)}
                        disabled={assigningTicketId === ticket.id}
                      >
                        <option value="">Sin asignar</option>
                        {technicians.map(technician => (
                          <option key={technician.id} value={technician.id}>
                            {technician.username}
                          </option>
                        ))}
                      </select>
                    </TableCell>
                    <TableCell>{ticket.priority}</TableCell>
                    <TableCell>{ticket.status}</TableCell>
                    <TableCell className="text-muted-foreground text-xs hidden sm:table-cell">
                      {formatDate(ticket.created_at)}
                    </TableCell>
                  </TableRow>
                ))}
              </TableBody>
            </Table>

            {totalPages > 1 ? (
              <div className="flex items-center justify-between px-4 py-4">
                <p className="text-sm text-muted-foreground">
                  Página {adminTicketsPage} de {totalPages}
                </p>
                <div className="flex gap-2">
                  <Button
                    type="button"
                    variant="outline"
                    size="sm"
                    onClick={() => onAdminTicketsPageChange(Math.max(1, adminTicketsPage - 1))}
                    disabled={adminTicketsPage === 1}
                  >
                    Anterior
                  </Button>
                  <Button
                    type="button"
                    variant="outline"
                    size="sm"
                    onClick={() => onAdminTicketsPageChange(Math.min(totalPages, adminTicketsPage + 1))}
                    disabled={adminTicketsPage === totalPages}
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
