<script setup>
import { ref, onMounted, onUnmounted } from 'vue'
import { db } from '../firebase.js'
import { ref as dbRef, onValue, set } from 'firebase/database'

const props = defineProps({
  currentUserUid: { type: String, required: true }
})

const users = ref([])
const isUpdating = ref(null)
const updateError = ref('')

let unsubscribe = null

onMounted(() => {
  const usersRef = dbRef(db, 'users')
  unsubscribe = onValue(usersRef, (snapshot) => {
    const data = snapshot.val()
    if (!data) { users.value = []; return }
    users.value = Object.entries(data).map(([uid, info]) => ({
      uid,
      email: info.email || 'Unknown',
      role: info.role || 'viewer',
      createdAt: info.createdAt || null
    })).sort((a, b) => (a.email > b.email ? 1 : -1))
  })
})

onUnmounted(() => {
  if (unsubscribe) unsubscribe()
})

const toggleRole = async (user) => {
  if (user.uid === props.currentUserUid) return
  const newRole = user.role === 'admin' ? 'viewer' : 'admin'
  isUpdating.value = user.uid
  updateError.value = ''
  try {
    await set(dbRef(db, `users/${user.uid}/role`), newRole)
  } catch (e) {
    updateError.value = 'Failed to update role. Check Firebase security rules.'
    console.error(e)
  } finally {
    isUpdating.value = null
  }
}

const formatDate = (ts) => {
  if (!ts) return '—'
  return new Date(ts).toLocaleDateString(undefined, { year: 'numeric', month: 'short', day: 'numeric' })
}
</script>

<template>
  <div class="admin-panel">
    <div class="panel-header">
      <h2>User Management</h2>
      <p class="panel-note">
        The first admin must be assigned manually in the Firebase console.<br />
        Admins can promote or demote other users. You cannot change your own role.
      </p>
    </div>

    <div v-if="updateError" class="alert-error">{{ updateError }}</div>

    <div class="user-table-wrapper">
      <table class="user-table">
        <thead>
          <tr>
            <th>Email</th>
            <th>Role</th>
            <th>Joined</th>
            <th>Action</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="user in users" :key="user.uid" :class="{ 'self-row': user.uid === currentUserUid }">
            <td class="email-cell">
              {{ user.email }}
              <span v-if="user.uid === currentUserUid" class="you-badge">You</span>
            </td>
            <td>
              <span class="role-pill" :class="user.role">{{ user.role }}</span>
            </td>
            <td class="date-cell">{{ formatDate(user.createdAt) }}</td>
            <td>
              <button
                v-if="user.uid !== currentUserUid"
                class="role-toggle-btn"
                :class="user.role === 'admin' ? 'demote' : 'promote'"
                :disabled="isUpdating === user.uid"
                @click="toggleRole(user)"
              >
                <span v-if="isUpdating === user.uid">Updating…</span>
                <span v-else-if="user.role === 'admin'">Demote to Viewer</span>
                <span v-else>Promote to Admin</span>
              </button>
              <span v-else class="no-action">—</span>
            </td>
          </tr>
          <tr v-if="users.length === 0">
            <td colspan="4" class="empty-cell">No registered users found.</td>
          </tr>
        </tbody>
      </table>
    </div>
  </div>
</template>

<style scoped>
.admin-panel {
  padding: 0;
}

.panel-header {
  margin-bottom: 24px;
}

.panel-header h2 {
  margin: 0 0 8px;
  font-size: 1.4rem;
  font-weight: 700;
  color: #f8fafc;
}

.panel-note {
  margin: 0;
  font-size: 0.875rem;
  color: #64748b;
  line-height: 1.6;
}

.alert-error {
  padding: 12px 16px;
  background: rgba(239, 68, 68, 0.1);
  border: 1px solid rgba(239, 68, 68, 0.3);
  border-radius: 8px;
  color: #ef4444;
  font-size: 0.875rem;
  margin-bottom: 16px;
}

.user-table-wrapper {
  background: #1e293b;
  border-radius: 12px;
  border: 1px solid #334155;
  overflow: hidden;
  overflow-x: auto;
}

.user-table {
  width: 100%;
  border-collapse: collapse;
  color: #cbd5e1;
  min-width: 520px;
}

.user-table th {
  background: #0f172a;
  padding: 12px 16px;
  text-align: left;
  font-size: 0.8rem;
  font-weight: 600;
  letter-spacing: 0.05em;
  text-transform: uppercase;
  color: #94a3b8;
}

.user-table td {
  padding: 14px 16px;
  border-top: 1px solid #1e293b;
}

.user-table tbody tr {
  transition: background 0.15s;
}

.user-table tbody tr:hover {
  background: rgba(51, 65, 85, 0.4);
}

.self-row {
  background: rgba(56, 189, 248, 0.04);
}

.email-cell {
  display: flex;
  align-items: center;
  gap: 8px;
  font-weight: 500;
  color: #f1f5f9;
  word-break: break-all;
}

.you-badge {
  font-size: 0.7rem;
  padding: 2px 7px;
  background: rgba(56, 189, 248, 0.15);
  color: #38bdf8;
  border-radius: 99px;
  font-weight: 600;
  white-space: nowrap;
}

.date-cell {
  font-size: 0.875rem;
  color: #64748b;
  white-space: nowrap;
}

.role-pill {
  display: inline-block;
  padding: 4px 12px;
  border-radius: 99px;
  font-size: 0.8rem;
  font-weight: 600;
}

.role-pill.admin {
  background: rgba(16, 185, 129, 0.15);
  color: #10b981;
  border: 1px solid rgba(16, 185, 129, 0.3);
}

.role-pill.viewer {
  background: rgba(56, 189, 248, 0.12);
  color: #38bdf8;
  border: 1px solid rgba(56, 189, 248, 0.25);
}

.role-toggle-btn {
  padding: 7px 14px;
  border-radius: 7px;
  font-size: 0.8rem;
  font-weight: 600;
  cursor: pointer;
  border: none;
  transition: all 0.2s;
  white-space: nowrap;
}

.role-toggle-btn.promote {
  background: rgba(16, 185, 129, 0.15);
  color: #10b981;
  border: 1px solid rgba(16, 185, 129, 0.3);
}

.role-toggle-btn.promote:hover:not(:disabled) {
  background: rgba(16, 185, 129, 0.25);
}

.role-toggle-btn.demote {
  background: rgba(239, 68, 68, 0.12);
  color: #ef4444;
  border: 1px solid rgba(239, 68, 68, 0.25);
}

.role-toggle-btn.demote:hover:not(:disabled) {
  background: rgba(239, 68, 68, 0.22);
}

.role-toggle-btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.no-action {
  color: #475569;
}

.empty-cell {
  text-align: center;
  padding: 32px;
  color: #475569;
  font-style: italic;
}
</style>
