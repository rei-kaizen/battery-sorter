<script setup>
import { ref } from 'vue'
import { getAuth, signInWithEmailAndPassword } from 'firebase/auth'

const email = ref('')
const password = ref('')
const errorMessage = ref('')
const isLoading = ref(false)

const handleLogin = async () => {
  if (!email.value || !password.value) {
    errorMessage.value = 'Please enter both email and password.'
    return
  }

  isLoading.value = true
  errorMessage.value = ''
  
  const auth = getAuth()
  try {
    await signInWithEmailAndPassword(auth, email.value, password.value)
    // The App.vue will listen to the auth state change automatically
  } catch (error) {
    console.error("Login failed", error)
    errorMessage.value = 'Invalid email or password. Please try again.'
  } finally {
    isLoading.value = false
  }
}
</script>

<template>
  <div class="overlay">
    <div class="login-box">
      <h2>E-Waste Dashboard</h2>
      <p>Please log in to access the control panel.</p>
      
      <form @submit.prevent="handleLogin">
        <div class="input-group">
          <label>Email</label>
          <input type="email" v-model="email" placeholder="admin@example.com" required />
        </div>
        
        <div class="input-group">
          <label>Password</label>
          <input type="password" v-model="password" placeholder="********" required />
        </div>

        <p v-if="errorMessage" class="error-msg">{{ errorMessage }}</p>

        <button type="submit" :disabled="isLoading">
          {{ isLoading ? 'Logging in...' : 'Log In' }}
        </button>
      </form>
    </div>
  </div>
</template>

<style scoped>
.overlay {
  position: fixed;
  top: 0;
  left: 0;
  width: 100vw;
  height: 100vh;
  background-color: rgba(0, 0, 0, 0.75);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 1000;
  backdrop-filter: blur(4px);
}

.login-box {
  background: white;
  padding: 30px;
  border-radius: 12px;
  box-shadow: 0 10px 25px rgba(0,0,0,0.2);
  width: 100%;
  max-width: 400px;
  text-align: center;
}

.login-box h2 {
  margin-top: 0;
  color: #333;
}

.login-box p {
  color: #666;
  margin-bottom: 20px;
}

.input-group {
  margin-bottom: 15px;
  text-align: left;
}

.input-group label {
  display: block;
  margin-bottom: 5px;
  font-weight: 600;
  color: #444;
  font-size: 0.9em;
}

.input-group input {
  width: 100%;
  padding: 10px;
  border: 1px solid #ccc;
  border-radius: 6px;
  font-size: 1em;
  box-sizing: border-box;
}

.input-group input:focus {
  border-color: #42b883;
  outline: none;
}

button {
  width: 100%;
  padding: 12px;
  background-color: #42b883;
  color: white;
  border: none;
  border-radius: 6px;
  font-size: 1.05em;
  font-weight: bold;
  cursor: pointer;
  transition: background-color 0.2s;
  margin-top: 10px;
}

button:hover:not(:disabled) {
  background-color: #33a06f;
}

button:disabled {
  background-color: #9cdbbf;
  cursor: not-allowed;
}

.error-msg {
  color: #e74c3c !important;
  font-size: 0.85em;
  margin-top: 0;
  margin-bottom: 10px;
}
</style>
