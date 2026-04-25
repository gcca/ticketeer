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
import type { Ticket } from '@/lib/api'

type Props = {
  tickets: Ticket[]
  onCreateTicket: () => void
  formatDate: (value?: string) => string
  renderPriority: (id: number) => React.ReactNode
  renderStatus: (id: number) => React.ReactNode
}

export function RequesterTicketsView({
  tickets,
  onCreateTicket,
  formatDate,
  renderPriority,
  renderStatus,
}: Props) {
  return (
    <Card>
      <CardHeader className="flex flex-row items-center justify-between">
        <div>
          <CardTitle className="text-base">
            Mis tickets
            <span className="ml-2 text-muted-foreground font-normal text-sm">
              ({tickets.length})
            </span>
          </CardTitle>
          <CardDescription>Tickets creados por tu perfil.</CardDescription>
        </div>
        <Button type="button" onClick={onCreateTicket}>
          + Agregar
        </Button>
      </CardHeader>
      <CardContent className="px-4 pb-4">
        {tickets.length === 0 ? (
          <p className="text-sm text-muted-foreground text-center py-10">
            No tienes tickets registrados.
          </p>
        ) : (
          <Table>
            <TableHeader>
              <TableRow>
                <TableHead className="w-16">#</TableHead>
                <TableHead>Descripción</TableHead>
                <TableHead className="w-28">Prioridad</TableHead>
                <TableHead className="w-28">Estado</TableHead>
                <TableHead className="w-36 hidden sm:table-cell">Fecha</TableHead>
              </TableRow>
            </TableHeader>
            <TableBody>
              {tickets.map(ticket => (
                <TableRow key={ticket.id}>
                  <TableCell className="text-muted-foreground">{ticket.id}</TableCell>
                  <TableCell className="max-w-xs truncate">{ticket.description}</TableCell>
                  <TableCell>{renderPriority(ticket.priority_id)}</TableCell>
                  <TableCell>{renderStatus(ticket.status_id)}</TableCell>
                  <TableCell className="text-muted-foreground text-xs hidden sm:table-cell">
                    {formatDate(ticket.created_at)}
                  </TableCell>
                </TableRow>
              ))}
            </TableBody>
          </Table>
        )}
      </CardContent>
    </Card>
  )
}
