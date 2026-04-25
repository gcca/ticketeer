import { useEffect, useState } from 'react'
import { DashboardPage } from '@/pages/DashboardPage'
import { LoginPage } from '@/pages/LoginPage'

const STORAGE_KEY = 'ticketeer_token'
const APP_BASE = '/ticketeer/app'
const LOGIN_PATH = `${APP_BASE}/login`
const DASHBOARD_PATH = `${APP_BASE}/dashboard`

function App() {
  const [token, setToken] = useState<string | null>(
    () => localStorage.getItem(STORAGE_KEY)
  )
  const [path, setPath] = useState(() => window.location.pathname || DASHBOARD_PATH)

  useEffect(() => {
    function handlePopState() {
      setPath(window.location.pathname || DASHBOARD_PATH)
    }

    window.addEventListener('popstate', handlePopState)
    return () => window.removeEventListener('popstate', handlePopState)
  }, [])

  function navigate(nextPath: string, replace = false) {
    const method = replace ? 'replaceState' : 'pushState'
    window.history[method](null, '', nextPath)
    setPath(nextPath)
  }

  useEffect(() => {
    if (!token && path !== LOGIN_PATH) {
      navigate(LOGIN_PATH, true)
      return
    }

    if (token && path === LOGIN_PATH) {
      navigate(DASHBOARD_PATH, true)
    }
  }, [path, token])

  function handleLogin(t: string) {
    localStorage.setItem(STORAGE_KEY, t)
    setToken(t)
    navigate(DASHBOARD_PATH, true)
  }

  function handleSignout() {
    localStorage.removeItem(STORAGE_KEY)
    setToken(null)
    navigate(LOGIN_PATH, true)
  }

  if (token) {
    return (
      <DashboardPage
        token={token}
        onSignout={handleSignout}
        path={path}
        onNavigate={navigate}
      />
    )
  }

  return <LoginPage onSuccess={handleLogin} />
}

export default App
