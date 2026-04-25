import { useEffect, useState } from 'react'
import { Badge } from '@/components/ui/badge'
import { Button } from '@/components/ui/button'
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card'
import { GetLanding, type LandingData } from '@/lib/api'
import { TicketsPage } from '@/pages/role/requester/TicketsPage'

type Props = {
  token: string
  onSignout: () => void
}

type State =
  | { status: 'loading' }
  | { status: 'error'; message: string }
  | { status: 'ready'; data: LandingData }

const ROLE_LABEL: Record<string, string> = {
  administrator: 'Administrador',
  supervisor: 'Supervisor',
  technician: 'Técnico',
  requester: 'Solicitante',
}

type NavSection = 'dashboard' | 'tickets'

export function DashboardPage({ token, onSignout }: Props) {
  const [state, setState] = useState<State>({ status: 'loading' })
  const [isUserMenuOpen, setIsUserMenuOpen] = useState(false)
  const [section, setSection] = useState<NavSection>('dashboard')

  useEffect(() => {
    GetLanding(token)
      .then(data => setState({ status: 'ready', data }))
      .catch(err => {
        if ((err as { status?: number }).status === 401) {
          onSignout()
          return
        }
        const message = err instanceof Error ? err.message : 'Error al cargar los datos.'
        setState({ status: 'error', message })
      })
  }, [token, onSignout])

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

  const { profile } = state.data

  return (
    <div className="flex h-full overflow-hidden bg-muted/40">
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
          <button
            className={`w-full flex items-center gap-2 rounded-md px-3 py-2 text-sm font-medium ${section === 'dashboard' ? 'bg-accent text-accent-foreground' : 'text-muted-foreground hover:bg-accent/50 hover:text-accent-foreground'}`}
            type="button"
            onClick={() => setSection('dashboard')}
          >
            <svg xmlns="http://www.w3.org/2000/svg" width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">
              <rect width="7" height="9" x="3" y="3" rx="1" />
              <rect width="7" height="5" x="14" y="3" rx="1" />
              <rect width="7" height="9" x="14" y="12" rx="1" />
              <rect width="7" height="5" x="3" y="16" rx="1" />
            </svg>
            Dashboard
          </button>
          <button
            className={`w-full flex items-center gap-2 rounded-md px-3 py-2 text-sm font-medium ${section === 'tickets' ? 'bg-accent text-accent-foreground' : 'text-muted-foreground hover:bg-accent/50 hover:text-accent-foreground'}`}
            type="button"
            onClick={() => setSection('tickets')}
          >
            <svg xmlns="http://www.w3.org/2000/svg" width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">
              <path d="M15 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V7Z" />
              <path d="M14 2v4a2 2 0 0 0 2 2h4" />
              <path d="M10 9H8" />
              <path d="M16 13H8" />
              <path d="M16 17H8" />
            </svg>
            Tickets
          </button>
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
                {profile.user.username.slice(0, 1).toUpperCase()}
              </span>
              <span className="truncate text-left">
                <span className="block text-xs font-medium text-foreground">{profile.user.username}</span>
                <span className="block text-xs text-muted-foreground">{ROLE_LABEL[profile.role] ?? profile.role}</span>
              </span>
            </span>
            <svg xmlns="http://www.w3.org/2000/svg" width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true">
              <path d="m6 9 6 6 6-6" />
            </svg>
          </Button>

          {isUserMenuOpen && (
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
          )}
        </div>
      </aside>

      <main className="flex-1 overflow-y-auto px-8 py-8">
        <div className="mx-auto flex w-full max-w-5xl flex-col space-y-6">
          {section === 'dashboard' && (
            <>
              <div>
                <h2 className="text-xl font-semibold tracking-tight">Dashboard</h2>
                <p className="text-sm text-muted-foreground mt-1">
                  Bienvenido, {profile.user.username}
                </p>
              </div>

              <Card>
                <CardHeader>
                  <CardTitle className="text-base">Mi perfil</CardTitle>
                </CardHeader>
                <CardContent className="grid grid-cols-2 gap-4 text-sm sm:grid-cols-3">
                  <div>
                    <p className="text-muted-foreground mb-1">Usuario</p>
                    <p className="font-medium">{profile.user.username}</p>
                  </div>
                  <div>
                    <p className="text-muted-foreground mb-1">Rol</p>
                    <Badge variant="secondary">
                      {ROLE_LABEL[profile.role] ?? profile.role}
                    </Badge>
                  </div>
                  <div>
                    <p className="text-muted-foreground mb-1">ID de perfil</p>
                    <p className="font-medium">{profile.id}</p>
                  </div>
                </CardContent>
              </Card>
            </>
          )}

          {section === 'tickets' && (
            <TicketsPage
              token={token}
              onSignout={onSignout}
              requestTypes={state.data.request_types}
              departments={state.data.departments}
              priorities={state.data.priorities}
              ticketStatuses={state.data.ticket_statuses}
            />
          )}
        </div>
      </main>
    </div>
  )
}
