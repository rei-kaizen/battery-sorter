<script setup>
import { ref } from 'vue'
import { auth, db } from '../firebase.js'
import { signInWithEmailAndPassword, createUserWithEmailAndPassword } from 'firebase/auth'
import { ref as dbRef, set, get } from 'firebase/database'

const email = ref('')
const password = ref('')
const errorMessage = ref('')
const isLoading = ref(false)
const isSignUp = ref(false)
const showPassword = ref(false)

const handleSubmit = async () => {
  if (!email.value || !password.value) {
    errorMessage.value = 'Please enter both email and password.'
    return
  }
  isLoading.value = true
  errorMessage.value = ''
  try {
    if (isSignUp.value) {
      const credential = await createUserWithEmailAndPassword(auth, email.value, password.value)
      // Write user profile to RTDB; role defaults to 'viewer'
      await set(dbRef(db, `users/${credential.user.uid}`), {
        email: email.value,
        role: 'viewer',
        createdAt: Date.now()
      })
    } else {
      const credential = await signInWithEmailAndPassword(auth, email.value, password.value)
      // Create profile if missing (users who registered before RBAC was added)
      const snap = await get(dbRef(db, `users/${credential.user.uid}`))
      if (!snap.exists()) {
        await set(dbRef(db, `users/${credential.user.uid}`), {
          email: email.value,
          role: 'viewer',
          createdAt: Date.now()
        })
      }
    }
  } catch (error) {
    console.error('Auth failed', error)
    const code = error.code
    if (code === 'auth/invalid-credential' || code === 'auth/wrong-password' || code === 'auth/user-not-found') {
      errorMessage.value = 'Invalid email or password.'
    } else if (code === 'auth/email-already-in-use') {
      errorMessage.value = 'This email is already registered.'
    } else if (code === 'auth/weak-password') {
      errorMessage.value = 'Password must be at least 6 characters.'
    } else {
      errorMessage.value = error.message
    }
  } finally {
    isLoading.value = false
  }
}
</script>

<template>
  <div class="overlay">
    <div class="login-box">
      <div class="brand-mark">
        <svg width="48" height="48" viewBox="0 0 48 48" fill="none" xmlns="http://www.w3.org/2000/svg" aria-hidden="true">
          <rect x="2" y="12" width="36" height="24" rx="5" stroke="#38bdf8" stroke-width="2.5"/>
          <rect x="38" y="20" width="7" height="8" rx="2" fill="#38bdf8"/>
          <rect x="5" y="15" width="20" height="18" rx="2.5" fill="#38bdf8" opacity="0.75"/>
          <path d="M21 12 L16 24 H22 L17 36" stroke="#0f172a" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
          <circle cx="40" cy="38" r="8" fill="#10b981"/>
          <path d="M37 38 L39.5 40.5 L44 36" stroke="white" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/>
        </svg>
      </div>
      <h2>Battery Sorter</h2>
      <p>{{ isSignUp ? 'Create a new account to get started.' : 'Sign in to access the dashboard.' }}</p>

      <form @submit.prevent="handleSubmit" novalidate>
        <div class="input-group">
          <label for="email">Email</label>
          <input id="email" type="email" v-model="email" placeholder="admin@example.com" autocomplete="email" required />
        </div>

        <div class="input-group">
          <label for="password">Password</label>
          <div class="password-wrapper">
            <input
              id="password"
              :type="showPassword ? 'text' : 'password'"
              v-model="password"
              placeholder="••••••••"
              autocomplete="current-password"
              required
            />
            <button type="button" class="toggle-password" @click="showPassword = !showPassword" :aria-label="showPassword ? 'Hide password' : 'Show password'">
              <svg v-if="!showPassword" xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                <path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/>
                <circle cx="12" cy="12" r="3"/>
              </svg>
              <svg v-else xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                <path d="M17.94 17.94A10.07 10.07 0 0 1 12 20c-7 0-11-8-11-8a18.45 18.45 0 0 1 5.06-5.94M9.9 4.24A9.12 9.12 0 0 1 12 4c7 0 11 8 11 8a18.5 18.5 0 0 1-2.16 3.19m-6.72-1.07a3 3 0 1 1-4.24-4.24"/>
                <line x1="1" y1="1" x2="23" y2="23"/>
              </svg>
            </button>
          </div>
        </div>

        <p v-if="errorMessage" class="error-msg" role="alert">{{ errorMessage }}</p>

        <button type="submit" class="submit-btn" :disabled="isLoading">
          <svg v-if="isLoading" class="spinner" xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round">
            <path d="M12 2v4M12 18v4M4.93 4.93l2.83 2.83M16.24 16.24l2.83 2.83M2 12h4M18 12h4M4.93 19.07l2.83-2.83M16.24 7.76l2.83-2.83"/>
          </svg>
          <span>{{ isLoading ? (isSignUp ? 'Creating account…' : 'Signing in…') : (isSignUp ? 'Create Account' : 'Sign In') }}</span>
        </button>
      </form>

      <p class="toggle-mode">
        {{ isSignUp ? 'Already have an account?' : "Don't have an account?" }}
        <a href="#" @click.prevent="isSignUp = !isSignUp; errorMessage = ''">
          {{ isSignUp ? 'Sign In' : 'Sign Up' }}
        </a>
      </p>

      <p class="viewer-note">New accounts are granted <strong>Viewer</strong> access by default.</p>
    </div>
  </div>
</template>

<style scoped>
.overlay {
  position: fixed;
  inset: 0;
  background: linear-gradient(135deg, #0f172a 0%, #1a2744 100%);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 1000;
  padding: 16px;
}

.login-box {
  background: #1e293b;
  padding: 40px 36px;
  border-radius: 20px;
  box-shadow: 0 25px 50px rgba(0, 0, 0, 0.5), 0 0 0 1px rgba(56, 189, 248, 0.1);
  width: 100%;
  max-width: 420px;
  text-align: center;
  border: 1px solid #334155;
}

.brand-mark {
  margin-bottom: 16px;
  display: flex;
  justify-content: center;
}

.login-box h2 {
  margin: 0 0 8px;
  color: #f8fafc;
  font-size: 1.75rem;
  font-weight: 700;
  letter-spacing: -0.5px;
}

.login-box > p {
  color: #64748b;
  margin: 0 0 28px;
  font-size: 0.9rem;
}

form {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.input-group {
  text-align: left;
}

.input-group label {
  display: block;
  margin-bottom: 6px;
  font-weight: 500;
  color: #cbd5e1;
  font-size: 0.875rem;
}

.password-wrapper {
  position: relative;
  display: flex;
  align-items: center;
}

.password-wrapper input {
  padding-right: 44px;
}

.toggle-password {
  position: absolute;
  right: 0;
  height: 100%;
  width: 44px;
  background: transparent;
  border: none;
  color: #64748b;
  cursor: pointer;
  display: flex;
  align-items: center;
  justify-content: center;
  transition: color 0.2s;
  padding: 0;
}

.toggle-password:hover {
  color: #cbd5e1;
}

input {
  width: 100%;
  padding: 11px 14px;
  background: #0f172a;
  border: 1px solid #334155;
  border-radius: 9px;
  font-size: 0.95rem;
  color: #f8fafc;
  box-sizing: border-box;
  transition: border-color 0.2s, box-shadow 0.2s;
  font-family: inherit;
}

input:focus {
  border-color: #38bdf8;
  box-shadow: 0 0 0 3px rgba(56, 189, 248, 0.15);
  outline: none;
}

input::placeholder {
  color: #475569;
}

.error-msg {
  margin: 0;
  padding: 10px 14px;
  background: rgba(239, 68, 68, 0.1);
  border: 1px solid rgba(239, 68, 68, 0.25);
  border-radius: 8px;
  color: #ef4444;
  font-size: 0.85rem;
  text-align: left;
}

.submit-btn {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 8px;
  width: 100%;
  padding: 13px;
  background: #38bdf8;
  color: #0f172a;
  border: none;
  border-radius: 9px;
  font-size: 0.95rem;
  font-weight: 700;
  cursor: pointer;
  transition: background 0.2s, transform 0.15s, box-shadow 0.2s;
  font-family: inherit;
  margin-top: 4px;
}

.submit-btn:hover:not(:disabled) {
  background: #0ea5e9;
  box-shadow: 0 0 20px rgba(56, 189, 248, 0.35);
  transform: translateY(-1px);
}

.submit-btn:active:not(:disabled) {
  transform: translateY(0);
}

.submit-btn:disabled {
  opacity: 0.65;
  cursor: not-allowed;
}

.spinner {
  animation: spin 1s linear infinite;
  flex-shrink: 0;
}

@keyframes spin {
  to { transform: rotate(360deg); }
}

.toggle-mode {
  margin-top: 22px;
  margin-bottom: 0;
  font-size: 0.875rem;
  color: #64748b;
}

.toggle-mode a {
  color: #38bdf8;
  font-weight: 600;
  text-decoration: none;
  margin-left: 4px;
}

.toggle-mode a:hover {
  color: #7dd3fc;
  text-decoration: underline;
}

.viewer-note {
  margin-top: 12px;
  margin-bottom: 0;
  font-size: 0.78rem;
  color: #475569;
}

.viewer-note strong {
  color: #64748b;
}
</style>
