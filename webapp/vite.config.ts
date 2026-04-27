import path from 'path'
import tailwindcss from '@tailwindcss/vite'
import react from '@vitejs/plugin-react'
import { defineConfig } from 'vite'

export default defineConfig({
  base: '/ticketeer/app',
  plugins: [tailwindcss(), react()],
  server: {
    port: 5522,
    hmr: false,
    watch: { ignored: ['**/*'] },
    allowedHosts: true
  },
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
    },
  },
})
