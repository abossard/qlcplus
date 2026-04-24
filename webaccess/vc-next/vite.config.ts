import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

export default defineConfig({
  plugins: [react()],
  base: '/vc/',
  server: {
    port: 5173,
    proxy: {
      '/vc.json': {
        target: 'http://127.0.0.1:9999',
        changeOrigin: true,
      },
      '/qlcplusWS': {
        target: 'http://127.0.0.1:9999',
        ws: true,
        changeOrigin: true,
      },
      '/qrc': {
        target: 'http://127.0.0.1:9999',
        changeOrigin: true,
      },
    },
  },
  build: {
    outDir: 'dist',
    assetsDir: 'assets',
  },
})
