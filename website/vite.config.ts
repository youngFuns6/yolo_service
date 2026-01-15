import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import path from 'path'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  build: {
    outDir: path.resolve(__dirname, '../build/linux-x64-Release/website'),
  },
  resolve: {
    alias: {
      '@': path.resolve(__dirname, 'src')
    }
  },
  css: {
    preprocessorOptions: {
      scss: {
        additionalData: '@use "@/assets/css/global.scss" as *;',
      },
      less: {
        javascriptEnabled: true,
      },
    },
    lightningcss: {
      errorRecovery: true,
    },
  },
})
