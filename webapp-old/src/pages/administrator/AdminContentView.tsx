import type { ReactNode } from 'react'
import { Button } from '@/components/ui/button'
import { Card, CardDescription, CardHeader, CardTitle } from '@/components/ui/card'

type ContentTab = 'departments' | 'priorities' | 'request-types' | 'ticket-statuses'

type Props = {
  contentTab: ContentTab
  onChangeContentTab: (tab: ContentTab) => void
  onCreate: () => void
  listContent: ReactNode
  detailContent: ReactNode
}

export function AdminContentView({
  contentTab,
  onChangeContentTab,
  onCreate,
  listContent,
  detailContent,
}: Props) {
  return (
    <div className="space-y-6">
      <Card>
        <CardHeader className="gap-3">
          <div className="flex flex-col gap-3 md:flex-row md:items-center md:justify-between">
            <div>
              <CardTitle className="text-base">Contenido</CardTitle>
              <CardDescription>Gestiona departamentos, prioridades, estados y tipos de solicitud.</CardDescription>
            </div>
            <Button type="button" onClick={onCreate}>
              Crear
            </Button>
          </div>
          <div className="flex flex-wrap gap-2">
            <button
              type="button"
              className={`rounded-md px-3 py-2 text-sm ${
                contentTab === 'departments'
                  ? 'bg-primary text-primary-foreground'
                  : 'border border-input bg-background text-muted-foreground hover:bg-accent hover:text-accent-foreground'
              }`}
              onClick={() => onChangeContentTab('departments')}
            >
              Departamentos
            </button>
            <button
              type="button"
              className={`rounded-md px-3 py-2 text-sm ${
                contentTab === 'priorities'
                  ? 'bg-primary text-primary-foreground'
                  : 'border border-input bg-background text-muted-foreground hover:bg-accent hover:text-accent-foreground'
              }`}
              onClick={() => onChangeContentTab('priorities')}
            >
              Prioridades
            </button>
            <button
              type="button"
              className={`rounded-md px-3 py-2 text-sm ${
                contentTab === 'request-types'
                  ? 'bg-primary text-primary-foreground'
                  : 'border border-input bg-background text-muted-foreground hover:bg-accent hover:text-accent-foreground'
              }`}
              onClick={() => onChangeContentTab('request-types')}
            >
              Tipos de solicitud
            </button>
            <button
              type="button"
              className={`rounded-md px-3 py-2 text-sm ${
                contentTab === 'ticket-statuses'
                  ? 'bg-primary text-primary-foreground'
                  : 'border border-input bg-background text-muted-foreground hover:bg-accent hover:text-accent-foreground'
              }`}
              onClick={() => onChangeContentTab('ticket-statuses')}
            >
              Estados de ticket
            </button>
          </div>
        </CardHeader>
      </Card>

      <div className="grid items-start gap-6 lg:grid-cols-[320px_minmax(0,1fr)]">
        {listContent}
        {detailContent}
      </div>
    </div>
  )
}
