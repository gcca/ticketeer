import { Button } from '@/components/ui/button'
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import type {
  Department,
  Priority,
  RequestType,
  SupervisorTicketDetail,
  TechnicianProfile,
  TicketStatus,
} from '@/lib/api'

export type SupervisorTicketFormState = {
  requestTypeId: string
  requesterId: string
  assignedToId: string
  departmentId: string
  priorityId: string
  statusId: string
  description: string
  dueDate: string
}

type Props = {
  open: boolean
  detail: SupervisorTicketDetail | null
  form: SupervisorTicketFormState
  requestTypes: RequestType[]
  departments: Department[]
  priorities: Priority[]
  ticketStatuses: TicketStatus[]
  technicians: TechnicianProfile[]
  isLoading: boolean
  isSaving: boolean
  onClose: () => void
  onSubmit: () => void
  onFieldChange: (field: keyof SupervisorTicketFormState, value: string) => void
}

export function SupervisorTicketDetailModal({
  open,
  detail,
  form,
  requestTypes,
  departments,
  priorities,
  ticketStatuses,
  technicians,
  isLoading,
  isSaving,
  onClose,
  onSubmit,
  onFieldChange,
}: Props) {
  if (!open) {
    return null
  }

  return (
    <div className="fixed inset-0 z-40 flex items-center justify-center bg-black/35 p-4">
      <Card className="max-h-[90vh] w-full max-w-3xl overflow-y-auto">
        <CardHeader className="flex flex-row items-start justify-between gap-4">
          <div>
            <CardTitle className="text-base">
              Ticket {detail ? `#${detail.id}` : ''}
            </CardTitle>
            <CardDescription>Consulta y actualiza el ticket seleccionado.</CardDescription>
          </div>
          <Button type="button" variant="outline" size="sm" onClick={onClose} disabled={isSaving}>
            Cerrar
          </Button>
        </CardHeader>
        <CardContent>
          {isLoading || !detail ? (
            <p className="py-10 text-center text-sm text-muted-foreground">Cargando detalle…</p>
          ) : (
            <form
              className="space-y-4"
              onSubmit={event => {
                event.preventDefault()
                onSubmit()
              }}
            >
              <div className="grid gap-4 md:grid-cols-2">
                <div className="space-y-2">
                  <Label htmlFor="supervisor-request-type">Tipo de solicitud</Label>
                  <select
                    id="supervisor-request-type"
                    className="flex h-8 w-full rounded-lg border border-input bg-transparent px-2.5 py-1 text-sm outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50"
                    value={form.requestTypeId}
                    onChange={event => onFieldChange('requestTypeId', event.target.value)}
                    disabled={isSaving}
                  >
                    {requestTypes.map(requestType => (
                      <option key={requestType.id} value={requestType.id}>
                        {requestType.name}
                      </option>
                    ))}
                  </select>
                </div>
                <div className="space-y-2">
                  <Label htmlFor="supervisor-requester-id">Requester ID</Label>
                  <Input
                    id="supervisor-requester-id"
                    type="number"
                    value={form.requesterId}
                    onChange={event => onFieldChange('requesterId', event.target.value)}
                    disabled={isSaving}
                  />
                </div>
                <div className="space-y-2">
                  <Label htmlFor="supervisor-department">Departamento</Label>
                  <select
                    id="supervisor-department"
                    className="flex h-8 w-full rounded-lg border border-input bg-transparent px-2.5 py-1 text-sm outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50"
                    value={form.departmentId}
                    onChange={event => onFieldChange('departmentId', event.target.value)}
                    disabled={isSaving}
                  >
                    {departments.map(department => (
                      <option key={department.id} value={department.id}>
                        {department.name}
                      </option>
                    ))}
                  </select>
                </div>
                <div className="space-y-2">
                  <Label htmlFor="supervisor-priority">Prioridad</Label>
                  <select
                    id="supervisor-priority"
                    className="flex h-8 w-full rounded-lg border border-input bg-transparent px-2.5 py-1 text-sm outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50"
                    value={form.priorityId}
                    onChange={event => onFieldChange('priorityId', event.target.value)}
                    disabled={isSaving}
                  >
                    {priorities.map(priority => (
                      <option key={priority.id} value={priority.id}>
                        {priority.display_name}
                      </option>
                    ))}
                  </select>
                </div>
                <div className="space-y-2">
                  <Label htmlFor="supervisor-status">Estado</Label>
                  <select
                    id="supervisor-status"
                    className="flex h-8 w-full rounded-lg border border-input bg-transparent px-2.5 py-1 text-sm outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50"
                    value={form.statusId}
                    onChange={event => onFieldChange('statusId', event.target.value)}
                    disabled={isSaving}
                  >
                    {ticketStatuses.map(status => (
                      <option key={status.id} value={status.id}>
                        {status.display_name}
                      </option>
                    ))}
                  </select>
                </div>
                <div className="space-y-2">
                  <Label htmlFor="supervisor-assigned-to">Asignado a</Label>
                  <select
                    id="supervisor-assigned-to"
                    className="flex h-8 w-full rounded-lg border border-input bg-transparent px-2.5 py-1 text-sm outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50"
                    value={form.assignedToId}
                    onChange={event => onFieldChange('assignedToId', event.target.value)}
                    disabled={isSaving}
                  >
                    <option value="">Sin asignar</option>
                    {technicians.map(technician => (
                      <option key={technician.id} value={technician.id}>
                        {technician.username}
                      </option>
                    ))}
                  </select>
                </div>
                <div className="space-y-2 md:col-span-2">
                  <Label htmlFor="supervisor-due-date">Fecha límite</Label>
                  <Input
                    id="supervisor-due-date"
                    type="datetime-local"
                    value={form.dueDate}
                    onChange={event => onFieldChange('dueDate', event.target.value)}
                    disabled={isSaving}
                  />
                </div>
                <div className="space-y-2 md:col-span-2">
                  <Label htmlFor="supervisor-description">Descripción</Label>
                  <textarea
                    id="supervisor-description"
                    className="flex min-h-28 w-full rounded-lg border border-input bg-transparent px-3 py-2 text-sm outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50"
                    value={form.description}
                    onChange={event => onFieldChange('description', event.target.value)}
                    disabled={isSaving}
                  />
                </div>
              </div>

              <div className="flex items-center justify-between text-xs text-muted-foreground">
                <span>Creado: {detail.created_at}</span>
                <span>Actualizado: {detail.updated_at}</span>
              </div>

              <div className="flex justify-end gap-2">
                <Button type="button" variant="outline" onClick={onClose} disabled={isSaving}>
                  Cancelar
                </Button>
                <Button type="submit" disabled={isSaving}>
                  {isSaving ? 'Guardando…' : 'Guardar'}
                </Button>
              </div>
            </form>
          )}
        </CardContent>
      </Card>
    </div>
  )
}
