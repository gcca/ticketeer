import { Button } from '@/components/ui/button'
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card'
import { Label } from '@/components/ui/label'
import type { Department, Priority, RequestType } from '@/lib/api'

type TicketFormState = {
  requestTypeId: string
  departmentId: string
  priorityId: string
  description: string
}

type Props = {
  form: TicketFormState
  priorities: Priority[]
  departments: Department[]
  requestTypes: RequestType[]
  selectedRequestType?: RequestType
  isSubmitting: boolean
  onRequestTypeChange: (requestTypeId: string) => void
  onDepartmentChange: (departmentId: string) => void
  onPriorityChange: (priorityId: string) => void
  onDescriptionChange: (description: string) => void
  onCancel: () => void
  onSubmit: (event: React.FormEvent<HTMLFormElement>) => void
}

export function RequesterNewTicketView({
  form,
  priorities,
  departments,
  requestTypes,
  selectedRequestType,
  isSubmitting,
  onRequestTypeChange,
  onDepartmentChange,
  onPriorityChange,
  onDescriptionChange,
  onCancel,
  onSubmit,
}: Props) {
  return (
    <Card>
      <CardHeader>
        <CardTitle className="text-base">Nuevo ticket</CardTitle>
        <CardDescription>Completa la solicitud y guárdala.</CardDescription>
      </CardHeader>
      <CardContent>
        <form className="space-y-4" onSubmit={onSubmit}>
          <div className="grid gap-4 md:grid-cols-3">
            <div className="space-y-2">
              <Label htmlFor="requestType">Tipo de solicitud</Label>
              <select
                id="requestType"
                className="flex h-8 w-full rounded-lg border border-input bg-transparent px-2.5 py-1 text-sm outline-none focus-visible:border-ring focus-visible:ring-3 focus-visible:ring-ring/50"
                value={form.requestTypeId}
                onChange={event => onRequestTypeChange(event.target.value)}
                disabled={isSubmitting}
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
                value={form.departmentId}
                onChange={event => onDepartmentChange(event.target.value)}
                disabled={isSubmitting}
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
                value={form.priorityId}
                onChange={event => onPriorityChange(event.target.value)}
                disabled={isSubmitting}
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
              value={form.description}
              onChange={event => onDescriptionChange(event.target.value)}
              placeholder={selectedRequestType?.description ?? 'Describe el problema'}
              disabled={isSubmitting}
            />
          </div>

          <div className="flex flex-col gap-3 sm:flex-row sm:items-center sm:justify-between">
            <div className="text-sm text-muted-foreground">
              {selectedRequestType
                ? `Prioridad sugerida: ${priorities.find(priority => priority.id === selectedRequestType.default_priority_id)?.display_name ?? selectedRequestType.default_priority_id}`
                : 'Selecciona un tipo de solicitud.'}
            </div>
            <div className="flex gap-2">
              <Button type="button" variant="outline" onClick={onCancel} disabled={isSubmitting}>
                Cancelar
              </Button>
              <Button type="submit" disabled={isSubmitting}>
                {isSubmitting ? 'Guardando…' : 'Guardar'}
              </Button>
            </div>
          </div>
        </form>
      </CardContent>
    </Card>
  )
}
