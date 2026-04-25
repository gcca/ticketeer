import { useState } from 'react'
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
import type { TechnicianTicket } from '@/lib/api'

type Props = {
  tickets: TechnicianTicket[]
  isLoading: boolean
  formatDate: (value?: string) => string
}

export function TechnicianTicketsView({ tickets, isLoading, formatDate }: Props) {
  const [page, setPage] = useState(1)
  const pageSize = 10
  const totalPages = Math.max(1, Math.ceil(tickets.length / pageSize))
  const paginatedTickets = tickets.slice((page - 1) * pageSize, page * pageSize)

  return (
    <Card>
      <CardHeader>
        <CardTitle className="text-base">
          Mis tickets asignados
          <span className="ml-2 text-muted-foreground font-normal text-sm">
            ({tickets.length})
          </span>
        </CardTitle>
        <CardDescription>Tickets asignados a tu perfil.</CardDescription>
      </CardHeader>
      <CardContent className="px-4 pb-4">
        {isLoading ? (
          <p className="py-10 text-center text-sm text-muted-foreground">Cargando tickets…</p>
        ) : tickets.length === 0 ? (
          <p className="py-10 text-center text-sm text-muted-foreground">No tienes tickets asignados.</p>
        ) : (
          <>
            <Table>
              <TableHeader>
                <TableRow>
                  <TableHead className="w-16">#</TableHead>
                  <TableHead>Descripción</TableHead>
                  <TableHead className="w-36">Solicitante</TableHead>
                  <TableHead className="w-28">Prioridad</TableHead>
                  <TableHead className="w-28">Estado</TableHead>
                  <TableHead className="w-36 hidden sm:table-cell">Fecha</TableHead>
                </TableRow>
              </TableHeader>
              <TableBody>
                {paginatedTickets.map(ticket => (
                  <TableRow key={ticket.id}>
                    <TableCell className="text-muted-foreground">{ticket.id}</TableCell>
                    <TableCell className="max-w-xs truncate">{ticket.description}</TableCell>
                    <TableCell>{ticket.created_by}</TableCell>
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
                  Página {page} de {totalPages}
                </p>
                <div className="flex gap-2">
                  <Button
                    type="button"
                    variant="outline"
                    size="sm"
                    onClick={() => setPage(p => Math.max(1, p - 1))}
                    disabled={page === 1}
                  >
                    Anterior
                  </Button>
                  <Button
                    type="button"
                    variant="outline"
                    size="sm"
                    onClick={() => setPage(p => Math.min(totalPages, p + 1))}
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
